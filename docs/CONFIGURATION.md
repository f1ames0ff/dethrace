# Configuration

Dethrace looks for a `dethrace.ini` file in the [system preferences directory](https://wiki.libsdl.org/SDL2/SDL_GetPrefPath):
| Platform | Example Path                                                                 |
|----------|----------------------------------------------------------------------|
| Windows  | `C:\Users\<NAME>\AppData\Roaming\dethrace\dethrace.ini`                |
| macOS    | `/Users/<NAME>/Library/Application Support/dethrace/dethrace.ini`      |
| Linux    | `/home/<NAME>/.local/share/dethrace/dethrace.ini`                      |


If this file is not present, dethrace will run with default configuration and attempt to discover the correct Carmageddon directory to use (see below).

## Example dethrace.ini file
```ini
[General]
; Enable original CD check
CdCheck = 0

; Enable original censorship check
GoreCheck = 0

; set to 0 to disable
FPSLimit = 60

; Full screen or window
Windowed = 1

; 3dfx mode (via OpenGL)
Emulate3DFX = 0

; Censored zombie/robots mode
BoringMode = 0

; Play cut scenes on startup and between races
Cutscenes = 0

; "hires" mode is 640x480, otherwise default 320x200
Hires = 1

; Only used in 'demo' mode. Default demo time out is 240s (4 mins)
DemoTimeout = 240

; Which directory in the [Games] section to run
DefaultGame = c1

; Original physics updates every 40ms, causing noticable camera juddering when cornering at slow speed
; Enabling this runs physics every frame, smoothing movement out
PhysicsPerFrame = 1

[Games]
c1 = /opt/carma/c1
c1demo = /opt/carma/c1demo
sp = /opt/carma/splatpack

[Cheats]
EditMode = 0
FreezeTimer = 0
GameCompleted = 0

[Sound]
Enabled = 1
SoundOptionsScreen = 1
VolumeMultiplier = 1

[Network]
AdapterName = ""
```

## Order of precedence for game directory detection:
1. Directory pointed to by DefaultGame
2. First game in the list if at least 1 game dir is specified
3. `--dir` command line argument
4. Current working directory (if `DATA/GENERAL.TXT` exists)
5. `SDL_GetPrefPath` directory

## Vulkan (experimental)

Builds configured with `-DDETHRACE_VULKAN=ON` (requires the SDL2 or SDL3 platform driver) accept:

- `--vulkan` - probe for a Vulkan device; falls back to the default renderer with a warning
  when Vulkan is unavailable.
- `--require-vulkan` - like `--vulkan`, but exits with an error when Vulkan is unavailable.

`--vulkan` cannot be combined with `--opengl`/`Emulate3DFX`. This feature is under active
development: currently only the device probe is implemented and logged, no rendering path uses
Vulkan yet.
