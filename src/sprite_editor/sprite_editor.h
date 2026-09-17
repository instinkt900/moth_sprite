#pragma once

#include "editor_action.h"
#include "frame_detection.h"
#include "sheet_packing.h"

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

// The image file of a cell imported from an image other than the sheet.
struct CellImage {
    std::string path;        // absolute
    moth::gfx::Image image;  // empty when the file could not be loaded
};

// A cell: a rectangle of the sheet image and a pivot. A cell imported from another image has a source, and is the
// whole of that image: its rectangle is (0, 0) and the image size. The source is shared, so the cell list and undo
// snapshots keep its texture alive.
struct CellEntry : moth::gfx::SpriteSheet::FrameEntry {
    std::shared_ptr<CellImage const> source;
};

// What a cell is drawn from: an image and the UVs of the cell in it. image is null when there is nothing to draw.
struct CellDrawSource {
    moth::gfx::Image const* image = nullptr;
    moth::gfx::FloatVec2 uv0{ 0.0f, 0.0f };
    moth::gfx::FloatVec2 uv1{ 1.0f, 1.0f };
};

// Where a pivot rule puts a cell's pivot on one axis: at 0, at half the cell's size (rounded down), or at its full size.
enum class PivotAnchor {
    Start,
    Center,
    End,
};

