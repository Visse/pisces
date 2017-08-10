#include "UniformBlockInfo.h"
#include "Context.h"
#include "PipelineManager.h"
#include "internal/PipelineManagerImpl.h"

#include "Common/ErrorUtils.h"

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

        using UniformBlockMap = std::unordered_map<const char*, UniformBlockInfo, StrHash, StrEqual>;

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

    PISCES_API bool verifyUniformBlocksInProgram( ProgramHandle handle )
    {
        PipelineManagerImpl::Impl *pipelineMgr = Context::GetContext()->getPipelineManager()->impl();

        PipelineManagerImpl::RenderProgramInfo *programInfo = pipelineMgr->renderPrograms.find(handle);
        if (programInfo == nullptr) return true;

        return verifyUniformBlocksInProgram(programInfo->glProgram);
    }

    PISCES_API bool verifyUniformBlocksInProgram(unsigned int program)
    {
        GLint uniformBlockCount = 0;

        const int MAX_NAME_LENGHT = 256;
        const int MAX_MEMBER_COUNT = 256;

        char name[MAX_NAME_LENGHT],
             memberName[MAX_NAME_LENGHT];
        GLuint members[MAX_MEMBER_COUNT];

        GLint types[MAX_MEMBER_COUNT];
        GLint offsets[MAX_MEMBER_COUNT];


        glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &uniformBlockCount);

        bool wasErrors = false;

        for (int i=0; i < uniformBlockCount; ++i) {
            GLint lenght = 0;
            glGetActiveUniformBlockName(program, i, MAX_NAME_LENGHT, &lenght, name);

            if (lenght > MAX_NAME_LENGHT) {
                LOG_WARNING("Too long uniform block name (%i), try increasing MAX_NAME_LENGHT (%i)", (int)lenght, (int)MAX_NAME_LENGHT);
                wasErrors = true;
                continue;
            }

            auto iter = RegistredUniformBlocks().find(name);
            if (iter == RegistredUniformBlocks().end()) {
                LOG_WARNING("Unknown uniform block %s", name);
                wasErrors = true;
                continue;
            }
            GLint memberCount;
            glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &memberCount);

            if (memberCount > MAX_MEMBER_COUNT) {
                LOG_WARNING("Too many members (%i) in UniformBlock %s - try increasing MAX_MEMBER_COUNT (%i)", (int)memberCount, name, (int)MAX_MEMBER_COUNT);
                wasErrors = true;
                continue;
            }

            glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, reinterpret_cast<GLint*>(members));

            glGetActiveUniformsiv(program, memberCount, members, GL_UNIFORM_TYPE, types);
            glGetActiveUniformsiv(program, memberCount, members, GL_UNIFORM_OFFSET, offsets);

            const UniformBlockInfo &info = iter->second;

            for (int j=0; j < memberCount; ++j) {
                bool found = false;

                glGetActiveUniformName(program, members[j], MAX_NAME_LENGHT, &lenght, memberName);
                if (lenght > MAX_NAME_LENGHT) {
                    LOG_ERROR("Uniform member has to long name (%i) consider incresing MAX_NAME_LENGHT (%i)", (int)lenght, (int)MAX_NAME_LENGHT);
                    wasErrors = true;
                    continue;
                }

                for (const UniformBlockMember &memberInfo : info.members) {
                    if (strcmp(memberInfo.name, memberName) == 0) {
                        if (memberInfo.offset != offsets[j]) {
                            LOG_ERROR("Wrong offset for member %s in block %s, expected %i, got %i", memberName, name, (int)memberInfo.offset, (int)offsets[j]);
                            wasErrors = true;
                        }
                        if (ToGL(memberInfo.type) != types[j]) {
                            const auto &expected = glbinding::Meta::getString(ToGL(memberInfo.type));
                            const auto &got      =  glbinding::Meta::getString((GLenum)types[j]);
                            LOG_ERROR("Wrong type for member %s in block %s, expected %s, got %s", memberName, name, expected.c_str(), got.c_str());
                            wasErrors = true;
                        }
                        found = true;
                    }
                }

                if (!found) {
                    LOG_ERROR("Couldn't match member %s in block %s with the registred block!", memberName, name);
                    wasErrors = true;
                }
            }
        }
        
        return wasErrors == false;
    }

    PISCES_API bool impl::registerUniformBlock( const UniformBlockInfo &info )
    {
        auto res = RegistredUniformBlocks().emplace(info.name, info);
        return res.second;
    }
}
