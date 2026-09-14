#include "common.h"
#include "sprite_application.h"
#include "sprite_editor/sprite_editor.h"

#include <moth/graphics/graphics/surface_context.h>

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
    uiWindow->PushLayer(std::make_unique<SpriteEditor>(uiWindow->GetSurfaceContext().GetAssetContext(),
                                                       uiWindow->GetImGuiContext(), m_config));
}

void SpriteApplication::Shutdown() {
    std::ofstream ofile(m_configPath);
    if (!ofile.is_open()) {
        moth::core::log::warn("SpriteApplication: could not write '{}'", m_configPath.string());
        return;
    }
    ofile << nlohmann::json(m_config).dump(2);
}
