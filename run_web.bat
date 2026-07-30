@echo off
setlocal

for %%I in ("%~dp0.") do set "REPO_DIR=%%~fI"
set "OUTPUT_DIR=%REPO_DIR%\build-web\output"

if not exist "%OUTPUT_DIR%\index.html" (
    echo ERROR: Web build not found:
    echo %OUTPUT_DIR%\index.html
    echo Run build_web.bat first.
    pause
    exit /b 1
)

set "PYTHON_CMD="
where py >nul 2>nul
if not errorlevel 1 set "PYTHON_CMD=py -3"

if not defined PYTHON_CMD (
    where python >nul 2>nul
    if not errorlevel 1 set "PYTHON_CMD=python"
)

if not defined PYTHON_CMD (
    echo ERROR: Python 3 was not found.
    pause
    exit /b 1
)

pushd "%OUTPUT_DIR%"
echo Serving http://localhost:8080/index.html
%PYTHON_CMD% -m http.server 8080
set "SERVER_EXIT_CODE=%ERRORLEVEL%"
popd
exit /b %SERVER_EXIT_CODE%
