#pragma once

#include <vector>

#include <glm/glm.hpp>

class Camera;
class Scene;

struct RayTraceSettings
{
    int width = 480;
    int height = 270;
    int samplesPerPixel = 4;
    int maxBounces = 1;
};

class RayTracer
{
public:
    std::vector<glm::vec3> Render(
        const Scene& scene,
        const Camera& camera,
        const RayTraceSettings& settings
    ) const;
};
