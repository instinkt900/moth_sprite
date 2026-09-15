#pragma once

#include "editor_action.h"
#include "frame_detection.h"

#include <moth/graphics/graphics/asset_context.h>
#include <moth/graphics/graphics/spritesheet.h>
#include <moth/graphics/platform/imgui_context.h>
#include <moth/ui/layers/layer.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct SpriteEditorConfig;

// Where a pivot rule puts a cell's pivot on one axis: at 0, at half the cell's size (rounded down), or at its full size.
enum class PivotAnchor {
    Start,
    Center,
    End,
};

class SpriteEditor : public moth::ui::Layer {
public:
    // setWindowTitle sets the application window's title.
    SpriteEditor(moth::gfx::AssetContext& assetContext, moth::gfx::platform::ImGuiContext& imgui, SpriteEditorConfig& config,
                 std::function<void(std::string_view)> setWindowTitle);
    ~SpriteEditor() override = default;

    SpriteEditor(SpriteEditor const&) = delete;
    SpriteEditor(SpriteEditor&&) = delete;
    SpriteEditor& operator=(SpriteEditor const&) = delete;
    SpriteEditor& operator=(SpriteEditor&&) = delete;

    void Draw() override;

    // Called with a quit request (closing the window, or File > Exit). Returns true when the quit must wait
    // because the project has unsaved changes; the editor then asks the user, and sends the request again after
    // Save or Don't Save.
    bool HoldQuitForUnsavedChanges();

private:
    // Ctrl+Z, Ctrl+Y, Ctrl+A, Delete and Esc, in every window.
    void HandleShortcuts();
    void DrawMainMenuBar();
    // The dock space fills the application window below the main menu bar.
    void DrawDockSpace();
    void NewSpriteSheet();
    // Load a project file. On success it replaces the open project, and path becomes the project path (the window
    // title, Save and Open Recent). A failed load changes nothing.
    void LoadSpriteSheet(std::filesystem::path const& path);
    // File > Open Recent: move a loaded or saved project to the front of the list, which keeps 10 projects.
    void AddRecentProject(std::filesystem::path const& path);
    // Load a project chosen from Open Recent. A project file that no longer exists is removed from the list instead.
    // Takes a copy, because it changes the list the path comes from.
    void OpenRecentProject(std::string path);
    // Unsaved changes. The project differs from its last save when the undo position is not the one it had then,
    // or after a change that is not on the undo stack (Import Sheet).
    bool HasUnsavedChanges() const;
    void MarkSaved();
    uint64_t CurrentUndoId() const;
    // Actions that replace or close the project. With unsaved changes they wait for the unsaved changes prompt.
    enum class ProjectActionKind {
        New,
        Load,
        OpenRecent,
        Quit,
    };
    struct ProjectAction {
        ProjectActionKind kind = ProjectActionKind::New;
        std::string recentPath; // the project to open, for OpenRecent
    };
    void RequestProjectAction(ProjectAction action);
    void RunProjectAction(ProjectAction const& action);
    // The unsaved changes prompt (Save, Don't Save, Cancel) for the pending project action.
    void DrawUnsavedChangesPrompt();
    // File > Load: choose a project file in a dialog, then load it.
    void LoadWithDialog();
    // Save to the project path, or choose a path first when there is none. Returns true when the file was written.
    bool SaveProject();
    // File > Save As: choose a path in a dialog, then save. Returns true when the file was written.
    bool SaveProjectAs();
    void ImportSheet(std::filesystem::path const& imagePath);
    // Copy the sheet image file, unchanged, to exportPath (given the sheet's extension), then point the project at
    // the copy as one undoable action.
    void ExportSheet(std::filesystem::path exportPath);
    // Write the project file to path. Returns true when it was written. Only then path becomes the project path (the
    // window title, Save and Open Recent).
    bool SaveSpriteSheet(std::filesystem::path const& path);
    void DrawPreview();
    // Mouse-wheel zoom centered on the cursor, for the current scrolling child window.
    static void ZoomWithMouseWheel(float& zoom);
    // Sets the window title to "Moth Sprite - <project file name>", or "Moth Sprite - Untitled", when it changes.
    void UpdateWindowTitle();
    // The Cells window: the cell list, and a form for the selected cell below it.
    void DrawCellListWindow();
    // The Selected Cell window: the selected cell with zoom, pivot drag and pivot presets.
    void DrawCellWindow();
    // Remove cells as one undoable action, fixing up the selection and clip step indices.
    void DeleteFrames(std::vector<int> framesToDelete);
    // The prime cell: the most recently added cell that is still selected, or -1.
    int PrimeCell() const;
    bool IsCellSelected(int frameIndex) const;
    // Ctrl+click: add the cell to the selection as the prime cell, or remove it.
    void ToggleCellSelection(int frameIndex);
    // Set the pivot of every selected cell by a rule, relative to each cell's own size, as one undoable action.
    void SetSelectionPivot(PivotAnchor x, PivotAnchor y);
    // Clip playback advances once per frame, whichever windows are open.
    void AdvanceClipPlayback();
    // Play/Pause and Step buttons for the selected clip, with its current step.
    void DrawClipPlaybackControls();
    // The Clip Preview window: the selected clip's animation, anchored on each cell's pivot, with zoom and the
    // same playback controls as the Clips window.
    void DrawClipPreviewWindow();
    // The Clips window: every clip as a timeline of steps.
    void DrawClipEditorWindow();
    // Selecting a different clip moves playback to its first step.
    void SelectClip(int clipIndex);
    // Remove one step from a clip as one undoable action. Playback stays on a step that still exists.
    void DeleteClipStep(int clipIndex, int stepIndex);
    // Select only this cell (-1 clears the selection) from the cell list or the sheet. While picking a cell for a
    // clip step, also assigns it to the step as one undoable action.
    void SelectCell(int frameIndex);
    // Undo for a text or number input in the Clips window. Call right after the widget, with its return value.
    void TrackClipEdit(bool changed);
    void CommitClipEdit();
    // Undo for a number input in the Cells form. Call right after the widget, with its return value, before the value
    // is applied to the cell. Returns true when the edit ended: apply the value, then call CommitFrameEdit.
    bool TrackFrameEdit(bool changed);
    void CommitFrameEdit();
    void DrawGridTool();
    void DrawDetectFramesTool();
    // Append one frame per rect (pivot 0,0) as a single undoable action and select the first.
    void AppendFrames(std::vector<moth::gfx::IntRect> const& rects);
    void DrawImage(moth::gfx::Image const& image, moth::gfx::IntVec2 const& size,
                   moth::gfx::FloatVec2 const& uv0 = { 0.0f, 0.0f },
                   moth::gfx::FloatVec2 const& uv1 = { 1.0f, 1.0f });
    // The preview background for the screen area at pos with size, drawn before the image: the Preferences color,
    // or a gray and white checkerboard when its alpha is 0. Squares are checkerSize screen pixels, counted from pos.
    void DrawImageBackground(moth::gfx::FloatVec2 const& pos, moth::gfx::FloatVec2 const& size,
                             float checkerSize = 128.0f) const;

