@echo off
setlocal

cd /d "%~dp0Moxiang2D"

if not exist "tools\rebuild_level01_two_chambers.py" (
    echo ERROR: tools\rebuild_level01_two_chambers.py was not found.
    echo Copy the Python script into Moxiang2D\tools first.
    pause
    exit /b 1
)

python tools\rebuild_level01_two_chambers.py
if errorlevel 1 (
    echo.
    echo ERROR: Two-chamber level generation failed.
    pause
    exit /b 1
)

echo.
echo Two-chamber level generated successfully.
pause
