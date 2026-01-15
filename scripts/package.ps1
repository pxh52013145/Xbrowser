param(
  [ValidateSet("Release", "Debug")]
  [string]$Config = "Release",
  [string]$OutDir = "dist",
  [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $repoRoot "build"
$cachePath = Join-Path $buildDir "CMakeCache.txt"

if (!(Test-Path $buildDir)) {
  throw "Build dir not found: $buildDir (run CMake configure first)"
}

if (!$SkipBuild) {
  Write-Host "Building ($Config)..." -ForegroundColor Cyan
  cmake --build $buildDir --config $Config
}

if (!(Test-Path $cachePath)) {
  throw "CMake cache not found: $cachePath (run CMake configure first)"
}

function Get-QtPrefixFromCache([string]$Path) {
  $qtDirLine = Select-String -Path $Path -Pattern '^Qt6_DIR:.*=' -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($qtDirLine) {
    $qtDir = ($qtDirLine.Line.Split('=') | Select-Object -Last 1).Trim()
    if ($qtDir) {
      $candidate = [System.IO.Path]::GetFullPath((Join-Path $qtDir "..\\..\\.."))
      if (Test-Path $candidate) {
        return $candidate
      }
    }
  }

  $prefixLine = Select-String -Path $Path -Pattern '^CMAKE_PREFIX_PATH:.*=' -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($prefixLine) {
    $prefix = ($prefixLine.Line.Split('=') | Select-Object -Last 1).Trim()
    if ($prefix -like "*;*") {
      $prefix = $prefix.Split(';')[0].Trim()
    }
    if ($prefix -and (Test-Path $prefix)) {
      return $prefix
    }
  }

  return $null
}

$qtPrefix = Get-QtPrefixFromCache $cachePath
if (!$qtPrefix) {
  throw "Unable to determine Qt prefix from $cachePath (missing Qt6_DIR / CMAKE_PREFIX_PATH)."
}

$windeployqt = Join-Path $qtPrefix "bin\\windeployqt.exe"
if (!(Test-Path $windeployqt)) {
  throw "windeployqt.exe not found: $windeployqt"
}

$exePath = $null
$configExe = Join-Path $buildDir (Join-Path $Config "xbrowser.exe")
$singleExe = Join-Path $buildDir "xbrowser.exe"

if (Test-Path $configExe) {
  $exePath = $configExe
} elseif (Test-Path $singleExe) {
  $exePath = $singleExe
} else {
  $exe = Get-ChildItem -Path $buildDir -Recurse -Filter "xbrowser.exe" -File |
    Where-Object { $_.FullName -match "\\$Config\\xbrowser\\.exe$" } |
    Select-Object -First 1
  if ($exe) {
    $exePath = $exe.FullName
  }
}

if (!$exePath) {
  throw "xbrowser.exe not found under $buildDir for config '$Config' (build it first)."
}

$qmlDir = Join-Path $repoRoot "ui\\qml"
if (!(Test-Path $qmlDir)) {
  throw "QML dir not found: $qmlDir"
}

$distRoot = Join-Path $repoRoot $OutDir
$distDir = Join-Path $distRoot $Config

if (Test-Path $distDir) {
  Remove-Item -Recurse -Force $distDir
}

New-Item -ItemType Directory -Force -Path $distDir | Out-Null

Write-Host "Packaging to: $distDir" -ForegroundColor Cyan
Copy-Item -Path $exePath -Destination (Join-Path $distDir "xbrowser.exe") -Force

$modeArg = if ($Config -eq "Debug") { "--debug" } else { "--release" }
$args = @($modeArg, "--force", "--no-translations", "--compiler-runtime", "--qmldir", $qmlDir, (Join-Path $distDir "xbrowser.exe"))

& $windeployqt @args

$qtConfSource = Join-Path $repoRoot "cmake\\qt.conf"
if (Test-Path $qtConfSource) {
  Copy-Item -Path $qtConfSource -Destination (Join-Path $distDir "qt.conf") -Force
}

Write-Host "Done. Run:" -ForegroundColor Green
Write-Host "  $distDir\\xbrowser.exe"

