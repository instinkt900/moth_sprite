#pragma once

#include "editor_action.h"
#include "frame_detection.h"

#include <moth/graphics/graphics/asset_context.h>
#include <moth/graphics/graphics/spritesheet.h>
#include <moth/graphics/platform/imgui_context.h>
#include <moth/ui/layers/layer.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

struct SpriteEditorConfig;

class SpriteEditor : public moth::ui::Layer {
public:
    SpriteEditor(moth::gfx::AssetContext& assetContext, moth::gfx::platform::ImGuiContext& imgui, SpriteEditorConfig& config);
    ~SpriteEditor() override = default;

    SpriteEditor(SpriteEditor const&) = delete;
    SpriteEditor(SpriteEditor&&) = delete;
    SpriteEditor& operator=(SpriteEditor const&) = delete;
    SpriteEditor& operator=(SpriteEditor&&) = delete;

    void Draw() override;

private:
    void NewSpriteSheet();
    void LoadSpriteSheet(std::filesystem::path const& path);
    void ImportSheet(std::filesystem::path const& imagePath);
    void SaveSpriteSheet();
    void DrawPreview();
    void DrawDataEditor();
    void DrawFramesPane();
    // Remove a frame as one undoable action, fixing up the selection and clip step indices.
    void DeleteFrame(int frameToDelete);
    void DrawClipsPane();
    void DrawGridTool();
    void DrawDetectFramesTool();
    // Append one frame per rect (pivot 0,0) as a single undoable action and select the first.
    void AppendFrames(std::vector<moth::gfx::IntRect> const& rects);
    void DrawImage(moth::gfx::Image const& image, moth::gfx::IntVec2 const& size,
                   moth::gfx::FloatVec2 const& uv0 = { 0.0f, 0.0f },
                   moth::gfx::FloatVec2 const& uv1 = { 1.0f, 1.0f });

    using FrameVec = std::vector<moth::gfx::SpriteSheet::FrameEntry>;
    using ClipVec  = std::vector<moth::gfx::SpriteSheet::ClipEntry>;

    // Undo/redo stack
    void AddSpriteAction(std::unique_ptr<IEditorAction> action);
    void UndoSpriteAction();
    void RedoSpriteAction();
    void ClearSpriteActions();

    // Snapshot helpers — capture current state as "after" and push a reversible action
    void PushFrameAction(FrameVec before, int selBefore, int selAfter);
    void PushClipAction(ClipVec before, int selBefore, int selAfter);
    void PushFrameClipAction(FrameVec beforeF, ClipVec beforeC, int selBefore, int selAfter);

    moth::gfx::AssetContext& m_assetContext;
    moth::gfx::platform::ImGuiContext& m_imgui;
    SpriteEditorConfig& m_config;
    char m_pathBuffer[1024] = {};
    char m_imagePathBuffer[1024] = {};
    std::shared_ptr<moth::gfx::SpriteSheet> m_spriteSheet;
    std::vector<moth::gfx::SpriteSheet::FrameEntry> m_frames;
    std::vector<moth::gfx::SpriteSheet::ClipEntry> m_clips;
    int m_selectedFrame = -1;
    float m_zoom = 1.0f; // -1 = auto-fit on next draw
    char m_newClipNameBuffer[256] = {};
    int m_selectedClip = -1;
    bool m_clipPlaying = false;
    int m_clipCurrentStep = 0;
    float m_clipElapsedMs = 0.0f;

    // Undo stack
    std::vector<std::unique_ptr<IEditorAction>> m_undoStack;
    int m_undoIndex = -1;

    // Deferred InputInt/InputText snapshots (captured on activate, committed on deactivate)
    std::optional<FrameVec> m_pendingFrameSnapshot;
    std::optional<ClipVec>  m_pendingClipSnapshot;

    // Pivot drag state (click-drag in frame mini-preview)
    bool m_pivotDragging = false;
    std::optional<FrameVec> m_pivotDragSnapshot;

    // Frame drag/resize state (click-drag in the preview canvas).
    // op is one of the FrameDragOp enum values (defined in sprite_editor_preview.cpp);
    // stored as int to avoid exposing imgui.h from this header.
    struct FrameDragState {
        int op;
        FrameVec snapshot;
    };
    std::optional<FrameDragState> m_frameDrag;

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
