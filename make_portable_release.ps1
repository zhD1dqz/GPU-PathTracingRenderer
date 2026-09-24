[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$projectRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
$releaseRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot 'release'))
$packageRoot = [System.IO.Path]::GetFullPath((Join-Path $releaseRoot 'PathTracerStudio-Portable'))
$archivePath = [System.IO.Path]::GetFullPath((Join-Path $releaseRoot 'PathTracerStudio-Portable.zip'))

function Assert-ReleasePath([string]$Path)
{
    $resolved = [System.IO.Path]::GetFullPath($Path)
    $prefix = $releaseRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar) +
              [System.IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase))
    {
        throw "Refusing to modify a path outside the release directory: $resolved"
    }
}

New-Item -ItemType Directory -Path $releaseRoot -Force | Out-Null

if (Test-Path -LiteralPath $packageRoot)
{
    Assert-ReleasePath $packageRoot
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
if (Test-Path -LiteralPath $archivePath)
{
    Assert-ReleasePath $archivePath
    Remove-Item -LiteralPath $archivePath -Force
}

$packageBin = Join-Path $packageRoot 'bin'
New-Item -ItemType Directory -Path $packageBin -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $packageRoot 'assets') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $packageRoot 'src') -Force | Out-Null

$rendererSource = Join-Path $projectRoot 'bin\PathTracerRenderer_research.exe'
if (-not (Test-Path -LiteralPath $rendererSource))
{
    throw 'The Research renderer executable was not found in the bin directory.'
}
Copy-Item -LiteralPath $rendererSource -Destination $packageBin

foreach ($runtimeName in @('SDL2.dll', 'OpenImageDenoise.dll', 'tbb.dll'))
{
    $runtimeSource = Join-Path $projectRoot "bin\$runtimeName"
    if (-not (Test-Path -LiteralPath $runtimeSource))
    {
        throw "Required runtime library is missing: $runtimeSource"
    }
    Copy-Item -LiteralPath $runtimeSource -Destination $packageBin
}

# App-local deployment makes Visual Studio unnecessary on the destination PC.
foreach ($runtimeName in @('MSVCP140.dll', 'VCRUNTIME140.dll', 'VCRUNTIME140_1.dll'))
{
    $runtimeSource = Join-Path $env:WINDIR "System32\$runtimeName"
    if (-not (Test-Path -LiteralPath $runtimeSource))
    {
        throw "Microsoft runtime library is missing on the packaging PC: $runtimeSource"
    }
    Copy-Item -LiteralPath $runtimeSource -Destination $packageBin
}

Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\myscene') -Destination (Join-Path $packageRoot 'assets') -Recurse
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\HDR') -Destination (Join-Path $packageRoot 'assets') -Recurse
Copy-Item -LiteralPath (Join-Path $projectRoot 'src\shaders') -Destination (Join-Path $packageRoot 'src') -Recurse
Copy-Item -LiteralPath (Join-Path $projectRoot 'packaging\README.txt') -Destination $packageRoot
Copy-Item -LiteralPath (Join-Path $projectRoot 'packaging\Launch.bat') -Destination $packageRoot

$launcherCompiler = Get-Command 'g++.exe' -ErrorAction SilentlyContinue
if (-not $launcherCompiler)
{
    throw 'g++.exe is required to build the portable launcher on this packaging PC.'
}

$launcherSource = Join-Path $projectRoot 'tools\launcher\PathTracerStudioLauncher.cpp'
$launcherOutput = Join-Path $packageRoot 'PathTracerStudio.exe'
& $launcherCompiler.Source $launcherSource '-std=c++17' '-O2' '-s' '-mwindows' '-municode' '-o' $launcherOutput
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $launcherOutput))
{
    throw 'Failed to build the portable launcher.'
}

$hashLines = Get-ChildItem -LiteralPath $packageBin -File |
             Sort-Object Name |
             ForEach-Object {
                 $hash = Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256
                 "$($hash.Hash)  bin/$($_.Name)"
             }
$hashLines | Set-Content -LiteralPath (Join-Path $packageRoot 'SHA256SUMS.txt') -Encoding ascii

Compress-Archive -LiteralPath $packageRoot -DestinationPath $archivePath -CompressionLevel Optimal

$archive = Get-Item -LiteralPath $archivePath
Write-Host "Portable package created: $($archive.FullName)"
Write-Host ("Archive size: {0:N2} MB" -f ($archive.Length / 1MB))
Write-Host "Renderer source: $rendererSource"
