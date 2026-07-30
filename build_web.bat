@echo off
setlocal
call "%~dp0Moxiang2D\build_web.bat"
set "BUILD_EXIT_CODE=%ERRORLEVEL%"
exit /b %BUILD_EXIT_CODE%
