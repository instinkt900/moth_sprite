#include "common.h"
#include "sprite_editor.h"
#include "sprite_editor_config.h"

#include <moth/graphics/events/event_window.h>

#include <imgui_internal.h>
#include <nfd.h>

namespace {
    // Window names are also the IDs that imgui.ini uses to remember the layout. Do not rename them.
    char const* const kSpriteEditorWindow = "Sprite Editor";
    char const* const kDockSpaceHostWindow = "##dock_space_host";
    char const* const kDockSpaceId = "##dock_space";

    // Replace the dock space's layout with the built-in default.
    void BuildDefaultLayout(ImGuiID dockSpaceId, ImVec2 size) {
        ImGui::DockBuilderRemoveNode(dockSpaceId);
        ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockSpaceId, size);
        ImGui::DockBuilderDockWindow(kSpriteEditorWindow, dockSpaceId);
        ImGui::DockBuilderFinish(dockSpaceId);
    }
} // namespace

SpriteEditor::SpriteEditor(moth::gfx::AssetContext& assetContext, moth::gfx::platform::ImGuiContext& imgui, SpriteEditorConfig& config)
    : m_assetContext(assetContext)
    , m_imgui(imgui)
    , m_config(config) {
    // Start with a blank project so the user can import a sheet straight away.
    NewSpriteSheet();
}

void SpriteEditor::NewSpriteSheet() {
    ClearSpriteActions();
    m_pathBuffer[0]      = '\0';
    m_imagePathBuffer[0] = '\0';
    m_frames.clear();
    m_clips.clear();
    m_selectedFrame   = -1;
    m_selectedClip    = -1;
    m_clipPlaying     = false;
    m_clipCurrentStep = 0;
    m_clipElapsedMs   = 0.0f;
    m_zoom            = 1.0f;
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

void SpriteEditor::DrawDataEditor() {
    // Read-only path display
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::InputText("##sprite_path", m_pathBuffer, sizeof(m_pathBuffer) - 1, ImGuiInputTextFlags_ReadOnly);

    if (!m_spriteSheet) {
        ImGui::TextDisabled("Use File > Load to open a sprite sheet.");
        return;
    }

    ImGui::Separator();

    float const totalH = ImGui::GetContentRegionAvail().y;
    float const framesH = std::floor(totalH * 0.5f);

    if (ImGui::BeginChild("##frames_pane", ImVec2(0, framesH), ImGuiChildFlags_None)) {
        DrawFramesPane();
    }
    ImGui::EndChild();

    ImGui::BeginChild("##clips_pane", ImVec2(0, 0), ImGuiChildFlags_None);
    DrawClipsPane();
    ImGui::EndChild();
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
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        DeleteFrame(m_selectedFrame);
    }
    if (m_newCellMode && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        m_newCellMode = false;
        m_newCellAnchor.reset();
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
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("Border thickness##pref", &cfg.SpriteEditorRectThickness);
        cfg.SpriteEditorRectThickness = std::max(1, cfg.SpriteEditorRectThickness);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window")) {
        ImGui::MenuItem(kSpriteEditorWindow, nullptr, &m_config.ShowSpriteEditorWindow);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout")) {
            // The default layout shows every window, as on first run.
            m_config.ShowSpriteEditorWindow = true;
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
    // New Cell mode draws on the sheet image, so it can't outlive the image.
    if (!(m_spriteSheet && m_spriteSheet->GetImage())) {
        m_newCellMode = false;
        m_newCellAnchor.reset();
    }

    DrawMainMenuBar();
    DrawDockSpace();

    // All of the editor UI is in one dockable window.
    if (m_config.ShowSpriteEditorWindow) {
        if (ImGui::Begin(kSpriteEditorWindow, &m_config.ShowSpriteEditorWindow)) {
            if (ImGui::BeginTable("##sprite_layout", 2,
                    ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
                ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch, 0.6f);
                ImGui::TableSetupColumn("Editor",  ImGuiTableColumnFlags_WidthStretch, 0.4f);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                DrawPreview();

                ImGui::TableSetColumnIndex(1);
                ImGui::BeginChild("##sprite_data", ImVec2(0, 0), ImGuiChildFlags_None);
                DrawDataEditor();
                ImGui::EndChild();

                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    // The tool popups are outside every window, so they work with every window closed.
    DrawGridTool();
    DrawDetectFramesTool();
}
