add_library(CGEngineScene STATIC
    Camera.cpp
    Camera.h
    Environment.h
    Light.h
    Material.cpp
    Material.h
    Scene.cpp
    Scene.h
    Transform.cpp
    Transform.h
)
add_library(CGEngine::Scene ALIAS CGEngineScene)
target_include_directories(CGEngineScene
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)
target_link_libraries(CGEngineScene
    PUBLIC
        glm::glm
)
