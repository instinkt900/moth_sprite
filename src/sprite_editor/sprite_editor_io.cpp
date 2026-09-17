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
    // The project file format version this editor writes. A file with a higher version is not loaded.
    constexpr int kProjectFormatVersion = 1;

    char const* LoopTypeName(moth::gfx::SpriteSheet::LoopType loop) {
        switch (loop) {
        case moth::gfx::SpriteSheet::LoopType::Reset: return "reset";
        case moth::gfx::SpriteSheet::LoopType::Loop:  return "loop";
        case moth::gfx::SpriteSheet::LoopType::Stop:
        default:                                      return "stop";
        }
    }

    moth::gfx::SpriteSheet::LoopType ParseLoopType(std::string const& name) {
        if (name == "reset") {
            return moth::gfx::SpriteSheet::LoopType::Reset;
        }
        if (name == "loop") {
            return moth::gfx::SpriteSheet::LoopType::Loop;
        }
        return moth::gfx::SpriteSheet::LoopType::Stop;
    }

    // The contents of a project file, read before any editor state changes.
    struct ProjectFileData {
        std::string imagePath; // absolute, or empty when the project has no sheet image
        std::vector<moth::gfx::SpriteSheet::FrameEntry> frames;
        std::vector<moth::gfx::SpriteSheet::ClipEntry> clips;
    };

    // Read a project file. The project is the editing source, so cells, clips and steps are read as they were saved,
    // with no checks for what games reject. Returns nothing, with a logged error, when the file cannot be read.
    std::optional<ProjectFileData> ReadProjectFile(std::filesystem::path const& path) {
        std::ifstream ifile(path);
        if (!ifile.is_open()) {
            moth::core::log::error("SpriteEditor: failed to open project '{}'", path.string());
            return std::nullopt;
        }
        try {
            nlohmann::json json;
            ifile >> json;
            int const version = json.at("version").get<int>();
            if (version < 1 || version > kProjectFormatVersion) {
                moth::core::log::error("SpriteEditor: project '{}' has format version {}, this editor reads version {}",
                    path.string(), version, kProjectFormatVersion);
                return std::nullopt;
            }

            ProjectFileData data;
            if (json.contains("image")) {
                data.imagePath = std::filesystem::absolute(path.parent_path() / json.at("image").get<std::string>())
                                     .lexically_normal()
                                     .string();
            }
            for (auto const& frameJson : json.value("frames", nlohmann::json::array())) {
                moth::gfx::SpriteSheet::FrameEntry frame;
                frame.rect = moth::gfx::MakeRect(frameJson.at("x").get<int>(), frameJson.at("y").get<int>(),
                                                 frameJson.at("w").get<int>(), frameJson.at("h").get<int>());
                frame.pivot.x = frameJson.value("pivot_x", 0);
                frame.pivot.y = frameJson.value("pivot_y", 0);
                data.frames.push_back(frame);
            }
            for (auto const& clipJson : json.value("clips", nlohmann::json::array())) {
                moth::gfx::SpriteSheet::ClipEntry clip;
                clip.name = clipJson.at("name").get<std::string>();
                clip.desc.loop = ParseLoopType(clipJson.value("loop", std::string{ "stop" }));
                for (auto const& stepJson : clipJson.value("frames", nlohmann::json::array())) {
                    moth::gfx::SpriteSheet::ClipFrame step;
                    step.frameIndex = stepJson.at("frame").get<int>();
                    step.durationMs = stepJson.at("duration_ms").get<int>();
                    clip.desc.frames.push_back(step);
                }
                data.clips.push_back(std::move(clip));
            }
            return data;
        } catch (std::exception const& e) {
            moth::core::log::error("SpriteEditor: failed to read project '{}': {}", path.string(), e.what());
            return std::nullopt;
        }
    }
} // namespace

void SpriteEditor::LoadSpriteSheet(std::filesystem::path const& path) {
    if (path.extension() == ".json") {
        ImportDescriptor(path);
    } else {
        LoadProjectFile(path);
    }
}

void SpriteEditor::ReplaceProject(std::shared_ptr<moth::gfx::SpriteSheet> sheet, std::string const& imagePath,
                                  FrameVec frames, ClipVec clips) {
    ClearSpriteActions();
    m_selection.clear();
    m_selectedClip = -1;
    m_clipPlaying = false;
    m_clipCurrentStep = 0;
    m_clipElapsedMs = 0.0f;
    m_zoom = -1.0f; // trigger auto-fit on next draw
    m_cellZoom = -1.0f;
    strncpy(m_imagePathBuffer, imagePath.c_str(), sizeof(m_imagePathBuffer) - 1);
    m_imagePathBuffer[sizeof(m_imagePathBuffer) - 1] = '\0';
    m_spriteSheet = std::move(sheet);
    m_frames = std::move(frames);
    m_clips = std::move(clips);
}

