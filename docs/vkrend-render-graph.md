# vkrend render graph

Current end-to-end picture of how a frame reaches the screen with the `vkrend` BRender driver,
where the CPU and GPU time goes, which parts are known to be wrong or fragile, and what the
render graph should become. Snapshot: engine `53d27fd`, dethrace `e80493b`, 2026-10-18; symbol
names are stable, line numbers drift.

Paths are relative to `lib/BRender-v1.3.2/drivers/vkrend/` unless stated otherwise. The visual
reference remains the software renderer; `docs/vkrend-parity.md` tracks feature parity and is not
a pipeline description.

## 1. Frame in one picture

```text
game frame (src/DETHRACE/common/graphics.c, RenderAFrame)
  shadow passes per car      ProcessShadow -> BrZbSceneRenderBegin/End (projected geometry)
  world pass                 BrZbSceneRenderBegin(gRender_screen)
  rearview pass (mirror on)  BrZbSceneRenderBegin(gRearview_screen)
  dim/pratcam passes         DimRectangle / DoPratcam -> BrZbSceneRender(gBack_screen)
  2D HUD                     text, gauges, map, powerups into the CPU shadow
  PDScreenBufferSwap
    ReallyCopyBackScreen     normally a no-op at the same resolution; the lores
                             320x200 INDEX_8 -> 640x480 RGB565 conversion runs only when
                             gReal_graf_data_index != gGraf_data_index
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
2. **GPU 3D layer** - every offscreen colour pixelmap lazily gets a `vk_gpu_target`: RGBA8 colour
   image, D16 depth image, render pass, framebuffer, sampler, 1x1 white texture, a 2 MB host-visible
   UBO, 513 model descriptor sets (1 base + 512 texture cache) plus one overlay descriptor set, and
   two pipelines (model + overlay) (`GpuVkTargetInit`, `gpu.c:1301`; descriptor setup `gpu.c:646`).
   `gBack_screen` is the root; `gRender_screen`, `gRearview_screen` and `gWreck_render_area` are
   `allocateSub` children (the wreck area is allocated from `gReal_back_screen`), and each gets its
   own target lazily on first scene (`devpixmp.c:386-418`, `gpu.c:1967-2028`). The per-target
   resources multiply with the number of targets.

The present pass combines them: where the 2D overlay is not the magenta key it wins, otherwise the
selected 3D target is shown (`shaders/present.frag`).

**Sub-pixelmap aliasing.** `gRender_screen`, `gRearview_screen` and `gWreck_render_area` are
`allocateSub` children of `gBack_screen` and share its CPU shadow at `pm_base_x/pm_base_y`
(`devpixmp.c:395-404`). The game expects direct writes into that shared surface; vkrend gives each
child its own GPU target. This is the root of the mirror/map composition behaviour described in
section 6.

## 2. Scene recording

Entry points are `GpuVkSceneBeginRenderer` (`gpu.c:1967`) and `GpuVkSceneEndRenderer`
(`gpu.c:2058`), called from BRender's renderer scene hooks. All passes of one game frame are
recorded into **one** scene command buffer (`video->scene_command_buffer`).

`GpuVkBeginScene` (`gpu.c:1709`), first scene of a frame:

- waits the pending scene fence and the previous present fence;
- flushes deferred buffer/image releases;
- resets the command buffer, resets the overlay staging slot, retires temporary geometry for the
  first target only (`GpuVkRetireFlush`, `gpu.c:1094`; retired buffers of other targets linger);
- begins the command buffer, increments `frame_epoch`, clears `present_target`.

For every scene (`target` = the colour pixelmap of the pass):

- first use of this target in this frame: reset its UBO slot, its per-frame draw counter and its
  overlay epoch; clear the colour image through `TRANSFER_DST` and move it to
  `COLOR_ATTACHMENT_OPTIMAL` (`GpuVkClearTargetColour`, `gpu.c:1614`);
- once per target per frame: if the target dimensions equal the overlay dimensions, upload the
  target's CPU shadow into the shared R5G6B5 overlay image and reset the target's sub-rect of the
  shadow to magenta (`GpuVkUploadOverlayUnder`, `gpu.c:1681`). Smaller targets (mirror/wreck) get
  no underlay. The overlay image itself is single and shared;
- begin a render pass on the target's framebuffer, set viewport/scissor, draw the overlay quad if it
  was uploaded, bind the model pipeline (`gpu.c:1777-1830`);
- record draws.

Draw recording (`GpuVkRenderStored` `gpu.c:2316`, `GpuVkRenderV1Model` `gpu.c:2348`):

- stored geometry is cached per `br_geometry_stored` and Vulkan device generation
  (`GpuVkGeometryEnsure`); a cache miss converts positions/UVs/vertex colours and rebuilds
  vertex/index buffers (`GpuVkGeometryUpload` `gpu.c:2109`). Normal `BrModelUpdate` frees and
  recreates `model->stored` (`prepmesh.c:1204-1212`), so the next draw rebuilds the GPU buffers;
  the cache tracks no model revision, which matters only for direct data mutations that bypass
  `BrModelUpdate` or for `BR_MODF_PREPREPARED` models;
- immediate `v11model` draws allocate two buffers, draw, and retire them immediately; the buffers
  are destroyed at the target's next first-scene flush (`GpuVkRetireFlush`, `gpu.c:1094`);
- per material group: resolve state (`GpuVkMaterialResolve` `gpu.c:1422`), look up/reuse a texture
  descriptor set (`GpuVkMaterialDescriptor` `gpu.c:1479`), write 192 bytes of UBO
  (`GpuVkWriteUbo` `gpu.c:1382`), bind with a dynamic offset and issue `vkCmdDrawIndexed`;
- textures are built lazily: `BufferStoredVkGetTexture` (`sbuffer.c:797`) converts INDEX_8/RGB565/
  RGB888/RGBA to RGBA8 on the CPU, then uploads with its own command pool, submit and
  `vkWaitForFences` (`BufferTextureUpload` `sbuffer.c:319`).

`GpuVkEndScene` (`gpu.c:1831`) only ends the render pass. `GpuVkSceneEndRenderer` then chooses the
frame's presented target: if none is selected, take the current target; otherwise replace it only
if the incumbent has fewer than `VKREND_MAIN_TARGET_MIN_DRAWS` (8) draw calls and the candidate has
more. Once an incumbent reaches 8 draws, no later target can replace it. This is a heuristic, not a
"main target" test.

`frame_sequence` is incremented per scene begin (`gpu.c:1826`). Descriptor-cache entries used by an
earlier pass of the same still-unsubmitted command buffer can be evicted and rewritten for a later
pass (`gpu.c:1495-1546`). That is a real multi-pass correctness hazard (wrong textures), not a
device-loss explanation by itself.

## 3. Frame end

`GpuVkFrameEnd` (`gpu.c:1842`, called from `devpixmp.c:421` `doubleBuffer`):

1. end the scene command buffer;
2. submit it with `video->scene_fence`;
3. **wait the scene fence on the CPU immediately**;
4. reset the fence.

Only one frame is in flight. Scene preparation/recording cannot overlap the GPU execution of the
previous scene; the present submission is asynchronous and can overlap the CPU game logic until
the next scene begin or present waits `video->fence`. There is no double-buffered scene command
buffer or scene target.

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
(`video.c:870-880`; the present fence/scene fence/acquire semaphore are created in
`video_create_sync_objects`, `video.c:1221`). This is spec-correct for a one-frame-in-flight
design; what it lacks is scene-recording overlap, not present/CPU overlap.

The swapchain currently hardcodes `VK_PRESENT_MODE_FIFO_KHR` when available (otherwise
`present_modes[0]`) in `video_create_swapchain` (`video.c:746-790`). There is **no** uncapped or
IMMEDIATE switch yet. FIFO on the attached 165 Hz monitor has a 6.06 ms floor (165 FPS), so a
200 FPS result requires an explicit uncapped mode and cannot be a displayed-frame count on this
monitor.

### Measured cost (provenance matters)

- Static 2D menu: ~21.5 ms/frame, but the whole present path is ~0.2-0.6 ms (typically
  ~0.3 ms). The frame time is a game-side `DoMouseCursor` wait loop
  (`src/DETHRACE/common/graphics.c:3037`), not Vulkan. The menu records zero 3D scenes.
- Race with scenes: **~19.18 ms/frame** (60 frames / 1151 ms) is a contract measurement from the
  investigation, taken with a temporary `VKREND_WHITE_TEXTURES=1` diagnostic (textures built and
  uploaded but sampled as white; descriptor churn/colour-key discard disabled) and a temporary
  `VKREND-PROF` wall-clock probe. That diagnostic code was removed afterwards, so the exact log is
  not in the current tree; treat the number as an approximate Debug diagnostic, not the default
  textured cost. The default textured path lost the device before a stable measurement.
- The ~0.5 ms scene-fence wait and ~0.2 ms present figures are from the same contract measurement;
  the 600-frame pre-V3a artifact has different phase timers and must not be summed to 19.18 ms.
- The 13-19 passes per frame (600-frame pre-V3a artifact, average 16.6) include one projected
  shadow pass per car in range, the world pass, optionally the mirror pass, and the `gDevious_2d`
  dim/pratcam passes. Only the last group is caused by `gDevious_2d`; the whole count is not.
- The 320x200 -> 640x480 lores conversion is **not** part of the normal 640x480 race path; it runs
  only in the resolution-switch branch of `ReallyCopyBackScreen` (`allsys.c:583-595`).

### Frame pacing and simulation decoupling

The main loop runs once per rendered frame (`MainGameLoop`, `src/DETHRACE/common/mainloop.c:562-739`),
and in a normal race (not action replay) every iteration calls
`ApplyPhysicsToCars(..., gFrame_period)` with `gFrame_period` derived from the real frame interval
(`UpdateFramePeriod`, `mainloop.c:408-482`). Car physics is already fixed-step:
`ApplyPhysicsToCars` advances 0..5 steps of `PHYSICS_STEP_TIME` (40 ms) and interpolates after the
steps (`car.c:1290-1420`, `constants.h:912`, `car.c:235`). A 40 ms tick can still run two 40 ms
steps depending on the phase (`gLast_mechanics_time` realignment), so the design needs an explicit
per-tick cap or forced phase alignment before claiming "at most one physics step per tick". After a
stall `UpdateFramePeriod` can feed a ~1000 ms outer period (car physics remains capped at
5 x 40 ms); the 10 ms period floor that would cap high FPS is compiled out only under
`DETHRACE_FIX_BUGS`. What is variable-rate is the **outer simulation** (AI, controls, car animation,
HUD timers, pedestrians, shrapnel, oil, checkpoint/recovery logic, race timers) and the render-side
consumers of `gFrame_period` (`RenderSmoke`, `RenderSparks`, `RenderProximityRays`,
`graphics.c:2052-2055`, `2124-2125`; fancy headup offsets, `displays.c:558,647`).

Target design:

- a fixed outer simulation tick with an accumulator over real time; `ApplyPhysicsToCars` receives
  that tick, and the loop must cap the number of 40 ms physics steps per tick (the current function
  can run two at a phase offset and up to five after a stall);
- rendering as fast as the platform allows, with the latest simulation state; a separate render
  delta (`gRender_period`) for particles, headup fly-ins and other render-side animation;
- a cap on catch-up ticks and an explicit drop/`AddLostTime` for long stalls;
- gameplay RNG must not advance once per rendered frame (`ServiceGameInRace` is called twice per
  iteration, each calling `ServiceTheGame` and `rand()` once, `main.c:93`, `mainloop.c:622,673`;
  `RenderSmoke`/`DrawTheGlow` also call `rand()`/`srand()` from the render path, `spark.c:1481,1568,
  2235,2248`). A dedicated render RNG and a per-tick gameplay seed/stream are required;
- the interface `DoMouseCursor` wait loop (`graphics.c:3037-3050`) must stop blocking the render
  loop; its animation timers are already wall-based, and its `Chance()` use must not consume the
  gameplay RNG;
- replay recording boundaries move to the simulation tick (`PipeCarPositions` is in the loop,
  `PipeFrameFinish` is inside `RenderAFrame`, `graphics.c:2278`);
- network play stays on the legacy variable-period path until all network timestamp sources
  (`senders_time_stamp`, `time_sync.race_start_time`, `this_players_time_stamp`,
  `last_heard_from_him`) are migrated;
- `gKnobbled_frame_period` (`mainloop.c:720-724`) must be logged and skipped in the uncapped mode;
  the installed data currently decodes to 0, but the busy-wait is compiled;
- the recovery budget/cooldown (`video.c:1591-1597`, `1759-1764`) is present-counted; at 200 FPS it
  must be converted to wall time to avoid a faster recovery storm.
- `PDScreenBufferSwap` is skipped during palette fades (`graphics.c:2281`), so the render loop must
  still produce a frame boundary; the driver cannot rely on `doubleBuffer` being called every
  iteration.

The fixed tick can be 20 ms (matching the current measured cadence) or 40 ms (matching the car
physics step). The choice changes non-physics game-logic cadence and needs an A/B; 20 ms is the
less invasive default.

## 5. Resource lifetime and device recovery

- Geometry buffers retired per target are flushed only for the target of the first scene of the
  frame (`gpu.c:1755`). Retired buffers of other targets linger until those targets begin a frame
  and can exhaust the 1024-entry capacity, after which immediate models are skipped.
- Deferred image/buffer releases are flushed after the scene and present fences.
- Target cleanup waits `vkDeviceIdle` before destroying; a target from an older device generation
  is freed without Vulkan calls.
- Textures record `texture_device` and `built_generation`; a device or generation change abandons
  the old image/staging state and rebuilds (`sbuffer.c:797`).
- `video_recover_device_lost` (`video.c:1522`): destroy the instance/device/swapchain, recreate
  them, reset `gpu_disabled`, GPU and texture function tables, log flags and deferred lists. The
  recovery budget is 8 attempts, intended as one attempt per `present_serial`; `VIDEOVkCreate`
  memsets the struct and resets that serial, while `recovery_attempted` is preserved on a
  successful create. After exhaustion a 600-present cooldown allows a new batch. A failed
  `VIDEOVkCreate` resets the attempt budget; Vulkan-call failures go through `video_fail` ->
  `BrFailure` (which can abort), while some pre-call validation failures return `BRE_FAIL` without
  it.
- **Freeze:** after budget exhaustion the device is not destroyed, `video_present_frame` waits the
  lost device's fence, recovery returns `BRE_FAIL`, and the function returns `BRE_OK` without
  acquiring or presenting. The scene path can also call `GpuVkHandleDeviceLost` ->
  `GpuVkDisable` when the present-fence wait in `GpuVkBeginScene` fails. The window freezes while
  the process lives; the 600-present cooldown retries the same batch. This is a silent
  degradation, not an explicit failure.
- `VIDEOVkDestroy`/`VIDEOVkCreate` memset the whole video struct; `present_serial`,
  `recovery_serial`, `recovery_cooldown` and `recovery_exhausted` are not preserved across a
  re-init. `recovery_attempted` is preserved on success, but a failed create does not increment
  it, so the budget can be bypassed on the failure path.

## 6. What is wrong or fragile today

### Correctness

- **Single-target composition.** The present pass samples one 3D target. The target is chosen by
  draw count with an 8-draw floor; that is a heuristic, not a designated main target. The mirror
  and wreck targets are not composited as explicit sub-rects. The cockpit screenshot's mirror-shaped
  region is not proof of a second-target composite.
- **Shared shadow / cross-target ordering.** `gRender_screen`, `gRearview_screen` and the wreck
  area share `gBack_screen`'s CPU shadow. The first full-size target uploads that shared shadow
  into the single overlay image and resets it to magenta; 2D drawn before the first 3D pass (for
  example the map) is erased. The present overlay is therefore not always "fresh". This explains
  the map screenshot showing no map; it is a cross-target ordering bug, not just an intra-target
  one.
- **2D ordering between passes.** The overlay-under is flushed once per target per frame. 2D
  content drawn between two 3D passes on the same target is not re-flushed under the later pass.
- **Skipped swaps.** `PDScreenBufferSwap` is deliberately skipped during palette fades
  (`graphics.c:2281`), so the scene command buffer, `frame_epoch` and target counters span game
  frames. `doubleBuffer` is not called, so the driver cannot fix this by ending on `doubleBuffer`;
  it needs an independent game-frame boundary. Color (`loadOp = LOAD`) can accumulate across
  frames.
- **Descriptor-cache eviction across passes.** `frame_sequence` increments per scene begin; sets
  used by an earlier pass of the same unsubmitted command buffer can be rewritten for a later pass.
- **Per-frame stored-geometry rebuild.** Normal `BrModelUpdate` frees and recreates
  `model->stored` (`prepmesh.c:1204-1212`), so vkrend rebuilds vertex/index buffers on the next
  draw. The cache tracks no revision; direct data mutations or preprepared models can bypass the
  rebuild, but that is not the common dynamic-model path.
- **Camera NaN / assert.** The shadow pass temporarily adds a hither fudge to the shared
  `gCamera->hither_z` (`graphics.c:1687-1729`). A NaN (or -Inf) `pos_cam_space.v[2]` bypasses the
  comparisons and the `< 0.0002` clamp, making the fudge NaN and poisoning the shared camera; the
  assert at `actsupt.c:494` then fires. `+Inf` takes the `>= 36` branch and is safe. The shadow
  fudge is the immediate NaN path; `CheckCameraHither` (`car.c:7431-7443`) restores through
  `static int old_hither`, which stores 0 for the shipped hither (0.02) and only handles NaN. A
  restored 0 does not produce the ProcessShadow assert in the normal fudge branch (+0.0002 masks
  it), but it still violates the assert at the next camera call (the world pass), and with a low
  yon (`gYon_multiplier` 0.125 can make yon 0.625) a car at `pos_z >= yon` gets fudge 0 and the
  restored 0 asserts in the shadow pass. A ~1000 ms outer period after a device-loss stall
  (`UpdateFramePeriod` re-adds a stale `error`; car physics is capped at 5 x 40 ms) is a plausible
  source of the non-finite transform. The only 20:54 crash dump on disk is a `vulkan-1.dll`
  fail-fast (`0xc0000409`), not a ProcessShadow dump; the exact NaN origin remains open. The assert
  itself is doing its job and must not be weakened.
- **TDR.** In G/L/M each run produced **one** Windows Display 4101 event about 34-36 seconds after
  process start; the 7-8 `Vulkan device lost, reinitialising` lines are retry attempts, not repeated
  OS TDRs. The event is system-wide and does not name the process. Pre-V3a runs also produced 4101
  events plus a `vulkan-1.dll` fail-fast (BEX64), so V3a is not the origin. M's marker log has no
  scene-side `vk failure at` line; the only named stage is L's present-fence wait at scene begin.
  The first faulting call is not logged. Cross-pass descriptor reuse (which requires more than 512
  distinct views on one target) is a strong code-level candidate; sampling the persistent target in
  the composite is another. The A-D white/no-draw runs are menu-only and not race controls. A
  10-minute software race control is in progress; an earlier software run never left the grid
  screen.

### Performance

- **13-19 scene passes per race frame** (one projected shadow pass per car, the world, the mirror,
  and the `gDevious_2d` dim/pratcam passes). HUD text/gauges are CPU 2D, not scene passes; only the
  dim/pratcam group is removable by `gDevious_2d`; the shadow/world/mirror passes are the game's
  render structure.
- **`gDevious_2d = 0` is not a safe flag flip.** The CPU `DimRectangle` fallback indexes an 8-bit
  shade table byte-by-byte (`displays.c:331-359`); on the RGB565 back screen it corrupts pixels. A
  16-bit dim path is required before the HUD passes can be removed.
- **CPU 2D work every frame**: full-surface overlay row copy, full-surface magenta reset, and a CPU
  depth-buffer fill (`graphics.c:1980`) that the GPU path never reads (the GPU depth image is
  cleared by the render pass; the CPU depth/fog pass is disabled because `gMaterial_fogging=1`,
  while material fogging itself is enabled; 2D smoke/sparks are disabled). The lores->hires
  conversion is not part of the 640x480 race path.
- **One frame in flight** with an immediate scene-fence wait.
- **Per-frame dynamic rebuilds**: shadow models are added/removed per car per frame; the pratcam
  texture is re-added/removed per frame; immediate `v11model` draws allocate two buffers; normal
  `BrModelUpdate` also frees/recreates stored objects and forces a geometry rebuild.
- **Synchronous texture uploads**: command pool + submit + fence wait per rebuilt texture.
- **Full-surface overlay upload** instead of dirty rectangles; no previous shadow to diff because
  it is reset to magenta.
- **Per-draw CPU cost**: 192-byte UBO write, descriptor bind, linear descriptor-cache scan.
- **FIFO hardcoded**; no uncapped mode. On the 165 Hz monitor, 200 FPS is a render-loop
  throughput target, not a displayed-frame target.
- **Debug build**: the only race measurement is Debug `/Od /Ob0 /RTC1`. Release/RelWithDebInfo does
  not exist in the tree, and 200 FPS cannot be claimed from Debug. Release needs explicit
  authorization.

## 7. Target render graph

The goal is an explicit frame graph with stable resources, a real main target and a named
composite stage, not an incidental sequence of renderer callbacks.

```text
Frame N (slot = N % 2 or 3)
  wait slot.fence                     # only when recycling this slot
  acquire swapchain image             # slot.acquire_sem
  begin slot.command_buffer
    pass 0: begin frame               # explicit game-frame boundary, independent of swaps
    pass 1: main target clear + 2D underlay   # dirty rects, before the 3D passes that sample it
    pass 2: projected shadows         # batched where clip state allows; not a shadow map
    pass 3: world                     # main target
    pass 4: rearview target clear + underlay + mirror   # own target, composited as an inset
    pass 5: composite                 # main target + mirror/wreck insets + overlay
  end + submit (wait acquire_sem, signal render_sem[image], slot.fence)
  present (wait render_sem[image])
