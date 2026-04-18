add_library(CGEngineRHI STATIC
    Framebuffer.cpp
    Framebuffer.h
    IndexBuffer.cpp
    IndexBuffer.h
    Mesh.cpp
    Mesh.h
    RenderBufferTypes.h
    Shader.cpp
    Shader.h
    ShaderBuffer.cpp
    ShaderBuffer.h
    ShaderBufferManager.cpp
    ShaderBufferManager.h
    Texture2D.cpp
    Texture2D.h
    VertexArray.cpp
    VertexArray.h
    VertexBuffer.cpp
    VertexBuffer.h
)
add_library(CGEngine::RHI ALIAS CGEngineRHI)
target_include_directories(CGEngineRHI
    PUBLIC
        ${PROJECT_SOURCE_DIR}
)
target_link_libraries(CGEngineRHI
    PUBLIC
        CGEngine::Core
        CGEngine::glad
        glm::glm
)

if (WIN32)
    target_link_libraries(CGEngineRHI
        PUBLIC
            opengl32
    )
endif()
