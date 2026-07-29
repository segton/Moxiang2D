@echo off
setlocal

set EMSDK_QUIET=1

set "EMSDK_DIR=D:\Dev\emsdk"
set "PROJECT_DIR=D:\Dev\Moxiang2D\Moxiang2D"
set "OUTPUT_DIR=D:\Dev\Moxiang2D\build-web\output"
set "RAYLIB_INCLUDE=D:\Dev\Libraries\raylib-src\raylib\src"
set "RAYLIB_WEB_LIB=D:\Dev\Moxiang2D\build-web\lib\libraylib_web.a"

echo.
echo ==========================================
echo Moxiang2D Web Build
echo ==========================================
echo.

if not exist "%EMSDK_DIR%\emsdk_env.bat" (
    echo ERROR: Emscripten environment file was not found:
    echo %EMSDK_DIR%\emsdk_env.bat
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

echo Emscripten:
em++ --version
echo.

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
    pause
    exit /b 1
)

if not exist "%RAYLIB_WEB_LIB%" (
    echo ERROR: Missing raylib web library:
    echo %RAYLIB_WEB_LIB%
    pause
    exit /b 1
)

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

del /q "%OUTPUT_DIR%\index.html" 2>nul
del /q "%OUTPUT_DIR%\index.js" 2>nul
del /q "%OUTPUT_DIR%\index.wasm" 2>nul
del /q "%OUTPUT_DIR%\index.data" 2>nul

cd /d "%PROJECT_DIR%"

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

if errorlevel 1 (
    echo.
    echo ==========================================
    echo WEB BUILD FAILED
    echo ==========================================
    echo.
    pause
    exit /b 1
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
echo To run the game:
echo cd /d "%OUTPUT_DIR%"
echo emrun index.html
echo.
pause