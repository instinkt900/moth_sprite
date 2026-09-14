#include "common.h"
#include "frame_detection.h"

// moth_graphics links its own copy of stb_image. STB_IMAGE_STATIC keeps this
// copy's symbols local to this translation unit so the two don't collide.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace {
    constexpr size_t kChannels = 4;
    using Rgb = std::array<uint8_t, 3>;

    size_t PixelIndex(int width, int x, int y) {
        return (static_cast<size_t>(y) * static_cast<size_t>(width)) + static_cast<size_t>(x);
    }

    // Average of the four corner pixels, or nullopt when any channel spreads by more than threshold.
    std::optional<Rgb> SampleCornerBackground(ImagePixels const& image, int threshold) {
        int const w = image.width;
        int const h = image.height;
        std::array<std::pair<int, int>, 4> const corners{ { { 0, 0 }, { w - 1, 0 }, { 0, h - 1 }, { w - 1, h - 1 } } };

        std::array<int, 3> lo{ 255, 255, 255 };
        std::array<int, 3> hi{ 0, 0, 0 };
        std::array<int, 3> sum{ 0, 0, 0 };
        for (auto const& [x, y] : corners) {
            size_t const base = PixelIndex(w, x, y) * kChannels;
            for (size_t ch = 0; ch < 3; ++ch) {
                int const v = image.rgba[base + ch];
                lo[ch] = std::min(lo[ch], v);
                hi[ch] = std::max(hi[ch], v);
                sum[ch] += v;
            }
        }

        Rgb avg{};
        for (size_t ch = 0; ch < 3; ++ch) {
            if (hi[ch] - lo[ch] > threshold) {
                return std::nullopt;
            }
            avg[ch] = static_cast<uint8_t>(sum[ch] / 4);
        }
        return avg;
    }

    // One axis of a box dilation: dst is set wherever src has a set pixel within
    // [i - after, i + before] along that axis. Prefix sums keep it O(pixels) for any window.
    void DilateAxis(std::vector<uint8_t> const& src, std::vector<uint8_t>& dst, int width, int height,
                    int before, int after, bool horizontal) {
        int const lineCount  = horizontal ? height : width;
        int const lineLength = horizontal ? width : height;
        auto const index = [&](int line, int i) {
            return horizontal ? PixelIndex(width, i, line) : PixelIndex(width, line, i);
        };

        std::vector<int> prefix(static_cast<size_t>(lineLength) + 1, 0);
        for (int line = 0; line < lineCount; ++line) {
            for (int i = 0; i < lineLength; ++i) {
                prefix[static_cast<size_t>(i) + 1] = prefix[static_cast<size_t>(i)] + (src[index(line, i)] != 0 ? 1 : 0);
            }
            for (int i = 0; i < lineLength; ++i) {
                int const lo = std::max(i - after, 0);
                int const hi = std::min(i + before, lineLength - 1);
                dst[index(line, i)] = (prefix[static_cast<size_t>(hi) + 1] - prefix[static_cast<size_t>(lo)]) > 0 ? 1 : 0;
            }
        }
    }

    // Order rects into visual rows (bucketed by centre-y using half the median height),
    // each row left to right.
    void SortRowMajor(std::vector<moth::gfx::IntRect>& rects) {
        if (rects.size() <= 1) {
            return;
        }

        std::vector<int> heights;
        heights.reserve(rects.size());
        for (auto const& r : rects) {
            heights.push_back(r.h());
        }
        auto const mid = heights.begin() + static_cast<ptrdiff_t>(heights.size() / 2);
        std::nth_element(heights.begin(), mid, heights.end());
        int const rowThreshold = std::max(1, *mid / 2);

        auto const centreY = [](moth::gfx::IntRect const& r) { return r.y() + (r.h() / 2); };
        std::sort(rects.begin(), rects.end(), [&](auto const& a, auto const& b) { return centreY(a) < centreY(b); });

        std::vector<std::vector<moth::gfx::IntRect>> rows;
        int baseline = centreY(rects[0]);
        rows.push_back({ rects[0] });
        for (size_t i = 1; i < rects.size(); ++i) {
            int const cy = centreY(rects[i]);
            if (std::abs(cy - baseline) > rowThreshold) {
                baseline = cy;
                rows.push_back({ rects[i] });
            } else {
                rows.back().push_back(rects[i]);
            }
        }

        rects.clear();
        for (auto& row : rows) {
            std::sort(row.begin(), row.end(), [](auto const& a, auto const& b) { return a.x() < b.x(); });
            rects.insert(rects.end(), row.begin(), row.end());
        }
    }
} // namespace

std::optional<ImagePixels> LoadImagePixels(std::filesystem::path const& path) {
    int width = 0;
    int height = 0;
    int srcChannels = 0;
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> const data(
        stbi_load(path.string().c_str(), &width, &height, &srcChannels, static_cast<int>(kChannels)),
        stbi_image_free);
    if (data == nullptr) {
        return std::nullopt;
    }

    ImagePixels image;
    image.width  = width;
    image.height = height;
    size_t const byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * kChannels;
    image.rgba.assign(data.get(), data.get() + byteCount);
    return image;
}

