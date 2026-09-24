param(
    [Parameter(Mandatory = $true)] [string] $Executable,
    [Parameter(Mandatory = $true)] [string] $Scene,
    [string] $OutputRoot = ".\runs",
    [int[]] $Seeds = @(1, 2),
    [int] $Frames = 180,
    [int] $Width = 640,
    [int] $Height = 360,
    [int] $Spp = 1,
    [int] $ReferenceSpp = 256,
    [int] $MaxDepth = 5
)

$ErrorActionPreference = "Stop"
$executablePath = (Resolve-Path -LiteralPath $Executable).Path
$scenePath = (Resolve-Path -LiteralPath $Scene).Path
$rootPath = [System.IO.Path]::GetFullPath($OutputRoot)
# Focused experiment: one baseline, one key ablation, and the complete method.
$methods = @("fixed", "geometry", "proposed")

New-Item -ItemType Directory -Force -Path $rootPath | Out-Null
$originalLocation = Get-Location
Set-Location -LiteralPath ([System.IO.Path]::GetDirectoryName($executablePath))

try {
    $referenceDirectory = Join-Path $rootPath "reference"
    & $executablePath --scene $scenePath --experiment-dir $referenceDirectory `
        --method reference --frames $Frames --reference-spp $ReferenceSpp `
        --spp 8 --seed 104729 --width $Width --height $Height `
        --max-depth $MaxDepth
    if ($LASTEXITCODE -ne 0) { throw "Reference run failed" }

    foreach ($seed in $Seeds) {
        foreach ($method in $methods) {
            $methodDirectory = Join-Path $rootPath "${method}_s$seed"
            & $executablePath --scene $scenePath --experiment-dir $methodDirectory `
                --method $method --frames $Frames --spp $Spp --seed $seed `
                --width $Width --height $Height --max-depth $MaxDepth
            if ($LASTEXITCODE -ne 0) { throw "$method run failed for seed $seed" }
        }

        foreach ($method in $methods) {
            $metricsDirectory = Join-Path $rootPath "metrics_${method}_s$seed"
            $arguments = @(
                (Join-Path $PSScriptRoot "evaluate.py"),
                "--candidate", (Join-Path $rootPath "${method}_s$seed"),
                "--reference", $referenceDirectory,
                "--output", $metricsDirectory
            )
            if ($method -eq "proposed") {
                $arguments += @("--baseline", (Join-Path $rootPath "fixed_s$seed"))
            }
            python @arguments
            if ($LASTEXITCODE -ne 0) { throw "Evaluation failed for $method, seed $seed" }
        }
    }

    python (Join-Path $PSScriptRoot "aggregate.py") --root $rootPath
    if ($LASTEXITCODE -ne 0) { throw "Metric aggregation failed" }
}
finally {
    Set-Location -LiteralPath $originalLocation
}
