// Scene only owns data; systems derive renderable state through versioned rebuilds.
#include "Scene/Scene.h"

void Scene::MarkDirty()
{
    ++m_ContentVersion;
}

RenderObject& Scene::AddObject(RenderObject object)
{
    MarkDirty();
    m_Objects.push_back(std::move(object));
    return m_Objects.back();
}
