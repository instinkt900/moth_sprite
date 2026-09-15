#include "common.h"
#include "sprite_editor.h"
#include "sprite_editor_config.h"

void SpriteEditor::DeleteFrame(int frameToDelete) {
    if (frameToDelete < 0 || frameToDelete >= static_cast<int>(m_frames.size())) {
        return;
    }
    auto beforeFrames = m_frames;
    auto beforeClips  = m_clips;
    int const beforeSel = m_selectedFrame;
    m_frames.erase(m_frames.begin() + frameToDelete);
    // Fix up selection
    if (m_selectedFrame == frameToDelete) {
        m_selectedFrame = -1;
    } else if (m_selectedFrame > frameToDelete) {
        --m_selectedFrame;
    }
    // Fix up clip steps: decrement indices that point past the deleted frame.
    // Steps pointing AT frameToDelete and not the last frame are left unchanged
    // — they now point at the frame that shifted into that slot.
    // Then clamp all indices to the new valid range so steps that pointed at the
    // deleted last frame don't go out of bounds.
    for (auto& clip : m_clips) {
        for (auto& step : clip.desc.frames) {
            if (step.frameIndex > frameToDelete) {
                --step.frameIndex;
            }
            if (!m_frames.empty()) {
                step.frameIndex = std::min(step.frameIndex,
                    static_cast<int>(m_frames.size()) - 1);
            } else {
                step.frameIndex = -1;
            }
        }
    }
    PushFrameClipAction(std::move(beforeFrames), std::move(beforeClips),
                        beforeSel, m_selectedFrame);
}

