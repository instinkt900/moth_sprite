#include "common.h"
#include "sprite_editor.h"
#include "sprite_editor_config.h"

#include <moth/graphics/events/event_window.h>

#include <imgui_internal.h>
#include <nfd.h>

namespace {
    // Window names are also the IDs that imgui.ini uses to remember the layout. Do not rename them.
    char const* const kSheetWindow = "Sheet";
    char const* const kCellWindow = "Selected Cell";
    char const* const kCellListWindow = "Cells";
    char const* const kClipEditorWindow = "Clips";
    char const* const kClipPreviewWindow = "Clip Preview";
    char const* const kDockSpaceHostWindow = "##dock_space_host";
    char const* const kDockSpaceId = "##dock_space";

    // Replace the dock space's layout with the built-in default.
    void BuildDefaultLayout(ImGuiID dockSpaceId, ImVec2 size) {
        ImGui::DockBuilderRemoveNode(dockSpaceId);
        ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockSpaceId, size);
        ImGuiID leftId = 0;
        ImGuiID rightId = 0;
        ImGui::DockBuilderSplitNode(dockSpaceId, ImGuiDir_Left, 0.6f, &leftId, &rightId);
        ImGuiID clipsId = 0;
        ImGuiID sheetId = 0;
        ImGui::DockBuilderSplitNode(leftId, ImGuiDir_Down, 0.35f, &clipsId, &sheetId);
        ImGuiID clipPreviewId = 0;
        ImGuiID clipEditorId = 0;
        ImGui::DockBuilderSplitNode(clipsId, ImGuiDir_Right, 0.3f, &clipPreviewId, &clipEditorId);
        ImGuiID cellId = 0;
        ImGuiID cellListId = 0;
        ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Up, 0.4f, &cellId, &cellListId);
        ImGui::DockBuilderDockWindow(kSheetWindow, sheetId);
        ImGui::DockBuilderDockWindow(kCellWindow, cellId);
        ImGui::DockBuilderDockWindow(kCellListWindow, cellListId);
        ImGui::DockBuilderDockWindow(kClipEditorWindow, clipEditorId);
        ImGui::DockBuilderDockWindow(kClipPreviewWindow, clipPreviewId);
        ImGui::DockBuilderFinish(dockSpaceId);
    }
} // namespace

SpriteEditor::SpriteEditor(moth::gfx::AssetContext& assetContext, moth::gfx::platform::ImGuiContext& imgui, SpriteEditorConfig& config,
                           std::function<void(std::string_view)> setWindowTitle)
    : m_assetContext(assetContext)
    , m_imgui(imgui)
    , m_config(config)
    , m_setWindowTitle(std::move(setWindowTitle)) {
    // Start with a blank project so the user can import a sheet straight away.
    NewSpriteSheet();
}

void SpriteEditor::NewSpriteSheet() {
    ClearSpriteActions();
    m_pathBuffer[0]      = '\0';
    m_imagePathBuffer[0] = '\0';
    m_frames.clear();
    m_clips.clear();
    m_selection.clear();
    m_selectedClip    = -1;
    m_clipPlaying     = false;
    m_clipCurrentStep = 0;
    m_clipElapsedMs   = 0.0f;
    m_zoom            = 1.0f;
    m_cellZoom        = -1.0f;
    m_clipZoom        = -1.0f;
    m_spriteSheet     = std::make_shared<moth::gfx::SpriteSheet>(
        moth::gfx::Image{},
        std::vector<moth::gfx::SpriteSheet::FrameEntry>{},
        std::vector<moth::gfx::SpriteSheet::ClipEntry>{});
}

void SpriteEditor::DrawImage(moth::gfx::Image const& image, moth::gfx::IntVec2 const& size,
                             moth::gfx::FloatVec2 const& uv0, moth::gfx::FloatVec2 const& uv1) {
    if (image) {
        m_imgui.Image(*image.GetTexture(), size, uv0, uv1);
    }
}

