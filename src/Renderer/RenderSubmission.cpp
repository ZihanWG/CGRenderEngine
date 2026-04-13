// Flattens Scene data into pass-friendly draw items and scene constants.
//
// This is the explicit CPU submission step of the engine:
// Scene (authoring/data model) -> RenderSubmission (frame-ready draw list)
//
// The renderer never iterates Scene directly once submission has been built.
// That separation makes it easier to later add sorting, batching, culling, or
// multiple views without rewriting the scene container itself.
#include "Renderer/RenderSubmission.h"

#include "Scene/Scene.h"

const RenderSubmission& RenderSubmissionCache::Build(const Scene& scene)
{
    if (m_BuiltSceneVersion == scene.GetContentVersion())
    {
        // Submission is immutable until the scene version changes.
        return m_Submission;
    }

    // Rebuild from scratch for now. The important architectural choice is not the
    // rebuild strategy, but that all passes consume this flattened representation.
    m_Submission.drawItems.clear();
    m_Submission.sceneVersion = scene.GetContentVersion();
    m_Submission.sceneState.directionalLight = scene.GetDirectionalLight();
    m_Submission.sceneState.pointLight = scene.GetPointLight();
    m_Submission.sceneState.environment = &scene.GetEnvironment();

    const auto& objects = scene.GetObjects();
    m_Submission.drawItems.reserve(objects.size());
    for (std::size_t objectIndex = 0; objectIndex < objects.size(); ++objectIndex)
    {
        const RenderObject& object = objects[objectIndex];
        if (!object.mesh)
        {
            continue;
        }

        // Precompute the model matrix here so passes do not re-derive it independently.
        m_Submission.drawItems.push_back(RenderItem{
            objectIndex,
            object.name,
            object.mesh,
            &object.material,
            object.transform.GetMatrix()
        });
    }

    m_BuiltSceneVersion = m_Submission.sceneVersion;
    return m_Submission;
}

void RenderSubmissionCache::Invalidate()
{
    m_BuiltSceneVersion = 0;
    m_Submission = {};
}
