#include "common.h"
#include "sprite_editor.h"
#include "sprite_editor_config.h"

int SpriteEditor::PrimeCell() const {
    return m_selection.empty() ? -1 : m_selection.back();
}

bool SpriteEditor::IsCellSelected(int frameIndex) const {
    return std::find(m_selection.begin(), m_selection.end(), frameIndex) != m_selection.end();
}

void SpriteEditor::ToggleCellSelection(int frameIndex) {
    auto const it = std::find(m_selection.begin(), m_selection.end(), frameIndex);
    if (it != m_selection.end()) {
        // The most recently added cell that is still selected becomes prime.
        m_selection.erase(it);
    } else if (frameIndex >= 0 && frameIndex < static_cast<int>(m_frames.size())) {
        m_selection.push_back(frameIndex);
    }
}

void SpriteEditor::SetSelectionPivot(PivotAnchor x, PivotAnchor y) {
    auto const offset = [](PivotAnchor anchor, int size) {
        switch (anchor) {
        case PivotAnchor::Start:
            return 0;
        case PivotAnchor::Center:
            return size / 2;
        case PivotAnchor::End:
            return size;
        }
        return 0;
    };

    auto before = m_frames;
    bool changed = false;
    for (int const sel : m_selection) {
        if (sel >= 0 && sel < static_cast<int>(m_frames.size())) {
            auto& fr = m_frames[sel];
            int const pivotX = offset(x, fr.rect.w());
            int const pivotY = offset(y, fr.rect.h());
            if (fr.pivot.x != pivotX || fr.pivot.y != pivotY) {
                fr.pivot = { pivotX, pivotY };
                changed = true;
            }
        }
    }
    // One undo action for the whole selection, and none when no pivot changed.
    if (changed) {
        PushFrameAction(std::move(before), m_selection, m_selection);
    }
}

void SpriteEditor::DeleteFrames(std::vector<int> framesToDelete) {
    int const frameCount = static_cast<int>(m_frames.size());
    framesToDelete.erase(std::remove_if(framesToDelete.begin(), framesToDelete.end(),
                                        [frameCount](int i) { return i < 0 || i >= frameCount; }),
                         framesToDelete.end());
    // Delete from the highest index down, so that the indices still to delete stay valid.
    std::sort(framesToDelete.begin(), framesToDelete.end(), std::greater<>());
    framesToDelete.erase(std::unique(framesToDelete.begin(), framesToDelete.end()), framesToDelete.end());
    if (framesToDelete.empty()) {
        return;
    }

    auto beforeFrames = m_frames;
    auto beforeClips  = m_clips;
    Selection const beforeSel = m_selection;
    for (int const frameToDelete : framesToDelete) {
        m_frames.erase(m_frames.begin() + frameToDelete);
        // Fix up selection: drop the deleted cell and shift the cells after it down by one.
        Selection kept;
        kept.reserve(m_selection.size());
        for (int const sel : m_selection) {
            if (sel != frameToDelete) {
                kept.push_back((sel > frameToDelete) ? sel - 1 : sel);
            }
        }
        m_selection = std::move(kept);
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
    }
    PushFrameClipAction(std::move(beforeFrames), std::move(beforeClips),
                        beforeSel, m_selection);
}

bool SpriteEditor::TrackFrameEdit(bool changed) {
    ImGuiID const id = ImGui::GetItemID();
    // Check for the end before the start. An InputInt is a group, so when focus moves from its text field to its own
    // - or + button, the group reports both in one frame, with the button's id. The start below commits the text
    // field's edit, and the button's new edit must not end at once.
    bool const ended = ImGui::IsItemDeactivated() && m_pendingFrameEdit.has_value() && m_pendingFrameEdit->id == id;
    if (ImGui::IsItemActivated()) {
        // When focus moves straight from another field, that field's edit may not be committed yet. This frame's
        // change is not applied to the cell yet, so the snapshot is the state before it.
        CommitFrameEdit();
        m_pendingFrameEdit = PendingFrameEdit{ id, m_frames, false };
    }
    if (changed && m_pendingFrameEdit.has_value() && m_pendingFrameEdit->id == id) {
        m_pendingFrameEdit->edited = true;
    }
    return ended;
}

