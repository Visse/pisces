#pragma once

#include "Fwd.h"

namespace Pisces
{
    namespace BuiltinTextures
    {
    
        struct BuiltinTexture {
            int width, height;
            PixelFormat format;
            const void *pixels;
        };

        extern BuiltinTexture MissingTexture;
    }
}