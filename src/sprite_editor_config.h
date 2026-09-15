#pragma once

#include <moth/core/vector_serialization.h>
#include <moth/graphics/graphics/color.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <vector>

struct SpriteEditorConfig {
    moth::gfx::Color SpriteEditorNormalColor = moth::gfx::Color{ 1.0f, 1.0f, 0.0f, 200.0f / 255.0f };
    moth::gfx::Color SpriteEditorSelectedColor = moth::gfx::Color{ 0.0f, 1.0f, 1.0f, 1.0f };
    moth::gfx::Color SpriteEditorPrimeColor = moth::gfx::Color{ 1.0f, 0.0f, 1.0f, 1.0f };
    int SpriteEditorRectThickness = 1;
    // The background behind the image in every preview window. Alpha 0 draws a gray and white checkerboard.
    moth::gfx::Color PreviewBackgroundColor = moth::gfx::Color{ 0.0f, 0.0f, 0.0f, 0.0f };
    // Whether each editor window is open. Toggled from the Window menu.
    bool ShowSheetWindow = true;
    bool ShowCellWindow = true;
    bool ShowCellListWindow = true;
    bool ShowClipEditorWindow = true;
    // File > Open Recent: project file paths, newest first.
    std::vector<std::string> RecentProjects;
    // The folders that the project dialogs (Load, Save As) and the image dialogs (Import Sheet, Export Sheet)
    // last used. Empty until a dialog is used.
    std::string LastProjectDir;
    std::string LastImageDir;
};

inline void to_json(nlohmann::json& j, SpriteEditorConfig const& config) {
    j["SpriteEditorNormalColor"] = config.SpriteEditorNormalColor;
    j["SpriteEditorSelectedColor"] = config.SpriteEditorSelectedColor;
    j["SpriteEditorPrimeColor"] = config.SpriteEditorPrimeColor;
    j["SpriteEditorRectThickness"] = config.SpriteEditorRectThickness;
    j["PreviewBackgroundColor"] = config.PreviewBackgroundColor;
    j["ShowSheetWindow"] = config.ShowSheetWindow;
    j["ShowCellWindow"] = config.ShowCellWindow;
    j["ShowCellListWindow"] = config.ShowCellListWindow;
    j["ShowClipEditorWindow"] = config.ShowClipEditorWindow;
    j["RecentProjects"] = config.RecentProjects;
    j["LastProjectDir"] = config.LastProjectDir;
    j["LastImageDir"] = config.LastImageDir;
}

inline void from_json(nlohmann::json const& j, SpriteEditorConfig& config) {
    config.SpriteEditorNormalColor = j.value("SpriteEditorNormalColor", config.SpriteEditorNormalColor);
    config.SpriteEditorSelectedColor = j.value("SpriteEditorSelectedColor", config.SpriteEditorSelectedColor);
    config.SpriteEditorPrimeColor = j.value("SpriteEditorPrimeColor", config.SpriteEditorPrimeColor);
    config.SpriteEditorRectThickness = std::max(j.value("SpriteEditorRectThickness", config.SpriteEditorRectThickness), 1);
    config.PreviewBackgroundColor = j.value("PreviewBackgroundColor", config.PreviewBackgroundColor);
    config.ShowSheetWindow = j.value("ShowSheetWindow", config.ShowSheetWindow);
    config.ShowCellWindow = j.value("ShowCellWindow", config.ShowCellWindow);
    config.ShowCellListWindow = j.value("ShowCellListWindow", config.ShowCellListWindow);
    config.ShowClipEditorWindow = j.value("ShowClipEditorWindow", config.ShowClipEditorWindow);
    config.RecentProjects = j.value("RecentProjects", config.RecentProjects);
    config.LastProjectDir = j.value("LastProjectDir", config.LastProjectDir);
    config.LastImageDir = j.value("LastImageDir", config.LastImageDir);
}
