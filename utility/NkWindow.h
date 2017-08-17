#pragma once

#include "Pisces/build_config.h"

#include <glm/vec2.hpp>

struct nk_context;

namespace Pisces
{
    class NkWindow {
    public:
        enum WindowPosition {
            TopLeft,
            Top,
            TopRight,

            Left, 
            Center,
            Right,

            BottomLeft,
            Bottom,
            BottomRight
        };
    public:
        PISCES_API NkWindow( nk_context *ctx, const char *title, WindowPosition position, glm::vec2 size, unsigned flags=0 );
        PISCES_API NkWindow( nk_context *ctx, const char *title, glm::vec2 position, glm::vec2 size, unsigned flags=0 );
        PISCES_API ~NkWindow();

        NkWindow() = delete;
        NkWindow( const NkWindow& ) = delete;

        NkWindow& operator = ( const NkWindow& ) = delete;

        operator bool () {
            return mIsOpen;
        }

    private:
        nk_context *mCtx;
        bool mIsOpen;
    };
}