#include "common.h"
#include "sprite_editor.h"
#include "sprite_editor_config.h"

namespace {
    char const* const kGridPopupId = "Grid Tool##tool_grid";
    // Upper bounds keep the preview, the frame list and the rect math sane.
    constexpr int kMaxGridDim = 1024;
    constexpr int kMaxCellDim = 65536;
    // Above this many cells the preview draws edge lines instead of one outline per cell.
    constexpr int kMaxPreviewCells = 16384;
    constexpr float kFormWidth = 240.0f;

    ImU32 ToU32(moth::gfx::Color const& c) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4{ c.data[0], c.data[1], c.data[2], c.data[3] });
    }

    // How many cells of size `cell` separated by `spacing` fit in `imageDim` after `offset`.
    int FitCount(int imageDim, int offset, int cell, int spacing) {
        return std::clamp((imageDim - offset + spacing) / (cell + spacing), 1, kMaxGridDim);
    }

    // Total extent in pixels from the image origin to the far edge of the last cell.
    int GridExtent(int offset, int count, int cell, int spacing) {
        return offset + (count * cell) + ((count - 1) * spacing);
    }
} // namespace

void SpriteEditor::DrawGridTool() {
    auto const* image = (m_spriteSheet && m_spriteSheet->GetImage())
                        ? &m_spriteSheet->GetImage() : nullptr;
    auto& g = m_gridTool;

    auto const fitToImage = [&g](int imgW, int imgH) {
        g.cols = FitCount(imgW, g.offsetX, g.cellW, g.spacingX);
        g.rows = FitCount(imgH, g.offsetY, g.cellH, g.spacingY);
    };

    if (m_openGridTool) {
        m_openGridTool = false;
        if (image != nullptr) {
            // Start with as many cells of the last-used layout as fit in the image.
            fitToImage(image->GetWidth(), image->GetHeight());
            ImGui::OpenPopup(kGridPopupId);
        }
    }

    ImGuiViewport const* const viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2{ viewport->WorkSize.x * 0.75f, viewport->WorkSize.y * 0.75f }, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2{ 0.5f, 0.5f });
    if (!ImGui::BeginPopupModal(kGridPopupId, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        return;
    }

    if (image == nullptr) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    int const imgW = image->GetWidth();
    int const imgH = image->GetHeight();

    // ---- Form (left) ----
    bool accepted = false;
    bool cancelled = ImGui::IsKeyPressed(ImGuiKey_Escape, false);

    ImGui::BeginChild("##grid_form", ImVec2{ kFormWidth, 0.0f }, ImGuiChildFlags_None);
    {
        auto inputRow = [](char const* label, char const* id, int& value, int minValue, int maxValue) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::SameLine(90.0f);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputInt(id, &value);
            value = std::clamp(value, minValue, maxValue);
        };

        ImGui::SeparatorText("Cell size (px)");
        inputRow("Width",   "##grid_w",    g.cellW,    1, kMaxCellDim);
        inputRow("Height",  "##grid_h",    g.cellH,    1, kMaxCellDim);

        ImGui::SeparatorText("Offset (px)");
        inputRow("X",       "##grid_ox",   g.offsetX,  0, kMaxCellDim);
        inputRow("Y",       "##grid_oy",   g.offsetY,  0, kMaxCellDim);

        ImGui::SeparatorText("Spacing (px)");
        inputRow("X",       "##grid_sx",   g.spacingX, 0, kMaxCellDim);
        inputRow("Y",       "##grid_sy",   g.spacingY, 0, kMaxCellDim);

        ImGui::SeparatorText("Layout");
        inputRow("Rows",    "##grid_rows", g.rows,     1, kMaxGridDim);
        inputRow("Columns", "##grid_cols", g.cols,     1, kMaxGridDim);
        if (ImGui::Button("Fit to image", ImVec2{ -FLT_MIN, 0.0f })) {
            fitToImage(imgW, imgH);
        }

        ImGui::Separator();
        int const gridW = GridExtent(g.offsetX, g.cols, g.cellW, g.spacingX);
        int const gridH = GridExtent(g.offsetY, g.rows, g.cellH, g.spacingY);
        ImGui::Text("Frames to add: %d", g.rows * g.cols);
        ImGui::Text("Grid extent: %d x %d px", gridW, gridH);
        ImGui::TextDisabled("Image: %d x %d px", imgW, imgH);
        if (gridW > imgW || gridH > imgH) {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextColored(ImVec4{ 1.0f, 0.45f, 0.35f, 1.0f }, "The grid extends past the image.");
            ImGui::PopTextWrapPos();
        }

        // Buttons pinned to the bottom of the form.
        float const buttonsH = ImGui::GetFrameHeight();
        float const bottomY = ImGui::GetWindowContentRegionMax().y - buttonsH;
        if (ImGui::GetCursorPosY() < bottomY) {
            ImGui::SetCursorPosY(bottomY);
        }
        float const btnW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        accepted = ImGui::Button("Add Frames", ImVec2{ btnW, 0.0f });
        ImGui::SameLine();
        cancelled |= ImGui::Button("Cancel", ImVec2{ btnW, 0.0f });
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // ---- Sheet preview with grid overlay (right) ----
    ImGui::BeginChild("##grid_preview", ImVec2{ 0.0f, 0.0f }, ImGuiChildFlags_Border,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        int const gridW = GridExtent(g.offsetX, g.cols, g.cellW, g.spacingX);
        int const gridH = GridExtent(g.offsetY, g.rows, g.cellH, g.spacingY);
        // Fit whichever is larger — the image or the grid — so overflow stays visible.
        float const extentW = static_cast<float>(std::max(imgW, gridW));
        float const extentH = static_cast<float>(std::max(imgH, gridH));
        ImVec2 const avail = ImGui::GetContentRegionAvail();
        float const scale = (extentW > 0.0f && extentH > 0.0f)
            ? std::max(std::min(avail.x / extentW, avail.y / extentH), 0.0f)
            : 1.0f;

        ImVec2 const origin = ImGui::GetCursorScreenPos();
        float const dispImgW = static_cast<float>(imgW) * scale;
        float const dispImgH = static_cast<float>(imgH) * scale;
        DrawImage(*image, { static_cast<int>(dispImgW), static_cast<int>(dispImgH) });

        ImDrawList* const dl = ImGui::GetWindowDrawList();
        ImU32 const normalU32   = ToU32(m_config.SpriteEditorNormalColor);
        ImU32 const gridU32     = ToU32(m_config.SpriteEditorSelectedColor);
        ImU32 const overflowU32 = IM_COL32(255, 60, 40, 70);

        // Image bounds
        dl->AddRect(origin, ImVec2{ origin.x + dispImgW, origin.y + dispImgH }, normalU32);

        float const dispGridW = static_cast<float>(gridW) * scale;
        float const dispGridH = static_cast<float>(gridH) * scale;

        // Tint the parts of the grid that fall outside the image.
        if (gridW > imgW) {
            dl->AddRectFilled(ImVec2{ origin.x + dispImgW, origin.y },
                              ImVec2{ origin.x + dispGridW, origin.y + dispGridH }, overflowU32);
        }
        if (gridH > imgH) {
            dl->AddRectFilled(ImVec2{ origin.x, origin.y + dispImgH },
                              ImVec2{ origin.x + std::min(dispImgW, dispGridW), origin.y + dispGridH }, overflowU32);
        }

        // Screen-space position of a cell's leading edge.
        auto const cellX = [&](int c) { return origin.x + (static_cast<float>(g.offsetX + (c * (g.cellW + g.spacingX))) * scale); };
        auto const cellY = [&](int r) { return origin.y + (static_cast<float>(g.offsetY + (r * (g.cellH + g.spacingY))) * scale); };
        float const dispCellW = static_cast<float>(g.cellW) * scale;
        float const dispCellH = static_cast<float>(g.cellH) * scale;

        if (g.rows * g.cols <= kMaxPreviewCells) {
            for (int r = 0; r < g.rows; ++r) {
                float const y = cellY(r);
                for (int c = 0; c < g.cols; ++c) {
                    float const x = cellX(c);
                    dl->AddRect(ImVec2{ x, y }, ImVec2{ x + dispCellW, y + dispCellH }, gridU32);
                }
            }
        } else {
            // Too many cells to outline individually; draw each cell's edges as
            // full-length lines. At this density the spacing gaps are not visible anyway.
            float const x0 = cellX(0);
            float const y0 = cellY(0);
            float const x1 = cellX(g.cols - 1) + dispCellW;
            float const y1 = cellY(g.rows - 1) + dispCellH;
            for (int c = 0; c < g.cols; ++c) {
                float const x = cellX(c);
                dl->AddLine(ImVec2{ x, y0 }, ImVec2{ x, y1 }, gridU32);
                dl->AddLine(ImVec2{ x + dispCellW, y0 }, ImVec2{ x + dispCellW, y1 }, gridU32);
            }
            for (int r = 0; r < g.rows; ++r) {
                float const y = cellY(r);
                dl->AddLine(ImVec2{ x0, y }, ImVec2{ x1, y }, gridU32);
                dl->AddLine(ImVec2{ x0, y + dispCellH }, ImVec2{ x1, y + dispCellH }, gridU32);
            }
        }
    }
    ImGui::EndChild();

    if (accepted) {
        auto before = m_frames;
        int const beforeSel = m_selectedFrame;
        int const firstNew = static_cast<int>(m_frames.size());
        m_frames.reserve(m_frames.size() + static_cast<size_t>(g.rows * g.cols));
        // Row-major order: left to right, top to bottom.
        for (int r = 0; r < g.rows; ++r) {
            for (int c = 0; c < g.cols; ++c) {
                moth::gfx::SpriteSheet::FrameEntry frame;
                frame.rect  = moth::gfx::MakeRect(g.offsetX + (c * (g.cellW + g.spacingX)),
                                                  g.offsetY + (r * (g.cellH + g.spacingY)),
                                                  g.cellW, g.cellH);
                frame.pivot = { 0, 0 };
                m_frames.push_back(frame);
            }
        }
        m_selectedFrame = firstNew;
        PushFrameAction(std::move(before), beforeSel, m_selectedFrame);
    }

    if (accepted || cancelled) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
