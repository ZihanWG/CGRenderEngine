// A fixed, fully deterministic scene used to measure draw call batching.
//
// The scene is intentionally frozen: changing it invalidates the recorded draw call
// numbers in docs/BATCHING.md and DrawCallBatchingTests.cpp. It is built from
// CPU-only primitives, so it needs no GPU, no window, and no asset files, and every
// object sits well inside the camera frustum so frustum culling never removes
// anything. That is what makes the measured numbers independent of the resolution
// the engine happens to run at.
#pragma once

#include <cstddef>
#include <string>

#include <glm/glm.hpp>

#include "Engine/RHI/Mesh.h"
#include "Engine/Scene/Camera.h"
#include "Engine/Scene/Material.h"
#include "Engine/Scene/Scene.h"

namespace BatchingScene
{
    // 12 x 12 cubes sharing one mesh and one material: the batchable bulk of the scene.
    // 144 > kMaxObjectMatricesPerDraw (128), so this group also pins the batch split rule.
    inline constexpr int kCubeGridSide = 12;
    inline constexpr std::size_t kCubeCount = static_cast<std::size_t>(kCubeGridSide * kCubeGridSide);
    // 8 x 8 spheres sharing one mesh, cycling through four materials.
    inline constexpr int kSphereGridSide = 8;
    inline constexpr std::size_t kSphereCount = static_cast<std::size_t>(kSphereGridSide * kSphereGridSide);
    inline constexpr std::size_t kSphereMaterialCount = 4;
    // Alpha-blended cubes. The transparent queue is depth-sorted and never instanced.
    inline constexpr std::size_t kTransparentCount = 24;
    inline constexpr std::size_t kGroundCount = 1;
    inline constexpr std::size_t kObjectCount =
        kCubeCount + kSphereCount + kTransparentCount + kGroundCount;

    // Camera placement. The scene's widest object centre sits ~4.4 units off the view
    // axis at ~45 units of depth, so it stays inside the horizontal frustum for every
    // aspect ratio from 9:16 (portrait) to 32:9 (ultrawide).
    inline constexpr float kCameraFovDegrees = 45.0f;
    inline constexpr float kCameraNearClip = 0.1f;
    inline constexpr float kCameraFarClip = 200.0f;

    inline Camera BuildCamera(float aspectRatio)
    {
        return Camera(
            glm::vec3(0.0f, 8.0f, 45.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            kCameraFovDegrees,
            aspectRatio,
            kCameraNearClip,
            kCameraFarClip
        );
    }

    inline Material MakeOpaqueMaterial(const glm::vec3& albedo, float metallic, float roughness)
    {
        Material material;
        material.albedo = albedo;
        material.metallic = metallic;
        material.roughness = roughness;
        return material;
    }

    inline Material MakeSphereMaterial(std::size_t materialIndex)
    {
        // Four distinct materials; the hashed material state id is what the sort key uses.
        static const glm::vec3 albedos[kSphereMaterialCount] = {
            {0.84f, 0.28f, 0.24f},
            {1.00f, 0.80f, 0.24f},
            {0.15f, 0.72f, 0.64f},
            {0.30f, 0.34f, 0.86f}
        };

        return MakeOpaqueMaterial(
            albedos[materialIndex % kSphereMaterialCount],
            0.25f * static_cast<float>(materialIndex % kSphereMaterialCount),
            0.20f + 0.15f * static_cast<float>(materialIndex % kSphereMaterialCount)
        );
    }

    inline Scene Build()
    {
        // CPU-only meshes: real bounds and real mesh state ids, zero OpenGL calls.
        auto cubeMesh = Mesh::CreateCube(1.0f, MeshUploadPolicy::CpuOnly);
        auto sphereMesh = Mesh::CreateSphere(1.0f, 24, 16, MeshUploadPolicy::CpuOnly);
        auto groundMesh = Mesh::CreatePlane(16.0f, 6.0f, MeshUploadPolicy::CpuOnly);

        Scene scene;
        scene.GetDirectionalLight().direction = glm::vec3(-0.55f, -0.9f, -0.25f);
        scene.GetDirectionalLight().intensity = 3.8f;
        scene.GetPointLight().intensity = 0.0f;
        scene.GetPointLight().range = 0.0f;

        {
            RenderObject ground;
            ground.name = "Ground";
            ground.mesh = groundMesh;
            ground.material = MakeOpaqueMaterial(glm::vec3(0.52f, 0.54f, 0.58f), 0.0f, 0.92f);
            scene.AddObject(std::move(ground));
        }

        // Objects are added interleaved on purpose: in authoring order the scene never
        // has two batch-compatible neighbours, so every batch below is produced by
        // RenderQueue::Sort rather than by lucky insertion order.
        const Material cubeMaterial = MakeOpaqueMaterial(glm::vec3(0.62f, 0.63f, 0.66f), 0.0f, 0.55f);
        for (std::size_t index = 0; index < kCubeCount; ++index)
        {
            {
                const int gridX = static_cast<int>(index) % kCubeGridSide;
                const int gridZ = static_cast<int>(index) / kCubeGridSide;
                RenderObject cube;
                cube.name = "Cube" + std::to_string(index);
                cube.mesh = cubeMesh;
                cube.transform.position = glm::vec3(
                    (static_cast<float>(gridX) - 5.5f) * 0.6f,
                    0.5f,
                    (static_cast<float>(gridZ) - 5.5f) * 0.6f
                );
                cube.material = cubeMaterial;
                scene.AddObject(std::move(cube));
            }

            if (index < kSphereCount)
            {
                const int gridX = static_cast<int>(index) % kSphereGridSide;
                const int gridZ = static_cast<int>(index) / kSphereGridSide;
                RenderObject sphere;
                sphere.name = "Sphere" + std::to_string(index);
                sphere.mesh = sphereMesh;
                sphere.transform.position = glm::vec3(
                    (static_cast<float>(gridX) - 3.5f) * 0.7f,
                    2.5f,
                    (static_cast<float>(gridZ) - 3.5f) * 0.7f
                );
                sphere.transform.scale = glm::vec3(0.25f);
                sphere.material = MakeSphereMaterial(index % kSphereMaterialCount);
                scene.AddObject(std::move(sphere));
            }

            if (index < kTransparentCount)
            {
                RenderObject glass;
                glass.name = "Glass" + std::to_string(index);
                glass.mesh = cubeMesh;
                glass.transform.position = glm::vec3(
                    (static_cast<float>(index) - 11.5f) * 0.3f,
                    3.4f,
                    0.0f
                );
                glass.transform.scale = glm::vec3(0.25f);
                glass.material = MakeOpaqueMaterial(glm::vec3(0.70f, 0.82f, 0.92f), 0.0f, 0.10f);
                glass.material.opacity = 0.45f;
                glass.material.blendMode = MaterialBlendMode::AlphaBlend;
                scene.AddObject(std::move(glass));
            }
        }

        return scene;
    }
}
