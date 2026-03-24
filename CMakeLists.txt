cmake_minimum_required(VERSION 3.20)
project(RealtimeRenderer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(GLOB_RECURSE SRC_FILES
        src/*.cpp
)

add_executable(RealtimeRenderer ${SRC_FILES}
        src/Renderer/VertexBuffer.cpp
        src/Renderer/VertexBuffer.h
        src/Renderer/IndexBuffer.cpp
        src/Renderer/IndexBuffer.h
        src/Renderer/VertexArray.cpp
        src/Renderer/VertexArray.h
        src/Renderer/Mesh.cpp
        src/Renderer/Mesh.h)

target_include_directories(RealtimeRenderer PRIVATE
        src
        external/glad/include
        external/glm
)

add_subdirectory(external/glfw)

add_library(glad external/glad/src/glad.c
        src/Scene/Transform.h
        src/Scene/Camera.h)
target_include_directories(glad PUBLIC external/glad/include)

target_link_libraries(RealtimeRenderer PRIVATE glfw glad)

if (WIN32)
    target_link_libraries(RealtimeRenderer PRIVATE opengl32)
endif()