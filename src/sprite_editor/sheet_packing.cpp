#include "common.h"
#include "sheet_packing.h"

#include "packed_image_write.h"

namespace {
    constexpr size_t kChannels = 4;

    // The format WritePackedImageFile takes.
    int PackedImageFormat(moth::packer::AtlasFormat format) {
        switch (format) {
        case moth::packer::AtlasFormat::BMP:  return PACKED_IMAGE_BMP;
        case moth::packer::AtlasFormat::TGA:  return PACKED_IMAGE_TGA;
        case moth::packer::AtlasFormat::JPEG: return PACKED_IMAGE_JPEG;
        case moth::packer::AtlasFormat::PNG:
        default:                              return PACKED_IMAGE_PNG;
        }
    }

    int NextPowerOfTwo(int value) {
        int result = 1;
        while (result < value) {
            result *= 2;
        }
        return result;
    }
} // namespace

PackSettings EffectivePackSettings(PackSettings settings) {
    if (settings.bestPack) {
        settings.minWidth = 1;
        settings.minHeight = 1;
        settings.maxWidth = kBestPackMaxSize;
        settings.maxHeight = kBestPackMaxSize;
    }
    return settings;
}

char const* PackFormatExtension(moth::packer::AtlasFormat format) {
    switch (format) {
    case moth::packer::AtlasFormat::BMP:  return ".bmp";
    case moth::packer::AtlasFormat::TGA:  return ".tga";
    case moth::packer::AtlasFormat::JPEG: return ".jpg";
    case moth::packer::AtlasFormat::PNG:
    default:                              return ".png";
    }
}

ImagePixels CellPixels(PackCell const& cell, ImagePixels const& source) {
    ImagePixels cellImage;
    cellImage.width = cell.rect.w();
    cellImage.height = cell.rect.h();
    cellImage.rgba.assign(static_cast<size_t>(cellImage.width) * static_cast<size_t>(cellImage.height) * kChannels, 0);
    int const x0 = std::max(cell.rect.x(), 0);
    int const y0 = std::max(cell.rect.y(), 0);
    int const x1 = std::min(cell.rect.x() + cellImage.width, source.width);
    int const y1 = std::min(cell.rect.y() + cellImage.height, source.height);
    for (int y = y0; y < y1 && x0 < x1; ++y) {
        size_t const srcOffset = ((static_cast<size_t>(y) * static_cast<size_t>(source.width)) + static_cast<size_t>(x0)) * kChannels;
        size_t const dstOffset = ((static_cast<size_t>(y - cell.rect.y()) * static_cast<size_t>(cellImage.width)) +
                                  static_cast<size_t>(x0 - cell.rect.x())) * kChannels;
        std::memcpy(&cellImage.rgba[dstOffset], &source.rgba[srcOffset], static_cast<size_t>(x1 - x0) * kChannels);
    }
    return cellImage;
}

bool WriteImageFile(std::filesystem::path const& path, ImagePixels const& image, moth::packer::AtlasFormat format,
                    int jpegQuality, std::string& error) {
    std::string const pathStr = path.string();
    if (image.width <= 0 || image.height <= 0 ||
        image.rgba.size() != static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * kChannels) {
        error = fmt::format("Could not write the image '{}': the image is empty.", pathStr);
        return false;
    }
    int const written = WritePackedImageFile(pathStr.c_str(), PackedImageFormat(format), image.width, image.height,
                                             image.rgba.data(), jpegQuality);
    if (written == 0) {
        error = fmt::format("Could not write the image '{}'.", pathStr);
        return false;
    }
    return true;
}

std::optional<PackedSheet> PackCells(std::vector<PackCell> const& cells, PackSettings const& requestedSettings,
                                     PackImageCache& imageCache, std::string& error) {
    PackSettings const settings = EffectivePackSettings(requestedSettings);
    if (cells.empty()) {
        error = "The project has no cells.";
        return std::nullopt;
    }
    // The packer rounds the maximum size up to a power of two.
    int const maxWidth = NextPowerOfTwo(settings.maxWidth);
    int const maxHeight = NextPowerOfTwo(settings.maxHeight);

    std::vector<moth::packer::ImageInput> inputs;
    inputs.reserve(cells.size());
    for (size_t i = 0; i < cells.size(); ++i) {
        auto const& cell = cells[i];
        int const w = cell.rect.w();
        int const h = cell.rect.h();
        if (w <= 0 || h <= 0) {
            error = fmt::format("Cell #{} has a size of {} x {}.", i, w, h);
            return std::nullopt;
        }
        if (w + (settings.padding * 2) > maxWidth || h + (settings.padding * 2) > maxHeight) {
            error = fmt::format("Cell #{} ({} x {} with padding {}) is larger than the maximum size {} x {}.", i, w, h,
                                settings.padding, maxWidth, maxHeight);
            return std::nullopt;
        }

        auto found = imageCache.find(cell.imagePath);
        if (found == imageCache.end()) {
            std::optional<ImagePixels> pixels = LoadImagePixels(cell.imagePath);
            if (!pixels.has_value()) {
                error = fmt::format("Could not read the image '{}'.", cell.imagePath.string());
                return std::nullopt;
            }
            found = imageCache.emplace(cell.imagePath, std::move(*pixels)).first;
        }
        ImagePixels const& source = found->second;

        // Flipbook packing sorts images by name, so zero-padded indices keep the cells in order.
        ImagePixels cellImage = CellPixels(cell, source);
        moth::packer::ImageInput input;
        input.name = fmt::format("{:010}", i);
        input.width = w;
        input.height = h;
        input.pixels = std::move(cellImage.rgba);
        inputs.push_back(std::move(input));
    }

    moth::packer::PackOptions options;
    options.packType = moth::packer::PackType::Flipbook;
    options.padding = settings.padding;
    options.paddingType = settings.paddingType;
    options.paddingColor = settings.paddingColor;
    options.minWidth = settings.minWidth;
    options.minHeight = settings.minHeight;
    options.maxWidth = settings.maxWidth;
    options.maxHeight = settings.maxHeight;
    options.format = settings.format;
    options.jpegQuality = settings.jpegQuality;
    moth::packer::PackResult result = moth::packer::PackToMemory(std::move(inputs), options);
    if (!result.ok || result.atlases.size() != 1 || result.frames.size() != cells.size()) {
        error = fmt::format("The cells do not fit into one image of {} x {}.", maxWidth, maxHeight);
        return std::nullopt;
    }

    PackedSheet sheet;
    sheet.width = result.atlases[0].width;
    sheet.height = result.atlases[0].height;
    sheet.rgba = std::move(result.atlases[0].pixels);
    sheet.rects.reserve(result.frames.size());
    for (auto const& frame : result.frames) {
        sheet.rects.push_back(frame.rect);
    }
    return sheet;
}

bool WritePackedSheet(std::filesystem::path const& path, PackedSheet const& sheet, PackSettings const& settings,
                      std::string& error) {
    constexpr int kComponents = 4;
    std::string const pathStr = path.string();
    if (sheet.width <= 0 || sheet.height <= 0 ||
        sheet.rgba.size() != static_cast<size_t>(sheet.width) * static_cast<size_t>(sheet.height) * kComponents) {
        error = fmt::format("Could not write the packed image '{}': the image is empty.", pathStr);
        return false;
    }
    int const written = WritePackedImageFile(pathStr.c_str(), PackedImageFormat(settings.format), sheet.width,
                                             sheet.height, sheet.rgba.data(), settings.jpegQuality);
    if (written == 0) {
        error = fmt::format("Could not write the packed image '{}'.", pathStr);
        return false;
    }
    return true;
}
