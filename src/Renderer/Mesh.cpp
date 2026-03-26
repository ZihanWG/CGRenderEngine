#include "Renderer/Mesh.h"

#include <glad/glad.h>

#include <glm/gtc/constants.hpp>

Mesh::Mesh(std::vector<Vertex> vertices,
           std::vector<std::uint32_t> indices)
    : m_Vertices(std::move(vertices)),
      m_Indices(std::move(indices))
{
    Upload();
}

void Mesh::Draw() const
{
    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_Indices.size()), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

Mesh::~Mesh()
{
    if (m_EBO)
    {
        glDeleteBuffers(1, &m_EBO);
    }

    if (m_VBO)
    {
        glDeleteBuffers(1, &m_VBO);
    }

    if (m_VAO)
    {
        glDeleteVertexArrays(1, &m_VAO);
    }
}

std::shared_ptr<Mesh> Mesh::CreateCube(float size)
{
    const float h = size * 0.5f;
    std::vector<Vertex> vertices = {
        {{-h, -h,  h}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}},
        {{ h, -h,  h}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}},
        {{ h,  h,  h}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}},
        {{-h,  h,  h}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}},

        {{ h, -h, -h}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}},
        {{-h, -h, -h}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}},
        {{-h,  h, -h}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}},
        {{ h,  h, -h}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}},

        {{-h, -h, -h}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
        {{-h, -h,  h}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
        {{-h,  h,  h}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
        {{-h,  h, -h}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},

        {{ h, -h,  h}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
        {{ h, -h, -h}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
        {{ h,  h, -h}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
        {{ h,  h,  h}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},

        {{-h,  h,  h}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}},
        {{ h,  h,  h}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}},
        {{ h,  h, -h}, { 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}},
        {{-h,  h, -h}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}},

        {{-h, -h, -h}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}},
        {{ h, -h, -h}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}},
        {{ h, -h,  h}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}},
        {{-h, -h,  h}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}}
    };

    std::vector<std::uint32_t> indices = {
        0, 1, 2, 0, 2, 3,
        4, 5, 6, 4, 6, 7,
        8, 9,10, 8,10,11,
       12,13,14,12,14,15,
       16,17,18,16,18,19,
       20,21,22,20,22,23
    };

    return std::make_shared<Mesh>(std::move(vertices), std::move(indices));
}

std::shared_ptr<Mesh> Mesh::CreatePlane(float size, float uvScale)
{
    const float h = size * 0.5f;
    std::vector<Vertex> vertices = {
        {{-h, 0.0f, -h}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ h, 0.0f, -h}, {0.0f, 1.0f, 0.0f}, {uvScale, 0.0f}},
        {{ h, 0.0f,  h}, {0.0f, 1.0f, 0.0f}, {uvScale, uvScale}},
        {{-h, 0.0f,  h}, {0.0f, 1.0f, 0.0f}, {0.0f, uvScale}}
    };

    std::vector<std::uint32_t> indices = {0, 1, 2, 0, 2, 3};
    return std::make_shared<Mesh>(std::move(vertices), std::move(indices));
}

std::shared_ptr<Mesh> Mesh::CreateSphere(float radius, int xSegments, int ySegments)
{
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    for (int y = 0; y <= ySegments; ++y)
    {
        const float yRatio = static_cast<float>(y) / static_cast<float>(ySegments);
        const float theta = yRatio * glm::pi<float>();

        for (int x = 0; x <= xSegments; ++x)
        {
            const float xRatio = static_cast<float>(x) / static_cast<float>(xSegments);
            const float phi = xRatio * glm::two_pi<float>();

            const glm::vec3 normal = {
                glm::sin(theta) * glm::cos(phi),
                glm::cos(theta),
                glm::sin(theta) * glm::sin(phi)
            };

            vertices.push_back({
                normal * radius,
                glm::normalize(normal),
                glm::vec2(xRatio, 1.0f - yRatio)
            });
        }
    }

    for (int y = 0; y < ySegments; ++y)
    {
        for (int x = 0; x < xSegments; ++x)
        {
            const std::uint32_t row0 = static_cast<std::uint32_t>(y * (xSegments + 1));
            const std::uint32_t row1 = static_cast<std::uint32_t>((y + 1) * (xSegments + 1));

            const std::uint32_t a = row0 + static_cast<std::uint32_t>(x);
            const std::uint32_t b = row0 + static_cast<std::uint32_t>(x + 1);
            const std::uint32_t c = row1 + static_cast<std::uint32_t>(x + 1);
            const std::uint32_t d = row1 + static_cast<std::uint32_t>(x);

            indices.push_back(a);
            indices.push_back(b);
            indices.push_back(c);
            indices.push_back(a);
            indices.push_back(c);
            indices.push_back(d);
        }
    }

    return std::make_shared<Mesh>(std::move(vertices), std::move(indices));
}

std::shared_ptr<Mesh> Mesh::CreateFullscreenQuad()
{
    std::vector<Vertex> vertices = {
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}
    };

    std::vector<std::uint32_t> indices = {0, 1, 2, 0, 2, 3};
    return std::make_shared<Mesh>(std::move(vertices), std::move(indices));
}

void Mesh::Upload()
{
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(m_Vertices.size() * sizeof(Vertex)),
        m_Vertices.data(),
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(m_Indices.size() * sizeof(std::uint32_t)),
        m_Indices.data(),
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, texCoord)));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, tangent)));

    glBindVertexArray(0);
}
