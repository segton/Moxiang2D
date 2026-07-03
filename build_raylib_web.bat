@echo off
setlocal

call D:\Dev\emsdk\emsdk_env.bat

set RAYLIB_SRC=external\raylib\src
set OUT_DIR=build-web\raylib
set LIB_DIR=build-web\lib

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
if not exist "%LIB_DIR%" mkdir "%LIB_DIR%"

if exist "%LIB_DIR%\libraylib_web.a" del "%LIB_DIR%\libraylib_web.a"

set COMMON_FLAGS=-std=gnu99 -Os -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -DSUPPORT_FILEFORMAT_PNG -DSUPPORT_FILEFORMAT_BMP -DSUPPORT_FILEFORMAT_TGA -DSUPPORT_FILEFORMAT_JPG

echo Building raylib for HTML5...

emcc -c "%RAYLIB_SRC%\rcore.c" -o "%OUT_DIR%\rcore.o" ^
    %COMMON_FLAGS% ^
    -I"%RAYLIB_SRC%" ^
    -I"%RAYLIB_SRC%\external\glfw\include"
if errorlevel 1 exit /b 1

emcc -c "%RAYLIB_SRC%\rshapes.c" -o "%OUT_DIR%\rshapes.o" ^
    %COMMON_FLAGS% ^
    -I"%RAYLIB_SRC%"
if errorlevel 1 exit /b 1

emcc -c "%RAYLIB_SRC%\rtextures.c" -o "%OUT_DIR%\rtextures.o" ^
    -std=gnu99 ^
    -Os ^
    -DPLATFORM_WEB ^
    -DGRAPHICS_API_OPENGL_ES2 ^
    -DSUPPORT_FILEFORMAT_JPG=1 ^
    -DSUPPORT_FILEFORMAT_GIF=1 ^
    -I"%RAYLIB_SRC%" ^
    -I"%RAYLIB_SRC%\external\glfw\include"

emcc -c "%RAYLIB_SRC%\rtext.c" -o "%OUT_DIR%\rtext.o" ^
    %COMMON_FLAGS% ^
    -I"%RAYLIB_SRC%"
if errorlevel 1 exit /b 1

emar rcs "%LIB_DIR%\libraylib_web.a" ^
    "%OUT_DIR%\rcore.o" ^
    "%OUT_DIR%\rshapes.o" ^
    "%OUT_DIR%\rtextures.o" ^
    "%OUT_DIR%\rtext.o"
if errorlevel 1 exit /b 1

echo.
echo Done: %LIB_DIR%\libraylib_web.a

endlocal