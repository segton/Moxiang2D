@echo off
setlocal

set "EMSDK_QUIET=1"

for %%I in ("%~dp0..") do set "REPO_DIR=%%~fI"

if not defined EMSDK_DIR (
    if defined EMSDK (
        set "EMSDK_DIR=%EMSDK%"
    ) else (
        set "EMSDK_DIR=D:\Dev\emsdk"
    )
)

if not defined RAYLIB_SRC (
    if exist "%REPO_DIR%\external\raylib\src\rcore.c" (
        set "RAYLIB_SRC=%REPO_DIR%\external\raylib\src"
    ) else (
        set "RAYLIB_SRC=D:\Dev\Libraries\raylib-src\raylib\src"
    )
)

set "OBJ_DIR=%REPO_DIR%\build-web\raylib-obj"
set "LIB_DIR=%REPO_DIR%\build-web\lib"
set "OUT_LIB=%LIB_DIR%\libraylib_web.a"

echo.
echo ==========================================
echo Building complete raylib Web library
echo ==========================================
echo Source: %RAYLIB_SRC%
echo Output: %OUT_LIB%
echo.

if not exist "%EMSDK_DIR%\emsdk_env.bat" (
    echo ERROR: Could not find:
    echo %EMSDK_DIR%\emsdk_env.bat
    echo Set EMSDK_DIR or EMSDK before running this script.
    pause
    exit /b 1
)

call "%EMSDK_DIR%\emsdk_env.bat"

if errorlevel 1 (
    echo ERROR: Failed to initialize Emscripten.
    pause
    exit /b 1
)

where emcc >nul 2>nul
if errorlevel 1 (
    echo ERROR: emcc was not found.
    pause
    exit /b 1
)

where emar >nul 2>nul
if errorlevel 1 (
    echo ERROR: emar was not found.
    pause
    exit /b 1
)

if not exist "%RAYLIB_SRC%\rcore.c" (
    echo ERROR: Missing rcore.c
    echo Expected: %RAYLIB_SRC%\rcore.c
    echo Set RAYLIB_SRC to the raylib src folder.
    pause
    exit /b 1
)

if not exist "%RAYLIB_SRC%\rmodels.c" (
    echo ERROR: Missing rmodels.c
    echo Expected: %RAYLIB_SRC%\rmodels.c
    pause
    exit /b 1
)

if exist "%OBJ_DIR%" rmdir /s /q "%OBJ_DIR%"
mkdir "%OBJ_DIR%"
if not exist "%LIB_DIR%" mkdir "%LIB_DIR%"
if exist "%OUT_LIB%" del /q "%OUT_LIB%"

set COMMON_FLAGS=-std=gnu99 -O2 -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -I"%RAYLIB_SRC%" -I"%RAYLIB_SRC%\external\glfw\include"

echo [1/6] Compiling rcore.c...
emcc -c "%RAYLIB_SRC%\rcore.c" %COMMON_FLAGS% -o "%OBJ_DIR%\rcore.o"
if errorlevel 1 goto build_failed

echo [2/6] Compiling rshapes.c...
emcc -c "%RAYLIB_SRC%\rshapes.c" %COMMON_FLAGS% -o "%OBJ_DIR%\rshapes.o"
if errorlevel 1 goto build_failed

echo [3/6] Compiling rtextures.c...
emcc -c "%RAYLIB_SRC%\rtextures.c" %COMMON_FLAGS% -o "%OBJ_DIR%\rtextures.o"
if errorlevel 1 goto build_failed

echo [4/6] Compiling rtext.c...
emcc -c "%RAYLIB_SRC%\rtext.c" %COMMON_FLAGS% -o "%OBJ_DIR%\rtext.o"
if errorlevel 1 goto build_failed

echo [5/6] Compiling rmodels.c...
emcc -c "%RAYLIB_SRC%\rmodels.c" %COMMON_FLAGS% -o "%OBJ_DIR%\rmodels.o"
if errorlevel 1 goto build_failed

echo [6/6] Compiling raudio.c...
emcc -c "%RAYLIB_SRC%\raudio.c" %COMMON_FLAGS% -o "%OBJ_DIR%\raudio.o"
if errorlevel 1 goto build_failed

echo.
echo Creating libraylib_web.a...
emar rcs "%OUT_LIB%" ^
    "%OBJ_DIR%\rcore.o" ^
    "%OBJ_DIR%\rshapes.o" ^
    "%OBJ_DIR%\rtextures.o" ^
    "%OBJ_DIR%\rtext.o" ^
    "%OBJ_DIR%\rmodels.o" ^
    "%OBJ_DIR%\raudio.o"

if errorlevel 1 goto build_failed
if not exist "%OUT_LIB%" goto build_failed

emar t "%OUT_LIB%" | findstr /i "rmodels.o" >nul
if errorlevel 1 (
    echo ERROR: rmodels.o is missing from the library.
    goto build_failed
)

echo.
echo ==========================================
echo RAYLIB WEB BUILD COMPLETED
echo ==========================================
echo %OUT_LIB%
echo.
pause
exit /b 0

:build_failed
echo.
echo ==========================================
echo RAYLIB WEB BUILD FAILED
echo ==========================================
echo.
pause
exit /b 1
