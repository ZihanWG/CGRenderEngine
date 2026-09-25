#include "Tests/TestSupport.h"

#include <cstddef>
#include <vector>

#include <glm/glm.hpp>

#include "Engine/Renderer/RayTracer.h"
#include "Engine/Scene/Camera.h"
#include "Tests/BatchingScene.h"

namespace
{
    std::vector<glm::vec3> RenderWithThreads(const Scene& scene, const Camera& camera, int width, int height, int threads)
    {
        RayTraceSettings settings;
        settings.width = width;
        settings.height = height;
        settings.samplesPerPixel = 2;
        settings.maxBounces = 1;
        settings.threadCount = threads;

        RayTracer tracer;
        return tracer.Render(scene, camera, settings);
    }

    bool BitwiseEqual(const std::vector<glm::vec3>& left, const std::vector<glm::vec3>& right)
    {
        if (left.size() != right.size())
        {
            return false;
        }

        for (std::size_t i = 0; i < left.size(); ++i)
        {
            if (left[i] != right[i])
            {
                return false;
            }
        }

        return true;
    }
}

// Every pixel is traced independently with deterministic jitter, so the image must not
// depend on how rows are split across threads. A data race or a row written twice or
// skipped shows up here as a mismatch against the single-threaded render.
int main()
{
    TestContext test;

    constexpr int kWidth = 64;
    constexpr int kHeight = 36;
    const Scene scene = BatchingScene::Build();
    const Camera camera = BatchingScene::BuildCamera(static_cast<float>(kWidth) / static_cast<float>(kHeight));

    const std::vector<glm::vec3> serial = RenderWithThreads(scene, camera, kWidth, kHeight, 1);
    const std::vector<glm::vec3> threeThreads = RenderWithThreads(scene, camera, kWidth, kHeight, 3);
    const std::vector<glm::vec3> automatic = RenderWithThreads(scene, camera, kWidth, kHeight, 0);

    EXPECT(test, serial.size() == static_cast<std::size_t>(kWidth * kHeight));
    EXPECT(test, BitwiseEqual(serial, threeThreads));
    EXPECT(test, BitwiseEqual(serial, automatic));

    // The image must actually contain the scene, or equality above proves nothing.
    bool hasVariation = false;
    for (const glm::vec3& pixel : serial)
    {
        if (pixel != serial.front())
        {
            hasVariation = true;
            break;
        }
    }
    EXPECT(test, hasVariation);

    // More threads than rows: the extra threads find no work and the image is still complete.
    const std::vector<glm::vec3> oneRowSerial = RenderWithThreads(scene, camera, kWidth, 1, 1);
    const std::vector<glm::vec3> oneRowWide = RenderWithThreads(scene, camera, kWidth, 1, 8);
    EXPECT(test, BitwiseEqual(oneRowSerial, oneRowWide));

    return test.Finish("ray_tracer");
}
