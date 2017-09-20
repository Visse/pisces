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
        struct Object {
            Common::StringId name;
            int firstIndex = 0, 
                indexCount = 0,
                firstVertex = 0,
                vertexCount = 0;
        };
        struct Result {
            std::vector<Vertex> vertexes;
            std::vector<uint16_t> indexes;
            std::vector<Object> objects;
        };

        static PISCES_API Result LoadFile( const std::string &filename );
    };
}