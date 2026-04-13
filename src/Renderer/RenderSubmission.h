// CPU-side flattened submission structures consumed by the render passes.
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Scene/Environment.h"
#include "Scene/Light.h"

class Mesh;
class Scene;
struct Material;

struct RenderItem
{
    std::size_t objectIndex = 0;
    std::string name;
    std::shared_ptr<Mesh> mesh;
    const Material* material = nullptr;
    glm::mat4 modelMatrix{1.0f};
};

struct RenderSceneState
{
    DirectionalLight directionalLight;
    PointLight pointLight;
    const SceneEnvironment* environment = nullptr;
};

struct RenderSubmission
{
    std::vector<RenderItem> drawItems;
    RenderSceneState sceneState;
    std::size_t sceneVersion = 0;
};

class RenderSubmissionCache
{
public:
    // Rebuild only when the Scene version changes; otherwise return cached draw items.
    const RenderSubmission& Build(const Scene& scene);
    void Invalidate();

private:
    std::size_t m_BuiltSceneVersion = 0;
    RenderSubmission m_Submission;
};
