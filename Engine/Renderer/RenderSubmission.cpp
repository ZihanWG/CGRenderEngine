// Builds sorted MeshDrawCommand queues from RenderWorld.
//
// This is the explicit "Build Draw Commands" step of the engine:
// RenderWorld -> RenderSubmission
//
// The renderer never iterates Scene directly once RenderWorld + RenderSubmission
// have been built. That separation makes it easier to later add sorting, batching,
// instancing, and indirect draw generation without rewriting scene extraction.
#include "Engine/Renderer/RenderSubmission.h"

#include <algorithm>
#include <stdexcept>

#include "Engine/Scene/Material.h"

namespace
{
    std::uint16_t QuantizeViewDepth(float viewDepth)
    {
        const float scaledDepth = std::clamp(viewDepth * 128.0f, 0.0f, 65535.0f);
        return static_cast<std::uint16_t>(scaledDepth);
    }

    std::uint16_t NarrowStateId(std::uint32_t stateId)
    {
        return static_cast<std::uint16_t>(stateId & 0xFFFFu);
    }

    bool AreMaterialsBatchCompatible(const Material& left, const Material& right)
    {
        return left.albedo == right.albedo &&
               left.metallic == right.metallic &&
               left.roughness == right.roughness &&
               left.ao == right.ao &&
               left.emissive == right.emissive &&
               left.normalScale == right.normalScale &&
               left.occlusionStrength == right.occlusionStrength &&
               left.opacity == right.opacity &&
               left.blendMode == right.blendMode &&
               left.cullMode == right.cullMode &&
               left.castShadows == right.castShadows &&
               left.baseColorTexture.get() == right.baseColorTexture.get() &&
               left.metallicRoughnessTexture.get() == right.metallicRoughnessTexture.get() &&
               left.normalTexture.get() == right.normalTexture.get() &&
               left.occlusionTexture.get() == right.occlusionTexture.get() &&
               left.emissiveTexture.get() == right.emissiveTexture.get();
    }
}

SortKey SortKey::MakeShadow(std::uint32_t meshStateId, std::size_t visibleObjectIndex)
{
    const std::uint64_t meshKey = static_cast<std::uint64_t>(meshStateId & 0xFFFFu);
    const std::uint64_t objectKey = static_cast<std::uint64_t>(visibleObjectIndex & 0xFFFFu);
    return SortKey{(meshKey << 16u) | objectKey};
}

SortKey SortKey::MakeOpaque(
    std::uint32_t renderStateId,
    std::uint32_t materialAssetStateId,
    std::uint32_t meshStateId,
    float viewDepth
)
{
    const std::uint64_t renderStateKey = static_cast<std::uint64_t>(NarrowStateId(renderStateId));
    const std::uint64_t assetKey = static_cast<std::uint64_t>(materialAssetStateId & 0xFFFFu);
    const std::uint64_t meshKey = static_cast<std::uint64_t>(meshStateId & 0xFFFFu);
    const std::uint64_t depthKey = static_cast<std::uint64_t>(QuantizeViewDepth(viewDepth));
    return SortKey{(renderStateKey << 48u) | (assetKey << 32u) | (meshKey << 16u) | depthKey};
}

SortKey SortKey::MakeTransparent(
    std::uint32_t renderStateId,
    std::uint32_t materialAssetStateId,
    std::uint32_t meshStateId,
    float viewDepth
)
{
    const std::uint64_t depthKey = static_cast<std::uint64_t>(0xFFFFu - QuantizeViewDepth(viewDepth));
    const std::uint64_t renderStateKey = static_cast<std::uint64_t>(NarrowStateId(renderStateId));
    const std::uint64_t materialKey = static_cast<std::uint64_t>(materialAssetStateId & 0xFFFFu);
    const std::uint64_t meshKey = static_cast<std::uint64_t>(meshStateId & 0xFFFFu);
    return SortKey{(depthKey << 48u) | (renderStateKey << 32u) | (materialKey << 16u) | meshKey};
}

void RenderQueue::Clear()
{
    shadowCommands.clear();
    opaqueCommands.clear();
    transparentCommands.clear();
    shadowBatches.clear();
    opaqueBatches.clear();
    transparentBatches.clear();
}

void RenderQueue::Reserve(std::size_t visibleObjectCount)
{
    shadowCommands.reserve(visibleObjectCount);
    opaqueCommands.reserve(visibleObjectCount);
    transparentCommands.reserve(visibleObjectCount);
    shadowBatches.reserve(visibleObjectCount);
    opaqueBatches.reserve(visibleObjectCount);
    transparentBatches.reserve(visibleObjectCount);
}

void RenderQueue::Push(MeshDrawCommand command)
{
    switch (command.passMask)
    {
    case PassMask::Shadow:
        shadowCommands.push_back(std::move(command));
        break;
    case PassMask::Opaque:
        opaqueCommands.push_back(std::move(command));
        break;
    case PassMask::Transparent:
        transparentCommands.push_back(std::move(command));
        break;
    default:
        throw std::runtime_error("RenderQueue::Push expects a single-pass MeshDrawCommand.");
    }
}