void SpriteEditor::CommitFrameEdit() {
    if (m_pendingFrameEdit.has_value() && m_pendingFrameEdit->edited) {
        PushFrameAction(std::move(m_pendingFrameEdit->snapshot), m_selection, m_selection);
    }
    m_pendingFrameEdit.reset();
}

void SpriteEditor::DrawCellListWindow() {
    ImGui::Text("Cells: %d", static_cast<int>(m_frames.size()));
    if (m_selection.size() > 1) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%d selected)", static_cast<int>(m_selection.size()));
    }

    // The list takes the height that the form below it does not need. The form measures its height each time it
    // is drawn, so it fits without scrolling. Before the first measurement, estimate two titles, three rows of fields
    // and three rows of pivot presets.
    ImGuiStyle const& style = ImGui::GetStyle();
    float const formH = (m_cellFormHeight > 0.0f)
        ? m_cellFormHeight
        : ((ImGui::GetTextLineHeightWithSpacing() + style.ItemSpacing.y) * 2.0f) +
              (ImGui::GetFrameHeightWithSpacing() * 6.0f);
    float const listH = std::max(ImGui::GetContentRegionAvail().y - formH,
                                 ImGui::GetFrameHeightWithSpacing() * 3.0f);

    // ---- Cell list ----
    auto const& io = ImGui::GetIO();
    int const prime = PrimeCell();
    std::vector<bool> cellSelected(m_frames.size(), false);
    for (int const sel : m_selection) {
        if (sel >= 0 && sel < static_cast<int>(m_frames.size())) {
            cellSelected[static_cast<size_t>(sel)] = true;
        }
    }
    // The prime cell's row is highlighted in the prime colour.
    auto const& primeColor = m_config.SpriteEditorPrimeColor.data;
    ImVec4 const primeHeader{ primeColor[0], primeColor[1], primeColor[2], primeColor[3] * 0.45f };
    ImVec4 const primeHeaderHovered{ primeColor[0], primeColor[1], primeColor[2], primeColor[3] * 0.65f };

    int frameToDelete = -1;
    if (ImGui::BeginChild("##cell_list", ImVec2{ 0.0f, listH }, ImGuiChildFlags_Border)) {
        float const deleteW = ImGui::CalcTextSize("x").x + (style.FramePadding.x * 2.0f);
        for (int i = 0; i < static_cast<int>(m_frames.size()); ++i) {
            auto const& fr = m_frames[i];
            ImGui::PushID(i);

            bool const isPrime = (i == prime);
            if (isPrime) {
                ImGui::PushStyleColor(ImGuiCol_Header, primeHeader);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, primeHeaderHovered);
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, primeHeaderHovered);
            }
            std::string const label = fmt::format("{:<4}({}, {}) {}x{}",
                i, fr.rect.x(), fr.rect.y(), fr.rect.w(), fr.rect.h());
            if (ImGui::Selectable(label.c_str(), cellSelected[static_cast<size_t>(i)], ImGuiSelectableFlags_AllowOverlap)) {
                // While picking a cell for a clip step, every click is a plain click and picks the cell.
                bool const plainClick = m_cellPick.has_value() || (!io.KeyCtrl && !io.KeyShift);
                if (plainClick || (io.KeyShift && prime < 0)) {
                    SelectCell(i);
                } else if (io.KeyShift) {
                    // Shift+click selects every cell from the prime cell to this one. The prime cell stays prime.
                    m_selection.clear();
                    int const towardsPrime = (i > prime) ? -1 : 1;
                    for (int c = i; c != prime; c += towardsPrime) {
                        m_selection.push_back(c);
                    }
                    m_selection.push_back(prime);
                } else {
                    ToggleCellSelection(i);
                }
            }
            if (isPrime) {
                ImGui::PopStyleColor(3);
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
        DeleteFrames({ frameToDelete });
    }

    // ---- Cell form (InputInt fields and pivot presets), for the prime cell ----
    float const formStartY = ImGui::GetCursorPosY();
    int const formCell = PrimeCell();
    if (formCell < 0 || formCell >= static_cast<int>(m_frames.size())) {
        ImGui::SeparatorText("Cell");
        ImGui::TextDisabled("Select a cell to edit it.");
        return;
    }

    auto& fr = m_frames[formCell];
    int x = fr.rect.x();
    int y = fr.rect.y();
    int w = fr.rect.w();
    int h = fr.rect.h();
    int pivotX = fr.pivot.x;
    int pivotY = fr.pivot.y;

    ImGui::SeparatorText(fmt::format("Cell {}", formCell).c_str());

    // 4-column table: label | input | label | input
    if (ImGui::BeginTable("##fedit_tbl", 4, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("##fl1", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("##fv1", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##fl2", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("##fv2", ImGuiTableColumnFlags_WidthStretch);

        // Each field applies its value to the cell right after it is drawn, and only then commits an edit that
        // ended. An edit can end in the same frame as its last change, such as a click on - or +.
        auto editField = [&](char const* id, int& value) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            bool const changed = ImGui::InputInt(id, &value);
            bool const ended = TrackFrameEdit(changed);
            w = std::max(w, 1);
            h = std::max(h, 1);
            fr.rect  = moth::gfx::MakeRect(x, y, w, h);
            fr.pivot = { pivotX, pivotY };
            if (ended) {
                CommitFrameEdit();
            }
        };
        auto editRow = [&](char const* l1, char const* id1, int& v1,
                           char const* l2, char const* id2, int& v2) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(l1);
            ImGui::TableSetColumnIndex(1); editField(id1, v1);
            ImGui::TableSetColumnIndex(2); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(l2);
            ImGui::TableSetColumnIndex(3); editField(id2, v2);
        };

        editRow("X",       "##fedit_x",  x,      "Y",       "##fedit_y",  y);
        editRow("W",       "##fedit_w",  w,      "H",       "##fedit_h",  h);
        editRow("Pivot X", "##fedit_px", pivotX, "Pivot Y", "##fedit_py", pivotY);

        ImGui::EndTable();
    }

    // Pivot preset grid (3×3). Like Edit > Pivot, each button sets the pivot of every selected cell.
    ImGui::SeparatorText("Pivot presets");
    float const btnW = (ImGui::GetContentRegionAvail().x - (style.ItemSpacing.x * 2.0f)) / 3.0f;
    ImVec2 const bs{ btnW, 0.0f };

    auto pivotPreset = [&](char const* label, ImVec2 sz, PivotAnchor px, PivotAnchor py) {
        if (ImGui::Button(label, sz)) {
            SetSelectionPivot(px, py);
        }
    };

    using Anchor = PivotAnchor;
    pivotPreset("TL##pv", bs, Anchor::Start,  Anchor::Start);   ImGui::SameLine();
    pivotPreset("T##pv",  bs, Anchor::Center, Anchor::Start);   ImGui::SameLine();
    pivotPreset("TR##pv", bs, Anchor::End,    Anchor::Start);
    pivotPreset("L##pv",  bs, Anchor::Start,  Anchor::Center);  ImGui::SameLine();
    pivotPreset("C##pv",  bs, Anchor::Center, Anchor::Center);  ImGui::SameLine();
    pivotPreset("R##pv",  bs, Anchor::End,    Anchor::Center);
    pivotPreset("BL##pv", bs, Anchor::Start,  Anchor::End);     ImGui::SameLine();
    pivotPreset("B##pv",  bs, Anchor::Center, Anchor::End);     ImGui::SameLine();
    pivotPreset("BR##pv", bs, Anchor::End,    Anchor::End);

    // The cursor is past the last row's item spacing, which balances the spacing above the form.
    m_cellFormHeight = ImGui::GetCursorPosY() - formStartY;
}

