param(
  [ValidateSet("Debug", "Release")]
  [string]$Config = "Debug",
  [string[]]$Args = @()
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $repoRoot "build"

if (!(Test-Path $buildDir)) {
  throw "Build dir not found: $buildDir (run CMake configure first)"
}

Write-Host "Building ($Config)..." -ForegroundColor Cyan
cmake --build $buildDir --config $Config

& (Join-Path $PSScriptRoot "run.ps1") -Config $Config -Args $Args

