cmake_minimum_required(VERSION 3.20)
project(RealtimeRenderer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
        src/*.cpp
)

add_executable(RealtimeRenderer ${SRC_FILES})

target_include_directories(RealtimeRenderer PRIVATE
        src
        external/glad/include
        external/glm
        external/tinygltf
)

add_subdirectory(external/glfw)

add_library(glad external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" CGENGINE_ASSET_ROOT_PATH)
target_compile_definitions(RealtimeRenderer PRIVATE
        CGENGINE_ASSET_ROOT="${CGENGINE_ASSET_ROOT_PATH}"
)

target_link_libraries(RealtimeRenderer PRIVATE glfw glad)

if (WIN32)
    target_link_libraries(RealtimeRenderer PRIVATE opengl32)
endif()
