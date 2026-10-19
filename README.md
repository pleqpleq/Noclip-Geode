# Noclip — Input Buffer Lab v0.6.0

This is a **from-scratch, experimental** Geode mod for GD 2.2081 on Windows.
It contains a real queued input path and an *unverified* wave collision
predictor. There is **no proof it saves attempts in the real game yet**.
A successful Github Actions build and portable simulation tests are not proof
of in-game correctness.

## What is implemented

- Single-player wave **presses and releases**; the Geode BGL hook buffers an
  event and replays it before a later `processCommands` physics step.
- A **force-exact-delay diagnostic** mode: guarantees queue scheduling in
  portable tests (e.g. presses/release scheduled exactly two counted ticks
  later). It is *not intended to make the level easier*; it exists to verify
  actual interception, delivery and tick timing inside GD first.
- **Predictive mode** (experimental): samples x speed from recent game steps;
  traces wave position and static collision geometry for each candidate
  delay 0–8 ticks, and picks one only if it predicts at least two extra ticks
  of survival. This is NOT actual GD simulation; spikes, moving geometry,
  dash orbs, portals and triggers can be mispredicted.
- Dual mode is always **raw passthrough**, including levels configured
  for two-player mode. Existing queued edges are flushed in order when
  entering unsupported play modes, rather than intentionally dropped.
- CBF (`syzzi.click_between_frames`) detected => raw passthrough for
  safety, since delayed input injection through GJBaseGameLayer can be
  swallowed by CBF's filter; disable CBF to test this mod.
- F6 cycles wave corrections (2,4,8,8,OFF). Switch the mod on in settings.
- Debug logging prints requested and applied input ticks and whether the
  actual player button state matches a dispatched event.

## Install / first test (do not test first on Grief)

1. Make sure **Click Between Frames is disabled**, close/reopen Geometry Dash.
2. Download the latest *successful* `Noclip-Windows-Geode` artifact from
   https://github.com/pleqpleq/Noclip-Geode/actions and replace the old
   `supleq.noclip.geode`. Restart GD.
3. Geode > Noclip: **Enable input correction** = ON;
   **Diagnostic: force exact delay** = ON; **Diagnostic delay amount** = 2;
   **Log every buffered input and prediction** = ON.
4. In an *easy single-player wave test level*, make one press and one release.
   Read `geode/logs` for `Noclip INPUT`, `Noclip DISPATCH`, their tick values
   and `actualHeld=true`. Don't begin with Grief or any dual level.
5. Only if the diagnostic mode really delays press and release in-game,
   switch diagnostic mode OFF and test predictive mode on simple static spikes.
   Send the logs from the first miss; further prediction development should
   depend on these actual measurements.

## Known limitations

- **Not** guaranteed to save frame-perfect clicks; model doesn't predict
  the entire future input sequence or use GD's full engine collision model.
- **Not** capable of reliably correcting clicks 0.2–0.5 seconds early. The
  video you showed demonstrates approximately 1–8 *physics ticks*, not
  0.2–0.5 seconds. At 240 TPS, 8 ticks = 33.3 ms.
- Only single-player wave is assisted. Cube/ship/etc and dual are raw input.
- If CBF is installed/enabled, this mod deliberately does nothing.
- Does not spoof legitimate leaderboard scores. Using assisted inputs may
  violate competitive rules: disclose any mod-assisted completions.

This code deliberately avoids the old `Noclip` v0.1–0.5's global queues,
conflicting direct `PlayerObject` injection, and blind delays.