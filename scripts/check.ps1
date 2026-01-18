param(
  [ValidateSet("Debug", "Release")]
  [string]$Config = "Debug",
  [string]$QtPrefix = "",
  [string]$Generator = "Visual Studio 17 2022",
  [string]$Arch = "x64",
  [string[]]$CMakeArgs = @()
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $repoRoot "build"
$cachePath = Join-Path $buildDir "CMakeCache.txt"

if (!(Test-Path $cachePath)) {
  Write-Host "Build dir not configured, running CMake configure..." -ForegroundColor Yellow
  & (Join-Path $PSScriptRoot "configure.ps1") `
    -BuildDir $buildDir `
    -Generator $Generator `
    -Arch $Arch `
    -QtPrefix $QtPrefix `
    -CMakeArgs $CMakeArgs
}

if (!(Test-Path $buildDir)) {
  throw "Build dir not found: $buildDir (configure failed)"
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
