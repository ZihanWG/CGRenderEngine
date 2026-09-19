// Locks the draw call count of a fixed scene, before and after batching.
//
// Everything here is CPU-only integer accounting over the real
// Scene -> RenderWorld -> RenderSubmission path, so the numbers do not depend on the
// GPU, the driver, or the window size. Both figures are produced by the engine's own
// batching code: the "after" figure from the batches the passes will submit, the
// "before" figure by re-grouping the same commands with instancing disabled. The viewport sweep below re-measures the same
// scene at aspect ratios from 9:16 to 32:9 and requires every measurement to match,
// which is what makes the recorded numbers a permanent reference rather than a
// snapshot of one machine.
#include "Tests/TestSupport.h"

#include <array>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>

#include "Engine/Renderer/DrawCallStats.h"
#include "Engine/Renderer/RenderSubmission.h"
#include "Engine/Renderer/RenderWorld.h"
#include "Tests/BatchingScene.h"

namespace
{
    struct Viewport
    {
        const char* name;
        int width;
        int height;
    };

    // Recorded once for the fixed scene. These are the "before vs after" numbers.
    // Shadow pass: 144 cubes -> 1 batch -> 2 draws (the 128-matrix object UBO splits it),
    //              64 spheres -> 1 batch -> 1 draw (shadow batching ignores material),
    //              1 ground   -> 1 batch -> 1 draw.
    // Opaque pass: same geometry, but the 4 sphere materials cannot merge -> 4 draws.
    // Transparent pass: depth order must be preserved, so instancing is disabled.
    constexpr std::size_t kExpectedShadowCommands = 209;
    constexpr std::size_t kExpectedShadowBatches = 3;
    constexpr std::size_t kExpectedShadowDrawCalls = 4;
    constexpr std::size_t kExpectedOpaqueCommands = 209;
    constexpr std::size_t kExpectedOpaqueBatches = 6;
    constexpr std::size_t kExpectedOpaqueDrawCalls = 7;
    constexpr std::size_t kExpectedTransparentCommands = 24;
    constexpr std::size_t kExpectedTransparentBatches = 24;
    constexpr std::size_t kExpectedTransparentDrawCalls = 24;
    constexpr std::size_t kExpectedTotalCommands = 442;
    constexpr std::size_t kExpectedTotalBatches = 33;
    constexpr std::size_t kExpectedTotalDrawCalls = 35;

    struct Measurement
    {
        std::size_t visibleObjects = 0;
        DrawCallStats stats;
    };

    Measurement Measure(const Scene& scene, const Viewport& viewport)
    {
        const float aspectRatio =
            static_cast<float>(viewport.width) / static_cast<float>(viewport.height);
        const Camera camera = BatchingScene::BuildCamera(aspectRatio);

        RenderWorldCache renderWorldCache;
        RenderSubmissionCache submissionCache;
        const RenderWorld& renderWorld = renderWorldCache.Build(
            scene,
            camera,
            viewport.width,
            viewport.height,
            0u,
            0.0f,
            0.0f
        );
        const RenderSubmission& submission = submissionCache.Build(renderWorld);

        return Measurement{renderWorld.visibleSet.objects.size(), CountDrawCalls(submission)};
    }

    void PrintPass(const char* name, const PassDrawCallStats& stats)
    {
        const double reduction = stats.unbatchedDrawCalls == 0
            ? 0.0
            : 100.0 * (1.0 - static_cast<double>(stats.batchedDrawCalls) /
                              static_cast<double>(stats.unbatchedDrawCalls));
        std::cout << "  " << std::left << std::setw(13) << name << std::right
                  << std::setw(10) << stats.unbatchedDrawCalls
                  << std::setw(10) << stats.batches
                  << std::setw(10) << stats.batchedDrawCalls
                  << std::setw(11) << std::fixed << std::setprecision(1) << reduction << "%\n";
    }

    void PrintReport(const DrawCallStats& stats)
    {
        std::cout << "Fixed scene: " << BatchingScene::kObjectCount << " objects ("
                  << BatchingScene::kCubeCount << " cubes sharing 1 material, "
                  << BatchingScene::kSphereCount << " spheres across "
                  << BatchingScene::kSphereMaterialCount << " materials, "
                  << BatchingScene::kGroundCount << " ground, "
                  << BatchingScene::kTransparentCount << " alpha-blended)\n";
        std::cout << "  " << std::left << std::setw(13) << "pass" << std::right
                  << std::setw(10) << "before"
                  << std::setw(10) << "batches"
                  << std::setw(10) << "after"
                  << std::setw(12) << "reduction" << '\n';
        PrintPass("shadow", stats.shadow);
        PrintPass("opaque", stats.opaque);
        PrintPass("transparent", stats.transparent);
        PrintPass("total", stats.total);
    }

    void ExpectPass(
        TestContext& test,
        const PassDrawCallStats& stats,
        std::size_t expectedCommands,
        std::size_t expectedBatches,
        std::size_t expectedDrawCalls
    )
    {
        EXPECT(test, stats.drawCommands == expectedCommands);
        // Measured, not assumed: with instancing disabled the grouping must fall back
        // to exactly one draw call per submittable command.
        EXPECT(test, stats.unbatchedDrawCalls == expectedCommands);
        EXPECT(test, stats.batches == expectedBatches);
        EXPECT(test, stats.batchedDrawCalls == expectedDrawCalls);
        // Batching regroups instances; it must never drop or duplicate one.
        EXPECT(test, stats.instances == expectedCommands);
    }
}

int main()
{
    TestContext test;

    const Scene scene = BatchingScene::Build();
    EXPECT(test, scene.GetObjects().size() == BatchingScene::kObjectCount);

    // Aspect ratios from portrait phone to ultrawide, plus a degenerate 1x1 viewport.
    constexpr std::array<Viewport, 8> viewports{{
        {"1x1", 1, 1},
        {"1080x1920", 1080, 1920},
        {"640x480", 640, 480},
        {"1280x720", 1280, 720},
        {"1920x1080", 1920, 1080},
        {"2560x1440", 2560, 1440},
        {"3840x2160", 3840, 2160},
        {"5120x1440", 5120, 1440}
    }};

    const Measurement reference = Measure(scene, viewports.front());
    for (const Viewport& viewport : viewports)
    {
        const Measurement measurement = Measure(scene, viewport);
        // Nothing is culled at any aspect ratio, which is why the counts below hold
        // for every resolution rather than for one window size.
        EXPECT(test, measurement.visibleObjects == BatchingScene::kObjectCount);
        if (measurement.stats != reference.stats)
        {
            std::cerr << "draw call stats changed at viewport " << viewport.name << '\n';
            EXPECT(test, measurement.stats == reference.stats);
        }
    }

    const DrawCallStats& stats = reference.stats;
    ExpectPass(test, stats.shadow, kExpectedShadowCommands, kExpectedShadowBatches, kExpectedShadowDrawCalls);
    ExpectPass(test, stats.opaque, kExpectedOpaqueCommands, kExpectedOpaqueBatches, kExpectedOpaqueDrawCalls);
    ExpectPass(
        test,
        stats.transparent,
        kExpectedTransparentCommands,
        kExpectedTransparentBatches,
        kExpectedTransparentDrawCalls
    );
    ExpectPass(test, stats.total, kExpectedTotalCommands, kExpectedTotalBatches, kExpectedTotalDrawCalls);

    // Transparency keeps one draw per object on purpose: merging would break depth order.
    EXPECT(test, stats.transparent.batchedDrawCalls == stats.transparent.unbatchedDrawCalls);

    PrintReport(stats);
    return test.Finish("DrawCallBatchingTests");
}
