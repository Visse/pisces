#include "UniformBlockInfo.h"
#include "Context.h"
#include "PipelineManager.h"
#include "internal/PipelineManagerImpl.h"

#include "Common/ErrorUtils.h"
#include "Common/StringEqual.h"

#include <unordered_map>

#include <glbinding/gl33core/gl.h>
using namespace gl33core;

#include <glbinding/Meta.h>

namespace Pisces
{
    namespace 
    {
        size_t fnv1a( const char *str, size_t len ) {
            size_t res = 14695981039346656037u;
            for (size_t i=0; i < len; ++i) {
                res = (res ^ str[i]) * 1099511628211u;
            }
            return res;
        }

        struct StrEqual {
            bool operator () (const char *str1, const char *str2) const {
                return std::strcmp(str1,str2) == 0;
            }
        };

        struct StrHash {
            size_t operator () (const char *str) const {
                size_t len = std::strlen(str);
                return fnv1a(str,len);
            }
        };

        using UniformBlockMap = std::unordered_multimap<const char*, UniformBlockInfo, StrHash, StrEqual>;

        UniformBlockMap& RegistredUniformBlocks() {
            static UniformBlockMap map;
            return map;
        }
    }

    GLenum ToGL( UniformBlockMemberType type ) {
        switch (type) {
        case UniformBlockMemberType::Float:
            return GL_FLOAT;
        case UniformBlockMemberType::Vec2:
            return GL_FLOAT_VEC2;
        case UniformBlockMemberType::Vec3:
            return GL_FLOAT_VEC3;
        case UniformBlockMemberType::Vec4:
            return GL_FLOAT_VEC4;
        case UniformBlockMemberType::Mat4:
            return GL_FLOAT_MAT4;
        }
        FATAL_ERROR("Unknown UniformBlockMemberType %i", (int)type);
    }

    UniformBlockMemberType FromGL( GLenum type ) {
        switch (type) {
        case GL_FLOAT:
            return UniformBlockMemberType::Float;
        case GL_FLOAT_VEC2:
            return UniformBlockMemberType::Vec2;
        case GL_FLOAT_VEC3:
            return UniformBlockMemberType::Vec3;
        case GL_FLOAT_VEC4:
            return UniformBlockMemberType::Vec4;
        case GL_FLOAT_MAT4:
            return UniformBlockMemberType::Mat4;
        default:
            return UniformBlockMemberType(-1);
        }
    }

    PISCES_API bool verifyUniformBlocksInProgram( ProgramHandle handle )
    {
        PipelineManagerImpl::Impl *pipelineMgr = Context::GetContext()->getPipelineManager()->impl();

        PipelineManagerImpl::RenderProgramInfo *programInfo = pipelineMgr->renderPrograms.find(handle);
        if (programInfo == nullptr) return true;

        return PipelineManagerImpl::verifyUniformBlocksInProgram(programInfo);
    }

    const UniformBlockMember* findMemberAtOffset( const UniformBlockInfo &info, size_t offset )
    {
        for (const auto &member : info.members) {
            if (member.offset == offset) return &member;
        }
        return nullptr;
    }
    const UniformBlockMember* findMemberWithName( const UniformBlockInfo &info, const char *name )
    {
        for (const auto &member : info.members) {
            if (StringUtils::equal(name, member.name, StringUtils::EqualFlags::IgnoreCase)) return &member;
        }
        return nullptr;
    }


    enum struct ErrorType {
        MemberMissing,
        OffsetMissmatch,
        TypeMissmatch,
        NameMissmatch,
        NotAllMembersExist,
    };

    struct Error {
        ErrorType type;
        int member;

        static Error MemberMissing( int member ) {
            return Error{ErrorType::MemberMissing, member};
        }
        static Error OffsetMissmatch( int member ) {
            return Error{ErrorType::OffsetMissmatch, member};
        }
        static Error TypeMissmatch( int member ) {
            return Error{ErrorType::TypeMissmatch, member};
        }
        static Error NameMissmatch( int member ) {
            return Error{ErrorType::NameMissmatch, member};
        }
        static Error NotAllMembersExist() {
            return Error{ErrorType::NotAllMembersExist, -1};
        }
    };
    static const int MAX_ERRORS = 4;
    struct ErrorContext {
        int errorCount = 0;
        Error errors[MAX_ERRORS];

