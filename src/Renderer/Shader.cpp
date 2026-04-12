#include "Renderer/Shader.h"

#include <glad/glad.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

std::string Shader::ResolvePath(const std::string& path)
{
    const std::filesystem::path inputPath(path);
    if (inputPath.is_absolute())
    {
        return inputPath.string();
    }

    const std::filesystem::path root(CGENGINE_ASSET_ROOT);
    return (root / inputPath).string();
}

std::string Shader::ReadFile(const std::string& path)
{
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
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        throw std::runtime_error("Shader compile error in " + debugName + ": " + infoLog);
    }

    return shader;
}

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath)
{
    std::string vertexCode = ReadFile(vertexPath);
    std::string fragmentCode = ReadFile(fragmentPath);

    unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, vertexCode, vertexPath);
    unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentCode, fragmentPath);

    m_ID = glCreateProgram();
    glAttachShader(m_ID, vertexShader);
    glAttachShader(m_ID, fragmentShader);
    glLinkProgram(m_ID);

    int success;
    char infoLog[1024];
    glGetProgramiv(m_ID, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(m_ID, 1024, nullptr, infoLog);
        throw std::runtime_error("Program link error: " + std::string(infoLog));
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
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
        return;
    }

    glUniformBlockBinding(m_ID, blockIndex, bindingIndex);
}

int Shader::GetUniformLocation(const std::string& name) const
{
    return glGetUniformLocation(m_ID, name.c_str());
}
