@echo off
setlocal
call "%~dp0build-web\build_raylib_web.bat"
set "BUILD_EXIT_CODE=%ERRORLEVEL%"
exit /b %BUILD_EXIT_CODE%
