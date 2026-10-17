# vkrend render graph

Current end-to-end picture of how a frame reaches the screen with the `vkrend` BRender driver,
where the CPU and GPU time goes, which parts are known to be wrong or fragile, and what the
render graph should become. This document describes the code as of engine commit `53d27fd`
(dethrace `e80493b`, 2026-10-18); symbol names are stable, line numbers drift.

Paths are relative to `lib/BRender-v1.3.2/drivers/vkrend/` unless stated otherwise. The visual
reference remains the software renderer; `docs/vkrend-parity.md` tracks feature parity and is not
a pipeline description.

## 1. Frame in one picture

```text
game frame (src/DETHRACE/common/graphics.c, RenderAFrame)
  shadow passes per car      ProcessShadow -> BrZbSceneRenderBegin/End
  world pass                 BrZbSceneRenderBegin(gRender_screen)
  rearview pass (mirror on)  BrZbSceneRenderBegin(gRearview_screen)
  HUD/dim/pratcam passes     DimRectangle / DoPratcam -> BrZbSceneRender(gBack_screen)
  2D HUD                     text, gauges, map, powerups into the CPU shadow
  PDScreenBufferSwap
    ReallyCopyBackScreen     lores 320x200 INDEX_8 -> 640x480 RGB565 (CPU)
    SwapBackScreen -> BrPixelmapDoubleBuffer
      vkrend doubleBuffer (devpixmp.c:421)
        GpuVkFrameEnd        close + submit the scene command buffer, wait the scene fence
        VIDEOVkPresent
          overlay upload     full 640x480 CPU shadow -> R5G6B5 overlay image
          composite          1 fullscreen triangle: overlay (magenta key) + scene target
          vkQueuePresentKHR
```

There are two independent layers:

1. **CPU 2D layer** - every RGB565 device pixelmap owns a `shadow` buffer (`pm_pixels`). All 2D
   drawing (HUD, text, map, cockpit image, fades) is done by BRender's memory pixelmap routines
   into this shadow. Magenta `0xF81F` is the transparency key.
2. **GPU 3D layer** - every root colour pixelmap lazily gets a `vk_gpu_target`: RGBA8 colour image,
   D16 depth image, render pass, framebuffer, sampler, 1x1 white texture, a 2 MB host-visible UBO,
   513 descriptor sets and two pipelines (model + overlay). Sub-pixelmaps (`gRender_screen`,
   `gRearview_screen`) get their own targets (`devpixmp.c:386`, `gpu.c:1301`).

The present pass combines them: where the 2D overlay is not the magenta key it wins, otherwise the
selected 3D target is shown (`shaders/present.frag`).

## 2. Scene recording

Entry points are `GpuVkSceneBeginRenderer` (`gpu.c:1967`) and `GpuVkSceneEndRenderer`
(`gpu.c:2058`), called from BRender's renderer scene hooks. All passes of one game frame are
recorded into **one** scene command buffer (`video->scene_command_buffer`).

`GpuVkBeginScene` (`gpu.c:1709`), first scene of a frame:

- waits the pending scene fence and the previous present fence;
- flushes deferred buffer/image releases;
- resets the command buffer, resets the overlay staging slot, retires temporary geometry;
- begins the command buffer, increments `frame_epoch`, clears `present_target`.

For every scene (`target` = the colour pixelmap of the pass):

- first use of this target in this frame: reset its UBO slot, its per-frame draw counter and its
  overlay epoch; clear the colour image through `TRANSFER_DST` and move it to
  `COLOR_ATTACHMENT_OPTIMAL` (`GpuVkClearTargetColour`, `gpu.c:1614`);
- once per target per frame: upload the target's CPU shadow into the shared R5G6B5 overlay image
  and reset the shadow to magenta (`GpuVkUploadOverlayUnder`, `gpu.c:1681`);
- begin a render pass on the target's framebuffer, set viewport/scissor, draw the overlay quad if it
  was uploaded, bind the model pipeline (`gpu.c:1777-1830`);
- record draws.

Draw recording (`GpuVkRenderStored` `gpu.c:2316`, `GpuVkRenderV1Model` `gpu.c:2348`):

