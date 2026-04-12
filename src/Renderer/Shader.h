#pragma once

#include <glm/glm.hpp>

#include <string>

class Shader
{
public:
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    void Use() const;
    void SetInt(const std::string& name, int value) const;
    void SetFloat(const std::string& name, float value) const;
    void SetVec3(const std::string& name, const glm::vec3& value) const;
    void SetMat4(const std::string& name, const glm::mat4& value) const;
    void SetUniformBlockBinding(const std::string& blockName, unsigned int bindingIndex) const;

private:
    int GetUniformLocation(const std::string& name) const;

    unsigned int m_ID = 0;

    static std::string ResolvePath(const std::string& path);
    static std::string ReadFile(const std::string& path);
    static unsigned int CompileShader(unsigned int type, const std::string& source, const std::string& debugName);
};