void SpriteEditor::UpdateWindowTitle() {
    std::string const fileName = std::filesystem::path(m_pathBuffer).filename().string();
    std::string title = fmt::format("Moth Sprite - {}", fileName.empty() ? "Untitled" : fileName);
    if (title == m_windowTitle || !m_setWindowTitle) {
        return;
    }
    m_windowTitle = std::move(title);
    m_setWindowTitle(m_windowTitle);
}

void SpriteEditor::HandleShortcuts() {
    // Skip while a text field is active so the keys still edit the text, and while a modal
    // tool popup is open, because the popup blocks the rest of the editor.
    auto const& io = ImGui::GetIO();
    if (io.WantTextInput || ImGui::GetTopMostPopupModal() != nullptr) {
        return;
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        UndoSpriteAction();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        RedoSpriteAction();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
        // Select every cell. The prime cell stays prime.
        int const cellCount = static_cast<int>(m_frames.size());
        int const prime = PrimeCell();
        m_selection.clear();
        for (int i = 0; i < cellCount; ++i) {
            if (i != prime) {
                m_selection.push_back(i);
            }
        }
        if (prime >= 0 && prime < cellCount) {
            m_selection.push_back(prime);
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        DeleteFrames(m_selection);
    }
    // Esc cancels one thing: picking a cell for a clip step, else New Cell drawing, else the selection.
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        if (m_cellPick.has_value()) {
            m_cellPick.reset();
        } else if (m_newCellMode) {
            m_newCellMode = false;
            m_newCellAnchor.reset();
        } else if (!m_frameDrag.has_value() && !m_boxSelect.has_value()) {
            m_selection.clear();
        }
    }
}

