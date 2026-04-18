cmake_minimum_required(VERSION 3.20)
project(CGEngine LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(CGENGINE_BUILD_EDITOR "Build the editor target" OFF)

add_subdirectory(ThirdParty)
add_subdirectory(Engine)
add_subdirectory(Sandbox)

if (CGENGINE_BUILD_EDITOR)
    add_subdirectory(Editor)
endif()
