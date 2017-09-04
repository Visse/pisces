#include "NkSprite.h"
#include "Sprite.h"

#include "nuklear.h"

PISCES_API struct nk_image nk_image_sprite( const Pisces::Sprite &sprite )
{
    unsigned short h = std::numeric_limits<unsigned short>::max(),
                   w = std::numeric_limits<unsigned short>::max();
    return nk_subimage_id(
        sprite.texture.handle,
        w, h, 
        nk_rect(sprite.uv.x1*w, sprite.uv.y2*h, (sprite.uv.x2-sprite.uv.x1) * w, (sprite.uv.y1-sprite.uv.y2)*h)
    );
}

PISCES_API int nk_button_sprite( nk_context *ctx, const Pisces::Sprite &sprite )
{
    return nk_button_image(
        ctx,
        nk_image_sprite(sprite)
    );
}