void SpriteEditor::DrawMainMenuBar() {
    auto const doLoad = [this]() {
        nfdchar_t* outPath = nullptr;
        std::string const currentPath = std::filesystem::current_path().string();
        if (NFD_OpenDialog("json", currentPath.c_str(), &outPath) == NFD_OKAY && outPath != nullptr) {
            strncpy(m_pathBuffer, outPath, sizeof(m_pathBuffer) - 1);
            m_pathBuffer[sizeof(m_pathBuffer) - 1] = '\0';
            NFD_Free(outPath);
            LoadSpriteSheet(m_pathBuffer);
        }
    };

    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New")) {
            NewSpriteSheet();
        }
        if (ImGui::MenuItem("Load...")) {
            doLoad();
        }
        bool const hasImage = m_imagePathBuffer[0] != '\0';
        bool const hasPath  = m_pathBuffer[0] != '\0';
        if (ImGui::MenuItem("Save", nullptr, false, hasImage && hasPath)) {
            SaveSpriteSheet();
        }
        if (ImGui::MenuItem("Save As...", nullptr, false, hasImage)) {
            nfdchar_t* outPath = nullptr;
            std::string const currentPath = std::filesystem::current_path().string();
            if (NFD_SaveDialog("json", currentPath.c_str(), &outPath) == NFD_OKAY && outPath != nullptr) {
                strncpy(m_pathBuffer, outPath, sizeof(m_pathBuffer) - 1);
                m_pathBuffer[sizeof(m_pathBuffer) - 1] = '\0';
                NFD_Free(outPath);
                SaveSpriteSheet();
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Import Sheet...", nullptr, false, m_spriteSheet != nullptr)) {
            nfdchar_t* outPath = nullptr;
            std::string const currentPath = std::filesystem::current_path().string();
            if (NFD_OpenDialog("png,jpg,jpeg,bmp", currentPath.c_str(), &outPath) == NFD_OKAY && outPath != nullptr) {
                std::filesystem::path const imagePath = outPath;
                NFD_Free(outPath);
                ImportSheet(imagePath);
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {
            FireEvent(moth::gfx::EventRequestQuit{});
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        bool const canUndo = m_undoIndex >= 0;
        bool const canRedo = m_undoIndex < static_cast<int>(m_undoStack.size()) - 1;
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo)) {
            UndoSpriteAction();
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo)) {
            RedoSpriteAction();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Tools")) {
        bool const hasSheetImage = m_spriteSheet && m_spriteSheet->GetImage();
        if (ImGui::MenuItem("Grid...", nullptr, false, hasSheetImage)) {
            m_openGridTool = true;
        }
        // Detection decodes the image file, so it also needs a known image path.
        if (ImGui::MenuItem("Detect Frames...", nullptr, false, hasSheetImage && m_imagePathBuffer[0] != '\0')) {
            m_openDetectTool = true;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Preferences")) {
        auto& cfg = m_config;
        ImGui::ColorEdit4("Normal border##pref",   cfg.SpriteEditorNormalColor.data,   ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit4("Selected border##pref", cfg.SpriteEditorSelectedColor.data, ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit4("Prime border##pref",    cfg.SpriteEditorPrimeColor.data,    ImGuiColorEditFlags_NoInputs);
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("Border thickness##pref", &cfg.SpriteEditorRectThickness);
        cfg.SpriteEditorRectThickness = std::max(1, cfg.SpriteEditorRectThickness);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window")) {
        ImGui::MenuItem(kSheetWindow, nullptr, &m_config.ShowSheetWindow);
        ImGui::MenuItem(kCellWindow, nullptr, &m_config.ShowCellWindow);
        ImGui::MenuItem(kCellListWindow, nullptr, &m_config.ShowCellListWindow);
        ImGui::MenuItem(kClipEditorWindow, nullptr, &m_config.ShowClipEditorWindow);
        ImGui::MenuItem(kClipPreviewWindow, nullptr, &m_config.ShowClipPreviewWindow);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout")) {
            // The default layout shows every window, as on first run.
            m_config.ShowSheetWindow = true;
            m_config.ShowCellWindow = true;
            m_config.ShowCellListWindow = true;
            m_config.ShowClipEditorWindow = true;
            m_config.ShowClipPreviewWindow = true;
            m_resetLayout = true;
        }
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void SpriteEditor::DrawDockSpace() {
    // A borderless host window covers the work area (the viewport minus the main menu bar).
    ImGuiViewport const* const viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGuiWindowFlags const hostFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoDecoration |
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
    // The dock space must be submitted every frame, even when the host is collapsed or clipped.
    ImGui::Begin(kDockSpaceHostWindow, nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    ImGuiID const dockSpaceId = ImGui::GetID(kDockSpaceId);
    // imgui.ini has no node for the dock space on first run, or when it was written before docking.
    if (m_resetLayout || ImGui::DockBuilderGetNode(dockSpaceId) == nullptr) {
        m_resetLayout = false;
        BuildDefaultLayout(dockSpaceId, viewport->WorkSize);
    }
    ImGui::DockSpace(dockSpaceId);
    ImGui::End();
}

void SpriteEditor::Draw() {
    HandleShortcuts();
    AdvanceClipPlayback();
    UpdateWindowTitle();
    // New Cell mode draws on the sheet image in the Sheet window, so it can't outlive either.
    if (!(m_spriteSheet && m_spriteSheet->GetImage()) || !m_config.ShowSheetWindow) {
        m_newCellMode = false;
        m_newCellAnchor.reset();
    }

    DrawMainMenuBar();
    DrawDockSpace();

    if (m_config.ShowSheetWindow) {
        if (ImGui::Begin(kSheetWindow, &m_config.ShowSheetWindow)) {
            DrawPreview();
        }
        ImGui::End();
    }

    if (m_config.ShowCellWindow) {
        if (ImGui::Begin(kCellWindow, &m_config.ShowCellWindow)) {
            DrawCellWindow();
        }
        ImGui::End();
    }

    if (m_config.ShowCellListWindow) {
        if (ImGui::Begin(kCellListWindow, &m_config.ShowCellListWindow)) {
            DrawCellListWindow();
        }
        ImGui::End();
    }

    if (m_config.ShowClipEditorWindow) {
        if (ImGui::Begin(kClipEditorWindow, &m_config.ShowClipEditorWindow)) {
            DrawClipEditorWindow();
        }
        ImGui::End();
    }

    if (m_config.ShowClipPreviewWindow) {
        if (ImGui::Begin(kClipPreviewWindow, &m_config.ShowClipPreviewWindow)) {
            DrawClipPreviewWindow();
        }
        ImGui::End();
    }

    // The tool popups are outside every window, so they work with every window closed.
    DrawGridTool();
    DrawDetectFramesTool();
}