        void pushError( Error error ) {
            if (errorCount < MAX_ERRORS) {
                errors[errorCount] = error;
            }
            errorCount++;
        }

        operator bool () {
            return errorCount > 0;
        }

        friend bool operator < ( const ErrorContext &lhs, const ErrorContext &rhs )
        {
            return lhs.errorCount < rhs.errorCount;
        }
    };
    
    const int MAX_NAME_LENGHT = 256;
    const int MAX_MEMBER_COUNT = 32;

    void checkMatch( ErrorContext &ctx, const UniformBlockInfo &info, int count, GLuint *members, GLint *types, GLint *offsets, const char (&names)[MAX_MEMBER_COUNT][MAX_NAME_LENGHT] )
    {
        if (count < info.members.size()) {
            ctx.pushError(Error::NotAllMembersExist());
        }

        for (int i=0; i < count; ++i) {
            // First check if we have a name match
            const UniformBlockMember *member = findMemberWithName(info, names[i]);

            bool foundByName = member != nullptr;
            if (member) {
                // check offset
                if (offsets[i] != member->offset) {
                    ctx.pushError(Error::OffsetMissmatch(i));
                    continue;
                }
            }
            else {
                // Fallback on offset
                member = findMemberAtOffset(info, offsets[i]);
                
                if (!member) {
                    ctx.pushError(Error::MemberMissing(i));
                    continue;
                }
            }

            if (ToGL(member->type) != types[i]) {
                ctx.pushError(Error::TypeMissmatch(i));
                continue;
            }

            if (!foundByName) {
                // If type and offset match - warn about a name missmatch (it could mean that the variable changed meaning)
                ctx.pushError(Error::NameMissmatch(i));
            }
        }
    }
    

