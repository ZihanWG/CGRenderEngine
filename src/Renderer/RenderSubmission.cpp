#include "Renderer/RenderSubmission.h"

#include "Scene/Scene.h"

const RenderSubmission& RenderSubmissionCache::Build(const Scene& scene)
{
    if (m_BuiltSceneVersion == scene.GetContentVersion())
    {
        return m_Submission;
    }

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
