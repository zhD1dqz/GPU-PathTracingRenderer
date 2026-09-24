[CmdletBinding()]
param(
    [string]$OutputName = 'PathTracerRenderer_research_ui.exe'
)

$ErrorActionPreference = 'Stop'
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$compiler = Get-Command 'g++.exe' -ErrorAction SilentlyContinue
if (-not $compiler)
{
    throw 'A 64-bit MinGW g++ compiler is required to build this project without Visual Studio.'
}

$sources = @(
    'src\Main.cpp',
    'src\core\PTCamera.cpp',
    'src\core\PTEnvironmentMap.cpp',
    'src\core\PTMesh.cpp',
    'src\core\PTProgram.cpp',
    'src\core\PTQuad.cpp',
    'src\core\PTRenderer.cpp',
    'src\core\PTScene.cpp',
    'src\core\PTShader.cpp',
    'src\core\PTTexture.cpp',
    'src\loaders\GLTFLoader.cpp',
    'src\loaders\Loader.cpp',
    'thirdparty\gl3w\GL\gl3w.c',
    'thirdparty\RadeonRays\bbox.cpp',
    'thirdparty\RadeonRays\bvh.cpp',
    'thirdparty\RadeonRays\bvh_translator.cpp',
    'thirdparty\RadeonRays\split_bvh.cpp',
    'thirdparty\imgui\imgui.cpp',
    'thirdparty\imgui\imgui_draw.cpp',
    'thirdparty\imgui\imgui_tables.cpp',
    'thirdparty\imgui\imgui_widgets.cpp',
    'thirdparty\imgui\backends\imgui_impl_sdl2.cpp',
    'thirdparty\imgui\backends\imgui_impl_opengl3.cpp'
)

$includes = @(
    '-I.\src\core', '-I.\src\loaders', '-I.\src\math', '-I.\thirdparty\stb',
    '-I.\thirdparty\RadeonRays', '-I.\thirdparty\tinydir', '-I.\thirdparty\tinyobjloader',
    '-I.\thirdparty\oidn\include', '-I.\thirdparty\tinygltf', '-I.\thirdparty\SDL2\include',
    '-I.\thirdparty\SDL2\include\SDL2', '-I.\thirdparty\gl3w', '-I.\thirdparty\imgui',
    '-I.\thirdparty\imgui\backends'
)

$defines = @(
    '-DWIN32', '-D_WINDOWS', '-D_CRT_SECURE_NO_WARNINGS',
    '-D__STDC_FORMAT_MACROS', '-D__STDC_LIMIT_MACROS',
    '-D__STDC_CONSTANT_MACROS', '-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS',
    '-DUSE_DL_PREFIX'
)

$libraryFlags = @(
    '-L.\thirdparty\SDL2\lib\x64',
    '-L.\thirdparty\oidn\lib',
    '-lSDL2', '-lOpenImageDenoise', '-lopengl32', '-lglu32', '-lcomdlg32',
    '-lshell32', '-lole32', '-loleaut32', '-luuid', '-lgdi32', '-luser32',
    '-ladvapi32', '-lwinmm', '-limm32', '-lversion', '-static-libgcc',
    '-static-libstdc++', '-mwindows'
)

$outputPath = Join-Path $projectRoot ('bin\' + $OutputName)
Push-Location $projectRoot
try
{
    & $compiler.Source @sources @includes @defines '-std=c++17' '-O2' '-fpermissive' '-w' @libraryFlags '-o' $outputPath
    if ($LASTEXITCODE -ne 0)
    {
        throw "Build failed with exit code $LASTEXITCODE."
    }
}
finally
{
    Pop-Location
}

Write-Host "Research UI build created: $outputPath"
