#include "TextureLoader.h"

#include "HardwareResourceManager.h"

#include "Common/FileUtils.h"
#include "Common/Throw.h"
#include "Common/BuiltinFromString.h"
#include "Common/MemStreamBuf.h"
#include "Common/BuiltinFromString.h"
#include "Common/Throw.h"
#include "Common/BuiltinCast.h"

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
    
    PISCES_API ResourceHandle TextureLoader::loadResource( Common::Archive &archive, libyaml::Node node )
    {
#define LOG_ATTRIBUTE_WARNING(node, type, attribute)  \
        do {auto mark = node.startMark();             \
            LOG_WARNING("Expected %s for attribute \"%s\" at %i:%i when loading texture in archive %s", type, attribute, mark.line, mark.col, archive.name()); \
        } while(0);
        
        auto textureTypeNode = node["TextureType"];
        if (!textureTypeNode || textureTypeNode.isScalar() == false) {
            THROW(std::runtime_error,
                "Missing attribute \"TextureType\""
            );
        }
        std::string textureType = textureTypeNode.scalar();

        if (textureType == "Texture2D") {
            auto pixelFormatNode = node["PixelFormat"];
            if (!pixelFormatNode.isScalar()) {
                THROW(std::runtime_error, 
                        "Missing attribute \"PixelFormat\""
                );
            }

            std::string pixelFormatStr = pixelFormatNode.scalar();

            PixelFormat pixelFormat;
            if (!PixelFormatFromString(pixelFormatStr.c_str(), pixelFormatStr.size(), pixelFormat)) {
                auto mark = pixelFormatNode.startMark();
                THROW(std::runtime_error,
                        "Unkown PixelFormat \"%s\" at %i:%i", pixelFormatStr.c_str(), mark.line, mark.col
                );
            }

            auto fileNode = node["File"];
            if (!fileNode.isScalar()) {
                THROW(std::runtime_error,
                        "Missing attribute \"File\""
                );
            }

            int mipmaps = 0;
            auto mipmapsNode = node["Mipmaps"];
            if (mipmapsNode) {
                if (!(mipmapsNode.isScalar() && Common::BuiltinFromString(mipmapsNode.scalar(), mipmaps))) {
                    auto mark = mipmapsNode.startMark();
                    LOG_ATTRIBUTE_WARNING(node, "integear", "Mipmaps");
                }
            }

            TextureFlags flags = TextureFlags::None;

            std::string fileStr = fileNode.scalar();
            auto textureFile = archive.openFile(fileStr);

            if (!textureFile) {
                THROW(std::runtime_error,
                        "Failed to open file \"%s\"", fileStr.c_str()
                );
            }

            const void *rawTexture = archive.mapFile(textureFile);
            size_t rawTextureSize = archive.fileSize(textureFile);

            int width, height;
            stbi_image image = stbi_image(stbi_load_from_memory((const stbi_uc*)rawTexture, builtin_cast<int>(rawTextureSize), &width, &height, nullptr, STBI_rgb_alpha));
            if (!image) {
                THROW(std::runtime_error,
                    "Failed to decode image \"%s\" - error %s", fileStr.c_str(), stbi_failure_reason()
                );
            }

            TextureHandle texture = mImpl->hardwareMgr->allocateTexture2D(pixelFormat, flags, width, height, mipmaps);
            if (!texture) {
                THROW(std::runtime_error,
                        "Failed to allocate texture!"
                );
            }

            {
                SamplerParams params;
                    
                auto edgeSamplingXNode = node["EdgeSamplingX"],
                        edgeSamplingYNode = node["EdgeSamplingY"],
                        minFilterNode = node["MinFilter"],
                        magFilterNode = node["MagFilter"];

                if (edgeSamplingXNode) {
                    if (!(edgeSamplingXNode.isScalar() && EdgeSamplingModeFromString(edgeSamplingXNode.scalar(), params.edgeSamplingX))) {
                        LOG_ATTRIBUTE_WARNING(node, "EdgeSamplingMode", "EdgeSamplingX");
                    }
                }
                if (edgeSamplingYNode && edgeSamplingYNode.isScalar()) {
                    if (!(edgeSamplingYNode.isScalar() && EdgeSamplingModeFromString(edgeSamplingYNode.scalar(), params.edgeSamplingY))) {
                        LOG_ATTRIBUTE_WARNING(node, "EdgeSamplingMode", "EdgeSamplingY");
                    }
                }
                if (minFilterNode && minFilterNode.isScalar()) {
                    if (!(minFilterNode.isScalar() && SamplerMinFilterFromString(minFilterNode.scalar(), params.minFilter))) {
                        LOG_ATTRIBUTE_WARNING(node, "SamplerMinFilter", "MinFilter");
                    }
                }
                if (magFilterNode && magFilterNode.isScalar()) {
                    if (!(magFilterNode.isScalar() && SamplerMagFilterFromString(magFilterNode.scalar(), params.magFilter))) {
                        LOG_ATTRIBUTE_WARNING(node, "SamplerMagFilter", "MagFilter");
                    }
                }

                mImpl->hardwareMgr->setSamplerParams(texture, params);
            }

            auto nameNode = node["Name"];
            if (nameNode) {
                if (nameNode.isScalar()) {
                    const char *str = nameNode.scalar();
                    mImpl->hardwareMgr->setTextureName(texture, Common::CreateStringId(str));
                }
                else {
                    LOG_ATTRIBUTE_WARNING(node, "string", "Name");
                }
            }

            mImpl->hardwareMgr->uploadTexture2D(texture, 0, TextureUploadFlags::GenerateMipmaps, PixelFormat::RGBA8, image);

            return ResourceHandle(texture.handle);
        }
        else {
            auto mark = textureTypeNode.startMark();
            THROW(std::runtime_error, 
                    "Unknown TextureType \"%s\" at %i:%i", textureType.c_str(), mark.line, mark.col
            );
        }
    }
}