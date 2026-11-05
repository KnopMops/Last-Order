@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "PATH=E:\gcc-14.1.0\bin;C:\ProgramData\chocolatey\bin;%PATH%"

echo ========================================================
echo  KnopMops Engine - Visual Level and Map Editor
echo ========================================================

if not exist "build\release\Makefile" (
    echo [KnopMops Engine] Configuring CMake preset: release...
    cmake --preset release
    if errorlevel 1 (
        echo [ERROR] CMake configuration failed!
        exit /b 1
    )
)

echo [KnopMops Engine] Building project...
cmake --build --preset release -j 8
if errorlevel 1 (
    echo [ERROR] Build failed!
    exit /b 1
)

echo [KnopMops Engine] Build succeeded!

if not exist "bin" mkdir "bin"
if not exist "bin\assets" mkdir "bin\assets"
xcopy "assets" "bin\assets" /e /i /y /q >nul
if exist "build\release\KnopMops Engine.exe" (
    copy /y "build\release\KnopMops Engine.exe" "bin\KnopMops Engine.exe" >nul
)

if /i "%~1"=="--no-run" (
    echo [KnopMops Engine] Deploy completed.
    exit /b 0
)

echo [KnopMops Engine] Launching bin\KnopMops Engine.exe...
cd bin
start "" "KnopMops Engine.exe"
exit /b 0
