# CGEngine Architecture Guide

This file is a reading companion for the code comments. It explains how to navigate the project from top to bottom.

## 1. Startup Path

Read these files in order:

1. `Sandbox/main.cpp`
2. `Engine/Runtime/Application.cpp`
3. `Engine/Platform/Window.cpp`

What happens:

- `main.cpp` creates `Application`
- `Application` creates the window, camera, demo scene, and renderer
- `Application::Run()` drives the frame loop
- each frame is staged as `BeginFrame -> UpdateSystems -> BuildRenderWorld -> BuildRenderGraph -> ExecuteRenderGraph -> EndFrame`

## 2. Scene/Data Model

Relevant files:

- `Engine/Scene/Scene.h`
- `Engine/Scene/Transform.h`
- `Engine/Scene/Material.h`
- `Engine/Scene/Light.h`
- `Engine/Scene/Environment.h`

Key idea:

- `Scene` is the authoring/data container
- it owns `RenderObject` entries plus global lights/environment
- it does not directly know anything about GPU passes
- it exposes a content version so render-side caches know when to rebuild
- `Material` now also carries renderer-facing state such as blend mode, cull mode, opacity, and shadow participation

## 3. Asset Loading

Relevant files:

- `Engine/Assets/ResourceManager.cpp`
- `Engine/Assets/AssetCache.h`
- `Engine/Assets/GLTFLoader.cpp`
- `Engine/Core/JobSystem.cpp`

Key idea:

- `ResourceManager` caches reusable runtime assets like shaders and built-in meshes
- `JobSystem` owns the shared worker pool used by renderer and asset systems
- HDR environment maps can now be loaded asynchronously through `ResourceManager` and finalized into `Scene` once ready
- glTF content can now be decoded asynchronously into CPU-only `DecodedSceneModel` data, then finalized into runtime meshes/materials on the main thread
- imported glTF materials are promoted into shared `MaterialAsset` objects plus per-object `MaterialInstance` handles
- imported meshes/materials are flattened into a simple list of render objects

Why flatten:

- render passes become simpler
- the CPU ray tracer can reuse the same data more directly
- later systems like culling/sorting can operate on a draw list instead of a scene graph

Async model loading flow:

1. `ResourceManager::LoadDecodedModelAsync()` queues a decode task on `JobSystem`
2. `Application::PumpAsyncLoads()` waits for completion without blocking the frame loop
3. `GLTFLoader::AppendDecodedModelToScene()` uploads textures, creates meshes/material assets, and appends render objects on the main thread

## 4. CPU Submission Step

Relevant files:

- `Engine/Renderer/RenderWorld.h`
- `Engine/Renderer/RenderWorld.cpp`
- `Engine/Renderer/RenderSubmission.h`
- `Engine/Renderer/RenderSubmission.cpp`

Key idea:

- there is a deliberate separation between `Scene`, `RenderWorld`, and `RenderSubmission`
- `RenderWorld` is the renderer-facing extracted world: view, visible lights, visible objects, and per-frame/per-view/per-object data
- `RenderSubmission` is the final draw-command layer built from `RenderWorld`
- passes consume `RenderWorld + RenderSubmission`, not `Scene`

Current submission contents:

- `RenderScene`
- `ViewInfo`
- `VisibleLightList`
- `VisibleSet`
- `PerFrameData`, `PerViewData`, `PerObjectData`
- `RenderQueue` containing sorted `MeshDrawCommand` lists per pass plus instanced draw batches

Current behavior:

- `RenderWorld::BuildVisibleSet()` performs frustum culling against mesh bounding spheres
- `RenderWorld::BuildPassMask()` routes objects into opaque or transparent queues from material render state
- `RenderSubmission` sorts opaque draws by render state plus material/mesh state and groups contiguous compatible commands into instanced batches
- transparent draws are depth-sorted but intentionally not instanced so blend order stays stable enough for a forward renderer
- shadow submission groups by mesh/render state and skips materials that opt out of shadows

This is the natural place to later add:

- multiple view submissions
- GPU-driven culling / indirect generation
- clustered or tiled light assignment

## 5. Renderer Orchestration

Relevant files:

- `Engine/Renderer/Renderer.cpp`
- `Engine/Renderer/RenderGraph.h`
- `Engine/Renderer/RenderGraph.cpp`

Frame flow:

1. `BeginFrame()` updates viewport-dependent renderer state
2. `BuildRenderWorld()` rebuilds or reuses `RenderSubmission`
3. `BuildRenderWorld()` computes the light-space matrix for the shadow pass
4. `BuildRenderGraph()` declares passes and compiles a validated execution plan
5. `ExecuteRenderGraph()` kicks or consumes the async CPU reference job
6. `ExecuteRenderGraph()` composites to the swapchain framebuffer

Current pass order:

- `shadow`
- `scene`
- `bloom`
- `reference`
- `composite`

`reference` is logically independent from the realtime passes because it works from `Scene + Camera`, not from GPU textures.

`RenderGraph` is now resource-aware instead of only being an ordered callback list:

