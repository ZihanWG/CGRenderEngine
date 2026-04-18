add_library(CGEnginePlatform STATIC
    Window.cpp
    Window.h
)
add_library(CGEngine::Platform ALIAS CGEnginePlatform)
target_include_directories(CGEnginePlatform
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)
target_link_libraries(CGEnginePlatform
    PUBLIC
        CGEngine::glad
        CGEngine::glfw
)

if (WIN32)
    target_link_libraries(CGEnginePlatform
        PUBLIC
            opengl32
    )
endif()