void RenderQueue::Sort()
{
    const auto sortByKey = [](std::vector<MeshDrawCommand>& commands) {
        std::stable_sort(commands.begin(), commands.end(), [](const MeshDrawCommand& left, const MeshDrawCommand& right) {
            return left.sortKey < right.sortKey;
        });
    };

    sortByKey(shadowCommands);
    sortByKey(opaqueCommands);
    sortByKey(transparentCommands);

    BuildBatches(shadowCommands, shadowBatches, true, true);
    BuildBatches(opaqueCommands, opaqueBatches, false, true);
    BuildBatches(transparentCommands, transparentBatches, false, false);
}

void RenderQueue::BuildBatches(
    const std::vector<MeshDrawCommand>& commands,
    std::vector<InstancedDrawBatch>& batches,
    bool shadowOnly,
    bool allowInstancing
)
{
    batches.clear();
    batches.reserve(commands.size());

    for (const MeshDrawCommand& command : commands)
    {
        if (!command.mesh)
        {
            continue;
        }

        const bool canExtendBatch =
            !batches.empty() &&
            allowInstancing &&
            batches.back().meshStateId == command.meshStateId &&
            batches.back().renderStateId == command.renderStateId &&
            (shadowOnly || (
                batches.back().material &&
                command.material &&
                AreMaterialsBatchCompatible(*batches.back().material, *command.material)
            ));

        if (!canExtendBatch)
        {
            InstancedDrawBatch batch;
            batch.mesh = command.mesh;
            batch.material = command.material;
            batch.materialInstance = command.materialInstance;
            batch.renderStateId = command.renderStateId;
            batch.materialStateId = command.materialStateId;
            batch.materialAssetStateId = command.materialAssetStateId;
            batch.meshStateId = command.meshStateId;
            batches.push_back(std::move(batch));
        }

        batches.back().perObjectDataIndices.push_back(command.perObjectDataIndex);
    }
}

const RenderSubmission& RenderSubmissionCache::Build(const RenderWorld& renderWorld)
{
    // Rebuild from scratch for now. The important architectural choice is not the
    // rebuild strategy, but that all passes consume queues generated from RenderWorld
    // instead of poking at Scene extraction/culling data directly.
    m_Submission.renderQueue.Clear();
    m_Submission.sceneVersion = renderWorld.renderScene.sceneVersion;

    const auto& visibleObjects = renderWorld.visibleSet.objects;
    m_Submission.renderQueue.Reserve(visibleObjects.size());
    for (std::size_t visibleObjectIndex = 0; visibleObjectIndex < visibleObjects.size(); ++visibleObjectIndex)
    {
        const VisibleObject& visibleObject = visibleObjects[visibleObjectIndex];
        if (visibleObject.renderSceneObjectIndex >= renderWorld.renderScene.objects.size() ||
            visibleObject.perObjectDataIndex >= renderWorld.perObjectData.size())
        {
            continue;
        }

        const RenderSceneObject& sceneObject = renderWorld.renderScene.objects[visibleObject.renderSceneObjectIndex];

        if (HasPass(visibleObject.passMask, PassMask::Shadow))
        {
            m_Submission.renderQueue.Push(MeshDrawCommand{
                visibleObjectIndex,
                visibleObject.renderSceneObjectIndex,
                visibleObject.perObjectDataIndex,
                sceneObject.mesh,
                sceneObject.material,
                sceneObject.materialInstance,
                sceneObject.renderStateId,
                sceneObject.materialStateId,
                sceneObject.materialAssetStateId,
                sceneObject.meshStateId,
                SortKey::MakeShadow(sceneObject.meshStateId, visibleObjectIndex),
                PassMask::Shadow
            });
        }

        if (HasPass(visibleObject.passMask, PassMask::Opaque))
        {
            m_Submission.renderQueue.Push(MeshDrawCommand{
                visibleObjectIndex,
                visibleObject.renderSceneObjectIndex,
                visibleObject.perObjectDataIndex,
                sceneObject.mesh,
                sceneObject.material,
                sceneObject.materialInstance,
                sceneObject.renderStateId,
                sceneObject.materialStateId,
                sceneObject.materialAssetStateId,
                sceneObject.meshStateId,
                SortKey::MakeOpaque(
                    sceneObject.renderStateId,
                    sceneObject.materialAssetStateId,
                    sceneObject.meshStateId,
                    visibleObject.viewDepth
                ),
                PassMask::Opaque
            });
        }

        if (HasPass(visibleObject.passMask, PassMask::Transparent))
        {
            m_Submission.renderQueue.Push(MeshDrawCommand{
                visibleObjectIndex,
                visibleObject.renderSceneObjectIndex,
                visibleObject.perObjectDataIndex,
                sceneObject.mesh,
                sceneObject.material,
                sceneObject.materialInstance,
                sceneObject.renderStateId,
                sceneObject.materialStateId,
                sceneObject.materialAssetStateId,
                sceneObject.meshStateId,
                SortKey::MakeTransparent(
                    sceneObject.renderStateId,
                    sceneObject.materialAssetStateId,
                    sceneObject.meshStateId,
                    visibleObject.viewDepth
                ),
                PassMask::Transparent
            });
        }
    }

    m_Submission.renderQueue.Sort();
    return m_Submission;
}

void RenderSubmissionCache::Invalidate()
{
    m_Submission = {};
}
