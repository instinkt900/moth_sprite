#include "common.h"
#include "sprite_editor.h"
#include "sprite_editor_config.h"

#include <nfd.h>

#include <set>

namespace {
    char const* const kPackPopupId = "Pack##pack_dialog";
    char const* const kUnpackPopupId = "Unpack##unpack_dialog";
    constexpr float kPackLabelWidth = 130.0f;
    constexpr float kPackFieldWidth = 360.0f;
    constexpr int kMaxPackPadding = 1024;
    constexpr ImVec4 kPackWarningColor{ 1.0f, 0.45f, 0.35f, 1.0f };
    constexpr float kPackPreviewSize = 400.0f;
    constexpr float kPackPreviewCheckerSize = 16.0f;

    // The sizes offered for the minimum and maximum width and height.
    constexpr std::array<int, 15> kPackSizes{ 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384 };

    void PackLabel(char const* label) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine(kPackLabelWidth);
        ImGui::SetNextItemWidth(kPackFieldWidth);
    }

    // A combo of the power-of-two sizes. Returns true when the value changed.
    bool PackSizeCombo(char const* label, char const* id, int& value) {
        PackLabel(label);
        bool changed = false;
        std::string const preview = std::to_string(value);
        if (ImGui::BeginCombo(id, preview.c_str())) {
            for (int const size : kPackSizes) {
                bool const selected = size == value;
                if (ImGui::Selectable(std::to_string(size).c_str(), selected)) {
                    changed = size != value;
                    value = size;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    // Settings that change the packed pixels. The path, format and JPEG quality only change how the file is written.
    bool SamePackLayout(PackSettings const& a, PackSettings const& b) {
        return a.padding == b.padding && a.paddingType == b.paddingType && a.paddingColor == b.paddingColor &&
               a.bestPack == b.bestPack && a.minWidth == b.minWidth && a.minHeight == b.minHeight && a.maxWidth == b.maxWidth &&
               a.maxHeight == b.maxHeight;
    }

    // Two paths name the same file: the same file on disk, or the same absolute path when either does not exist.
    bool SameFile(std::filesystem::path const& a, std::filesystem::path const& b) {
        std::error_code ec;
        if (std::filesystem::equivalent(a, b, ec)) {
            return true;
        }
        ec.clear();
        std::filesystem::path const absA = std::filesystem::absolute(a, ec).lexically_normal();
        std::filesystem::path const absB = std::filesystem::absolute(b, ec).lexically_normal();
        return absA == absB;
    }
} // namespace

PackSettings SpriteEditor::InitialPackSettings() const {
    if (m_packSettings.has_value()) {
        return *m_packSettings;
    }
    // <project name>_packed beside the project, or in the last image folder for a project with no path.
    PackSettings settings;
    std::filesystem::path folder;
    std::string name = "Untitled";
    std::error_code ec;
    if (m_pathBuffer[0] != '\0') {
        std::filesystem::path const projectPath = m_pathBuffer;
        folder = projectPath.parent_path();
        name = projectPath.stem().string();
    } else if (!m_config.LastImageDir.empty() && std::filesystem::is_directory(m_config.LastImageDir, ec)) {
        folder = m_config.LastImageDir;
    } else {
        folder = std::filesystem::current_path(ec);
    }
    settings.imagePath = (folder / (name + "_packed" + PackFormatExtension(settings.format))).string();
    return settings;
}

void SpriteEditor::DrawPackDialog() {
    auto& dialog = m_packDialog;
    if (m_openPackDialog) {
        m_openPackDialog = false;
        // Every opening starts from the project's settings. Changes stay in the dialog until Pack succeeds.
        dialog.settings = InitialPackSettings();
        // For an export, the packed image goes beside the descriptor, with its name: hero.json packs to hero.png.
        if (m_exportAfterPack.has_value()) {
            std::filesystem::path packPath = *m_exportAfterPack;
            packPath.replace_extension(PackFormatExtension(dialog.settings.format));
            dialog.settings.imagePath = packPath.string();
        }
        strncpy(dialog.pathBuffer, dialog.settings.imagePath.c_str(), sizeof(dialog.pathBuffer) - 1);
        dialog.pathBuffer[sizeof(dialog.pathBuffer) - 1] = '\0';
        dialog.error.clear();
        dialog.previewSettings.reset();
        ImGui::OpenPopup(kPackPopupId);
    }
    ImGuiViewport const* const viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2{ 0.5f, 0.5f });
    if (!ImGui::BeginPopupModal(kPackPopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        // The preview and its source images are held only while the dialog is open.
        dialog.previewImages.clear();
        dialog.previewSettings.reset();
        ReleasePackPreviewImage();
        return;
    }
    auto& settings = dialog.settings;
    // Pack the preview before anything is drawn this frame, so this frame never draws a texture that is replaced.
    UpdatePackPreview();

    ImGui::BeginGroup();

    // Packed image path, with a browse button.
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Packed image");
    ImGui::SameLine(kPackLabelWidth);
    float const browseW = ImGui::CalcTextSize("...").x + (ImGui::GetStyle().FramePadding.x * 2.0f);
    ImGui::SetNextItemWidth(kPackFieldWidth - browseW - ImGui::GetStyle().ItemSpacing.x);
    if (ImGui::InputText("##pack_path", dialog.pathBuffer, sizeof(dialog.pathBuffer))) {
        dialog.error.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("...##pack_browse")) {
        std::filesystem::path const current = dialog.pathBuffer;
        std::error_code ec;
        std::string const startDir = std::filesystem::is_directory(current.parent_path(), ec)
                                         ? current.parent_path().string()
                                         : std::filesystem::current_path(ec).string();
        std::string const extension = PackFormatExtension(settings.format);
        nfdchar_t* outPath = nullptr;
        if (NFD_SaveDialog(extension.substr(1).c_str(), startDir.c_str(), &outPath) == NFD_OKAY && outPath != nullptr) {
            std::filesystem::path chosen = outPath;
            NFD_Free(outPath);
            if (chosen.extension() != extension) {
                chosen += extension;
            }
            std::string const chosenStr = chosen.string();
            strncpy(dialog.pathBuffer, chosenStr.c_str(), sizeof(dialog.pathBuffer) - 1);
            dialog.pathBuffer[sizeof(dialog.pathBuffer) - 1] = '\0';
            dialog.error.clear();
        }
    }

    PackLabel("Padding (px)");
    ImGui::InputInt("##pack_padding", &settings.padding);
    settings.padding = std::clamp(settings.padding, 0, kMaxPackPadding);

    static constexpr std::array<char const*, 4> kPaddingTypeNames{ "Color", "Extend", "Mirror", "Wrap" };
    int paddingType = static_cast<int>(settings.paddingType);
    PackLabel("Padding type");
    if (ImGui::Combo("##pack_padding_type", &paddingType, kPaddingTypeNames.data(), static_cast<int>(kPaddingTypeNames.size()))) {
        settings.paddingType = static_cast<moth::packer::PaddingType>(paddingType);
    }
    if (settings.paddingType == moth::packer::PaddingType::Color) {
        // RRGGBBAA
        std::array<float, 4> color{};
        for (size_t i = 0; i < color.size(); ++i) {
            color[i] = static_cast<float>((settings.paddingColor >> (24U - (8U * i))) & 0xFFU) / 255.0f;
        }
        PackLabel("Padding color");
        if (ImGui::ColorEdit4("##pack_padding_color", color.data(),
                              ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf)) {
            uint32_t packed = 0;
            for (size_t i = 0; i < color.size(); ++i) {
                auto const channel = static_cast<uint32_t>(std::lround(std::clamp(color[i], 0.0f, 1.0f) * 255.0f));
                packed |= channel << (24U - (8U * i));
            }
            settings.paddingColor = packed;
        }
    }

    PackLabel("Best pack");
    ImGui::Checkbox("##pack_best", &settings.bestPack);
    ImGui::SetItemTooltip("Pack into the smallest image the packer can find, up to %d x %d. The size limits are not used.",
                          kBestPackMaxSize, kBestPackMaxSize);

    // With Best pack, the size limits are disabled and show the range it uses; the chosen limits are kept for later.
    if (settings.bestPack) {
        dialog.bestPackSizes = EffectivePackSettings(settings);
    }
    auto& sizes = settings.bestPack ? dialog.bestPackSizes : settings;
    ImGui::BeginDisabled(settings.bestPack);
    // The minimum never goes above the maximum: changing one moves the other.
    if (PackSizeCombo("Min width", "##pack_min_w", sizes.minWidth)) {
        sizes.maxWidth = std::max(sizes.maxWidth, sizes.minWidth);
    }
    if (PackSizeCombo("Min height", "##pack_min_h", sizes.minHeight)) {
        sizes.maxHeight = std::max(sizes.maxHeight, sizes.minHeight);
    }
    if (PackSizeCombo("Max width", "##pack_max_w", sizes.maxWidth)) {
        sizes.minWidth = std::min(sizes.minWidth, sizes.maxWidth);
    }
    if (PackSizeCombo("Max height", "##pack_max_h", sizes.maxHeight)) {
        sizes.minHeight = std::min(sizes.minHeight, sizes.maxHeight);
    }
    ImGui::EndDisabled();

    static constexpr std::array<char const*, 4> kFormatNames{ "PNG", "BMP", "TGA", "JPEG" };
    int format = static_cast<int>(settings.format);
    PackLabel("Format");
    if (ImGui::Combo("##pack_format", &format, kFormatNames.data(), static_cast<int>(kFormatNames.size()))) {
        settings.format = static_cast<moth::packer::AtlasFormat>(format);
        // The path's extension follows the format.
        std::filesystem::path path = dialog.pathBuffer;
        if (!path.empty()) {
            path.replace_extension(PackFormatExtension(settings.format));
            std::string const pathStr = path.string();
            strncpy(dialog.pathBuffer, pathStr.c_str(), sizeof(dialog.pathBuffer) - 1);
            dialog.pathBuffer[sizeof(dialog.pathBuffer) - 1] = '\0';
        }
    }
    if (settings.format == moth::packer::AtlasFormat::JPEG) {
        PackLabel("JPEG quality");
        ImGui::SliderInt("##pack_jpeg_quality", &settings.jpegQuality, 1, 100);
        settings.jpegQuality = std::clamp(settings.jpegQuality, 1, 100);
    }

    // Only an empty path is refused. Writing over an existing file, even a source image of the project, is allowed
    // with a warning.
    std::string refusal;
    std::string warning;
    std::filesystem::path const packPath = dialog.pathBuffer;
    std::error_code existsError;
    if (packPath.empty()) {
        refusal = "Choose a path for the packed image.";
    } else if (std::filesystem::exists(packPath, existsError)) {
        // Each source image file is checked once, however many cells use it.
        std::set<std::string> sourcePaths;
        if (m_imagePathBuffer[0] != '\0') {
            sourcePaths.insert(m_imagePathBuffer);
        }
        for (auto const& cell : m_frames) {
            if (cell.source) {
                sourcePaths.insert(cell.source->path);
            }
        }
        bool const isSource = std::any_of(sourcePaths.begin(), sourcePaths.end(),
                                          [&packPath](std::string const& sourcePath) { return SameFile(packPath, sourcePath); });
        // The source pixels are read before the file is written, so the pack itself is correct. Undo restores the
        // project, but not the file on disk.
        warning = isSource ? "This file is a source image of the project and will be overwritten. Undo does not restore "
                             "the file, so the project will not match it after an undo."
                           : "This file already exists and will be overwritten.";
    }
    // A refusal, else the last Pack error, else the overwrite warning.
    std::string const* message = &warning;
    if (!refusal.empty()) {
        message = &refusal;
    } else if (!dialog.error.empty()) {
        message = &dialog.error;
    }
    if (!message->empty()) {
        ImGui::Spacing();
        ImGui::PushTextWrapPos(kPackLabelWidth + kPackFieldWidth);
        ImGui::TextColored(kPackWarningColor, "%s", message->c_str());
        ImGui::PopTextWrapPos();
    }

    ImGui::EndGroup();
    ImGui::SameLine();
    DrawPackPreview();

    ImGui::Spacing();
    constexpr float kButtonW = 110.0f;
    bool close = false;
    bool packed = false;
    ImGui::BeginDisabled(!refusal.empty());
    if (ImGui::Button("Pack", ImVec2{ kButtonW, 0.0f })) {
        std::error_code ec;
        std::filesystem::path absolutePath = std::filesystem::absolute(packPath, ec);
        settings.imagePath = (ec ? packPath : absolutePath).lexically_normal().string();
        dialog.error.clear();
        packed = PackProject(settings, dialog.error);
        close = packed;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2{ kButtonW, 0.0f }) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        close = true;
    }
    if (close) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();

    // An export that needed a pack continues after a successful pack. Cancelling the dialog cancels the export.
    if (close) {
        std::optional<std::filesystem::path> const exportAfterPack = m_exportAfterPack;
        m_exportAfterPack.reset();
        if (packed && exportAfterPack.has_value()) {
            ExportToPath(*exportAfterPack);
        }
    }
}

std::optional<std::vector<PackCell>> SpriteEditor::ProjectPackCells(std::string& error) const {
    std::vector<PackCell> cells;
    cells.reserve(m_frames.size());
    for (size_t i = 0; i < m_frames.size(); ++i) {
        auto const& frame = m_frames[i];
        if (frame.source) {
            if (!frame.source->image) {
                error = fmt::format("Could not read the image '{}' of cell #{}.", frame.source->path, i);
                return std::nullopt;
            }
            cells.push_back({ frame.source->path, frame.rect });
        } else {
            if (m_imagePathBuffer[0] == '\0') {
                error = "The project has no sheet image.";
                return std::nullopt;
            }
            cells.push_back({ m_imagePathBuffer, frame.rect });
        }
    }
    return cells;
}

void SpriteEditor::UpdatePackPreview() {
    auto& dialog = m_packDialog;
    if (dialog.previewSettings.has_value() && SamePackLayout(*dialog.previewSettings, dialog.settings)) {
        return;
    }
    dialog.previewSettings = dialog.settings;
    ReleasePackPreviewImage();
    dialog.previewWidth = 0;
    dialog.previewHeight = 0;
    dialog.previewError.clear();
    // The same cells as PackProject.
    std::optional<std::vector<PackCell>> const cells = ProjectPackCells(dialog.previewError);
    if (!cells.has_value()) {
        return;
    }
    std::optional<PackedSheet> const packed = PackCells(*cells, dialog.settings, dialog.previewImages, dialog.previewError);
    if (!packed.has_value()) {
        return;
    }
    std::shared_ptr<moth::gfx::ITexture> texture(
        m_assetContext.TextureFromPixels(packed->width, packed->height, packed->rgba.data()));
    if (!texture) {
        dialog.previewError = "Could not create the preview image.";
        return;
    }
    dialog.previewImage = moth::gfx::Image{ texture };
    dialog.previewWidth = packed->width;
    dialog.previewHeight = packed->height;
}

void SpriteEditor::ReleasePackPreviewImage() {
    auto& dialog = m_packDialog;
    if (!dialog.previewImage) {
        return;
    }
    // The swapchain keeps a submitted frame per image, and those frames may still sample the preview.
    if (m_waitForGpu) {
        m_waitForGpu();
    }
    dialog.previewImage = moth::gfx::Image{};
}

void SpriteEditor::DrawPackPreview() {
    auto const& dialog = m_packDialog;
    ImGui::BeginGroup();
    if (dialog.previewImage) {
        ImGui::Text("Packed image: %d x %d", dialog.previewWidth, dialog.previewHeight);
    } else {
        ImGui::TextUnformatted("Packed image");
    }
    ImVec2 const areaPos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2{ kPackPreviewSize, kPackPreviewSize });
    if (dialog.previewImage && dialog.previewWidth > 0 && dialog.previewHeight > 0) {
        // Fit the image in the area, centred.
        auto const w = static_cast<float>(dialog.previewWidth);
        auto const h = static_cast<float>(dialog.previewHeight);
        float const scale = std::min(kPackPreviewSize / w, kPackPreviewSize / h);
        float const drawW = std::max(std::floor(w * scale), 1.0f);
        float const drawH = std::max(std::floor(h * scale), 1.0f);
        ImVec2 const imagePos{ areaPos.x + std::floor((kPackPreviewSize - drawW) * 0.5f),
                               areaPos.y + std::floor((kPackPreviewSize - drawH) * 0.5f) };
        DrawImageBackground({ imagePos.x, imagePos.y }, { drawW, drawH }, kPackPreviewCheckerSize);
        ImVec2 const cursor = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(imagePos);
        DrawImage(dialog.previewImage, { static_cast<int>(drawW), static_cast<int>(drawH) });
        ImGui::SetCursorScreenPos(cursor);
    } else {
        ImGui::GetWindowDrawList()->AddRect(areaPos, ImVec2{ areaPos.x + kPackPreviewSize, areaPos.y + kPackPreviewSize },
                                            ImGui::GetColorU32(ImGuiCol_Border));
        ImGui::SetCursorScreenPos(ImVec2{ areaPos.x + ImGui::GetStyle().FramePadding.x, areaPos.y + ImGui::GetStyle().FramePadding.y });
        ImGui::PushTextWrapPos(areaPos.x + kPackPreviewSize - ImGui::GetStyle().FramePadding.x - ImGui::GetWindowPos().x);
        ImGui::TextColored(kPackWarningColor, "%s", dialog.previewError.c_str());
        ImGui::PopTextWrapPos();
        ImGui::SetCursorScreenPos(ImVec2{ areaPos.x, areaPos.y + kPackPreviewSize + ImGui::GetStyle().ItemSpacing.y });
    }
    ImGui::EndGroup();
}

bool SpriteEditor::PackProject(PackSettings const& settings, std::string& error) {
    std::optional<std::vector<PackCell>> const cells = ProjectPackCells(error);
    if (!cells.has_value()) {
        moth::core::log::error("SpriteEditor: pack failed: {}", error);
        return false;
    }
    PackImageCache imageCache;
    std::optional<PackedSheet> const packed = PackCells(*cells, settings, imageCache, error);
    if (!packed.has_value()) {
        moth::core::log::error("SpriteEditor: pack failed: {}", error);
        return false;
    }
    if (!WritePackedSheet(settings.imagePath, *packed, settings, error)) {
        moth::core::log::error("SpriteEditor: pack failed: {}", error);
        return false;
    }
    std::shared_ptr<moth::gfx::ITexture> texture(m_assetContext.TextureFromFile(settings.imagePath));
    if (!texture) {
        error = fmt::format("Could not load the packed image '{}'.", settings.imagePath);
        moth::core::log::error("SpriteEditor: pack failed: {}", error);
        return false;
    }

    // The sheet image, the cell rectangles and sources, and the pack settings change as one undoable action. Cell order,
    // sizes, pivots and clips stay. Each side keeps its sheet, and so its texture, alive. The packed file stays on disk.
    struct PackState {
        std::shared_ptr<moth::gfx::SpriteSheet> sheet;
        std::string imagePath;
        FrameVec frames;
        std::optional<PackSettings> settings;
    };
    auto const apply = [this](PackState const& state) {
        m_spriteSheet = state.sheet;
        strncpy(m_imagePathBuffer, state.imagePath.c_str(), sizeof(m_imagePathBuffer) - 1);
        m_imagePathBuffer[sizeof(m_imagePathBuffer) - 1] = '\0';
        m_frames = state.frames;
        m_packSettings = state.settings;
        m_zoom = -1.0f; // fit the new sheet
        m_cellZoom = -1.0f;
    };
    PackState before{ m_spriteSheet, m_imagePathBuffer, m_frames, m_packSettings };
    PackState after{ nullptr, settings.imagePath, m_frames, settings };
    for (size_t i = 0; i < after.frames.size(); ++i) {
        // Cells from other images become sheet cells. Undo restores their sources.
        after.frames[i].rect = packed->rects[i];
        after.frames[i].source.reset();
    }
    after.sheet = std::make_shared<moth::gfx::SpriteSheet>(moth::gfx::Image{ texture }, ToFrameEntries(after.frames), m_clips);
    apply(after);
    AddSpriteAction(std::make_unique<BasicAction>(
        [apply, after]()  { apply(after); },
        [apply, before]() { apply(before); }
    ));

    // As for Import Spritesheet.
    m_selection.clear();
    m_selectedClip    = -1;
    m_clipPlaying     = false;
    m_clipCurrentStep = 0;
    m_clipElapsedMs   = 0.0f;
    moth::core::log::info("SpriteEditor: packed {} cells into '{}' ({} x {})", m_frames.size(), settings.imagePath,
                          packed->width, packed->height);
    return true;
}

std::string SpriteEditor::UnpackBaseName() const {
    if (m_imagePathBuffer[0] != '\0') {
        std::string name = std::filesystem::path(m_imagePathBuffer).stem().string();
        if (!name.empty()) {
            return name;
        }
    }
    if (m_pathBuffer[0] != '\0') {
        std::string name = std::filesystem::path(m_pathBuffer).stem().string();
        if (!name.empty()) {
            return name;
        }
    }
    return "cells";
}

std::vector<std::filesystem::path> SpriteEditor::UnpackPaths(std::filesystem::path const& folder,
                                                             moth::packer::AtlasFormat format) const {
    // Three digits, as in hero_000.png, and more for a project with more than 1000 cells, so the names keep the
    // cell order.
    size_t const count = m_frames.size();
    int digits = 3;
    for (size_t limit = 1000; count > limit; limit *= 10) {
        ++digits;
    }
    std::string const base = UnpackBaseName();
    char const* const extension = PackFormatExtension(format);
    std::vector<std::filesystem::path> paths;
    paths.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        paths.push_back(folder / fmt::format("{}_{:0{}}{}", base, i, digits, extension));
    }
    return paths;
}

