#include "CommandQueueCompiler.h"

#include "Context.h"
#include "PipelineManager.h"
#include "PipelineManagerImpl.h"
#include "HardwareResourceManager.h"
#include "HardwareResourceManagerImpl.h"
#include "RenderCommandQueue.h"
#include "RenderCommandQueueImpl.h"
#include "CompiledRenderQueueImpl.h"
#include "Helpers.h"

#include "Common/ErrorUtils.h"

#include <glbinding/gl33core/gl.h>
#include <glbinding/gl33core/enum.h>
#include <glbinding/gl33core/bitfield.h>

using namespace gl33core;

LOG_MODULE(CommandQueueCompiler);

namespace Pisces
{
    struct ImageTexture {
        TextureHandle texture;
        ImageTextureAccess access = {};
        PixelFormat format = {};

        friend bool operator == ( const ImageTexture &lhs, const ImageTexture &rhs ) {
            if (lhs.texture != rhs.texture) return false;
            if (lhs.access != rhs.access) return false;
            if (lhs.format != rhs.format) return false;

            return true;
        }
        friend bool operator != ( const ImageTexture &lhs, const ImageTexture &rhs ) {
            return !operator == (lhs, rhs);
        }

        operator bool () const {
            return (bool)texture;
        }
    };

    struct Uniform {
        Uniform() {
            // Its bad to memset this, but its the easiest way to zero initilize
            memset(this, 0, sizeof(Uniform));
        }

        GLType type = GLTypeNone;
        union Data {
            int32_t i[16];
            uint32_t u[16];
            float f[16];
        } data;

        friend bool operator == (const Uniform &lhs, const Uniform &rhs) {
            return memcmp(&lhs, &rhs, sizeof(Uniform)) == 0;
        }
    };
    
    struct ResourceBindings {
        TextureHandle samplers[MAX_BOUND_SAMPLERS];
        UniformBufferHandle uniformBuffers[MAX_BOUND_UNIFORM_BUFFERS];
        ImageTexture imageTextures[MAX_BOUND_IMAGE_TEXTURES];

        Uniform uniforms[MAX_BOUND_UNIFORMS];
    };

    struct TextureUnitInfo {
        TextureHandle texture;
        TextureType type = TextureType(-1);
        int lastUsed = 0;
    };

    struct State {
        PipelineHandle pipeline;
        VertexArrayHandle vertexArray;
        ClipRect clipRect;

        bool clipping = false, 
             depthTest = true,
             faceCulling = true,
             depthWrite = true,
             primitiveRestart = false;
        BlendMode blendMode;

        ResourceBindings bindings;
    };

    
    namespace CQI = RenderCommandQueueImpl;
    namespace CCQI = CompiledRenderQueueImpl;

    namespace PMI = PipelineManagerImpl;
    namespace HRMI = HardwareResourceManagerImpl;


    struct CompilerImpl {
        Context *context;
        PMI::Impl *pipelineMgr;
        HRMI::Impl *hardwareMgr;

        RenderQueueCompileOptions options;

        const PMI::PipelineInfo *pipelineInfo = nullptr;

        const PMI::BaseProgramInfo *programInfo = nullptr;

        const PMI::RenderProgramInfo *renderProgramInfo = nullptr;
        const PMI::ComputeProgramInfo *computeProgramInfo = nullptr;
        const PMI::TransformProgramInfo *transformProgramInfo = nullptr;

        const HRMI::VertexArrayInfo *vertexArrayInfo = nullptr;

        State state;
        State current;
        std::vector<CCQI::Command> commands;

        int currentCommand = 0;

        int numTextureUnits = -1;
        std::vector<TextureUnitInfo> textureUnits;

        void init( Context *ctx,  const RenderQueueCompileOptions &opts ) 
        {
            context = ctx;
            pipelineMgr = ctx->getPipelineManager()->impl();
            hardwareMgr = ctx->getHardwareResourceManager()->impl();

            options = opts;
            
            state.clipRect.w = context->displayWidth();
            state.clipRect.y = context->displayHeight();

            numTextureUnits = ctx->getHardwareLimit(HardwareLimitName::TextureUnits);
            textureUnits.resize(numTextureUnits);
        }

        void onProgramChanged()
        {
            current.bindings = {};
        }
    };

