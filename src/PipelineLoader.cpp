#include "PipelineLoader.h"
#include "PipelineManager.h"
#include "ProgramLoader.h"

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
    struct PipelineLoader::Impl {
        PipelineManager *pipelineMgr;
        RenderProgramLoader *renderProgramLoader;
    };

    PISCES_API PipelineLoader::PipelineLoader( PipelineManager *pipelineMgr, RenderProgramLoader *renderProgramLoader )
    {
        mImpl->pipelineMgr = pipelineMgr;
        mImpl->renderProgramLoader = renderProgramLoader;
    }

    PISCES_API PipelineLoader::~PipelineLoader()
    {
    }

    PISCES_API ResourceHandle PipelineLoader::loadResource( Common::Archive &archive, libyaml::Node node )
    {
        using Common::FromString;
#define GET_VALUE(type, var, node, attribute)                                           \
        do {                                                                            \
            auto tmp = node[attribute];                                                 \
            if (tmp) {                                                                  \
                bool success = false;                                                   \
                if (tmp.isScalar()) {                                                   \
                    success = FromString(tmp.scalar(), var);                            \
                }                                                                       \
                if (!success) {                                                         \
                    auto mark = tmp.startMark();                                        \
                    THROW(std::runtime_error,                                           \
                          "Expected \"%s\" got \"%s\" in \"%s::%s\" at %i:%i",          \
                          type, node.scalar(),                                          \
                          archive.name(), node.filename(), mark.line, mark.col          \
                    );                                                                  \
                }                                                                       \
            }                                                                           \
        } while(false)

        auto typeNode = node["PipelineType"];
        if (!typeNode.isScalar()) {
            auto mark = typeNode ? typeNode.startMark() : node.endMark();
            THROW(std::runtime_error, 
                  "Missing attribute \"PipelineType\" in \"%s::%s\" at %i:%i",
                  archive.name(), node.filename(), mark.line, mark.col
            );
        }

        std::string type = typeNode.scalar();
        if (type == "Render") {
            PipelineInitParams params;

            GET_VALUE("BlendMode", params.blendMode, node, "BlendMode");
           

            bool depthWrite = false,
                 depthTest = false,
                 faceCulling = false;
            GET_VALUE("bool", depthWrite, node, "DepthWrite");
            GET_VALUE("bool", depthTest, node, "DepthTest");
            GET_VALUE("bool", faceCulling, node, "FaceCulling");

            if (depthWrite) params.flags = set(params.flags, PipelineFlags::DepthWrite);
            if (depthTest) params.flags = set(params.flags, PipelineFlags::DepthTest);
            if (faceCulling) params.flags = set(params.flags, PipelineFlags::FaceCulling);


            auto programNode = node["Program"];
            if (!programNode.isMap()) {
                auto mark = programNode ? programNode.startMark() : node.endMark();

                THROW(std::runtime_error, 
                      "Missing attribute \"Program\" in \"%s::%s\" at %i:%i",
                      archive.name(), node.filename(), mark.line, mark.col      
                );
            }

            
            Common::StringId defaultProgramName;

            auto nameNode = node["Name"];
            if (nameNode.isScalar()) {
                params.name = Common::CreateStringId(nameNode.scalar());
                
                char tmpBuffer[64];
                StringUtils::sprintf(tmpBuffer, "%s.program", nameNode.scalar());

                defaultProgramName = Common::CreateStringId(tmpBuffer); 
            }
            
            params.program = mImpl->renderProgramLoader->loadProgram(archive, programNode, defaultProgramName);


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
