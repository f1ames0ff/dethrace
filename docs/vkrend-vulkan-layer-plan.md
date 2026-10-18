# vkrend Vulkan layer plan

Goal: finish the Vulkan layer as a modern, self-contained renderer backend before the legacy renderer
is rewritten. The BRender DDI stays the producer API for compatibility, but the vkrend driver becomes
a deferred, GPU-driven-friendly renderer with an explicit frame graph. The legacy renderer rewrite
plan is in `docs/vkrend-renderer-rewrite-plan.md`.

## Constraints

- Windows first; Linux must keep compiling.
- Debug is the functional build. Performance acceptance is measured in an explicitly authorized
  optimized build at 640x480 on the RX 9070 XT.
- The default/software/OpenGL paths must not change.
- `src/DETHRACE` changes stay under `DETHRACE_FIX_BUGS`; reccmp builds force it off.
- One game instance at a time; runtime checks are main-agent only.
- 200 FPS means render-loop throughput in an uncapped/IMMEDIATE mode. The monitor is 165 Hz, so a
  displayed-frame count cannot exceed the panel; FIFO remains the compatibility default.

## Current state (2026-10-18)

- The TDR, camera assert and recovery freeze are fixed and verified; validation is clean after the
  texture-array/push-constant rollback.
- Race frame: 45-60 FPS in Debug/RelWithDebInfo, scene record 15-22 ms, GPU fence wait 0.2-0.3 ms,
  140-250 draw groups, 13-19 passes. The GPU is idle; the limit is CPU scene recording.
- The current layer records Vulkan commands directly from BRender callbacks into one command buffer,
  binds a per-texture descriptor set per group, and waits the scene fence synchronously.
- Persistent texture upload resources and an uncapped present mode are in place.

## Target architecture

```text
BRender DDI callbacks
  geometry upload / material resolve / draw
        |
        v
vk_scene_record (CPU only, no Vulkan calls)
  pass records, draw records, texture slots, transforms, state
        |
        v
vk_frame_graph
  shadow -> world -> mirror -> UI -> composite -> present
  explicit targets, barriers, transient aliasing
        |
        v
vk_submit
  static indirect batch + dynamic indirect batch + UI
  1-3 submits/frame, timeline semaphores, 2-3 frames in flight
        |
        v
vk_swapchain / present
```

### Modules

| Module | Responsibility |
| --- | --- |
| `vk_context` | instance, physical device, device, graphics/present/transfer queues, features, memory allocator, device-fault support |
| `vk_swapchain` | 3+ images, FIFO default, IMMEDIATE/mailbox opt-in, per-image semaphores, resize |
| `vk_frame` | per-slot command pools, timeline semaphores, descriptor arenas, staging ring, draw-data SSBO, query pool, overlay image |
| `vk_scene_record` | pass/draw records produced by the DDI, texture slot assignment, material state; no Vulkan calls |
| `vk_texture` | bindless slots, R8+palette LUT for INDEX_8, RGBA8 for true colour, transfer-queue uploads, global serial |
| `vk_geometry` | static global vertex/index arena, dynamic per-frame arena, mesh ranges, indirect command buffer |
| `vk_pipeline` | pipeline cache, material variants (textured/untextured, alpha test, blend), specialization constants |
| `vk_pass` | frame-graph passes, main target, mirror/wreck sub-rect composite, UI overlay, transient resources |
| `vk_debug` | validation, debug markers, GPU timestamps, per-pass CSV, device-fault reporting |
| `vk_recovery` | existing B3 state machine, wall-time retry, explicit terminal freeze |

## Descriptor and binding model

- One bindless descriptor set per frame slot: a combined-image-sampler array for textures, one
  storage buffer for per-draw data, immutable samplers.
- `VK_EXT_descriptor_indexing` is used for non-uniform texture indices. If the extension is
  unavailable, fall back to the existing 512-slot array and a pre-pass; the fallback is slower but
  correct.
- Texture slots are resolved in a **pre-pass** before any command buffer is recorded. Descriptor
  writes happen only while the set is not bound. This was the validation failure of the previous
  experiment and is a hard rule now.
- Per-draw data uses `gl_InstanceIndex`/`firstInstance` (or a push constant) to index the storage
  buffer; there is no per-draw descriptor bind.

## Geometry and submission

- Static world geometry is appended once to a global vertex/index arena. Each `br_geometry_stored`
  keeps `(vertex_offset, first_index, index_count)`.
- Dynamic geometry (cars, shadows, immediate v1model, pratcam) uses a per-frame arena.
- The scene record builds `VkDrawIndexedIndirectCommand` arrays. Each command sets
  `firstInstance = draw_data_slot`, `vertexOffset`, `firstIndex`, `indexCount`.
