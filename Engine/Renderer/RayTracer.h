// CPU path tracer-lite used as an offline-ish reference against the realtime renderer.
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
    // Threads tracing rows in parallel, including the calling thread. 0 picks one fewer
    // than the hardware concurrency so the realtime frame keeps a core. The image does
    // not depend on this value: every pixel is traced independently and deterministically.
    int threadCount = 0;
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
    // Cached BVH and flattened triangles are reused until scene geometry changes.
    struct CachedAcceleration;
    const CachedAcceleration& GetCachedAcceleration(const Scene& scene);

    std::unique_ptr<CachedAcceleration> m_CachedAcceleration;
};
