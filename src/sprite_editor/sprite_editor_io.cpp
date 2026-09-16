#include "common.h"
#include "sprite_editor.h"
#include "sprite_editor_config.h"

#include <moth/graphics/graphics/igraphics.h>
#include <moth/graphics/graphics/surface_context.h>
#include <moth/graphics/graphics/asset_context.h>
#include <moth/graphics/graphics/spritesheet_factory.h>

namespace {
    // File > Open Recent keeps this many projects.
    constexpr size_t kMaxRecentProjects = 10;
} // namespace

void SpriteEditor::LoadSpriteSheet(std::filesystem::path const& path) {
    // Load and validate before touching any editor state so a failed load
    // leaves the current document intact.
    auto& assetContext = m_assetContext;
    auto newSheet = assetContext.GetSpriteSheetFactory().GetSpriteSheet(path);
    if (!newSheet) {
        moth::core::log::error("Failed to load sprite sheet: {}", path.string());
        return;
    }

    ClearSpriteActions();
    MarkSaved();
    std::string const pathStr = path.string();
    strncpy(m_pathBuffer, pathStr.c_str(), sizeof(m_pathBuffer) - 1);
    m_pathBuffer[sizeof(m_pathBuffer) - 1] = '\0';
    m_selection.clear();
    m_selectedClip = -1;
    m_clipPlaying = false;
    m_clipCurrentStep = 0;
    m_clipElapsedMs = 0.0f;
    m_zoom = -1.0f; // trigger auto-fit on next draw
    m_cellZoom = -1.0f;
    m_frames.clear();
    m_imagePathBuffer[0] = '\0';
    m_spriteSheet = std::move(newSheet);

    // Read the image path from the JSON so Import Sheet and Save know it
    try {
        std::ifstream ifile(path);
        if (ifile.is_open()) {
            nlohmann::json json;
            ifile >> json;
            if (json.contains("image") && json["image"].is_string()) {
                auto const imageAbsPath = std::filesystem::absolute(
                    path.parent_path() / json["image"].get<std::string>()).lexically_normal();
                auto const imageStr = imageAbsPath.string();
                strncpy(m_imagePathBuffer, imageStr.c_str(), sizeof(m_imagePathBuffer) - 1);
                m_imagePathBuffer[sizeof(m_imagePathBuffer) - 1] = '\0';
            }
        }
    } catch (std::exception const& e) {
        moth::core::log::warn("SpriteEditor: could not read image path from '{}': {}", path.string(), e.what());
    }

    m_frames.reserve(static_cast<size_t>(m_spriteSheet->GetFrameCount()));
    for (int i = 0; i < m_spriteSheet->GetFrameCount(); ++i) {
        if (auto entry = m_spriteSheet->GetFrameDesc(i)) {
            m_frames.push_back(*entry);
        }
    }

    m_clips.clear();
    int const clipCount = m_spriteSheet->GetClipCount();
    m_clips.reserve(static_cast<size_t>(clipCount));
    for (int i = 0; i < clipCount; ++i) {
        moth::gfx::SpriteSheet::ClipEntry entry;
        entry.name = m_spriteSheet->GetClipName(i);
        if (auto desc = m_spriteSheet->GetClipDesc(entry.name)) {
            entry.desc = *desc;
        }
        m_clips.push_back(std::move(entry));
    }

    AddRecentProject(path);
}

void SpriteEditor::AddRecentProject(std::filesystem::path const& path) {
    std::error_code ec;
    std::filesystem::path absolutePath = std::filesystem::absolute(path, ec);
    if (ec) {
        absolutePath = path;
    }
    std::string const entry = absolutePath.lexically_normal().string();
    // Newest first, with no duplicates.
    auto& recent = m_config.RecentProjects;
    recent.erase(std::remove(recent.begin(), recent.end(), entry), recent.end());
    recent.insert(recent.begin(), entry);
    if (recent.size() > kMaxRecentProjects) {
        recent.resize(kMaxRecentProjects);
    }
}

