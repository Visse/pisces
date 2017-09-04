#include "SpriteLoader.h"
#include "Sprite.h"
#include "HardwareResourceManager.h"
#include "SpriteManager.h"

#include "Common/ErrorUtils.h"
#include "Common/StringId.h"

#include <yaml-cpp/yaml.h>

namespace Pisces
{
    PISCES_API ResourceHandle SpriteLoader::loadResource( Common::Archive &archive, YAML::Node node )
    {
        Sprite sprite;
        
#define LOAD_ERROR(error, ...) \
            LOG_ERROR("Failed to load sprite in archive \"%s\" error: " error, archive.name(), __VA_ARGS__); \
            return ResourceHandle {};
#define LOAD_ERROR0(error) LOAD_ERROR(error, 0)
#define GET_NODE(node, name, attribute, typeFunc)               \
        YAML::Node name = node[attribute];                      \
        if (!name|| !name.typeFunc) {                           \
           LOAD_ERROR0("Missing attribute \"" attribute "\"");  \
        }

        GET_NODE(node, nameNode, "Name", IsScalar());
        GET_NODE(node, textureNode, "Texture", IsScalar());

        GET_NODE(node, uvNode, "UV", IsMap());

        GET_NODE(uvNode, x1Node, "x1", IsScalar());
        GET_NODE(uvNode, y1Node, "y1", IsScalar());
        GET_NODE(uvNode, x2Node, "x2", IsScalar());
        GET_NODE(uvNode, y2Node, "y2", IsScalar());
        
        Common::StringId textureName = Common::CreateStringId(textureNode.Scalar().c_str());

        sprite.texture = mHardwareMgr->findTextureByName(textureName);

        sprite.uv.x1 = x1Node.as<float>();
        sprite.uv.y1 = y1Node.as<float>();
        sprite.uv.x2 = x2Node.as<float>();
        sprite.uv.y2 = y2Node.as<float>();

        Common::StringId name = Common::CreateStringId(nameNode.Scalar().c_str());
        mSpriteMgr->setSprite(name, sprite);

        return ResourceHandle(name.handle);
    }
}

