#pragma once

#include "Fwd.h"
#include "build_config.h"

#include "Common/PImplHelper.h"
#include "Common/Archive.h"

#include <string>


namespace Pisces
{
    class TextureLoader
    {
    public:
        PISCES_API TextureLoader( HardwareResourceManager *hardwareMgr );
        PISCES_API ~TextureLoader();
        
        PISCES_API TextureHandle loadFile( Common::Archive &archive, const std::string &filename );

    protected:
        struct Impl;
        PImplHelper<Impl, 8> mImpl;
    };
}