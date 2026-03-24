#pragma once

#include <memory>
#include <string>
#include <vector>

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
    RenderObject& AddObject(RenderObject object);

    std::vector<RenderObject>& GetObjects() { return m_Objects; }
    const std::vector<RenderObject>& GetObjects() const { return m_Objects; }

    DirectionalLight& GetDirectionalLight() { return m_DirectionalLight; }
    const DirectionalLight& GetDirectionalLight() const { return m_DirectionalLight; }

    PointLight& GetPointLight() { return m_PointLight; }
    const PointLight& GetPointLight() const { return m_PointLight; }

private:
    std::vector<RenderObject> m_Objects;
    DirectionalLight m_DirectionalLight;
    PointLight m_PointLight;
};