bool SpriteEditor::UnpackProject(std::filesystem::path const& folder, std::string& error) {
    // The same cells as a pack, so a cell with no image to read is refused in the same way.
    std::optional<std::vector<PackCell>> const cells = ProjectPackCells(error);
    if (!cells.has_value()) {
        return false;
    }
    if (cells->empty()) {
        error = "The project has no cells.";
        return false;
    }
    // Every cell is checked before any file is written, so a project that cannot be unpacked writes nothing.
    for (size_t i = 0; i < cells->size(); ++i) {
        auto const& rect = (*cells)[i].rect;
        if (rect.w() <= 0 || rect.h() <= 0) {
            error = fmt::format("Cell #{} has a size of {} x {}.", i, rect.w(), rect.h());
            return false;
        }
    }

    auto const& dialog = m_unpackDialog;
    std::vector<std::filesystem::path> const paths = UnpackPaths(folder, dialog.format);
    PackImageCache imageCache;
    for (size_t i = 0; i < cells->size(); ++i) {
        auto const& cell = (*cells)[i];
        auto found = imageCache.find(cell.imagePath);
        if (found == imageCache.end()) {
            std::optional<ImagePixels> pixels = LoadImagePixels(cell.imagePath);
            if (!pixels.has_value()) {
                error = fmt::format("Could not read the image '{}'.", cell.imagePath.string());
                return false;
            }
            found = imageCache.emplace(cell.imagePath, std::move(*pixels)).first;
        }
        ImagePixels const cellImage = CellPixels(cell, found->second);
        if (!WriteImageFile(paths[i], cellImage, dialog.format, dialog.jpegQuality, error)) {
            return false;
        }
    }
    moth::core::log::info("SpriteEditor: unpacked {} cells into '{}'", cells->size(), folder.string());
    return true;
}