void SpriteEditor::OpenRecentProject(std::string path) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        moth::core::log::warn("SpriteEditor: recent project '{}' no longer exists, removed it from Open Recent", path);
        auto& recent = m_config.RecentProjects;
        recent.erase(std::remove(recent.begin(), recent.end(), path), recent.end());
        return;
    }
    // A file that exists but fails to load stays in the list, and the project path does not change.
    LoadSpriteSheet(path);
}

void SpriteEditor::ImportSheet(std::filesystem::path const& imagePath) {
    auto& assetContext = m_assetContext;
    std::shared_ptr<moth::gfx::ITexture> texture(assetContext.TextureFromFile(imagePath));
    if (!texture) {
        moth::core::log::error("SpriteEditor: failed to load image '{}'", imagePath.string());
        return;
    }
    moth::gfx::Image image{ texture };

    // Replacing the sheet image is one undoable action. Each side of it keeps its sheet, and so its texture, alive.
    auto const setSheet = [this](std::shared_ptr<moth::gfx::SpriteSheet> const& sheet, std::string const& path) {
        m_spriteSheet = sheet;
        strncpy(m_imagePathBuffer, path.c_str(), sizeof(m_imagePathBuffer) - 1);
        m_imagePathBuffer[sizeof(m_imagePathBuffer) - 1] = '\0';
        m_zoom = -1.0f; // re-fit to the image dimensions
        m_cellZoom = -1.0f;
    };
    std::shared_ptr<moth::gfx::SpriteSheet> const previousSheet = m_spriteSheet;
    std::string const previousPath = m_imagePathBuffer;
    auto const importedSheet = std::make_shared<moth::gfx::SpriteSheet>(std::move(image), m_frames, m_clips);
    std::string const importedPath = imagePath.string();
    setSheet(importedSheet, importedPath);
    AddSpriteAction(std::make_unique<BasicAction>(
        [setSheet, importedSheet, importedPath]() { setSheet(importedSheet, importedPath); },
        [setSheet, previousSheet, previousPath]() { setSheet(previousSheet, previousPath); }
    ));

    m_selection.clear();
    m_selectedClip  = -1;
    m_clipPlaying   = false;
    m_clipCurrentStep = 0;
    m_clipElapsedMs   = 0.0f;
}

void SpriteEditor::ExportSheet(std::filesystem::path exportPath) {
    if (m_imagePathBuffer[0] == '\0') {
        return;
    }
    std::filesystem::path const sheetPath = m_imagePathBuffer;

    // The copy keeps the sheet's format, so it always gets the sheet's extension.
    if (exportPath.extension() != sheetPath.extension()) {
        exportPath.replace_extension(sheetPath.extension());
    }
    std::string const exportStr = exportPath.string();
    if (exportStr.size() >= sizeof(m_imagePathBuffer)) {
        moth::core::log::error("SpriteEditor: export path is too long: '{}'", exportStr);
        return;
    }

    // Exporting onto the sheet file itself copies nothing and keeps the project's path.
    std::error_code ec;
    if (std::filesystem::equivalent(sheetPath, exportPath, ec)) {
        return;
    }
    ec.clear();
    std::filesystem::copy_file(sheetPath, exportPath, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        moth::core::log::error("SpriteEditor: failed to export sheet '{}' to '{}': {}",
            sheetPath.string(), exportStr, ec.message());
        return;
    }
    moth::core::log::info("SpriteEditor: exported sheet to '{}'", exportStr);

    // Point the project at the exported file. Undo points it back at the old file; the copy stays on disk.
    std::string const previousStr = m_imagePathBuffer;
    auto const setImagePath = [this](std::string const& path) {
        strncpy(m_imagePathBuffer, path.c_str(), sizeof(m_imagePathBuffer) - 1);
        m_imagePathBuffer[sizeof(m_imagePathBuffer) - 1] = '\0';
    };
    setImagePath(exportStr);
    AddSpriteAction(std::make_unique<BasicAction>(
        [setImagePath, exportStr]()   { setImagePath(exportStr); },
        [setImagePath, previousStr]() { setImagePath(previousStr); }
    ));
}

