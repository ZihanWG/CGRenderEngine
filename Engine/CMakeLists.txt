add_subdirectory(Core)
add_subdirectory(Platform)
add_subdirectory(Scene)
add_subdirectory(RHI)
add_subdirectory(Assets)
add_subdirectory(Renderer)
add_subdirectory(Runtime)

add_library(Engine INTERFACE)
add_library(CGEngine::Engine ALIAS Engine)
target_link_libraries(Engine
    INTERFACE
        CGEngine::Core
        CGEngine::Platform
        CGEngine::Scene
        CGEngine::RHI
        CGEngine::Assets
        CGEngine::Renderer
        CGEngine::Runtime
)