    bool LookupTextureSampler( CompilerImpl &impl, TextureHandle handle, const HRMI::TextureInfo* &texture, const HRMI::SamplerInfo* &sampler )
    {
        if (impl.hardwareMgr->textures.IsHandleFromThis(handle)) {
            texture = impl.hardwareMgr->textures.find(handle);
            if (!texture) return false;
            return true;
        }
        else if (impl.hardwareMgr->samplers.IsHandleFromThis(handle)) {
            sampler = impl.hardwareMgr->samplers.find(handle);
            if (!sampler) return false;

            texture = impl.hardwareMgr->textures.find(handle);
            if (!texture) return false;

            return true;
        }
        return false;
    }

    int findSlotForTexture( CompilerImpl &impl, TextureHandle handle, bool &existing )
    {
        int lastUsed = -1;
        int slot = -1;
        assert (impl.textureUnits.size() == impl.numTextureUnits);
        for (int i=0; i < impl.numTextureUnits; ++i) {
            if (impl.textureUnits[i].texture == handle) {
                // We found a binding for the texture, return the slot
                existing = true;
                return i;
            }

            // Is this slot free?, and the first free slot we found so far?
            if (!impl.textureUnits[i].texture && (lastUsed != -1 || slot == -1)) {
                lastUsed = -1;
                slot = i;
            }
            else if(slot == -1 || impl.textureUnits[i].lastUsed < lastUsed) {
                // The slot isn't free, but its the oldest one (hasn't been used for the longest time)
                // so recycle it.
                lastUsed = impl.textureUnits[i].lastUsed;
                slot = i;
            }
        }
        // We didn't find any existing binding for the texture
        existing = false;
        return slot;
    }

    void Emit( CompilerImpl &impl, const CCQI::Command &command ) 
    {
        impl.commands.push_back(command);
    }

    void EmitEnableDisable( CompilerImpl &impl, bool &var, bool test, GLenum cap )
    {
        if (test != var) {
            if (test) {
                Emit(impl, CCQI::Enable(cap));
            }
            else {
                Emit(impl, CCQI::Disable(cap));
            }
            var = test;
        }
    }

