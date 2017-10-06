#include "ProgramLoader.h"
#include "PipelineManager.h"

#include "Common/StringEqual.h"
#include "Common/FileUtils.h"
#include "Common/Throw.h"
#include "Common/StringId.h"
#include "Common/BuiltinFromString.h"
#include "Common/ErrorUtils.h"
#include "Common/StringFormat.h"
#include "Common/FromString.h"

#include <cassert>
#include <vector>

namespace Pisces
{
    
    void LoadBindings( ProgramInitBindings &bindings, libyaml::Node node )
    {
        auto uniformsNode = node["Uniforms"],
             uniformBuffersNode = node["UniformBuffers"],
             samplersNode = node["Samplers"],
             imageTexturesNode = node["ImageTextures"];

#define FILL_BINDINGS(name, node, var, max)                             \
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
        FILL_BINDINGS("uniform", uniformsNode, bindings.uniforms, MAX_BOUND_UNIFORMS);
        FILL_BINDINGS("uniform buffer", uniformBuffersNode, bindings.uniformBuffers, MAX_BOUND_UNIFORM_BUFFERS);
        FILL_BINDINGS("sampler", samplersNode, bindings.samplers, MAX_BOUND_SAMPLERS);
        FILL_BINDINGS("image texture", imageTexturesNode, bindings.imageTextures, MAX_BOUND_IMAGE_TEXTURES);
#undef FILL_BINDINGS
    }


    PISCES_API ResourceHandle RenderProgramLoader::loadResource( Common::Archive &archive, libyaml::Node node )
    {
        auto vertexShaderNode = node["VertexShader"],
             fragmentShaderNode = node["FragmentShader"];

        if (!vertexShaderNode.isScalar()) {
            auto mark = vertexShaderNode ? vertexShaderNode.startMark() : node.endMark();
            THROW(std::runtime_error,
                  "Expected attribute \"VertexShader\" in \"%s::%s\" at %i:%i",
                  archive.name(), node.filename(), mark.line, mark.col
            );
        }
        if (!fragmentShaderNode.isScalar()) {
            auto mark = fragmentShaderNode ? fragmentShaderNode.startMark() : node.endMark();
            THROW(std::runtime_error,
                  "Expected attribute \"VertexShader\" in \"%s::%s\" at %i:%i",
                  archive.name(), node.filename(), mark.line, mark.col
            );
        }

        auto vertexShaderFile = archive.openFile(vertexShaderNode.scalar());
        if (!vertexShaderFile) {
            THROW(std::runtime_error, 
                "Failed to open file \"%s\"", vertexShaderNode.scalar()    
            );
        }

        auto fragmentShaderFile = archive.openFile(fragmentShaderNode.scalar());
        if (!fragmentShaderFile) {
            THROW(std::runtime_error, 
                "Failed to open file \"%s\"", fragmentShaderNode.scalar()    
            );
        }

        size_t vertexSourceLen = archive.fileSize(vertexShaderFile);
        const char *vertexSource = (const char*)archive.mapFile(vertexShaderFile);

        size_t fragmentSourceLen = archive.fileSize(fragmentShaderFile);
        const char *fragmentSource = (const char*)archive.mapFile(fragmentShaderFile);

        RenderProgramInitParams params;
        params.vertexSource.assign(vertexSource, vertexSourceLen);
        params.fragmentSource.assign(fragmentSource, fragmentSourceLen);

        auto bindingNode = node["Bindings"];
        if (bindingNode) {
            if (!bindingNode.isMap()) {
                auto mark = bindingNode.startMark();
                THROW(std::runtime_error,
                      "Expected map in \"%s::%s\" at %i:%i",
                       archive.name(), node.filename(), mark.line, mark.col
                );
            }
            LoadBindings(params.bindings, bindingNode);
        }

        auto nameNode = node["Name"];
        if (nameNode) {
            if (!nameNode.isScalar()) {
                auto mark = nameNode.startMark();
                THROW(std::runtime_error,
                      "Expected scalar \"%s::%s\" at %i:%i",
                       archive.name(), node.filename(), mark.line, mark.col
                );
            }
            params.name = Common::CreateStringId(nameNode.scalar());
        }

        ProgramHandle program = mPipelineMgr->createRenderProgram(params);
        return ResourceHandle{program.handle};
    }