    using FrameVec = std::vector<moth::gfx::SpriteSheet::FrameEntry>;
    using ClipVec  = std::vector<moth::gfx::SpriteSheet::ClipEntry>;
    // Selected cell indices in the order they were added. The last one is the prime cell.
    using Selection = std::vector<int>;

    // Undo/redo stack
    void AddSpriteAction(std::unique_ptr<IEditorAction> action);
    void UndoSpriteAction();
    void RedoSpriteAction();
    void ClearSpriteActions();

    // Snapshot helpers — capture current state as "after" and push a reversible action
    void PushFrameAction(FrameVec before, Selection selBefore, Selection selAfter);
    void PushClipAction(ClipVec before, int selBefore, int selAfter);
    void PushFrameClipAction(FrameVec beforeF, ClipVec beforeC, Selection selBefore, Selection selAfter);

    moth::gfx::AssetContext& m_assetContext;
    moth::gfx::platform::ImGuiContext& m_imgui;
    SpriteEditorConfig& m_config;
    std::function<void(std::string_view)> m_setWindowTitle;
    std::string m_windowTitle; // the title last set, so that it is only set again when it changes
    bool m_resetLayout = false; // set by Window > Reset Layout; applied before the dock space is drawn
    char m_pathBuffer[1024] = {};
    char m_imagePathBuffer[1024] = {};
    std::shared_ptr<moth::gfx::SpriteSheet> m_spriteSheet;
    std::vector<moth::gfx::SpriteSheet::FrameEntry> m_frames;
    std::vector<moth::gfx::SpriteSheet::ClipEntry> m_clips;
    Selection m_selection;
    float m_zoom = 1.0f; // -1 = auto-fit on next draw
    float m_cellZoom = -1.0f; // Selected Cell window zoom; -1 = auto-fit on next draw
    float m_clipZoom = -1.0f; // Clip Preview window zoom; -1 = auto-fit on next draw
    char m_newClipNameBuffer[256] = {};
    int m_selectedClip = -1;
    bool m_clipPlaying = false;
    int m_clipCurrentStep = 0;
    float m_clipElapsedMs = 0.0f;

