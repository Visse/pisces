#pragma once

#include "Fwd.h"
#include "build_config.h"

#include "Common/PImplHelper.h"

#include <functional>

namespace Pisces
{
    using DisplayResizedCallback = std::function<void(int width, int height)>;

    enum HardwareLimitName {
        TextureUnits
    };

    class Context {
    public:
        struct InitParams {
            const char *windowTitle = "Window";
            int displayWidth = 1080,
                displayHeight = 720;

            bool enableExtensions = true;
        };

    public:
        static PISCES_API Context* Initilize( const InitParams &params );
        static PISCES_API void Shutdown();

        static PISCES_API  Context* GetContext();

    private:
        PISCES_API Context( const InitParams &params );

        Context( const Context& ) = delete;
        Context& operator = ( const Context& ) = delete;

    public:
        PISCES_API HardwareResourceManager* getHardwareResourceManager();
        PISCES_API PipelineManager* getPipelineManager();
        PISCES_API RenderTargetHandle getMainRenderTarget();

        PISCES_API RenderCommandQueuePtr createRenderCommandQueue( RenderTargetHandle target=RenderTargetHandle(), RenderCommandQueueFlags flags=RenderCommandQueueFlags::None );

        PISCES_API CompiledRenderQueuePtr compile( const RenderCommandQueuePtr &queue, const RenderQueueCompileOptions &options=RenderQueueCompileOptions() );

        PISCES_API void execute( const RenderCommandQueuePtr &queue );
        PISCES_API void execute( const CompiledRenderQueuePtr &queue );
        PISCES_API void swapFrameBuffer();

        PISCES_API uint64_t currentFrame();

        PISCES_API int displayWidth();
        PISCES_API int displayHeight();

        PISCES_API void addDisplayResizedCallback( DisplayResizedCallback callback, const void *id=nullptr );
        PISCES_API void removeCallback( const void *id );

        PISCES_API void toogleFullscreen();
    
        PISCES_API void clearMainRenderTarget();

        PISCES_API int getHardwareLimit( HardwareLimitName limit );

    private:
        struct Impl;
        PImplHelper<Impl,1024> mImpl;
    };
}