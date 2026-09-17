#include "common.h"
#include "sprite_editor.h"
#include "sprite_editor_config.h"

#include <nfd.h>

namespace {
    char const* const kPackPopupId = "Pack##pack_dialog";
    constexpr float kPackLabelWidth = 130.0f;
    constexpr float kPackFieldWidth = 360.0f;
    constexpr int kMaxPackPadding = 1024;
    constexpr ImVec4 kPackWarningColor{ 1.0f, 0.45f, 0.35f, 1.0f };

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
        ImGui::OpenPopup(kPackPopupId);
    }
    ImGuiViewport const* const viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2{ 0.5f, 0.5f });
    if (!ImGui::BeginPopupModal(kPackPopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        return;
    }
    auto& settings = dialog.settings;

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
    }
    std::string const& message = refusal.empty() ? dialog.error : refusal;
    if (!message.empty()) {
        ImGui::Spacing();
        ImGui::PushTextWrapPos(kPackLabelWidth + kPackFieldWidth);
        ImGui::TextColored(kPackWarningColor, "%s", message.c_str());
        ImGui::PopTextWrapPos();
    }

    ImGui::Spacing();
    constexpr float kButtonW = 110.0f;
    bool close = false;
    ImGui::BeginDisabled(!refusal.empty());
    if (ImGui::Button("Pack", ImVec2{ kButtonW, 0.0f })) {
        std::error_code ec;
        std::filesystem::path absolutePath = std::filesystem::absolute(packPath, ec);
        settings.imagePath = (ec ? packPath : absolutePath).lexically_normal().string();
        dialog.error.clear();
        close = PackProject(settings, dialog.error);
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

bool SpriteEditor::PackProject(PackSettings const& settings, std::string& error) {
    if (m_imagePathBuffer[0] == '\0') {
        error = "The project has no sheet image.";
        return false;
    }
    std::vector<PackCell> cells;
    cells.reserve(m_frames.size());
    for (auto const& frame : m_frames) {
        cells.push_back({ m_imagePathBuffer, frame.rect });
    }
    PackImageCache imageCache;
    std::optional<PackedSheet> const packed = PackCells(cells, settings, imageCache, error);
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

    // The sheet image, the cell rectangles and the pack settings change as one undoable action. Cell order, sizes,
    // pivots and clips stay. Each side keeps its sheet, and so its texture, alive. The packed file stays on disk.
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
        after.frames[i].rect = packed->rects[i];
    }
    after.sheet = std::make_shared<moth::gfx::SpriteSheet>(moth::gfx::Image{ texture }, after.frames, m_clips);
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
