#include "SpriteManager.h"
#include "Sprite.h"

#include <unordered_map>
#include <algorithm>

#include "Common/FlatMap.h"


namespace Pisces
{
    struct SpriteManager::Impl {
        Context *context;
        Common::FlatMap<Common::StringId, Sprite> sprites;
    };

    PISCES_API SpriteManager::SpriteManager( Context *context )
    {
        mImpl->context = context;
    }

    PISCES_API SpriteManager::~SpriteManager()
    {
    }

    PISCES_API void SpriteManager::setSprite( Common::StringId name, const Sprite &sprite )
    {
        mImpl->sprites.insert_replace(name, sprite);
    }

    PISCES_API bool SpriteManager::findSprite( Common::StringId name, Sprite &sprite )
    {
        auto iter = mImpl->sprites.find(name);
        if (iter == mImpl->sprites.end()) return false;

        sprite = iter->second;
        return true;
    }
}
