#pragma once


#include "../Fwd.h"

namespace Pisces
{
    struct BuiltinVertex {
        struct {
            float x, y, z;
        } pos;
        struct {
            float x, y, z;
        } norm;
        struct {
            float x, y;
        } uv;
    };

    static const VertexAttribute BUILTIN_VERTEX_LAYOUT[] = {
        {VertexAttributeType::Float32, offsetof(BuiltinVertex, pos), 3, sizeof(BuiltinVertex), 0},
        {VertexAttributeType::Float32, offsetof(BuiltinVertex, norm), 3, sizeof(BuiltinVertex), 0},
        {VertexAttributeType::Float32, offsetof(BuiltinVertex, uv), 2, sizeof(BuiltinVertex), 0}
    };
    static const int BUILTIN_VERTEX_LAYOUT_SIZE = 3;


    struct BuiltinObjectInfo {
        BuiltinObjectInfo() = delete;
        BuiltinObjectInfo( const BuiltinObjectInfo& ) = default;

        BuiltinObject object;
        Primitive primitive;
        const BuiltinVertex *vertexes;
        const size_t vertexCount;

        const uint16_t *indexes;
        const size_t indexCount;
    };


    extern const BuiltinObjectInfo BuiltinObjectInfos[BUILTIN_OBJECT_COUNT];
}