# Noclip v0.3.0 - Wave frame-window timing assist

**Geode Windows mod, experimental.** The goal is to turn an early wave press or
release into the safest locally predicted press/release, within a user-adjustable
frame window (default **5 game simulation frames**). A trajectory is calculated
silently; nothing is drawn on screen.

## v0.3 changes

- Instead of requiring one continuous wave direction to survive the full
  20-frame lookahead, the prediction compares candidates through the
  **current obstacle**. A future turn doesn't falsely invalidate today's click.
- If an immediate input is predicted to collide soon, compare delays of
  1 through 5 frames and choose the predicted safe timing with the most
  clearance around that obstacle.
- The press/release sequence retains the original tap duration after buffering
  (for example, press+1-frame release remains a 1-frame hold).
- Portals/rings **far from the actual simulated path** no longer block input
  correction; relevant nearby portal intersections still disable prediction.
- Optional **Log wave decisions** Geode setting reports if a click was buffered
  and why it wasn't. **No onscreen trajectory overlay.**

It is **not true frame-perfect prediction**: the engine approximates static
rectangular hitboxes and 45-degree wave movement, not Geometry Dash's complete
collision/trigger physics. Complex rotated hitboxes, moving obstacles, portals,
mode transitions and other mods can cause errors. It cannot repair clicks after
a fatal collision or beyond the chosen delay window.

## Settings

- Enable Noclip timing: ON
- Timing window (frames): **5** by default, adjustable 0–20
- Hidden wave trajectory correction: ON
- Wave lookahead (frames): 20 by default
- Log wave decisions to Geode log: OFF (turn on for troubleshooting)
- Learn inputs that survive: OFF
- Delay unlearned inputs: OFF (wave skips blind delay regardless)

Other gamemodes still use optional learned reference positions, not automatic
trajectory correction.

## Install

This repository contains source. Open the [Actions](../../actions) page and
select the latest **successful** 'Build Noclip for Windows' run. Download the
'Noclip-Windows-Geode' artifact, extract the .geode package, and install
through Geode. The project targets **GD 2.2081** on Windows.
Install only on compatible GD/Geode versions.

If the wave does not delay, enable **Log wave decisions**, test a short local
section, then inspect the Geode logs for messages beginning 'Noclip wave'.
This distinguishes an unsupported state, early input, an already-safe input,
and a failed safe-frame search.

Current verification: portable tests run on Ubuntu in CI; Windows compilation
can be checked by GitHub Actions. Gameplay behavior has NOT been checked
inside GD and is not guaranteed accurate.

Influences: [Auto Dodge](https://github.com/Borixya/auto-dodge);
[Geode bindings](https://github.com/geode-sdk/bindings). No third-party
simulation source is copied into this project.
