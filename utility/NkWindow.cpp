#include "NkWindow.h"

#include "nuklear.h"

namespace Pisces
{
    glm::vec2 CalcPosition( nk_context *ctx, NkWindow::WindowPosition position, glm::vec2 size )
    {
        glm::vec2 screenSize(ctx->screen_size.x, ctx->screen_size.y);

        using WP = NkWindow::WindowPosition;
        glm::vec2 pos;
        
        switch (position) {
        case WP::TopLeft:
        case WP::Top:
        case WP::TopRight:
            pos.y = 0;
            break;
        case WP::Left:
        case WP::Center:
        case WP::Right:
            pos.y = (screenSize.y - size.y)/2.f;
            break;
        case WP::BottomLeft:
        case WP::Bottom:
        case WP::BottomRight:
            pos.y = screenSize.y - size.y;
            break;
        }
        switch (position) {
        case WP::TopLeft:
        case WP::Left:
        case WP::BottomLeft:
            pos.x = 0;
            break;
        case WP::Top:
        case WP::Center:
        case WP::Bottom:
            pos.x = (screenSize.x - size.x)/2.f;
            break;
        case WP::TopRight:
        case WP::Right:
        case WP::BottomRight:
            pos.x = screenSize.x - size.x;
            break;
        }
        return pos;
    }

    PISCES_API NkWindow::NkWindow( nk_context *ctx, const char *title, WindowPosition position, glm::vec2 size, unsigned flags ) :
        NkWindow(ctx, title, CalcPosition(ctx, position, size), size, flags)
    {}

    PISCES_API NkWindow::NkWindow( nk_context *ctx, const char *title, glm::vec2 position, glm::vec2 size, unsigned flags ) :
        mCtx(ctx)
    {
        mIsOpen = nk_begin(ctx, title, nk_rect(position.x, position.y, size.x, size.y), (nk_flags)flags) == 1;
    }

    PISCES_API NkWindow::~NkWindow()
    {
        nk_end(mCtx);
    }
}