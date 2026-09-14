#pragma once

#include <moth/graphics/utils/rect.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

// Decoded RGBA8 image held in CPU memory.
struct ImagePixels {
    int width  = 0;
    int height = 0;
    std::vector<uint8_t> rgba; // width * height * 4 bytes
};

// Decode an image file to RGBA8. Returns nullopt on failure.
std::optional<ImagePixels> LoadImagePixels(std::filesystem::path const& path);

// How a pixel is classified as background (not part of any frame).
enum class BackgroundMode {
    Alpha,       // alpha <= alphaThreshold is background
    CornerColor, // background color sampled from the four corners; falls back to Alpha if they disagree
    Color,       // explicit background color
};

struct FrameDetectOptions {
    BackgroundMode mode = BackgroundMode::Alpha;
    int alphaThreshold = 0;                                // 0-255
    std::array<uint8_t, 3> backgroundColor{ 255, 0, 255 }; // Color mode only
    int colorThreshold = 10;                               // per-channel tolerance, 0-255
    // Foreground pixels separated by at most this many background pixels join the same
    // region. Rects still bound only real foreground pixels. 0 = plain 8-connectivity.
    int mergeGap = 0;
    // Size filter in pixels, applied to the unpadded rect; 0 disables that bound.
    int minWidth  = 0;
    int minHeight = 0;
    int maxWidth  = 0;
    int maxHeight = 0;
    // Extra border added to each side of every kept rect, clamped to the image.
    int paddingX = 0;
    int paddingY = 0;
};

struct FrameDetectResult {
    std::vector<moth::gfx::IntRect> rects;                 // padded, in row-major order
    std::optional<std::array<uint8_t, 3>> backgroundColor; // color used; nullopt when alpha was used
    int filteredCount = 0;                                 // regions dropped by the size filter
};

// Find the bounding rect of every 8-connected region of non-background pixels.
// Ported from moth_packer's Unpack(), plus merge gap and padding.
FrameDetectResult DetectFrames(ImagePixels const& image, FrameDetectOptions const& options);
