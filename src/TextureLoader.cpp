#include "TextureLoader.h"

#include "HardwareResourceManager.h"

#include "Common/FileUtils.h"
#include "Common/Throw.h"
#include "Common/BuiltinFromString.h"
#include "Common/MemStreamBuf.h"
#include "Common/BuiltinFromString.h"
#include "Common/ErrorUtils.h"
#include "Common/BuiltinCast.h"

#include "stb_image.h"
WRAP_HANDLE_FUNC(stbi_image, stbi_uc*, stbi_image_free, nullptr);

#include <yaml-cpp/yaml.h>

namespace Pisces {
    struct TextureLoader::Impl 
    {
        HardwareResourceManager *hardwareMgr;
    };

    PISCES_API TextureLoader::TextureLoader( HardwareResourceManager *hardwareMgr )
    {
        mImpl->hardwareMgr = hardwareMgr;
    }

    PISCES_API TextureLoader::~TextureLoader()
    {
    }
    
    PISCES_API ResourceHandle TextureLoader::loadResource( Common::Archive &archive, YAML::Node node )
    {

#define LOAD_ERROR(error, ...) \
            LOG_ERROR("Failed to load texture in archive \"%s\" error: " error, archive.name(), __VA_ARGS__); \
            return ResourceHandle {};
#define LOAD_ERROR0(error) LOAD_ERROR(error, 0)
        
        try {
            YAML::Node textureTypeNode = node["TextureType"];
            if (!textureTypeNode || textureTypeNode.IsScalar() == false) {
                LOAD_ERROR0("Missing attribute \"TextureType\"");
            }
            const std::string &textureType = textureTypeNode.Scalar();

            if (textureType == "Texture2D") {
                YAML::Node pixelFormatNode = node["PixelFormat"];
                if (!pixelFormatNode || pixelFormatNode.IsScalar() == false) {
                    LOAD_ERROR0("Missing attribute \"PixelFormat\"");
                }

                const std::string &pixelFormatStr = pixelFormatNode.Scalar();

                PixelFormat pixelFormat;
                if (!PixelFormatFromString(pixelFormatStr.c_str(), pixelFormatStr.size(), pixelFormat)) {
                    LOAD_ERROR("Unkown PixelFormat \"%s\"", pixelFormatStr.c_str());
                }

                YAML::Node fileNode = node["File"];
                if (!fileNode || fileNode.IsScalar() == false) {
                    LOAD_ERROR0("Missing attribute \"File\"");
                }

                int mipmaps = 0;
                YAML::Node mipmapsNode = node["Mipmaps"];
                if (mipmapsNode) {
                    mipmaps = mipmapsNode.as<int>();
                }

                TextureFlags flags = TextureFlags::None;

                const std::string &fileStr = fileNode.Scalar();
                auto textureFile = archive.openFile(fileStr);

                if (!textureFile) {
                    LOAD_ERROR("Failed to open file \"%s\"", fileStr.c_str());
                }

                const void *rawTexture = archive.mapFile(textureFile);
                size_t rawTextureSize = archive.fileSize(textureFile);

                int width, height;
                stbi_image image = stbi_image(stbi_load_from_memory((const stbi_uc*)rawTexture, builtin_cast<int>(rawTextureSize), &width, &height, nullptr, STBI_rgb_alpha));
                if (!image) {
                    LOAD_ERROR("Failed to decode image \"%s\" - error %s", fileStr.c_str(), stbi_failure_reason());
                }

                TextureHandle texture = mImpl->hardwareMgr->allocateTexture2D(pixelFormat, flags, width, height, mipmaps);
                if (!texture) {
                    LOAD_ERROR0("Failed to allocate texture!");
                }


                {
                    SamplerParams params;
                    
                    YAML::Node edgeSamplingXNode = node["EdgeSamplingX"],
                                edgeSamplingYNode = node["EdgeSamplingY"],
                                minFilterNode = node["MinFilter"],
                                magFilterNode = node["MagFilter"];

                    if (edgeSamplingXNode && edgeSamplingXNode.IsScalar()) {
                        EdgeSamplingModeFromString(edgeSamplingXNode.Scalar().c_str(), params.edgeSamplingX);
                    }
                    if (edgeSamplingYNode && edgeSamplingYNode.IsScalar()) {
                        EdgeSamplingModeFromString(edgeSamplingYNode.Scalar().c_str(), params.edgeSamplingY);
                    }
                    if (minFilterNode && minFilterNode.IsScalar()) {
                        SamplerMinFilterFromString(minFilterNode.Scalar().c_str(), params.minFilter);
                    }
                    if (magFilterNode && magFilterNode.IsScalar()) {
                        SamplerMagFilterFromString(magFilterNode.Scalar().c_str(), params.magFilter);
                    }

                    mImpl->hardwareMgr->setSamplerParams(texture, params);
                }

                YAML::Node nameNode = node["Name"];
                if (nameNode && nameNode.IsScalar()) {
                    const std::string &name = nameNode.Scalar();
                    mImpl->hardwareMgr->setTextureName(texture, Common::CreateStringId(name.c_str(), name.size()));
                }

                mImpl->hardwareMgr->uploadTexture2D(texture, 0, TextureUploadFlags::GenerateMipmaps, PixelFormat::RGBA8, image);

                return ResourceHandle(texture.handle);
            }
            else {
                LOAD_ERROR("Unknown TextureType \"%s\"", textureType.c_str());
            }
        } catch( const std::exception &e) {
            LOAD_ERROR("Caught exception \"%s\"", e.what());
        }
    }
}