    void EmitBlendMode( CompilerImpl &impl, BlendMode blendMode )
    {
        switch (blendMode) {
        case BlendMode::Replace:
            Emit(impl, CCQI::Disable(GL_BLEND));
            break;
        case BlendMode::Alpha:
            Emit(impl, CCQI::Enable(GL_BLEND));
            Emit(impl, CCQI::SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            break;
        case BlendMode::PreMultipledAlpha:
            Emit(impl, CCQI::Enable(GL_BLEND));
            Emit(impl, CCQI::SetBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
            break;
        }
        impl.current.blendMode = blendMode;
    }

    bool EmitBindPipeline( CompilerImpl &impl, PipelineHandle handle )
    {
        if (handle && impl.current.pipeline == handle) return true;
        // ignore if we are in transform feedback mode
        if (impl.transformProgramInfo) return true;

        PMI::PipelineInfo *pipeline = impl.pipelineMgr->pipelines.find(handle);
        if (!pipeline) return false;

        EmitEnableDisable(impl, impl.current.depthTest, all(pipeline->flags, PipelineFlags::DepthTest), GL_DEPTH_TEST);
        EmitEnableDisable(impl, impl.current.faceCulling, all(pipeline->flags, PipelineFlags::FaceCulling), GL_CULL_FACE);

        bool depthWrite = all(pipeline->flags, PipelineFlags::DepthWrite);
        if (depthWrite != impl.current.depthWrite) {
            Emit(impl, CCQI::SetDepthMask(depthWrite));
            impl.current.depthWrite = depthWrite;
        }

        if (impl.current.blendMode != pipeline->blendMode) {
            EmitBlendMode(impl, pipeline->blendMode);
        }

        PMI::RenderProgramInfo *programInfo = impl.pipelineMgr->renderPrograms.find(pipeline->program);
        if (!programInfo) return false;

        Emit(impl, CCQI::SetProgram(programInfo->glProgram));

        impl.programInfo = programInfo;
        impl.renderProgramInfo = programInfo;
        impl.computeProgramInfo = nullptr;
        impl.pipelineInfo = pipeline;
        impl.current.pipeline = handle;

        impl.onProgramChanged();

        return true;
    }

    bool EmitBindSampler( CompilerImpl &impl, int i, TextureHandle handle )
    {
        assert (i >= 0 && i < MAX_BOUND_SAMPLERS);
        assert (impl.programInfo);

        if (!handle) return true;
        if (impl.current.bindings.samplers[i] == handle) return true;
        // Program doesn't have a sampler for this slot - ignore it
        if (impl.programInfo->samplers[i].type == TextureType(-1)) return true;

        bool existing = false;
        int slot = findSlotForTexture(impl, handle, existing);

        if (!existing) {
            const HRMI::TextureInfo *texture = nullptr;
            const HRMI::SamplerInfo *sampler = nullptr;

            if (!LookupTextureSampler(impl, handle, texture, sampler)) return false;

            Emit(impl, CCQI::BindSampler(
                slot,
                TextureTarget(texture->type),
                texture->glTexture,
                sampler ? sampler->glSampler : 0
            ));

            TextureUnitInfo &info = impl.textureUnits[slot];
                info.texture = handle;
                info.type = texture->type;
        }
        

        TextureUnitInfo &unit = impl.textureUnits[slot];
        if (unit.type != impl.programInfo->samplers[i].type) {
            LOG_WARNING("Missmatch between bound texture type (%i) and expected type (%i) for program %s", 
                (int)unit.type, (int)impl.programInfo->samplers[i].type, Common::GetCString(impl.programInfo->name)
            );
        }

        unit.lastUsed = impl.currentCommand;
        Emit(impl,
            CCQI::BindUniformInt(impl.programInfo->samplers[i].location, slot)
        );

        impl.current.bindings.samplers[i] = handle;
        return true;
    }

    bool EmitBindUniformBuffer( CompilerImpl &impl, int i, UniformBufferHandle handle )
    {
        assert (i >= 0 && i < MAX_BOUND_UNIFORM_BUFFERS);
        assert (impl.programInfo);
        if (!handle) return true;
        if (impl.current.bindings.uniformBuffers[i] == handle) return true;

        HRMI::BufferInfo *buffer = impl.hardwareMgr->buffers.find(handle.buffer);
        if (!buffer) return false;

        if ((handle.offset+handle.size) > buffer->size) return false;

        int loc = impl.programInfo->uniformBuffers[i].location;
        if (loc == -1) return true;

        Emit(impl, CCQI::BindBufferRange(GL_UNIFORM_BUFFER, loc, buffer->glBuffer, handle.offset, handle.size));
        impl.current.bindings.uniformBuffers[i] = handle;
        return true;
    }

    bool EmitBindImageTexture( CompilerImpl &impl, int i, const ImageTexture &handle )
    {
        assert (i >= 0 && i < MAX_BOUND_IMAGE_TEXTURES);
        assert(impl.programInfo);

        if (!handle) return true;
        if (impl.current.bindings.imageTextures[i] == handle) return true;
        
        const HRMI::TextureInfo *texture = nullptr;
        const HRMI::SamplerInfo *sampler = nullptr;

        if (!LookupTextureSampler(impl, handle.texture, texture, sampler)) return false;

        int loc = impl.programInfo->imageTextures[i].location;

        Emit(impl, CCQI::BindImageTexture(i,
            texture->glTexture,
            0,
            GL_FALSE,
            0,
            ToGL(handle.access),
            InternalPixelFormat(handle.format)
        ));
        Emit(impl, CCQI::BindUniformInt(loc, i));

        impl.current.bindings.imageTextures[i] = handle;
        return true;
    }

    bool EmitBindUniform( CompilerImpl &impl, int i, const Uniform &uniform )
    {
        assert (i >= 0 && i < MAX_BOUND_UNIFORMS);
        assert(impl.programInfo);

        if (impl.current.bindings.uniforms[i] == uniform) return true;
        if (uniform.type == GLTypeNone) return true;
        if (impl.programInfo->uniforms[i].type == GLTypeNone) return true;

        if (impl.programInfo->uniforms[i].type != uniform.type) {
            LOG_ERROR("Missmatch between uniform binding and program, expected type %i, got %i for program %s", 
                (int)impl.programInfo->uniforms[i].type, (int)uniform.type, Common::GetCString(impl.programInfo->name)
            );
            return false;
        }

        int loc = impl.programInfo->uniforms[i].location;

        switch(uniform.type) {
        default:
        case GLType::Mat2x2:
        case GLType::Mat2x3:
        case GLType::Mat2x4:
        case GLType::Mat3x2:
        case GLType::Mat3x3:
        case GLType::Mat3x4:
        case GLType::Mat4x2:
        case GLType::Mat4x3:
        case GLType::Sampler1D:
        case GLType::Sampler2D:
        case GLType::ImageTexture1D:
        case GLType::ImageTexture2D:
            FATAL_ERROR("Unsupported uniform type!");
            break;
        case GLType::Int:
            Emit(impl, CCQI::BindUniformInt(loc, uniform.data.i[0]));
            break;
        case GLType::UInt:
            Emit(impl, CCQI::BindUniformUInt(loc, uniform.data.u[0]));
            break;
        case GLType::Float:
            Emit(impl, CCQI::BindUniformFloat(loc, uniform.data.f[0]));
            break;
        case GLType::Vec2: {
            CCQI::vec2 data;
            for (int j=0; j < 2; ++j) data[j] = uniform.data.f[j];

            Emit(impl, CCQI::BindUniformVec2(loc, data));
          } break;
        case GLType::Vec3: {
            CCQI::vec3 data;
            for (int j=0; j < 3; ++j) data[j] = uniform.data.f[j];

            Emit(impl, CCQI::BindUniformVec3(loc, data));
          } break;
        case GLType::Vec4: {
            CCQI::vec4 data;
            for (int j=0; j < 4; ++j) data[j] = uniform.data.f[j];

            Emit(impl, CCQI::BindUniformVec4(loc, data));
          } break;
        case GLType::Mat4x4: {
            CCQI::mat4 mat;
            for (int j=0; j < 16; ++j) mat[j] = uniform.data.f[j];
            Emit(impl, CCQI::BindUniformMat4(loc, mat));
          } break;
        }
        impl.current.bindings.uniforms[i] = uniform;

        return true;
    }

    bool EmitBindResources( CompilerImpl &impl, const PMI::BaseProgramInfo *program, const ResourceBindings &bindings )
    {
        for (int i=0; i < MAX_BOUND_SAMPLERS; ++i) {
            if (!EmitBindSampler(impl, i, bindings.samplers[i])) {
                LOG_ERROR("Failed to bind texture (%i) to slot %i", (int)bindings.samplers[i], i);
                return false;
            }
        }

        for (int i=0; i < MAX_BOUND_UNIFORMS; ++i) {
            if (!EmitBindUniformBuffer(impl, i, bindings.uniformBuffers[i])) {
                LOG_ERROR("Failed to bind uniform buffer (%i) to slot %i", (int)bindings.uniformBuffers[i], i);
                return false;
            }
        }

        for (int i=0; i < MAX_BOUND_IMAGE_TEXTURES; ++i) {
            if (!EmitBindImageTexture(impl, i, bindings.imageTextures[i])) {
                LOG_ERROR("Failed to bind image texture(%i) to slot %i", (int)bindings.imageTextures[i].texture, i);
                return false;
            }
        }

        for (int i=0; i < MAX_BOUND_UNIFORMS; ++i) {
            if (!EmitBindUniform(impl, i, bindings.uniforms[i])) {
                LOG_ERROR("Failed to bind uniform of type %i to slot %i", (int)bindings.uniforms[i].type, i);
                return false;
            }
        }
        return true;
    }

    bool EmitBindVertexArray( CompilerImpl &impl, VertexArrayHandle handle )
    {
        if (handle && impl.current.vertexArray == handle) return true;

        HRMI::VertexArrayInfo *vertexArray = impl.hardwareMgr->vertexArrays.find(handle);
        if (!vertexArray) return false;


        Emit(impl, CCQI::BindVertexArray(vertexArray->glVertexArray));
        impl.vertexArrayInfo = vertexArray;
        impl.current.vertexArray = handle;

        if (vertexArray->indexBuffer) {
            HRMI::BufferInfo *indexBuffer = impl.hardwareMgr->buffers.find(vertexArray->indexBuffer);
            if (!indexBuffer) return false;

            Emit(impl, CCQI::BindBuffer(gl::GL_ELEMENT_ARRAY_BUFFER, indexBuffer->glBuffer));
            
        }

        if (vertexArray->indexType != IndexType::None && all(vertexArray->flags, VertexArrayFlags::UsePrimitiveRestart)) {
            EmitEnableDisable(impl, impl.current.primitiveRestart, true, GL_PRIMITIVE_RESTART);
            if (vertexArray->indexType == IndexType::UInt16) {
                Emit(impl, CCQI::PrimitiveRestartIndex(uint16_t(-1)));
            }
            else if((vertexArray->indexType == IndexType::UInt32)) {
                Emit(impl, CCQI::PrimitiveRestartIndex(uint32_t(-1)));
            }
            else {
                FATAL_ERROR("Unknown index type!");
            }
        }
        else {
            EmitEnableDisable(impl, impl.current.primitiveRestart, false, GL_PRIMITIVE_RESTART);
        }

        return true;
    }

    void EmitDraw( CompilerImpl &impl, const CQI::DrawData &data, const State &state, size_t base, size_t first )
    {
        if (!EmitBindPipeline(impl, state.pipeline)) {
            LOG_ERROR("Failed to bind pipeline (%i)", (int)state.pipeline);
            return;
        }
        if (!EmitBindVertexArray(impl, state.vertexArray)) {
            LOG_ERROR("Failed to bind vertexarray (%i)", (int)state.vertexArray);
            return;
        }

        assert (impl.programInfo);
        if (!EmitBindResources(impl, impl.programInfo, state.bindings)) {
            LOG_ERROR("Failed to bind resources!");
            return;
        }

        if (impl.state.clipping != impl.current.clipping) {
            if (impl.state.clipping) {
                Emit(impl, CCQI::Enable(GL_SCISSOR_TEST));
                impl.current.clipping = true;
            }
            else {
                Emit(impl, CCQI::Disable(GL_SCISSOR_TEST));
                impl.current.clipping = false;
            }
        }

        if (impl.state.clipping && state.clipRect != impl.current.clipRect) {
            Emit(impl, CCQI::SetClipRect(state.clipRect));
            impl.current.clipRect = state.clipRect;
        }

        assert(impl.vertexArrayInfo);
        if (impl.vertexArrayInfo->indexBuffer) {
            Emit(impl, CCQI::DrawIndexed(
                ToGL(data.primitive), 
                ToGL(impl.vertexArrayInfo->indexType), 
                data.count,
                data.base + base, 
                (void*)OffsetForIndexType(impl.vertexArrayInfo->indexType, data.first+first)
            ));
        }
        else {
            Emit(impl, CCQI::Draw(
                ToGL(data.primitive),
                data.first + data.base + first + base,
                data.count
            ));
        }
    }
    
    void EmitDraw( CompilerImpl &impl, const CQI::DrawData &data )
    {
        EmitDraw(impl, data, impl.state, impl.options.baseVertex, impl.options.firstIndex);
    }

    void EmitDrawBuiltin( CompilerImpl &impl, const CQI::DrawBuiltinData &data )
    {
        int idx = (int)data.object;
        FATAL_ASSERT(idx >= 0 && idx < BUILTIN_OBJECT_COUNT, "Invalid Builtin %i!", idx);

        const HRMI::BuiltinDrawInfo &drawInfo = impl.hardwareMgr->builtinDrawInfo[idx];

        CQI::DrawData draw = CQI::Draw(
            drawInfo.primitive,
            drawInfo.first,
            drawInfo.count,
            drawInfo.base
        );

        State state = impl.state;
        state.vertexArray = drawInfo.vertexArray;

        EmitDraw(impl, draw, state, 0, 0);
    }

    void EmitClear( CompilerImpl &impl, const CQI::ClearData &data )
    {
        gl::ClearBufferMask mask = GL_NONE_BIT;

        if (all(data.flags, ClearFlags::Color)) {
            Emit(impl, CCQI::SetClearColor(data.color));
            mask |= GL_COLOR_BUFFER_BIT;
        }
        if (all(data.flags, ClearFlags::Depth)) {
            Emit(impl, CCQI::SetClearDepth(data.depth));
            mask |= GL_DEPTH_BUFFER_BIT;
        }
        if (all(data.flags, ClearFlags::Stencil)) {
            Emit(impl, CCQI::SetClearStencil(data.stencil));
            mask |= GL_STENCIL_BUFFER_BIT;
        }

        Emit(impl, CCQI::Clear((GLenum)mask));
    }

    void EmitExecuteCompute( CompilerImpl &impl, const CQI::ExecuteComputeData &data )
    {
        FATAL_ASSERT(impl.transformProgramInfo == nullptr, "Can't execute compute! - Transform feedback is in progress");

        PMI::ComputeProgramInfo *programInfo = impl.pipelineMgr->computePrograms.find(data.program);
        if (!programInfo) return;

        impl.programInfo = programInfo;
        impl.computeProgramInfo = programInfo;
        impl.renderProgramInfo = nullptr;
        impl.pipelineInfo = nullptr;
        impl.current.pipeline = {};

        impl.onProgramChanged();

        Emit(impl, CCQI::SetProgram(programInfo->glProgram));

        if (!EmitBindResources(impl, programInfo, impl.state.bindings)) {
            LOG_ERROR("Failed to bind resources for compute program!");
            return;
        }

        Emit(impl, CCQI::DispatchCompute(data.size[0], data.size[1], data.size[2]));
    }

    void UsePipeline( CompilerImpl &impl, const CQI::UsePipelineData &data )
    {
        impl.state.pipeline = data.pipeline;
    }

    void UseVertexArray( CompilerImpl &impl, const CQI::UseVertexArrayData &data )
    {
        impl.state.vertexArray = data.vertexArray;
    }

    void UseClipping( CompilerImpl &impl, const CQI::UseClippingData &data )
    {
        impl.state.clipping = data.use;
    }

    void UseClipRect( CompilerImpl &impl, const CQI::UseClipRectData &data )
    {
        impl.state.clipRect = data.clipRect;
    }

    void BindSampler( CompilerImpl &impl, const CQI::BindSamplerData &data )
    {
        FATAL_ASSERT(data.slot >= 0 && data.slot < MAX_BOUND_SAMPLERS, "Invalid texture slot %i", data.slot);
        impl.state.bindings.samplers[data.slot] = data.sampler;
    }

    void BindBuiltinTexture( CompilerImpl &impl, const CQI::BindBuiltinTextureData &data )
    {
        int idx = (int)data.texture;
        FATAL_ASSERT(idx >= 0 && idx < BUILTIN_TEXTURE_COUNT, "Invalid Builtin Texture %i!", idx);

        CQI::BindSamplerData sampler;
            sampler.slot = data.slot;
            sampler.sampler = impl.hardwareMgr->builtinTextures[idx];

        BindSampler(impl, sampler);
    }

    void BindImageTexture( CompilerImpl &impl, const CQI::BindImageTextureData &data )
    {
        FATAL_ASSERT(data.slot >= 0 && data.slot < MAX_BOUND_IMAGE_TEXTURES, "Invalid image texture slot %i", data.slot);
        ImageTexture image;
            image.texture = data.texture;
            image.access = data.access;
            image.format = data.format;
        impl.state.bindings.imageTextures[data.slot] = image;
    }

    void BindUniformBuffer( CompilerImpl &impl, const CQI::BindUniformBufferData &data )
    {
        FATAL_ASSERT(data.slot >= 0 && data.slot < MAX_BOUND_UNIFORM_BUFFERS, "Invalid uniform buffer slot %i", data.slot);
        impl.state.bindings.uniformBuffers[data.slot] = data.buffer;
    }
    
    void BindUniformInt( CompilerImpl &impl, const CQI::BindUniformIntData &data ) 
    {
        FATAL_ASSERT(data.location >= 0 && data.location < MAX_BOUND_UNIFORMS, "Invalid uniform location %i", data.location);
        
        auto &uniform = impl.state.bindings.uniforms[data.location];
        uniform.type = GLType::Int;
        uniform.data.i[0] = data.value;
    }

    void BindUniformUInt( CompilerImpl &impl, const CQI::BindUniformUIntData &data ) 
    {
        FATAL_ASSERT(data.location >= 0 && data.location < MAX_BOUND_UNIFORMS, "Invalid uniform location %i", data.location);
        
        auto &uniform = impl.state.bindings.uniforms[data.location];
        uniform.type = GLType::UInt;
        uniform.data.u[0] = data.value;
    }
    
    void BindUniformFloat( CompilerImpl &impl, const CQI::BindUniformFloatData &data ) 
    {
        FATAL_ASSERT(data.location >= 0 && data.location < MAX_BOUND_UNIFORMS, "Invalid uniform location %i", data.location);
        
        auto &uniform = impl.state.bindings.uniforms[data.location];
        uniform.type = GLType::Float;
        uniform.data.f[0] = data.value;
    }

    void BindUniformVec2( CompilerImpl &impl, const CQI::BindUniformVec2Data &data ) 
    {
        FATAL_ASSERT(data.location >= 0 && data.location < MAX_BOUND_UNIFORMS, "Invalid uniform location %i", data.location);
        
        auto &uniform = impl.state.bindings.uniforms[data.location];
        uniform.type = GLType::Vec2;
        std::copy(std::begin(data.vec), std::end(data.vec), uniform.data.f);
    }

    void BindUniformVec3( CompilerImpl &impl, const CQI::BindUniformVec3Data &data ) 
    {
        FATAL_ASSERT(data.location >= 0 && data.location < MAX_BOUND_UNIFORMS, "Invalid uniform location %i", data.location);
        
        auto &uniform = impl.state.bindings.uniforms[data.location];
        uniform.type = GLType::Vec3;
        std::copy(std::begin(data.vec), std::end(data.vec), uniform.data.f);
    }

    void BindUniformVec4( CompilerImpl &impl, const CQI::BindUniformVec4Data &data ) 
    {
        FATAL_ASSERT(data.location >= 0 && data.location < MAX_BOUND_UNIFORMS, "Invalid uniform location %i", data.location);
        
        auto &uniform = impl.state.bindings.uniforms[data.location];
        uniform.type = GLType::Vec4;
        std::copy(std::begin(data.vec), std::end(data.vec), uniform.data.f);
    }

    void BindUniformMat4( CompilerImpl &impl, const CQI::BindUniformMat4Data &data ) 
    {
        FATAL_ASSERT(data.location >= 0 && data.location < MAX_BOUND_UNIFORMS, "Invalid uniform location %i", data.location);
        
        auto &uniform = impl.state.bindings.uniforms[data.location];
        uniform.type = GLType::Mat4x4;
        std::copy(std::begin(data.matrix), std::end(data.matrix), uniform.data.f);
    }

    void BeginTransformFeedback( CompilerImpl &impl, const CQI::BeginTransformFeedbackData &data )
    {
        FATAL_ASSERT(impl.transformProgramInfo == nullptr, "Already in transform feedback mode!");

        HRMI::BufferInfo *buffer = impl.hardwareMgr->buffers.find(data.buffer);
        if (!buffer) {
            LOG_ERROR("Failed to bind transform feedback buffer %i", (int)data.buffer);
            return;
        }

        if (data.size == 0 || 
            data.offset >= buffer->size || 
            (data.offset + data.size) > buffer->size) 
        {
            LOG_ERROR("Invalid range for transform feedback buffer %i - offset: %zu    size: %zu    buffer size: %zu", 
                (int)data.buffer, data.offset, data.size, buffer->size        
            );
            return;
        }

        PMI::TransformProgramInfo *programInfo = impl.pipelineMgr->transformPrograms.find(data.program);
        if (!programInfo) {
            LOG_ERROR("Faield to bind transform feedback program %i", (int)data.program);
            return;
        }

        impl.programInfo = programInfo;
        impl.renderProgramInfo = nullptr;
        impl.computeProgramInfo = nullptr;
        impl.transformProgramInfo = programInfo;

        impl.onProgramChanged();

        impl.current.pipeline = {};

        Emit(impl, CCQI::BindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, buffer->glBuffer, data.offset, data.size));
        Emit(impl, CCQI::SetProgram(programInfo->glProgram));
        Emit(impl, CCQI::Enable(GL_RASTERIZER_DISCARD));
        Emit(impl, CCQI::BeginTransformFeedback(ToGL(data.primitive)));
    }

    void EndTransformFeedback( CompilerImpl &impl, const CQI::EndTransformFeedbackData &data )
    {
        FATAL_ASSERT(impl.transformProgramInfo != nullptr, "Not in transform feedback mode!");
        Emit(impl, CCQI::EndTransformFeedback());
        Emit(impl, CCQI::Disable(GL_RASTERIZER_DISCARD));

        assert(impl.programInfo == impl.transformProgramInfo);
        impl.programInfo = nullptr;
        impl.transformProgramInfo = nullptr;
    }

    void CopyBuffer( CompilerImpl &impl, const CQI::CopyBufferData &data )
    {
        const HRMI::BufferInfo *source = impl.hardwareMgr->buffers.find(data.source);
        const HRMI::BufferInfo *target = impl.hardwareMgr->buffers.find(data.target);

        if (!source) {
            LOG_ERROR("Failed to find source buffer %i for buffer copy.", (int)data.source);
            return;
        }
        if (!target) {
            LOG_ERROR("Failed to find target buffer %i for buffer copy.", (int)data.target);
            return;
        }

        if (source->size < (data.sourceOffset + data.size)) {
            LOG_ERROR("Invalid offset/size for source buffer %i (its size is %zu) offset = %zu, size = %zu", 
                (int)data.source, source->size, data.sourceOffset, data.size
            );
            return;
        }
        
        if (target->size < (data.targetOffset + data.size)) {
            LOG_ERROR("Invalid offset/size for target buffer %i (its size is %zu) offset = %zu, size = %zu", 
                (int)data.target, target->size, data.sourceOffset, data.size
            );
            return;
        }

        Emit(impl, CCQI::BindBuffer(gl::GL_COPY_READ_BUFFER, source->glBuffer));
        Emit(impl, CCQI::BindBuffer(gl::GL_COPY_WRITE_BUFFER, target->glBuffer));
        Emit(impl, CCQI::CopyBufferSubData(gl::GL_COPY_READ_BUFFER, gl::GL_COPY_WRITE_BUFFER, data.sourceOffset, data.targetOffset, data.size));
    }

    void resetState( CompilerImpl &impl )
    {
        State state;

        if (impl.current.clipping != state.clipping) {
            EmitEnableDisable(impl, impl.current.clipping, state.clipping, GL_SCISSOR_TEST);
            impl.current.clipping = state.clipping;
        }
        if (impl.current.depthTest != state.depthTest) {
            EmitEnableDisable(impl, impl.current.depthTest, state.depthTest, GL_DEPTH_TEST);
            impl.current.depthTest = state.depthTest;
        }
        if (impl.current.faceCulling != state.faceCulling) {
            EmitEnableDisable(impl, impl.current.faceCulling, state.faceCulling, GL_CULL_FACE);
            impl.current.faceCulling = state.faceCulling;
        }
        if (impl.current.primitiveRestart != state.primitiveRestart) {
            EmitEnableDisable(impl, impl.current.primitiveRestart, state.primitiveRestart, GL_PRIMITIVE_RESTART);
        }
        if (impl.current.depthWrite != state.depthWrite) {
            Emit(impl, CCQI::SetDepthMask(state.depthTest));
            impl.current.depthWrite = state.depthWrite;
        }
        if (impl.current.blendMode != state.blendMode) {
            EmitBlendMode(impl, state.blendMode);
        }
    }

    std::vector<CompiledRenderQueueImpl::Command> Compile( Context *context, const RenderQueueCompileOptions &options, const RenderCommandQueuePtr &queue )
    {
        CompilerImpl impl;
        impl.init(context, options);


        const auto &commands = queue->impl()->commands;

        using CommandType = RenderCommandQueueImpl::Type;

        for (const auto &command : commands) {
            impl.currentCommand++;
            switch (command.type) {
            case CommandType::Draw:
                EmitDraw(impl, command.draw);
                break;
            case CommandType::DrawBuiltin:
                EmitDrawBuiltin(impl, command.drawBuiltin);
                break;
            case CommandType::Clear:
                EmitClear(impl, command.clear);
                break;
            case CommandType::ExecuteCompute:
                EmitExecuteCompute(impl, command.executeCompute);
                break;
            case CommandType::UsePipeline:
                UsePipeline(impl, command.usePipeline);
                break;
            case CommandType::UseVertexArray:
                UseVertexArray(impl, command.useVertexArray);
                break;
            case CommandType::UseClipping:
                UseClipping(impl, command.useClipping);
                break;
            case CommandType::UseClipRect:
                UseClipRect(impl, command.useClipRect);
                break;
            case CommandType::BindSampler:
                BindSampler(impl, command.bindSampler);
                break;
            case CommandType::BindBuiltinTexture:
                BindBuiltinTexture(impl, command.bindBuiltinTexture);
                break;
            case CommandType::BindImageTexture:
                BindImageTexture(impl, command.bindImageTexture);
                break;
            case CommandType::BindUniformBuffer:
                BindUniformBuffer(impl, command.bindUniformBuffer);
                break;
            case CommandType::BindUniformInt:
                BindUniformInt(impl, command.bindUniformInt);
                break;
            case CommandType::BindUniformUInt:
                BindUniformUInt(impl, command.bindUniformUInt);
                break;
            case CommandType::BindUniformFloat:
                BindUniformFloat(impl, command.bindUniformFloat);
                break;
            case CommandType::BindUniformVec2:
                BindUniformVec2(impl, command.bindUniformVec2);
                break;
            case CommandType::BindUniformVec3:
                BindUniformVec3(impl, command.bindUniformVec3);
                break;
            case CommandType::BindUniformVec4:
                BindUniformVec4(impl, command.bindUniformVec4);
                break;
            case CommandType::BindUniformMat4:
                BindUniformMat4(impl, command.bindUniformMat4);
                break;
            case CommandType::BeginTransformFeedback:
                BeginTransformFeedback(impl, command.beginTransformFeedback);
                break;
            case CommandType::EndTransformFeedback:
                EndTransformFeedback(impl, command.endTransformFeedback);
                break;
            case CommandType::CopyBuffer:
                CopyBuffer(impl, command.copyBuffer);
                break;
            }
        }

        if (impl.transformProgramInfo) {
            EndTransformFeedback(impl, CQI::EndTransformFeedback());
        }

        resetState(impl);
        return std::move(impl.commands);
    }
}
