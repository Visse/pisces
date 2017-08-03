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
             depthTest = false,
             faceCulling = false,
             depthWrite = false;
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
            else if(lastUsed == -1 || impl.textureUnits[i].lastUsed < lastUsed) {
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

    bool EmitBindPipeline( CompilerImpl &impl, PipelineHandle handle )
    {
        if (handle && impl.current.pipeline == handle) return true;

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
            switch (pipeline->blendMode) {
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
            LOG_WARNING("Missmatch between bound texture type (%i) and expected type (%i) for gl-program %i", (int)unit.type, (int)impl.programInfo->samplers[i].type, (int)impl.programInfo->glProgram);
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

        Emit(impl, CCQI::BindUniform(loc, buffer->glBuffer, handle.offset, handle.size));
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
        if (impl.current.bindings.uniforms[i] == uniform) return true;
        if (uniform.type == GLTypeNone) return true;

        assert (i >= 0 && i < MAX_BOUND_UNIFORMS);

        int loc = impl.programInfo->uniforms[i].location;
        if (impl.programInfo->uniforms[i].type != uniform.type) {
            LOG_WARNING("Missmatch between uniform binding and program, expected type %i, got %i", (int)impl.programInfo->uniforms[i].type, (int)uniform.type);
        }

        switch(uniform.type) {
        case GLType::Float:
        case GLType::Vec2:
        case GLType::Vec3:
        case GLType::Vec4:
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

            Emit(impl, CCQI::BindIndexBuffer(indexBuffer->glBuffer));
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

        assert (impl.renderProgramInfo);
        if (!EmitBindResources(impl, impl.renderProgramInfo, state.bindings)) {
            LOG_ERROR("Failed to bind resources!");
            return;
        }

        assert(impl.pipelineInfo);
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
        FATAL_ASSERT(data.slot >= 0 && data.slot < MAX_BOUND_IMAGE_TEXTURES, "Invalid uniform buffer slot %i", data.slot);
        impl.state.bindings.uniformBuffers[data.slot] = data.buffer;
    }

    void BindUniformMat4( CompilerImpl &impl, const CQI::BindUniformMat4Data &data ) 
    {
        FATAL_ASSERT(data.location >= 0 && data.location < MAX_BOUND_UNIFORMS, "Invalid uniform location %i", data.location);
        
        auto &uniform = impl.state.bindings.uniforms[data.location];
        uniform.type = GLType::Mat4x4;
        std::copy(std::begin(data.matrix), std::end(data.matrix), uniform.data.f);
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
            case CommandType::BindImageTexture:
                BindImageTexture(impl, command.bindImageTexture);
                break;
            case CommandType::BindUniformBuffer:
                BindUniformBuffer(impl, command.bindUniformBuffer);
                break;
            case CommandType::BindUniformMat4:
                BindUniformMat4(impl, command.bindUniformMat4);
                break;
            }
        }

        return std::move(impl.commands);
    }
}
