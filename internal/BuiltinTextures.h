#pragma once

#include "Fwd.h"

namespace Pisces
{
    struct BuiltinTextureInfo {
        int width, height;
        PixelFormat format;
        const void *pixels;
    };

    extern const BuiltinTextureInfo BuiltinTextures[BUILTIN_TEXTURE_COUNT];
}