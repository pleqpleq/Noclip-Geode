# Noclip v0.5.0 — experimental 200ms early-wave buffer

**This revision fixes a timing-window mismatch, not proven frame-perfect
collision prediction.** At GD's nominal 240 physics steps/sec, a press 0.2
seconds early arrives 48 steps before the desired turn. Older Noclip builds
allowed only 5 steps (~21ms) and examined only 20 steps ahead.

## Settings
- **Wave early input buffer (milliseconds)**: 200 ms by default, adjustable
  0–300 ms. This is separate from the legacy 5-physics-frame setting for
  cube/learned references.
- Hidden wave trajectory correction: enabled
- Wave lookahead: 76 steps default, automatically increased when needed
- Log wave decisions: optional diagnostics

## Expected wave behavior
On a wave press/release the mod predicts local collision paths for now and
delays up to the configured wave buffer. It buffers **only if** the immediate
trajectory predicts a collision and delaying the input predicts survival
through the next obstacle. A click far from a relevant hazard, or a section
the approximation cannot predict, **passes through normally**. It does not
blindly delay every early click.

The model uses approximate current wave speed and static collision geometry.
It does not include GD's full state/physics and cannot promise frame-perfect
survival in the user's specific level.

## Install and diagnose
https://github.com/pleqpleq/Noclip-Geode/actions — use latest successful
Windows Geode artifact. Targets GD 2.2081.
If a predicted early input still registers normally, enable Log wave
decisions and share Geode log lines starting with `Noclip`.
