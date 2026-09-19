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
    // The starting value of a clip's Set all duration box, and the duration of the steps of a new clip.
    constexpr int kDefaultStepDurationMs = 100;

    // "clip_N" with the lowest N from 1 up that no clip uses.
    std::string AutoClipName(std::vector<moth::gfx::SpriteSheet::ClipEntry> const& clips) {
        for (int n = 1;; ++n) {
            std::string name = fmt::format("clip_{}", n);
            bool const used = std::any_of(clips.begin(), clips.end(),
                                          [&name](auto const& clip) { return clip.name == name; });
            if (!used) {
                return name;
            }
        }
    }
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

bool SpriteEditor::IsStepSelected(int clipIndex, int stepIndex) const {
    return clipIndex == m_stepSelectionClip &&
           std::find(m_stepSelection.begin(), m_stepSelection.end(), stepIndex) != m_stepSelection.end();
}

void SpriteEditor::ClickClipStep(int clipIndex, int stepIndex, bool ctrl, bool shift) {
    // A selection is within one clip, so a click in another clip starts a new one.
    bool const sameClip = clipIndex == m_stepSelectionClip;
    if (shift && sameClip && m_stepSelectionAnchor >= 0) {
        int const first = std::min(m_stepSelectionAnchor, stepIndex);
        int const last = std::max(m_stepSelectionAnchor, stepIndex);
        m_stepSelection.clear();
        for (int step = first; step <= last; ++step) {
            m_stepSelection.push_back(step);
        }
        return;
    }
    if (ctrl && sameClip) {
        auto const found = std::find(m_stepSelection.begin(), m_stepSelection.end(), stepIndex);
        if (found != m_stepSelection.end()) {
            m_stepSelection.erase(found);
        } else {
            // The selection is kept in timeline order, which is the order a multi-step drag moves the steps in.
            m_stepSelection.insert(std::upper_bound(m_stepSelection.begin(), m_stepSelection.end(), stepIndex),
                                   stepIndex);
        }
        m_stepSelectionAnchor = stepIndex;
        return;
    }
    m_stepSelectionClip = clipIndex;
    m_stepSelection = { stepIndex };
    m_stepSelectionAnchor = stepIndex;
}

void SpriteEditor::ValidateStepSelection() {
    if (m_stepSelectionClip < 0 || m_stepSelectionClip >= static_cast<int>(m_clips.size())) {
        m_stepSelectionClip = -1;
        m_stepSelection.clear();
        m_stepSelectionAnchor = -1;
        return;
    }
    int const stepCount = static_cast<int>(m_clips[m_stepSelectionClip].desc.frames.size());
    m_stepSelection.erase(std::remove_if(m_stepSelection.begin(), m_stepSelection.end(),
                                         [stepCount](int step) { return step < 0 || step >= stepCount; }),
                          m_stepSelection.end());
    if (m_stepSelectionAnchor >= stepCount) {
        m_stepSelectionAnchor = m_stepSelection.empty() ? -1 : m_stepSelection.back();
    }
}

void SpriteEditor::DeleteSelectedClipSteps() {
    ValidateStepSelection();
    if (m_stepSelectionClip < 0 || m_stepSelection.empty()) {
        return;
    }
    int const clipIndex = m_stepSelectionClip;
    auto before = m_clips;
    auto& steps = m_clips[clipIndex].desc.frames;
    // Highest index first, so the indices of the steps still to remove do not move.
    int removedBeforeCurrent = 0;
    for (auto it = m_stepSelection.rbegin(); it != m_stepSelection.rend(); ++it) {
        if (*it < m_clipCurrentStep) {
            ++removedBeforeCurrent;
        }
        steps.erase(steps.begin() + *it);
    }
    // Playback moves to the step that took the current one's place, or to the new last step, as it does when one
    // step is removed.
    if (clipIndex == m_selectedClip) {
        m_clipCurrentStep -= removedBeforeCurrent;
        m_clipCurrentStep = std::clamp(m_clipCurrentStep, 0, std::max(static_cast<int>(steps.size()) - 1, 0));
    }
    m_stepSelection.clear();
    m_stepSelectionClip = -1;
    m_stepSelectionAnchor = -1;
    m_cellPick.reset();
    PushClipAction(std::move(before), m_selectedClip, m_selectedClip);
}

