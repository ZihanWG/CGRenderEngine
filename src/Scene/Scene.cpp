#include "Scene/Scene.h"

RenderObject& Scene::AddObject(RenderObject object)
{
    m_Objects.push_back(std::move(object));
    return m_Objects.back();
}
