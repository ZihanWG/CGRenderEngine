# CGEngine

CGEngine is a compact C++17/OpenGL 3.3 rendering-engine prototype. It focuses on a readable end-to-end rendering architecture rather than a complete game-engine feature set.

The sample application includes:

- forward metallic/roughness PBR rendering;
- HDR or procedural environment lighting;
- directional shadows, bloom, tone mapping, and debug views;
- asynchronous glTF and HDR decoding;
- CPU frustum culling, render sorting, and instanced submission;
- a resource-aware render graph;
- an asynchronous CPU ray-traced reference image for visual comparison.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the runtime flow and recommended code-reading order.

## Requirements

- CMake 3.20 or newer
- A C++17 compiler
- A GPU and driver supporting OpenGL 3.3 Core

GLFW, GLAD, GLM, TinyGLTF, and stb_image are vendored under `ThirdParty`, so no package-manager setup is required on Windows. Linux builds may need the development packages used by GLFW (X11/Wayland and OpenGL).

## Configure and build

```powershell
cmake -S . -B out/build -DCGENGINE_BUILD_TESTS=ON
cmake --build out/build --config Debug
```

Run the sample from the repository root so project assets can be found:

```powershell
.\out\build\bin\Debug\Sandbox.exe
```

To build the editor entry point as well:

```powershell
cmake -S . -B out/build -DCGENGINE_BUILD_EDITOR=ON -DCGENGINE_BUILD_TESTS=ON
cmake --build out/build --config Debug
```

## Tests

The tests are CPU-only and do not require an OpenGL window:

```powershell
ctest --test-dir out/build -C Debug --output-on-failure
```

## Controls

- `WASD`, `Q/E`: move the camera
- Right mouse button: look around
- `Shift`: move faster
- `B`, `N`, `M`: toggle bloom, shadows, and IBL
- `F`: toggle FXAA
- `C`: toggle realtime/reference comparison
- `R`: rebake the reference image
- `1`-`8`: switch debug views
- `+/-`: adjust exposure
- `[` / `]`: adjust comparison split

The optional `CGEngineEditor` executable adds keyboard-driven scene editing:

- `Tab`: select the next object
- Arrow keys and `Page Up`/`Page Down`: move the selected object
- `J/K`: decrease/increase roughness
- `U/I`: decrease/increase metallic

## Repository layout

- `Engine/Core`: paths and shared job system
- `Engine/Platform`: window and graphics-context creation
- `Engine/Scene`: authoring-side scene, camera, light, transform, and material data
- `Engine/RHI`: OpenGL resource wrappers
- `Engine/Assets`: caching, environment loading, and glTF import
- `Engine/Renderer`: extraction, submission, render graph, passes, and reference tracer
- `Engine/Runtime`: runnable application orchestration
- `Sandbox`: sample executable
- `Editor`: optional editor executable
- `Tests`: CPU-side regression tests
