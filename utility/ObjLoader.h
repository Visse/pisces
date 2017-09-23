#pragma once

#include "Pisces/Fwd.h"
#include "Pisces/build_config.h"

#include "Common/StringId.h"
#include "Common/Archive.h"

#include <vector>


namespace Pisces
{
    class ObjLoader {
    public:
        struct Vertex {
            float pos[3];
            uint16_t uv[2];
            // packed in 2_10_10_10 format
            uint32_t normal;
        };

        struct SubObject {
            int first=0, count=0;
            Common::StringId material;
        };
        struct Object {
            Common::StringId name;

            std::vector<Vertex> vertexes;
            std::vector<uint16_t> indexes;

            std::vector<SubObject> subobjects;
        };
        struct Result {
            std::vector<Object> objects;
        };

        static PISCES_API Result LoadFile( Common::Archive &archive, const std::string &filename );
    };
}