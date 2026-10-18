# Install Noclip (Windows / Steam Geometry Dash)

## First: check your version

**As of October 18, 2026, the official Geode team says GD 2.209 is not
supported yet.** If your Steam Geometry Dash is 2.209, stop here until
Geode's official compatibility release is available. This source currently
targets GD **2.2081** with Geode **5.11.2**. The latest game version and the
Geode SDK's supported game version are not necessarily the same.

Official support tracker: https://github.com/geode-sdk/geode/issues/2244

## A. Install developer prerequisites (once)

1. Install the *official* Geode loader from https://geode-sdk.org/ (only for
   a supported Geometry Dash version).
2. Install Git for Windows: https://git-scm.com/downloads/win
3. Install CMake 3.29 or newer: https://cmake.org/download/ (choose **Add
   CMake to system PATH** if asked).
4. Install a supported C++ toolchain. A good option is Visual Studio 2022
   Build Tools with C++ Desktop Development tools; alternatively follow
   Geode's current Clang/LLVM and Ninja instructions:
   https://docs.geode-sdk.org/getting-started/cpp-stuff/
5. Install the official Geode CLI. One supported command (PowerShell) is:

```powershell
winget install GeodeSDK.GeodeCLI
```

If you installed Git/CMake/Geode CLI just now, close and reopen PowerShell
so PATH updates are recognized.

## B. Install/configure Geode SDK (once)

Open PowerShell and run:

```powershell
geode sdk install
geode sdk install-binaries
geode config setup
```

Complete the profile prompts for your official Geometry Dash installation.
Restart PowerShell after the SDK install so the `GEODE_SDK` environment
variable is available. Verify it:

```powershell
$env:GEODE_SDK
```

If this is empty, Geode's SDK installation or environment configuration did
not complete. Follow https://docs.geode-sdk.org/getting-started/sdk/

## C. Build this source into a real .geode mod

1. Extract `Noclip-Geode-Source.zip` to an ordinary directory, for example
   `C:\Users\YOU\Desktop\Noclip-Geode-Source`.
2. Open this directory in File Explorer and double-click `BUILD_WINDOWS.bat`.
3. If the batch script fails, use PowerShell in the source directory:

```powershell
geode build --ninja
```

Or use CMake directly if the CLI build is unavailable:

```powershell
cmake -B build
cmake --build build --config RelWithDebInfo
```

Geode's official instructions:
https://docs.geode-sdk.org/getting-started/create-mod/

When the build succeeds, look for `supleq.noclip.geode` in the generated build
folder. With a configured Geode CLI profile, it may be copied into your
Geometry Dash mods folder automatically.

**Do not** rename the source `.zip` to `.geode`: the `.geode` must contain a
compiled Windows game mod binary and proper package metadata.

## D. Install the compiled mod in Geometry Dash

1. Launch **Geometry Dash** with the **Geode loader** installed and running.
2. In the Geode menu, open **Installed Mods** / **Mods**, then find the
   mod management/settings menu (often a small gear icon).
3. Select **Install From File** and choose `supleq.noclip.geode`.
4. Restart Geometry Dash if Geode requests it.
5. Open Geode > Installed Mods > **Noclip** > **Settings**.
6. Confirm `Timing window (frames)` is set to **5** (or change it to 2).

Alternative: in Steam, right-click Geometry Dash, select Manage > Browse
Local Files, open `geode\mods` inside the Geometry Dash folder and copy the
compiled `.geode` there while the game is closed. Then relaunch the game.

## E. Get the timing behavior you requested

- Keep **Enable Noclip timing** ON and **Timing window** at 5.
- Play a section once successfully. Auto-learn records the timing when the
  input survives at least 12 simulation steps.
- On a subsequent attempt, an input about 3 steps ahead of that same
  reference can be buffered to match its level position; one around 10
  steps early will bypass the selective buffer.
- The same applies to wave press/release events, though accuracy depends on
  the reference and player's path matching.
- For deliberately verified timings, turn ON **Record reference timing**,
  perform a clean reference section, then turn it OFF before using Noclip.

**Without a previously learned/recorded reference, the mod cannot know the
perfect frame for an arbitrary obstacle.** You can enable blind delay for an
immediate fixed delay, but then *every* unfamiliar press/release is delayed,
even one 10 frames early.

## F. Uninstall safely

Disable the mod in Geode or, with the game closed, remove
`Geometry Dash\geode\mods\supleq.noclip.geode` and restart.

If anything crashes, first remove the experimental mod before changing
other mods or game files. Do not use unofficial patchers to force an
unsupported Geode/GD combination.
