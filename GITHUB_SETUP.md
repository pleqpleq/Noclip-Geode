# Put Noclip on GitHub and get an automatic Windows build

## 1. Create a new repository

1. Go to https://github.com/new and sign into your GitHub account.
2. Repository name: `Noclip-Geode` (or any name you like).
3. Choose Public or Private.
4. Do NOT pre-create README, .gitignore, or a license; they are already in the project.
5. Click Create repository.

## 2. Upload project files

1. Extract `Noclip-Geode-GitHub-Ready.zip` to your PC.
2. Open the extracted `Noclip-Geode-Source` folder. You should see
   `mod.json`, `CMakeLists.txt`, `README.md`, `src/`, `.github/`, etc.
3. On your GitHub repository page, choose "uploading an existing file" or
   `Add file > Upload files`.
4. Drag the CONTENTS of `Noclip-Geode-Source` into the upload page (not
   the outer ZIP file, and not another containing folder).
5. Make sure `.github/workflows/build-windows.yml` is included! If the web UI
   skips this folder, upload it separately using GitHub's file editor,
   or use GitHub Desktop / Git and push the full folder instead.
6. Click "Commit changes".

IMPORTANT: `mod.json` and `CMakeLists.txt` must be at the ROOT of the GitHub
repository, not inside a second `Noclip-Geode-Source/` folder. If the build
workflow is missing, GitHub will not automatically try to compile it.

## 3. Build automatically on GitHub

1. Open the repository's `Actions` tab.
2. Allow workflows if GitHub asks you to enable Actions.
3. Open the `Build Noclip for Windows` workflow.
4. If no build started automatically, click `Run workflow` and select main.
5. Open the latest run. A green check means the compiler succeeded.
6. Scroll down to `Artifacts`. Download `Noclip-Windows-Geode`.
7. Extract the downloaded artifact ZIP. Look for `supleq.noclip.geode`.

If the workflow has a RED X, no working `.geode` was generated. Open the
failed job and inspect the error log. This source has NOT yet been compiled
against a live Windows SDK or tested in Geometry Dash, so a fix may be
necessary. Do not rename a source .zip to .geode.

## 4. Install in the game

With Geode installed on a SUPPORTED Geometry Dash version, either install
`supleq.noclip.geode` using Geode's `Install from File` UI or copy it to
`Geometry Dash/geode/mods/` while the game is closed.

As of October 18, 2026, Geometry Dash 2.209 is not officially supported
by Geode; this project's metadata targets Geometry Dash 2.2081. Check
Geode's current compatibility before using it.

Official references:
- https://docs.github.com/en/repositories/creating-and-managing-repositories/creating-a-new-repository
- https://docs.github.com/en/repositories/working-with-files/managing-files/adding-a-file-to-a-repository
- https://docs.github.com/en/actions/how-tos/manage-workflow-runs/download-workflow-artifacts
- https://github.com/geode-sdk/build-geode-mod

## 5. (Optional) Give others a public Releases download link

Once a Windows build has successfully produced `supleq.noclip.geode`, go to
your repository > `Releases` > `Draft a new release`, choose a version tag,
and attach the `.geode` file itself under release assets. Publish the release.
Visitors can then download the `.geode` directly from the Releases page,
without opening the workflow's temporary artifact ZIP.

Never label an untested binary as a verified, reliable mod. Make clear that
this is an experimental build and that physics-based trajectory correction
is not yet implemented.
