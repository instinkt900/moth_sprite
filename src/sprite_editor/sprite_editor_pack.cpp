#include "common.h"
#include "sprite_editor.h"
#include "sprite_editor_config.h"

#include <nfd.h>

#include <set>

namespace {
    char const* const kPackPopupId = "Pack##pack_dialog";
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
               a.minWidth == b.minWidth && a.minHeight == b.minHeight && a.maxWidth == b.maxWidth &&
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
        strncpy(dialog.pathBuffer, dialog.settings.imagePath.c_str(), sizeof(dialog.pathBuffer) - 1);
        dialog.pathBuffer[sizeof(dialog.pathBuffer) - 1] = '\0';
        dialog.error.clear();
        dialog.previewSettings.reset();
        ImGui::OpenPopup(kPackPopupId);
    }
    ImGuiViewport const* const viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2{ 0.5f, 0.5f });
    if (!ImGui::BeginPopupModal(kPackPopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        // The preview and its source images are held only while the dialog is open. The last frame that drew the
        // preview has been rendered, so its texture can go.
        dialog.previewImages.clear();
        dialog.previewSettings.reset();
        dialog.previewImage = moth::gfx::Image{};
        return;
    }
    auto& settings = dialog.settings;
    // Pack the preview before anything is drawn this frame, so a replaced texture is not used by this frame.
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

    // The minimum never goes above the maximum: changing one moves the other.
    if (PackSizeCombo("Min width", "##pack_min_w", settings.minWidth)) {
        settings.maxWidth = std::max(settings.maxWidth, settings.minWidth);
    }
    if (PackSizeCombo("Min height", "##pack_min_h", settings.minHeight)) {
        settings.maxHeight = std::max(settings.maxHeight, settings.minHeight);
    }
    if (PackSizeCombo("Max width", "##pack_max_w", settings.maxWidth)) {
        settings.minWidth = std::min(settings.minWidth, settings.maxWidth);
    }
    if (PackSizeCombo("Max height", "##pack_max_h", settings.maxHeight)) {
        settings.minHeight = std::min(settings.minHeight, settings.maxHeight);
    }

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

    // Refuse a path that would write over a source image: undo would point the old rectangles at new pixels.
    std::string refusal;
    std::filesystem::path const packPath = dialog.pathBuffer;
    if (packPath.empty()) {
        refusal = "Choose a path for the packed image.";
    } else if (m_imagePathBuffer[0] != '\0' && SameFile(packPath, m_imagePathBuffer)) {
        refusal = "The packed image cannot be written over the sheet image. Choose another path.";
    } else {
        // Each cell image file is checked once, however many cells use it.
        std::set<std::string> cellImagePaths;
        for (auto const& cell : m_frames) {
            if (cell.source) {
                cellImagePaths.insert(cell.source->path);
            }
        }
        if (std::any_of(cellImagePaths.begin(), cellImagePaths.end(),
                        [&packPath](std::string const& cellImagePath) { return SameFile(packPath, cellImagePath); })) {
            refusal = "The packed image cannot be written over the image of a cell. Choose another path.";
        }
    }
    std::string const& message = refusal.empty() ? dialog.error : refusal;
    if (!message.empty()) {
        ImGui::Spacing();
        ImGui::PushTextWrapPos(kPackLabelWidth + kPackFieldWidth);
        ImGui::TextColored(kPackWarningColor, "%s", message.c_str());
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
        std::optional<bool> const exportAfterPack = m_exportAfterPack;
        m_exportAfterPack.reset();
        if (packed && exportAfterPack.has_value()) {
            ExportProject(*exportAfterPack);
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
    dialog.previewImage = moth::gfx::Image{};
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

    // As for Import Sheet.
    m_selection.clear();
    m_selectedClip    = -1;
    m_clipPlaying     = false;
    m_clipCurrentStep = 0;
    m_clipElapsedMs   = 0.0f;
    moth::core::log::info("SpriteEditor: packed {} cells into '{}' ({} x {})", m_frames.size(), settings.imagePath,
                          packed->width, packed->height);
    return true;
}
