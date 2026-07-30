@echo off
setlocal

set "EMSDK_QUIET=1"

for %%I in ("%~dp0.") do set "PROJECT_DIR=%%~fI"
for %%I in ("%PROJECT_DIR%\..") do set "REPO_DIR=%%~fI"

if not defined EMSDK_DIR (
    if defined EMSDK (
        set "EMSDK_DIR=%EMSDK%"
    ) else (
        set "EMSDK_DIR=D:\Dev\emsdk"
    )
)

if not defined OUTPUT_DIR set "OUTPUT_DIR=%REPO_DIR%\build-web\output"
if not defined RAYLIB_WEB_LIB set "RAYLIB_WEB_LIB=%REPO_DIR%\build-web\lib\libraylib_web.a"

if not defined RAYLIB_INCLUDE (
    if exist "%REPO_DIR%\external\raylib\src\raylib.h" (
        set "RAYLIB_INCLUDE=%REPO_DIR%\external\raylib\src"
    ) else (
        set "RAYLIB_INCLUDE=D:\Dev\Libraries\raylib-src\raylib\src"
    )
)

echo.
echo ==========================================
echo Moxiang2D Web Build
echo ==========================================
echo Project: %PROJECT_DIR%
echo Output:  %OUTPUT_DIR%
echo.

if not exist "%EMSDK_DIR%\emsdk_env.bat" (
    echo ERROR: Emscripten environment file was not found:
    echo %EMSDK_DIR%\emsdk_env.bat
    echo Set EMSDK_DIR or EMSDK before running this script.
    pause
    exit /b 1
)

call "%EMSDK_DIR%\emsdk_env.bat"

if errorlevel 1 (
    echo.
    echo ERROR: Failed to initialise Emscripten.
    pause
    exit /b 1
)

where em++ >nul 2>nul

if errorlevel 1 (
    echo.
    echo ERROR: em++ was not found after running emsdk_env.bat.
    pause
    exit /b 1
)

if not exist "%PROJECT_DIR%\main.cpp" (
    echo ERROR: Missing main.cpp
    pause
    exit /b 1
)

if not exist "%PROJECT_DIR%\Game.cpp" (
    echo ERROR: Missing Game.cpp
    pause
    exit /b 1
)

if not exist "%PROJECT_DIR%\Game.h" (
    echo ERROR: Missing Game.h
    pause
    exit /b 1
)

if not exist "%PROJECT_DIR%\Assets" (
    echo ERROR: Missing Assets folder
    pause
    exit /b 1
)

if not exist "%PROJECT_DIR%\levels" (
    echo ERROR: Missing levels folder
    pause
    exit /b 1
)

if not exist "%RAYLIB_INCLUDE%\raylib.h" (
    echo ERROR: Missing raylib.h:
    echo %RAYLIB_INCLUDE%\raylib.h
    echo Set RAYLIB_INCLUDE to the raylib src folder.
    pause
    exit /b 1
)

if not exist "%RAYLIB_WEB_LIB%" (
    echo ERROR: Missing raylib web library:
    echo %RAYLIB_WEB_LIB%
    echo Run build_raylib_web.bat first or set RAYLIB_WEB_LIB.
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
    echo It is required to validate level and asset paths before the web build.
    pause
    exit /b 1
)

echo Emscripten:
em++ --version
echo.
echo Validating level data and exact asset paths...
%PYTHON_CMD% "%PROJECT_DIR%\tools\validate_level.py" "%PROJECT_DIR%\levels\level01.mox"

if errorlevel 1 (
    echo.
    echo ERROR: Content validation failed. Web compilation was cancelled.
    pause
    exit /b 1
)

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

del /q "%OUTPUT_DIR%\index.html" 2>nul
del /q "%OUTPUT_DIR%\index.js" 2>nul
del /q "%OUTPUT_DIR%\index.wasm" 2>nul
del /q "%OUTPUT_DIR%\index.data" 2>nul

pushd "%PROJECT_DIR%"

echo.
echo Compiling Moxiang2D Web...
echo.

em++ main.cpp Game.cpp "%RAYLIB_WEB_LIB%" ^
    -std=c++17 ^
    -O2 ^
    -Wall ^
    -DPLATFORM_WEB ^
    -DMOXIANG_USE_IMGUI=0 ^
    -I"%RAYLIB_INCLUDE%" ^
    -sUSE_GLFW=3 ^
    -sGL_ENABLE_GET_PROC_ADDRESS=1 ^
    -sASSERTIONS=1 ^
    -sFORCE_FILESYSTEM=1 ^
    -sALLOW_MEMORY_GROWTH=1 ^
    -sINITIAL_MEMORY=134217728 ^
    -sMAXIMUM_MEMORY=536870912 ^
    -sMIN_WEBGL_VERSION=1 ^
    -sMAX_WEBGL_VERSION=1 ^
    --preload-file "Assets@Assets" ^
    --preload-file "levels@levels" ^
    --emrun ^
    -o "%OUTPUT_DIR%\index.html"

set "BUILD_EXIT_CODE=%ERRORLEVEL%"
popd

if not "%BUILD_EXIT_CODE%"=="0" (
    echo.
    echo ==========================================
    echo WEB BUILD FAILED
    echo ==========================================
    echo.
    pause
    exit /b %BUILD_EXIT_CODE%
)

echo.
echo ==========================================
echo WEB BUILD COMPLETED
echo ==========================================
echo.
echo Output folder:
echo %OUTPUT_DIR%
echo.
echo Generated files:
dir "%OUTPUT_DIR%\index.*"
echo.
echo Run run_web.bat from the repository root to serve the build.
echo.
pause
exit /b 0
