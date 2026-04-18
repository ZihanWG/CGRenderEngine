// High-level scene container. Dirty tracking is version-based to keep CPU submission cheap.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Engine/Scene/Environment.h"
#include "Engine/Scene/Light.h"
#include "Engine/Scene/Material.h"
#include "Engine/Scene/Transform.h"

class Mesh;

struct RenderObject
{
    std::string name;
    std::shared_ptr<Mesh> mesh;
    Transform transform;
    Material material;
    std::shared_ptr<MaterialInstance> materialInstance;

    const Material& ResolveMaterial() const
    {
        return materialInstance ? materialInstance->GetMaterial() : material;
    }

    const MaterialInstance* GetMaterialInstance() const
    {
        return materialInstance.get();
    }
};

class Scene
{
public:
    // Any structural or mutable access bumps the content version for cached render data.
    void MarkDirty();
    std::size_t GetContentVersion() const { return m_ContentVersion; }

    // Stores the object and returns a mutable reference to the inserted entry.
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
