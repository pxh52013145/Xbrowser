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

function Get-ProjectVersionFromCache([string]$Path) {
  $line = Select-String -Path $Path -Pattern '^CMAKE_PROJECT_VERSION:.*=' -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($line) {
    $v = ($line.Line.Split('=') | Select-Object -Last 1).Trim()
    if ($v) {
      return $v
    }
  }
  return $null
}

function Find-WebView2Loader([string]$BuildDir) {
  if (!$BuildDir) {
    return $null
  }

  $depsDir = Join-Path $BuildDir "_deps"
  if (!(Test-Path $depsDir)) {
    return $null
  }

  $preferred = Get-ChildItem -Path $depsDir -Recurse -Filter "WebView2Loader.dll" -File -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\runtimes\\win-x64\\native\\WebView2Loader\.dll$' } |
    Select-Object -First 1
  if ($preferred) {
    return $preferred.FullName
  }

  $fallback = Get-ChildItem -Path $depsDir -Recurse -Filter "WebView2Loader.dll" -File -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\build\\native\\x64\\WebView2Loader\.dll$' } |
    Select-Object -First 1
  if ($fallback) {
    return $fallback.FullName
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

$version = Get-ProjectVersionFromCache $cachePath
$versionDir = if ($version) { "xbrowser-$version" } else { $null }

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

$changelogSource = Join-Path $repoRoot "docs\\CHANGELOG.md"
if (Test-Path $changelogSource) {
  Copy-Item -Path $changelogSource -Destination (Join-Path $distDir "CHANGELOG.md") -Force
}

$wv2Loader = Find-WebView2Loader $buildDir
if ($wv2Loader) {
  Copy-Item -Path $wv2Loader -Destination (Join-Path $distDir "WebView2Loader.dll") -Force
} else {
  Write-Warning "WebView2Loader.dll not found under $buildDir\\_deps; packaged folder may not run on machines without the SDK."
}

if ($versionDir) {
  $versionedRoot = Join-Path $distRoot $versionDir
  $versionedConfigDir = Join-Path $versionedRoot $Config

  if (Test-Path $versionedConfigDir) {
    Remove-Item -Recurse -Force $versionedConfigDir
  }
  New-Item -ItemType Directory -Force -Path $versionedRoot | Out-Null

  Copy-Item -Recurse -Force -Path $distDir -Destination $versionedRoot

  Write-Host "Versioned output:" -ForegroundColor Cyan
  Write-Host "  $versionedConfigDir\\xbrowser.exe"
}

Write-Host "Done. Run:" -ForegroundColor Green
Write-Host "  $distDir\\xbrowser.exe"
