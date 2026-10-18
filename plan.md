# vkrend modernization plan

Status: revised after the deep-reasoning review (8 solvers, 4 blind verifiers, 15 hypotheses).
The previous A0-A8/B0-B8 plan was not executable as written: it measured nothing first, put the
200 FPS gate before the only levers that can remove the measured cost, duplicated ownership, and
claimed gates that cannot run. This revision is the execution baseline.

## Review verdict (evidence-backed)

Confirmed: the fixed 20 ms tick is unguarded (`mainloop.c:40,586,640-659`); visual capture under
vkrend aborts (`PrintScreenFile16` -> `NOT_IMPLEMENTED`), software writes 8-bit BMP and the
comparator accepts 24-bit only; `VKREND_FPS` is a 300-frame average on 1 ms ticks; `VKREND_UNCAPPED`
can fall back to MAILBOX/FIFO (logged, not fail-loud); BRender submits via `BrZbSceneRenderAdd`, not
a scene walk; vkrend ignores order tables and does not implement clip planes; `VKREND_LEGACY_PATH`
and a public versioned scene record do not exist; skipped swaps break the frame boundary; a single
descriptor set + 2-3 frames in flight is unsafe; `image_count = minImageCount + 1`; feature
negotiation/recovery gates are missing; 16-bit indices and a generation-only geometry cache cannot
hold a merged world.

Refuted or qualified: damaged cars are not rebuilt every frame unconditionally (events/repair
periods; D0 must measure frequency); A6's "passes <= 5" before B3 is not proven impossible (default
shadow level may be one scene); A7 "unreachable" is not measured; the blanket "missing gates" is
false for one-instance/Linux/reccmp CI, true for executable local gates. The 19.18 ms figure is a
Debug white-texture diagnostic; 0.1 ms/draw is a fitted average, not a measurement. The mirror pass
is a second full world traversal; shadow models are re-added per car per frame.

## Principles

- Measure first (D0). Do not build architecture before the 15-22 ms is attributed.
- Single-threaded. No job system, worker threads or secondary command buffers.
- BRender stays the asset loader and software/OpenGL fallback. vkrend consumes a flat record
  produced at the Add/DDI boundary, not a second scene walk.
- Fixed 20 ms simulation tick under `DETHRACE_FIX_BUGS` (or an explicit game-side capability);
  reccmp/software/OpenGL with it off keep legacy pacing. One staged producer selector, not a
  matrix of booleans.
- 200 FPS = render-loop throughput, `1000/mean(frame_dt)`, in an authorized optimized build with
  asserted IMMEDIATE; FIFO remains the default and cannot prove 200.
- Keep the 512-set/single-sampler path and plain indexed draws until D0/D2 show that descriptor
  binds or draw calls are the dominant bucket. Bindless, indirect-count, frame graph and 3 FIF are
  optional D4 items, not prerequisites.

## Owners

| Deliverable | Owner |
| --- | --- |
| Frame boundary, record schema, arenas | D1/D2 layer |
| Descriptor immutability | D1 layer |
| Feature/recovery safety | D1 layer, before any FIF |
| Capture/determinism harness | D0b layer + game |
| Damage-stable geometry | D1 game-side (crush/prepmesh) |
| Shadows | B3 game-side; D2 batching API |
| Mirror | B2 game-side; D3 composite |
| UI/dim | B4 game-side; D1 dirty overlay |
| Overlay | D1/D3 layer |
| Frame graph executor | D3 layer |
| Pass producer | B2 game-side |

## Phase 1 - Vulkan layer

### D0 - attribution and baseline (mandatory)
QPC per-frame CSV at the `MainGameLoop` boundary, plus scopes for: sim block, `RenderAFrame`,
mirror block, per-car shadow construction/pass, first scene begin to `GpuVkFrameEnd`, texture
lookup/build, descriptor bind/UBO/draw, overlay upload/reset, present. Counters: scenes, draws,
v1model uploads, `stored` recreations, `BrModelUpdate`/damage rebuild frequency, texture builds,
overlay bytes, UBO slot use, sim ticks, `gFrame_period`, the camera 10 ms clamp and
`gKnobbled_frame_period`. Views: forward and default cockpit+mirror; damage/repair; palette fade;
map. Ablations: mirror off, shadow none/us_only/everyone, `NO_GEOMETRY`, white-sets/view, palette
static/fade. Baselines in Debug and one authorized optimized build.
Exit: committed CSV and p50/p95/p99 per phase; the dominant bucket is named; mirror-on/off and
damage-frequency counts are present; no 200 FPS claim. D0b is part of the D0 exit.

### D0b - capture, references and determinism harness
Offscreen 24-bit capture for vkrend and software, committed references for 11 screens: forward,
mirror, cockpit (mirror inset), map, summary/wreck, pratcam, fade, dim, gauges/powerups,
frontend/menu, damaged/repair. Comparator tolerance: <=0.5% pixels with any channel diff >8, no
16x12 cell mean >8, HUD band <=1%. Add `--seed`/fixed seed and a render cap honored by vkrend
(not the SDL limiter), plus a sim-tick/replay hash test. These are prerequisites for any D2/D3
parity claim. Extend `compare_vkrend_frames.ps1` with the cell/HUD metrics above.
Exit: committed references; comparator extended; seed/cap/replay-hash test passes (tick count
±1%, car positions <=1e-3 m or equal hash).

