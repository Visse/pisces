#pragma once


#if PISCES_BUILT_SHARED
#   if PISCES_BUILDING
#       define PISCES_API __declspec(dllexport)
#   else 
#       define PISCES_API __declspec(dllimport)
#   endif // BUILDING_COMMON
#else // Static build
#   define COMMON_API
#endif // BUILD_COMMON_SHARED