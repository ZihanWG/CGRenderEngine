#pragma once

#include <memory>
#include <vector>

class VertexArray;
class VertexBuffer;
class IndexBuffer;

class Mesh
{
public:
    Mesh(const std::vector<float>& vertices,
         const std::vector<unsigned int>& indices);
    ~Mesh();

    void Draw() const;

private:
    std::unique_ptr<VertexArray> m_VAO;
    std::unique_ptr<VertexBuffer> m_VBO;
    std::unique_ptr<IndexBuffer> m_IBO;
    unsigned int m_IndexCount = 0;
};