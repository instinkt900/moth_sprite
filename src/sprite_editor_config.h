#pragma once

#include <moth/core/vector_serialization.h>
#include <moth/graphics/graphics/color.h>

#include <nlohmann/json.hpp>

#include <algorithm>

struct SpriteEditorConfig {
    moth::gfx::Color SpriteEditorNormalColor = moth::gfx::Color{ 1.0f, 1.0f, 0.0f, 200.0f / 255.0f };
    moth::gfx::Color SpriteEditorSelectedColor = moth::gfx::Color{ 0.0f, 1.0f, 1.0f, 1.0f };
    int SpriteEditorRectThickness = 1;
    // Whether each editor window is open. Toggled from the Window menu.
    bool ShowSpriteEditorWindow = true;
    bool ShowSheetWindow = true;
    bool ShowCellWindow = true;
    bool ShowCellListWindow = true;
    bool ShowClipEditorWindow = true;
};

inline void to_json(nlohmann::json& j, SpriteEditorConfig const& config) {
    j["SpriteEditorNormalColor"] = config.SpriteEditorNormalColor;
    j["SpriteEditorSelectedColor"] = config.SpriteEditorSelectedColor;
    j["SpriteEditorRectThickness"] = config.SpriteEditorRectThickness;
    j["ShowSpriteEditorWindow"] = config.ShowSpriteEditorWindow;
    j["ShowSheetWindow"] = config.ShowSheetWindow;
    j["ShowCellWindow"] = config.ShowCellWindow;
    j["ShowCellListWindow"] = config.ShowCellListWindow;
    j["ShowClipEditorWindow"] = config.ShowClipEditorWindow;
}

inline void from_json(nlohmann::json const& j, SpriteEditorConfig& config) {
    config.SpriteEditorNormalColor = j.value("SpriteEditorNormalColor", config.SpriteEditorNormalColor);
    config.SpriteEditorSelectedColor = j.value("SpriteEditorSelectedColor", config.SpriteEditorSelectedColor);
    config.SpriteEditorRectThickness = std::max(j.value("SpriteEditorRectThickness", config.SpriteEditorRectThickness), 1);
    config.ShowSpriteEditorWindow = j.value("ShowSpriteEditorWindow", config.ShowSpriteEditorWindow);
    config.ShowSheetWindow = j.value("ShowSheetWindow", config.ShowSheetWindow);
    config.ShowCellWindow = j.value("ShowCellWindow", config.ShowCellWindow);
    config.ShowCellListWindow = j.value("ShowCellListWindow", config.ShowCellListWindow);
    config.ShowClipEditorWindow = j.value("ShowClipEditorWindow", config.ShowClipEditorWindow);
}
