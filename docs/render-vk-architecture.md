# render_vk architecture

Status: N0 is implemented and verified, and the first in-game race frame is rendered by the new
renderer. The new renderer is a clean C11 Vulkan module in `src/render_vk/`. It has no BRender
headers, types, callbacks or link dependency. The legacy renderer remains a behavioral reference
for what is drawn and in what order, and a fallback for software/OpenGL; it is not the base of the
new renderer.

## Decision

- **Language and API:** C11, opaque `RvkDevice`, flat C functions, explicit `RvkResult` codes.
  No C++, NVRHI, STL, exceptions or VMA.
- **Platform boundary:** the host supplies Vulkan loading and surface creation through
  `RvkPlatform`. `render_vk` includes no SDL, Win32 or BRender header.
- **Build boundary:** `DETHRACE_RENDER_VK` is independent of `DETHRACE_VULKAN`.
  `DETHRACE_VULKAN` still means the legacy `vkrend` driver and its presenter. A build can have
  both, either, or neither. The renderer library links no Vulkan loader; the host loads it.
- **N0 scope:** instance/device/swapchain, FIFO and IMMEDIATE present modes, clear + present,
  resize, validation, CPU timing and a standalone SDL2/SDL3 smoke test. No shaders, pipelines,
  descriptors, assets or game integration.
- **Deferred until measured:** bindless descriptors, indirect draws, a general frame graph,
  transient aliasing, and 2-3 frames in flight. The first draw implementation will use an explicit
  pass list, descriptor arrays and one frame in flight; the expensive features are added only
  when a frame-level profile justifies them.

## Boundary

```text
game state and BRender assets
  -> game_render bridge (outside src/render_vk)
  -> neutral draw/view/UI lists
  -> rvk API
  -> src/render_vk (Vulkan only)
  -> present
```

The bridge is the only layer that may know `br_actor`, `br_model`, `br_material`, `br_pixelmap`
or BRender renderer state. It may read assets at load time and extract a neutral scene at the
game's submission points. The runtime renderer never includes `brender.h`, never calls a `Br*`
function, and never uses the BRender DDI.

The asset path has two forms:

- An offline importer for the N1 viewer may link BRender as a parser and write a versioned neutral
  format. The runtime renderer loads only that format.
- Game content that is modified at runtime (damage, paint, texture animation, add-ons, shadows)
  needs a load-time or frame-boundary bridge with dirty tracking. An offline-only format cannot
  represent those mutations.

## N0 implementation

### Public API

`src/render_vk/include/rvk.h` exposes:

- `RvkResult`, `RvkPresentMode`, `RvkPass`, `RvkStats`.
- `RvkPlatform`: `load_vulkan`, `unload_vulkan`, `get_instance_proc_addr`,
  `get_instance_extensions`, `create_surface`, `get_error`, plus an opaque user pointer.
- `RvkInitInfo`: app name, size, platform, validation flag, present mode, log callback.
- `RvkPassInfo`: pass id, clear flag, clear colour.
- Lifecycle: `rvk_create`, `rvk_destroy`, `rvk_resize`, `rvk_wait_idle`.
- Frame: `rvk_begin_frame`, `rvk_begin_pass`, `rvk_end_pass`, `rvk_end_frame`.
- Introspection: `rvk_get_stats`, `rvk_get_adapter_name`, `rvk_result_string`.

The API does not expose `VkInstance`, `VkDevice`, `VkImage` or `VkSurfaceKHR`. The host creates
the surface from a `uint64_t` instance handle and returns a `uint64_t` surface handle.

### Modules

| Path | Responsibility |
| --- | --- |
| `src/render_vk/include/rvk.h` | public C API |
| `src/render_vk/src/rvk_internal.h` | Vulkan dispatch table, device/swapchain state |
| `src/render_vk/src/rvk_context.c` | instance, validation, physical device, device, queues, surface |
| `src/render_vk/src/rvk_swapchain.c` | swapchain, render pass, image views, framebuffers, resize |
| `src/render_vk/src/rvk_frame.c` | acquire, command buffer, clear pass, submit, present |
| `src/render_vk/src/rvk_api.c` | logging, monotonic clock, result strings, stats |
| `src/render_vk/smoke/rvk_smoke.c` | CLI, frame loop, resize test, CSV, percentiles |
| `src/render_vk/smoke/rvk_smoke_sdl2.c` | SDL2 host adapter and `main` |
| `src/render_vk/smoke/rvk_smoke_sdl3.c` | SDL3 3.2+ host adapter and `main` |
| `src/render_vk/CMakeLists.txt` | static library and smoke executables |

The renderer loads Vulkan entry points through the host-provided
`vkGetInstanceProcAddr`. Global functions are resolved before instance creation; instance and
device functions afterwards. `VK_NO_PROTOTYPES` keeps the loader out of the link line.

### N0 frame model

- Exactly one frame in flight. `rvk_begin_frame` waits the frame fence, resets and begins the
  command buffer, then acquires an image. `rvk_end_frame` ends the command buffer, resets the
  frame fence, submits with the acquire semaphore, presents, and measures CPU time. The fence is
  reset only immediately before submit; a failed reset or submit poisons the device so the next
  frame cannot wait. An out-of-date acquire leaves the fence signaled.
