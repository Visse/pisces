#include "Fwd.h"

#include "Common/StringEqual.h"

#include <type_traits>

#define CHECK(Val, Str)                                                                                 \
    if (StringUtils::equal(Str, std::strlen(Str), str, len, StringUtils::EqualFlags::IgnoreCase)) {     \
        value = std::remove_reference<decltype(value)>::type::Val;                                      \
        return true;                                                                                    \
    }

#define CHECK2(Val) CHECK(Val, #Val)

namespace Pisces
{
    bool PixelFormatFromString( const char *str, size_t len, PixelFormat &value )
    {
        CHECK2(R8);
        CHECK2(RG8);
        CHECK2(RGB8);
        CHECK2(RGBA8);
        CHECK(R8, "r");
        CHECK(RG8, "rg");
        CHECK(RGB8, "rgb");
        CHECK(RGBA8, "rgba");

        return false;
    }

    bool BlendModeFromString( const char *str, BlendMode &mode ) {
        return BlendModeFromString(str, strlen(str), mode);
    }

    bool BlendModeFromString( const char *str, size_t len, BlendMode &value )
    {
        CHECK2(Replace);
        CHECK2(Alpha);
        CHECK2(PreMultipledAlpha);
        CHECK(PreMultipledAlpha, "premultiplied_alpha");

        return false;
    }

    bool SamplerMinFilterFromString( const char *str, size_t len, SamplerMinFilter &value )
    {
        CHECK2(Nearest);
        CHECK2(Linear);
        CHECK2(NearestMipmapNearest);
        CHECK2(LinearMipmapNearest);
        CHECK2(NearestMipmapLinear);
        CHECK2(LinearMipmapLinear);

        CHECK(NearestMipmapNearest, "nearest_mipmap_nearest");
        CHECK(LinearMipmapNearest, "linear_mipmap_nearest");
        CHECK(NearestMipmapLinear, "nearest_mipmap_linear");
        CHECK(LinearMipmapLinear, "linear_mipmap_linear");
        
        return false;
    }

    bool SamplerMagFilterFromString( const char *str, size_t len,  SamplerMagFilter &value )
    {
        CHECK2(Nearest);
        CHECK2(Linear);

        return false;
    }
}