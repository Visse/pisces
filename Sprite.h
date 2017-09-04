#pragma once

#include "Fwd.h"


namespace Pisces
{
    struct Sprite {
        TextureHandle texture;
        struct {
            float x1, y1,
                  x2, y2;
        } uv = {};
    };
}