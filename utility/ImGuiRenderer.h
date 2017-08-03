#pragma once

#include "Fwd.h"
#include "build_config.h"

#include "Common/PImplHelper.h"

class ImGuiRenderer {
public:
    PISCES_API ImGuiRenderer( Pisces::Context *context );
    PISCES_API ~ImGuiRenderer();
    
    PISCES_API void render();

private:
    struct Impl;
    PImplHelper<Impl, 512> mImpl;

    friend void imguiRender( ImGuiRenderer::Impl *impl );
};