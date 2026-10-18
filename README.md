# Noclip v0.4.1 (experimental)

A Windows Geode mod that attempts to buffer an early wave press or release
until a nearby locally predicted safe frame (5 physics frames by default).
The wave trajectory is hidden. It is NOT actual noclip and cannot guarantee
perfect frames on unknown levels.

## Changes since v0.3.0

- Uses the live GD wave hitbox from PlayerObject::getObjectRect(.3f,.3f)
  rather than hardcoded player hitbox dimensions.
- Uses triangle collision polygons for slope objects, rotated four-corner
  polygons for oriented objects, and circles for circular obstacles instead of
  incorrectly treating all of them as filled rectangular boxes.
- Compares buffered press/release timings against the immediate obstacle
  rather than requiring a 20-frame straight hold to be safe.
- Direct buffered wave dispatch is now enabled by default for CBF compatibility. CBF can ignore delayed game-layer input calls outside its own input window; the wave-only direct PlayerObject press/release bypasses that filter. Turn it OFF if your setup conflicts with another mod.
- Log wave decisions now also reports DISPATCHED press/release timestamps and dispatch path;
  this reveals whether a click was actually delayed or simply passed through.
- Still avoids blind delaying unknown wave inputs; press/release ordering
  is retained after a buffered input.

## Install
Visit GitHub Actions for the latest green Build Noclip for Windows run:
https://github.com/pleqpleq/Noclip-Geode/actions

Download artifact Noclip-Windows-Geode, extract the .geode package, install
through Geode, and restart Geometry Dash. Targets Windows GD 2.2081 only.

## Diagnose one missed click

In Noclip settings enable Log wave decisions (debug). Reproduce the missed
wave turn once. Open Geode's log and send the lines beginning Noclip wave.
Examples:
- "buffered, delay=3" then "DISPATCHED ... delay 3": queued and delivered.
- "no-safe-frame": model found no safe trajectory within window.
- "already-safe": model incorrectly considered current input harmless.
- "unavailable": game state/speed not supported.

**Not yet tested inside the running game.** This is a read-only, approximate
trajectory model using current wave speed and obstacle collision geometry;
moving obstacles, triggers, collision quirks, and game/mod hook interactions
can still invalidate predictions. Never present assisted completions as
unassisted.
