// Hardware-independent draw call accounting for a built RenderSubmission.
//
// The counts here are pure CPU integer accounting over the submission queues, so they
// are identical on every GPU, driver, and viewport size. "Batched" is what the passes
// actually submit after RenderQueue::Sort merged compatible commands. "Unbatched" is
// measured by re-running the same grouping with instancing disabled, so both figures
// come out of the same code rather than one of them being assumed.
#pragma once

#include <cstddef>

struct RenderQueue;
struct RenderSubmission;

struct PassDrawCallStats
{
    // Draw commands in this pass: one per visible object that the pass touches.
    std::size_t drawCommands = 0;
    // Draw calls with batching disabled, measured by re-grouping the same commands.
    std::size_t unbatchedDrawCalls = 0;
    // Instanced batches produced by RenderQueue::Sort.
    std::size_t batches = 0;
    // Draw calls actually submitted, after splitting batches on the object UBO capacity.
    std::size_t batchedDrawCalls = 0;
    // Object instances covered by the submitted draw calls.
    std::size_t instances = 0;

    void Add(const PassDrawCallStats& other);
};

struct DrawCallStats
{
    PassDrawCallStats shadow;
    PassDrawCallStats opaque;
    PassDrawCallStats transparent;
    PassDrawCallStats total;

    bool operator==(const DrawCallStats& other) const;
    bool operator!=(const DrawCallStats& other) const { return !(*this == other); }
};

// Costs an extra grouping pass over every queue; intended for tools and tests.
DrawCallStats CountDrawCalls(const RenderQueue& renderQueue);
DrawCallStats CountDrawCalls(const RenderSubmission& submission);