```

The sim/render split sits above this graph: a fixed outer simulation tick updates the world
(physics, car animation, HUD timers) on its own accumulator, while the render graph runs at the
display or uncapped rate and consumes the latest state. The graph must not derive its pacing from
`gFrame_period`.

All `src/DETHRACE` behavior changes stay under `DETHRACE_FIX_BUGS` (reccmp builds force it off
along with `DETHRACE_VULKAN`); vkrend is only built when `DETHRACE_VULKAN=ON`, and engine changes
stay in the BRender fork. The default, software and OpenGL paths must be unchanged, and Linux must
keep compiling.

Two submit designs are possible and the document must not mix them:

- **one submit**: scene and composite share `slot.command_buffer`; no scene-done semaphore is
  needed;
- **two submits**: the scene submit signals `slot.scene_done`, the composite submit waits it; the
  graph must show the semaphore and the second submit.

Required changes, in dependency order:

1. **Correctness first**
   - designate a real main target (screen/world), not a draw-count heuristic;
   - add a composite stage with explicit source/destination rects for the mirror and wreck targets;
   - introduce a scene-record layer (CPU-side pass records: target, viewport, state, draws) so the
     driver can reorder, cull and express dependencies;
   - add an independent game-frame boundary (not `doubleBuffer`) and a color-reset policy for
     skipped swaps;
   - fix the shared-shadow/overlay ordering: upload and draw the 2D underlay before the 3D passes
     that sample it, and do not erase 2D content that belongs to a later target;
   - make descriptor sets immutable for the duration of a recorded batch (pin them or use a
     per-frame-slot descriptor arena);
   - batch the in-range car shadow geometry into a single projected-shadow pass where the clip
     state allows it; per-car camera fudge/clip planes otherwise keep separate passes;
   - define a recovery state machine: HEALTHY -> RECOVERING on a loss (at most one attempt per
     logical frame), RECOVERING -> HEALTHY on success, RECOVERING -> DEGRADED on failure (present a
     held/2D frame, no waits on the lost device), DEGRADED -> FAILED when the budget/cooldown is
     exhausted (log once, stop presenting, no abort/storm);
   - track model revisions in the geometry cache for direct data mutations/preprepared models;
   - fix the camera hither path (finite/range guards, float restore, separate per-camera state)
     without weakening the BRender assert;
   - establish the TDR root cause with `VK_EXT_device_fault`, GPU-assisted validation,
     `robustBufferAccess`, object names, and the present-black/per-pass-submit/geometry-texture
     ablations. `VK_EXT_device_fault` requires Vulkan 1.1 or `VK_KHR_get_physical_device_properties2`;
     the current instance is Vulkan 1.0, so the API version/extension and
     `VkPhysicalDeviceFaultFeaturesEXT` must be enabled, and the query must run before
     `VIDEOVkDestroy`. The first control run should be a software/OpenGL race to rule out a
     machine-wide cause.
2. **Frame pacing**
   - 2 frames in flight: per-slot command buffers, fences, acquire semaphores, scene targets, UBO
     rings, overlay **images** (not only staging slots), per-slot CPU shadow snapshots (the
     sub-pixelmaps share the parent shadow), descriptor arenas and deferred-deletion queues;
   - render every iteration: advance the simulation only when the accumulator is due, call
     `RenderAFrame` every iteration with `gRender_period`, and move `PipeFrameFinish`/present side
     effects to the correct boundary; otherwise the render rate remains the simulation rate;
   - replace the immediate scene-fence wait with a scene-done semaphore waited by the composite
     submit (two-submit design) or fuse the composite into the scene command buffer (one-submit
     design);
   - convert the recovery budget/cooldown to wall time with a portable monotonic clock.
3. **Simulation decoupling**
   - fixed outer tick with an accumulator (20 ms default, 40 ms compatible with `PHYSICS_STEP_TIME`);
   - a separate `gRender_period` for particles/headups; gameplay RNG isolated from render rate;
   - replay recording boundaries moved to the simulation tick; network stays legacy until all
     timestamp sources are migrated.
4. **Uploads and geometry**
   - persistent upload pool / staging ring per frame; batch texture builds before recording;
   - persistent device-local vertex/index/UBO rings; update dynamic models in place; pool immediate
     models;
   - texture prewarm/discovery pass so builds do not happen inside recording;
   - index/palette textures as R8 + palette LUT so CLUT animation does not rebuild every texture.
5. **2D layer**
   - dirty-rectangle tracking with a previous-image buffer (the shadow is reset to magenta today);
   - a 16-bit `DimRectangle` path (scalar or LUT, preserving signed stride, corner knock-out,
     clipping and `gDim_amount` indexing) before `gDevious_2d` can be disabled;
   - split the single `gDevious_2d` flag so the pratcam CPU path and its 128->104/110 buffer
     dimensions do not change when only the dim pass is disabled.
   - drop the unused CPU depth fill for vkrend (keep a one-time fill).
6. **Presentation and measurement**
   - `VKREND_PRESENT_MODE=fifo|mailbox|immediate`, default FIFO; `VKREND_UNCAPPED=1` for measurement,
     IMMEDIATE if supported, otherwise fail loudly; log the chosen mode, image count and refresh;
   - request at least 4 swapchain images, clamp to `maxImageCount`, and fail clearly if fewer are
     available; 2 frames in flight in uncapped mode;
   - a portable monotonic timer (QPC on Windows, `clock_gettime(CLOCK_MONOTONIC)` on Linux) and a
     frame CSV (frame, sim ticks, render, pass count, draws, scene wait, present, bytes, texture
     builds/waits) with p50/p95/p99/max;
   - acceptance for 200 FPS uses **mean frame time <= 5.0 ms** (1/mean >= 200), with p50/p95/p99
     reported; p50 <= 5 ms alone does not imply 200 FPS.
7. **Opt-in path tracing (later)**
   - capture per-draw data at scene time into frame-owned records, persistent BLAS/TLAS, HDR target,
     denoise/tonemap, composite after the 2D overlay; keep the raster path and DDI semantics
     unchanged when PT is off.

## 8. Verification and diagnostics

- `VKREND_VALIDATION=1` enables the Khronos validation layer; it is opt-in and writes
  `vkrend_validation.txt`. It currently enables only the core layer, not GPU-assisted or
  synchronization validation.
- `vkrend_markers.txt` is a diagnostic log (scene starts capped at 48, compositor activation,
  failures). Failure markers are not bounded, and `GpuVkLogMarker` opens/appends the file per
  message; it is not a profiler.
- No `VK_EXT_device_fault`, `robustBufferAccess`, GPU-assisted validation, object names or GPU
  timestamp queries are enabled today. The fault query must run before `VIDEOVkDestroy` in
  `video_recover_device_lost`.
- Recommended: device-fault reporting, QPC frame/pass CSV, per-pass counters, a frame-ID canary for
  cross-frame aliasing, and explicit degradation state (a visible error/stop, not a silent
  `BRE_OK` with no present).
- Acceptance is split into two gates:
  - **Debug correctness gate:** a 10-minute race with no Display 4101, no device loss, no camera
    assert, validation clean, world/mirror/cockpit/map/summary matching the software reference,
    physics rate unchanged, one game instance at a time, and the same replay/input run at 60 FPS,
    165 FPS and uncapped keeping the simulation tick rate and physics trajectory invariant (the
    20 ms vs 40 ms outer-tick choice is resolved by A/B);
  - **Release performance gate (explicit authorization required):** 640x480 race on the RX 9070 XT,
    uncapped IMMEDIATE mode, mean frame time <= 5.0 ms with p50/p95/p99/max and the fraction above
    5 ms reported. 200 FPS means render-loop throughput unless present timing proves displayed
    frames; FIFO remains the compatibility default; Linux must still compile.

## 9. Glossary

- **Target**: a `vk_gpu_target` for a root or sub-pixelmap - GPU colour/depth images plus
  pipelines/descriptors.
- **Overlay**: the single persistent R5G6B5 image holding the CPU 2D shadow; magenta is the key.
- **Pass**: one `GpuVkBeginScene`/`GpuVkEndScene` pair, i.e. one BRender renderer scene.
- **Scene command buffer**: the single command buffer that records all passes of a frame; it can
  span game frames when `PDScreenBufferSwap` is skipped.
- **Present command buffer**: the separate command buffer that uploads the overlay and composites
  to the swapchain image.
- **Projected shadow**: the game's dark car geometry rendered with clip planes and a temporary
  hither fudge; not a light-depth shadow map.
