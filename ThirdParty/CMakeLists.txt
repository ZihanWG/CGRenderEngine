set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(glfw)
add_library(CGEngine::glfw ALIAS glfw)

add_subdirectory(glm EXCLUDE_FROM_ALL)

add_library(glad STATIC
    glad/src/glad.c
)
add_library(CGEngine::glad ALIAS glad)
target_include_directories(glad
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/glad/include
)

add_library(CGEngineTinyGLTFHeaders INTERFACE)
add_library(CGEngine::tinygltf_headers ALIAS CGEngineTinyGLTFHeaders)
target_include_directories(CGEngineTinyGLTFHeaders
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/tinygltf
)
