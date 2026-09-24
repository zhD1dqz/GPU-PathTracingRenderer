@echo off
setlocal
cd /d "%~dp0bin"

if not exist "PathTracerRenderer_research.exe" (
    echo [ERROR] PathTracerRenderer_research.exe is missing from the bin folder.
    pause
    exit /b 1
)

start "PathTracer Studio" "PathTracerRenderer_research.exe"
endlocal
