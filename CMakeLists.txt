
source_group( " " REGULAR_EXPRESSION ./* )
source_group( "src" REGULAR_EXPRESSION ./src/* )
source_group( "internal" REGULAR_EXPRESSION ./internal/* )
source_group( "utility" REGULAR_EXPRESSION ./utility/* )

include_directories(.)

find_package(glbinding REQUIRED)
find_package(SDL2 REQUIRED)

find_path(GLM_INCLUDE_DIR FILES glm/glm.hpp REQUIRED)

option( PISCES_BUILD_IMGUI "Build ImGui as part of Pisces." ON )
option( PISCES_BUILD_NUKLEAR "Build Nuklear as part of Pisces." ON )
option( PISCES_SUPPORT_LOAD_OBJ "Support loading of .obj files" ON )
option( PISCES_MESH_BUILDER "Build mesh builder as part of pisces" ON )

option( PISCES_CLIP_ZERO_TO_ONE "Use clip range [0, 1], instead of the default [-1, 1] for better depth precision" ON)



add_library( Pisces-request-shared-gpu STATIC
    internal/RequestDiscreteGpu.cpp
)

if (MSVC)
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

    TextureLoader.h
    src/TextureLoader.cpp

    SpriteLoader.h
    src/SpriteLoader.cpp
)

if (PISCES_CLIP_ZERO_TO_ONE)
    target_compile_definitions( Pisces
        PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE
        PUBLIC PISCES_CLIP_ZERO_TO_ONE
    )
endif (PISCES_CLIP_ZERO_TO_ONE)

if (PISCES_BUILD_IMGUI)
    target_sources( Pisces
        PRIVATE utility/ImGuiRenderer.h
        PRIVATE utility/ImGuiRenderer.cpp
    )
    target_link_libraries( Pisces
        PRIVATE imgui
    )
    target_compile_definitions( Pisces
        PUBLIC PISCES_USE_IMGUI
    )
    
    get_target_property(IMGUI_INTERFACE_SOURCES imgui INTERFACE_SOURCES)
    source_group(imgui FILES ${IMGUI_INTERFACE_SOURCES})

    get_target_property(IMGUI_INTERFACE_DEFINITIONS imgui INTERFACE_COMPILE_DEFINITIONS)
    if (INTERFACE_COMPILE_DEFINITIONS)
        target_compile_definitions(Pisces INTERFACE ${INTERFACE_COMPILE_DEFINITIONS})
    endif (INTERFACE_COMPILE_DEFINITIONS)

    install(FILES utility/ImGuiRenderer.h DESTINATION include/Pisces/utility)
endif (PISCES_BUILD_IMGUI)

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
        PRIVATE nuklear
    )
    target_compile_definitions (Pisces
        PUBLIC PISCES_USE_NUKLEAR
    )
    
    get_target_property(NUKLEAR_INTERFACE_SOURCES nuklear INTERFACE_SOURCES)
    source_group(nuklear FILES ${NUKLEAR_INTERFACE_SOURCES})
    
    get_target_property(NUKLEAR_INTERFACE_DEFINITIONS nuklear INTERFACE_COMPILE_DEFINITIONS)
    if (NUKLEAR_INTERFACE_DEFINITIONS)
        target_compile_definitions(Pisces INTERFACE ${NUKLEAR_INTERFACE_DEFINITIONS})
    endif (NUKLEAR_INTERFACE_DEFINITIONS)
    
    install(FILES utility/NkContext.h utility/NkWindow.h utility/NkSprite.h DESTINATION include/Pisces/utility)
endif (PISCES_BUILD_NUKLEAR)


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
    PRIVATE yaml-cpp
    PUBLIC SDL2
    PUBLIC Common
    INTERFACE Pisces-request-shared-gpu
)
target_include_directories( Pisces
    PUBLIC ${GLM_INCLUDE_DIR}
)

target_compile_definitions(Pisces
    PUBLIC PISCES_BUILT_SHARED
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


if (PISCES_BUILD_IMGUI)
    get_target_property(IMGUI_INTERFACE_HEADERS imgui INTERFACE_HEADER)
    install(FILES ${IMGUI_INTERFACE_HEADERS} DESTINATION include)
endif (PISCES_BUILD_IMGUI)

if (PISCES_BUILD_NUKLEAR)
    get_target_property(NUKLEAR_INTERFACE_HEADERS nuklear INTERFACE_HEADER)
    install(FILES ${NUKLEAR_INTERFACE_HEADERS} DESTINATION include)
endif (PISCES_BUILD_NUKLEAR)

