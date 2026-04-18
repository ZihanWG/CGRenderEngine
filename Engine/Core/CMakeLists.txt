file(TO_CMAKE_PATH "${PROJECT_SOURCE_DIR}" CGENGINE_PROJECT_ROOT_PATH)

add_library(CGEngineCore STATIC
    AssetPaths.cpp
    AssetPaths.h
    JobSystem.cpp
    JobSystem.h
)
add_library(CGEngine::Core ALIAS CGEngineCore)
target_include_directories(CGEngineCore
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)
target_compile_definitions(CGEngineCore
    PRIVATE
        CGENGINE_PROJECT_ROOT="${CGENGINE_PROJECT_ROOT_PATH}"
)
