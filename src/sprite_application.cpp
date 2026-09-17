#include "common.h"
#include "sprite_application.h"
#include "sprite_editor/sprite_editor.h"

#include <moth/graphics/graphics/asset_context.h>
#include <moth/graphics/graphics/igraphics.h>
#include <moth/graphics/graphics/spritesheet_factory.h>
#include <moth/graphics/graphics/surface_context.h>
#include <moth/graphics/graphics/texture_factory.h>
#include <moth/core/event_window.h>

namespace {
    char const* const kConfigFile = "moth_sprite.json";
}

SpriteApplication::SpriteApplication(moth::gfx::platform::IPlatform& platform)
    : Application(platform, "Moth Sprite", 1280, 800)
    , m_configPath(std::filesystem::current_path() / kConfigFile) {
}

void SpriteApplication::Startup() {
    std::ifstream ifile(m_configPath);
    if (!ifile.is_open()) {
        return;
    }
    try {
        m_config = nlohmann::json::parse(ifile).get<SpriteEditorConfig>();
    } catch (std::exception const& e) {
        moth::core::log::warn("SpriteApplication: could not read '{}': {}", m_configPath.string(), e.what());
    }
}

void SpriteApplication::PostCreateWindow() {
    auto* uiWindow = GetUiWindow();
    // The window owns the layer stack, so it outlives the editor that sets its title.
    auto setWindowTitle = [uiWindow](std::string_view title) { uiWindow->SetWindowTitle(title); };
    auto waitForGpu = [uiWindow]() { uiWindow->GetGraphics().WaitIdle(); };
    auto editor = std::make_unique<SpriteEditor>(uiWindow->GetSurfaceContext().GetAssetContext(),
                                                 uiWindow->GetImGuiContext(), m_config, std::move(setWindowTitle),
                                                 std::move(waitForGpu));
    m_editor = editor.get();
    uiWindow->PushLayer(std::move(editor));
}

bool SpriteApplication::OnEvent(moth::core::Event const& event) {
    // Closing the window and File > Exit both send EventRequestQuit. With unsaved changes the editor shows its
    // prompt, and sends the request again once the user has chosen Save or Don't Save.
    if (moth::core::event_cast<moth::core::EventRequestQuit>(event) != nullptr && m_editor != nullptr &&
        m_editor->HoldQuitForUnsavedChanges()) {
        return true;
    }
    return Application::OnEvent(event);
}

void SpriteApplication::Shutdown() {
    // The editor layer is destroyed after this returns.
    m_editor = nullptr;

    // The layers (and the textures they hold) are destroyed right after this returns,
    // but the final frame's command buffers may still reference them. Drain the device
    // first so freeing their descriptor sets is safe.
    GetUiWindow()->GetGraphics().WaitIdle();

    // The asset factory caches live in the surface context, which is destroyed after the
    // ImGui context. A cached texture that was drawn through ImGui frees its ImGui
    // descriptor set on destruction, which crashes once the ImGui backend is gone
    // (e.g. a sheet opened with File > Open). Release the caches while ImGui is alive.
    auto& assetContext = GetUiWindow()->GetSurfaceContext().GetAssetContext();
    assetContext.GetSpriteSheetFactory().FlushCache();
    assetContext.GetTextureFactory().FlushCache();

    std::ofstream ofile(m_configPath);
    if (!ofile.is_open()) {
        moth::core::log::warn("SpriteApplication: could not write '{}'", m_configPath.string());
        return;
    }
    ofile << nlohmann::json(m_config).dump(2);
}