    // Undo stack
    std::vector<std::unique_ptr<IEditorAction>> m_undoStack;
    int m_undoIndex = -1;
    std::vector<uint64_t> m_undoIds;  // an id for each m_undoStack entry; ids are never reused
    uint64_t m_nextUndoId = 1;
    uint64_t m_savedUndoId = 0;        // CurrentUndoId() when the project was last saved, loaded or created
    bool m_unsavedOutsideUndo = false; // a change that is not on the undo stack, such as Import Sheet

    // Unsaved changes prompt
    std::optional<ProjectAction> m_pendingProjectAction; // the action waiting for the user's answer
    bool m_openUnsavedPrompt = false;                    // open the prompt on the next draw
    bool m_quitApproved = false;                         // the user answered for a quit; let the next request through

    // A Cells form input being edited. id is the widget's ImGuiID, so that focus moving straight from one field to
    // another commits the first edit before the second snapshot is taken.
    struct PendingFrameEdit {
        unsigned int id = 0;
        FrameVec snapshot;
        bool edited = false;
    };
    std::optional<PendingFrameEdit> m_pendingFrameEdit;
    // A Clips window input being edited. id is the widget's ImGuiID, so that focus moving straight from one field
    // to another commits the first edit before the second snapshot is taken.
    struct PendingClipEdit {
        unsigned int id = 0;
        ClipVec snapshot;
        bool edited = false;
    };
    std::optional<PendingClipEdit> m_pendingClipEdit;

    // Double-clicking a clip step starts picking a cell for it. The next cell selected is assigned to the step.
    struct CellPick {
        int clip = 0;
        int step = 0;
    };
    std::optional<CellPick> m_cellPick;
    bool m_scrollToClipStep = false; // set by Step; the timeline scrolls to show the current step
    bool m_clipWindowFocused = false; // the Clips window had focus when last drawn; Delete then removes a step
    float m_cellFormHeight = 0.0f;    // the Cells form's height when last drawn, so the list leaves room for it

    // Pivot drag state (click-drag in the Selected Cell window)
    bool m_pivotDragging = false;
    std::optional<FrameVec> m_pivotDragSnapshot;

    // Frame drag/resize state (click-drag in the preview canvas).
    // op is one of the FrameDragOp enum values (defined in sprite_editor_preview.cpp);
    // stored as int to avoid exposing imgui.h from this header.
    struct FrameDragState {
        int op;
        FrameVec snapshot;
        // The selected cell pressed to start a move. If the press moves nothing, only that cell stays selected.
        int clickedCell = -1;
    };
    std::optional<FrameDragState> m_frameDrag;

    // Box selection on the sheet: the image-space start point, and whether the box adds to the selection (Ctrl).
    struct BoxSelectState {
        float startX = 0.0f;
        float startY = 0.0f;
        bool additive = false;
    };
    std::optional<BoxSelectState> m_boxSelect;

    // "New Cell" mode: the next click-drag on the preview canvas draws a new frame rect.
    bool m_newCellMode = false;
    std::optional<moth::gfx::IntVec2> m_newCellAnchor; // image-space drag start, set while dragging

    // Tools > Grid popup state. Values persist between openings.
    struct GridToolState {
        int cellW    = 32;
        int cellH    = 32;
        int offsetX  = 0;
        int offsetY  = 0;
        int spacingX = 0;
        int spacingY = 0;
        int rows     = 1;
        int cols     = 1;
    };
    GridToolState m_gridTool;
    bool m_openGridTool = false; // set by the menu; the popup is opened outside the menu's ID scope

    // Tools > Detect Frames popup state. Options persist between openings.
    struct DetectToolState {
        FrameDetectOptions options;
        std::optional<ImagePixels> pixels; // decoded sheet, held only while the popup is open
        FrameDetectResult result;
        bool dirty = true;                 // re-run detection on the next draw
    };
    DetectToolState m_detectTool;
    bool m_openDetectTool = false;
};
