#pragma once

#include <cstddef>
#include <memory>
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
    RayTracer();
    ~RayTracer();

    std::vector<glm::vec3> Render(
        const Scene& scene,
        const Camera& camera,
        const RayTraceSettings& settings
    );

private:
    struct CachedAcceleration;
    const CachedAcceleration& GetCachedAcceleration(const Scene& scene);

    std::unique_ptr<CachedAcceleration> m_CachedAcceleration;
};