- stored geometry is cached per `br_geometry_stored` and Vulkan device generation
  (`GpuVkGeometryEnsure`); a cache miss converts positions/UVs/vertex colours and rebuilds
  vertex/index buffers (`GpuVkGeometryUpload` `gpu.c:2109`);
- immediate `v11model` draws allocate two buffers, upload and retire them after the frame;
- per material group: resolve state (`GpuVkMaterialResolve` `gpu.c:1422`), look up/reuse a texture
  descriptor set (`GpuVkMaterialDescriptor` `gpu.c:1479`), write 192 bytes of UBO
  (`GpuVkWriteUbo` `gpu.c:1382`), bind with a dynamic offset and issue `vkCmdDrawIndexed`;
- textures are built lazily: `BufferStoredVkGetTexture` (`sbuffer.c:797`) converts INDEX_8/RGB565/
  RGB888/RGBA to RGBA8 on the CPU, then uploads with its own command pool, submit and
  `vkWaitForFences` (`BufferTextureUpload` `sbuffer.c:319`).

`GpuVkEndScene` (`gpu.c:1831`) only ends the render pass. `GpuVkSceneEndRenderer` then chooses the
frame's presented target: the target with the largest draw count wins, but a target that already
has at least `VKREND_MAIN_TARGET_MIN_DRAWS` (8) draws cannot be replaced. Ties keep the first.

## 3. Frame end

`GpuVkFrameEnd` (`gpu.c:1842`, called from `devpixmp.c:421` `doubleBuffer`):

1. end the scene command buffer;
2. submit it with `video->scene_fence`;
3. **wait the scene fence on the CPU immediately**;
4. reset the fence.

Only one frame is in flight. The scene GPU work cannot overlap the CPU game logic; the present
submission is asynchronous but the next scene begin waits its fence. The present itself is not
waited at the end of `VIDEOVkPresent`.

## 4. Present / composite

`VIDEOVkPresent` (`video.c:1942`) -> `video_present_frame` (`video.c:1573`):

1. `VIDEOVkOverlayEnsure` (`video.c:1276`): ensure a 640x480 R5G6B5 overlay image plus a mapped
   staging buffer with 16 slots;
2. wait the previous present fence;
3. update the composite descriptor set: binding 0 = overlay, binding 1 = the selected target's
   colour view (or a 1x1 black image);
4. acquire a swapchain image with `acquire_semaphore`;
5. copy the entire 640x480 CPU shadow row by row into a staging slot
   (`VIDEOVkOverlayUpload` `video.c:1316`) and record a full buffer-to-image copy;
6. `GpuVkPresentPrepare` (`gpu.c:1903`): transition the selected target's colour image to
   `SHADER_READ_ONLY_OPTIMAL`;
7. begin the swapchain render pass, bind the present pipeline, draw one fullscreen triangle
   (`shaders/present.vert`), end the pass;
8. submit: wait `acquire_semaphore` at `COLOR_ATTACHMENT_OUTPUT`, signal
   `render_semaphores[image_index]`;
9. `vkQueuePresentKHR` waits that per-image semaphore.

Synchronization objects: one scene command buffer + scene fence, one present command buffer +
present fence, one acquire semaphore, one render-finished semaphore per swapchain image
(`video.c:1221`). This is spec-correct for a one-frame-in-flight design, but it serializes scene
work and gives no CPU/GPU pipeline overlap.

### Measured cost

- Static 2D menu: ~21.5 ms/frame, but the whole present path is ~0.2-0.5 ms. The frame time is a
  game-side `DoMouseCursor` wait loop (`src/DETHRACE/common/graphics.c:3037`), not Vulkan. The menu
  records zero 3D scenes.
- Race with scenes: the only stable measurement was ~19.2 ms/frame with the untextured path
  (scene fence wait ~0.5 ms, present ~0.2 ms). CPU recording dominates; the GPU is mostly idle.
