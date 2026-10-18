# Legacy renderer rewrite plan

Scope: replace the **render path** used by the game with a modern retained renderer while keeping
BRender as the asset/scene loader and the software/OpenGL fallbacks. The Vulkan backend is prepared
first (`docs/vkrend-vulkan-layer-plan.md`); this document describes how the legacy renderer is
migrated onto it.

## What "legacy renderer" means here

1. **BRender immediate-mode renderer**: `BrZbSceneRenderBegin/End`, the renderer state machine,
   per-object/per-group callbacks, `br_geometry_stored`/`v11model` drawing.
2. **Game render flow** in `src/DETHRACE/common/graphics.c`: `RenderAFrame`, `ProcessShadow`,
   `RenderShadows`, `DimRectangle`/`DoPratcam`, `RenderAFrame`'s 2D HUD, and the shared
   `gBack_screen`/`gRender_screen`/`gRearview_screen` shadow model.
3. **Legacy presentation model**: CPU RGB565 shadow, magenta transparency key, one selected target,
   draw-count heuristic, per-pass camera hither fudge.

The asset formats, gameplay, physics, race logic and reccmp compatibility are **not** in scope.

## Review corrections (2026-10-18)

- Extract at the `BrZbSceneRenderAdd`/DDI boundary, not by walking the BRender scene: the track
  culls its own columns and lollipops, and the game controls pass order. Preserve order-table
  semantics: opaque bucket plus blended submission-order bucket.
- Clip planes are a missing raster feature, not a batching limit; implement per-draw clip/discard
  before batching shadows.
- Split shadow generation from submission; no shared-camera hither mutation; batch only where
  clip state allows.
- B4 covers the full HUD/map/cockpit/pratcam/flic scope, map underlay and world viewport; a glyph
  atlas is later. B5 needs a global tint/CLUT post-process and exact dim, not only an R8 LUT.
- B6 is single-threaded state/material/batch reduction. No job system or worker threads.
- `VKREND_LEGACY_PATH` is a game-side producer selector, not a driver flag; gate game-side changes
  by capability/vkrend mode so software/OpenGL keep the legacy path.
- Damaged-car geometry frequency and the mirror second traversal must be measured in D0, not
  assumed per-frame. Add reference capture, seed/render cap, tick/replay hash and the full parity
  screen set before claiming parity.

## End-state architecture

```text
game/BRender assets
        |
        v
scene extraction (render proxies)
  mesh, material, transform, bounds, flags, palette
        |
        v
frame builder
  shadow -> world -> mirror -> UI -> composite
  explicit targets, sub-rects, transient resources
        |
        v
Vulkan backend (D0-D3: validated 512 sets/plain draws; D4 optional bindless/indirect/FIF)
        |
        v
present (FIFO default, uncapped opt-in)
```

### Components

- **Render proxy**: a flat, backend-neutral record `{ mesh, material, transform, bounds, flags }`.
  Extracted from BRender actors/models once per frame or when dirty.
- **Frame builder**: game-owned pass graph: shadow, world, rearview, UI, composite. It owns the main
  target and sub-rect layout; it does not choose a target by draw count.
- **Material system**: immutable material state (texture slot, sampler, blend, alpha test, colour
  key, vertex colour, UV transform). No shared camera mutation.
- **Mesh/geometry system**: static world meshes merged into a global arena; dynamic meshes in a
  per-frame arena; one or few indirect draws.
- **UI system**: RGBA8 glyph/quad atlas, alpha blending, dirty rects, wall-clock animation cadence.
  Replaces the CPU shadow and magenta key.
- **Shadow system**: batched projected shadows in the first rewrite stage; a shadow map/cascades is
  an optional later visual upgrade, not a prerequisite for the port.
- **Execution model**: single-threaded. Extraction, culling, material resolution and command
  recording run on the main thread. Multithreading is explicitly out of scope: at 640x480 the target
  is low draw count and low state churn, not parallelism.

## Migration stages

### B0 - Baseline and parity harness
- Reuse D0b: 11 committed references (forward, mirror, cockpit, map, summary/wreck, pratcam, fade,
  dim, gauges/powerups, frontend, damaged/repair), the 24-bit capture path and comparator tolerance.
- Keep the validation and 10-minute race gates; do not duplicate the capture implementation.
- Acceptance: repeatable parity set; no parity claim before D0b passes.

### B1 - Render proxy extraction
- Produce a flat proxy list at the `BrZbSceneRenderAdd`/DDI boundary, not by walking the BRender
  scene: the track culls its own columns/lollipops and the game controls pass order. Capture bounds,
  transform, material and mesh ranges in the shared record schema from D1.
- Add frustum culling and an opaque sort key plus a blended submission-order bucket; preserve the
  order-table semantics. No Vulkan changes; the backend can still draw the old way.
- Acceptance: proxy list matches the old visible set; no visual change; blended surfaces correct.

