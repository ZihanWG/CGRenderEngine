# CGEngine Project Context

## Project identity

CGEngine is a compact C++17/OpenGL 3.3 rendering-engine prototype. Its primary goals are readable renderer architecture, realtime PBR experimentation, and comparison against an asynchronous CPU reference renderer. It is not yet a full game engine.

## Architecture

- `Engine/Core`: project paths and the shared job system.
- `Engine/Platform`: GLFW window and OpenGL context ownership.
- `Engine/Scene`: scene, transform, camera, lights, environment, and materials.
- `Engine/RHI`: OpenGL resource wrappers.
- `Engine/Assets`: resource caching, HDR loading, and CPU-side glTF decoding.
- `Engine/Renderer`: render extraction, submission, RenderGraph, realtime passes, and CPU reference tracing.
- `Engine/Runtime`: frame orchestration and input handling.
- `Sandbox`: realtime renderer sample.
- `Editor`: minimal keyboard-driven editor executable.
- `Tests`: CPU regression tests for scene/material behavior, RenderGraph, and glTF import.

The runtime data path is:

`Scene -> RenderWorld -> RenderSubmission -> RenderGraph -> render passes`

Read `docs/ARCHITECTURE.md` for the detailed frame flow and recommended code-reading order.

## Current capabilities

- Forward metallic/roughness PBR with material textures and normal mapping.
- HDR/procedural environment lighting and approximate IBL.
- Directional shadows, bloom, ACES-style tone mapping, FXAA, and debug views.
- Frustum culling, state sorting, instanced opaque/shadow submission, and depth-sorted transparency.
- Asynchronous HDR/glTF decoding and asynchronous low-resolution CPU reference tracing.
- glTF static meshes, `TEXCOORD_0`, normalized integer UVs, metallic/roughness materials, alpha blending, and alpha masking.
- A resource-aware RenderGraph with validation, execution levels, lifetimes, transitions, and cached compilation for fixed topology.

## Build and verification

Use an out-of-source build. Do not add generated build or IDE files to version control.

```sh
cmake -S . -B out/build -DCGENGINE_BUILD_TESTS=ON -DCGENGINE_BUILD_EDITOR=ON
cmake --build out/build --config Debug
ctest --test-dir out/build -C Debug --output-on-failure
```

When changing GLSL, validate every shader when `glslangValidator` is available and perform a runtime visual smoke test when practical.

## Engineering rules

- Preserve unrelated user changes in the worktree.
- Keep GPU/OpenGL calls on the context-owning main thread. Async asset jobs produce CPU-only decoded data; GPU upload/finalization happens on the main thread.
- Render passes consume `RenderWorld` and `RenderSubmission`, not authoring-side `Scene` objects directly.
- Use explicit RenderGraph resource versions instead of reading and writing one logical graph resource in the same pass.
- Add or update regression tests for behavioral changes that can be verified without a graphics window.
- Keep `README.md` and `docs/ARCHITECTURE.md` synchronized with user-visible controls, targets, and architectural changes.
- Do not silently accept unsupported glTF features. Return an actionable decode error instead of importing corrupted geometry or materials.

## Known boundaries

- The RHI is currently OpenGL-specific.
- The glTF importer does not support animation, skinning, morph targets, sparse accessors, secondary UV sets, or non-triangle primitives.
- The RenderGraph models and schedules resources but does not yet allocate transient GPU resources or emit GPU barriers.
- The editor is intentionally minimal: it has keyboard selection, transform editing, and metallic/roughness tuning, but no GUI panels, gizmos, undo/redo, or scene serialization.
- The CPU reference tracer is a low-resolution comparison tool, not production-quality ground truth.