void SpriteEditor::DrawUnpackDialog() {
    auto& dialog = m_unpackDialog;
    if (m_openUnpackDialog) {
        m_openUnpackDialog = false;
        // The first opening starts in the sheet image's folder, else the project's, else the last image folder.
        if (!dialog.folderChosen) {
            std::error_code ec;
            std::filesystem::path folder;
            if (m_imagePathBuffer[0] != '\0') {
                folder = std::filesystem::path(m_imagePathBuffer).parent_path();
            } else if (m_pathBuffer[0] != '\0') {
                folder = std::filesystem::path(m_pathBuffer).parent_path();
            }
            if (!std::filesystem::is_directory(folder, ec)) {
                folder = (!m_config.LastImageDir.empty() && std::filesystem::is_directory(m_config.LastImageDir, ec))
                             ? std::filesystem::path(m_config.LastImageDir)
                             : std::filesystem::current_path(ec);
            }
            std::string const folderStr = folder.string();
            strncpy(dialog.folderBuffer, folderStr.c_str(), sizeof(dialog.folderBuffer) - 1);
            dialog.folderBuffer[sizeof(dialog.folderBuffer) - 1] = '\0';
            dialog.folderChosen = true;
        }
        dialog.error.clear();
        ImGui::OpenPopup(kUnpackPopupId);
    }
    ImGuiViewport const* const viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2{ 0.5f, 0.5f });
    if (!ImGui::BeginPopupModal(kUnpackPopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        return;
    }

    // Output folder, with a browse button.
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Output folder");
    ImGui::SameLine(kPackLabelWidth);
    float const browseW = ImGui::CalcTextSize("...").x + (ImGui::GetStyle().FramePadding.x * 2.0f);
    ImGui::SetNextItemWidth(kPackFieldWidth - browseW - ImGui::GetStyle().ItemSpacing.x);
    if (ImGui::InputText("##unpack_folder", dialog.folderBuffer, sizeof(dialog.folderBuffer))) {
        dialog.error.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("...##unpack_browse")) {
        std::error_code ec;
        std::filesystem::path const current = dialog.folderBuffer;
        std::string const startDir = std::filesystem::is_directory(current, ec)
                                         ? current.string()
                                         : std::filesystem::current_path(ec).string();
        nfdchar_t* outPath = nullptr;
        if (NFD_PickFolder(startDir.c_str(), &outPath) == NFD_OKAY && outPath != nullptr) {
            std::string const chosen = outPath;
            NFD_Free(outPath);
            strncpy(dialog.folderBuffer, chosen.c_str(), sizeof(dialog.folderBuffer) - 1);
            dialog.folderBuffer[sizeof(dialog.folderBuffer) - 1] = '\0';
            dialog.error.clear();
        }
    }

    static constexpr std::array<char const*, 4> kFormatNames{ "PNG", "BMP", "TGA", "JPEG" };
    int format = static_cast<int>(dialog.format);
    PackLabel("Format");
    if (ImGui::Combo("##unpack_format", &format, kFormatNames.data(), static_cast<int>(kFormatNames.size()))) {
        dialog.format = static_cast<moth::packer::AtlasFormat>(format);
        dialog.error.clear();
    }
    if (dialog.format == moth::packer::AtlasFormat::JPEG) {
        PackLabel("JPEG quality");
        ImGui::SliderInt("##unpack_jpeg_quality", &dialog.jpegQuality, 1, 100);
        dialog.jpegQuality = std::clamp(dialog.jpegQuality, 1, 100);
    }

    // The files that would be written, and how many of them are already there.
    std::filesystem::path const folder = dialog.folderBuffer;
    std::vector<std::filesystem::path> const paths = UnpackPaths(folder, dialog.format);
    std::error_code ec;
    std::string refusal;
    if (folder.empty()) {
        refusal = "Choose the folder to write the cell images to.";
    } else if (!std::filesystem::is_directory(folder, ec)) {
        refusal = "That folder does not exist.";
    } else if (paths.empty()) {
        refusal = "The project has no cells.";
    }
    size_t existing = 0;
    if (refusal.empty()) {
        for (auto const& path : paths) {
            ec.clear();
            if (std::filesystem::exists(path, ec)) {
                ++existing;
            }
        }
    }

    ImGui::Spacing();
    if (!paths.empty()) {
        ImGui::Text("%zu cells, written as %s to %s", paths.size(), paths.front().filename().string().c_str(),
                    paths.back().filename().string().c_str());
    }
    // A refusal, else the last Unpack error, else the overwrite warning.
    std::string warning;
    if (existing > 0) {
        warning = fmt::format("{} file{} in this folder will be overwritten.", existing, existing == 1 ? "" : "s");
    }
    std::string const* message = &warning;
    if (!refusal.empty()) {
        message = &refusal;
    } else if (!dialog.error.empty()) {
        message = &dialog.error;
    }
    if (!message->empty()) {
        ImGui::PushTextWrapPos(kPackLabelWidth + kPackFieldWidth);
        ImGui::TextColored(kPackWarningColor, "%s", message->c_str());
        ImGui::PopTextWrapPos();
    }

    ImGui::Spacing();
    constexpr float kButtonW = 110.0f;
    bool close = false;
    ImGui::BeginDisabled(!refusal.empty());
    if (ImGui::Button("Unpack", ImVec2{ kButtonW, 0.0f })) {
        dialog.error.clear();
        close = UnpackProject(folder, dialog.error);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2{ kButtonW, 0.0f }) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        close = true;
    }
    if (close) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