FrameDetectResult DetectFrames(ImagePixels const& image, FrameDetectOptions const& options) {
    FrameDetectResult result;
    int const width  = image.width;
    int const height = image.height;
    size_t const pixelCount = static_cast<size_t>(std::max(width, 0)) * static_cast<size_t>(std::max(height, 0));
    if (pixelCount == 0 || image.rgba.size() < pixelCount * kChannels) {
        return result;
    }

    switch (options.mode) {
    case BackgroundMode::Alpha:
        break;
    case BackgroundMode::CornerColor:
        result.backgroundColor = SampleCornerBackground(image, options.colorThreshold);
        break;
    case BackgroundMode::Color:
        result.backgroundColor = options.backgroundColor;
        break;
    }

    // Classify every pixel up front.
    std::vector<uint8_t> foreground(pixelCount, 0);
    auto const& px = image.rgba;
    for (size_t i = 0; i < pixelCount; ++i) {
        size_t const base = i * kChannels;
        bool active = false;
        if (result.backgroundColor.has_value()) {
            auto const& bg = *result.backgroundColor;
            active = std::abs(int{ px[base + 0] } - int{ bg[0] }) > options.colorThreshold ||
                     std::abs(int{ px[base + 1] } - int{ bg[1] }) > options.colorThreshold ||
                     std::abs(int{ px[base + 2] } - int{ bg[2] }) > options.colorThreshold;
        } else {
            active = int{ px[base + 3] } > options.alphaThreshold;
        }
        foreground[i] = active ? 1 : 0;
    }

    // The flood fill walks `reachable`, clearing pixels as regions claim them. With a merge gap
    // it is the foreground dilated so that regions separated by <= mergeGap background pixels
    // touch; bounds still only grow over real foreground pixels, so rects stay tight.
    std::vector<uint8_t> reachable = foreground;
    if (options.mergeGap > 0) {
        // The asymmetric window [-floor(gap/2), +ceil(gap/2)] makes two pixels' dilations
        // touch exactly when their Chebyshev distance is <= gap + 1.
        int const before = options.mergeGap / 2;
        int const after  = options.mergeGap - before;
        std::vector<uint8_t> horizontal(pixelCount, 0);
        DilateAxis(foreground, horizontal, width, height, before, after, true);
        DilateAxis(horizontal, reachable, width, height, before, after, false);
    }

    auto const outsideSizeFilter = [&options](int w, int h) {
        return (options.minWidth  > 0 && w < options.minWidth)  ||
               (options.minHeight > 0 && h < options.minHeight) ||
               (options.maxWidth  > 0 && w > options.maxWidth)  ||
               (options.maxHeight > 0 && h > options.maxHeight);
    };

    // Scan top-to-bottom, left-to-right. Each unclaimed foreground pixel seeds a flood fill
    // (8-connectivity) that claims its whole region while tracking the bounding box.
    std::vector<size_t> pending;
    for (int sy = 0; sy < height; ++sy) {
        for (int sx = 0; sx < width; ++sx) {
            size_t const seed = PixelIndex(width, sx, sy);
            if (foreground[seed] == 0 || reachable[seed] == 0) {
                continue;
            }
            reachable[seed] = 0;
            pending.push_back(seed);

            int minX = sx;
            int maxX = sx;
            int minY = sy;
            int maxY = sy;
            while (!pending.empty()) {
                size_t const idx = pending.back();
                pending.pop_back();
                int const cx = static_cast<int>(idx % static_cast<size_t>(width));
                int const cy = static_cast<int>(idx / static_cast<size_t>(width));
                if (foreground[idx] != 0) {
                    minX = std::min(minX, cx);
                    maxX = std::max(maxX, cx);
                    minY = std::min(minY, cy);
                    maxY = std::max(maxY, cy);
                }

                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        int const nx = cx + dx;
                        int const ny = cy + dy;
                        if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                            continue;
                        }
                        size_t const nidx = PixelIndex(width, nx, ny);
                        if (reachable[nidx] == 0) {
                            continue;
                        }
                        reachable[nidx] = 0;
                        pending.push_back(nidx);
                    }
                }
            }

            if (outsideSizeFilter(maxX - minX + 1, maxY - minY + 1)) {
                ++result.filteredCount;
                continue;
            }
            int const x0 = std::max(minX - options.paddingX, 0);
            int const y0 = std::max(minY - options.paddingY, 0);
            int const x1 = std::min(maxX + 1 + options.paddingX, width);
            int const y1 = std::min(maxY + 1 + options.paddingY, height);
            result.rects.push_back(moth::gfx::MakeRect(x0, y0, x1 - x0, y1 - y0));
        }
    }

    SortRowMajor(result.rects);
    return result;
}