class SpriteEditor : public moth::ui::Layer {
public:
    // setWindowTitle sets the application window's title. waitForGpu blocks until the GPU has finished every submitted
    // frame, so a texture that recent frames drew can be freed.
    SpriteEditor(moth::gfx::AssetContext& assetContext, moth::gfx::platform::ImGuiContext& imgui, SpriteEditorConfig& config,
                 std::function<void(std::string_view)> setWindowTitle, std::function<void()> waitForGpu);
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
    // Ctrl+S, Ctrl+Shift+S, Ctrl+N, Ctrl+L, Ctrl+X, Ctrl+Z, Ctrl+Y, Ctrl+A, Delete and Esc, in every window.
    void HandleShortcuts();
    void DrawMainMenuBar();
    // The dock space fills the application window below the main menu bar.
    void DrawDockSpace();
    void NewSpriteSheet();
    // The extension of project files.
    static constexpr char const* kProjectExtension = ".mothsprite";
    // Open a file from File > Load or Open Recent: a sprite sheet descriptor (.json) is imported with
    // ImportDescriptor, and any other file is loaded as a project file with LoadProjectFile.
    void LoadSpriteSheet(std::filesystem::path const& path);
    // Load a project file. On success it replaces the open project, and path becomes the project path (the window
    // title, Save and Open Recent). A failed load changes nothing.
    void LoadProjectFile(std::filesystem::path const& path);
    // Make a new project from a sprite sheet descriptor, the format games load. The project has no path and has
    // unsaved changes. A descriptor that SpriteSheetFactory does not load changes nothing.
    void ImportDescriptor(std::filesystem::path const& path);
    // File > Open Recent: move a loaded or saved project to the front of the list, which keeps 10 projects.
    void AddRecentProject(std::filesystem::path const& path);
    // Load a project chosen from Open Recent. A project file that no longer exists is removed from the list instead.
    // Takes a copy, because it changes the list the path comes from.
    void OpenRecentProject(std::string path);
    // Unsaved changes. The project differs from its last save when the undo position is not the one it had then.
    bool HasUnsavedChanges() const;
    void MarkSaved();
    // The project differs from its last save until it is saved, even with nothing to undo.
    void MarkUnsaved();
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
    // Help > About: the tool's name, version, description, author and repository.
    void DrawAboutDialog();
    // File > Load: choose a project file in a dialog, then load it.
    void LoadWithDialog();
    // Save to the project path, or choose a path first when there is none. Returns true when the file was written.
    bool SaveProject();
    // File > Save As: choose a path in a dialog, then save. Returns true when the file was written.
    bool SaveProjectAs();
    // Replace the sheet image, keeping the cells and clips, as one undoable action.
    void ImportSheet(std::filesystem::path const& imagePath);
    // File > Import Sheet and the Sheet window's Import Sheet button: choose an image in a dialog, then import it.
    void ImportSheetWithDialog();
    // Add one cell for each image, at the end of the cell list, as one undoable action. Each cell is the whole image
    // with its pivot at (0, 0). An image that does not load is skipped; when none load, nothing changes.
    void ImportCells(std::vector<std::filesystem::path> const& imagePaths);
    // The Cells window's Import button: choose images in a dialog, then import them as cells.
    void ImportCellsWithDialog();
    // File > Export (choosePath false) and File > Export As (choosePath true): write the sprite sheet descriptor
    // that games load, and copy the sheet image beside it. Export uses the project's export path, and chooses one in
    // a dialog when there is none. A new export path is set as one undoable action after a successful export.
    // Refuses, writing no files, when ExportProblems finds any. A project with cells from other images gets its path
    // first, then the pack dialog, and is exported after a successful pack.
    void ExportProject(bool choosePath);
    // Write the descriptor to exportPath (absolute) and copy the sheet image beside it, then set the export path.
    void ExportToPath(std::filesystem::path const& exportPath);
    // What stops the project from being exported as game data, one message each. Empty when it can be exported.
    std::vector<std::string> ExportProblems() const;
    // Show the export message popup on the next draw.
    void ShowExportMessage(std::string heading, std::vector<std::string> lines);
    void DrawExportMessage();
    // File > Pack: the pack dialog. Pack runs PackProject with the dialog's settings.
    void DrawPackDialog();
    // The pack dialog's preview of the packed image for its current settings. Writes no files.
    void UpdatePackPreview();
    // Free the preview texture. Frames still in flight may use it, so this waits for the GPU first.
    void ReleasePackPreviewImage();
    void DrawPackPreview();
    // The settings the pack dialog opens with: the project's, or defaults with the path <project name>_packed.<ext>.
    PackSettings InitialPackSettings() const;
    // The cells to pack: each cell's source image and rectangle. Returns nothing, and sets error, when a cell has no
    // image to read.
    std::optional<std::vector<PackCell>> ProjectPackCells(std::string& error) const;
    // Pack every cell into a new image with settings, write it, and use it as the sheet: the sheet image, the cell
    // rectangles and the pack settings change as one undoable action. Cells from other images become sheet cells. Returns false, sets error and changes nothing
    // when the pack fails.
    bool PackProject(PackSettings const& settings, std::string& error);
    // Write the project file (the .mothsprite format) to path. Returns true when it was written. Only then path becomes the project path (the
    // window title, Save and Open Recent).
    bool SaveSpriteSheet(std::filesystem::path const& path);
    void DrawPreview();
    // Mouse-wheel zoom centered on the cursor, for the current scrolling child window.
    static void ZoomWithMouseWheel(float& zoom);
    // Sets the window title to "Moth Sprite - <project file name>", or "Moth Sprite - Untitled", when it changes.
    void UpdateWindowTitle();
    // The Cells window: the cell list, and a form for the selected cell below it.
    void DrawCellListWindow();
    // The Selected Cell window: the playback buttons, and the prime cell with zoom and pivot drag. With a clip
    // selected, the cell is placed on its pivot inside the clip's bounding box, so the window previews the clip.
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
    // Select the selected clip's current step's cell, as a click on the step does, so every window shows the same
    // cell while the clip plays or steps. Does nothing during a drag on the sheet or on the pivot.
    void SelectClipStepCell();
    // Play/Pause and Step buttons for the selected clip, with its current step.
    void DrawClipPlaybackControls();
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
                             float checkerSize = 32.0f) const;

    using FrameVec = std::vector<CellEntry>;
    // The cells as moth_graphics frame entries, without their sources, for building a SpriteSheet.
    static std::vector<moth::gfx::SpriteSheet::FrameEntry> ToFrameEntries(FrameVec const& cells);
    // The image and UVs to draw a cell with: its own image, or its rectangle of the sheet image.
    CellDrawSource GetCellDrawSource(CellEntry const& cell) const;
    using ClipVec  = std::vector<moth::gfx::SpriteSheet::ClipEntry>;
    // Selected cell indices in the order they were added. The last one is the prime cell.
    using Selection = std::vector<int>;

    // Replace the open project with a sheet, its image path, cells and clips. Clears the undo history, the selection
    // and clip playback, and fits the views. The caller sets the project path and the saved state.
    void ReplaceProject(std::shared_ptr<moth::gfx::SpriteSheet> sheet, std::string const& imagePath,
                        FrameVec frames, ClipVec clips);

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
    std::function<void()> m_waitForGpu;
    std::string m_windowTitle; // the title last set, so that it is only set again when it changes
    bool m_resetLayout = false; // set by Window > Reset Layout; applied before the dock space is drawn
    char m_pathBuffer[1024] = {};
    char m_imagePathBuffer[1024] = {};
    std::string m_exportPath; // absolute path of the last export's descriptor, or empty; saved in the project file
    std::shared_ptr<moth::gfx::SpriteSheet> m_spriteSheet;
    FrameVec m_frames;
    std::vector<moth::gfx::SpriteSheet::ClipEntry> m_clips;
    Selection m_selection;
    float m_zoom = 1.0f; // -1 = auto-fit on next draw
    float m_cellZoom = -1.0f; // Selected Cell window zoom; -1 = auto-fit on next draw
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

    // Unsaved changes prompt
    std::optional<ProjectAction> m_pendingProjectAction; // the action waiting for the user's answer
    bool m_openUnsavedPrompt = false;                    // open the prompt on the next draw
    bool m_quitApproved = false;                         // the user answered for a quit; let the next request through
    bool m_openAboutDialog = false;                      // set by Help > About; opened outside the menu's ID scope

    // The export message popup: why an export was refused or failed.
    struct ExportMessage {
        std::string heading;
        std::vector<std::string> lines;
        bool open = false; // open the popup on the next draw
    };
    ExportMessage m_exportMessage;

    // File > Pack. The project's settings are set by a successful pack, and saved in the project file.
    std::optional<PackSettings> m_packSettings;
    struct PackDialogState {
        PackSettings settings;     // being edited; the project changes only when Pack succeeds
        PackSettings bestPackSizes; // the sizes shown, disabled, while Best pack is on
        char pathBuffer[1024] = {};
        std::string error;         // why the last Pack failed
        // The preview, packed in memory. Sources are read once while the dialog is open.
        PackImageCache previewImages;
        std::optional<PackSettings> previewSettings; // the settings the preview was packed with; unset = pack again
        moth::gfx::Image previewImage;
        int previewWidth = 0;
        int previewHeight = 0;
        std::string previewError; // why the preview could not be packed
    };
    PackDialogState m_packDialog;
    bool m_openPackDialog = false; // set by the menu; the popup is opened outside the menu's ID scope
    // Set when Export opened the pack dialog because the project has cells from other images: the descriptor path
    // chosen for the export. The dialog's packed image is named after it, and the export continues after a successful
    // pack. Cleared when the dialog closes.
    std::optional<std::filesystem::path> m_exportAfterPack;

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
