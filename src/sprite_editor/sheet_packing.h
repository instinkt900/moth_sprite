#pragma once

#include "frame_detection.h"

#include <moth/graphics/utils/rect.h>
#include <moth/packer/packer.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

// File > Pack settings, saved in the project file after a successful pack.
struct PackSettings {
    std::string imagePath; // absolute path of the packed image
    int padding = 0;       // pixels around each cell
    moth::packer::PaddingType paddingType = moth::packer::PaddingType::Color;
    uint32_t paddingColor = 0; // RRGGBBAA; also the background of the packed image
    // Best pack: the smallest image the packer can find, from 1 x 1 up to kBestPackMaxSize. The sizes below are kept
    // but not used.
    bool bestPack = false;
    // Powers of two.
    int minWidth = 256;
    int minHeight = 256;
    int maxWidth = 4096;
    int maxHeight = 4096;
    moth::packer::AtlasFormat format = moth::packer::AtlasFormat::PNG;
    int jpegQuality = 90; // 1-100, JPEG only
};

// One cell to pack: a rectangle in a source image. Parts of the rectangle outside the image are transparent.
struct PackCell {
    std::filesystem::path imagePath;
    moth::gfx::IntRect rect;
};

// A packed image in memory, with each cell's rectangle in it, in the order the cells were given.
struct PackedSheet {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba; // width * height * 4 bytes
    std::vector<moth::gfx::IntRect> rects;
};

// Decoded source images by path, so that they are read once.
using PackImageCache = std::map<std::filesystem::path, ImagePixels>;

// The largest width and height that Best pack tries.
constexpr int kBestPackMaxSize = 16384;

// The settings with the sizes that packing uses: the Best pack range when bestPack is set.
PackSettings EffectivePackSettings(PackSettings settings);

// The file extension of a packed image format, with the dot.
char const* PackFormatExtension(moth::packer::AtlasFormat format);

// Pack every cell as its own image into one image. Returns nothing, and sets error to the reason, when a source
// image cannot be read or the cells do not fit into one image of the maximum size. Writes no files.
std::optional<PackedSheet> PackCells(std::vector<PackCell> const& cells, PackSettings const& requestedSettings,
                                     PackImageCache& imageCache, std::string& error);

// Write a packed image to path in the settings' format. Returns false, and sets error, when it cannot be written.
bool WritePackedSheet(std::filesystem::path const& path, PackedSheet const& sheet, PackSettings const& settings,
                      std::string& error);
