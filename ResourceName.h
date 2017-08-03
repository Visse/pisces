#include "Fwd.h"
#include "Common/Murmur3_32.h"

namespace Pisces
{ 
    static const uint32_t RESOURCE_NAME_SEED = 708175590; // random number

    inline ResourceName StringToResourceName( const char *str )
    {
        return (ResourceName) Common::murmur3_32(str, (uint32_t)std::strlen(str), RESOURCE_NAME_SEED);
    }
}