- Race passes: 13-19 per frame (600-frame pre-V3a artifact, average 16.6), because the vkrend
  screen does not match the 3dfx identifier, so `gDevious_2d` is enabled
  (`src/DETHRACE/pc-all/allsys.c:470`) and every HUD `DimRectangle` becomes a
  `BrZbSceneRender` pass (`src/DETHRACE/common/displays.c:307`).

## 5. Resource lifetime and device recovery

- Geometry buffers retired per target are flushed at the next frame begin; deferred image/buffer
  releases are flushed after the scene and present fences.
- Target cleanup waits `vkDeviceIdle` before destroying; a target from an older device generation
  is freed without Vulkan calls.
- Textures record `texture_device` and `built_generation`; a device or generation change abandons
  the old image/staging state and rebuilds (`sbuffer.c:797`).
- `video_recover_device_lost` (`video.c:1522`): destroy the instance/device/swapchain, recreate
  them, reset `gpu_disabled`, GPU and texture function tables, log flags and deferred lists. The
  recovery budget is 8 attempts; one attempt per presented frame; after exhaustion a 600-frame
  cooldown allows a new batch. Hitting the budget no longer calls `BrFailure`/`abort()`; a failed
  `VIDEOVkCreate` still goes through `video_fail`, which can abort.

Known limits of this model: the recovery is not atomic (destroy before create), scene-side and
present-side losses share the budget, and a failed recreation leaves the 2D present path without a
device. The renderer does not fall back to the software rasterizer.

## 6. What is wrong or fragile today

### Correctness

- **Single-target composition.** The present pass samples one 3D target. The main target is chosen
  by draw count with a minimum, which works for forward/cockpit/mirror/map screenshots, but
  auxiliary targets are not composited as explicit sub-rects: the rearview mirror and the
  wreck/summary view have no dedicated composite stage, and the mirror inset seen in the cockpit
  screenshot is not produced by the present pass sampling a second target. Multi-target composition
  with source rects is still missing.
- **2D ordering between passes.** The overlay-under is flushed once per target per frame. 2D
  content drawn between two 3D passes on the same target is not re-flushed under the later pass;
  the final present still shows the fresh overlay, so this is mostly an intra-target issue, but it
  is not a general ordering guarantee.
- **Skipped swaps.** `PDScreenBufferSwap` is deliberately skipped during palette fades
  (`graphics.c:2281`), so the scene command buffer, `frame_epoch` and target counters can span game
  frames. The overlay staging ring also has only 16 slots.
- **Descriptor-cache eviction across passes.** `frame_sequence` increments per scene begin; sets
  used by an earlier pass of the same unsubmitted command buffer can be rewritten for a later pass.
- **Camera NaN / assert.** The shadow pass temporarily adds a hither fudge to the shared
  `gCamera->hither_z` (`graphics.c:1687-1729`). A NaN/degenerate input can make `hither_z` NaN or
  non-positive; `CheckCameraHither` restores it from an `int`-truncated static and can restore 0,
  then `CameraToScreenMatrix4` asserts `hither_z > 0` (`actsupt.c:494`). This is pre-existing and
  independent of the compositor.
- **TDR.** The race still loses the Vulkan device roughly every 1-2 seconds (Windows Display event
  4101). The trigger is not established; recovery hides the symptom but does not remove it.

### Performance

- **13-19 scene passes per race frame**, each with its own render pass begin/end, UBO resets,
  descriptor work and draw recording. Only 1-2 passes are needed for the world.
- **CPU 2D work every frame**: lores 320x200 INDEX_8 -> 640x480 RGB565 conversion, top/bottom bar
  fills, full-surface overlay row copy, full-surface magenta reset, and a CPU depth-buffer fill
  (`graphics.c:1980`) that the GPU path never reads.
- **One frame in flight** with an immediate scene-fence wait: the CPU cannot record frame N+1 while
  the GPU executes frame N.
- **Per-frame dynamic rebuilds**: `BrModelUpdate` recreates stored objects for shadow/dim/pratcam
  models every frame; `BrMapRemove`/`BrMapAdd` recreates the pratcam texture; every immediate
  `v11model` allocates two buffers.