void SpriteEditor::MoveSelectedClipSteps(int clipIndex, int to) {
    ValidateStepSelection();
    if (clipIndex != m_stepSelectionClip || m_stepSelection.empty() || IsStepSelected(clipIndex, to)) {
        return;
    }
    auto before = m_clips;
    auto& steps = m_clips[clipIndex].desc.frames;
    int const stepCount = static_cast<int>(steps.size());
    // The moved steps go where the step dropped on is, so a drag to the right lands after it and one to the left
    // before it, as a single step's drag does.
    int insertAt = 0;
    bool selectedBeforeTarget = false;
    for (int step = 0; step < to; ++step) {
        if (IsStepSelected(clipIndex, step)) {
            selectedBeforeTarget = true;
        } else {
            ++insertAt;
        }
    }
    if (selectedBeforeTarget) {
        ++insertAt;
    }

    std::vector<moth::gfx::SpriteSheet::ClipFrame> moved;
    std::vector<moth::gfx::SpriteSheet::ClipFrame> rest;
    std::vector<int> restIndices; // where each kept step came from, so playback can follow it
    moved.reserve(m_stepSelection.size());
    rest.reserve(steps.size());
    restIndices.reserve(steps.size());
    for (int step = 0; step < stepCount; ++step) {
        if (IsStepSelected(clipIndex, step)) {
            moved.push_back(steps[step]);
        } else {
            rest.push_back(steps[step]);
            restIndices.push_back(step);
        }
    }
    std::vector<int> const movedIndices = m_stepSelection;
    rest.insert(rest.begin() + insertAt, moved.begin(), moved.end());
    restIndices.insert(restIndices.begin() + insertAt, movedIndices.begin(), movedIndices.end());
    steps = std::move(rest);

    // Playback stays on the step it was on, wherever it moved to.
    if (clipIndex == m_selectedClip) {
        for (size_t i = 0; i < restIndices.size(); ++i) {
            if (restIndices[i] == m_clipCurrentStep) {
                m_clipCurrentStep = static_cast<int>(i);
                break;
            }
        }
        m_clipCurrentStep = std::clamp(m_clipCurrentStep, 0, std::max(static_cast<int>(steps.size()) - 1, 0));
    }
    // The moved steps stay selected, in their new places.
    m_stepSelection.clear();
    for (size_t i = 0; i < moved.size(); ++i) {
        m_stepSelection.push_back(insertAt + static_cast<int>(i));
    }
    m_stepSelectionAnchor = m_stepSelection.empty() ? -1 : m_stepSelection.front();
    m_cellPick.reset();
    PushClipAction(std::move(before), m_selectedClip, m_selectedClip);
}

