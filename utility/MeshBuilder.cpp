#include "MeshBuilder.h"
#include "Context.h"
#include "HardwareResourceManager.h"
#include "RenderCommandQueue.h"

#include "Common/GeneratorN.h"
#include "Common/PointerHelpers.h"
#include <vector>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

namespace Pisces
{
    struct MeshBuilder::Impl {
        Context *context;

        const VertexAttribute *layout;
        int layoutSize,
            positionIndex;

        BufferHandle vertexBuffer,
                     indexBuffer;
        VertexArrayHandle vertexArray;

        int vertexSize = 0;

        std::vector<uint8_t> vertexes;
        std::vector<uint16_t> indexes;

        struct Command {
            size_t first = 0, count = 0,
                   base = 0;
        };
        std::vector<Command> commands;

    };

    PISCES_API MeshBuilder::MeshBuilder( Context *context, const VertexAttribute *layout, int layoutSize, int positionIndex )
    {
        mImpl->context = context;
        mImpl->layout = layout;
        mImpl->layoutSize = layoutSize;
        mImpl->positionIndex = positionIndex;

        int size = layout->stride;
        for (int i=0; i < layoutSize; ++i) {
            assert(layout[i].source == 0);
            assert(layout[i].stride == size);
            assert(layout[i].offset < size);
        }
        mImpl->vertexSize = size;
    }

    PISCES_API MeshBuilder::~MeshBuilder()
    {
        HardwareResourceManager *hardwareMgr = mImpl->context->getHardwareResourceManager();

        hardwareMgr->freeBuffer(mImpl->vertexBuffer);
        hardwareMgr->freeBuffer(mImpl->indexBuffer);
        hardwareMgr->deleteVertexArray(mImpl->vertexArray);
    }

    PISCES_API void MeshBuilder::begin()
    {
        mImpl->commands.clear();
    }

    PISCES_API void MeshBuilder::end()
    {
        bool recreateVA = (bool)mImpl->vertexArray;

        HardwareResourceManager *hardwareMgr = mImpl->context->getHardwareResourceManager();

        hardwareMgr->freeBuffer(mImpl->vertexBuffer);
        hardwareMgr->freeBuffer(mImpl->indexBuffer);
        hardwareMgr->deleteVertexArray(mImpl->vertexArray);

        mImpl->vertexBuffer = hardwareMgr->allocateBuffer(BufferType::Vertex, BufferUsage::Static, BufferFlags::None, mImpl->vertexes.size(), mImpl->vertexes.data());
        mImpl->indexBuffer  = hardwareMgr->allocateBuffer(BufferType::Index, BufferUsage::Static, BufferFlags::None, mImpl->indexes.size()*sizeof(uint16_t), mImpl->indexes.data());

        mImpl->vertexArray = hardwareMgr->createVertexArray(mImpl->layout, mImpl->layoutSize, &mImpl->vertexBuffer, 1, mImpl->indexBuffer, IndexType::UInt16);

        mImpl->vertexes.clear();
        mImpl->indexes.clear();
    }

    PISCES_API void MeshBuilder::pushMesh( glm::mat4 modelMatrix, 
                                           const void *vertexes, size_t vertexCount, 
                                           const uint16_t *indexes, size_t indexCount )
    {
        size_t base = mImpl->vertexes.size() / mImpl->vertexSize;

        Impl::Command *command = nullptr;
        if (!mImpl->commands.empty()) {
            command = &mImpl->commands.back();
            if ((command->count+base) >= std::numeric_limits<uint16_t>::max()) {
                command = nullptr;
            }
        }

        if (!command) {
            mImpl->commands.emplace_back();
            command = &mImpl->commands.back();

            command->first = mImpl->indexes.size();
            command->base = mImpl->vertexes.size() / mImpl->vertexSize;;
        }
        base -= command->base;
        command->count += indexCount;

        assert((base + vertexCount) <  std::numeric_limits<uint16_t>::max());

        { // Copy indexes
            auto gen = Common::CreateGeneratorN(indexCount, [&](size_t i) {
                assert (indexes[i] < vertexCount);
                return indexes[i] + base;
            });

            mImpl->indexes.insert(mImpl->indexes.end(), gen.begin(), gen.end());
        }
        { // Copy vertexes
            VertexAttribute positionAttribute = mImpl->layout[mImpl->positionIndex];
            assert (positionAttribute.count == 3);
            assert (positionAttribute.type == VertexAttributeType::Float32);

            size_t requiredSize = mImpl->vertexSize * vertexCount + mImpl->vertexes.size();
            if (requiredSize > mImpl->vertexes.capacity()) {
                mImpl->vertexes.reserve(requiredSize*2);
            }

            const int MAX_VERTEX_SIZE = sizeof(float)*32;
            uint8_t tmp[MAX_VERTEX_SIZE];

            assert (mImpl->vertexSize < sizeof(tmp));

            for (size_t i=0; i < vertexCount; ++i) {
                const void *from = Common::advance(vertexes, i * mImpl->vertexSize);
                memcpy(tmp, from, mImpl->vertexSize);

                
                glm::vec4 pos;
                memcpy(&pos, Common::advance(tmp, positionAttribute.offset), sizeof(float)*3);
                pos.w = 1.f;
                pos = modelMatrix * pos;
                pos /= pos.w;

                memcpy(Common::advance(tmp, positionAttribute.offset), &pos, sizeof(float)*3);

                mImpl->vertexes.insert(mImpl->vertexes.end(), tmp, tmp + mImpl->vertexSize);
            }
        }

    }

    PISCES_API void MeshBuilder::draw( RenderCommandQueuePtr commandQueue )
    {
        commandQueue->useVertexArray(mImpl->vertexArray);

        for (const auto &command : mImpl->commands) {
            commandQueue->draw(Primitive::Triangles, command.first, command.count, command.base);
        }
    }
}
