add_executable(CGEngineSceneTests
    SceneTests.cpp
)
target_link_libraries(CGEngineSceneTests
    PRIVATE
        CGEngine::Scene
)
add_test(NAME scene COMMAND CGEngineSceneTests)

add_executable(CGEngineRenderGraphTests
    RenderGraphTests.cpp
)
target_link_libraries(CGEngineRenderGraphTests
    PRIVATE
        CGEngine::Renderer
)
add_test(NAME render_graph COMMAND CGEngineRenderGraphTests)

add_executable(CGEngineGLTFTests
    GLTFTests.cpp
)
target_link_libraries(CGEngineGLTFTests
    PRIVATE
        CGEngine::Assets
)
target_compile_definitions(CGEngineGLTFTests
    PRIVATE
        CGENGINE_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Fixtures"
)
add_test(NAME gltf COMMAND CGEngineGLTFTests)

if (MINGW)
    # Keep CPU-only test executables runnable when CTest is launched outside the
    # compiler's shell environment. Full static linking is not reliable across all
    # MinGW distributions, so copy only the compiler runtime beside each test.
    get_filename_component(CGENGINE_MINGW_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(CGENGINE_TEST_TARGET IN ITEMS CGEngineSceneTests CGEngineRenderGraphTests CGEngineGLTFTests)
        foreach(CGENGINE_RUNTIME_DLL IN ITEMS libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
            if (EXISTS "${CGENGINE_MINGW_RUNTIME_DIR}/${CGENGINE_RUNTIME_DLL}")
                add_custom_command(TARGET ${CGENGINE_TEST_TARGET} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${CGENGINE_MINGW_RUNTIME_DIR}/${CGENGINE_RUNTIME_DLL}"
                        "$<TARGET_FILE_DIR:${CGENGINE_TEST_TARGET}>"
                )
            endif()
        endforeach()
    endforeach()
endif()
