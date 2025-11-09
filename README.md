# Elemental Breakout

Elemental Breakout is a fast-paced twist on the classic brick breaker, built with C++20 and [raylib](https://www.raylib.com/). Swap your paddle's element on the fly, trigger cascading reactions, and chain combos as the ball picks up new elemental properties.

## Gameplay Overview

The game opens on an in-engine instruction screen. Press `Enter` or `Space` to jump into the action.

- **Paddle movement**: `←/→` or `A/D`
- **Launch ball**: `Space`
- **Pause**: `P`
- **Element swap**: `1-5` chooses from five elemental palettes
- **Forfeit run**: `Q`
- **Restart after game over**: `Enter`

Only one life stands between you and defeat. Clear every brick to spawn a fresh randomized wave and increase the ball speed by 15%.

### Elemental Reactions

Mixing ball, paddle, and brick colors unlocks powerful interactions:

- **Overloaded** (`Purple` ball + `Red` paddle) → next impact causes an AoE blast.
- **Swirl** (`Green` ball vs. non-green brick) → spreads the ball's element through the cluster.
- **Freeze** (`Blue` + `Light Blue` paddle) → ball parks on the paddle, then freezes a connected cluster.
- **Melt** (`Red` ball vs. frozen brick) → thaws bricks back to their base state.
- **Vaporize** (`Blue` ball vs. `Red` brick) → instant brick destruction.
- **Liquefy** (`Light Blue` ball vs. `Red` brick) → converts the brick to blue.
- **Superconduct** (`Purple` + `Light Blue` paddle) → ball phases through bricks briefly.
- **Surge** (`Purple` ball vs. `Blue` brick or vice versa) → chain lightning clears diagonal targets.
- **Infuse** (non-green ball vs. `Green` brick) → repaints matching greens to the ball's element.
- Frozen clusters shattered by other elements propagate the break.

Keep an eye on the reaction banner near the bottom of the screen to track active effects and timers.

## Project Layout

- `src/` – Core gameplay systems (`ElementalGame`, `InstructionsScreen`, `AudioManager`, `main`)
- `sounds/` – Bounce and game-over audio assets
- `CMakeLists.txt` – CMake configuration targeting a single executable (`elemental_pong`)
- `run.sh` – Convenience script to configure, build, and launch the game

## Prerequisites

- **C++20** capable compiler (Clang, GCC, or MSVC)
- **CMake ≥ 3.22**
- **raylib** installed with CMake config files (`find_package(raylib CONFIG REQUIRED)`)

### Installing raylib

| Platform | Recommended command |
| --- | --- |
| macOS | `brew install raylib` |
| Ubuntu/Debian | `sudo apt install libraylib-dev` (or build raylib from source for the latest release) |
| Windows (MSYS2) | `pacman -S mingw-w64-x86_64-raylib` |

If you build raylib manually, be sure to install it (`cmake --install`) so that `raylibConfig.cmake` is discoverable by CMake. Set `CMAKE_PREFIX_PATH` or `RAYLIB_DIR` if CMake cannot locate it automatically.

## Building & Running

### One-liner script (macOS/Linux)

```bash
/Users/maxcui/Downloads/ElementalBreakout/run.sh
```

The script creates `/Users/maxcui/Downloads/ElementalBreakout/build`, builds the `elemental_pong` binary, and launches the game.

### Manual CMake workflow

```bash
cmake -S /Users/maxcui/Downloads/ElementalBreakout -B /Users/maxcui/Downloads/ElementalBreakout/build
cmake --build /Users/maxcui/Downloads/ElementalBreakout/build
/Users/maxcui/Downloads/ElementalBreakout/build/elemental_pong
```

Pass `-DCMAKE_BUILD_TYPE=Release` if you prefer an optimized build.

### Windows (Visual Studio)

```powershell
cmake -S C:\path\to\ElementalBreakout -B C:\path\to\ElementalBreakout\build -G "Visual Studio 17 2022"
cmake --build C:\path\to\ElementalBreakout\build --config Release
.\build\Release\elemental_pong.exe
```

Ensure that raylib is installed for the same architecture and compiler toolchain you use with Visual Studio.

## Troubleshooting

- **CMake cannot find raylib**: confirm that `raylibConfig.cmake` is on CMake's search path. On macOS with Homebrew, add `-DCMAKE_PREFIX_PATH="$(brew --prefix raylib)"`.
- **No audio**: the game falls back silently if the audio device fails to initialize. Verify system audio permissions and that the `sounds/` folder is present next to the executable.
- **Blank window**: raylib requires a GPU supporting OpenGL 3.3. Update graphics drivers if rendering fails.



