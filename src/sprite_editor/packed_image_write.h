#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Formats for WritePackedImageFile. */
enum {
    PACKED_IMAGE_PNG = 0,
    PACKED_IMAGE_BMP = 1,
    PACKED_IMAGE_TGA = 2,
    PACKED_IMAGE_JPEG = 3,
};

/* Write width x height RGBA8 pixels to path. Returns 0 on failure. */
int WritePackedImageFile(char const* path, int format, int width, int height, unsigned char const* rgba,
                         int jpegQuality);

#ifdef __cplusplus
}
#endif
