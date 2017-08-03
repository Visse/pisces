#pragma once

#include "Fwd.h"
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
        static Context* Initilize( const InitParams &params );
        static void Shutdown();

        static Context* GetContext();

    private:
        Context( const InitParams &params );

        Context( const Context& ) = delete;
        Context& operator = ( const Context& ) = delete;

    public:
        HardwareResourceManager* getHardwareResourceManager();
        PipelineManager* getPipelineManager();
        RenderTargetHandle getMainRenderTarget();

        RenderCommandQueuePtr createRenderCommandQueue( RenderTargetHandle target=RenderTargetHandle(), RenderCommandQueueFlags flags=RenderCommandQueueFlags::None );

        CompiledRenderQueuePtr compile( const RenderCommandQueuePtr &queue, const RenderQueueCompileOptions &options=RenderQueueCompileOptions() );

        void execute( const RenderCommandQueuePtr &queue );
        void execute( const CompiledRenderQueuePtr &queue );
        void swapFrameBuffer();

        uint64_t currentFrame();

        int displayWidth();
        int displayHeight();

        void addDisplayResizedCallback( DisplayResizedCallback callback, const void *id=nullptr );
        void removeCallback( const void *id );

        void toogleFullscreen();
    
        void clearMainRenderTarget();

        int getHardwareLimit( HardwareLimitName limit );

    private:
        struct Impl;
        PImplHelper<Impl,1024> mImpl;
    };
}