#pragma once

#include "Pisces/Fwd.h"
#include "Pisces/build_config.h"

#include "Common/PImplHelper.h"

#include <glm/fwd.hpp>

namespace Pisces
{
    class MeshBuilder {
    public:
        PISCES_API MeshBuilder( Context *context, const VertexAttribute *layout, int layoutSize, int positionIndex );
        PISCES_API ~MeshBuilder();

        PISCES_API void begin();
        PISCES_API void end();

        PISCES_API void pushMesh( glm::mat4 modelMatrix, 
            const void *vertexes, size_t vertexCount,
            const uint16_t *indexes, size_t indexCount              
        );

        // binds vertex array & draws all meshes
        PISCES_API void draw( RenderCommandQueuePtr commandQueue );

    private:
        struct Impl;
        PImplHelper<Impl, 256> mImpl;
    };
}