#pragma once

#include "Fwd.h"
#include "build_config.h"
#include "IResourceLoader.h"

#include "Common/PImplHelper.h"
#include "Common/Archive.h"

#include <string>


namespace Pisces
{
    class TextureLoader :
        public IResourceLoader
    {
    public:
        PISCES_API TextureLoader( HardwareResourceManager *hardwareMgr );
        PISCES_API ~TextureLoader();
        
        PISCES_API virtual ResourceHandle loadResource( Common::Archive &archive, YAML::Node node ) override;

    protected:
        struct Impl;
        PImplHelper<Impl, 8> mImpl;
    };
}