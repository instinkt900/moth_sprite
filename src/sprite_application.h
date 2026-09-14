#pragma once

#include "sprite_editor_config.h"

#include <moth/bridge/application.h>

#include <filesystem>

class SpriteApplication : public moth::bridge::Application {
public:
    explicit SpriteApplication(moth::gfx::platform::IPlatform& platform);

private:
    void Startup() override;
    void PostCreateWindow() override;
    void Shutdown() override;

    std::filesystem::path m_configPath;
    SpriteEditorConfig m_config;
};
