@echo off
setlocal
cd /d "%~dp0bin"

set "APP=PathTracerRenderer_preview_modes.exe"

if not exist "%APP%" (
    echo [ERROR] PathTracerRenderer_preview_modes.exe was not found in the bin folder.
    echo Please rebuild the Release x64 configuration first.
    pause
    exit /b 1
)

start "PathTracer Renderer" "%APP%"
endlocal
