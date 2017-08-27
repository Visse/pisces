#pragma once

#include "Fwd.h"
#include "yaml-cpp/fwd.h"

#include "Common/Archive.h"

namespace Pisces
{
    class IResourceLoader {
    public:
        PISCES_API virtual ~IResourceLoader();
        PISCES_API virtual ResourceHandle loadFile( Common::Archive &archive, const std::string &filename );
        PISCES_API virtual ResourceHandle loadResource( Common::Archive &archive, YAML::Node node ) = 0;
    };
}