#pragma once

#include "sprite_editor_config.h"

#include <moth/bridge/application.h>

#include <filesystem>

class SpriteEditor;

class SpriteApplication : public moth::bridge::Application {
public:
    explicit SpriteApplication(moth::gfx::platform::IPlatform& platform);

    // Holds back a quit request while the editor asks about unsaved changes.
    bool OnEvent(moth::core::Event const& event) override;

private:
    void Startup() override;
    void PostCreateWindow() override;
    void Shutdown() override;

    std::filesystem::path m_configPath;
    SpriteEditorConfig m_config;
    SpriteEditor* m_editor = nullptr; // owned by the UI window's layer stack
};