void SpriteEditor::DrawCellListWindow() {
    ImGui::Text("Cells: %d", static_cast<int>(m_frames.size()));

    // The list takes the height that the form below it does not need: a title and three rows of fields.
    ImGuiStyle const& style = ImGui::GetStyle();
    float const formH = ImGui::GetTextLineHeightWithSpacing() + style.ItemSpacing.y +
                        (ImGui::GetFrameHeightWithSpacing() * 3.0f);
    float const listH = std::max(ImGui::GetContentRegionAvail().y - formH,
                                 ImGui::GetFrameHeightWithSpacing() * 3.0f);

    // ---- Cell list ----
    int frameToDelete = -1;
    if (ImGui::BeginChild("##cell_list", ImVec2{ 0.0f, listH }, ImGuiChildFlags_Border)) {
        float const deleteW = ImGui::CalcTextSize("x").x + (style.FramePadding.x * 2.0f);
        for (int i = 0; i < static_cast<int>(m_frames.size()); ++i) {
            auto const& fr = m_frames[i];
            ImGui::PushID(i);

            bool const isSelected = (m_selectedFrame == i);
            std::string const label = fmt::format("{:<4}({}, {}) {}x{}",
                i, fr.rect.x(), fr.rect.y(), fr.rect.w(), fr.rect.h());
            if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_AllowOverlap)) {
                m_selectedFrame = isSelected ? -1 : i;
            }

            // Delete button at the right end of the row, over the selectable.
            ImGui::SameLine(ImGui::GetContentRegionMax().x - deleteW);
            if (ImGui::SmallButton("x")) {
                frameToDelete = i;
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    if (frameToDelete >= 0) {
        DeleteFrame(frameToDelete);
    }

    // ---- Cell form (InputInt fields) ----
    if (m_selectedFrame < 0 || m_selectedFrame >= static_cast<int>(m_frames.size())) {
        ImGui::SeparatorText("Cell");
        ImGui::TextDisabled("Select a cell to edit it.");
        return;
    }

    auto& fr = m_frames[m_selectedFrame];
    int x = fr.rect.x();
    int y = fr.rect.y();
    int w = fr.rect.w();
    int h = fr.rect.h();
    int pivotX = fr.pivot.x;
    int pivotY = fr.pivot.y;

    ImGui::SeparatorText(fmt::format("Cell {}", m_selectedFrame).c_str());

    // 4-column table: label | input | label | input
    if (ImGui::BeginTable("##fedit_tbl", 4, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("##fl1", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("##fv1", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##fl2", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("##fv2", ImGuiTableColumnFlags_WidthStretch);

        bool anyActivated   = false;
        bool anyDeactivated = false;
        auto editRow = [&](char const* l1, char const* id1, int& v1,
                           char const* l2, char const* id2, int& v2) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(l1);
            ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputInt(id1, &v1);
            anyActivated   |= ImGui::IsItemActivated();
            anyDeactivated |= ImGui::IsItemDeactivatedAfterEdit();
            ImGui::TableSetColumnIndex(2); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(l2);
            ImGui::TableSetColumnIndex(3); ImGui::SetNextItemWidth(-FLT_MIN); ImGui::InputInt(id2, &v2);
            anyActivated   |= ImGui::IsItemActivated();
            anyDeactivated |= ImGui::IsItemDeactivatedAfterEdit();
        };

        editRow("X",       "##fedit_x",  x,      "Y",       "##fedit_y",  y);
        editRow("W",       "##fedit_w",  w,      "H",       "##fedit_h",  h);
        editRow("Pivot X", "##fedit_px", pivotX, "Pivot Y", "##fedit_py", pivotY);

        ImGui::EndTable();

        w = std::max(w, 1);
        h = std::max(h, 1);
        fr.rect  = moth::gfx::MakeRect(x, y, w, h);
        fr.pivot = { pivotX, pivotY };

        if (anyActivated && !m_pendingFrameSnapshot.has_value()) {
            m_pendingFrameSnapshot = m_frames;
        }
        if (anyDeactivated && m_pendingFrameSnapshot.has_value()) {
            PushFrameAction(std::move(*m_pendingFrameSnapshot),
                            m_selectedFrame, m_selectedFrame);
            m_pendingFrameSnapshot.reset();
        }
    }
}

void SpriteEditor::DrawCellWindow() {
    auto const* image = (m_spriteSheet && m_spriteSheet->GetImage())
                        ? &m_spriteSheet->GetImage() : nullptr;
    if (image == nullptr) {
        ImGui::TextDisabled("Use File > Import Sheet to add a sheet image.");
        return;
    }
    if (m_selectedFrame < 0 || m_selectedFrame >= static_cast<int>(m_frames.size())) {
        ImGui::TextDisabled("Select a cell on the sheet or in the Cells window.");
        return;
    }

    auto& fr = m_frames[m_selectedFrame];
    float const imgW = static_cast<float>(image->GetWidth());
    float const imgH = static_cast<float>(image->GetHeight());
    // Clamp UVs so the preview shows a valid region even when the frame rect
    // extends beyond the imported image.
    moth::gfx::FloatVec2 const uv0{
        std::clamp(static_cast<float>(fr.rect.x())      / imgW, 0.0f, 1.0f),
        std::clamp(static_cast<float>(fr.rect.y())      / imgH, 0.0f, 1.0f) };
    moth::gfx::FloatVec2 const uv1{
        std::clamp(static_cast<float>(fr.rect.right())  / imgW, 0.0f, 1.0f),
        std::clamp(static_cast<float>(fr.rect.bottom()) / imgH, 0.0f, 1.0f) };
    float const cellW = static_cast<float>(std::max(fr.rect.w(), 1));
    float const cellH = static_cast<float>(std::max(fr.rect.h(), 1));

    // One toolbar row above the canvas and three rows of pivot presets below it.
    ImVec2 const totalAvail = ImGui::GetContentRegionAvail();
    float const canvasH = totalAvail.y - (ImGui::GetFrameHeightWithSpacing() * 4.0f);
    auto const fitZoom = [&]() {
        if (totalAvail.x > 0.0f && canvasH > 0.0f) {
            m_cellZoom = std::min(totalAvail.x / cellW, canvasH / cellH);
        }
    };
    if (m_cellZoom < 0.0f) {
        fitZoom();
    }

    // Toolbar
    if (ImGui::Button("Fit")) {
        fitZoom();
    }
    ImGui::SameLine();
    if (ImGui::Button("1:1")) {
        m_cellZoom = 1.0f;
    }
    ImGui::SameLine();
    ImGui::Text("%.0f%%", m_cellZoom * 100.0f);

    // Scrollable canvas. Click or drag on the cell to set its pivot.
    ImGui::BeginChild("##cell_canvas", ImVec2{ 0.0f, std::max(canvasH, 1.0f) }, ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ZoomWithMouseWheel(m_cellZoom);
    // Draw at 1:1 until the window has room to compute a fit.
    float const zoom = (m_cellZoom > 0.0f) ? m_cellZoom : 1.0f;
    float const dispW = cellW * zoom;
    float const dispH = cellH * zoom;

    ImVec2 const imagePos = ImGui::GetCursorScreenPos();
    DrawImage(*image, { static_cast<int>(dispW), static_cast<int>(dispH) }, uv0, uv1);

    // InvisibleButton over the cell so ImGui owns the left-button press
    // and the window cannot start a drag.
    ImGui::SetCursorScreenPos(imagePos);
    ImGui::InvisibleButton("##pivot_drag_area", ImVec2{ dispW, dispH });

    if (ImGui::IsItemActivated()) {
        m_pivotDragSnapshot = m_frames;
        m_pivotDragging = true;
    }
    if (ImGui::IsItemActive()) {
        ImVec2 const mouse = ImGui::GetMousePos();
        fr.pivot.x = static_cast<int>(std::round(std::clamp((mouse.x - imagePos.x) / zoom, 0.0f, static_cast<float>(fr.rect.w()))));
        fr.pivot.y = static_cast<int>(std::round(std::clamp((mouse.y - imagePos.y) / zoom, 0.0f, static_cast<float>(fr.rect.h()))));
    }
    if (ImGui::IsItemDeactivated() && m_pivotDragging) {
        if (m_pivotDragSnapshot.has_value()) {
            bool changed = false;
            if (m_selectedFrame >= 0 &&
                m_selectedFrame < static_cast<int>(m_frames.size()) &&
                m_selectedFrame < static_cast<int>(m_pivotDragSnapshot->size())) {
                auto const& oldFr = (*m_pivotDragSnapshot)[m_selectedFrame];
                changed = (oldFr.pivot.x != fr.pivot.x || oldFr.pivot.y != fr.pivot.y);
            }
            if (changed) {
                PushFrameAction(std::move(*m_pivotDragSnapshot),
                                m_selectedFrame, m_selectedFrame);
            }
            m_pivotDragSnapshot.reset();
        }
        m_pivotDragging = false;
    }

    // Pivot crosshair
    ImDrawList* const dl = ImGui::GetWindowDrawList();
    float const px = imagePos.x + (static_cast<float>(fr.pivot.x) * zoom);
    float const py = imagePos.y + (static_cast<float>(fr.pivot.y) * zoom);
    auto const& selCol = m_config.SpriteEditorSelectedColor;
    ImU32 const crossColor = ImGui::ColorConvertFloat4ToU32(
        ImVec4{ selCol.data[0], selCol.data[1], selCol.data[2], selCol.data[3] });
    constexpr float kArm = 5.0f;
    dl->AddLine({ px - kArm, py }, { px + kArm, py }, crossColor, 1.5f);
    dl->AddLine({ px, py - kArm }, { px, py + kArm }, crossColor, 1.5f);
    ImGui::EndChild();

    // Pivot preset grid (3×3)
    int const hw = fr.rect.w() / 2;
    int const hh = fr.rect.h() / 2;
    int const fw = fr.rect.w();
    int const fh = fr.rect.h();
    float const btnW = (ImGui::GetContentRegionAvail().x
                        - (ImGui::GetStyle().ItemSpacing.x * 2.0f)) / 3.0f;
    ImVec2 const bs{ btnW, 0.0f };

    auto pivotPreset = [&](char const* label, ImVec2 sz, int px, int py) {
        if (ImGui::Button(label, sz)) {
            auto before = m_frames;
            fr.pivot.x = px; fr.pivot.y = py;
            PushFrameAction(std::move(before), m_selectedFrame, m_selectedFrame);
        }
    };

    pivotPreset("TL##pv", bs, 0,  0);   ImGui::SameLine();
    pivotPreset("T##pv",  bs, hw, 0);   ImGui::SameLine();
    pivotPreset("TR##pv", bs, fw, 0);
    pivotPreset("L##pv",  bs, 0,  hh);  ImGui::SameLine();
    pivotPreset("C##pv",  bs, hw, hh);  ImGui::SameLine();
    pivotPreset("R##pv",  bs, fw, hh);
    pivotPreset("BL##pv", bs, 0,  fh);  ImGui::SameLine();
    pivotPreset("B##pv",  bs, hw, fh);  ImGui::SameLine();
    pivotPreset("BR##pv", bs, fw, fh);
}
