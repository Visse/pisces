#include "IResourceLoader.h"
#include "Common/ErrorUtils.h"
#include "Common/MemStreamBuf.h"

#include "yaml-cpp/yaml.h"


namespace Pisces
{
    PISCES_API IResourceLoader::~IResourceLoader()
    {
    }

    PISCES_API ResourceHandle IResourceLoader::loadFile( Common::Archive &archive, const std::string &filename )
    {
#define LOAD_ERROR(error, ...) \
            LOG_ERROR("Failed to load resource from file \"%s\" in archive \"%s\" error: " error, filename.c_str(), archive.name(), __VA_ARGS__); \
            return ResourceHandle {};
#define LOAD_ERROR0(error) LOAD_ERROR(error, 0)
        try {
            auto fileHandle = archive.openFile(filename);
            if (!fileHandle) {
                LOAD_ERROR0("Failed to open file.");
                return ResourceHandle {};
            }

            Common::MemStreamBuf streambuf(archive.mapFile(fileHandle), archive.fileSize(fileHandle));
            std::istream stream(&streambuf);

            YAML::Node node = YAML::Load(stream);
            return loadResource(archive, node);
        }
        catch (const std::exception &e) {
            LOAD_ERROR("Uncaught exception \"%s\"", e.what());
        }
    }
}