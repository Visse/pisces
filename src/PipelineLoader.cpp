#include "PipelineLoader.h"
#include "PipelineManager.h"

#include "Common/StringEqual.h"
#include "Common/FileUtils.h"
#include "Common/Throw.h"
#include "Common/StringId.h"
#include "Common/BuiltinFromString.h"
#include "Common/ErrorUtils.h"
#include "Common/StringFormat.h"

#include "yaml-cpp/yaml.h"

#include <cassert>
#include <vector>

namespace Pisces
{
    struct PipelineLoader::Impl {
        PipelineManager *pipelineMgr;
    };

    PISCES_API PipelineLoader::PipelineLoader( PipelineManager *pipelineMgr )
    {
    }

    PISCES_API PipelineLoader::~PipelineLoader()
    {
    }

    PISCES_API ResourceHandle PipelineLoader::loadResource( Common::Archive &archive, YAML::Node node )
    {
#define LOAD_ERROR(error, ...) \
            LOG_ERROR("Failed to load pipeline in archive \"%s\" error: " error, archive.name(), __VA_ARGS__); \
            return ResourceHandle {};
#define LOAD_ERROR0(error) LOAD_ERROR(error, 0)

#define LOAD_WARNING(warning, ...) \
            LOG_ERROR("When loading pipeline in archive \"%s\" warning: " warning, archive.name(), __VA_ARGS__); \
            return ResourceHandle {};

        try {
            YAML::Node typeNode = node["Type"];
            if (!(typeNode && typeNode.IsScalar())) {
                LOAD_ERROR0("Missing attribute \"Type\"");
            }

            const std::string &type = typeNode.Scalar();
            if (type == "Render") {
                PipelineProgramInitParams params;

                YAML::Node blendModeNode = node["BlendMode"];


                if (blendModeNode && blendModeNode.IsScalar()) {
                    const std::string &str = blendModeNode.Scalar();

                    if (!BlendModeFromString(str.c_str(), str.size(), params.blendMode)) {
                        LOAD_ERROR("Unknown blend mode \"%s\"", str.c_str());   
                    }
                }

#define BUILTIN_ATTRIBUTE(name, var)                                                    \
                do {                                                                    \
                    YAML::Node varNode = node[name];                                    \
                    if (varNode && varNode.IsScalar()) {                                \
                        const std::string &str = varNode.Scalar();                      \
                        if (!Common::BuiltinFromString(str.c_str(), str.size(), var)) { \
                            LOAD_ERROR("Unkown " name " \"%s\"", str.c_str());          \
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


                YAML::Node programNode = node["Program"];
                if (!programNode) {
                    LOAD_ERROR0("Missing attribute \"Program\"");
                }

                YAML::Node vertexShaderNode = programNode["VertexShader"],
                           fragmentShaderNode = programNode["FragmentShader"];

                if (!(vertexShaderNode && vertexShaderNode.IsScalar())) {
                    LOAD_ERROR0("Missing attribute \"VertexShader\"");
                }
                if (!(fragmentShaderNode && fragmentShaderNode.IsScalar()))  {
                    LOAD_ERROR0("Missing attribute \"FragmentShader\"");
                }

                const std::string &vertexShaderStr = vertexShaderNode.Scalar();
                const std::string &fragmentShaderStr = fragmentShaderNode.Scalar();

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

                YAML::Node bindingNode = programNode["Bindings"];
                if (bindingNode) {
                    YAML::Node uniformsNode = bindingNode["Uniforms"],
                               uniformBuffersNode = bindingNode["UniformBuffers"],
                               samplersNode = bindingNode["Samplers"],
                               imageTexturesNode = bindingNode["ImageTextures"];

#define FILL_BINDINGS(name, node, var, max)                                     \
                    if (node && node.IsMap()) {                                 \
                        for (std::pair<YAML::Node, YAML::Node> entry : node) {  \
                            int slot = entry.second.as<int>();                  \
                            const std::string &val = entry.first.Scalar();      \
                            if (slot < 0 ||slot >= max) {                       \
                                LOG_WARNING("Invalid slot %i for " name " \"%s\"", slot, val.c_str()); \
                            }                                                   \
                            var[slot] = val;                                    \
                        }                                                       \
                    }
                    FILL_BINDINGS("uniform", uniformsNode, programParams.bindings.uniforms, MAX_BOUND_UNIFORMS);
                    FILL_BINDINGS("uniform buffer", uniformBuffersNode, programParams.bindings.uniformBuffers, MAX_BOUND_UNIFORM_BUFFERS);
                    FILL_BINDINGS("sampler", samplersNode, programParams.bindings.samplers, MAX_BOUND_SAMPLERS);
                    FILL_BINDINGS("image texture", imageTexturesNode, programParams.bindings.imageTextures, MAX_BOUND_IMAGE_TEXTURES);
#undef FILL_BINDINGS
                }


                YAML::Node nameNode = node["Name"];
                if (nameNode && nameNode.IsScalar()) {
                    const std::string &name = nameNode.Scalar();
                    params.name = Common::CreateStringId(name.c_str(), name.size());
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
