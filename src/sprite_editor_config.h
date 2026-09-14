#pragma once

#include <moth/core/vector_serialization.h>
#include <moth/graphics/graphics/color.h>

#include <nlohmann/json.hpp>

#include <algorithm>

struct SpriteEditorConfig {
    moth::gfx::Color SpriteEditorNormalColor = moth::gfx::Color{ 1.0f, 1.0f, 0.0f, 200.0f / 255.0f };
    moth::gfx::Color SpriteEditorSelectedColor = moth::gfx::Color{ 0.0f, 1.0f, 1.0f, 1.0f };
    int SpriteEditorRectThickness = 1;
};

inline void to_json(nlohmann::json& j, SpriteEditorConfig const& config) {
    j["SpriteEditorNormalColor"] = config.SpriteEditorNormalColor;
    j["SpriteEditorSelectedColor"] = config.SpriteEditorSelectedColor;
    j["SpriteEditorRectThickness"] = config.SpriteEditorRectThickness;
}

inline void from_json(nlohmann::json const& j, SpriteEditorConfig& config) {
    config.SpriteEditorNormalColor = j.value("SpriteEditorNormalColor", config.SpriteEditorNormalColor);
    config.SpriteEditorSelectedColor = j.value("SpriteEditorSelectedColor", config.SpriteEditorSelectedColor);
    config.SpriteEditorRectThickness = std::max(j.value("SpriteEditorRectThickness", config.SpriteEditorRectThickness), 1);
}
