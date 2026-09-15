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
    char const* const kDockSpaceHostWindow = "##dock_space_host";
    char const* const kDockSpaceId = "##dock_space";
    char const* const kUnsavedPromptId = "Unsaved Changes##unsaved_prompt";
    // The checkerboard that shows transparency behind preview images.
    constexpr ImU32 kCheckerGray = IM_COL32(192, 192, 192, 255);
    constexpr ImU32 kCheckerWhite = IM_COL32(255, 255, 255, 255);

    // The folder a file dialog starts in: the remembered folder while it still exists, else the fallback.
    std::string DialogFolder(std::string const& remembered, std::filesystem::path const& fallback) {
        std::error_code ec;
        if (!remembered.empty() && std::filesystem::is_directory(remembered, ec)) {
            return remembered;
        }
        return fallback.string();
    }

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
        ImGuiID cellId = 0;
        ImGuiID cellListId = 0;
        ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Up, 0.4f, &cellId, &cellListId);
        ImGui::DockBuilderDockWindow(kSheetWindow, sheetId);
        ImGui::DockBuilderDockWindow(kCellWindow, cellId);
        ImGui::DockBuilderDockWindow(kCellListWindow, cellListId);
        ImGui::DockBuilderDockWindow(kClipEditorWindow, clipsId);
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
    MarkSaved();
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

void SpriteEditor::DrawImageBackground(moth::gfx::FloatVec2 const& pos, moth::gfx::FloatVec2 const& size,
                                       float checkerSize) const {
    // Draw only the part inside the clip rect. A zoomed-in sheet can be much larger than its window.
    ImDrawList* const drawList = ImGui::GetWindowDrawList();
    ImVec2 const clipMin = drawList->GetClipRectMin();
    ImVec2 const clipMax = drawList->GetClipRectMax();
    float const x0 = std::max(pos.x, clipMin.x);
    float const y0 = std::max(pos.y, clipMin.y);
    float const x1 = std::min(pos.x + size.x, clipMax.x);
    float const y1 = std::min(pos.y + size.y, clipMax.y);
    if (x0 >= x1 || y0 >= y1 || checkerSize <= 0.0f) {
        return;
    }

    auto const& c = m_config.PreviewBackgroundColor;
    ImU32 const color = ImGui::ColorConvertFloat4ToU32(ImVec4{ c.data[0], c.data[1], c.data[2], c.data[3] });
    // Any alpha that is not 0 in the 8-bit color draws the color.
    if (((color >> IM_COL32_A_SHIFT) & 0xFF) != 0) {
        drawList->AddRectFilled({ x0, y0 }, { x1, y1 }, color);
        return;
    }

    // Squares are counted from pos, so the pattern moves with the image. The top-left square is gray.
    drawList->AddRectFilled({ x0, y0 }, { x1, y1 }, kCheckerWhite);
    int const col0 = static_cast<int>(std::floor((x0 - pos.x) / checkerSize));
    int const col1 = static_cast<int>(std::ceil((x1 - pos.x) / checkerSize));
    int const row0 = static_cast<int>(std::floor((y0 - pos.y) / checkerSize));
    int const row1 = static_cast<int>(std::ceil((y1 - pos.y) / checkerSize));
    for (int row = row0; row < row1; ++row) {
        for (int col = col0; col < col1; ++col) {
            if ((row + col) % 2 != 0) {
                continue;
            }
            float const sx = pos.x + (static_cast<float>(col) * checkerSize);
            float const sy = pos.y + (static_cast<float>(row) * checkerSize);
            drawList->AddRectFilled({ std::max(sx, x0), std::max(sy, y0) },
                                    { std::min(sx + checkerSize, x1), std::min(sy + checkerSize, y1) }, kCheckerGray);
        }
    }
}

