#pragma once


#include "Pisces/Fwd.h"
#include "Pisces/build_config.h"

struct nk_context;

PISCES_API struct nk_image nk_image_sprite( const Pisces::Sprite &sprite );
PISCES_API int nk_button_sprite( nk_context *ctx, const Pisces::Sprite &sprite );

