#pragma once

#include "Fwd.h"
#include "IResourceLoader.h"

namespace Pisces
{
    class SpriteLoader :
        public IResourceLoader
    {
        SpriteManager *mSpriteMgr;
        HardwareResourceManager *mHardwareMgr;
    public:
        SpriteLoader( SpriteManager *spriteMgr, HardwareResourceManager *hardwareMgr ) :
            mSpriteMgr(spriteMgr),
            mHardwareMgr(hardwareMgr)
        {}

        PISCES_API virtual ResourceHandle loadResource( Common::Archive &archive, Common::YamlNode node ) override;
    };
}