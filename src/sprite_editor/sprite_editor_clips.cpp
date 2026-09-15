#include "common.h"
#include "sprite_editor.h"
#include "sprite_editor_config.h"

namespace {
    // Side of a step thumbnail on a clip timeline, in pixels.
    constexpr float kThumbSize = 72.0f;
    // Drag-and-drop payload type for moving a step along its timeline.
    char const* const kStepPayloadType = "CLIP_STEP";
    struct StepPayload {
        int clip = 0;
        int step = 0;
    };
    // Outline and hint colour for the step that is picking a cell.
    constexpr ImU32 kPickColor = IM_COL32(255, 160, 0, 255);
    constexpr ImVec4 kPickTextColor{ 1.0f, 160.0f / 255.0f, 0.0f, 1.0f };
} // namespace

void SpriteEditor::AdvanceClipPlayback() {
    if (!m_clipPlaying || m_selectedClip < 0 || m_selectedClip >= static_cast<int>(m_clips.size())) {
        return;
    }
    auto const& clip = m_clips[m_selectedClip];
    if (clip.desc.frames.empty()) {
        return;
    }

    m_clipElapsedMs += ImGui::GetIO().DeltaTime * 1000.0f;
    m_clipCurrentStep = std::clamp(m_clipCurrentStep, 0,
        static_cast<int>(clip.desc.frames.size()) - 1);
    while (m_clipPlaying) {
        int const dur = std::max(clip.desc.frames[m_clipCurrentStep].durationMs, 1);
        if (m_clipElapsedMs < static_cast<float>(dur)) { break; }
        m_clipElapsedMs -= static_cast<float>(dur);
        int const next = m_clipCurrentStep + 1;
        if (next < static_cast<int>(clip.desc.frames.size())) {
            m_clipCurrentStep = next;
        } else {
            using LoopType = moth::gfx::SpriteSheet::LoopType;
            switch (clip.desc.loop) {
            case LoopType::Stop:
                m_clipCurrentStep = static_cast<int>(clip.desc.frames.size()) - 1;
                m_clipPlaying = false;
                m_clipElapsedMs = 0.0f;
                break;
            case LoopType::Reset:
                m_clipCurrentStep = 0;
                m_clipPlaying = false;
                m_clipElapsedMs = 0.0f;
                break;
            case LoopType::Loop:
                m_clipCurrentStep = 0;
                break;
            }
        }
    }
}

