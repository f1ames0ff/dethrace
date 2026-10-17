# vkrend parity checklist

Living document: what the `vkrend` BRender driver renders today, compared with the reference.
The parity target is the software renderer (`softrend`) - that is the frame the game shows by
default; `glrend` (the OpenGL driver) is the porting reference for DDI behaviour only, not a
visual target.

Snapshot: 2026-10-17, dethrace `a7d6b92` plus the uncommitted V2c work under
`lib/BRender-v1.3.2/drivers/vkrend/`. `gpu.c` was being extended while this was written, so its
line numbers can drift by a few lines; the symbol names are stable. Driver-wide fallback: when any
GPU step fails, `GpuVkDisable` (`gpu.c:201`) puts the device into record-only mode and the game
keeps running without GPU output.

## Feature status

- **Targets / present** - `partial`. Offscreen colour targets become Vulkan render targets (RGBA8
  colour + D16 depth) with CPU shadows (`gpu.c` `GpuVkTargetInit` ~892); the RGB565 front buffer is
  presented through the device swapchain (`devpixmp.c:409` `doubleBuffer` -> `video.c:1590`
  `VIDEOVkPresent` -> `video.c:1207` `video_present_frame`, FIFO preferred `video.c:663-696`).
  Only `BR_PMT_RGB_565` render targets and `BR_PMT_DEPTH_16` depth targets are accepted
  (`devpixmp.c:147`, `devpixmp.c:306-319`).
- **Scene begin/end + readback** - `done`. `renderer.c:48` / `renderer.c:59` drive
  `GpuVkSceneBeginRenderer` (`gpu.c` ~1527) and `GpuVkSceneEndRenderer` (~1606); scene end submits
  and waits (`GpuVkEndScene` ~1465), dumps up to four frames (`gpu.c:1134` `GpuVkDumpImage`) and
  copies the result back into the CPU shadow (`gpu.c:1197` `GpuVkReadback`).
- **Projection / depth** - `partial`. `gpu.c:971` `GpuVkProjection` converts BRender
  `view_to_screen` into Vulkan clip space (flip Y, depth `0.5w - 0.5z`); the depth attachment is
  created and cleared per scene (`gpu.c:489` render pass, `gpu.c:916` depth image). Depth test and
  write are hardcoded to `LESS_OR_EQUAL` with writing on (`gpu.c:764-770`); the per-state
  `depth_test` token and `PRIMF_DEPTH_WRITE` are captured (`state_primitive.c`) but not applied.
- **Vertex colours** - `partial` (landing in the uncommitted V2c change). The vertex buffer now
  carries position + UV + RGBA (`gpu.c:9` stride 36, attributes `gpu.c:716-735`, upload
  `gpu.c:1677-1719`) and the fragment shader multiplies model colour x vertex colour x texture
  (`shaders/model.frag:19`). No rendered frame has been verified yet.
- **Textures / colour maps** - `partial`. Colour maps resolve per group with group -> default-state
  fallback (`gpu.c:1018` `GpuVkMaterialResolve`); a 512-entry descriptor cache reuses texture sets
  within a frame (`gpu.c:11`, `gpu.c:1065`) and a white texture is used when a colour map has no GPU
  texture (`gpu.c:1073-1078`). The pixel upload (`BufferStoredVkGetTexture`) is declared at
  `gpu.c:23` but has no definition in this snapshot, and `sbuffer.c` only records the source
  pixelmap - index8+palette versus RGB565 coverage is `unknown`.
- **Opacity / blending** - `partial`. Opacity is clamped (`gpu.c:1053-1056`), written into the UBO
  (`gpu.c:1003`) and scales alpha (`shaders/model.frag:21`); the pipeline blends every draw with
  `SRC_ALPHA` / `ONE_MINUS_SRC_ALPHA` (`gpu.c:772-786`). `PRIMF_BLEND` gating and the per-state
  blend modes (`BRT_BLEND_SUMMED`, premultiplied, dimmed) are captured (`state_primitive.c:20`) but
  not implemented.
- **Colour key** - `partial`. `PRIMF_COLOUR_KEY` is captured (`state_primitive.c:14`) and resolved
  (`gpu.c:1059`); the shader discards texels that are exactly `(0,0,0)` when the flag is set
  (`shaders/model.frag:20`). No keyed-alpha or palette-index variants; unverified in a run.
