# Supleq Input Buffer - standalone console research build

**This is not an exact recreation of the video cheat.** It contains a real,
Windows-native mechanism for delaying input edges at the game's own physics
callback, but **it does not yet detect the safe frame using GD collisions**.
GRID mode aligns events to a repeating tick grid. EXACT mode applies an exact
2-tick delay, useful for verifying input interception. Neither mode can claim
that a corrected input would have prevented a death. Do not call the logs
"SAVED" unless a counterfactual simulation proves it.

## No Geode dependency

Run `SupleqInputBuffer.exe` next to `SupleqBufferHook.dll`. The launcher finds
GeometryDash.exe and loads its own native helper DLL into that process. It
does not require Geode, install a Geode mod, or write files into GD's folders.
**Windows x64, GD 2.2081 only.** It uses game-specific memory offsets and can
crash an incompatible game version. Do not use it on GD 2.209 or 2.2074.

The launcher starts **disarmed**: your inputs remain raw until F7 is pressed.
The native helper also stops interception if the launcher exits or loses its
heartbeat. Do not run the launcher elevated unless your own GD process requires
it. Windows Defender may warn about DLL injection; examine this source first.

## Controls

- `F6` cycles **2 ticks, 4 ticks, 8 ticks, 8 wave / 16 all, OFF**
- `F7` arms/disarms the input buffer (defaults OFF)
- `F8` switches **EXACT** (2-tick experimental test) / **GRID** alignment
- `F9` enables/disables verbose input logs
- `End` exits the console and disarms correction

A `QUEUED PRESS/RELEASE tick A -> B` line confirms a click was intercepted.
`DELIVERED +N TICKS` means a queued event was dispatched to the actual
`GJBaseGameLayer::handleButton` function **before** GD's next physics tick.
It does not mean the player avoided a death.

## Safeguards and limitations

- Delays only P1 wave jump inputs in non-dual, non-platformer gameplay;
  all other inputs pass through normally.
- Applies events in order, keeps press/release at least one physics step apart.
- Queued inputs flush if the game enters an unsupported mode or disarms.
- Not verified against a live copy of GD. This is **compilation- and
  unit-test-verified software, not game-tested**.
- The video likely uses collision analysis to find useful input timings.
  A fixed/grid delay cannot reproduce that behavior. This is the instrumentation
  and input-delivery foundation for a true predictor.
- Running another input mod (especially CBF) concurrently may conflict with
  native input hooks; test with other input mods disabled.
- GD leaderboard completions with input modification are assisted.

## Build

On Windows with Visual Studio 2022:

```powershell
cmake -S . -B out -A x64
cmake --build out --config Release
```

The build uses [MinHook](https://github.com/TsudaKageyu/minhook)
for Windows x64 detouring. GitHub Actions packages the executable and helper.
The Windows build downloads MinHook during configuration; no runtime MinHook
installation is needed.

### Documented GD 2.2081 addresses

Native helper hooks `GJBaseGameLayer::handleButton` RVA `0x2338e0` and
`GJBaseGameLayer::processCommands` RVA `0x239c60`, with mode/player offsets
based on publicly documented GD 2.2081 binary analysis. These are NOT stable
across game updates or patches.