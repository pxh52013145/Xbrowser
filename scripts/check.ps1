param(
  [ValidateSet("Debug", "Release")]
  [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $repoRoot "build"

if (!(Test-Path $buildDir)) {
  throw "Build dir not found: $buildDir (run CMake configure first)"
}

Write-Host "Building ($Config)..." -ForegroundColor Cyan
cmake --build $buildDir --config $Config

Write-Host "Running tests ($Config)..." -ForegroundColor Cyan
Push-Location $buildDir
try {
  ctest -C $Config --output-on-failure
  exit $LASTEXITCODE
} finally {
  Pop-Location
}