void SpriteEditor::DrawClipPlaybackControls() {
    using LoopType = moth::gfx::SpriteSheet::LoopType;
    bool const hasClip = m_selectedClip >= 0 && m_selectedClip < static_cast<int>(m_clips.size());
    int const totalSteps = hasClip ? static_cast<int>(m_clips[m_selectedClip].desc.frames.size()) : 0;

    ImGui::BeginDisabled(!hasClip);
    // The ### part keeps the button's ID the same while its label changes.
    if (ImGui::Button(m_clipPlaying ? "Pause###clip_play" : "Play###clip_play") && hasClip) {
        if (m_clipPlaying) {
            // Pause keeps the current step and the time already spent on it.
            m_clipPlaying = false;
        } else {
            auto const& clip = m_clips[m_selectedClip];
            // A Stop clip that played to its end starts again from the first step.
            if (clip.desc.loop == LoopType::Stop && m_clipCurrentStep >= totalSteps - 1 && m_clipElapsedMs <= 0.0f) {
                m_clipCurrentStep = 0;
            }
            m_clipPlaying = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Step") && totalSteps > 0) {
        auto const& clip = m_clips[m_selectedClip];
        m_clipPlaying = false;
        m_clipElapsedMs = 0.0f;
        int const next = m_clipCurrentStep + 1;
        if (next < totalSteps) {
            m_clipCurrentStep = next;
        } else {
            m_clipCurrentStep = (clip.desc.loop == LoopType::Stop) ? m_clipCurrentStep : 0;
        }
        m_scrollToClipStep = true;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (hasClip) {
        ImGui::Text("Step %d / %d  \"%s\"",
            totalSteps > 0 ? m_clipCurrentStep + 1 : 0, totalSteps, m_clips[m_selectedClip].name.c_str());
    } else {
        ImGui::TextDisabled("No clip selected");
    }
}

void SpriteEditor::DrawClipPreview() {
    if (m_selectedClip < 0 || m_selectedClip >= static_cast<int>(m_clips.size())) {
        ImGui::TextDisabled("Select a clip in the Clips window to preview it.");
        return;
    }
    auto const& clip = m_clips[m_selectedClip];

    // Draw the current frame, pivot-anchored
    auto const* image = (m_spriteSheet && m_spriteSheet->GetImage())
                        ? &m_spriteSheet->GetImage() : nullptr;
    if (clip.desc.frames.empty()) {
        ImGui::TextDisabled("(no steps)");
    } else if (image != nullptr) {
        m_clipCurrentStep = std::clamp(m_clipCurrentStep, 0,
            static_cast<int>(clip.desc.frames.size()) - 1);
        int const maxFrameIdx = static_cast<int>(m_frames.size()) - 1;

        // Compute bounding box in pivot-relative space so the anchor stays fixed
        // across all frames in the clip.
        int minOX = 0;
        int maxOX = 1;
        int minOY = 0;
        int maxOY = 1;
        if (maxFrameIdx >= 0) {
            auto const expand = [&](moth::gfx::SpriteSheet::FrameEntry const& f) {
                minOX = std::min(minOX, -f.pivot.x);
                maxOX = std::max(maxOX, f.rect.w() - f.pivot.x);
                minOY = std::min(minOY, -f.pivot.y);
                maxOY = std::max(maxOY, f.rect.h() - f.pivot.y);
            };
            // Initialise from first step then expand over the rest
            {
                auto const& f0 = m_frames[std::clamp(clip.desc.frames[0].frameIndex, 0, maxFrameIdx)];
                minOX = -f0.pivot.x;  maxOX = f0.rect.w() - f0.pivot.x;
                minOY = -f0.pivot.y;  maxOY = f0.rect.h() - f0.pivot.y;
            }
            for (int si = 1; si < static_cast<int>(clip.desc.frames.size()); ++si) {
                expand(m_frames[std::clamp(clip.desc.frames[si].frameIndex, 0, maxFrameIdx)]);
            }
        }
        int const boundW = std::max(maxOX - minOX, 1);
        int const boundH = std::max(maxOY - minOY, 1);

        constexpr float kPreviewH = 120.0f;
        float const availW = ImGui::GetContentRegionAvail().x;
        float const zoom = std::min(availW / static_cast<float>(boundW),
                                    kPreviewH / static_cast<float>(boundH));

        if (ImGui::BeginChild("##clip_canvas", ImVec2{ availW, kPreviewH }, ImGuiChildFlags_None,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            ImVec2 const canvasMin = ImGui::GetWindowPos();
            ImVec2 const canvasMax{ canvasMin.x + availW, canvasMin.y + kPreviewH };

            float const contentW = static_cast<float>(boundW) * zoom;
            float const contentH = static_cast<float>(boundH) * zoom;
            float const anchorX  = canvasMin.x + ((availW    - contentW) * 0.5f) + (static_cast<float>(-minOX) * zoom);
            float const anchorY  = canvasMin.y + ((kPreviewH - contentH) * 0.5f) + (static_cast<float>(-minOY) * zoom);

            ImDrawList* const dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(canvasMin, canvasMax, IM_COL32(30, 30, 30, 200));

            // Draw current frame with its pivot landing on the anchor point
            int const frameIdx = clip.desc.frames[m_clipCurrentStep].frameIndex;
            if (frameIdx >= 0 && frameIdx <= maxFrameIdx) {
                auto const& fr = m_frames[frameIdx];
                float const imgW = static_cast<float>(image->GetWidth());
                float const imgH = static_cast<float>(image->GetHeight());
                moth::gfx::FloatVec2 const uv0{
                    static_cast<float>(fr.rect.x())     / imgW,
                    static_cast<float>(fr.rect.y())     / imgH };
                moth::gfx::FloatVec2 const uv1{
                    static_cast<float>(fr.rect.right()) / imgW,
                    static_cast<float>(fr.rect.bottom())/ imgH };
                float const fw = static_cast<float>(fr.rect.w()) * zoom;
                float const fh = static_cast<float>(fr.rect.h()) * zoom;
                if (fw > 0.0f && fh > 0.0f) {
                    ImGui::SetCursorScreenPos({
                        anchorX - (static_cast<float>(fr.pivot.x) * zoom),
                        anchorY - (static_cast<float>(fr.pivot.y) * zoom) });
                    DrawImage(*image, { static_cast<int>(fw), static_cast<int>(fh) }, uv0, uv1);
                }
            }

            // Pivot crosshair
            constexpr float kArm = 5.0f;
            dl->AddLine({ anchorX - kArm, anchorY }, { anchorX + kArm, anchorY }, IM_COL32(255, 80, 80, 220), 1.5f);
            dl->AddLine({ anchorX, anchorY - kArm }, { anchorX, anchorY + kArm }, IM_COL32(255, 80, 80, 220), 1.5f);
        }
        ImGui::EndChild();
    }
}

void SpriteEditor::SelectClip(int clipIndex) {
    if (clipIndex == m_selectedClip) {
        return;
    }
    m_selectedClip    = clipIndex;
    m_clipCurrentStep = 0;
    m_clipElapsedMs   = 0.0f;
    m_clipPlaying     = false;
}

void SpriteEditor::SelectCell(int frameIndex) {
    m_selectedFrame = frameIndex;
    if (!m_cellPick.has_value() || frameIndex < 0 || frameIndex >= static_cast<int>(m_frames.size())) {
        return;
    }
    CellPick const pick = *m_cellPick;
    m_cellPick.reset();
    if (pick.clip < 0 || pick.clip >= static_cast<int>(m_clips.size()) ||
        pick.step < 0 || pick.step >= static_cast<int>(m_clips[pick.clip].desc.frames.size())) {
        return;
    }
    auto before = m_clips;
    m_clips[pick.clip].desc.frames[pick.step].frameIndex = frameIndex;
    PushClipAction(std::move(before), m_selectedClip, m_selectedClip);
}

void SpriteEditor::TrackClipEdit(bool changed) {
    ImGuiID const id = ImGui::GetItemID();
    if (ImGui::IsItemActivated()) {
        // When focus moves straight from another field, that field's edit may not be committed yet.
        CommitClipEdit();
        m_pendingClipEdit = PendingClipEdit{ id, m_clips, false };
    }
    bool const isPending = m_pendingClipEdit.has_value() && m_pendingClipEdit->id == id;
    if (changed && isPending) {
        m_pendingClipEdit->edited = true;
    }
    if (ImGui::IsItemDeactivated() && isPending) {
        CommitClipEdit();
    }
}

void SpriteEditor::CommitClipEdit() {
    if (m_pendingClipEdit.has_value() && m_pendingClipEdit->edited) {
        PushClipAction(std::move(m_pendingClipEdit->snapshot), m_selectedClip, m_selectedClip);
    }
    m_pendingClipEdit.reset();
}

void SpriteEditor::DrawClipEditorWindow() {
    // ---- Playback ----
    DrawClipPlaybackControls();
    if (m_cellPick.has_value()) {
        ImGui::TextColored(kPickTextColor, "Select a cell on the sheet or in the Cells window for the highlighted step (Esc to cancel)");
    }

    // ---- New-clip row ----
    {
        float const addBtnW = ImGui::CalcTextSize("+ Clip").x + (ImGui::GetStyle().FramePadding.x * 2.0f);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - addBtnW - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputTextWithHint("##new_clip_name", "New clip name", m_newClipNameBuffer, sizeof(m_newClipNameBuffer) - 1);
        ImGui::SameLine();
        if (ImGui::Button("+ Clip") && m_newClipNameBuffer[0] != '\0') {
            auto before = m_clips;
            int const beforeSel = m_selectedClip;
            moth::gfx::SpriteSheet::ClipEntry newClip;
            newClip.name = m_newClipNameBuffer;
            newClip.desc.loop = moth::gfx::SpriteSheet::LoopType::Stop;
            m_clips.push_back(std::move(newClip));
            m_newClipNameBuffer[0] = '\0';
            SelectClip(static_cast<int>(m_clips.size()) - 1);
            PushClipAction(std::move(before), beforeSel, m_selectedClip);
        }
    }

    ImGui::Separator();
    ImGui::Text("Clips: %d", static_cast<int>(m_clips.size()));

    // ---- Clip timelines, stacked ----
    ImGuiStyle const& style = ImGui::GetStyle();
    // A timeline row: the thumbnails, the duration fields below them, and a horizontal scrollbar.
    float const timelineH = kThumbSize + style.ItemSpacing.y + ImGui::GetFrameHeight() +
                            style.ScrollbarSize + style.ItemSpacing.y;
    // A clip block: a header row above its timeline, inside a bordered child.
    float const blockH = ImGui::GetFrameHeightWithSpacing() + timelineH + (style.WindowPadding.y * 2.0f);
    float const removeW = ImGui::CalcTextSize("x").x + (style.FramePadding.x * 2.0f);
    int const maxFrameIdx = static_cast<int>(m_frames.size()) - 1;
    auto const* image = (m_spriteSheet && m_spriteSheet->GetImage())
                        ? &m_spriteSheet->GetImage() : nullptr;
    ImU32 const currentU32 = ImGui::ColorConvertFloat4ToU32(ImVec4{
        m_config.SpriteEditorSelectedColor.data[0], m_config.SpriteEditorSelectedColor.data[1],
        m_config.SpriteEditorSelectedColor.data[2], m_config.SpriteEditorSelectedColor.data[3] });

    // Changes to the clip and step lists are applied after the loop, at most one per frame.
    struct StepMove {
        int clip = 0;
        int from = 0;
        int to = 0;
    };
    std::optional<StepMove> stepMove;
    std::optional<StepPayload> stepToDelete;
    int clipToAddStep = -1;
    int clipToDelete = -1;

    ImGui::BeginChild("##clip_list", ImVec2{ 0.0f, 0.0f }, ImGuiChildFlags_None);
    for (int c = 0; c < static_cast<int>(m_clips.size()); ++c) {
        auto& clip = m_clips[c];
        ImGui::PushID(c);

        bool const clipSelected = (c == m_selectedClip);
        if (clipSelected) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_Header));
        }
        ImGui::BeginChild("##clip", ImVec2{ 0.0f, blockH }, ImGuiChildFlags_Border,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        if (clipSelected) {
            ImGui::PopStyleColor();
        }

        // ---- Header: name, loop type, step count, delete ----
        char nameBuf[256];
        strncpy(nameBuf, clip.name.c_str(), sizeof(nameBuf) - 1);
        nameBuf[sizeof(nameBuf) - 1] = '\0';
        ImGui::SetNextItemWidth(160.0f);
        bool const nameChanged = ImGui::InputText("##cname", nameBuf, sizeof(nameBuf));
        if (nameChanged) {
            clip.name = nameBuf;
        }
        TrackClipEdit(nameChanged);

        ImGui::SameLine();
        static char const* const kLoopItems[] = { "Stop", "Reset", "Loop" };
        int loopIdx = static_cast<int>(clip.desc.loop);
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::Combo("##loop", &loopIdx, kLoopItems, 3)) {
            auto before = m_clips;
            clip.desc.loop = static_cast<moth::gfx::SpriteSheet::LoopType>(loopIdx);
            PushClipAction(std::move(before), m_selectedClip, m_selectedClip);
        }

        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%d steps", static_cast<int>(clip.desc.frames.size()));

        ImGui::SameLine(ImGui::GetContentRegionMax().x - removeW);
        if (ImGui::Button("X")) {
            clipToDelete = c;
        }
        ImGui::SetItemTooltip("Remove clip");

        // ---- Timeline: one thumbnail per step, duration below it ----
        ImGui::BeginChild("##timeline", ImVec2{ 0.0f, timelineH }, ImGuiChildFlags_None,
                          ImGuiWindowFlags_HorizontalScrollbar);
        int const stepCount = static_cast<int>(clip.desc.frames.size());
        for (int f = 0; f < stepCount; ++f) {
            auto& step = clip.desc.frames[f];
            ImGui::PushID(f);
            if (f > 0) {
                ImGui::SameLine();
            }
            ImGui::BeginGroup();

            ImVec2 const boxMin = ImGui::GetCursorScreenPos();
            ImVec2 const boxMax{ boxMin.x + kThumbSize, boxMin.y + kThumbSize };
            ImDrawList* const dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(boxMin, boxMax, IM_COL32(30, 30, 30, 200));

            // Thumbnail of the step's cell, fitted in the box.
            int const frameIdx = step.frameIndex;
            if (image != nullptr && frameIdx >= 0 && frameIdx <= maxFrameIdx) {
                auto const& fr = m_frames[frameIdx];
                if (fr.rect.w() > 0 && fr.rect.h() > 0) {
                    float const imgW = static_cast<float>(image->GetWidth());
                    float const imgH = static_cast<float>(image->GetHeight());
                    moth::gfx::FloatVec2 const uv0{
                        std::clamp(static_cast<float>(fr.rect.x())      / imgW, 0.0f, 1.0f),
                        std::clamp(static_cast<float>(fr.rect.y())      / imgH, 0.0f, 1.0f) };
                    moth::gfx::FloatVec2 const uv1{
                        std::clamp(static_cast<float>(fr.rect.right())  / imgW, 0.0f, 1.0f),
                        std::clamp(static_cast<float>(fr.rect.bottom()) / imgH, 0.0f, 1.0f) };
                    float const scale = std::min(kThumbSize / static_cast<float>(fr.rect.w()),
                                                 kThumbSize / static_cast<float>(fr.rect.h()));
                    float const thumbW = static_cast<float>(fr.rect.w()) * scale;
                    float const thumbH = static_cast<float>(fr.rect.h()) * scale;
                    ImGui::SetCursorScreenPos({ boxMin.x + ((kThumbSize - thumbW) * 0.5f),
                                                boxMin.y + ((kThumbSize - thumbH) * 0.5f) });
                    DrawImage(*image, { static_cast<int>(thumbW), static_cast<int>(thumbH) }, uv0, uv1);
                }
            }

            ImGui::SetCursorScreenPos(boxMin);
            if (ImGui::InvisibleButton("##step", ImVec2{ kThumbSize, kThumbSize })) {
                // Single click: select the step's cell and move playback to the step.
                SelectClip(c);
                m_clipCurrentStep = f;
                m_clipElapsedMs = 0.0f;
                m_clipPlaying = false;
                if (maxFrameIdx >= 0) {
                    m_selectedFrame = std::clamp(frameIdx, 0, maxFrameIdx);
                }
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                m_cellPick = CellPick{ c, f };
            }

            // Keep the current step in view while playing, and after Step.
            bool const isCurrent = clipSelected && f == m_clipCurrentStep;
            if (isCurrent && (m_clipPlaying || m_scrollToClipStep)) {
                float const viewMinX = ImGui::GetWindowPos().x;
                float const viewMaxX = viewMinX + ImGui::GetWindowWidth();
                if (boxMin.x < viewMinX || boxMax.x > viewMaxX) {
                    ImGui::SetScrollHereX(0.5f);
                }
            }

            // Drag a step onto another step of the same clip to move it there.
            if (ImGui::BeginDragDropSource()) {
                StepPayload const payload{ c, f };
                ImGui::SetDragDropPayload(kStepPayloadType, &payload, sizeof(payload));
                ImGui::Text("Step %d", f + 1);
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (ImGuiPayload const* const payload = ImGui::AcceptDragDropPayload(kStepPayloadType)) {
                    StepPayload source{};
                    std::memcpy(&source, payload->Data, sizeof(source));
                    if (source.clip == c && source.step != f) {
                        stepMove = StepMove{ c, source.step, f };
                    }
                }
                ImGui::EndDragDropTarget();
            }

            bool const isPicking = m_cellPick.has_value() && m_cellPick->clip == c && m_cellPick->step == f;
            if (isPicking) {
                dl->AddRect(boxMin, boxMax, kPickColor, 0.0f, 0, 3.0f);
            } else if (isCurrent) {
                dl->AddRect(boxMin, boxMax, currentU32, 0.0f, 0, 2.0f);
            } else {
                dl->AddRect(boxMin, boxMax, IM_COL32(90, 90, 90, 255));
            }
            std::string const cellLabel = fmt::format("{}", frameIdx);
            dl->AddText({ boxMin.x + 3.0f, boxMin.y + 1.0f }, IM_COL32(255, 255, 255, 220), cellLabel.c_str());

            // Duration (ms) and remove button below the thumbnail.
            ImGui::SetCursorScreenPos({ boxMin.x, boxMax.y + style.ItemSpacing.y });
            ImGui::SetNextItemWidth(kThumbSize - removeW - style.ItemSpacing.x);
            bool const durationChanged = ImGui::InputInt("##ms", &step.durationMs, 0, 0);
            if (durationChanged) {
                step.durationMs = std::max(step.durationMs, 0);
            }
            TrackClipEdit(durationChanged);
            ImGui::SetItemTooltip("Duration (ms)");
            ImGui::SameLine();
            if (ImGui::Button("x")) {
                stepToDelete = StepPayload{ c, f };
            }
            ImGui::SetItemTooltip("Remove step");

            ImGui::EndGroup();
            ImGui::PopID();
        }

        // "+ Step" adds the selected cell at the end of the timeline.
        if (stepCount > 0) {
            ImGui::SameLine();
        }
        if (ImGui::Button("+ Step", ImVec2{ 0.0f, kThumbSize })) {
            clipToAddStep = c;
        }
        ImGui::EndChild();

        // Clicking anywhere in the clip selects it.
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            SelectClip(c);
        }
        ImGui::EndChild();
        ImGui::PopID();
    }
    if (m_clips.empty()) {
        ImGui::TextDisabled("No clips. Enter a name above and click + Clip.");
    }
    ImGui::EndChild();
    m_scrollToClipStep = false;

    if (stepMove.has_value()) {
        auto before = m_clips;
        auto& steps = m_clips[stepMove->clip].desc.frames;
        auto const moved = steps[stepMove->from];
        steps.erase(steps.begin() + stepMove->from);
        steps.insert(steps.begin() + stepMove->to, moved);
        // Playback stays on the step that moved.
        if (stepMove->clip == m_selectedClip && m_clipCurrentStep == stepMove->from) {
            m_clipCurrentStep = stepMove->to;
        }
        m_cellPick.reset();
        PushClipAction(std::move(before), m_selectedClip, m_selectedClip);
    } else if (stepToDelete.has_value()) {
        auto before = m_clips;
        auto& steps = m_clips[stepToDelete->clip].desc.frames;
        steps.erase(steps.begin() + stepToDelete->step);
        m_cellPick.reset();
        PushClipAction(std::move(before), m_selectedClip, m_selectedClip);
    } else if (clipToAddStep >= 0) {
        // Add step — defaults to the currently selected frame (or 0)
        auto before = m_clips;
        auto& steps = m_clips[clipToAddStep].desc.frames;
        moth::gfx::SpriteSheet::ClipFrame newStep;
        newStep.frameIndex = (m_selectedFrame >= 0 && maxFrameIdx >= 0)
            ? std::clamp(m_selectedFrame, 0, maxFrameIdx) : 0;
        newStep.durationMs = steps.empty() ? 100 : steps.back().durationMs;
        steps.push_back(newStep);
        PushClipAction(std::move(before), m_selectedClip, m_selectedClip);
    } else if (clipToDelete >= 0) {
        auto before = m_clips;
        int const beforeSel = m_selectedClip;
        m_clips.erase(m_clips.begin() + clipToDelete);
        if (m_selectedClip == clipToDelete) {
            m_selectedClip = -1;
            m_clipPlaying = false;
        } else if (m_selectedClip > clipToDelete) {
            --m_selectedClip;
        }
        m_cellPick.reset();
        PushClipAction(std::move(before), beforeSel, m_selectedClip);
    }
}
