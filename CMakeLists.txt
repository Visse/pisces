
source_group( " " REGULAR_EXPRESSION ./* )
source_group( "src" REGULAR_EXPRESSION ./src/* )
source_group( "internal" REGULAR_EXPRESSION ./internal/* )
source_group( "utility" REGULAR_EXPRESSION ./utility/* )

include_directories(.)

add_library( Pisces-request-shared-gpu STATIC
    internal/RequestDiscreteGpu.cpp
)

if (MSVC)
    # force visual studio to include symbols
    target_link_libraries( Pisces-request-shared-gpu
        INTERFACE "-INCLUDE:NvOptimusEnablement;-INCLUDE:AmdPowerXpressRequestHighPerformance"
    )
endif(MSVC)

add_library( Pisces SHARED
    Fwd.h
    src/Fwd.cpp
    build_config.h
    
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

    Sprite.h

    SpriteManager.h
    src/SpriteManager.cpp

    internal/GLCompat.h
    internal/GLCompat.cpp
    internal/GLTypes.h
    internal/Helpers.h
    
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

    IResourceLoader.h
    src/IResourceLoader.cpp
    
    PipelineLoader.h
    src/PipelineLoader.cpp

    ProgramLoader.h
    src/ProgramLoader.cpp

    TextureLoader.h
    src/TextureLoader.cpp

    SpriteLoader.h
    src/SpriteLoader.cpp
)

option (PISCES_CLIP_ZERO_TO_ONE "Use clip range [0, 1], instead of the default [-1, 1] for better depth precision" ON)
if (PISCES_CLIP_ZERO_TO_ONE)
    target_compile_definitions( Pisces
        PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE
        PUBLIC PISCES_CLIP_ZERO_TO_ONE
    )
endif (PISCES_CLIP_ZERO_TO_ONE)


option (PISCES_BUILD_IMGUI "Include imgui (debug gui) support" ON)
if (PISCES_BUILD_IMGUI)
    target_sources( Pisces
        PRIVATE utility/ImGuiRenderer.h
        PRIVATE utility/ImGuiRenderer.cpp
    )
    target_link_libraries( Pisces
        PUBLIC imgui
    )
    target_compile_definitions( Pisces
        PUBLIC PISCES_USE_IMGUI
    )

    install(FILES utility/ImGuiRenderer.h DESTINATION include/Pisces/utility)
endif (PISCES_BUILD_IMGUI)

option (PISCES_BUILD_NUKLEAR "Include nuklear (gui) support" ON )
if (PISCES_BUILD_NUKLEAR)
    target_sources (Pisces
        PRIVATE utility/NkContext.h
        PRIVATE utility/NkContext.cpp
        PRIVATE utility/NkWindow.h
        PRIVATE utility/NkWindow.cpp
        PRIVATE utility/NkSprite.h
        PRIVATE utility/NkSprite.cpp
    )
    target_link_libraries (Pisces
        PUBLIC nuklear
    )
    target_compile_definitions (Pisces
        PUBLIC PISCES_USE_NUKLEAR
    )
    
    install(FILES utility/NkContext.h utility/NkWindow.h utility/NkSprite.h DESTINATION include/Pisces/utility)
endif (PISCES_BUILD_NUKLEAR)


option( PISCES_SUPPORT_LOAD_OBJ "Support loading of .obj files" ON )
if (PISCES_SUPPORT_LOAD_OBJ)
    target_sources (Pisces
        PRIVATE utility/ObjLoader.h
        PRIVATE utility/ObjLoader.cpp
    )
    target_link_libraries (Pisces
        PRIVATE tinyobjloader
    )
    target_compile_definitions (Pisces
        PUBLIC PISCES_SUPPORT_LOAD_OBJ
    )
    
    install(FILES utility/ObjLoader.h DESTINATION include/Pisces/utility)
endif (PISCES_SUPPORT_LOAD_OBJ)

option (PISCES_MESH_BUILDER "Build mesh builder as part of pisces" ON)
if (PISCES_MESH_BUILDER)
    target_sources (Pisces
        PRIVATE utility/MeshBuilder.h
        PRIVATE utility/MeshBuilder.cpp
    )
    install(FILES utility/MeshBuilder.h DESTINATION include/Pisces/utility)
endif (PISCES_MESH_BUILDER)


target_link_libraries( Pisces
    PRIVATE stb
    PRIVATE glbinding::glbinding
    PUBLIC SDL2
    PUBLIC Common
    PUBLIC libyaml-cpp
    PUBLIC glm
    INTERFACE Pisces-request-shared-gpu
)

target_compile_definitions(Pisces
    PUBLIC PISCES_BUILT_SHARED
    PUBLIC PISCES_USE_LIBYAML
    PRIVATE PISCES_BUILDING
)


file(GLOB PISCES_HEADERS ./*.h)

install(TARGETS Pisces-request-shared-gpu Pisces
    EXPORT Zodiac
    RUNTIME DESTINATION bin/$<CONFIG>
    ARCHIVE DESTINATION lib/$<CONFIG>
    INCLUDES DESTONATION include
)
install(FILES ${PISCES_HEADERS} DESTINATION include/Pisces)




