@echo off
setlocal
cd /d "%~dp0KnopMops Engine"
if not exist "bin\KnopMops Engine.exe" call build.bat --no-run
cd bin
start "" "KnopMops Engine.exe"
