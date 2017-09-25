#include "PipelineLoader.h"
#include "PipelineManager.h"

#include "Common/StringEqual.h"
#include "Common/FileUtils.h"
#include "Common/Throw.h"
#include "Common/StringId.h"
#include "Common/BuiltinFromString.h"
#include "Common/ErrorUtils.h"
#include "Common/StringFormat.h"

#include <cassert>
#include <vector>

namespace Pisces
{
    struct PipelineLoader::Impl {
        PipelineManager *pipelineMgr;
    };

    PISCES_API PipelineLoader::PipelineLoader( PipelineManager *pipelineMgr )
    {
        mImpl->pipelineMgr = pipelineMgr;
    }

    PISCES_API PipelineLoader::~PipelineLoader()
    {
    }

    PISCES_API ResourceHandle PipelineLoader::loadResource( Common::Archive &archive, libyaml::Node node )
    {
        auto typeNode = node["PipelineType"];
        if (!typeNode.isScalar()) {
            THROW(std::runtime_error, 
                    "Missing attribute \"PipelineType\""
            );
        }

        std::string type = typeNode.scalar();
        if (type == "Render") {
            PipelineProgramInitParams params;

            auto blendModeNode = node["BlendMode"];


            if (blendModeNode && blendModeNode.isScalar()) {
                const char *str = blendModeNode.scalar();

                if (!BlendModeFromString(str, params.blendMode)) {
                    auto mark = blendModeNode.startMark();
                    THROW(std::runtime_error,
                            "Unknown blend mode \"%s\" at %i:%i", str, mark.line, mark.col
                    );   
                }
            }

#define BUILTIN_ATTRIBUTE(name, var)                                                    \
            do {                                                                        \
                auto varNode = node[name];                                              \
                if (varNode.isScalar()) {                                               \
                    const char *str = varNode.scalar();                                 \
                    if (!Common::BuiltinFromString(str, strlen(str), var)) {            \
                        auto mark = varNode.startMark();                                \
                        THROW(std::runtime_error,                                       \
                                "Failed to parse \"%s\" for attribute \"%s\" at %i:%i", \
                                str, name, mark.line, mark.col                          \
                        );                                                              \
                    }                                                                   \
                }                                                                       \
            } while(0)

            bool depthWrite = false,
                    depthTest = false,
                    faceCulling = false;
            BUILTIN_ATTRIBUTE("DepthWrite", depthWrite);
            BUILTIN_ATTRIBUTE("DepthTest", depthTest);
            BUILTIN_ATTRIBUTE("FaceCulling", faceCulling);

            if (depthWrite) params.flags = set(params.flags, PipelineFlags::DepthWrite);
            if (depthTest) params.flags = set(params.flags, PipelineFlags::DepthTest);
            if (faceCulling) params.flags = set(params.flags, PipelineFlags::FaceCulling);


            auto programNode = node["Program"];
            if (!(programNode && programNode.isMap())) {
                THROW(std::runtime_error, "Missing attribute \"Program\"");
            }

            auto vertexShaderNode = programNode["VertexShader"],
                    fragmentShaderNode = programNode["FragmentShader"];

            if (!(vertexShaderNode && vertexShaderNode.isScalar())) {
                THROW(std::runtime_error, "Missing attribute \"VertexShader\"");
            }
            if (!(fragmentShaderNode && fragmentShaderNode.isScalar()))  {
                THROW(std::runtime_error, "Missing attribute \"FragmentShader\"");
            }

            std::string vertexShaderStr = vertexShaderNode.scalar();
            std::string fragmentShaderStr = fragmentShaderNode.scalar();

            auto vertexShaderFile = archive.openFile(vertexShaderStr);
            if (!vertexShaderFile) {
                THROW(std::runtime_error, "Failed to open file \"%s\"", vertexShaderStr.c_str());
            }

            auto fragmentShaderFile = archive.openFile(fragmentShaderStr);
            if (!fragmentShaderFile) {
                THROW(std::runtime_error, "Failed to open file \"%s\"", fragmentShaderStr.c_str());
            }


            size_t vertexSourceLen = archive.fileSize(vertexShaderFile),
                   fragmentSourceLen = archive.fileSize(fragmentShaderFile);

            const char *vertexSource = (const char*) archive.mapFile(vertexShaderFile),
                       *fragmentSource = (const char*) archive.mapFile(fragmentShaderFile);

            // Since the file was open succesfully it should succed to map it
            assert (vertexSource && fragmentSource);

            auto &programParams = params.programParams;

            programParams.vertexSource.assign(vertexSource, vertexSourceLen);
            programParams.fragmentSource.assign(fragmentSource, fragmentSourceLen);

            auto bindingNode = programNode["Bindings"];
            if (bindingNode && bindingNode.isMap()) {
                auto uniformsNode = bindingNode["Uniforms"],
                     uniformBuffersNode = bindingNode["UniformBuffers"],
                     samplersNode = bindingNode["Samplers"],
                     imageTexturesNode = bindingNode["ImageTextures"];

#define FILL_BINDINGS(name, node, var, max)                                         \
                if (node.isMap()) {                                             \
                    for (std::pair<libyaml::Node, libyaml::Node> entry : node) {\
                        int slot;                                               \
                        if (entry.second.isScalar() &&                          \
                            Common::BuiltinFromString(entry.second.scalar(), slot)) {\
                            std::string val = entry.first.scalar();             \
                            if (slot < 0 || slot >= max) {                      \
                                LOG_WARNING("Invalid slot %i for " name " \"%s\"", slot, val.c_str()); \
                            }                                                   \
                            var[slot] = val;                                    \
                        }                                                       \
                    }                                                           \
                }
                FILL_BINDINGS("uniform", uniformsNode, programParams.bindings.uniforms, MAX_BOUND_UNIFORMS);
                FILL_BINDINGS("uniform buffer", uniformBuffersNode, programParams.bindings.uniformBuffers, MAX_BOUND_UNIFORM_BUFFERS);
                FILL_BINDINGS("sampler", samplersNode, programParams.bindings.samplers, MAX_BOUND_SAMPLERS);
                FILL_BINDINGS("image texture", imageTexturesNode, programParams.bindings.imageTextures, MAX_BOUND_IMAGE_TEXTURES);
#undef FILL_BINDINGS
            }


            auto nameNode = node["Name"];
            if (nameNode && nameNode.isScalar()) {
                params.name = Common::CreateStringId(nameNode.scalar());
            }

            PipelineHandle pipeline = mImpl->pipelineMgr->createPipeline(params);
            if (!pipeline) {
                THROW(std::runtime_error, "Failed to create pipeline");
            }
            return ResourceHandle(pipeline.handle);
        }
        else if (type == "Compute") {
            THROW(std::runtime_error, "Not implemented type \"Compute\" yet.");
        }

        return ResourceHandle {};
    }
}