- One render-finished semaphore per swapchain image. This avoids reusing a binary semaphore while
  the presentation engine still holds it; validation flagged the single-semaphore version.
- FIFO is the default. IMMEDIATE is opt-in and fails loudly if the surface does not offer it.
- Resize is deferred to the next `rvk_begin_frame`. `VK_ERROR_OUT_OF_DATE_KHR` and
  `VK_SUBOPTIMAL_KHR` recreate the swapchain; zero-sized/minimized frames return out-of-date
  without spinning.
- Validation is opt-in. The smoke counts only `VALIDATION`/`PERFORMANCE` warnings and errors;
  loader noise and layer-manifest warnings are not validation results.
- The smoke captures CPU frame and wait time, writes a CSV, and reports mean/p50/p95/p99.

### Build

```powershell
cmake -S . -B build-rvk -G "Visual Studio 17 2022" -A x64 `
  -DDETHRACE_RENDER_VK=ON -DDETHRACE_VULKAN=OFF `
  -DDETHRACE_PLATFORM_SDL2=ON `
  -DSDL2_DIR=C:/Users/f1am3d/repos/SDL2-2.30.10/SDL2-2.30.10/cmake
cmake --build build-rvk --config Debug --target render_vk_smoke_sdl2
```

The target is forced off under `MSVC_42_FOR_RECCMP`. The library compiles without SDL; only the
smoke executables need an SDL platform. SDL3 support requires SDL 3.2.0 or newer.

### N0 evidence

- Debug build with `DETHRACE_VULKAN=OFF`, `DETHRACE_RENDER_VK=ON`: library and SDL2 smoke build.
- Full `dethrace` target builds with the same configuration; the new renderer is linked into the
  game when `DETHRACE_RENDER_VK=ON` and selected with `--render-vk`.
- 90-frame smoke with `--validate --resize-test`: 640x480 -> 800x600 -> 640x480, zero validation
  warnings/errors, exit 0.
- 90-frame smoke with `--validate --acquire-ood-test`: window resized without notifying the
  renderer first, one out-of-date frame observed, no hang, zero validation warnings/errors,
  exit 0.
- 120-frame smoke with `--present immediate --validate`: IMMEDIATE selected, zero validation
  warnings/errors, mean CPU about 0.2 ms for clear + present. This is a skeleton measurement, not
  a renderer or 200 FPS result.
- The smoke links SDL2 and loads Vulkan at runtime; the renderer link line has no BRender target
  and no Vulkan import library.
- `rg "brender|br_actor|br_model|br_material|br_pixelmap|Br[A-Z]" src/render_vk` returns nothing.

### First in-game race frame

The new path is selected with `--render-vk`. A test hook, `RVK_AUTOSTART=1`, starts a one-player
race without menu input; it works in both renderer builds so the same pose can be captured with
the legacy path for comparison. It is a verification aid, not a shipping default. The log shows
`VKREND: race loop started`, and the new renderer builds and submits a world pass every frame. The
first captured race frame is `docs/images/render-vk-first-race.png`; it shows the track and the
player car upright, with the existing HUD overlay.

The frame uses:

- `src/DETHRACE/common/render_vk_bridge.c` for BRender data extraction and the CPU 2D overlay;
- `rvk_draw_world` with per-actor model matrices, a shared view-projection matrix, per-draw
  materials and textures;
- `rvk_draw_overlay` for the existing 16-bit/8-bit HUD surface while the UI is ported;
- `rvk_create_texture` with bridge-side texture caching, and prepared `v11group` data for track
  models whose original vertex/face arrays were released by `DodgyModelUpdate`.

The first measured scene has about 3512 draws, 39917 vertices and 70020 indices. The CPU bridge
walks the live BRender scene every frame with no batching, indirect submission, shadows, mirror,
cockpit target, smoke/spark effect or full UI pass. The horizon/sky pass is also missing, so the
track backdrop does not yet match the legacy capture.

A preliminary performance run used the Release build, 640x480, IMMEDIATE present, `--fps=0` and
`RVK_AUTOSTART=1` with `RVK_FPS_LOG=1`. One 60-second stationary start scene produced 1016-1052
presented frames per second (59 one-second samples, about 1013 frames/s including the warm-up
sample), with 3512 draws. This exceeds the 200 FPS target for this scene. It is not the formal N4
acceptance: that still needs 5 x 60 s moving-race runs, p50/p95/p99 and the parity gates.

Depth needs the BRender convention, not the usual OpenGL one. BRender's perspective maps the near
plane to `z/w = +1` and the far plane to `-1`. The world shader therefore writes
`(clip.w - clip.z) * 0.5` and the pipeline compares depth with `LESS_OR_EQUAL`; using
`(clip.z + clip.w) * 0.5` hides the player car behind the track. Actor children are traversed even
when a parent's render style is `BR_RSTYLE_NONE`, because the car uses a non-rendering master actor
above its model actors. `docs/images/legacy-race-reference.png` is the legacy vkrend capture from
the same autostart pose.

## N1 plan

The runtime bridge (N3) was brought forward to prove the boundary. N1 still needs the standalone
viewer and offline importer:

1. Offline importer (BRender allowed as parser) writes a versioned neutral scene: meshes, indices,
   submeshes, bounds, materials, texture blobs and node transforms.
2. `rvk_mesh`, `rvk_texture`, `rvk_material` and one textured pipeline with depth.
3. A fixed camera and a screenshot path; compare against a software reference.
4. Keep the runtime bridge as the game-facing path; add dirty tracking and batching before it can
   be considered a renderer core.

N1 exit: imported counts match the source, the viewer links no BRender, the renderer includes no
BRender headers, one screenshot matches the software reference within the comparator tolerance,
and resize/validation/timing still pass.

The comparator must be used with a real reference set. The rewrite plan's claim of 11 committed
references is not true in this tree, and `tools/compare_vkrend_frames.ps1` had a 24-bit stride bug
for 640x480; the stride now uses `floor`. References still need to be captured and committed.

## N2/N3 constraints

- The pass set is fixed by the game, not by a generic graph: sky, world opaque, shadows, world
  blended, mirror, map/cockpit, UI, composite. A graph compiler is deferred until the explicit
  list is measured.
- Start with one frame in flight, one descriptor set per frame slot, a static vertex/index arena
  and a dynamic ring. Add a second slot only if the profile shows the CPU waiting on presentation.
- Write descriptors before binding; never update a bound set. This is a hard rule.
- The UI is a real 2D renderer: RGBA8 atlas/quads, alpha, scissor, dirty rects. A full-surface CPU
  shadow upload is a temporary bridge, not the target.
- Shadows need per-draw clip planes. Vulkan has no fixed-function clip planes; implement them in
  the material path before batching shadows.
- The mirror and wreck are explicit sub-rect composites, not targets selected by draw count.
- The game's 2D and 3D layers are interleaved: map underlay before the world viewport, cockpit,
  mirror inset, then HUD/pratcam/instruments. A single trailing UI pass will break parity.
- The bridge snapshots transforms and material state at submission. BRender models, materials and
  actors mutate during the frame; the renderer must not walk the live scene later.
- Single-threaded. No worker threads, job system or secondary command buffers.

## Game integration

`RenderAFrame` keeps the legacy calls for software/OpenGL. Under `DETHRACE_RENDER_VK` and
`--render-vk`, it replaces the `BrZbSceneRenderBegin/Add/End` block with
`GameRender_BeginScene`/`GameRender_AddUniverse`/`GameRender_EndScene`; the point lights, shadows,
mirror and effects blocks are not called yet. The CPU 2D code still writes the existing HUD
surface, and `GameRender_Swap` presents the new renderer's world plus that surface as an overlay.

The legacy path stays selectable for A/B. The new path must not coexist with the legacy vkrend
driver or the harness Vulkan presenter in one process: one component owns the Vulkan instance,
device, swapchain and present queue.

## Measurement and acceptance

The D0 `scene_us` value is not a whole-game-frame budget. It starts at the first scene and ends at
the scene submit, excludes simulation and most 2D/present work, and can span skipped swaps. The
200 FPS gate remains unmeasured.

Before a 200 FPS claim, measure on a real race:

- simulation, bridge extraction, culling/sort, HUD, command recording, submit, present wait;
- draws, passes, descriptor binds, uploads, pipeline switches;
- CPU mean/p50/p95/p99 and GPU pass timestamps;
- render-loop and presented rates separately.

The final acceptance remains the one in `plan.md`: authorized optimized build, 640x480, IMMEDIATE,
median mean <= 4.75 ms, every run <= 5.00 ms, p99 <= 6.5 ms, <= 0.5% frames over 5 ms, with FIFO
and Debug functional runs reported separately.

## What not to do

- Do not add BRender includes, types or DDI callbacks to `src/render_vk`.
- Do not link `harness` or `dethrace_obj` into the renderer to reuse its window or presenter; both
  pull BRender.
- Do not use `VkPresent_Init` from `src/harness/platforms/vulkan/vk_present.c` as the new
  renderer's context; it is a software-frame overlay presenter with shaders and descriptors.
- Do not build bindless, indirect, a frame graph or multi-FIF before the frame-level profile shows
  the legacy overhead survives the explicit pass list.
- Do not claim 200 FPS from a clear-only smoke or from D0 scene numbers.

## Risks

- The asset bridge and UI port are larger than the N0 skeleton. Runtime material and geometry
  mutation is the main correctness risk.
- The software and 3dfx asset/effect modes differ; one reference mode must be chosen and
  documented before parity claims.
- Windowed IMMEDIATE can be compositor-limited; render-loop throughput and presented rate must be
  reported separately.
- SDL3 support is written against 3.2.0 and is not exercised by the SDL2 build on this machine.
- The Vulkan loader environment contains third-party implicit layers; validation runs should
  disable them with `VK_LOADER_LAYERS_DISABLE=~implicit~`.
