@echo off
setlocal
cd /d "%~dp0"

set "PATH=E:\gcc-14.1.0\bin;C:\ProgramData\chocolatey\bin;%PATH%"

if not exist "build\debug\Makefile" (
    echo [Last Order] Configuring CMake preset: debug...
    cmake --preset debug
    if errorlevel 1 exit /b 1
)

echo [Last Order] Building project...
cmake --build --preset debug -j 8
if errorlevel 1 exit /b 1

echo [Last Order] Verifying release package in release/...
if not exist "release" mkdir "release"
xcopy "assets" "release\assets" /e /i /y /q >nul
if exist "build\debug\Last Order.exe" (
    copy /y "build\debug\Last Order.exe" "release\Last Order.exe" >nul
)

if exist "release\last_order.exe" (
    del /q "release\last_order.exe" >nul 2>&1
)

echo [Last Order] Standalone release files deployed to release\

if /i "%~1"=="--no-run" (
    echo [Last Order] Build succeeded.
    exit /b 0
)

echo [Last Order] Launching release\Last Order.exe...
cd release
"Last Order.exe" %*