void SpriteEditor::LoadProjectFile(std::filesystem::path const& path) {
    // Read the whole file before touching any editor state, so a failed load leaves the open project intact.
    std::optional<ProjectFileData> data = ReadProjectFile(path);
    if (!data.has_value()) {
        return;
    }

    // A sheet image that does not load does not stop the project from loading. The project keeps the image path, so
    // saving does not lose it.
    moth::gfx::Image image;
    if (!data->imagePath.empty()) {
        std::shared_ptr<moth::gfx::ITexture> texture(m_assetContext.TextureFromFile(data->imagePath));
        if (texture) {
            image = moth::gfx::Image{ texture };
        } else {
            moth::core::log::warn("SpriteEditor: project '{}' sheet image '{}' could not be loaded",
                path.string(), data->imagePath);
        }
    }
    auto sheet = std::make_shared<moth::gfx::SpriteSheet>(std::move(image), data->frames, data->clips);

    ReplaceProject(std::move(sheet), data->imagePath, std::move(data->frames), std::move(data->clips));
    std::string const pathStr = path.string();
    strncpy(m_pathBuffer, pathStr.c_str(), sizeof(m_pathBuffer) - 1);
    m_pathBuffer[sizeof(m_pathBuffer) - 1] = '\0';
    MarkSaved();
    AddRecentProject(path);
}

void SpriteEditor::ImportDescriptor(std::filesystem::path const& path) {
    // Load and validate before touching any editor state so a failed load
    // leaves the current document intact. The cache is flushed so the file is read as it is now.
    auto& spriteSheetFactory = m_assetContext.GetSpriteSheetFactory();
    spriteSheetFactory.FlushCache();
    auto newSheet = spriteSheetFactory.GetSpriteSheet(path);
    if (!newSheet) {
        moth::core::log::error("Failed to load sprite sheet: {}", path.string());
        return;
    }

    // Read the image path from the JSON so Import Sheet and Save know it
    std::string imagePath;
    try {
        std::ifstream ifile(path);
        if (ifile.is_open()) {
            nlohmann::json json;
            ifile >> json;
            if (json.contains("image") && json["image"].is_string()) {
                imagePath = std::filesystem::absolute(path.parent_path() / json["image"].get<std::string>())
                                .lexically_normal()
                                .string();
            }
        }
    } catch (std::exception const& e) {
        moth::core::log::warn("SpriteEditor: could not read image path from '{}': {}", path.string(), e.what());
    }

    FrameVec frames;
    frames.reserve(static_cast<size_t>(newSheet->GetFrameCount()));
    for (int i = 0; i < newSheet->GetFrameCount(); ++i) {
        if (auto entry = newSheet->GetFrameDesc(i)) {
            frames.push_back(*entry);
        }
    }

    ClipVec clips;
    int const clipCount = newSheet->GetClipCount();
    clips.reserve(static_cast<size_t>(clipCount));
    for (int i = 0; i < clipCount; ++i) {
        moth::gfx::SpriteSheet::ClipEntry entry;
        entry.name = newSheet->GetClipName(i);
        if (auto desc = newSheet->GetClipDesc(entry.name)) {
            entry.desc = *desc;
        }
        clips.push_back(std::move(entry));
    }

    // The import is a new project with no path, so the first save opens Save As and never writes over the
    // descriptor. It is not added to Open Recent, which lists project files only.
    ReplaceProject(std::move(newSheet), imagePath, std::move(frames), std::move(clips));
    m_pathBuffer[0] = '\0';
    MarkUnsaved();
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
    if (path.empty()) {
        return false;
    }

    // The project file is written from scratch with the current format.
    nlohmann::json json = nlohmann::json::object();
    json["version"] = kProjectFormatVersion;

    // The sheet image is optional.
    if (m_imagePathBuffer[0] != '\0') {
        std::filesystem::path const imagePath = m_imagePathBuffer;
        std::filesystem::path const relImage  = imagePath.lexically_relative(path.parent_path());
        // A relative path cannot reach an image on another drive, so that image keeps its absolute path.
        json["image"] = relImage.empty() ? imagePath.string() : relImage.string();
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

    // Write clips. Steps are written as they are, so a project keeps clips with no steps, 0 ms steps, and the steps
    // of a project with no cells.
    nlohmann::json clipsJson = nlohmann::json::array();
    for (auto const& entry : m_clips) {
        nlohmann::json clipObj;
        clipObj["name"] = entry.name;
        clipObj["loop"] = LoopTypeName(entry.desc.loop);

        nlohmann::json stepsJson = nlohmann::json::array();
        for (auto const& step : entry.desc.frames) {
            nlohmann::json stepObj;
            stepObj["frame"]       = step.frameIndex;
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
    return true;
}
