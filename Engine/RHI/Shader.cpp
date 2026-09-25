// Shader program compilation/loading utilities for the OpenGL renderer.
#include "Engine/RHI/Shader.h"

#include <glad/glad.h>

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "Engine/Core/AssetPaths.h"

std::string Shader::ResolvePath(const std::string& path)
{
    return CGEngine::Core::ResolveProjectPathString(path);
}

std::string Shader::ReadFile(const std::string& path)
{
    // Keep shader paths project-root relative so the executable can run from the build folder.
    const std::string resolvedPath = ResolvePath(path);
    std::ifstream file(resolvedPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open shader file: " + resolvedPath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

unsigned int Shader::CompileShader(unsigned int type, const std::string& source, const std::string& debugName)
{
    unsigned int shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    char infoLog[1024];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        // Include the shader path in the exception so build/runtime failures are actionable.
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        glDeleteShader(shader);
        throw std::runtime_error("Shader compile error in " + debugName + ": " + infoLog);
    }

    return shader;
}

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
{
    // Compile individual stages first so errors report the original source file cleanly.
    std::string vertexCode = ReadFile(vertexPath);
    std::string fragmentCode = ReadFile(fragmentPath);

    // A throwing constructor never runs the destructor, so every GL name created here
    // has to be released on the failure paths below or it leaks for the context's lifetime.
    const unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, vertexCode, vertexPath);
    unsigned int fragmentShader = 0;
    try
    {
        fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentCode, fragmentPath);
    }
    catch (...)
    {
        glDeleteShader(vertexShader);
        throw;
    }

    const unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // The program keeps what it linked; the stage objects are no longer needed either way.
    glDetachShader(program, vertexShader);
    glDetachShader(program, fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    int success;
    char infoLog[1024];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        glDeleteProgram(program);
        throw std::runtime_error(
            "Program link error (" + vertexPath + ", " + fragmentPath + "): " + std::string(infoLog)
        );
    }

    m_ID = program;
}

Shader::~Shader()
{
    if (m_ID)
    {
        glDeleteProgram(m_ID);
    }
}

void Shader::Use() const
{
    glUseProgram(m_ID);
}

void Shader::SetInt(const std::string& name, int value) const
{
    glUniform1i(GetUniformLocation(name), value);
}

void Shader::SetFloat(const std::string& name, float value) const
{
    glUniform1f(GetUniformLocation(name), value);
}

void Shader::SetVec2(const std::string& name, const glm::vec2& value) const
{
    glUniform2fv(GetUniformLocation(name), 1, &value[0]);
}

void Shader::SetVec3(const std::string& name, const glm::vec3& value) const
{
    glUniform3fv(GetUniformLocation(name), 1, &value[0]);
}

void Shader::SetMat4(const std::string& name, const glm::mat4& value) const
{
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, &value[0][0]);
}

void Shader::SetUniformBlockBinding(const std::string& blockName, unsigned int bindingIndex) const
{
    const unsigned int blockIndex = glGetUniformBlockIndex(m_ID, blockName.c_str());
    if (blockIndex == GL_INVALID_INDEX)
    {
        // Allow shaders that do not consume a given block to share the same setup path.
        return;
    }

    glUniformBlockBinding(m_ID, blockIndex, bindingIndex);
}

int Shader::GetUniformLocation(const std::string& name) const
{
    return glGetUniformLocation(m_ID, name.c_str());
}