### D1 - flatten and stabilise
Gate the fixed tick; add `BrVkrendFrameBegin/End` at `RenderAFrame`/menu/flic boundaries independent
of `doubleBuffer`; one versioned POD record schema shared by vkrend and the rewrite; stable dynamic
geometry ring (no per-frame `vkCreateBuffer`, no steady-state re-upload); pin descriptor sets for
the recorded frame (per-pass/bindless sets are D4; never evict a set used by an earlier pass);
feature/fallback table, fail-loud uncapped, `image_count >= frames + 1`, forced-loss test,
device-fault query. Slot-aware recovery is a precondition for any D4 FIF.
dirty-rect overlay/single upload. Mirror proxy reuse belongs to B2; D1 only stabilizes records.
Exit: palette-fade A/B; zero steady-state geometry uploads; no skipped-swap accumulation.

### D2 - batch, measured
Static arena with UINT32 indices + per-instance/indexed draws over shared model ranges; clip planes
implemented in raster; opaque order bucket + blended submission-order bucket; one shadow pass/batch
where clip state allows; draw target <40, CPU record target [[to be set by D0]] .
Exit: measured saving vs D0; shadows/cockpit/map parity; no unclipped artifacts.

### D3 - share and composite
Explicit main target; mirror/wreck sub-rect composite; overlay once per frame; correct sub-pixelmap
base rects (`base_x/base_y`) and viewport/scissor.
Exit: forward/mirror/cockpit/map/summary parity; no blank map; pass/scene counts measured.

### D4 - architecture only if D0/D2 require
Per-slot descriptor sets + timeline wait + slot-keyed deferred destruction (required before any
2-3 FIF); device-fault query and slot-aware recovery before enabling FIF; bindless/descriptor
indexing; indirect-count/GPU culling; frame graph/transient aliasing; dynamic rendering. Each item
needs a D0/D2 measurement that justifies it.
Exit: validation clean, churn race clean, measured win.

## Phase 2 - legacy renderer rewrite plan

Update `docs/vkrend-renderer-rewrite-plan.md` to the verified boundary:
- B0 parity harness: offscreen 24-bit capture for softrend and vkrend, committed references,
  11 screens, seed/render cap, tick/replay hash.
- B1 extraction at `BrZbSceneRenderAdd`/DDI, not a scene walk; track column/lollipop culling and
  order-table semantics; one record format shared with D1.
- B2 frame builder after B1 and D3; A4 is the executor, B2 the pass producer.
- B3 shadows: clip planes first, split generation from submission; no camera mutation.
- B4 UI: full HUD/map/cockpit/pratcam/flic scope, map underlay/world viewport; 16-bit dim bridge;
  glyph atlas later.
- B5 palette: global tint/CLUT post-process, exact dim; R8+palette LUT only if measured.
- B6 single-thread state/material/batch reduction; no threads.
- B7 remove the per-group BRender hot path from vkrend behind a game-side producer selector;
  keep software/OpenGL.
- B8 final 200 FPS gate.

## Phase 3 - execution

D0 + D0b -> D1 -> D2 -> D3 -> (D4 only with evidence) -> B0-B8. A7-style checkpoints are provisional; the
final 200 FPS gate is after B8. Create `VKREND_LEGACY_PATH` in D1 as a game-side producer selector,
defaulting to the legacy producer until B2.

## Acceptance

- Debug: 10-minute race, validation loaded and zero ERROR lines, no Display 4101, no device loss,
  no camera assert, world/HUD/mirror/cockpit/map match software within the comparator tolerance,
  one game instance. Software/OpenGL smoke, `FIX_BUGS=OFF`, reccmp report, Linux ON/OFF builds.
- Performance: authorized optimized build, 640x480, asserted IMMEDIATE, 5 x 60 s after 10 s
  warm-up, N >= 8000 frames per run; `1000/mean(dt)`; median run mean <= 4.75 ms, every run
  <= 5.00 ms, p99 <= 6.5 ms, <= 0.5% frames > 5 ms; forward and cockpit+mirror, damaged and
  repair; report mean/p50/p95/p99/max and presented vs render-loop.
- FIFO control: 60 s, present mode 2, presented rate and no loss; it is not a throughput gate.
- Visual: D0b capture; 11 screens; comparator tolerance; software/OpenGL references.

## Local gates

- Build: configure with `-DBUILD_TESTS=ON`; `cmake --build build-vk-on --config Debug --target dethrace`.
- Software: run `build-vk-on\Debug\dethrace.exe --window --dir <CARMA>` for 60 s; no crash/visual smoke.
- OpenGL: same with `--opengl`.
- Validation: `VKREND_VALIDATION=1`; require "validation layers enabled" and zero `VUID`/`ERROR` lines.
- Reccmp: configure `-DMSVC_42_FOR_RECCMP=ON`, build, run the reccmp report; no new mismatch.
- `FIX_BUGS=OFF`: configure/build with `-DDETHRACE_FIX_BUGS=OFF`, ctest.
- Linux: local WSL/container configure/build `-DDETHRACE_VULKAN=ON` and default, ctest; CI alone
  is not a substitute.
- Recovery: D1 adds `VKREND_FORCE_DEVICE_LOST`; three losses, recovery <=10 s, no freeze/abort,
  plus a Display 4101 query over the run window.
- One instance: a runner check for `dethrace`/`quakeray`; serialize runs.
- If 5 ms is not reached, report the dominant D0 bucket and stop before claiming 200 FPS.

## Risks

- A0 may show traversal/mirror/shadow, not submission, dominates; D2/D3 must be re-aimed before D4.
- The visual comparator needs a new capture path; without it no parity claim is valid.
- The tick gate must not break the user-visible FPS decoupling; verify with a seed/replay test.
- Windowed IMMEDIATE can be compositor-limited; render-loop and presented rates stay separate.
