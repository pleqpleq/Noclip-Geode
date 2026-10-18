# Noclip - timing assist (Geode C++ source)

Developer: Supleq | Project version: v0.1.0 | Platform: Windows

**Important:** This is SOURCE CODE, not a compiled `.geode` file. The frame-window
logic and C++ syntax have been checked locally, but the complete mod has NOT
been compiled against the real Geode SDK or tested inside Geometry Dash.

## What the mod is intended to do

Noclip listens for Geometry Dash jump-button DOWN and UP events. It can delay a
press or a release to an already-learned target position, by up to a configurable
number of game simulation frames. The window defaults to **5**. Inputs farther
away from a target are sent normally (unless optional blind delay is enabled).

Example, assuming a target was previously recorded at simulation frame 100:

| When you click | Window | Intended behavior |
| --- | ---: | --- |
| Frame 97 (3 early) | 5 | Buffer until near the recorded frame 100 |
| Frame 90 (10 early) | 5 | Send the click immediately |
| Frame 97 (3 early) | 2 | Send the click immediately |
| Frame 98 (2 early) | 2 | Buffer until near the recorded frame 100 |

In reality the mod anchors to the player's **level X position** (not a fixed
absolute frame). That helps reference timing survive level restarts and start
positions, but may be inaccurate during reverse movement, teleports, speed
changes, platformer movement, or other complex gameplay.

Supported edges: jump press AND release. Uses the same jump control in cube,
wave, ship, UFO, ball, robot, spider, and swing. Support is **experimental**
for every mode, especially wave and releases. Two-player inputs have separate
queues. Platformer mode is OFF by default.

## Critical limitation - cannot infer arbitrary perfect frames

A player click 3 frames early does not inherently tell the mod which future
frame is correct. Determining that automatically on any arbitrary level would
require a predictive simulation of collisions and level state. THIS MOD DOES
NOT DO THAT. It only snaps to known reference timings.

It can learn a timing automatically after an input survives at least 12
simulation frames on an attempt, but surviving is **not proof** the timing was
perfect. Alternatively you can use the explicit 'Record reference timing'
setting to record a clean run. Initially, unfamiliar input positions pass
through unchanged by default.

Do not use this as proof of legitimate unassisted gameplay/record completion.

## In-game settings

| Setting | Default | Meaning |
| --- | --- | --- |
| Enable Noclip timing | ON | Master switch |
| Timing window (frames) | **5** | Change to 2, 3, 10, etc. (0-20) |
| Learn inputs that survive | ON | Learn useful reference positions after 12 survived simulation steps |
| Record reference timing (training) | OFF | Record EVERY input at its original position (disables delay during recording) |
| Delay unlearned inputs too | OFF | Blindly delay all unfamiliar inputs by the whole window. Can hurt timings |
| Enable in platformer levels | OFF | Experimental |
| Clear all timing references | OFF | Turn ON to delete learned data; turn OFF again before learning |

Saved reference positions are stored in Geode's normal per-mod saved values.
A reference is scoped to its level, player, press/release state, and gamemode.

### How to get an early wave press/release corrected

1. Play that section correctly at least once (or enable **Record reference
   timing**, demonstrate the input, then turn it OFF).
2. With the setting **Timing window = 5**, click or release 3 simulation
   frames before the reference position on another attempt.
3. Noclip should hold that edge until your player reaches the learned
   position. An input 10 frames early will bypass the buffer (if blind
   delay is OFF).
4. If you see bad snaps, clear timing references and record a cleaner run.

If you want to try an immediate delay before any reference exists, turn ON
**Delay unlearned inputs too**. That is a simple fixed delay, not intelligent
perfect-frame detection: it also delays clicks 10 frames early.

## Build and installation

Open `INSTALL_WINDOWS.md` for all steps. For an already configured Geode SDK:

```powershell
cd "PATH_TO_EXTRACTED_SOURCE_FOLDER"
geode build --ninja
```

You can also double-click `BUILD_WINDOWS.bat` after Geode CLI, its SDK,
Windows compiler dependencies, and a Geode game profile are installed.

Successful builds usually create a file named `supleq.noclip.geode` in a
build subdirectory, or install it automatically if your Geode CLI profile is
configured. You can then use Geode's **Install From File** UI.

## Current compatibility warning - October 18, 2026

This project targets Geode v5.11.2 and Geometry Dash 2.2081 (Windows).
Geometry Dash 2.209 was released October 15, 2026, and the official Geode team
said October 18 that this game version was NOT YET supported. Do not force
this build onto unsupported GD versions. Wait for official compatibility,
then update the Geode SDK/manifest and rebuild if necessary.

Official references:
- https://github.com/geode-sdk/geode/issues/2244
- https://github.com/geode-sdk/geode/releases
- https://docs.geode-sdk.org/getting-started/create-mod/
- https://docs.geode-sdk.org/getting-started/sdk/

## Troubleshooting

- **No Noclip in installed mods:** A ZIP of source code cannot be installed
  as a Geode mod. You must first build an actual `.geode` package.
- **Geode reports unsupported game version:** You may be on GD 2.209.
  Check the official Geode release's supported version before attempting.
- **No input correction:** No learned reference exists, your click is too far
  before it, or the window is 0. Keep auto-learn ON or record reference timing.
- **A click or release fires at a bad moment:** A previously learned anchor
  may be inaccurate; use clear references and re-record deliberately.
- **C++ build fails:** Verify the installed SDK is v5.11.2 and GD bindings
  target 2.2081. Share the last build-error lines and `mod.json` to debug.
- **Geode freezes/crashes:** Remove `supleq.noclip.geode` from the game's
  `geode/mods` folder and restart. This is experimental and untested in-game.
- **Click pack/macro conflicts:** Other mods can also inject/record inputs.
  Disable bots/click automation when teaching Noclip targets.

## Test status

- Native unit tests for window math, release parity, reverse direction,
  and maximum delay: PASS.
- C++ syntax check of the Geode-facing source using local API-shape stubs: PASS.
- Real Geode SDK Windows build: NOT RUN (SDK/toolchain not installed here).
- In-game Geometry Dash behavior: NOT TESTED.

This project is an experimental timing-assisted mod, not real invincibility
noclip and not an automatic solution to every frame-perfect obstacle.

## GitHub build and trajectory feature planning

**Want GitHub to compile the `.geode` file?** Read `GITHUB_SETUP.md`.
This project now contains `.github/workflows/build-windows.yml`, which asks
GitHub Actions to build the mod on a Windows runner after the files are pushed
to `main`. A successful build publishes a temporary `Noclip-Windows-Geode`
artifact on the run page. This workflow has NOT been run or validated against
the real Windows SDK yet; a failed build means no installable mod was made.

**Want correction based on physics trajectories, rather than pre-recorded
anchors?** Read `docs/TRAJECTORY_DESIGN.md` for the technical plan. This is
NOT implemented in the current mod; do not confuse the architectural plan
with a working trajectory prediction feature.