- **Synchronous texture uploads**: command pool + submit + fence wait per rebuilt texture.
- **Full-surface work instead of dirty rectangles**: the overlay upload always copies 640x480.
- **Per-draw CPU cost**: 192-byte UBO write, descriptor bind, linear descriptor-cache scan.

## 7. Target render graph

The goal is an explicit frame graph with stable resources, not an incidental sequence of renderer
callbacks.

```text
Frame N (slot = N % 2 or 3)
  wait slot.fence                     # only when recycling this slot
  acquire swapchain image             # slot.acquire_sem
  begin slot.command_buffer
    pass 1: 3D target clears          # once per target
    pass 2: shadow map(s)             # optional, same target as world
    pass 3: world                     # main target, 3D geometry
    pass 4: rearview mirror           # own target, sampled as an inset
    pass 5: 2D underlay               # uploaded dirty rects, sampled under 3D
    pass 6: composite                 # main target + mirror/wreck insets + overlay
  end + submit (wait acquire_sem, signal render_sem[image], slot.fence)
  present (wait render_sem[image])
```

Required changes, in dependency order:

1. **Correctness first**
   - designate a real main target (screen/world), not a draw-count heuristic;
   - composite auxiliary targets (mirror, wreck) as explicit sub-rects with their own source rects;
   - keep the 2D overlay ordering by flushing dirty rects before the passes that need them;
   - make skipped swaps safe: end the frame on `doubleBuffer`, never accumulate across game frames;
   - fix the camera hither path (finite values, float restore, clamp) before enabling long runs;
   - establish the TDR root cause with `VK_EXT_device_fault`, GPU-assisted validation,
     `robustBufferAccess` and object names.
2. **Frame pacing**
   - 2 frames in flight: per-slot command buffers, fences, acquire semaphores, scene targets, UBO
     rings, overlay staging rings and descriptor arenas;
   - replace the immediate scene-fence wait with a scene-done semaphore waited by the present
     submit; wait a slot's fence only when recycling it.
3. **Uploads and geometry**
   - one persistent upload pool / staging ring per frame; batch texture builds before recording;
   - persistent device-local vertex/index/UBO rings; pool immediate models; stop recreating stored
     objects for static shadow/dim models;
   - index/palette textures as R8 + palette LUT so CLUT animation does not rebuild every texture.
4. **2D layer**
   - dirty-rectangle tracking in the device pixelmap; upload only changed regions;
   - drop the unused CPU depth fill for vkrend;
   - decide whether `gDevious_2d` should be off for vkrend (the CPU 2D path already draws those
     HUD rectangles); this removes most of the 13-19 passes.
5. **Opt-in path tracing (later)**
   - capture per-draw data at scene time into frame-owned records, build persistent BLAS/TLAS,
   - render to an HDR target, denoise/tonemap, composite after the 2D overlay,
   - keep the raster path and DDI semantics unchanged when PT is off.

## 8. Verification and diagnostics

- `VKREND_VALIDATION=1` enables the Khronos validation layer; it is opt-in and writes
  `vkrend_validation.txt`.
- `vkrend_markers.txt` is a bounded marker log (scene starts, compositor activation, failures); it
  is not a profiler.
- Recommended additions: `VK_EXT_device_fault` reporting before device destruction, GPU timestamp
  queries around scene/present passes, QPC frame phases, per-pass counters (passes, draws, bytes
  uploaded, descriptor misses), and a frame-ID canary for cross-frame aliasing.
- Acceptance for the render graph: no TDR in a 10-minute race; validation clean; the world,
  mirror/cockpit/map/summary screens match the software reference; no cross-frame aliasing; p95
  frame time within the target; device loss recovers or degrades explicitly.

## 9. Glossary

- **Target**: a `vk_gpu_target` - GPU colour/depth images plus pipelines/descriptors for one root
  colour pixelmap.
- **Overlay**: the persistent R5G6B5 image holding the CPU 2D shadow; magenta is the key.
- **Pass**: one `GpuVkBeginScene`/`GpuVkEndScene` pair, i.e. one BRender renderer scene.
- **Scene command buffer**: the single command buffer that records all passes of a frame.
- **Present command buffer**: the separate command buffer that uploads the overlay and composites
  to the swapchain image.
