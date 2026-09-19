# Draw Call Batching Reference

This document records how many draw calls CGEngine submits for one fixed scene,
before and after instanced batching. The measurement is CPU-only integer accounting,
so it is reproducible on any machine and at any resolution: it is measured once and
stays valid until the batching rules or the fixed scene change.

## The fixed scene

`Tests/BatchingScene.h` builds the scene. It is frozen on purpose; editing it
invalidates every number below.

| Group | Count | Mesh | Material | Passes |
| --- | --- | --- | --- | --- |
| Cubes | 144 (12 x 12) | shared cube | one shared opaque material | shadow + opaque |
| Spheres | 64 (8 x 8) | shared sphere | four materials, cycled per object | shadow + opaque |
| Ground | 1 | plane | opaque | shadow + opaque |
| Glass cubes | 24 | shared cube | alpha-blended | transparent |
| **Total** | **233 objects** | 3 meshes | 7 materials | 442 draw commands |

Two properties of the scene matter:

- Objects are added interleaved (cube, sphere, glass, cube, ...), so in authoring order
  no two neighbours are batch-compatible. Every batch below is produced by
  `RenderQueue::Sort`, not by lucky insertion order.
- The cube group has 144 instances, which is more than the 128-matrix object uniform
  block (`kMaxObjectMatricesPerDraw`), so the scene also pins the batch-splitting rule.

The camera sits at `(0, 8, 45)` looking at `(0, 1, 0)` with a 45 degree vertical FOV.
Every object stays inside the frustum for aspect ratios from 9:16 to 32:9, so frustum
culling never removes anything and the counts do not move with the window size.

## Measured result

Produced by `Tests/DrawCallBatchingTests.cpp`, which also asserts these exact values:

```
Fixed scene: 233 objects (144 cubes sharing 1 material, 64 spheres across 4 materials, 1 ground, 24 alpha-blended)
  pass             before   batches     after   reduction
  shadow              209         3         4       98.1%
  opaque              209         6         7       96.7%
  transparent          24        24        24        0.0%
  total               442        33        35       92.1%
```

**442 draw calls before batching, 35 after: a 92.1% reduction.**

Where the numbers come from:

- **Shadow, 209 -> 4.** The shadow pass ignores material differences unless a material
  uses alpha cutoff, so the 64 spheres collapse into one batch despite having four
  materials. 144 cubes -> 1 batch -> 2 draws (128 + 16), 64 spheres -> 1 draw,
  1 ground -> 1 draw.
- **Opaque, 209 -> 7.** Same geometry, but the four sphere materials cannot share a
  draw: 144 cubes -> 2 draws, 64 spheres -> 4 draws of 16, 1 ground -> 1 draw.
- **Transparent, 24 -> 24.** Instancing is deliberately disabled for the transparent
  queue (`BuildDrawBatches` is called with `allowInstancing = false`) because merging
  would break back-to-front blend order. This group is in the scene to keep that
  trade-off visible in the numbers: with instancing allowed, these 24 identical cubes
  would collapse into a single draw.

Full-screen work (sky, bloom, tone map, FXAA) is excluded. Those draw calls are a fixed
cost per frame and are unaffected by batching.

## Both figures come from the engine's batching code

Neither number is hand-derived from the object count.

- **After (35)** is counted from the `InstancedDrawBatch` lists the passes will iterate,
  applying the same skip and chunk-split rules as `ShadowPass::Execute` and
  `ScenePass::Execute`.
- **Before (442)** is measured, not assumed to be "one draw per object". `CountDrawCalls`
  re-runs `BuildDrawBatches` over the same command lists with `allowInstancing = false`
  and counts the result identically. That is the real submission path with merging
  switched off, so if the grouping rules change, the before figure moves with them.

The two paths are cross-checked: the test asserts that the instance totals after
batching still equal the command counts, so batching cannot silently drop or duplicate
an object. Flipping that `false` to `true` collapses the before figure to 12 and fails
the test, which is how the plumbing was verified to be live.

## Why the numbers are hardware- and resolution-independent

- **No GPU involved.** Everything above is integer accounting over CPU-side queues. The
  test builds CPU-only meshes (`MeshUploadPolicy::CpuOnly`), so no OpenGL context,
  driver, or window is needed.
- **No timing.** The output is integer counts, not milliseconds, so it cannot drift
  with GPU load, driver version, or thermal state.
- **Resolution proven, not assumed.** The test re-measures the same scene at 1x1,
  1080x1920, 640x480, 1280x720, 1920x1080, 2560x1440, 3840x2160 and 5120x1440, setting
  the camera aspect ratio from the viewport exactly as `Application` does, and requires
  all eight measurements to be identical. It also asserts that all 233 objects survive
  culling at every one of them, which is the reason the counts cannot move.

## Reproducing

```powershell
cmake -S . -B out/build -DCGENGINE_BUILD_TESTS=ON
cmake --build out/build --config Debug
ctest --test-dir out/build -C Debug -R draw_call_batching --output-on-failure
```

Running the test executable directly prints the table above regardless of pass or fail.

## When this test fails

A failure means the batching behaviour changed, not that the test is flaky. Check the
diff against `BuildDrawBatches`, the sort keys in `RenderSubmission.cpp`,
`kMaxObjectMatricesPerDraw`, and the pass submit loops. If the change is intended,
update the expected constants in `Tests/DrawCallBatchingTests.cpp` and the table above
in the same commit.
