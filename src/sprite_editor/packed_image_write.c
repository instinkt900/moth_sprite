/* The stb_image_write implementation, behind one function for sheet_packing.cpp.
 *
 * moth_packer links its own copy of stb_image_write. STB_IMAGE_WRITE_STATIC keeps this
 * copy's symbols local to this translation unit so the two don't collide. It is compiled as C,
 * outside clang-tidy, because clang-analyzer reports a zero-size malloc inside the library's
 * PNG encoder that its callers cannot rule out. */
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "packed_image_write.h"

int WritePackedImageFile(char const* path, int format, int width, int height, unsigned char const* rgba,
                         int jpegQuality) {
    enum { kComponents = 4 };
    switch (format) {
    case PACKED_IMAGE_PNG:
        return stbi_write_png(path, width, height, kComponents, rgba, width * kComponents);
    case PACKED_IMAGE_BMP:
        return stbi_write_bmp(path, width, height, kComponents, rgba);
    case PACKED_IMAGE_TGA:
        return stbi_write_tga(path, width, height, kComponents, rgba);
    case PACKED_IMAGE_JPEG:
        return stbi_write_jpg(path, width, height, kComponents, rgba, jpegQuality);
    default:
        return 0;
    }
}
