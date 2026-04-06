#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Scene/Environment.h"
#include "Scene/Light.h"
#include "Scene/Material.h"
#include "Scene/Transform.h"

class Mesh;

struct RenderObject
{
    std::string name;
    std::shared_ptr<Mesh> mesh;
    Transform transform;
    Material material;
};

class Scene
{
public:
    void MarkDirty();
    std::size_t GetContentVersion() const { return m_ContentVersion; }

    RenderObject& AddObject(RenderObject object);

    std::vector<RenderObject>& GetObjects()
    {
        MarkDirty();
        return m_Objects;
    }
    const std::vector<RenderObject>& GetObjects() const { return m_Objects; }

    DirectionalLight& GetDirectionalLight()
    {
        MarkDirty();
        return m_DirectionalLight;
    }
    const DirectionalLight& GetDirectionalLight() const { return m_DirectionalLight; }

    PointLight& GetPointLight()
    {
        MarkDirty();
        return m_PointLight;
    }
    const PointLight& GetPointLight() const { return m_PointLight; }

    SceneEnvironment& GetEnvironment()
    {
        MarkDirty();
        return m_Environment;
    }
    const SceneEnvironment& GetEnvironment() const { return m_Environment; }

private:
    std::vector<RenderObject> m_Objects;
    DirectionalLight m_DirectionalLight;
    PointLight m_PointLight;
    SceneEnvironment m_Environment;
    std::size_t m_ContentVersion = 1;
};
