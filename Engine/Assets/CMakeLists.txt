add_library(CGEngineAssets STATIC
    AssetCache.h
    EnvironmentLighting.cpp
    EnvironmentLighting.h
    GLTFLoader.cpp
    GLTFLoader.h
    ResourceManager.cpp
    ResourceManager.h
)
add_library(CGEngine::Assets ALIAS CGEngineAssets)
target_include_directories(CGEngineAssets
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)
target_link_libraries(CGEngineAssets
    PUBLIC
        CGEngine::Core
        CGEngine::Scene
        CGEngine::RHI
        CGEngine::tinygltf_headers
        glm::glm
)
