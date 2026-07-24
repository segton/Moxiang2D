@echo off
set EMSDK_QUIET=1

call D:\Dev\emsdk\emsdk_env.bat

cd /d D:\Dev\Moxiang2D\Moxiang2D

if not exist "..\build-web\output" mkdir "..\build-web\output"

echo.
echo Compiling Moxiang2D Web...
echo.

em++ main.cpp Game.cpp "D:/Dev/Moxiang2D/build-web/lib/libraylib_web.a" -std=c++17 -O2 -Wall -DPLATFORM_WEB -DMOXIANG_USE_IMGUI=0 -I"D:/Dev/Libraries/raylib-src/raylib/src" -sUSE_GLFW=3 -sGL_ENABLE_GET_PROC_ADDRESS=1 -sASSERTIONS=1 -sFORCE_FILESYSTEM=1 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 -sMAXIMUM_MEMORY=536870912 -sMIN_WEBGL_VERSION=1 -sMAX_WEBGL_VERSION=1 --preload-file "Assets@Assets" --preload-file "levels@levels" --emrun -o "../build-web/output/index.html"

if errorlevel 1 (
    echo.
    echo WEB BUILD FAILED
    pause
    exit /b 1
)

echo.
echo WEB BUILD COMPLETED
echo Output:
echo D:\Dev\Moxiang2D\build-web\output\index.html
echo.
pause