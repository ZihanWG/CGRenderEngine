// Counts the draw calls a built RenderSubmission produces, before and after batching.
//
// The counting rules mirror ShadowPass::Execute and ScenePass::Execute exactly:
// a batch is skipped when it has no mesh (and, for the scene pass, no material), and
// every batch is split into chunks of kMaxObjectMatricesPerDraw because that is the
// object uniform block capacity. Keeping the rules here means the numbers can be
// verified without a window, a GPU, or a specific resolution.
//
// The "before batching" figure is not assumed to be the command count: it is measured
// by re-running BuildDrawBatches over the same commands with instancing disabled and
// counting the result the same way as the real batches. That costs one extra grouping
// pass and a temporary vector, so this is tooling and test code, not per-frame code.
#include "Engine/Renderer/DrawCallStats.h"

#include <vector>

#include "Engine/RHI/RenderBufferTypes.h"
#include "Engine/Renderer/RenderSubmission.h"

namespace
{
    constexpr std::size_t kInstancesPerDrawCall = static_cast<std::size_t>(kMaxObjectMatricesPerDraw);

    bool IsSubmittable(const MeshDrawCommand& command, bool requireMaterial)
    {
        return command.mesh != nullptr && (!requireMaterial || command.material != nullptr);
    }

    bool IsSubmittable(const InstancedDrawBatch& batch, bool requireMaterial)
    {
        return batch.mesh != nullptr && (!requireMaterial || batch.material != nullptr);
    }

    std::size_t DrawCallsForBatch(std::size_t instanceCount)
    {
        return (instanceCount + kInstancesPerDrawCall - 1) / kInstancesPerDrawCall;
    }

    struct BatchTotals
    {
        std::size_t batches = 0;
        std::size_t drawCalls = 0;
        std::size_t instances = 0;
    };

    BatchTotals CountBatches(const std::vector<InstancedDrawBatch>& batches, bool requireMaterial)
    {
        BatchTotals totals;
        for (const InstancedDrawBatch& batch : batches)
        {
            if (!IsSubmittable(batch, requireMaterial) || batch.perObjectDataIndices.empty())
            {
                continue;
            }

            ++totals.batches;
            totals.drawCalls += DrawCallsForBatch(batch.perObjectDataIndices.size());
            totals.instances += batch.perObjectDataIndices.size();
        }

        return totals;
    }

    PassDrawCallStats CountPass(
        const std::vector<MeshDrawCommand>& commands,
        const std::vector<InstancedDrawBatch>& batches,
        bool shadowOnly,
        bool requireMaterial
    )
    {
        PassDrawCallStats stats;
        for (const MeshDrawCommand& command : commands)
        {
            if (IsSubmittable(command, requireMaterial))
            {
                ++stats.drawCommands;
            }
        }

        // Re-group the same commands with instancing off. This is the real submission
        // path, just without merging, so the "before" figure comes from the batching
        // code rather than from an assumption about what it would have produced.
        std::vector<InstancedDrawBatch> unbatched;
        BuildDrawBatches(commands, unbatched, shadowOnly, false);
        const BatchTotals unbatchedTotals = CountBatches(unbatched, requireMaterial);
        stats.unbatchedDrawCalls = unbatchedTotals.drawCalls;

        const BatchTotals batchedTotals = CountBatches(batches, requireMaterial);
        stats.batches = batchedTotals.batches;
        stats.batchedDrawCalls = batchedTotals.drawCalls;
        stats.instances = batchedTotals.instances;
        return stats;
    }
}

void PassDrawCallStats::Add(const PassDrawCallStats& other)
{
    drawCommands += other.drawCommands;
    unbatchedDrawCalls += other.unbatchedDrawCalls;
    batches += other.batches;
    batchedDrawCalls += other.batchedDrawCalls;
    instances += other.instances;
}

bool DrawCallStats::operator==(const DrawCallStats& other) const
{
    const auto samePass = [](const PassDrawCallStats& left, const PassDrawCallStats& right) {
        return left.drawCommands == right.drawCommands &&
               left.unbatchedDrawCalls == right.unbatchedDrawCalls &&
               left.batches == right.batches &&
               left.batchedDrawCalls == right.batchedDrawCalls &&
               left.instances == right.instances;
    };

    return samePass(shadow, other.shadow) &&
           samePass(opaque, other.opaque) &&
           samePass(transparent, other.transparent) &&
           samePass(total, other.total);
}

DrawCallStats CountDrawCalls(const RenderQueue& renderQueue)
{
    DrawCallStats stats;
    // The shadow pass binds material state per batch but never requires a material.
    stats.shadow = CountPass(renderQueue.shadowCommands, renderQueue.shadowBatches, true, false);
    stats.opaque = CountPass(renderQueue.opaqueCommands, renderQueue.opaqueBatches, false, true);
    stats.transparent =
        CountPass(renderQueue.transparentCommands, renderQueue.transparentBatches, false, true);

    stats.total.Add(stats.shadow);
    stats.total.Add(stats.opaque);
    stats.total.Add(stats.transparent);
    return stats;
}

DrawCallStats CountDrawCalls(const RenderSubmission& submission)
{
    return CountDrawCalls(submission.renderQueue);
}
