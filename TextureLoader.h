#pragma once

#include "Fwd.h"
#include "Common/PImplHelper.h"
#include "Common/Archive.h"

#include <string>


namespace Pisces
{
    class TextureLoader
    {
    public:
        TextureLoader( HardwareResourceManager *hardwareMgr );
        ~TextureLoader();
        
        TextureHandle loadFile( Common::Archive &archive, const std::string &filename );

    protected:
        struct Impl;
        PImplHelper<Impl, 8> mImpl;
    };
}