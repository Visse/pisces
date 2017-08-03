
source_group( " " REGULAR_EXPRESSION ./* )
source_group( "src" REGULAR_EXPRESSION ./src/* )
source_group( "internal" REGULAR_EXPRESSION ./internal/* )
source_group( "utility" REGULAR_EXPRESSION ./utility/* )

include_directories(.)

find_package(glbinding REQUIRED)
find_package(SDL2 REQUIRED)

find_path(GLM_INCLUDE_DIR FILES glm/glm.hpp REQUIRED)

option( PISCES_BUILD_IMGUI "Build ImGui as part of Pisces." ON )

add_library( Pisces SHARED
    Fwd.h
    src/Fwd.cpp
    build_config.h
    ResourceName.h
    ResourceMap.h

    Context.h
    src/Context.cpp

    HardwareResourceManager.h
    src/HardwareResourceManager.cpp

    PipelineManager.h
    src/PipelineManager.cpp

    RenderCommandQueue.h
    src/RenderCommandQueue.cpp

    CompiledRenderQueue.h
    src/CompiledRenderQueue.cpp

    StreamingBuffer.h
    src/StreamingBuffer.cpp

    UniformBlockInfo.h
    src/UniformBlockInfo.cpp
    
    internal/GLCompat.h
    internal/GLCompat.cpp
    internal/GLTypes.h
    internal/Helpers.h

    internal/RequestDiscreteGpu.cpp
    
    internal/GLDebugCallback.h
    internal/GLDebugCallback.cpp

    internal/RenderCommandQueueImpl.h
    internal/HardwareResourceManagerImpl.h
    internal/HardwareResourceManagerImpl.cpp
    internal/PipelineManagerImpl.h
    internal/PipelineManagerImpl.cpp
    internal/CompiledRenderQueueImpl.h

    internal/BuiltinObjects.h
    internal/BuiltinObjects.cpp

    internal/BuiltinTextures.h
    internal/BuiltinTextures.cpp

    internal/CommandQueueCompiler.h
    internal/CommandQueueCompiler.cpp

    PipelineLoader.h
    src/PipelineLoader.cpp

    TextureLoader.h
    src/TextureLoader.cpp
)

if (PISCES_BUILD_IMGUI)
    target_sources( Pisces
        PRIVATE utility/ImGuiRenderer.h
        PRIVATE utility/ImGuiRenderer.cpp
    )
    target_link_libraries( Pisces
        PRIVATE imgui
    )
    target_compile_definitions( Pisces
        PUBLIC IMGUI_API=PISCES_API
    )
endif (PISCES_BUILD_IMGUI)


target_link_libraries( Pisces
    PRIVATE stb
    PRIVATE glbinding::glbinding
    PUBLIC SDL2
    PUBLIC yaml-cpp
    PUBLIC Common
)
target_include_directories( Pisces
    PUBLIC ${GLM_INCLUDE_DIR}
)

target_compile_definitions(Pisces
    PUBLIC PISCES_BUILT_SHARED
    PRIVATE PISCES_BUILDING
)


