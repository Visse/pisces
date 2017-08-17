#pragma once

#include "Pisces/Fwd.h"
#include "Pisces/build_config.h"

#include "Common/PImplHelper.h"

struct nk_context;
typedef union SDL_Event SDL_Event;


namespace Pisces
{
    class NkContext {
    public:
        PISCES_API NkContext( Context *ctx );
        PISCES_API ~NkContext();

        PISCES_API operator nk_context* ();
        PISCES_API nk_context* nkContext();
        
        PISCES_API void beginFrame( float dt );
        PISCES_API void endFrame();

        PISCES_API void beginInput();
        PISCES_API bool injectInputEvent( const SDL_Event &event );
        PISCES_API void endInput();

        PISCES_API void render();

    private:
        struct Impl;
        PImplHelper<Impl, 256> mImpl;
    };
}