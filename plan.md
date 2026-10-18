# New renderer plan

Status: supersedes the in-place vkrend modernization. Decision: the new renderer must not be
based on the legacy BRender/DDI/vkrend code. The legacy path remains a reference and fallback
only. Branch `feature/vkrend-v1`; engine `92e0f91` plus D0 instrumentation, dethrace `3ab8c7a`.

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
- **New runtime:** `src/render_vk/` (name provisional), a clean Vulkan renderer. Its public API
  has no BRender headers, types or callbacks.
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
       frame graph (shadow, world, mirror, UI, composite)
       bindless descriptors, indirect draws, 2-3 frames in flight
       resource manager (static/dynamic arenas, staging ring, pipelines)
  -> present (FIFO default, IMMEDIATE opt-in)
```

- Renderer-owned scene: flat render proxies, frustum culling, opaque/blended buckets, stable
  sort keys; no BRender actor traversal in the render loop.
- Static geometry in a global vertex/index arena; dynamic geometry in per-frame rings; indirect
  draws; texture slots resolved in a pre-pass; descriptor sets written only before binding.
- Explicit main target; mirror/wreck as sub-rect composites; UI as a separate pass.
- 2-3 frame slots with timeline semaphores; no synchronous scene-fence wait; slot-keyed deferred
  destruction and slot-aware recovery.
- Single-threaded. No job system, worker threads or secondary command buffers.
- Debug path: validation, GPU timestamps, per-pass CSV, device-fault reporting.

## Phases

### N0 - freeze and skeleton
Freeze legacy vkrend (critical fixes only). Create `render_vk` skeleton: instance/device/swapchain,
clear + present, validation, frame timing. No game integration, no BRender includes.
Exit: standalone binary presents a clear frame; Debug and Linux compile; legacy untouched.

### N1 - asset import and parity viewer
Offline importer for one track and one car: meshes, materials, textures, transforms. Standalone
viewer renders the static scene with the new renderer. Software-reference screenshots for the
same assets.
Exit: imported scene matches the software reference within the comparator tolerance; asset format
documented; no BRender dependency in the runtime renderer.

### N2 - renderer core
Bindless textures, static/dynamic arenas, indirect draws, frame graph, 2-3 frame slots, timeline
semaphores, UI pass, shadow pass, mirror sub-rect composite, resource lifetime/recovery.
Exit: 640x480 scene at target throughput in the authorized optimized build; validation clean;
p50/p95/p99 recorded; no per-draw descriptor binds; draw count measured.

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

1. Write the `render_vk` public API and module skeleton (no BRender includes).
2. Build the N1 asset importer and standalone viewer around one track and one car.
3. Use the legacy vkrend/D0 only as a reference for parity and baseline numbers.
4. Keep the game on the legacy path until N3.

## Risks

- Asset import and HUD port are larger than the previous in-place plan; budget accordingly.
- BRender data is interleaved with gameplay; the bridge must be read-only and stable.
- Visual parity for shadows, map, cockpit and palette fades is the main risk.
- The 200 FPS target still depends on measured CPU cost; the clean renderer removes the DDI
  overhead but the game bridge and asset conversion must also stay within the 5 ms budget.
