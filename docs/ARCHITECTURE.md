# CGEngine Architecture Guide

This file is a reading companion for the code comments. It explains how to navigate the project from top to bottom.

## 1. Startup Path

Read these files in order:

1. `src/main.cpp`
2. `src/Core/Application.cpp`
3. `src/Core/Window.cpp`

What happens:

- `main.cpp` creates `Application`
- `Application` creates the window, camera, demo scene, and renderer
- `Application::Run()` drives the frame loop
- each frame polls input, updates the camera/settings, renders, and swaps buffers

## 2. Scene/Data Model

Relevant files:

- `src/Scene/Scene.h`
- `src/Scene/Transform.h`
- `src/Scene/Material.h`
- `src/Scene/Light.h`
- `src/Scene/Environment.h`

Key idea:

- `Scene` is the authoring/data container
- it owns `RenderObject` entries plus global lights/environment
- it does not directly know anything about GPU passes
- it exposes a content version so render-side caches know when to rebuild

## 3. Asset Loading

Relevant files:

- `src/Assets/ResourceManager.cpp`
- `src/Assets/AssetCache.h`
- `src/Assets/GLTFLoader.cpp`

Key idea:

- `ResourceManager` caches reusable runtime assets like shaders and built-in meshes
- `GLTFLoader` converts glTF content into the engine's own `Scene` representation
- imported meshes/materials are flattened into a simple list of render objects

Why flatten:

- render passes become simpler
- the CPU ray tracer can reuse the same data more directly
- later systems like culling/sorting can operate on a draw list instead of a scene graph

## 4. CPU Submission Step

Relevant files:

- `src/Renderer/RenderSubmission.h`
- `src/Renderer/RenderSubmission.cpp`

Key idea:

- there is a deliberate separation between `Scene` and `RenderSubmission`
- `RenderSubmission` is the per-frame, render-ready view of the scene
- passes only consume `RenderSubmission`, not `Scene`

Current submission contents:

- flat `drawItems`
- baked model matrices
- copied global light/environment state

This is the natural place to later add:

- frustum culling
- material sorting
- batching
- instancing
- multiple view submissions

## 5. Renderer Orchestration

Relevant files:

- `src/Renderer/Renderer.cpp`
- `src/Renderer/RenderGraph.h`

Frame flow:

1. detect scene or viewport invalidation
2. rebuild or reuse `RenderSubmission`
3. compute light-space matrix for the shadow pass
4. schedule passes through `RenderGraph`
5. kick or consume the async CPU reference job
6. composite to the swapchain framebuffer

Current pass order:

- `shadow`
- `scene`
- `bloom`
- `reference`
- `composite`

`reference` is logically independent from the realtime passes because it works from `Scene + Camera`, not from GPU textures.

## 6. Main Realtime Pass

Relevant files:

- `src/Renderer/ScenePass.cpp`
- `src/Renderer/MaterialBinder.cpp`
- `src/Renderer/RenderBufferTypes.h`
- `shaders/pbr.vert`
- `shaders/pbr.frag`
- `shaders/sky.frag`

What `ScenePass` does:

- uploads `Frame` and `Lighting` UBOs
- ensures environment map and BRDF LUT are ready
- renders a fullscreen sky background
- renders all draw items with forward PBR shading
- writes extra MRT outputs for debugging

Material binding flow:

- `ScenePass` calls `MaterialBinder::Bind(material)`
- `MaterialBinder` uploads the material UBO
- `MaterialBinder` binds real textures or default fallback textures
- `pbr.frag` reads both the material block and texture flags

## 7. Post Process

Relevant files:

- `src/Renderer/BloomPass.cpp`
- `src/Renderer/CompositePass.cpp`
- `shaders/blur.frag`
- `shaders/composite.frag`

What happens:

- `BloomPass` blurs the bright-pass texture using ping-pong framebuffers
- `CompositePass` combines scene color + bloom
- the same composite shader can also show debug textures or the realtime/reference split view

## 8. Offline Reference Path

Relevant files:

- `src/Renderer/RayTracer.cpp`
- `src/Renderer/EnvironmentLighting.cpp`

Purpose:

- provide a second renderer to compare against the realtime path
- reuse the same scene, materials, and environment inputs
- run asynchronously so the main window stays responsive

Acceleration structure flow:

1. flatten scene meshes into world-space triangles
2. build a BVH over triangle centroids
3. cache the result using a geometry hash
4. reuse it across reference re-bakes until geometry changes

Shading flow:

1. intersect BVH
2. sample material textures on CPU
3. evaluate directional light and point light
4. add environment ambient term
5. optionally recurse for one glossy bounce

## 9. Uniform/Data Ownership

Relevant files:

- `src/Renderer/ShaderBuffer.h`
- `src/Renderer/ShaderBufferManager.h`
- `src/Renderer/RenderBufferTypes.h`

Current UBO slots:

- `Frame`
- `Lighting`
- `Material`

Rule of thumb:

- if data changes once per frame, it belongs in `Frame` or `Lighting`
- if data changes per draw, it belongs in `Material`
- if data becomes large and many-draw, the next likely step is SSBOs + OpenGL 4.3+

## 10. Best Reading Order For Understanding

If you want the shortest route to understanding the project, read in this order:

1. `src/Core/Application.cpp`
2. `src/Scene/Scene.h`
3. `src/Renderer/RenderSubmission.cpp`
4. `src/Renderer/Renderer.cpp`
5. `src/Renderer/ScenePass.cpp`
6. `shaders/pbr.vert`
7. `shaders/pbr.frag`
8. `src/Renderer/CompositePass.cpp`
9. `src/Renderer/RayTracer.cpp`
10. `src/Assets/GLTFLoader.cpp`

That path follows the actual runtime flow instead of reading the repository alphabetically.
