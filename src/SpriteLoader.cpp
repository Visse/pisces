#include "SpriteLoader.h"
#include "Sprite.h"
#include "HardwareResourceManager.h"
#include "SpriteManager.h"

#include "Common/Throw.h"
#include "Common/StringId.h"
#include "Common/BuiltinFromString.h"

namespace Pisces
{
    PISCES_API ResourceHandle SpriteLoader::loadResource( Common::Archive &archive, libyaml::Node node )
    {
        Sprite sprite;
        
#define GET_NODE(node, name, attribute, typeFunc, force)        \
        auto name = node[attribute];                            \
        if (force && (!name|| !name.typeFunc)) {                \
           THROW(std::runtime_error,                            \
                "Missing attribute \"" attribute "\""           \
            );                                                  \
        }

        GET_NODE(node, nameNode, "Name", isScalar(), true);
        GET_NODE(node, textureNode, "Texture", isScalar(), true);
        GET_NODE(node, pixelSpaceNode, "PixelSpace", isScalar(), false);

        GET_NODE(node, uvNode, "UV", isMap(), true);

        GET_NODE(uvNode, x1Node, "x1", isScalar(), true);
        GET_NODE(uvNode, y1Node, "y1", isScalar(), true);
        GET_NODE(uvNode, x2Node, "x2", isScalar(), true);
        GET_NODE(uvNode, y2Node, "y2", isScalar(), true);
        

        bool pixelSpace = false;
        if (pixelSpaceNode && !Common::BuiltinFromString(pixelSpaceNode.scalar(),pixelSpace)) {
            auto mark = pixelSpaceNode.startMark();
            THROW(std::runtime_error,
                    "Expected bool for attribute \"%s\" at %i:%i",
                    "PixelSpace", mark.line, mark.col
            ); 
        }

        Common::StringId textureName = Common::CreateStringId(textureNode.scalar());

        sprite.texture = mHardwareMgr->findTextureByName(textureName);

        if (!sprite.texture) {
            THROW(std::runtime_error,
                "Failed to find texture \"%s\"",
                Common::GetCString(textureName)
            );
        }

#define GET_FLOAT(val, node, name)                                      \
            if (!Common::BuiltinFromString(node.scalar(), val)) {       \
                auto mark = node.startMark();                           \
                THROW(std::runtime_error,                               \
                      "Expected number for attribute \"%s\" at %i:%i",  \
                      name, mark.line, mark.col                         \
                );                                                      \
            }

        GET_FLOAT(sprite.uv.x1, x1Node, "x1");
        GET_FLOAT(sprite.uv.y1, y1Node, "y1");
        GET_FLOAT(sprite.uv.x2, x2Node, "x2");
        GET_FLOAT(sprite.uv.y2, y2Node, "y2");

        int width=1, height=1;
        if (pixelSpace && mHardwareMgr->getTextureSize(sprite.texture, &width, &height)) {
            sprite.uv.x1 /= width;
            sprite.uv.x2 /= width;
            sprite.uv.y1 /= height;
            sprite.uv.y2 /= height;
        }

        Common::StringId name = Common::CreateStringId(nameNode.scalar());
        mSpriteMgr->setSprite(name, sprite);

        return ResourceHandle(name.handle);
    }
}

