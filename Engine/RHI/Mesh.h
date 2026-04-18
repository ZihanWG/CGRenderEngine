// CPU-owned vertex/index data plus the uploaded OpenGL buffers for drawing it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

struct Vertex
{
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 texCoord{0.0f};
    // Tangent.xyz plus handedness in w for normal mapping.
    glm::vec4 tangent{0.0f, 0.0f, 0.0f, 0.0f};
};

struct BoundingSphere
{
    glm::vec3 center{0.0f};
    float radius = 0.0f;
};

class Mesh
{
public:
    Mesh(std::vector<Vertex> vertices,
         std::vector<std::uint32_t> indices);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void Draw() const;
    void DrawInstanced(std::size_t instanceCount) const;

    const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
    const std::vector<std::uint32_t>& GetIndices() const { return m_Indices; }
    const BoundingSphere& GetBounds() const { return m_Bounds; }
    std::uint32_t GetStateId() const { return m_StateId; }

    static std::shared_ptr<Mesh> CreateCube(float size = 1.0f);
    static std::shared_ptr<Mesh> CreatePlane(float size = 1.0f, float uvScale = 1.0f);
    static std::shared_ptr<Mesh> CreateSphere(float radius = 1.0f, int xSegments = 24, int ySegments = 16);
    static std::shared_ptr<Mesh> CreateFullscreenQuad();

private:
    void ComputeBounds();
    // Upload the CPU mesh once at construction time; meshes are immutable afterward.
    void Upload();
    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;
    unsigned int m_EBO = 0;
    BoundingSphere m_Bounds;
    std::uint32_t m_StateId = 0;
    std::vector<Vertex> m_Vertices;
    std::vector<std::uint32_t> m_Indices;
};
