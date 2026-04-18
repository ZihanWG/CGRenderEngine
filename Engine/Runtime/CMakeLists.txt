add_library(CGEngineRuntime STATIC
    Application.cpp
    Application.h
)
add_library(CGEngine::Runtime ALIAS CGEngineRuntime)
target_include_directories(CGEngineRuntime
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)
target_link_libraries(CGEngineRuntime
    PUBLIC
        CGEngine::Core
        CGEngine::Assets
        CGEngine::Platform
        CGEngine::Renderer
        CGEngine::Scene
        glm::glm
)
