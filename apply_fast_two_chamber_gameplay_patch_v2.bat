@echo off
setlocal

set "PROJECT_DIR=D:\Dev\Moxiang2D\Moxiang2D"

echo ==========================================
echo Fast Two-Chamber Gameplay Patch V2
echo ==========================================
echo Project: %PROJECT_DIR%
echo.

cd /d "%PROJECT_DIR%"
if errorlevel 1 (
    echo ERROR: Could not open project folder.
    pause
    exit /b 1
)

python tools\apply_fast_two_chamber_gameplay_patch_v2.py
if errorlevel 1 (
    echo.
    echo ERROR: Gameplay patch V2 failed.
    pause
    exit /b 1
)

echo.
echo Gameplay patch V2 completed successfully.
pause
