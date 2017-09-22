#include "TextureLoader.h"

#include "HardwareResourceManager.h"

#include "Common/FileUtils.h"
#include "Common/Throw.h"
#include "Common/BuiltinFromString.h"
#include "Common/MemStreamBuf.h"
#include "Common/BuiltinFromString.h"
#include "Common/ErrorUtils.h"
#include "Common/BuiltinCast.h"
#include "Common/Yaml.h"

#include "stb_image.h"
WRAP_HANDLE_FUNC(stbi_image, stbi_uc*, stbi_image_free, nullptr);


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
    
    PISCES_API ResourceHandle TextureLoader::loadResource( Common::Archive &archive, Common::YamlNode node )
    {

#define LOAD_ERROR(error, ...) \
            LOG_ERROR("Failed to load texture in archive \"%s\" error: " error, archive.name(), __VA_ARGS__); \
            return ResourceHandle {};
#define LOAD_ERROR0(error) LOAD_ERROR(error, 0)
        
        try {
            auto textureTypeNode = node["TextureType"];
            if (!textureTypeNode || textureTypeNode.isScalar() == false) {
                LOAD_ERROR0("Missing attribute \"TextureType\"");
            }
            std::string textureType = textureTypeNode.scalar();

            if (textureType == "Texture2D") {
                auto pixelFormatNode = node["PixelFormat"];
                if (!pixelFormatNode || pixelFormatNode.isScalar() == false) {
                    LOAD_ERROR0("Missing attribute \"PixelFormat\"");
                }

                std::string pixelFormatStr = pixelFormatNode.scalar();

                PixelFormat pixelFormat;
                if (!PixelFormatFromString(pixelFormatStr.c_str(), pixelFormatStr.size(), pixelFormat)) {
                    LOAD_ERROR("Unkown PixelFormat \"%s\"", pixelFormatStr.c_str());
                }

                auto fileNode = node["File"];
                if (!fileNode || fileNode.isScalar() == false) {
                    LOAD_ERROR0("Missing attribute \"File\"");
                }

                int mipmaps = 0;
                auto mipmapsNode = node["Mipmaps"];
                if (mipmapsNode) {
                    mipmapsNode.as(mipmaps);
                }

                TextureFlags flags = TextureFlags::None;

                std::string fileStr = fileNode.scalar();
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
                    
                    auto edgeSamplingXNode = node["EdgeSamplingX"],
                         edgeSamplingYNode = node["EdgeSamplingY"],
                         minFilterNode = node["MinFilter"],
                         magFilterNode = node["MagFilter"];

                    if (edgeSamplingXNode && edgeSamplingXNode.isScalar()) {
                        const char *str = edgeSamplingXNode.c_str();

                        EdgeSamplingModeFromString(str, params.edgeSamplingX);
                    }
                    if (edgeSamplingYNode && edgeSamplingYNode.isScalar()) {
                        const char *str = edgeSamplingYNode.c_str();
                        EdgeSamplingModeFromString(str, params.edgeSamplingY);
                    }
                    if (minFilterNode && minFilterNode.isScalar()) {
                        const char *str = minFilterNode.c_str();
                        SamplerMinFilterFromString(str, params.minFilter);
                    }
                    if (magFilterNode && magFilterNode.isScalar()) {
                        const char *str = magFilterNode.c_str();
                        SamplerMagFilterFromString(str, params.magFilter);
                    }

                    mImpl->hardwareMgr->setSamplerParams(texture, params);
                }

                auto nameNode = node["Name"];
                if (nameNode && nameNode.isScalar()) {
                    const char *str = nameNode.c_str();
                    mImpl->hardwareMgr->setTextureName(texture, Common::CreateStringId(str));
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