- **Lighting** - `missing`. Light state is captured (`state_light.c`, `state.h:83-98`) and
  `LIGHTING_B` / `PRELIGHTING_B` are captured (`state_surface.c:26-27`), but the draw path reads
  only colour/opacity/texture/key (`gpu.c:1018`) and the shaders contain no lighting terms
  (`shaders/model.frag`), so lit geometry renders flat.
- **Fog** - `missing`. `FOG_B`, fog colour and fog range are captured (`state_primitive.c:40`,
  `state.h:190-193`); `GpuVkWriteUbo` (`gpu.c:987`) writes only textured/opacity/colour-key
  parameters and neither shader applies fog.
- **Order tables / blending order** - `missing`. `V1ORDER_TABLE_P` is captured
  (`state_hidden.c:14`) but nothing consumes it; both the stored and the non-stored path issue
  `vkCmdDrawIndexed` group by group in model order (`gpu.c` `GpuVkDrawGeometry` ~1802).
- **Sky / horizon** - `unknown`. There is no sky- or horizon-specific code in `drivers/vkrend/`;
  the horizon is an ordinary actor model, so its parity depends on the model, texture and ordering
  rows above.
- **2D / HUD** - `partial`. 2D writes go to CPU shadows through `directLock` / `directUnlock`
  (`devpixmp.c:490`, `devpixmp.c:501`); the shadow is uploaded at scene begin (`gpu.c:1225`
  `GpuVkUploadScene`) and overwritten by the GPU readback at scene end (`gpu.c:1197`), so only
  pixels outside the 3D pass survive. vkrend requires the hires 3dfx buffer class
  (`allsys.c:414-432`); there is no CLUT handling inside the driver (no `devclut`), and the front
  end uses the lores INDEX_8 path converted by the harness.
- **Non-stored models** - `partial` (this change). `gv1model.c` `_render` / `_renderOnScreen` were
  silent no-ops and now call `GpuVkRenderV1Model` (`gv1model.c:97`, call at `gv1model.c:122`),
  returning `BRE_FAIL` when there is no active GPU scene; render statistics mirror the stored path
  (`gv1model.c:125-129`; stored: `gstored.c:156-160`). The GPU side uploads the model on every draw
  with no per-model cache (`gpu.c:1903-1941`). A rendered frame is unverified.
- **Performance** - `unknown` (no measurements taken). Structure only: stored geometry is cached
  per object (`gstored.h:53`, `gpu.c:1768` `GpuVkGeometryEnsure`) while non-stored models re-upload
  every draw (`gpu.c:1922-1937`); one command buffer, one fence wait and a full-target
  upload/readback per scene (`gpu.c:1225`, `gpu.c:1465`); UBO ring 8192 slots x 256 B, 160 B used
  (`gpu.c:6-8`); texture descriptor sets are evicted when unused in the current frame
  (`gpu.c:1094-1105`).

## How to verify

1. Reference frame: run the game without `--vkrend` (software renderer, the parity target) and
   capture the frame as a 24-bit BMP. Candidate frame: run with `--vkrend` and capture the same
   frame; the driver also writes its own dumps - `vkrend_scene<N>.bmp` (up to four, `gpu.c:1149`)
   and `vkrend_shadow.bmp` (`devpixmp.c:429`).
2. Compare the two BMPs:
   `powershell -File tools\compare_vkrend_frames.ps1 -Reference soft.bmp -Candidate vk.bmp`.
   It prints the image sizes, per-channel mean absolute difference, the fraction of pixels
   differing by more than the threshold (default 8 per channel), a 16x12 grid of per-cell mean
   differences (sky/world/HUD regions), and a `MATCH` / `MISMATCH` verdict. Tune with `-Threshold`
   and `-MaxDiffFraction` (default 0.02 = 2% of pixels). Exit codes: 0 match, 1 mismatch, 2 input
   error.
3. Smoke-test the driver itself: `powershell -File tools\run_vkrend_probe.ps1` starts the game,
   drives the menu, checks the VKREND markers and access violations, and refuses to start when
   another dethrace instance is already running.
