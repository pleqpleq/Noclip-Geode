@echo off
setlocal
cd /d "%~dp0"
echo ======================================================
echo  Noclip - Build a Windows .geode mod from source
echo ======================================================
echo.
where geode >nul 2>nul
if errorlevel 1 (
    echo ERROR: Geode CLI is not on PATH.
    echo Open INSTALL_WINDOWS.md and complete steps A and B.
    pause
    exit /b 1
)
if "%GEODE_SDK%"=="" (
    echo ERROR: GEODE_SDK environment variable is missing.
    echo Run: geode sdk install
    echo Then close and reopen the terminal.
    pause
    exit /b 1
)
echo Building with: geode build --ninja
geode build --ninja
if errorlevel 1 (
    echo.
    echo BUILD FAILED. See messages above for missing tools/compile errors.
    pause
    exit /b 1
)
echo.
echo BUILD FINISHED. If a Geode profile was configured, it may be installed.
echo Looking for package files:
for /r %%F in (supleq.noclip.geode) do echo %%~fF
echo.
echo See INSTALL_WINDOWS.md for Geode Install From File steps.
pause
endlocal
