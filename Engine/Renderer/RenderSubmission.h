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

private:
    static void BuildBatches(
        const std::vector<MeshDrawCommand>& commands,
        std::vector<InstancedDrawBatch>& batches,
        bool shadowOnly,
        bool allowInstancing
    );
};

struct RenderSubmission
{
    RenderQueue renderQueue;
    std::size_t sceneVersion = 0;
};

class RenderSubmissionCache
{
public:
    // Draw commands are built from the already-extracted render world every frame.
    const RenderSubmission& Build(const RenderWorld& renderWorld);
    void Invalidate();

private:
    RenderSubmission m_Submission;
};
