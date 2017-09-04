#pragma once

#include "Fwd.h"

#include "Common/StringId.h"
#include "Common/PImplHelper.h"

namespace Pisces
{
    class SpriteManager {
    public:
        PISCES_API SpriteManager( Context *contet );
        PISCES_API ~SpriteManager();

        PISCES_API void setSprite( Common::StringId name, const Sprite &sprite );
        PISCES_API bool findSprite( Common::StringId name, Sprite &sprite );

    private:
        struct Impl;
        PImplHelper<Impl, 64> mImpl;
    };
}