void SpriteEditor::DrawCellWindow() {
    // The same playback, and the same buttons, as the Clips window. Playback selects each step's cell, so this window
    // also previews the selected clip.
    DrawClipPlaybackControls();

    auto const* image = (m_spriteSheet && m_spriteSheet->GetImage())
                        ? &m_spriteSheet->GetImage() : nullptr;
    if (image == nullptr) {
        ImGui::TextDisabled("Use File > Import Sheet to add a sheet image.");
        return;
    }
    // The window shows the prime cell.
    int const prime = PrimeCell();
    if (prime < 0 || prime >= static_cast<int>(m_frames.size())) {
        ImGui::TextDisabled("Select a cell on the sheet or in the Cells window.");
        return;
    }

    auto& fr = m_frames[prime];
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
    int const maxFrameIdx = static_cast<int>(m_frames.size()) - 1;

    // During a pivot drag the layout uses the pivots from before the drag, so the cell stays still under the mouse
    // while its pivot moves.
    auto const layoutFrame = [&](int index) -> moth::gfx::SpriteSheet::FrameEntry const& {
        if (m_pivotDragging && m_pivotDragSnapshot.has_value() && index < static_cast<int>(m_pivotDragSnapshot->size())) {
            return (*m_pivotDragSnapshot)[index];
        }
        return m_frames[index];
    };
    // The canvas extent, relative to the pivot. It covers the cell and, with a clip selected, every step's cell placed
    // on the same pivot point, so the cell does not jump or resize while the clip plays.
    auto const& primeLayout = layoutFrame(prime);
    int const primePivotX = primeLayout.pivot.x;
    int const primePivotY = primeLayout.pivot.y;
    int minOX = -primePivotX;
    int maxOX = std::max(primeLayout.rect.w(), 1) - primePivotX;
    int minOY = -primePivotY;
    int maxOY = std::max(primeLayout.rect.h(), 1) - primePivotY;
    if (m_selectedClip >= 0 && m_selectedClip < static_cast<int>(m_clips.size())) {
        for (auto const& step : m_clips[m_selectedClip].desc.frames) {
            auto const& f = layoutFrame(std::clamp(step.frameIndex, 0, maxFrameIdx));
            minOX = std::min(minOX, -f.pivot.x);
            maxOX = std::max(maxOX, f.rect.w() - f.pivot.x);
            minOY = std::min(minOY, -f.pivot.y);
            maxOY = std::max(maxOY, f.rect.h() - f.pivot.y);
        }
    }
    float const boundW = static_cast<float>(std::max(maxOX - minOX, 1));
    float const boundH = static_cast<float>(std::max(maxOY - minOY, 1));

    // One toolbar row above the canvas.
    ImVec2 const totalAvail = ImGui::GetContentRegionAvail();
    float const canvasH = totalAvail.y - ImGui::GetFrameHeightWithSpacing();
    auto const fitZoom = [&]() {
        if (totalAvail.x > 0.0f && canvasH > 0.0f) {
            m_cellZoom = std::min(totalAvail.x / boundW, canvasH / boundH);
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
    float const contentW = boundW * zoom;
    float const contentH = boundH * zoom;

    // Centre the extent when it is smaller than the canvas.
    ImVec2 const canvasAvail = ImGui::GetContentRegionAvail();
    ImVec2 const origin = ImGui::GetCursorScreenPos();
    ImVec2 const contentMin{ origin.x + std::max((canvasAvail.x - contentW) * 0.5f, 0.0f),
                             origin.y + std::max((canvasAvail.y - contentH) * 0.5f, 0.0f) };
    // The background covers the whole extent, so it does not change from step to step.
    DrawImageBackground({ contentMin.x, contentMin.y }, { contentW, contentH });

    // The cell's pivot lands on the extent's pivot point.
    ImVec2 const imagePos{ contentMin.x + (static_cast<float>(-minOX - primePivotX) * zoom),
                           contentMin.y + (static_cast<float>(-minOY - primePivotY) * zoom) };
    float const dispW = static_cast<float>(std::max(fr.rect.w(), 1)) * zoom;
    float const dispH = static_cast<float>(std::max(fr.rect.h(), 1)) * zoom;
    ImGui::SetCursorScreenPos(imagePos);
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
            if (prime < static_cast<int>(m_pivotDragSnapshot->size())) {
                auto const& oldFr = (*m_pivotDragSnapshot)[prime];
                changed = (oldFr.pivot.x != fr.pivot.x || oldFr.pivot.y != fr.pivot.y);
            }
            if (changed) {
                PushFrameAction(std::move(*m_pivotDragSnapshot),
                                m_selection, m_selection);
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

    // Reserve the whole extent, so the scroll range does not change from step to step.
    ImGui::SetCursorScreenPos(contentMin);
    ImGui::Dummy(ImVec2{ contentW, contentH });
    ImGui::EndChild();
}
