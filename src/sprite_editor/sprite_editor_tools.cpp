#include "common.h"
#include "sprite_editor.h"
#include "sprite_editor_config.h"

namespace {
    char const* const kGridPopupId   = "Grid Tool##tool_grid";
    char const* const kDetectPopupId = "Detect Frames##tool_detect";
    // Upper bounds keep the preview, the frame list and the rect math sane.
    constexpr int kMaxGridDim = 1024;
    constexpr int kMaxCellDim = 65536;
    // Above this many cells the grid preview draws edge lines instead of one outline per cell.
    constexpr int kMaxPreviewCells = 16384;
    // The detect preview outlines at most this many frames.
    constexpr int kMaxPreviewRects = 16384;
    constexpr float kFormWidth = 240.0f;
    constexpr float kFormLabelWidth = 90.0f;
    constexpr ImVec4 kWarningColor{ 1.0f, 0.45f, 0.35f, 1.0f };

    ImU32 ToU32(moth::gfx::Color const& c) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4{ c.data[0], c.data[1], c.data[2], c.data[3] });
    }

    // Size and centre the next tool popup within the main viewport.
    void SetNextToolPopupLayout() {
        ImGuiViewport const* const viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowSize(ImVec2{ viewport->WorkSize.x * 0.75f, viewport->WorkSize.y * 0.75f }, ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2{ 0.5f, 0.5f });
    }

    // Label followed by a full-width InputInt on the same line. Clamps the value; returns true when edited.
    bool InputIntRow(char const* label, char const* id, int& value, int minValue, int maxValue) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(kFormLabelWidth);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool const edited = ImGui::InputInt(id, &value);
        value = std::clamp(value, minValue, maxValue);
        return edited;
    }

    void WrappedText(ImVec4 const& color, char const* text) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(color, "%s", text);
        ImGui::PopTextWrapPos();
    }

    // Accept/Cancel buttons pinned to the bottom of the current child window.
    // Returns true when accepted; sets `cancelled` when Cancel is pressed.
    bool ToolPopupButtons(char const* acceptLabel, bool acceptEnabled, bool& cancelled) {
        float const bottomY = ImGui::GetWindowContentRegionMax().y - ImGui::GetFrameHeight();
        if (ImGui::GetCursorPosY() < bottomY) {
            ImGui::SetCursorPosY(bottomY);
        }
        float const btnW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        ImGui::BeginDisabled(!acceptEnabled);
        bool const accepted = ImGui::Button(acceptLabel, ImVec2{ btnW, 0.0f });
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2{ btnW, 0.0f })) {
            cancelled = true;
        }
        return accepted;
    }

    // Where the sheet lands in the preview: screen-space origin and image-to-screen scale.
    struct PreviewFit {
        ImVec2 origin;
        float scale = 1.0f;
    };

    // Fit an extent (in image pixels) into the remaining content region of the current window.
    PreviewFit FitPreview(float extentW, float extentH) {
        ImVec2 const avail = ImGui::GetContentRegionAvail();
        float const scale = (extentW > 0.0f && extentH > 0.0f)
            ? std::max(std::min(avail.x / extentW, avail.y / extentH), 0.0f)
            : 1.0f;
        return { ImGui::GetCursorScreenPos(), scale };
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

void SpriteEditor::AppendFrames(std::vector<moth::gfx::IntRect> const& rects) {
    if (rects.empty()) {
        return;
    }
    auto before = m_frames;
    Selection const beforeSel = m_selection;
    int const firstNew = static_cast<int>(m_frames.size());
    m_frames.reserve(m_frames.size() + rects.size());
    for (auto const& rect : rects) {
        moth::gfx::SpriteSheet::FrameEntry frame;
        frame.rect  = rect;
        frame.pivot = { 0, 0 };
        m_frames.push_back(frame);
    }
    m_selection = { firstNew };
    PushFrameAction(std::move(before), beforeSel, m_selection);
}

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

    SetNextToolPopupLayout();
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
        ImGui::SeparatorText("Cell size (px)");
        InputIntRow("Width",   "##grid_w",    g.cellW,    1, kMaxCellDim);
        InputIntRow("Height",  "##grid_h",    g.cellH,    1, kMaxCellDim);

        ImGui::SeparatorText("Offset (px)");
        InputIntRow("X",       "##grid_ox",   g.offsetX,  0, kMaxCellDim);
        InputIntRow("Y",       "##grid_oy",   g.offsetY,  0, kMaxCellDim);

        ImGui::SeparatorText("Spacing (px)");
        InputIntRow("X",       "##grid_sx",   g.spacingX, 0, kMaxCellDim);
        InputIntRow("Y",       "##grid_sy",   g.spacingY, 0, kMaxCellDim);

        ImGui::SeparatorText("Layout");
        InputIntRow("Rows",    "##grid_rows", g.rows,     1, kMaxGridDim);
        InputIntRow("Columns", "##grid_cols", g.cols,     1, kMaxGridDim);
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
            WrappedText(kWarningColor, "The grid extends past the image.");
        }

        accepted = ToolPopupButtons("Add Frames", true, cancelled);
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
        PreviewFit const fit = FitPreview(static_cast<float>(std::max(imgW, gridW)),
                                          static_cast<float>(std::max(imgH, gridH)));
        ImVec2 const origin = fit.origin;
        float const scale = fit.scale;

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
        std::vector<moth::gfx::IntRect> rects;
        rects.reserve(static_cast<size_t>(g.rows * g.cols));
        // Row-major order: left to right, top to bottom.
        for (int r = 0; r < g.rows; ++r) {
            for (int c = 0; c < g.cols; ++c) {
                rects.push_back(moth::gfx::MakeRect(g.offsetX + (c * (g.cellW + g.spacingX)),
                                                    g.offsetY + (r * (g.cellH + g.spacingY)),
                                                    g.cellW, g.cellH));
            }
        }
        AppendFrames(rects);
    }

    if (accepted || cancelled) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void SpriteEditor::DrawDetectFramesTool() {
    auto const* image = (m_spriteSheet && m_spriteSheet->GetImage())
                        ? &m_spriteSheet->GetImage() : nullptr;
    auto& d = m_detectTool;
    auto& opt = d.options;

    if (m_openDetectTool) {
        m_openDetectTool = false;
        if (image != nullptr && m_imagePathBuffer[0] != '\0') {
            d.pixels = LoadImagePixels(m_imagePathBuffer);
            if (d.pixels.has_value()) {
                d.dirty = true;
                ImGui::OpenPopup(kDetectPopupId);
            } else {
                moth::core::log::error("SpriteEditor: could not read pixels from '{}'", m_imagePathBuffer);
            }
        }
    }

    SetNextToolPopupLayout();
    if (!ImGui::BeginPopupModal(kDetectPopupId, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        // Release the decoded sheet once the popup has closed.
        if (d.pixels.has_value()) {
            d.pixels.reset();
            d.result = {};
        }
        return;
    }

    if (image == nullptr || !d.pixels.has_value()) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    // ---- Form (left) ----
    bool accepted = false;
    bool cancelled = ImGui::IsKeyPressed(ImGuiKey_Escape, false);

    ImGui::BeginChild("##detect_form", ImVec2{ kFormWidth, 0.0f }, ImGuiChildFlags_None);
    {
        bool changed = false;

        ImGui::SeparatorText("Background");
        static constexpr std::array<char const*, 3> kModeNames{ "Alpha", "Corner color", "Color" };
        int mode = static_cast<int>(opt.mode);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::Combo("##detect_mode", &mode, kModeNames.data(), static_cast<int>(kModeNames.size()))) {
            opt.mode = static_cast<BackgroundMode>(mode);
            changed = true;
        }

        switch (opt.mode) {
        case BackgroundMode::Alpha:
            WrappedText(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),
                        "Pixels more opaque than the alpha threshold are part of a frame.");
            break;
        case BackgroundMode::CornerColor:
            WrappedText(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),
                        "The four corner pixels set the background color. "
                        "If they differ, the alpha threshold is used instead.");
            break;
        case BackgroundMode::Color:
            WrappedText(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),
                        "Pixels that differ from the background color are part of a frame.");
            break;
        }

        if (opt.mode == BackgroundMode::Color) {
            std::array<float, 3> color{};
            for (size_t i = 0; i < color.size(); ++i) {
                color[i] = static_cast<float>(opt.backgroundColor[i]) / 255.0f;
            }
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Color");
            ImGui::SameLine(kFormLabelWidth);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::ColorEdit3("##detect_bg", color.data(), ImGuiColorEditFlags_DisplayHex)) {
                for (size_t i = 0; i < color.size(); ++i) {
                    opt.backgroundColor[i] = static_cast<uint8_t>(std::lround(std::clamp(color[i], 0.0f, 1.0f) * 255.0f));
                }
                changed = true;
            }
        }
        if (opt.mode != BackgroundMode::Alpha) {
            changed |= InputIntRow("Tolerance", "##detect_ct", opt.colorThreshold, 0, 255);
        }
        if (opt.mode != BackgroundMode::Color) {
            changed |= InputIntRow("Alpha", "##detect_at", opt.alphaThreshold, 0, 255);
        }

        ImGui::SeparatorText("Grouping");
        changed |= InputIntRow("Merge gap", "##detect_gap", opt.mergeGap, 0, kMaxGridDim);
        WrappedText(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled),
                    "Pixels up to this many pixels apart join the same frame. "
                    "Pulls stray edge pixels into the frame they belong to.");

        ImGui::SeparatorText("Size filter (px, 0 = off)");
        changed |= InputIntRow("Min width",  "##detect_minw", opt.minWidth,  0, kMaxCellDim);
        changed |= InputIntRow("Min height", "##detect_minh", opt.minHeight, 0, kMaxCellDim);
        changed |= InputIntRow("Max width",  "##detect_maxw", opt.maxWidth,  0, kMaxCellDim);
        changed |= InputIntRow("Max height", "##detect_maxh", opt.maxHeight, 0, kMaxCellDim);

        ImGui::SeparatorText("Buffer (px)");
        changed |= InputIntRow("X", "##detect_px", opt.paddingX, 0, kMaxCellDim);
        changed |= InputIntRow("Y", "##detect_py", opt.paddingY, 0, kMaxCellDim);

        if (changed) {
            d.dirty = true;
        }
        if (d.dirty) {
            d.result = DetectFrames(*d.pixels, opt);
            d.dirty = false;
        }

        ImGui::Separator();
        int const found = static_cast<int>(d.result.rects.size());
        ImGui::Text("Frames found: %d", found);
        if (d.result.filteredCount > 0) {
            ImGui::TextDisabled("Removed by size filter: %d", d.result.filteredCount);
        }
        if (d.result.backgroundColor.has_value()) {
            auto const& bg = *d.result.backgroundColor;
            float const swatch = ImGui::GetTextLineHeight();
            ImGui::ColorButton("##detect_bg_swatch",
                               ImVec4{ static_cast<float>(bg[0]) / 255.0f, static_cast<float>(bg[1]) / 255.0f,
                                       static_cast<float>(bg[2]) / 255.0f, 1.0f },
                               ImGuiColorEditFlags_NoTooltip, ImVec2{ swatch, swatch });
            ImGui::SameLine();
            ImGui::Text("Background #%02X%02X%02X", unsigned{ bg[0] }, unsigned{ bg[1] }, unsigned{ bg[2] });
        } else if (opt.mode == BackgroundMode::CornerColor) {
            WrappedText(kWarningColor, "Corner pixels differ; using the alpha threshold.");
        }
        if (found > kMaxPreviewRects) {
            ImGui::TextDisabled("Preview shows the first %d.", kMaxPreviewRects);
        }

        accepted = ToolPopupButtons("Add Frames", found > 0, cancelled);
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // ---- Sheet preview with detected frames (right) ----
    ImGui::BeginChild("##detect_preview", ImVec2{ 0.0f, 0.0f }, ImGuiChildFlags_Border,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        int const imgW = image->GetWidth();
        int const imgH = image->GetHeight();
        PreviewFit const fit = FitPreview(static_cast<float>(imgW), static_cast<float>(imgH));
        float const dispImgW = static_cast<float>(imgW) * fit.scale;
        float const dispImgH = static_cast<float>(imgH) * fit.scale;
        DrawImage(*image, { static_cast<int>(dispImgW), static_cast<int>(dispImgH) });

        ImDrawList* const dl = ImGui::GetWindowDrawList();
        ImU32 const normalU32 = ToU32(m_config.SpriteEditorNormalColor);
        ImU32 const frameU32  = ToU32(m_config.SpriteEditorSelectedColor);
        dl->AddRect(fit.origin, ImVec2{ fit.origin.x + dispImgW, fit.origin.y + dispImgH }, normalU32);

        int const drawCount = std::min(static_cast<int>(d.result.rects.size()), kMaxPreviewRects);
        for (int i = 0; i < drawCount; ++i) {
            auto const& r = d.result.rects[static_cast<size_t>(i)];
            ImVec2 const p0{ fit.origin.x + (static_cast<float>(r.left())  * fit.scale),
                             fit.origin.y + (static_cast<float>(r.top())   * fit.scale) };
            ImVec2 const p1{ fit.origin.x + (static_cast<float>(r.right()) * fit.scale),
                             fit.origin.y + (static_cast<float>(r.bottom()) * fit.scale) };
            dl->AddRect(p0, p1, frameU32);
        }
    }
    ImGui::EndChild();

    if (accepted) {
        AppendFrames(d.result.rects);
    }

    if (accepted || cancelled) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