void SpriteEditor::DrawClipEditorWindow() {
    m_clipWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    // An undo, or a change made elsewhere, can leave the selection naming steps that no longer exist.
    ValidateStepSelection();

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
        if (ImGui::Button("+ Clip")) {
            auto before = m_clips;
            int const beforeSel = m_selectedClip;
            moth::gfx::SpriteSheet::ClipEntry newClip;
            // An empty name gets a generated one.
            newClip.name = (m_newClipNameBuffer[0] != '\0') ? std::string{ m_newClipNameBuffer } : AutoClipName(m_clips);
            newClip.desc.loop = moth::gfx::SpriteSheet::LoopType::Stop;
            // With more than one cell selected, the clip gets a step for each, in selection order.
            if (m_selection.size() > 1) {
                for (int const sel : m_selection) {
                    if (sel >= 0 && sel < static_cast<int>(m_frames.size())) {
                        moth::gfx::SpriteSheet::ClipFrame step;
                        step.frameIndex = sel;
                        step.durationMs = kDefaultStepDurationMs;
                        newClip.desc.frames.push_back(step);
                    }
                }
            }
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
    // Each step's cell number is drawn on a line above its thumbnail.
    float const stepLabelH = ImGui::GetTextLineHeight() + 2.0f;
    // A timeline row: the cell numbers, the thumbnails, the duration fields below them, and a horizontal scrollbar.
    float const timelineH = stepLabelH + kThumbSize + style.ItemSpacing.y + ImGui::GetFrameHeight() +
                            style.ScrollbarSize + style.ItemSpacing.y;
    // A clip block: a header row above its timeline, inside a bordered child.
    float const blockH = ImGui::GetFrameHeightWithSpacing() + timelineH + (style.WindowPadding.y * 2.0f);
    float const removeW = ImGui::CalcTextSize("x").x + (style.FramePadding.x * 2.0f);
    int const maxFrameIdx = static_cast<int>(m_frames.size()) - 1;
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
    bool moveSelectedSteps = false; // the dragged step was selected, so every selected step moves
    std::optional<StepPayload> stepToDelete;
    bool deleteSelectedSteps = false; // the x pressed was on a selected step, so every selected step goes
    int clipToAddStep = -1;
    int addStepDurationMs = kDefaultStepDurationMs; // the Set all value of clipToAddStep, used when it has no steps
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
        int allDurationMs = storage->GetInt(allDurationId, kDefaultStepDurationMs);
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

            // The cell number goes above the thumbnail box.
            ImVec2 const labelMin = ImGui::GetCursorScreenPos();
            ImVec2 const boxMin{ labelMin.x, labelMin.y + stepLabelH };
            ImVec2 const boxMax{ boxMin.x + kThumbSize, boxMin.y + kThumbSize };
            ImDrawList* const dl = ImGui::GetWindowDrawList();
            DrawImageBackground({ boxMin.x, boxMin.y }, { kThumbSize, kThumbSize }, kThumbCheckerSize);

            // Thumbnail of the step's cell, fitted in the box.
            int const frameIdx = step.frameIndex;
            if (frameIdx >= 0 && frameIdx <= maxFrameIdx) {
                auto const& fr = m_frames[frameIdx];
                CellDrawSource const drawSource = GetCellDrawSource(fr);
                if (drawSource.image != nullptr && fr.rect.w() > 0 && fr.rect.h() > 0) {
                    float const scale = std::min(kThumbSize / static_cast<float>(fr.rect.w()),
                                                 kThumbSize / static_cast<float>(fr.rect.h()));
                    float const thumbW = static_cast<float>(fr.rect.w()) * scale;
                    float const thumbH = static_cast<float>(fr.rect.h()) * scale;
                    ImGui::SetCursorScreenPos({ boxMin.x + ((kThumbSize - thumbW) * 0.5f),
                                                boxMin.y + ((kThumbSize - thumbH) * 0.5f) });
                    DrawImage(*drawSource.image, { static_cast<int>(thumbW), static_cast<int>(thumbH) }, drawSource.uv0,
                              drawSource.uv1);
                }
            }

            ImGui::SetCursorScreenPos(boxMin);
            if (ImGui::InvisibleButton("##step", ImVec2{ kThumbSize, kThumbSize })) {
                ImGuiIO const& io = ImGui::GetIO();
                ClickClipStep(c, f, io.KeyCtrl, io.KeyShift);
                // A plain click also selects the step's cell and moves playback to the step, as it always has.
                // Ctrl+click and Shift+click only change which steps are selected.
                if (!io.KeyCtrl && !io.KeyShift) {
                    SelectClip(c);
                    m_clipCurrentStep = f;
                    m_clipElapsedMs = 0.0f;
                    m_clipPlaying = false;
                    if (maxFrameIdx >= 0) {
                        m_selection = { std::clamp(frameIdx, 0, maxFrameIdx) };
                    }
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
            bool const stepSelected = IsStepSelected(c, f);
            bool const multiSelected = stepSelected && m_stepSelection.size() > 1;
            if (ImGui::BeginDragDropSource()) {
                StepPayload const payload{ c, f };
                ImGui::SetDragDropPayload(kStepPayloadType, &payload, sizeof(payload));
                if (multiSelected) {
                    ImGui::Text("%d steps", static_cast<int>(m_stepSelection.size()));
                } else {
                    ImGui::Text("Step %d", f + 1);
                }
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget()) {
                if (ImGuiPayload const* const payload = ImGui::AcceptDragDropPayload(kStepPayloadType)) {
                    StepPayload source{};
                    std::memcpy(&source, payload->Data, sizeof(source));
                    // Dragging a selected step moves every selected step; dragging an unselected one moves it alone.
                    bool const sourceSelected = IsStepSelected(source.clip, source.step) && m_stepSelection.size() > 1;
                    if (source.clip == c && sourceSelected && !IsStepSelected(c, f)) {
                        stepMove = StepMove{ c, source.step, f };
                        moveSelectedSteps = true;
                    } else if (source.clip == c && !sourceSelected && source.step != f) {
                        stepMove = StepMove{ c, source.step, f };
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // The selection is drawn outside the box, so a step can show that it is selected, current and picking.
            if (stepSelected) {
                auto const& sel = m_config.SpriteEditorSelectedColor;
                ImU32 const selectedU32 = ImGui::ColorConvertFloat4ToU32(
                    ImVec4{ sel.data[0], sel.data[1], sel.data[2], sel.data[3] });
                dl->AddRect({ boxMin.x - 3.0f, boxMin.y - 3.0f }, { boxMax.x + 3.0f, boxMax.y + 3.0f }, selectedU32,
                            0.0f, 0, 2.0f);
            }
            bool const isPicking = m_cellPick.has_value() && m_cellPick->clip == c && m_cellPick->step == f;
            if (isPicking) {
                dl->AddRect(boxMin, boxMax, kPickColor, 0.0f, 0, 3.0f);
            } else if (isCurrent) {
                dl->AddRect(boxMin, boxMax, currentU32, 0.0f, 0, 2.0f);
            } else {
                dl->AddRect(boxMin, boxMax, IM_COL32(90, 90, 90, 255));
            }
            // The cell number sits above the box, on the window background, so the thumbnail does not hide it.
            std::string const cellLabel = fmt::format("{}", frameIdx);
            dl->AddText(labelMin, IM_COL32(255, 255, 255, 255), cellLabel.c_str());

            // Duration (ms) and remove button below the thumbnail.
            ImGui::SetCursorScreenPos({ boxMin.x, boxMax.y + style.ItemSpacing.y });
            ImGui::SetNextItemWidth(kThumbSize - removeW - style.ItemSpacing.x);
            bool const durationChanged = ImGui::InputInt("##ms", &step.durationMs, 0, 0);
            if (durationChanged) {
                step.durationMs = std::max(step.durationMs, 0);
                // Typing under a selected step gives every selected step that duration, in one undo step, because
                // TrackClipEdit took its snapshot when the field was activated.
                if (stepSelected) {
                    for (int const selected : m_stepSelection) {
                        if (selected >= 0 && selected < stepCount) {
                            clip.desc.frames[selected].durationMs = step.durationMs;
                        }
                    }
                }
            }
            TrackClipEdit(durationChanged);
            ImGui::SetItemTooltip(multiSelected ? "Duration (ms) for every selected step" : "Duration (ms)");
            ImGui::SameLine();
            if (ImGui::Button("x")) {
                if (multiSelected) {
                    deleteSelectedSteps = true;
                } else {
                    stepToDelete = StepPayload{ c, f };
                }
            }
            ImGui::SetItemTooltip(multiSelected ? "Remove every selected step" : "Remove step");

            ImGui::EndGroup();
            ImGui::PopID();
        }

        // "+ Step" adds the selected cells at the end of the timeline, with the last step's duration, or the Set all
        // duration when the clip has no steps.
        if (stepCount > 0) {
            ImGui::SameLine();
        }
        // Line the button up with the thumbnails, below the cell numbers.
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + stepLabelH);
        if (ImGui::Button("+ Step", ImVec2{ 0.0f, kThumbSize })) {
            clipToAddStep = c;
            addStepDurationMs = std::max(allDurationMs, 0);
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
        ImGui::TextDisabled("No clips. Click + Clip to add one.");
    }
    ImGui::EndChild();
    m_scrollToClipStep = false;

    if (stepMove.has_value() && moveSelectedSteps) {
        MoveSelectedClipSteps(stepMove->clip, stepMove->to);
    } else if (stepMove.has_value()) {
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
    } else if (deleteSelectedSteps) {
        DeleteSelectedClipSteps();
    } else if (stepToDelete.has_value()) {
        DeleteClipStep(stepToDelete->clip, stepToDelete->step);
    } else if (clipToAddStep >= 0) {
        // With more than one cell selected, a step for each in selection order. Otherwise the prime cell (or 0).
        auto before = m_clips;
        auto& steps = m_clips[clipToAddStep].desc.frames;
        moth::gfx::SpriteSheet::ClipFrame newStep;
        newStep.durationMs = steps.empty() ? addStepDurationMs : steps.back().durationMs;
        if (m_selection.size() > 1) {
            for (int const sel : m_selection) {
                if (sel >= 0 && sel <= maxFrameIdx) {
                    newStep.frameIndex = sel;
                    steps.push_back(newStep);
                }
            }
        } else {
            int const prime = PrimeCell();
            newStep.frameIndex = (prime >= 0 && maxFrameIdx >= 0)
                ? std::clamp(prime, 0, maxFrameIdx) : 0;
            steps.push_back(newStep);
        }
        PushClipAction(std::move(before), m_selectedClip, m_selectedClip);
    } else if (clipToDelete >= 0) {
        auto before = m_clips;
        int const beforeSel = m_selectedClip;
        m_clips.erase(m_clips.begin() + clipToDelete);
        // The timeline selection follows its clip, and goes when that clip goes.
        if (m_stepSelectionClip == clipToDelete) {
            m_stepSelectionClip = -1;
            m_stepSelection.clear();
            m_stepSelectionAnchor = -1;
        } else if (m_stepSelectionClip > clipToDelete) {
            --m_stepSelectionClip;
        }
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
