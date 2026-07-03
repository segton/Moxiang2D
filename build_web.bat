@echo off
setlocal

call D:\Dev\emsdk\emsdk_env.bat

set APP_DIR=Moxiang2D
set RAYLIB_SRC=external\raylib\src
set OUT_DIR=build-web
set LIB_DIR=build-web\lib

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

em++ "%APP_DIR%\src\main.cpp" "%APP_DIR%\src\Game.cpp" ^
    -o "%OUT_DIR%\moxiang2d.html" ^
    -std=c++17 ^
    -Os ^
    -Wall -Wextra ^
    -DPLATFORM_WEB ^
    -I"%APP_DIR%\src" ^
    -I"%RAYLIB_SRC%" ^
    "%LIB_DIR%\libraylib_web.a" ^
    -sUSE_GLFW=3 ^
    -sWASM=1 ^
    -sALLOW_MEMORY_GROWTH=1 ^
    -sINITIAL_MEMORY=67108864 ^
    -sASSERTIONS=1 ^
    --shell-file "%RAYLIB_SRC%\shell.html" ^
    --preload-file "%APP_DIR%\Assets@Assets"

if errorlevel 1 exit /b 1

echo.
echo Done: %OUT_DIR%\moxiang2d.html

endlocal