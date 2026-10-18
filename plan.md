# New renderer plan

Status: the new renderer must not be based on the legacy BRender/DDI/vkrend code. N0 is
implemented in `src/render_vk/` and verified. The legacy path remains a behavioral reference and
the software/OpenGL fallback; it is not the base of the new renderer. Branch
`feature/vkrend-v1`; engine `b006d70` and dethrace `441327a` plus the N0 work.

## Decisions (2026-10-18)

- `src/render_vk/` is C11 with raw Vulkan; no C++, NVRHI, STL or VMA.
- The public API is Vulkan-free and BRender-free. The host supplies loader and surface callbacks;
  the renderer never includes SDL, Win32 or BRender headers.
- `DETHRACE_RENDER_VK` is independent of `DETHRACE_VULKAN`. The latter remains the legacy vkrend
  switch; the new renderer does not link it.
- N0 is clear/present only: instance/device/swapchain, FIFO default, IMMEDIATE opt-in, resize,
  validation, CPU timing and a standalone SDL2/SDL3 smoke test.
- Defer bindless, indirect draws, a general frame graph, transient aliasing and 2-3 frames in
  flight until a frame-level profile justifies them. Start with an explicit pass list, descriptor
  arrays and one frame in flight.
- The asset path uses an offline importer for the viewer and a runtime bridge for mutable game
  content. BRender may parse assets or expose data to the bridge, but the renderer never calls it.

## Why the legacy path is not the base

The current renderer is a BRender DDI adapter:

- the game calls `BrZbSceneRenderBegin/Add/End` and `BrPixelmapDoubleBuffer`;
- vkrend receives per-scene/per-geometry callbacks and records Vulkan directly;
- the CPU shadow/magenta-key HUD, per-car shadow scenes, per-texture descriptor sets, one
  frame in flight and the BRender state machine remain in the architecture.

D0: 171 draws, 14.4 passes, 13.4 geometry uploads/frame, 15.9 ms scene record, 0.27 ms GPU wait,
45-60 FPS. It is a useful baseline and a working fallback, not a foundation for a modern
renderer. Rewriting inside the DDI would preserve the legacy decomposition.

## Boundary

- **Legacy, frozen:** `lib/BRender-v1.3.2` renderer/DDI, `drivers/vkrend`, the software/OpenGL
  drivers and the game's `RenderAFrame`. Critical fixes only; no new architecture.
- **New runtime:** `src/render_vk/`, a clean Vulkan renderer. Its public API has no BRender
  headers, types or callbacks.
- **Game bridge:** `src/DETHRACE` code extracts a neutral scene from game state/assets and calls
  the new render API. The bridge may use BRender at load/proxy-extraction time; the renderer
  never does.
- **Asset import:** an offline tool converts BRender DAT/model/material/texture data into a
  neutral format. The tool may use BRender as a parser; the runtime renderer loads only the
  neutral format. A runtime import path may exist temporarily for tracks/cars, but it is not
  part of the renderer.
- **UI:** a new 2D renderer (RGBA8 atlas, alpha blend, dirty rects). HUD/map/cockpit/pratcam are
  ported to neutral draw lists; the legacy HUD remains for fallback.
- **Compatibility:** software/OpenGL paths and gameplay/reccmp remain. `src/DETHRACE` behavior
  changes stay under `DETHRACE_FIX_BUGS`; the new path is gated by a renderer selector.

## Architecture

```text
game state / assets
  -> game_render bridge (neutral scene: meshes, materials, transforms, lights, UI)
  -> render_api
  -> render_vk
       context/device/swapchain
       explicit pass list (shadow, world, mirror, UI, composite)
       static/dynamic arenas, descriptor arrays, one frame in flight
       bindless/indirect/frame graph/2-3 FIF only after measurement
       resource manager (static/dynamic arenas, staging ring, pipelines)
  -> present (FIFO default, IMMEDIATE opt-in)
```

- Renderer-owned scene: flat render proxies, frustum culling, opaque/blended buckets, stable
  sort keys; no BRender actor traversal in the render loop.
- Static geometry in a global vertex/index arena; dynamic geometry in per-frame rings; descriptor
  arrays first; indirect draws only if a frame-level profile justifies them.
- Explicit main target; mirror/wreck as sub-rect composites; UI as a separate pass.
- One frame in flight first; 2-3 frame slots with timeline semaphores only after the simple model
  is measured; slot-keyed deferred destruction and slot-aware recovery.
- Single-threaded. No job system, worker threads or secondary command buffers.
- Debug path: validation, GPU timestamps, per-pass CSV, device-fault reporting.

## Phases