    bool PipelineManagerImpl::verifyUniformBlocksInProgram( PipelineManagerImpl::BaseProgramInfo *program )
    {
        GLuint glProgram = program->glProgram;

        GLint uniformBlockCount = 0;

        GLint nameLenght = 0;
        char name[MAX_NAME_LENGHT],
             memberNames[MAX_MEMBER_COUNT][MAX_NAME_LENGHT];
        GLuint members[MAX_MEMBER_COUNT];

        GLint types[MAX_MEMBER_COUNT];
        GLint offsets[MAX_MEMBER_COUNT];


        glGetProgramiv(glProgram, GL_ACTIVE_UNIFORM_BLOCKS, &uniformBlockCount);

        bool wasErrors = false;

        for (int i=0; i < uniformBlockCount; ++i) {
            glGetActiveUniformBlockName(glProgram, i, MAX_NAME_LENGHT, &nameLenght, name);

            if (nameLenght > MAX_NAME_LENGHT) {
                LOG_WARNING("Too long uniform block name (%i), try increasing MAX_NAME_LENGHT (%i)", (int)nameLenght, (int)MAX_NAME_LENGHT);
                wasErrors = true;
                continue;
            }

            GLint memberCount;
            glGetActiveUniformBlockiv(glProgram, i, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &memberCount);

            if (memberCount > MAX_MEMBER_COUNT) {
                LOG_WARNING("Too many members (%i) in UniformBlock %s - try increasing MAX_MEMBER_COUNT (%i)", (int)memberCount, name, (int)MAX_MEMBER_COUNT);
                wasErrors = true;
                continue;
            }

            glGetActiveUniformBlockiv(glProgram, i, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, reinterpret_cast<GLint*>(members));

            glGetActiveUniformsiv(glProgram, memberCount, members, GL_UNIFORM_TYPE, types);
            glGetActiveUniformsiv(glProgram, memberCount, members, GL_UNIFORM_OFFSET, offsets);

            for (int i=0; i < memberCount; ++i) {
                GLint lenght = 0;
                glGetActiveUniformName(glProgram, members[i], MAX_NAME_LENGHT, &lenght, memberNames[i]);
                memberNames[i][MAX_NAME_LENGHT-1] = '\0';

                // If the uniform block is given a name, all members are named with the block name as a prefix
                //   >   uniform ModelBlcok {
                //   >         mat4 transform;
                //   >   } model;
                // Then the name of the transform uniform will be 'ModelBlock.transform'
                // instead of just 'transform'
                if (StringUtils::startsWith(memberNames[i], lenght, name, nameLenght) && memberNames[i][nameLenght] == '.') {
                    memmove(&memberNames[i][0], &memberNames[i][nameLenght+1], lenght - nameLenght);
                }
            }
            
            auto range = RegistredUniformBlocks().equal_range(name);

            if (range.first == range.second) {
                // Noo need to retrive uniform info, if we didn't find a matching uniform block
                LOG_WARNING("No uniform block with the name \"%s\" have been registred!", name);
                continue;
            }

            ErrorContext error;
            error.errorCount = -1;
            for (auto iter = range.first; iter != range.second; ++iter) {
                const UniformBlockInfo &info = iter->second;
                ErrorContext ctx;
                checkMatch(ctx, info, memberCount, members, types, offsets, memberNames);

                if (error.errorCount == -1 || ctx < error) error = ctx;
            }

            if (error) {
                LOG_WARNING("The following erros where found for uniform block \"%s\" in program \"%s\"", name, Common::GetCString(program->name));

                for (int i=0; i < std::min(MAX_ERRORS, error.errorCount); ++i) {
                    const auto &err= error.errors[i];
                    switch (err.type) {
                    case ErrorType::MemberMissing:
                        LOG_ERROR("\tMember \"%s\" don't exist in the registred block", memberNames[err.member]);
                        break;
                    case ErrorType::NameMissmatch:
                        LOG_WARNING("\tMember \"%s\" have a different name in the registred block", memberNames[err.member]);
                        break;
                    case ErrorType::TypeMissmatch:
                        LOG_ERROR("\tMember \"%s\" have a different type in the registred block", memberNames[err.member]);
                        break;
                    case ErrorType::OffsetMissmatch:
                        LOG_ERROR("\tMember \"%s\" have a different offset in the registred block", memberNames[err.member]);
                        break;
                    case ErrorType::NotAllMembersExist:
                        LOG_WARNING("\tNot all members in block where found.");
                        break;
                    }
                }

                if (error.errorCount >= MAX_ERRORS) {
                    LOG_ERROR("\tAn additional %i errors was found.", (int)(MAX_ERRORS - error.errorCount+1));
                }

                wasErrors = true;
            }
        }
        
        return wasErrors == false;
    }

    
    size_t PipelineManagerImpl::hashUniformBlock( const BaseProgramInfo *program, int uniformBlock )
    {
        GLuint glProgram = program->glProgram;
        
        GLuint members[MAX_MEMBER_COUNT];
        GLint types[MAX_MEMBER_COUNT];
        GLint offsets[MAX_MEMBER_COUNT];

        int blockIdx = program->uniformBuffers[uniformBlock].index;
        
        GLint memberCount;
        glGetActiveUniformBlockiv(glProgram, blockIdx, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &memberCount);

        if (memberCount > MAX_MEMBER_COUNT) return 0;
        
        glGetActiveUniformBlockiv(glProgram, blockIdx, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, reinterpret_cast<GLint*>(members));
        glGetActiveUniformsiv(glProgram, memberCount, members, GL_UNIFORM_TYPE, types);
        glGetActiveUniformsiv(glProgram, memberCount, members, GL_UNIFORM_OFFSET, offsets);

        UniformBlockMember layout[MAX_MEMBER_COUNT];
        for (int i=0; i < memberCount; ++i) {
            layout[i].type = FromGL(GLenum(types[i]));
            layout[i].offset = offsets[i];
        }

        return impl::hashMembers(
            std::initializer_list<UniformBlockMember>(layout, layout + memberCount)
        );
    }

    PISCES_API bool impl::registerUniformBlock( const UniformBlockInfo &info )
    {
        RegistredUniformBlocks().emplace(info.name, info);
        return true;
    }
}
