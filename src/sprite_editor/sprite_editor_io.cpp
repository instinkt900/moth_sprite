#include "common.h"
#include "sprite_editor.h"
#include "sprite_editor_config.h"

#include <moth/graphics/graphics/igraphics.h>
#include <moth/graphics/graphics/surface_context.h>
#include <moth/graphics/graphics/asset_context.h>

#include <nfd.h>

namespace {
    // File > Open Recent keeps this many projects.
    constexpr size_t kMaxRecentProjects = 10;
    // The project file format version this editor writes. A file with a higher version is not loaded.
    constexpr int kProjectFormatVersion = 1;
    char const* const kExportMessageId = "Export##export_message";

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

    // A path stored in a project file: relative to the project file's folder, or absolute when there is no relative
    // path (another drive).
    std::string ProjectRelativePath(std::filesystem::path const& target, std::filesystem::path const& projectPath) {
        std::filesystem::path const rel = target.lexically_relative(projectPath.parent_path());
        return rel.empty() ? target.string() : rel.string();
    }

    // A path read from a project file, made absolute.
    std::string ResolveProjectPath(std::string const& stored, std::filesystem::path const& projectPath) {
        return std::filesystem::absolute(projectPath.parent_path() / stored).lexically_normal().string();
    }

    // Write the cells and clips in the format that project files and sprite sheet descriptors share. Steps are
    // written as they are. A cell from another image is written as its image path, relative to filePath, and its
    // pivot; descriptors never have such cells, because Export packs them first.
    void WriteFramesAndClips(nlohmann::json& json, std::vector<CellEntry> const& frames,
                             std::vector<moth::gfx::SpriteSheet::ClipEntry> const& clips,
                             std::filesystem::path const& filePath) {
        nlohmann::json framesJson = nlohmann::json::array();
        for (auto const& fr : frames) {
            nlohmann::json obj;
            if (fr.source) {
                obj["image"] = ProjectRelativePath(fr.source->path, filePath);
            } else {
                obj["x"] = fr.rect.x();
                obj["y"] = fr.rect.y();
                obj["w"] = fr.rect.w();
                obj["h"] = fr.rect.h();
            }
            obj["pivot_x"] = fr.pivot.x;
            obj["pivot_y"] = fr.pivot.y;
            framesJson.push_back(std::move(obj));
        }
        json["frames"] = std::move(framesJson);

        nlohmann::json clipsJson = nlohmann::json::array();
        for (auto const& entry : clips) {
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
    }

    // Write a JSON file. Returns false when it could not be written completely.
    bool WriteJsonFile(std::filesystem::path const& path, nlohmann::json const& json) {
        try {
            // Dump first, because it throws on text that is not UTF-8, and the file must not be left empty.
            std::string const text = json.dump(2);
            std::ofstream ofile(path);
            if (!ofile.is_open()) {
                return false;
            }
            ofile << text;
            ofile.flush();
            return static_cast<bool>(ofile);
        } catch (std::exception const& e) {
            moth::core::log::error("SpriteEditor: failed to write '{}': {}", path.string(), e.what());
            return false;
        }
    }

    constexpr std::array<char const*, 4> kPaddingTypeNames{ "color", "extend", "mirror", "wrap" };
    constexpr std::array<char const*, 4> kPackFormatNames{ "png", "bmp", "tga", "jpeg" };

    // The index of name in names, or fallback when it is not there.
    int NameIndex(std::array<char const*, 4> const& names, std::string const& name, int fallback) {
        for (size_t i = 0; i < names.size(); ++i) {
            if (name == names[i]) {
                return static_cast<int>(i);
            }
        }
        return fallback;
    }

    nlohmann::json PackSettingsToJson(PackSettings const& settings, std::filesystem::path const& projectPath) {
        nlohmann::json json;
        json["image"] = ProjectRelativePath(settings.imagePath, projectPath);
        json["padding"] = settings.padding;
        json["padding_type"] = kPaddingTypeNames[static_cast<size_t>(settings.paddingType)];
        json["padding_color"] = fmt::format("{:08x}", settings.paddingColor);
        json["best_pack"] = settings.bestPack;
        json["min_width"] = settings.minWidth;
        json["min_height"] = settings.minHeight;
        json["max_width"] = settings.maxWidth;
        json["max_height"] = settings.maxHeight;
        json["format"] = kPackFormatNames[static_cast<size_t>(settings.format)];
        json["jpeg_quality"] = settings.jpegQuality;
        return json;
    }

    // Missing fields keep their defaults. Throws on fields of the wrong type.
    PackSettings PackSettingsFromJson(nlohmann::json const& json, std::filesystem::path const& projectPath) {
        PackSettings settings;
        settings.imagePath = ResolveProjectPath(json.at("image").get<std::string>(), projectPath);
        settings.padding = std::max(json.value("padding", settings.padding), 0);
        settings.paddingType = static_cast<moth::packer::PaddingType>(
            NameIndex(kPaddingTypeNames, json.value("padding_type", std::string{}), 0));
        settings.paddingColor = static_cast<uint32_t>(std::stoul(json.value("padding_color", std::string{ "0" }), nullptr, 16));
        settings.bestPack = json.value("best_pack", settings.bestPack);
        settings.minWidth = std::max(json.value("min_width", settings.minWidth), 1);
        settings.minHeight = std::max(json.value("min_height", settings.minHeight), 1);
        settings.maxWidth = std::max(json.value("max_width", settings.maxWidth), settings.minWidth);
        settings.maxHeight = std::max(json.value("max_height", settings.maxHeight), settings.minHeight);
        settings.format = static_cast<moth::packer::AtlasFormat>(
            NameIndex(kPackFormatNames, json.value("format", std::string{}), 0));
        settings.jpegQuality = std::clamp(json.value("jpeg_quality", settings.jpegQuality), 1, 100);
        return settings;
    }

    // The contents of a project file, read before any editor state changes.
    struct ProjectFileData {
        std::string imagePath; // absolute, or empty when the project has no sheet image
        std::string exportPath; // absolute, or empty when the project has not been exported
        std::optional<PackSettings> pack; // set when the project has been packed
        std::vector<CellEntry> frames; // a cell from another image has a source with its path and no image yet
        std::vector<moth::gfx::SpriteSheet::ClipEntry> clips;
    };

    // Read the cells and clips that project files and sprite sheet descriptors share. They are read as they were
    // written, with no checks for what games reject, because the project is the editing source. Throws on fields of
    // the wrong type or a cell with no rectangle.
    void ReadFramesAndClips(nlohmann::json const& json, std::filesystem::path const& path, ProjectFileData& data) {
        for (auto const& frameJson : json.value("frames", nlohmann::json::array())) {
            CellEntry frame;
            if (frameJson.contains("image")) {
                frame.source = std::make_shared<CellImage const>(
                    CellImage{ ResolveProjectPath(frameJson.at("image").get<std::string>(), path), {} });
            } else {
                frame.rect = moth::gfx::MakeRect(frameJson.at("x").get<int>(), frameJson.at("y").get<int>(),
                                                 frameJson.at("w").get<int>(), frameJson.at("h").get<int>());
            }
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
    }

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
                data.imagePath = ResolveProjectPath(json.at("image").get<std::string>(), path);
            }
            if (json.contains("export_path")) {
                data.exportPath = ResolveProjectPath(json.at("export_path").get<std::string>(), path);
            }
            if (json.contains("pack")) {
                data.pack = PackSettingsFromJson(json.at("pack"), path);
            }
            ReadFramesAndClips(json, path, data);
            return data;
        } catch (std::exception const& e) {
            moth::core::log::error("SpriteEditor: failed to read project '{}': {}", path.string(), e.what());
            return std::nullopt;
        }
    }

    // Read a sprite sheet descriptor, the file File > Export writes and games load. The editor reads it itself,
    // rather than with SpriteSheetFactory, so it decides what is fatal: a sheet image that cannot be loaded is not,
    // and neither is data that games reject. Returns nothing, with a logged error, when the file cannot be parsed,
    // has no 'image' string field, or has no frames.
    std::optional<ProjectFileData> ReadDescriptorFile(std::filesystem::path const& path) {
        std::ifstream ifile(path);
        if (!ifile.is_open()) {
            moth::core::log::error("SpriteEditor: failed to open descriptor '{}'", path.string());
            return std::nullopt;
        }
        try {
            nlohmann::json json;
            ifile >> json;
            if (!json.contains("image") || !json.at("image").is_string()) {
                moth::core::log::error("SpriteEditor: descriptor '{}' has no 'image' field", path.string());
                return std::nullopt;
            }

            ProjectFileData data;
            data.imagePath = ResolveProjectPath(json.at("image").get<std::string>(), path);
            ReadFramesAndClips(json, path, data);
            if (data.frames.empty()) {
                moth::core::log::error("SpriteEditor: descriptor '{}' has no frames", path.string());
                return std::nullopt;
            }
            return data;
        } catch (std::exception const& e) {
            moth::core::log::error("SpriteEditor: failed to read descriptor '{}': {}", path.string(), e.what());
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
    m_stepSelectionClip = -1;
    m_stepSelection.clear();
    m_stepSelectionAnchor = -1;
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
    m_exportPath.clear();
    m_packSettings.reset();
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
    // Load the images of cells from other images, once per file. A missing image does not stop the project from
    // loading: the cell keeps its path, and has no image and a size of 0.
    std::map<std::string, std::shared_ptr<CellImage const>> cellImages;
    for (auto& frame : data->frames) {
        if (!frame.source) {
            continue;
        }
        std::string const cellImagePath = frame.source->path;
        auto found = cellImages.find(cellImagePath);
        if (found == cellImages.end()) {
            CellImage cellImage{ cellImagePath, {} };
            std::shared_ptr<moth::gfx::ITexture> texture(m_assetContext.TextureFromFile(cellImagePath));
            if (texture) {
                cellImage.image = moth::gfx::Image{ texture };
            } else {
                moth::core::log::warn("SpriteEditor: project '{}' cell image '{}' could not be loaded",
                    path.string(), cellImagePath);
            }
            found = cellImages.emplace(cellImagePath, std::make_shared<CellImage const>(std::move(cellImage))).first;
        }
        frame.source = found->second;
        frame.rect = moth::gfx::MakeRect(0, 0, frame.source->image.GetWidth(), frame.source->image.GetHeight());
    }
    auto sheet = std::make_shared<moth::gfx::SpriteSheet>(std::move(image), ToFrameEntries(data->frames), data->clips);

    ReplaceProject(std::move(sheet), data->imagePath, std::move(data->frames), std::move(data->clips));
    m_exportPath = data->exportPath;
    m_packSettings = data->pack;
    std::string const pathStr = path.string();
    strncpy(m_pathBuffer, pathStr.c_str(), sizeof(m_pathBuffer) - 1);
    m_pathBuffer[sizeof(m_pathBuffer) - 1] = '\0';
    MarkSaved();
    AddRecentProject(path);
}

void SpriteEditor::ImportDescriptor(std::filesystem::path const& path) {
    // Read the whole file before touching any editor state, so a failed import leaves the open project intact.
    std::optional<ProjectFileData> data = ReadDescriptorFile(path);
    if (!data.has_value()) {
        return;
    }

    // As for a project file, a sheet image that does not load does not stop the descriptor from being imported. The
    // project keeps the image path, so saving does not lose it.
    moth::gfx::Image image;
    std::shared_ptr<moth::gfx::ITexture> texture(m_assetContext.TextureFromFile(data->imagePath));
    if (texture) {
        image = moth::gfx::Image{ texture };
    } else {
        moth::core::log::warn("SpriteEditor: descriptor '{}' sheet image '{}' could not be loaded",
            path.string(), data->imagePath);
    }
    auto sheet = std::make_shared<moth::gfx::SpriteSheet>(std::move(image), ToFrameEntries(data->frames), data->clips);

    // The import is a new project with no path, so the first save opens Save As and never writes over the
    // descriptor. It is not added to Open Recent, which lists project files only.
    ReplaceProject(std::move(sheet), data->imagePath, std::move(data->frames), std::move(data->clips));
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

std::vector<moth::gfx::SpriteSheet::FrameEntry> SpriteEditor::ToFrameEntries(FrameVec const& cells) {
    return { cells.begin(), cells.end() };
}

void SpriteEditor::ImportCells(std::vector<std::filesystem::path> const& imagePaths) {
    auto before = m_frames;
    Selection const beforeSel = m_selection;
    size_t const firstNew = m_frames.size();
    for (auto const& imagePath : imagePaths) {
        std::shared_ptr<moth::gfx::ITexture> texture(m_assetContext.TextureFromFile(imagePath));
        if (!texture) {
            moth::core::log::error("SpriteEditor: failed to load image '{}'", imagePath.string());
            continue;
        }
        std::error_code ec;
        std::filesystem::path absolutePath = std::filesystem::absolute(imagePath, ec);
        if (ec) {
            absolutePath = imagePath;
        }
        CellEntry cell;
        cell.source = std::make_shared<CellImage const>(
            CellImage{ absolutePath.lexically_normal().string(), moth::gfx::Image{ texture } });
        cell.rect = moth::gfx::MakeRect(0, 0, cell.source->image.GetWidth(), cell.source->image.GetHeight());
        cell.pivot = { 0, 0 };
        m_frames.push_back(std::move(cell));
    }
    if (m_frames.size() == firstNew) {
        return;
    }
    // Select the first new cell, as the other ways of adding cells do.
    m_selection = { static_cast<int>(firstNew) };
    PushFrameAction(std::move(before), beforeSel, m_selection);
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
    auto const importedSheet = std::make_shared<moth::gfx::SpriteSheet>(std::move(image), ToFrameEntries(m_frames), m_clips);
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

void SpriteEditor::ExportSheet(std::filesystem::path const& imagePath) {
    std::filesystem::path const sourcePath = m_imagePathBuffer;
    std::error_code ec;
    // Exporting the sheet image over itself needs no copy, and would empty the file.
    if (!std::filesystem::equivalent(sourcePath, imagePath, ec)) {
        ec.clear();
        std::filesystem::copy_file(sourcePath, imagePath, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            std::string message = fmt::format("Could not write the sheet image '{}' to '{}': {}", sourcePath.string(),
                                              imagePath.string(), ec.message());
            moth::core::log::error("SpriteEditor: {}", message);
            ShowExportMessage("The sprite sheet could not be exported:", { std::move(message) });
            return;
        }
    }
    moth::core::log::info("SpriteEditor: exported the sheet image to '{}'", imagePath.string());

    // The project uses the exported file from now on, as one undoable action. Undo does not remove the file.
    std::string const exportedPath = imagePath.string();
    std::string const previousPath = m_imagePathBuffer;
    if (exportedPath == previousPath) {
        return;
    }
    auto const setPath = [this](std::string const& path) {
        strncpy(m_imagePathBuffer, path.c_str(), sizeof(m_imagePathBuffer) - 1);
        m_imagePathBuffer[sizeof(m_imagePathBuffer) - 1] = '\0';
    };
    setPath(exportedPath);
    AddSpriteAction(std::make_unique<BasicAction>(
        [setPath, exportedPath]() { setPath(exportedPath); },
        [setPath, previousPath]() { setPath(previousPath); }
    ));
}

std::vector<std::string> SpriteEditor::ExportProblems(bool beforePack) const {
    // Everything that SpriteSheetFactory rejects or skips, so the game data never differs from the project.
    std::vector<std::string> problems;
    // Before a pack, the sheet image and the cell sizes are left to the pack: it makes the sheet image, and it
    // refuses cells it cannot pack without changing the project.
    if (!beforePack) {
        if (m_imagePathBuffer[0] == '\0') {
            problems.emplace_back("The project has no sheet image.");
        } else if (!m_spriteSheet || !m_spriteSheet->GetImage()) {
            problems.push_back(fmt::format("The project's sheet image '{}' could not be loaded.", m_imagePathBuffer));
        }
    }
    if (m_frames.empty()) {
        problems.emplace_back("The project has no cells.");
    }
    int const frameCount = static_cast<int>(m_frames.size());
    for (int i = 0; i < frameCount && !beforePack; ++i) {
        auto const& rect = m_frames[static_cast<size_t>(i)].rect;
        if (rect.w() <= 0 || rect.h() <= 0) {
            problems.push_back(fmt::format("Cell #{} has a size of {} x {}.", i, rect.w(), rect.h()));
        }
    }
    for (auto const& clip : m_clips) {
        auto const& steps = clip.desc.frames;
        if (steps.empty()) {
            problems.push_back(fmt::format("Clip \"{}\" has no steps.", clip.name));
        }
        for (size_t i = 0; i < steps.size(); ++i) {
            if (steps[i].durationMs <= 0) {
                problems.push_back(fmt::format("Clip \"{}\" step {} has a duration of {} ms.", clip.name, i + 1,
                                               steps[i].durationMs));
            }
            if (steps[i].frameIndex < 0 || steps[i].frameIndex >= frameCount) {
                problems.push_back(fmt::format("Clip \"{}\" step {} has no cell.", clip.name, i + 1));
            }
        }
    }
    return problems;
}

void SpriteEditor::ShowExportMessage(std::string heading, std::vector<std::string> lines) {
    m_exportMessage.heading = std::move(heading);
    m_exportMessage.lines = std::move(lines);
    m_exportMessage.open = true;
}

void SpriteEditor::ExportProject(bool choosePath) {
    // Cells from other images must be packed onto the sheet first.
    bool const needsPack =
        std::any_of(m_frames.begin(), m_frames.end(), [](CellEntry const& cell) { return cell.source != nullptr; });

    // Refuse before asking for a path, and write no files. When a pack comes first, the problems a pack cannot fix
    // are checked now, so a pack never changes the project for an export that will be refused.
    std::vector<std::string> problems = ExportProblems(needsPack);
    if (!problems.empty()) {
        ShowExportMessage("The project cannot be exported:", std::move(problems));
        return;
    }

    std::filesystem::path exportPath = m_exportPath;
    if (choosePath || exportPath.empty()) {
        // Start in the folder of the last export, else the project's folder, else the last project dialog folder.
        std::filesystem::path folder;
        if (!m_exportPath.empty()) {
            folder = std::filesystem::path(m_exportPath).parent_path();
        } else if (m_pathBuffer[0] != '\0') {
            folder = std::filesystem::path(m_pathBuffer).parent_path();
        }
        std::error_code ec;
        std::string startDir = std::filesystem::current_path().string();
        if (std::filesystem::is_directory(folder, ec)) {
            startDir = folder.string();
        } else if (!m_config.LastProjectDir.empty()) {
            startDir = m_config.LastProjectDir;
        }
        nfdchar_t* outPath = nullptr;
        if (NFD_SaveDialog("json", startDir.c_str(), &outPath) != NFD_OKAY || outPath == nullptr) {
            return;
        }
        exportPath = outPath;
        NFD_Free(outPath);
        if (exportPath.extension() != ".json") {
            exportPath += ".json";
        }
    }
    std::error_code ec;
    std::filesystem::path absolutePath = std::filesystem::absolute(exportPath, ec);
    if (ec) {
        absolutePath = exportPath;
    }
    exportPath = absolutePath.lexically_normal();

    // Nothing is written yet. The pack dialog opens with the packed image named after the descriptor, and the export
    // continues after a successful pack.
    if (needsPack) {
        m_exportAfterPack = exportPath;
        m_openPackDialog = true;
        return;
    }
    ExportToPath(exportPath);
}

void SpriteEditor::ExportToPath(std::filesystem::path const& exportPath) {
    std::vector<std::string> problems = ExportProblems(false);
    if (!problems.empty()) {
        ShowExportMessage("The project cannot be exported:", std::move(problems));
        return;
    }

    // Only the descriptor is written. The sheet image the project already has stays where it is, and an export that
    // packs first has just written its packed image beside the descriptor.
    nlohmann::json json = nlohmann::json::object();
    json["image"] = ProjectRelativePath(m_imagePathBuffer, exportPath);
    WriteFramesAndClips(json, m_frames, m_clips, exportPath);
    if (!WriteJsonFile(exportPath, json)) {
        std::string message = fmt::format("Could not write '{}'.", exportPath.string());
        moth::core::log::error("SpriteEditor: {}", message);
        ShowExportMessage("The export failed:", { std::move(message) });
        return;
    }
    moth::core::log::info("SpriteEditor: exported '{}'", exportPath.string());

    // The project remembers a new export path as one undoable action. Undo does not remove the exported files.
    std::string const exportStr = exportPath.string();
    if (exportStr == m_exportPath) {
        return;
    }
    std::string const previousStr = m_exportPath;
    m_exportPath = exportStr;
    AddSpriteAction(std::make_unique<BasicAction>(
        [this, exportStr]()   { m_exportPath = exportStr; },
        [this, previousStr]() { m_exportPath = previousStr; }
    ));
}

void SpriteEditor::DrawExportMessage() {
    if (m_exportMessage.open) {
        m_exportMessage.open = false;
        ImGui::OpenPopup(kExportMessageId);
    }
    ImGuiViewport const* const viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2{ 0.5f, 0.5f });
    if (!ImGui::BeginPopupModal(kExportMessageId, nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        return;
    }
    ImGui::TextUnformatted(m_exportMessage.heading.c_str());
    for (auto const& line : m_exportMessage.lines) {
        ImGui::BulletText("%s", line.c_str());
    }
    ImGui::Spacing();
    constexpr float kButtonW = 110.0f;
    if (ImGui::Button("OK", ImVec2{ kButtonW, 0.0f }) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

bool SpriteEditor::SaveSpriteSheet(std::filesystem::path const& path) {
    if (path.empty()) {
        return false;
    }

    // The project file is written from scratch with the current format.
    nlohmann::json json = nlohmann::json::object();
    json["version"] = kProjectFormatVersion;

    // The sheet image and the export path are optional.
    if (m_imagePathBuffer[0] != '\0') {
        json["image"] = ProjectRelativePath(m_imagePathBuffer, path);
    }
    if (!m_exportPath.empty()) {
        json["export_path"] = ProjectRelativePath(m_exportPath, path);
    }
    if (m_packSettings.has_value()) {
        json["pack"] = PackSettingsToJson(*m_packSettings, path);
    }

    // Steps are written as they are, so a project keeps clips with no steps, 0 ms steps, and the steps of a project
    // with no cells.
    WriteFramesAndClips(json, m_frames, m_clips, path);

    // WriteJsonFile serializes before it opens the file, so a failed save never empties an existing project file.
    // Callers go on (for example, quit) only after a successful save, so it also checks that the write worked.
    if (!WriteJsonFile(path, json)) {
        moth::core::log::error("SpriteEditor: failed to write '{}'", path.string());
        return false;
    }
    moth::core::log::info("SpriteEditor: saved '{}'", path.string());
    // The file was written, so the project uses it from now on.
    std::string const pathStr = path.string();
    strncpy(m_pathBuffer, pathStr.c_str(), sizeof(m_pathBuffer) - 1);
    m_pathBuffer[sizeof(m_pathBuffer) - 1] = '\0';
    AddRecentProject(path);
    MarkSaved();
    return true;
}
