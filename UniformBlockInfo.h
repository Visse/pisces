#pragma once

#include "Fwd.h"
#include "build_config.h"

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
        const char *name = nullptr;
        UniformBlockMemberType type = UniformBlockMemberType(-1);
        size_t offset = 0;
    };

    struct UniformBlockInfo {
        const char *name = nullptr;
        size_t size=0, hash=0;
        std::initializer_list<UniformBlockMember> members;
    };

    template< typename Type >
    const UniformBlockInfo* _getUniformBlockInfo(const Type*)
    {
        //static_assert( std::is_same<Type,Type>::value, "Type haven't been declared a uniform block!" );
        return nullptr;s
    }
    
    template< typename Type >
    const UniformBlockInfo* getUniformBlockInfo() {
        return _getUniformBlockInfo((Type*)0);
    }

    template< typename Type >
    struct UniformBlockTypeInfo {
        const UniformBlockInfo *info = getUniformBlockInfo<Type>();
    };

    PISCES_API bool verifyUniformBlocksInProgram( ProgramHandle program );
    
    // Internal helper function
    namespace PipelineManagerImpl
    {
        struct BaseProgramInfo;
        bool verifyUniformBlocksInProgram( BaseProgramInfo *program );

        size_t hashUniformBlock( const BaseProgramInfo *program, int uniformBlockIndex );
    }

    namespace impl 
    {
        PISCES_API bool registerUniformBlock( const UniformBlockInfo &info );

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

        static constexpr size_t hashCombine( size_t a, size_t b )
        {
            return a ^ (b + 0x9e3779b97f4a7c16 + (a<<6) + (a>>2));
        }

        static constexpr size_t hash( const UniformBlockMember &member )
        {
            return hashCombine(size_t(member.type), member.offset);
        }

        template< typename Type >
        static constexpr size_t hashMembers( const Type &type )
        {
            size_t res = 0;
            for (const UniformBlockMember &member : type) {
                // Note add is used to be order independent
                //     ('order' is already enforced by using offset)
                res += hash(member);
            }
            return res;
        }
    }
}

#ifdef PISCES_NO_REGISTER_UNIFORM_BLOCK
#   define PISCES_DECLARE_UNIFORM_BLOCK( Type, ... )
#   define PISCES_REGISTER_UNIFORM_BLOCK( Type )
#else
#   include "Common/PPUtils.h"

#   define _PISCES_REGISTER_UNIFORM_BEGIN( Type, N )                                                                    \
        struct _##Type##_uniform_block_info {                                                                           \
            const char *name = #Type;                                                                                   \
            const ::Pisces::UniformBlockMember members[N] = {

#   define _PISCES_REGISTER_UNIFORM_END(Type)                                                                           \
            };                                                                                                          \
        };                                                                                                              \
        const ::Pisces::UniformBlockInfo* _getUniformBlockInfo(const Type*);

#   define _PISCES_REGISTER_UNIFORM_MEMBER(Type, var)                                                                   \
        {#var, ::Pisces::impl::MemberType<decltype(Type::var)>::type, offsetof(Type,var)},

#   define PISCES_DECLARE_UNIFORM_BLOCK(Type, ...)                                                                      \
        _PISCES_REGISTER_UNIFORM_BEGIN(Type, PP_UTILS_NARGS(__VA_ARGS__))                                               \
           PP_UTILS_MAP(Type, _PISCES_REGISTER_UNIFORM_MEMBER, __VA_ARGS__)                                             \
        _PISCES_REGISTER_UNIFORM_END(Type)

#   define PISCES_REGISTER_UNIFORM_BLOCK(Type)                                                                          \
        const ::Pisces::UniformBlockInfo* _##Type##_getUniformBlockInfo() {                                             \
            static const _##Type##_uniform_block_info type;                                                             \
            static const ::Pisces::UniformBlockInfo info {                                                              \
                #Type, sizeof(Type), ::Pisces::impl::hashMembers(type.members),                                         \
                std::initializer_list<::Pisces::UniformBlockMember>(std::begin(type.members), std::end(type.members))   \
            };                                                                                                          \
            return &info;                                                                                               \
        }                                                                                                               \
        const ::Pisces::UniformBlockInfo* _getUniformBlockInfo( const Type* ) {                                         \
            return _##Type##_getUniformBlockInfo();                                                                     \
        }                                                                                                               \
        static const bool _##Type##_uniform_block_registred = ::Pisces::impl::registerUniformBlock(*_##Type##_getUniformBlockInfo());
        

#endif
