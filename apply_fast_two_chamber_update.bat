@echo off
setlocal

set "REPO_ROOT=%~dp0"
set "PROJECT_ROOT=%REPO_ROOT%Moxiang2D"

if not exist "%PROJECT_ROOT%\Game.cpp" (
    echo ERROR: Could not find "%PROJECT_ROOT%\Game.cpp"
    echo Place this BAT file in D:\Dev\Moxiang2D\
    pause
    exit /b 1
)

echo ==========================================
echo Fast Two-Chamber Update
echo ==========================================
echo Project: %PROJECT_ROOT%
echo.

python "%PROJECT_ROOT%\tools\rebuild_level01_two_chambers_updated.py" --root "%PROJECT_ROOT%"
if errorlevel 1 (
    echo.
    echo ERROR: Level generation failed.
    pause
    exit /b 1
)

python "%PROJECT_ROOT%\tools\apply_fast_two_chamber_gameplay_patch.py" --root "%PROJECT_ROOT%"
if errorlevel 1 (
    echo.
    echo ERROR: Gameplay patch failed.
    pause
    exit /b 1
)

echo.
echo Update completed successfully.
echo Rebuild the Visual Studio project before testing.
pause