### N0 - freeze and skeleton (complete)
Freeze legacy vkrend (critical fixes only). `src/render_vk/` now provides the C11 library, public
API, Vulkan context/device/swapchain, FIFO/IMMEDIATE present, clear pass, resize, validation, CPU
timing and SDL2/SDL3 smoke adapters. No shaders, assets or game integration yet.

Evidence:
- Debug build with `DETHRACE_VULKAN=OFF`, `DETHRACE_RENDER_VK=ON`: library and SDL2 smoke build.
- `dethrace` builds with the same configuration; the renderer is not linked into the game yet.
- 90-frame validation smoke with resize (640x480 -> 800x600 -> 640x480): zero validation
  warnings/errors, exit 0.
- 90-frame validation smoke with a forced acquire-out-of-date resize: one out-of-date frame,
  no hang, zero validation warnings/errors, exit 0; the frame fence remains signaled across the
  out-of-date path.
- 120-frame IMMEDIATE validation smoke: zero validation warnings/errors; mean CPU about 0.2 ms
  for clear + present. This is a skeleton number, not a renderer or 200 FPS result.
- No BRender include or symbol in `src/render_vk`; no Vulkan loader link; no game code changes.
- `tools/compare_vkrend_frames.ps1` 24-bit stride fixed for 640x480; references still need to be
  captured and committed.

### N1 - asset import and parity viewer
Offline importer for one track and one car: meshes, materials, textures, transforms. Standalone
viewer renders the static scene with the new renderer. A runtime bridge prototype for one car
proves mutable transforms/materials. Software-reference screenshots for the same assets.
Exit: imported scene matches the software reference within the comparator tolerance; asset format
documented; runtime renderer has no BRender dependency.

### N2 - renderer core
Explicit pass list: shadow, world, mirror, UI, composite. Static/dynamic arenas, descriptor
arrays, one frame in flight, material/pipeline variants, UI pass, mirror sub-rect composite,
resource lifetime/recovery. No general frame graph, bindless, indirect draws, timeline
semaphores or 2-3 frames in flight until a frame-level profile shows the simpler path is the
bottleneck.

Exit: 640x480 scene at target throughput in the authorized optimized build; validation clean;
p50/p95/p99 recorded; no per-draw descriptor writes; draw count measured; the measured reason for
deferring or adding the heavier features recorded.

### N3 - game integration
Add a game-facing `render_api` and `game_render` bridge. Port `RenderAFrame` orchestration:
camera, cars, track, effects, HUD, map, mirror, cockpit, shadows. Keep the legacy path behind a
selector for A/B. No BRender renderer/DDI calls in the new path.
Exit: race playable through the new path; forward/mirror/cockpit/map/summary parity; legacy
software/OpenGL still work.

### N4 - parity and performance
Full visual parity set (forward, mirror, cockpit, map, summary, pratcam, fades, dim, gauges,
frontend, damaged/repair), 10-minute race, validation, 200 FPS in the authorized optimized build.
Exit: mean <= 5.0 ms, p50/p95/p99 reported; no 4101/device loss/camera assert; one instance.

### N5 - remove the legacy render path
Switch the default to the new renderer; keep software/OpenGL fallback and the legacy code only as
an optional comparison mode. Update docs.

## Acceptance

- Debug: 10-minute race, validation loaded and zero errors, no 4101/device loss/camera assert,
  visual parity, one game instance.
- Performance: authorized optimized build, 640x480, asserted IMMEDIATE, 5 x 60 s after warm-up,
  N >= 8000/run, `1000/mean(dt)`; median mean <= 4.75 ms, every run <= 5.00 ms, p99 <= 6.5 ms,
  <= 0.5% > 5 ms; FIFO control separate; render-loop vs presented reported.
- Software/OpenGL/reccmp/`FIX_BUGS=OFF`/Linux gates remain.

## Immediate next steps

1. Build the N1 neutral asset format, offline importer and standalone viewer around one track and
   one car.
2. Add one textured mesh pipeline with depth, a fixed camera and a screenshot path; compare against
   a software reference.
3. Prototype the runtime bridge for one car: transform and material updates, no BRender renderer
   calls.
4. Keep the game on the legacy path until N3; do not claim 200 FPS before the N4 measurement.

## Risks

- Asset import and HUD port are larger than the previous in-place plan; budget accordingly.
- BRender data is interleaved with gameplay; the bridge must be read-only and stable.
- Visual parity for shadows, map, cockpit and palette fades is the main risk.
- The 200 FPS target still depends on measured CPU cost; the clean renderer removes the DDI
  overhead but the game bridge and asset conversion must also stay within the 5 ms budget.
