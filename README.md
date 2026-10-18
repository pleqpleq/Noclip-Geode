# Noclip v0.4.2 — diagnostics build (NOT validated in-game)

This experimental Geode mod attempts to snap an early wave press or release
forward by up to 5 simulation frames using a limited prediction of static
collision geometry. Compilation and portable geometry tests do not prove it
works on Geometry Dash levels. **No guarantee of perfect inputs.**

## Why v0.4.2

Multiple successful builds were reported, but live gameplay still failed.
The preceding builds log only after their initial eligibility checks; if
your click is skipped before those checks, the log contains no explanation.
v0.4.2 adds logging **before every pass-through gate**.

## Diagnose it in one test

1. Install a Windows Geode artifact from the latest successful build in
   https://github.com/pleqpleq/Noclip-Geode/actions
   (this project currently targets GD 2.2081).
2. In Geode > Noclip Settings, turn on **Log wave decisions (debug)**.
3. Restart the level and reproduce *one* failed wave input.
4. Go to your Geometry Dash installation directory > `geode/logs`.
5. Send the latest log file or copy lines containing `Noclip`.

Interpretation:
- `Noclip v0.4.2 loaded` but no `Noclip INPUT` lines:
  the mod's game-layer hook never received your jump event.
- `play=false editor=true`: the mod bypasses editor playtesting.
- `eligible=false` or `wave=false`: the assistance was gated off.
- `scan hazards=0`: it didn't find nearby collision objects.
- `unavailable`, `already-safe`, `no-safe-frame`: it chose no delay.
- `buffered` and `DISPATCHED`: input was queued and sent.
- `heldNow` does not match `expectedHeld`: an input hook may have
  prevented the event from reaching the player.

TESTMODE text **alone** is not proof of editor playtesting: start positions
also display TESTMODE in normal level play.

Features remain experimental. The predictor still does not simulate the real
GD collision engine or moving triggers, and can't reliably correct arbitrary
frame-perfect gaps. The purpose of this build is evidence-based debugging,
not another unsupported accuracy claim.
