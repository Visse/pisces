#include "PipelineLoader.h"
#include "PipelineManager.h"

#include "Common/StringEqual.h"
#include "Common/FileUtils.h"
#include "Common/Throw.h"
#include "Common/StringId.h"
#include "Common/BuiltinFromString.h"
#include "Common/ErrorUtils.h"
#include "Common/StringFormat.h"
#include "Common/Yaml.h"

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

    PISCES_API ResourceHandle PipelineLoader::loadResource( Common::Archive &archive, Common::YamlNode node )
    {
#define LOAD_ERROR(error, ...) \
            LOG_ERROR("Failed to load pipeline in archive \"%s\" error: " error, archive.name(), __VA_ARGS__); \
            return ResourceHandle {};
#define LOAD_ERROR0(error) LOAD_ERROR(error, 0)

#define LOAD_WARNING(warning, ...) \
            LOG_ERROR("When loading pipeline in archive \"%s\" warning: " warning, archive.name(), __VA_ARGS__); \
            return ResourceHandle {};

        try {
            auto typeNode = node["PipelineType"];
            if (!(typeNode && typeNode.isScalar())) {
                LOAD_ERROR0("Missing attribute \"PipelineType\"");
            }

            std::string type = typeNode.scalar();
            if (type == "Render") {
                PipelineProgramInitParams params;

                auto blendModeNode = node["BlendMode"];


                if (blendModeNode && blendModeNode.isScalar()) {
                    const char *str = blendModeNode.c_str();

                    if (!BlendModeFromString(str, params.blendMode)) {
                        LOAD_ERROR("Unknown blend mode \"%s\"", str);   
                    }
                }

#define BUILTIN_ATTRIBUTE(name, var)                                                    \
                do {                                                                    \
                    auto varNode = node[name];                                          \
                    if (varNode && varNode.isScalar()) {                                \
                        const char *str = varNode.c_str();                              \
                        if (!Common::BuiltinFromString(str, strlen(str), var)) {        \
                            LOAD_ERROR("Unkown " name " \"%s\"", str);                  \
                        }                                                               \
                    }                                                                   \
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
                    LOAD_ERROR0("Missing attribute \"Program\"");
                }

                auto vertexShaderNode = programNode["VertexShader"],
                     fragmentShaderNode = programNode["FragmentShader"];

                if (!(vertexShaderNode && vertexShaderNode.isScalar())) {
                    LOAD_ERROR0("Missing attribute \"VertexShader\"");
                }
                if (!(fragmentShaderNode && fragmentShaderNode.isScalar()))  {
                    LOAD_ERROR0("Missing attribute \"FragmentShader\"");
                }

                std::string vertexShaderStr = vertexShaderNode.scalar();
                std::string fragmentShaderStr = fragmentShaderNode.scalar();

                auto vertexShaderFile = archive.openFile(vertexShaderStr);
                if (!vertexShaderFile) {
                    LOAD_ERROR("Failed to open file \"%s\"", vertexShaderStr.c_str());
                }

                auto fragmentShaderFile = archive.openFile(fragmentShaderStr);
                if (!fragmentShaderFile) {
                    LOAD_ERROR("Failed to open file \"%s\"", fragmentShaderStr.c_str());
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
                        for (std::pair<Common::YamlNode, Common::YamlNode> entry : node) {  \
                            int slot;                                               \
                            if (entry.second.as(slot)) {                            \
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
                    params.name = nameNode.scalarAsStringId();
                }

                PipelineHandle pipeline = mImpl->pipelineMgr->createPipeline(params);
                if (!pipeline) {
                    LOAD_ERROR0("Failed to create pipeline");
                }
                return ResourceHandle(pipeline.handle);
            }
            else if (type == "Compute") {
                LOAD_ERROR0("Not implemented type \"Compute\" yet.");
            }
        } catch (const std::exception &e) {
            LOAD_ERROR("Caught exception \"%s\"", e.what());
        }

        return ResourceHandle {};
    }
}
