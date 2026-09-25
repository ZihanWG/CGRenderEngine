// Builds draw-command queues from the already-built render-visible world.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "Engine/Renderer/RenderWorld.h"

class Mesh;
struct Material;
class MaterialInstance;

struct SortKey
{
    std::uint64_t value = 0;

    bool operator<(const SortKey& other) const { return value < other.value; }

    static SortKey MakeShadow(std::uint32_t meshStateId, std::size_t visibleObjectIndex);
    static SortKey MakeOpaque(
        std::uint32_t renderStateId,
        std::uint32_t materialAssetStateId,
        std::uint32_t meshStateId,
        float viewDepth
    );
    static SortKey MakeTransparent(
        std::uint32_t renderStateId,
        std::uint32_t materialAssetStateId,
        std::uint32_t meshStateId,
        float viewDepth
    );
};

struct MeshDrawCommand
{
    std::size_t visibleObjectIndex = 0;
    std::size_t renderSceneObjectIndex = 0;
    std::size_t perObjectDataIndex = 0;
    std::shared_ptr<Mesh> mesh;
    const Material* material = nullptr;
    const MaterialInstance* materialInstance = nullptr;
    std::uint32_t renderStateId = 0;
    std::uint32_t materialStateId = 0;
    std::uint32_t materialAssetStateId = 0;
    std::uint32_t meshStateId = 0;
    SortKey sortKey{};
    PassMask passMask = PassMask::None;
};

struct InstancedDrawBatch
{
    std::shared_ptr<Mesh> mesh;
    const Material* material = nullptr;
    const MaterialInstance* materialInstance = nullptr;
    std::uint32_t renderStateId = 0;
    std::uint32_t materialStateId = 0;
    std::uint32_t materialAssetStateId = 0;
    std::uint32_t meshStateId = 0;
    std::vector<std::size_t> perObjectDataIndices;
};

// Groups already-sorted draw commands into instanced batches.
//
// Only adjacent compatible commands merge, which is why the queues are sorted first.
// With allowInstancing == false every command becomes its own single-instance batch,
// which is both what the transparent queue needs and what the passes would submit if
// batching were switched off.
void BuildDrawBatches(
    const std::vector<MeshDrawCommand>& commands,
    std::vector<InstancedDrawBatch>& batches,
    bool shadowOnly,
    bool allowInstancing
);

struct RenderQueue
{
    std::vector<MeshDrawCommand> shadowCommands;
    std::vector<MeshDrawCommand> opaqueCommands;
    std::vector<MeshDrawCommand> transparentCommands;
    std::vector<InstancedDrawBatch> shadowBatches;
    std::vector<InstancedDrawBatch> opaqueBatches;
    std::vector<InstancedDrawBatch> transparentBatches;

    void Clear();
    void Reserve(std::size_t visibleObjectCount);
    void Push(MeshDrawCommand command);
    void Sort();
};

struct RenderSubmission
{
    RenderQueue renderQueue;
    std::size_t sceneVersion = 0;
};

class RenderSubmissionCache
{
public:
    // Draw commands are rebuilt only when the render world's extracted content changed,
    // i.e. when its contentVersion differs from the one the cached submission was built from.
    const RenderSubmission& Build(const RenderWorld& renderWorld);
    void Invalidate();

    // Number of times Build() actually rebuilt the submission; exposed for regression tests.
    std::size_t GetBuildCount() const { return m_BuildCount; }

private:
    RenderSubmission m_Submission;
    std::uint64_t m_BuiltContentVersion = 0;
    std::size_t m_BuildCount = 0;
};
