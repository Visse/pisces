#pragma once

#include "Fwd.h"

#include <glm/fwd.hpp>
#include <initializer_list>


namespace Pisces
{
    enum class UniformBlockMemberType {
        Float,
        Vec2, Vec3, Vec4,
        Mat4
    };

    struct UniformBlockMember {
        const char *name;
        UniformBlockMemberType type;
        int offset;
    };

    struct UniformBlockInfo {
        const char *name;
        std::initializer_list<UniformBlockMember> members;
    };

    bool verifyUniformBlocksInProgram( ProgramHandle program );
    bool verifyUniformBlocksInProgram( unsigned int program );

    namespace impl 
    {
        bool registerUniformBlock( const UniformBlockInfo &info );

        template< typename Type >
        struct MemberType {
            static_assert( std::is_same<Type,Type>::value, "Unknown Type!" );
        };

        template<>
        struct MemberType<float> {
            static const UniformBlockMemberType type = UniformBlockMemberType::Float;
        };
        template<>
        struct MemberType<glm::vec2> {
            static const UniformBlockMemberType type = UniformBlockMemberType::Vec2;
        };
        template<>
        struct MemberType<glm::vec3> {
            static const UniformBlockMemberType type = UniformBlockMemberType::Vec3;
        };
        template<>
        struct MemberType<glm::vec4> {
            static const UniformBlockMemberType type = UniformBlockMemberType::Vec4;
        };
        template<>
        struct MemberType<glm::mat4> {
            static const UniformBlockMemberType type = UniformBlockMemberType::Mat4;
        };
    }

#ifdef PISCES_NO_REGISTER_UNIFORM_BLOCK
#   define REGISTER_UNIFORM_BLOCK( Type, ... )
#elif !defined(PISCES_REGISTER_UNIFORM_REG)
#   define REGISTER_UNIFORM_BLOCK(...)
#else
#   include "Common/PPUtils.h"

#   define _REGISTER_UNIFORM_BEGIN( Type )                                      \
        const std::initializer_list<UniformBlockMember> _##Type##_members = {

#   define _REGISTER_UNIFORM_END(Type)                                          \
        };                                                                      \
        const UniformBlockInfo _##Type##_info = {#Type, _##Type##_members};     \
        static const bool _##Type##_registred = impl::registerUniformBlock(_##Type##_info);

#   define _REGISTER_UNIFORM_MEMBER(Type, var)                                  \
        {#var, impl::MemberType<decltype(Type::var)>::type, offsetof(Type,var)},


#   define REGISTER_UNIFORM_BLOCK(Type, ... )                                               \
        _REGISTER_UNIFORM_BEGIN(Type)                                                       \
           PP_UTILS_MAP(Type, _REGISTER_UNIFORM_MEMBER, __VA_ARGS__)    \
        _REGISTER_UNIFORM_END(Type)

#endif
}