- Static and dynamic batches are submitted with `vkCmdDrawIndexedIndirect` (then
  `vkCmdDrawIndexedIndirectCount` once the count path is stable).
- The target is one bind of the vertex/index buffers and one indirect draw for the static world,
  plus a small number for dynamic content. Draw count is not allowed to be 140-250.

## Textures and assets

- INDEX_8 source textures become R8 index images plus a 256x1 palette LUT; palette changes update
  1 KB, not the whole texture.
- True-colour sources stay RGBA8.
- Uploads use a transfer queue and a staging ring; no synchronous command-pool/fence per texture.
- A texture is uploaded once and referenced by a bindless slot; rebuilds invalidate the slot only.
- Compressed formats (BCn/KTX2) are a later optimization, not a blocker for 200 FPS at 640x480.

## Passes and frame graph

1. Shadow pass: batched projected shadows for all in-range cars where clip state allows; otherwise
   per-car draws in one pass. A real shadow map is a legacy-renderer rewrite item, not a prerequisite.
2. World pass: main target.
3. Mirror pass: rearview target, composited as an explicit sub-rect.
4. Wreck/other target: explicit sub-rect, not a draw-count heuristic.
5. UI pass: RGBA8 glyph/quad atlas, alpha blending, dirty rects; no magenta key.
6. Composite: main target + mirror/wreck insets + UI.
7. Present: FIFO by default, IMMEDIATE/mailbox with `VKREND_UNCAPPED=1`.

The main target is explicit (screen/world), not "the target with the most draws". The composite stage
owns the sub-rect layout.

## Synchronization and frames in flight

- 2-3 frame slots, each with command buffer, timeline semaphore, descriptor arena, staging ring,
  draw-data buffer, overlay image.
- No synchronous scene-fence wait on the CPU. The scene submit signals a timeline semaphore; the
  composite submit waits it.
- Use `VK_KHR_synchronization2` and `VK_KHR_dynamic_rendering` where available; render passes and
  framebuffers remain a fallback.
- Skipped game swaps get an explicit frame boundary; the driver never depends on `doubleBuffer` per
  game frame.

## Debug and measurement

- `VKREND_VALIDATION=1` for correctness.
- GPU timestamp queries per pass; a per-frame CSV with pass, draw count, CPU record time, GPU time,
  present mode, sim ticks, and texture uploads; p50/p95/p99/max.
- `VK_EXT_device_fault` before device destroy on a loss.
- PresentMon/`VK_KHR_present_wait` for displayed-frame rate; render-loop and present rate reported
  separately.
- Debug markers and object names for RenderDoc/Nsight.

## Acceptance

- Debug: 10-minute race, no Display 4101, no device loss, no camera assert, validation clean,
  world/HUD/mirror/cockpit/map match the software reference, one instance.
- Release (explicitly authorized): 640x480 race, `VKREND_UNCAPPED=1`, mean <= 5.0 ms, p50/p95/p99
  reported; render-loop throughput >= 200 FPS; FIFO remains the default; Linux compiles.
- The 200 FPS sign-off is blocked until the draw count and per-frame CPU recording are measured under
  the new layer.

## Milestones

| ID | Deliverable | Acceptance |
| --- | --- | --- |
| A0 | Feature flags, GPU timestamps, CSV, benchmark harness; no behavior change | Same FPS as today; validation clean |
| A1 | Deferred scene record; all DDI calls append records; one submit at frame end | Same visuals; validation clean; no regression |
| A2 | Static/dynamic arenas + indirect draw commands | Draw submissions < 20/frame; scene record measurably lower |
| A3 | Bindless textures + pre-pass; one descriptor set | Validation clean; no bound-set updates; textures correct |
| A4 | Frame graph, explicit main target, mirror/wreck sub-rect composite | Map/mirror/summary correct; pass count reduced |
| A5 | 2-3 frames in flight, timeline semaphores, dynamic rendering | No CPU scene-fence wait; p99 improves |
| A6 | UI pass/dirty rects, 16-bit dim, no CPU depth fill | Passes <= 5 in the race; visuals match |
| A7 | Performance acceptance | Mean <= 5 ms in the authorized optimized build |
| A8 | Hardening, docs, Linux, recovery/device-fault | Validation clean; parity doc updated |

## Risks

- BRender traversal may still dominate after A2/A3. The A0 profile decides whether the legacy
  rewrite's multithreaded extraction (B5) must come before A7.
- Bindless extension availability; the 512-slot array fallback is kept.
- The 2D HUD/shadow is a large legacy dependency; A6 is a bridge until the UI rewrite.
- Windows windowed IMMEDIATE may be compositor-limited; render-loop throughput is the primary metric.