- passes are registered as typed handles; pass names are debug labels, not dependency keys
- passes carry an explicit type (`Graphics`, `Compute`, or `CPU`); the reference pass is modeled as a CPU pass
- resources are registered as typed handles, not passed around as raw strings
- imported resources model externally owned inputs such as `RenderWorld`, `RenderSubmission`, camera data, and the swapchain backbuffer
- graph resources model produced textures/framebuffers such as shadow maps, scene MRT outputs, bloom output, and reference color
- graph resources follow a single-producer/versioned-resource model; multiple writes to the same logical resource should be represented as separate resource versions
- every pass declares `reads`
- every pass declares `writes`
- every pass declares a `target`
- passes may declare explicit typed-handle `dependencies` for non-resource ordering constraints
- passes are normally declared through the `RenderGraph::PassBuilder` chain: `AddPass(name).Type(...).Read(...).Write(...).Target(...).Execute(...)`
- `Compile()` automatically derives producer/consumer dependencies from resource reads and writes
- resource dependency compilation is based on producer/consumer sets, not on pass declaration order
- `Compile()` validates duplicate pass names, invalid dependencies, missing execute callbacks, dependency cycles, and invalid resource use
- `Compile()` also builds a resource lifetime table with first use, last use, read count, write count, and target count per resource
- `Compile()` also builds resource transition metadata for producer/consumer edges, including source pass, destination pass, and read/write/target access types
- `Compile()` also builds execution levels, grouping passes whose dependencies are satisfied at the same topology depth
- compiled graph data is available through read-only accessors for passes, resources, execution order, execution levels, lifetimes, and transitions
- `Execute()` only walks the compiled pass order; it no longer re-solves the graph at execution time

## 6. Main Realtime Pass

Relevant files:

- `Engine/Renderer/ScenePass.cpp`
- `Engine/Renderer/MaterialBinder.cpp`
- `Engine/RHI/RenderBufferTypes.h`
- `Shaders/pbr.vert`
- `Shaders/pbr.frag`
- `Shaders/sky.frag`

What `ScenePass` does:

- uploads `Frame` and `Lighting` UBOs
- ensures environment map and BRDF LUT are ready
- renders a fullscreen sky background
- renders all draw items with forward PBR shading
- writes extra MRT outputs for debugging

Material binding flow:

- `ScenePass` calls `MaterialBinder::Bind(material)`
- `MaterialBinder` allocates a slice from the material uniform ring buffer and binds that range
- `MaterialBinder` binds real textures or default fallback textures
- `pbr.frag` reads both the material block and texture flags
- `ScenePass` applies per-batch blend and cull state before issuing the instanced draw

Geometry submission flow:

- `RenderSubmission` exposes instanced batches instead of only one-command-at-a-time iteration
- `ScenePass` uploads one `ObjectData` UBO ring slice per batch chunk and issues `glDrawElementsInstanced`
- `pbr.vert` fetches per-instance model matrices from the bound object uniform block through `gl_InstanceID`
- `ShadowPass` reuses the same object-ring submission path for the directional shadow map
- transparent geometry still goes through the same submission layer, but one batch maps to one draw so ordering is preserved

## 7. Post Process

Relevant files:

- `Engine/Renderer/BloomPass.cpp`
- `Engine/Renderer/CompositePass.cpp`
- `Shaders/blur.frag`
- `Shaders/composite.frag`

What happens:

- `BloomPass` blurs the bright-pass texture using ping-pong framebuffers
- `CompositePass` combines scene color + bloom
- the same composite shader can also show debug textures or the realtime/reference split view

## 8. Offline Reference Path

Relevant files:

- `Engine/Renderer/RayTracer.cpp`
- `Engine/Assets/EnvironmentLighting.cpp`

Purpose:

- provide a second renderer to compare against the realtime path
- reuse the same scene, materials, and environment inputs
- run asynchronously on `JobSystem` so the main window stays responsive

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

- `Engine/RHI/ShaderBuffer.h`
- `Engine/RHI/ShaderBufferManager.h`
- `Engine/RHI/RenderBufferTypes.h`

Current UBO slots:

- `Frame`
- `Lighting`
- `Material`
- `Object`

Rule of thumb:

- if data changes once per frame, it belongs in `Frame` or `Lighting`
- if data changes per material draw state, it belongs in `Material`
- if data changes per draw or per instance transform chunk, it belongs in `Object`
- if data becomes large and many-draw, the next likely step is SSBOs + OpenGL 4.3+

## 10. Best Reading Order For Understanding

If you want the shortest route to understanding the project, read in this order:

1. `Engine/Runtime/Application.cpp`
2. `Engine/Scene/Scene.h`
3. `Engine/Renderer/RenderSubmission.cpp`
4. `Engine/Renderer/Renderer.cpp`
5. `Engine/Renderer/ScenePass.cpp`
6. `Shaders/pbr.vert`
7. `Shaders/pbr.frag`
8. `Engine/Renderer/CompositePass.cpp`
9. `Engine/Renderer/RayTracer.cpp`
10. `Engine/Assets/GLTFLoader.cpp`

That path follows the actual runtime flow instead of reading the repository alphabetically.
