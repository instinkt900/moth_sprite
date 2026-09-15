#include "common.h"
#include "sprite_editor.h"
#include "sprite_editor_config.h"

namespace {
    // Side of a step thumbnail on a clip timeline, in pixels.
    constexpr float kThumbSize = 72.0f;
    // Checkerboard square size behind a step thumbnail, in pixels. Smaller than elsewhere, to fit the thumbnail.
    constexpr float kThumbCheckerSize = 8.0f;
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
    int const startStep = m_clipCurrentStep;
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
    // Only when the step changes, so a cell selected while the clip plays stays selected until the next step.
    if (m_clipCurrentStep != startStep) {
        SelectClipStepCell();
    }
}

void SpriteEditor::SelectClipStepCell() {
    // A drag on the sheet or on the pivot works on the selection, so it keeps the selection it started with.
    if (m_frameDrag.has_value() || m_boxSelect.has_value() || m_pivotDragging) {
        return;
    }
    if (m_selectedClip < 0 || m_selectedClip >= static_cast<int>(m_clips.size())) {
        return;
    }
    auto const& steps = m_clips[m_selectedClip].desc.frames;
    int const maxFrameIdx = static_cast<int>(m_frames.size()) - 1;
    if (steps.empty() || maxFrameIdx < 0) {
        return;
    }
    int const step = std::clamp(m_clipCurrentStep, 0, static_cast<int>(steps.size()) - 1);
    m_selection = { std::clamp(steps[step].frameIndex, 0, maxFrameIdx) };
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
            SelectClipStepCell();
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
        SelectClipStepCell();
        m_scrollToClipStep = true;
    }
    ImGui::SameLine();
    // Reset goes back to the first step. A playing clip keeps playing from there.
    if (ImGui::Button("Reset") && totalSteps > 0) {
        m_clipCurrentStep = 0;
        m_clipElapsedMs = 0.0f;
        SelectClipStepCell();
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
    m_selection.clear();
    if (frameIndex >= 0) {
        m_selection.push_back(frameIndex);
    }
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

void SpriteEditor::DeleteClipStep(int clipIndex, int stepIndex) {
    if (clipIndex < 0 || clipIndex >= static_cast<int>(m_clips.size()) ||
        stepIndex < 0 || stepIndex >= static_cast<int>(m_clips[clipIndex].desc.frames.size())) {
        return;
    }
    auto before = m_clips;
    auto& steps = m_clips[clipIndex].desc.frames;
    steps.erase(steps.begin() + stepIndex);
    // Playback moves to the step that took the removed one's place, or to the new last step.
    if (clipIndex == m_selectedClip) {
        if (m_clipCurrentStep > stepIndex) {
            --m_clipCurrentStep;
        }
        m_clipCurrentStep = std::clamp(m_clipCurrentStep, 0, std::max(static_cast<int>(steps.size()) - 1, 0));
    }
    m_cellPick.reset();
    PushClipAction(std::move(before), m_selectedClip, m_selectedClip);
}

void SpriteEditor::DrawClipEditorWindow() {
    m_clipWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

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
    // The current step has the prime cell's colour, because playback makes the step's cell the prime cell.
    ImU32 const currentU32 = ImGui::ColorConvertFloat4ToU32(ImVec4{
        m_config.SpriteEditorPrimeColor.data[0], m_config.SpriteEditorPrimeColor.data[1],
        m_config.SpriteEditorPrimeColor.data[2], m_config.SpriteEditorPrimeColor.data[3] });

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

        // A duration for every step. Typing only changes the field; Set all applies it, so a stray edit changes
        // nothing. The typed value is view state, kept in the clip block's ImGui storage.
        ImGui::SameLine();
        ImGuiStorage* const storage = ImGui::GetStateStorage();
        ImGuiID const allDurationId = ImGui::GetID("##all_ms_value");
        int allDurationMs = storage->GetInt(allDurationId, 100);
        ImGui::SetNextItemWidth(64.0f);
        if (ImGui::InputInt("##all_ms", &allDurationMs, 0, 0)) {
            storage->SetInt(allDurationId, std::max(allDurationMs, 0));
        }
        ImGui::SetItemTooltip("Duration (ms) for every step");
        ImGui::SameLine();
        ImGui::BeginDisabled(clip.desc.frames.empty());
        if (ImGui::Button("Set all")) {
            // A duration field still being edited gets its own undo step first.
            CommitClipEdit();
            int const durationMs = std::max(allDurationMs, 0);
            auto before = m_clips;
            bool changed = false;
            for (auto& step : clip.desc.frames) {
                changed |= (step.durationMs != durationMs);
                step.durationMs = durationMs;
            }
            if (changed) {
                PushClipAction(std::move(before), m_selectedClip, m_selectedClip);
            }
        }
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Set every step's duration to this value");

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
            DrawImageBackground({ boxMin.x, boxMin.y }, { kThumbSize, kThumbSize }, kThumbCheckerSize);

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
                    m_selection = { std::clamp(frameIdx, 0, maxFrameIdx) };
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
            // A black copy 1 px down and right gives the white number a drop shadow, so it reads on light backgrounds.
            dl->AddText({ boxMin.x + 4.0f, boxMin.y + 2.0f }, IM_COL32(0, 0, 0, 255), cellLabel.c_str());
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
        DeleteClipStep(stepToDelete->clip, stepToDelete->step);
    } else if (clipToAddStep >= 0) {
        // Add step — defaults to the prime cell (or 0)
        auto before = m_clips;
        auto& steps = m_clips[clipToAddStep].desc.frames;
        moth::gfx::SpriteSheet::ClipFrame newStep;
        int const prime = PrimeCell();
        newStep.frameIndex = (prime >= 0 && maxFrameIdx >= 0)
            ? std::clamp(prime, 0, maxFrameIdx) : 0;
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
