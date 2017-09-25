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
        
#define GET_NODE(node, name, attribute, typeFunc)               \
        auto name = node[attribute];                            \
        if (!name|| !name.typeFunc) {                           \
           THROW(std::runtime_error,                            \
                "Missing attribute \"" attribute "\""           \
            );                                                  \
        }

        GET_NODE(node, nameNode, "Name", isScalar());
        GET_NODE(node, textureNode, "Texture", isScalar());

        GET_NODE(node, uvNode, "UV", isMap());

        GET_NODE(uvNode, x1Node, "x1", isScalar());
        GET_NODE(uvNode, y1Node, "y1", isScalar());
        GET_NODE(uvNode, x2Node, "x2", isScalar());
        GET_NODE(uvNode, y2Node, "y2", isScalar());
        

        Common::StringId textureName = Common::CreateStringId(textureNode.scalar());

        sprite.texture = mHardwareMgr->findTextureByName(textureName);


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

        Common::StringId name = Common::CreateStringId(nameNode.scalar());
        mSpriteMgr->setSprite(name, sprite);

        return ResourceHandle(name.handle);
    }
}

