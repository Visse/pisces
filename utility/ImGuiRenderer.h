#pragma once

#include "Fwd.h"
#include "Pisces/build_config.h"

#include "Common/PImplHelper.h"

typedef union SDL_Event SDL_Event;

namespace Pisces
{
    class ImGuiRenderer {
    public:
        PISCES_API ImGuiRenderer( Pisces::Context *context );
        PISCES_API ~ImGuiRenderer();
    
        PISCES_API void render();

        PISCES_API bool injectEvent( const SDL_Event &event );

    private:
        struct Impl;
        PImplHelper<Impl, 512> mImpl;

        friend void imguiRender( ImGuiRenderer::Impl *impl );
    };
}