    PISCES_API ResourceHandle ComputeProgramLoader::loadResource( Common::Archive &archive, libyaml::Node node )
    {
        auto sourceNode = node["Source"];
        
        if (!sourceNode.isScalar()) {
            auto mark = sourceNode ? sourceNode.startMark() : node.endMark();
            THROW(std::runtime_error,
                  "Expected attribute \"Source\" in \"%s::%s\" at %i:%i",
                  archive.name(), node.filename(), mark.line, mark.col
            );
        }
        auto sourceFile = archive.openFile(sourceNode.scalar());
        if (!sourceFile) {
            THROW(std::runtime_error, 
                "Failed to open file \"%s\"", sourceNode.scalar()    
            );
        }

        size_t sourceLen = archive.fileSize(sourceFile);
        const char *source = (const char*) archive.mapFile(sourceFile);

        
        ComputeProgramInitParams params;
        params.source.assign(source, sourceLen);
        
        auto bindingNode = node["Bindings"];
        if (bindingNode) {
            if (!bindingNode.isMap()) {
                auto mark = bindingNode.startMark();
                THROW(std::runtime_error,
                      "Expected map in \"%s::%s\" at %i:%i",
                       archive.name(), node.filename(), mark.line, mark.col
                );
            }
            LoadBindings(params.bindings, bindingNode);
        }

        auto nameNode = node["Name"];
        if (nameNode) {
            if (!nameNode.isScalar()) {
                auto mark = nameNode.startMark();
                THROW(std::runtime_error,
                      "Expected scalar \"%s::%s\" at %i:%i",
                       archive.name(), node.filename(), mark.line, mark.col
                );
            }
            params.name = Common::CreateStringId(nameNode.scalar());
        }

        ComputeProgramHandle handle = mPipelineMgr->createComputeProgram(params);
        return ResourceHandle(handle.handle);
    }

    PISCES_API ResourceHandle TransformProgramLoader::loadResource( Common::Archive &archive, libyaml::Node node )
    {
        auto sourceNode = node["Source"];
        
        if (!sourceNode.isScalar()) {
            auto mark = sourceNode ? sourceNode.startMark() : node.endMark();
            THROW(std::runtime_error,
                  "Expected attribute \"Source\" in \"%s::%s\" at %i:%i",
                  archive.name(), node.filename(), mark.line, mark.col
            );
        }
        auto sourceFile = archive.openFile(sourceNode.scalar());
        if (!sourceFile) {
            THROW(std::runtime_error, 
                "Failed to open file \"%s\"", sourceNode.scalar()    
            );
        }

        size_t sourceLen = archive.fileSize(sourceFile);
        const char *source = (const char*) archive.mapFile(sourceFile);

        
        TransformProgramInitParams params;
        params.source.assign(source, sourceLen);
        
        auto bindingNode = node["Bindings"];
        if (bindingNode) {
            if (!bindingNode.isMap()) {
                auto mark = bindingNode.startMark();
                THROW(std::runtime_error,
                      "Expected map in \"%s::%s\" at %i:%i",
                       archive.name(), node.filename(), mark.line, mark.col
                );
            }
            LoadBindings(params.bindings, bindingNode);
        }

        std::vector<TransformCaptureVariable> capture;

        auto captureNode = node["Capture"];
        if (!captureNode.isSequence()) {
            auto mark = captureNode ? captureNode.startMark() : node.endMark();

            THROW(std::runtime_error,
                  "Expected sequence in \"%s::%s\" at %i:%i",
                  archive.name(), node.filename(), mark.line, mark.col
            );


        }

        for (libyaml::Node node : captureNode) {
            if (!node.isMap()) {
                auto mark = node.startMark();
                THROW(std::runtime_error,
                      "Expected map in \"%s::%s\" at %i:%i",
                      archive.name(), node.filename(), mark.line, mark.col   
                );
            }

            auto nameNode = node["Name"],
                 typeNode = node["Type"];

            if (!nameNode.isScalar()) {
                auto mark = nameNode ? nameNode.startMark() : node.endMark();
                THROW(std::runtime_error,
                      "Expected attribute \"Name\" in \"%s::%s\" at %i:%i",
                      archive.name(), node.filename(), mark.line, mark.col   
                );
            }
            if (!typeNode.isScalar()) {
                auto mark = nameNode ? nameNode.startMark() : node.endMark();
                THROW(std::runtime_error,
                      "Expected attribute \"Type\" in \"%s::%s\" at %i:%i",
                      archive.name(), node.filename(), mark.line, mark.col   
                );
            }

            TransformCaptureType type;
            if (!FromString(typeNode.scalar(), type)) {
                auto mark = typeNode.startMark();
                THROW(std::runtime_error,
                      "Unknown TransformCaptureType \"%s\" in \"%s::%s\" at %i:%i",
                      typeNode.scalar(),
                      archive.name(), node.filename(), mark.line, mark.col   
                );
            }

            TransformCaptureVariable variable;
            variable.name = nameNode.scalar();
            variable.type = type;

            capture.push_back(variable);
        }

        auto nameNode = node["Name"];
        if (nameNode) {
            if (!nameNode.isScalar()) {
                auto mark = nameNode.startMark();
                THROW(std::runtime_error,
                      "Expected scalar \"%s::%s\" at %i:%i",
                       archive.name(), node.filename(), mark.line, mark.col
                );
            }
            params.name = Common::CreateStringId(nameNode.scalar());
        }

        TransformProgramHandle handle = mPipelineMgr->createTransformProgram(params, capture.data(), capture.size());
        return ResourceHandle(handle.handle);
    }
}