bool SpriteEditor::SaveSpriteSheet(std::filesystem::path const& path) {
    if (path.empty() || !m_spriteSheet) {
        return false;
    }

    // Read existing JSON to preserve unknown fields; start fresh if the file doesn't exist yet.
    nlohmann::json json = nlohmann::json::object();
    {
        std::ifstream ifile(path);
        if (ifile.is_open()) {
            try {
                ifile >> json;
            } catch (std::exception const& e) {
                moth::core::log::warn("SpriteEditor: could not parse existing '{}', overwriting: {}", path.string(), e.what());
                json = nlohmann::json::object();
            }
        }
    }

    // Update the image field if we have a known image path
    if (m_imagePathBuffer[0] != '\0') {
        std::filesystem::path const imagePath = m_imagePathBuffer;
        std::filesystem::path const relImage  = imagePath.lexically_relative(path.parent_path());
        json["image"] = relImage.empty() ? imagePath.filename().string() : relImage.string();
    }

    // Write frames
    nlohmann::json framesJson = nlohmann::json::array();
    for (auto const& fr : m_frames) {
        nlohmann::json obj;
        obj["x"]       = fr.rect.x();
        obj["y"]       = fr.rect.y();
        obj["w"]       = fr.rect.w();
        obj["h"]       = fr.rect.h();
        obj["pivot_x"] = fr.pivot.x;
        obj["pivot_y"] = fr.pivot.y;
        framesJson.push_back(std::move(obj));
    }
    json["frames"] = std::move(framesJson);

    // Write clips
    nlohmann::json clipsJson = nlohmann::json::array();
    for (auto const& entry : m_clips) {
        nlohmann::json clipObj;
        clipObj["name"] = entry.name;

        char const* loopStr = nullptr;
        switch (entry.desc.loop) {
        case moth::gfx::SpriteSheet::LoopType::Stop:  loopStr = "stop";  break;
        case moth::gfx::SpriteSheet::LoopType::Reset: loopStr = "reset"; break;
        case moth::gfx::SpriteSheet::LoopType::Loop:  loopStr = "loop";  break;
        default:                                                     loopStr = "stop";  break;
        }
        clipObj["loop"] = loopStr;

        nlohmann::json stepsJson = nlohmann::json::array();
        int const frameCount = static_cast<int>(m_frames.size());
        for (auto const& step : entry.desc.frames) {
            nlohmann::json stepObj;
            int const safeFrameIdx = (frameCount > 0)
                ? std::clamp(step.frameIndex, 0, frameCount - 1)
                : 0;
            if (safeFrameIdx != step.frameIndex) {
                moth::core::log::warn("SpriteEditor: clip '{}' step has out-of-range frameIndex {}, saving as {}",
                    entry.name, step.frameIndex, safeFrameIdx);
            }
            stepObj["frame"]       = safeFrameIdx;
            stepObj["duration_ms"] = step.durationMs;
            stepsJson.push_back(std::move(stepObj));
        }
        clipObj["frames"] = std::move(stepsJson);

        clipsJson.push_back(std::move(clipObj));
    }
    json["clips"] = std::move(clipsJson);

    // Write to file
    std::ofstream ofile(path);
    if (!ofile.is_open()) {
        moth::core::log::error("SpriteEditor: failed to open '{}' for writing", path.string());
        return false;
    }
    try {
        ofile << json.dump(2);
        // Callers go on (for example, quit) only after a successful save, so check that the write worked.
        ofile.flush();
        if (!ofile) {
            moth::core::log::error("SpriteEditor: failed to write '{}'", path.string());
            return false;
        }
        moth::core::log::info("SpriteEditor: saved '{}'", path.string());
    } catch (std::exception const& e) {
        moth::core::log::error("SpriteEditor: failed to write '{}': {}", path.string(), e.what());
        return false;
    }
    // The file was written, so the project uses it from now on.
    std::string const pathStr = path.string();
    strncpy(m_pathBuffer, pathStr.c_str(), sizeof(m_pathBuffer) - 1);
    m_pathBuffer[sizeof(m_pathBuffer) - 1] = '\0';
    AddRecentProject(path);
    MarkSaved();

    // Flush the factory cache so a subsequent load picks up the new data
    auto& assetContext = m_assetContext;
    assetContext.GetSpriteSheetFactory().FlushCache();
    return true;
}
