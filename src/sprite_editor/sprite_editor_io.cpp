#include "common.h"
#include "sprite_editor.h"
#include "sprite_editor_config.h"

#include <moth/graphics/graphics/igraphics.h>
#include <moth/graphics/graphics/surface_context.h>
#include <moth/graphics/graphics/asset_context.h>
#include <moth/graphics/graphics/spritesheet_factory.h>

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

    // Write the cells and clips in the format that project files and sprite sheet descriptors share. Steps are
    // written as they are.
    void WriteFramesAndClips(nlohmann::json& json, std::vector<moth::gfx::SpriteSheet::FrameEntry> const& frames,
                             std::vector<moth::gfx::SpriteSheet::ClipEntry> const& clips) {
        nlohmann::json framesJson = nlohmann::json::array();
        for (auto const& fr : frames) {
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
                data.imagePath = ResolveProjectPath(json.at("image").get<std::string>(), path);
            }
            if (json.contains("export_path")) {
                data.exportPath = ResolveProjectPath(json.at("export_path").get<std::string>(), path);
            }
            if (json.contains("pack")) {
                data.pack = PackSettingsFromJson(json.at("pack"), path);
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
    auto sheet = std::make_shared<moth::gfx::SpriteSheet>(std::move(image), data->frames, data->clips);

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

std::vector<std::string> SpriteEditor::ExportProblems() const {
    // Everything that SpriteSheetFactory rejects or skips, so the game data never differs from the project.
    std::vector<std::string> problems;
    if (m_imagePathBuffer[0] == '\0') {
        problems.emplace_back("The project has no sheet image.");
    }
    if (m_frames.empty()) {
        problems.emplace_back("The project has no cells.");
    }
    int const frameCount = static_cast<int>(m_frames.size());
    for (int i = 0; i < frameCount; ++i) {
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
    // Refuse before asking for a path, and write no files.
    std::vector<std::string> problems = ExportProblems();
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

    // The sheet image is copied beside the descriptor, named after it with the image's extension.
    std::filesystem::path const sheetPath = m_imagePathBuffer;
    std::filesystem::path imageTarget = exportPath;
    imageTarget.replace_extension(sheetPath.extension());
    // Exporting beside the sheet image with its own name needs no copy.
    ec.clear();
    if (!std::filesystem::equivalent(sheetPath, imageTarget, ec)) {
        ec.clear();
        std::filesystem::copy_file(sheetPath, imageTarget, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            std::string message = fmt::format("Could not copy the sheet image '{}' to '{}': {}", sheetPath.string(),
                                              imageTarget.string(), ec.message());
            moth::core::log::error("SpriteEditor: {}", message);
            ShowExportMessage("The export failed:", { std::move(message) });
            return;
        }
    }

    nlohmann::json json = nlohmann::json::object();
    json["image"] = imageTarget.filename().string();
    WriteFramesAndClips(json, m_frames, m_clips);
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
    WriteFramesAndClips(json, m_frames, m_clips);

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