void SpriteEditor::UpdateWindowTitle() {
    std::string const fileName = std::filesystem::path(m_pathBuffer).filename().string();
    // " *" marks unsaved changes.
    std::string title = fmt::format("Moth Sprite - {}{}", fileName.empty() ? "Untitled" : fileName,
                                    HasUnsavedChanges() ? " *" : "");
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
    // File shortcuts. Ctrl+S on an untitled project chooses a path first, like Save in the unsaved changes prompt.
    // Saving needs a sheet image, as in the File menu.
    bool const hasImage = m_imagePathBuffer[0] != '\0';
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false) && hasImage) {
        if (io.KeyShift) {
            SaveProjectAs();
        } else {
            SaveProject();
        }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
        RequestProjectAction({ ProjectActionKind::New, {} });
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_L, false)) {
        RequestProjectAction({ ProjectActionKind::Load, {} });
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X, false)) {
        FireEvent(moth::gfx::EventRequestQuit{});
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
        if (m_clipWindowFocused) {
            // In the Clips window, Delete removes the selected clip's current step and never removes cells.
            DeleteClipStep(m_selectedClip, m_clipCurrentStep);
        } else {
            DeleteFrames(m_selection);
        }
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
    // Image dialogs (Import Sheet, Export Sheet) start in the folder that an image dialog last used, and remember
    // the folder of the file chosen. The project dialogs do the same in LoadWithDialog and SaveProjectAs.
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    if (ImGui::BeginMenu("File")) {
        // New, Load and Open Recent replace the project, so with unsaved changes they ask first.
        if (ImGui::MenuItem("New", "Ctrl+N")) {
            RequestProjectAction({ ProjectActionKind::New, {} });
        }
        if (ImGui::MenuItem("Load...", "Ctrl+L")) {
            RequestProjectAction({ ProjectActionKind::Load, {} });
        }
        // The entry is opened after the submenu is drawn, because opening a project changes the list.
        std::optional<std::string> recentToOpen;
        if (ImGui::BeginMenu("Open Recent", !m_config.RecentProjects.empty())) {
            for (size_t i = 0; i < m_config.RecentProjects.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::MenuItem(m_config.RecentProjects[i].c_str())) {
                    recentToOpen = m_config.RecentProjects[i];
                }
                ImGui::PopID();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Clear Recent")) {
                m_config.RecentProjects.clear();
            }
            ImGui::EndMenu();
        }
        if (recentToOpen.has_value()) {
            RequestProjectAction({ ProjectActionKind::OpenRecent, *recentToOpen });
        }
        bool const hasImage = m_imagePathBuffer[0] != '\0';
        bool const hasPath  = m_pathBuffer[0] != '\0';
        if (ImGui::MenuItem("Save", "Ctrl+S", false, hasImage && hasPath)) {
            SaveSpriteSheet(m_pathBuffer);
        }
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, hasImage)) {
            SaveProjectAs();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Import Sheet...", nullptr, false, m_spriteSheet != nullptr)) {
            nfdchar_t* outPath = nullptr;
            std::string const startDir = DialogFolder(m_config.LastImageDir, std::filesystem::current_path());
            if (NFD_OpenDialog("png,jpg,jpeg,bmp", startDir.c_str(), &outPath) == NFD_OKAY && outPath != nullptr) {
                std::filesystem::path const imagePath = outPath;
                NFD_Free(outPath);
                m_config.LastImageDir = imagePath.parent_path().string();
                ImportSheet(imagePath);
            }
        }
        if (ImGui::MenuItem("Export Sheet...", nullptr, false, hasImage)) {
            // Filter on the sheet's own extension. Before any image dialog has been used, start in the sheet's folder.
            std::filesystem::path const sheetPath = m_imagePathBuffer;
            std::string const extension = sheetPath.extension().string();
            std::string const filter = extension.empty() ? std::string{} : extension.substr(1);
            std::string const startDir = DialogFolder(m_config.LastImageDir, sheetPath.parent_path());
            nfdchar_t* outPath = nullptr;
            if (NFD_SaveDialog(filter.empty() ? nullptr : filter.c_str(), startDir.c_str(), &outPath) == NFD_OKAY && outPath != nullptr) {
                std::filesystem::path const exportPath = outPath;
                NFD_Free(outPath);
                m_config.LastImageDir = exportPath.parent_path().string();
                ExportSheet(exportPath);
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Ctrl+X")) {
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
        ImGui::Separator();
        // Pivot rules apply to every selected cell, relative to each cell's own size.
        if (ImGui::BeginMenu("Pivot", !m_selection.empty())) {
            struct PivotRule {
                char const* label;
                PivotAnchor x;
                PivotAnchor y;
            };
            static constexpr std::array<PivotRule, 9> kPivotRules{ {
                { "Top Left",      PivotAnchor::Start,  PivotAnchor::Start },
                { "Top Center",    PivotAnchor::Center, PivotAnchor::Start },
                { "Top Right",     PivotAnchor::End,    PivotAnchor::Start },
                { "Center Left",   PivotAnchor::Start,  PivotAnchor::Center },
                { "Center",        PivotAnchor::Center, PivotAnchor::Center },
                { "Center Right",  PivotAnchor::End,    PivotAnchor::Center },
                { "Bottom Left",   PivotAnchor::Start,  PivotAnchor::End },
                { "Bottom Center", PivotAnchor::Center, PivotAnchor::End },
                { "Bottom Right",  PivotAnchor::End,    PivotAnchor::End },
            } };
            for (size_t i = 0; i < kPivotRules.size(); ++i) {
                // A separator between the top, center and bottom rows.
                if (i > 0 && i % 3 == 0) {
                    ImGui::Separator();
                }
                if (ImGui::MenuItem(kPivotRules[i].label)) {
                    SetSelectionPivot(kPivotRules[i].x, kPivotRules[i].y);
                }
            }
            ImGui::EndMenu();
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
        ImGui::Separator();
        // Shared by every preview window. Alpha 0 shows a checkerboard.
        ImGui::ColorEdit4("Preview background##pref", cfg.PreviewBackgroundColor.data,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window")) {
        ImGui::MenuItem(kSheetWindow, nullptr, &m_config.ShowSheetWindow);
        ImGui::MenuItem(kCellWindow, nullptr, &m_config.ShowCellWindow);
        ImGui::MenuItem(kCellListWindow, nullptr, &m_config.ShowCellListWindow);
        ImGui::MenuItem(kClipEditorWindow, nullptr, &m_config.ShowClipEditorWindow);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout")) {
            // The default layout shows every window, as on first run.
            m_config.ShowSheetWindow = true;
            m_config.ShowCellWindow = true;
            m_config.ShowCellListWindow = true;
            m_config.ShowClipEditorWindow = true;
            m_resetLayout = true;
        }
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}

void SpriteEditor::LoadWithDialog() {
    // Project dialogs (Load, Save As) start in the folder that a project dialog last used, and remember the folder
    // of the file chosen.
    nfdchar_t* outPath = nullptr;
    std::string const startDir = DialogFolder(m_config.LastProjectDir, std::filesystem::current_path());
    if (NFD_OpenDialog("json", startDir.c_str(), &outPath) == NFD_OKAY && outPath != nullptr) {
        std::filesystem::path const path = outPath;
        NFD_Free(outPath);
        // The folder is remembered even when the load fails. The project path changes only when it succeeds.
        m_config.LastProjectDir = path.parent_path().string();
        LoadSpriteSheet(path);
    }
}

bool SpriteEditor::SaveProjectAs() {
    nfdchar_t* outPath = nullptr;
    std::string const startDir = DialogFolder(m_config.LastProjectDir, std::filesystem::current_path());
    if (NFD_SaveDialog("json", startDir.c_str(), &outPath) != NFD_OKAY || outPath == nullptr) {
        return false;
    }
    std::filesystem::path const path = outPath;
    NFD_Free(outPath);
    // The folder is remembered even when the save fails. The project path changes only when the file is written.
    m_config.LastProjectDir = path.parent_path().string();
    return SaveSpriteSheet(path);
}

bool SpriteEditor::SaveProject() {
    return (m_pathBuffer[0] != '\0') ? SaveSpriteSheet(m_pathBuffer) : SaveProjectAs();
}

bool SpriteEditor::HoldQuitForUnsavedChanges() {
    if (m_quitApproved || !HasUnsavedChanges()) {
        return false;
    }
    RequestProjectAction({ ProjectActionKind::Quit, {} });
    return true;
}

void SpriteEditor::RequestProjectAction(ProjectAction action) {
    if (!HasUnsavedChanges()) {
        RunProjectAction(action);
        return;
    }
    m_pendingProjectAction = std::move(action);
    m_openUnsavedPrompt = true;
}

void SpriteEditor::RunProjectAction(ProjectAction const& action) {
    switch (action.kind) {
    case ProjectActionKind::New:
        NewSpriteSheet();
        break;
    case ProjectActionKind::Load:
        LoadWithDialog();
        break;
    case ProjectActionKind::OpenRecent:
        OpenRecentProject(action.recentPath);
        break;
    case ProjectActionKind::Quit:
        // Send the quit request again. SpriteApplication lets it through now.
        m_quitApproved = true;
        FireEvent(moth::gfx::EventRequestQuit{});
        break;
    }
}

void SpriteEditor::DrawUnsavedChangesPrompt() {
    if (m_openUnsavedPrompt) {
        m_openUnsavedPrompt = false;
        ImGui::OpenPopup(kUnsavedPromptId);
    }
    ImGuiViewport const* const viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2{ 0.5f, 0.5f });
    if (!ImGui::BeginPopupModal(kUnsavedPromptId, nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        return;
    }

    std::string const fileName = std::filesystem::path(m_pathBuffer).filename().string();
    ImGui::Text("\"%s\" has unsaved changes.", fileName.empty() ? "Untitled" : fileName.c_str());
    ImGui::TextUnformatted("Save them first?");
    // A project without a sheet image cannot be saved.
    bool const canSave = m_imagePathBuffer[0] != '\0';
    if (!canSave) {
        ImGui::TextDisabled("The project has no sheet image, so it cannot be saved.");
    }
    ImGui::Spacing();

    // Set when the user answers: true to go on with the action, false to return to the editor.
    std::optional<bool> proceed;
    constexpr float kButtonW = 110.0f;
    ImGui::BeginDisabled(!canSave);
    // Save runs Save, or Save As when the project has no path. If saving fails or is cancelled, the prompt stays.
    if (ImGui::Button("Save", ImVec2{ kButtonW, 0.0f }) && SaveProject()) {
        proceed = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Don't Save", ImVec2{ kButtonW, 0.0f })) {
        proceed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2{ kButtonW, 0.0f }) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        proceed = false;
    }

    if (!proceed.has_value()) {
        ImGui::EndPopup();
        return;
    }
    ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    std::optional<ProjectAction> const action = std::move(m_pendingProjectAction);
    m_pendingProjectAction.reset();
    if (*proceed && action.has_value()) {
        RunProjectAction(*action);
    }
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

    // Set again while the Clips window is drawn, so a closed or hidden window never counts as focused.
    m_clipWindowFocused = false;
    if (m_config.ShowClipEditorWindow) {
        if (ImGui::Begin(kClipEditorWindow, &m_config.ShowClipEditorWindow)) {
            DrawClipEditorWindow();
        }
        ImGui::End();
    }

    // The tool popups are outside every window, so they work with every window closed.
    DrawGridTool();
    DrawDetectFramesTool();

    // Asks about unsaved changes before New, Load, Open Recent and quitting.
    DrawUnsavedChangesPrompt();
}