### B2 - Frame builder / pass graph
- Replace `RenderAFrame`'s pass orchestration with a frame builder that emits the pass list and
  explicit targets. The Vulkan layer executes it (depends on A4).
- Main target is explicit; rearview/wreck are sub-rect composites; skipped swaps get a frame boundary.
- Acceptance: forward/mirror/cockpit/map/summary correct; pass count and CPU record measured.

### B3 - Shadow system
- Batch all in-range projected shadows into one pass or one indirect batch; remove per-car camera
  hither fudge and clip-plane passes where possible.
- Keep the shared camera immutable; no shadow pass may mutate `hither_z`.
- Acceptance: shadows visually match the software reference; no `actsupt.c:494`; fewer passes.

### B4 - UI system
- Replace the CPU shadow/magenta-key HUD with an RGBA8 UI pass: text glyph atlas, gauges, map,
  cockpit/pratcam, powerups, fades. Dirty-rect updates; wall-clock timers.
- Add a 16-bit `DimRectangle` bridge for the transition (or move dimming into the UI shader).
- Acceptance: HUD/map/cockpit/pratcam/fades match the software reference; UI pass count <= 2.

### B5 - Palette and texture pipeline
- INDEX_8 textures become R8 + palette LUT (or pre-baked RGBA where the palette is static).
- Uploads via transfer queue/staging ring; no per-texture synchronous fence.
- Acceptance: palette animation correct; texture upload CPU cost near zero per frame.

### B6 - Single-thread efficiency
- Collapse per-frame material/state churn, sort the proxy list by pipeline/material/texture, merge
  static groups, and keep culling/extraction in one deterministic pass.
- No worker threads, job system or secondary command buffers. The CPU budget is won by doing less
  work, not by spreading it.
- Acceptance: same visuals; CPU scene record reduced with one thread; no Vulkan threading changes.

### B7 - Remove the legacy render path from vkrend
- The vkrend hot path no longer calls `BrZbSceneRenderBegin/End` per model/group; it consumes proxies
  and the frame builder. BRender remains for asset loading, software and OpenGL.
- Keep the game-side producer selector created in D1 (`VKREND_LEGACY_PATH`) for A/B until parity is
  proven; it selects the old `RenderAFrame` producer or the B2 frame builder, not a second driver.
- Acceptance: 10-minute race, parity, no regression in software/OpenGL.

### B8 - Parity, performance, documentation
- Visual parity set, validation, 10-minute race, 200 FPS acceptance in the authorized optimized build.
- Update `docs/vkrend-render-graph.md`, `docs/vkrend-parity.md`, and this plan's status.
- Acceptance: all gates pass or the result is explicitly qualified.

## Dependencies

- B1 starts after D1's record schema/frame boundary; it does not start before the layer boundary.
- B2 depends on D3 (frame graph/main target/composite).
- B3/B4 can be staged behind flags and A/B tested.
- B5 uses the current texture path; bindless/palette LUT are optional D4 items, not dependencies.
- B6 depends on B1/B2 and the proxy list being immutable during recording; it is single-threaded.

## What stays

- BRender asset/model/scene formats and the software/OpenGL drivers.
- Gameplay, physics, AI, race logic, reccmp builds.
- `DETHRACE_FIX_BUGS` gating for any `src/DETHRACE` behavior change.
- One game instance at a time; Debug functional verification.

## What is replaced

- The immediate-mode render path for vkrend, per-pass camera state mutation, shared CPU shadow and
  magenta key, the per-group Vulkan bind/draw submission, and the draw-count target heuristic.
- The old 2D HUD raster path for vkrend (behind a flag until parity).

## Risks and open questions

1. **BRender coupling**: actors, models and materials are interleaved with gameplay. Extraction must
   be read-only and stable; no gameplay state changes.
2. **2D HUD**: the game's art assumes the shadow/magenta model; the UI rewrite is the largest visual
   parity risk. A 16-bit dim bridge and a dual-path A/B reduce it.
3. **Shadows**: per-car clip planes and projected geometry may limit batching; a shadow map is a
   visual upgrade with its own art/performance cost.
4. **CPU vs GPU**: at 640x480 the GPU is idle; the rewrite must target CPU submission/extraction, not
   more GPU features. If traversal remains the limit, B6 (single-threaded batching/state reduction) is
   the required lever; multithreading is not in scope.
5. **200 FPS**: the rewrite lowers CPU cost, but the final number must be measured. A qualified result
   is required if the authorized build cannot reach 5 ms.

## Acceptance summary

- Debug: 10-minute race, no 4101/device loss/camera assert, validation clean, one instance.
- Visual parity: forward, mirror, cockpit, map, summary, HUD dim/pratcam against the software
  reference.
- Performance: mean <= 5.0 ms, p50/p95/p99 reported, render-loop >= 200 FPS in the authorized
  uncapped optimized build; FIFO default; Linux compiles.
- No regressions in software/OpenGL/secondary paths or reccmp.
