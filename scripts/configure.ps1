param(
  [string]$BuildDir = "build",
  [string]$Generator = "Visual Studio 17 2022",
  [string]$Arch = "x64",
  [string]$QtPrefix = "",
  [string[]]$CMakeArgs = @()
)

$ErrorActionPreference = "Stop"

function Get-RepoRoot() {
  return (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

function Resolve-PathFromRepo([string]$RepoRoot, [string]$PathValue) {
  if ([string]::IsNullOrWhiteSpace($PathValue)) {
    return $null
  }
  if ([System.IO.Path]::IsPathRooted($PathValue)) {
    return $PathValue
  }
  return (Join-Path $RepoRoot $PathValue)
}

function Split-SemicolonList([string]$Value) {
  if ([string]::IsNullOrWhiteSpace($Value)) {
    return @()
  }
  return @($Value.Split(';') | ForEach-Object { $_.Trim() } | Where-Object { $_ })
}

function Get-QtPrefixFromQt6Dir([string]$Qt6Dir) {
  if ([string]::IsNullOrWhiteSpace($Qt6Dir)) {
    return $null
  }
  $candidate = $Qt6Dir.Trim()
  if (!(Test-Path $candidate)) {
    return $null
  }
  try {
    $prefix = [System.IO.Path]::GetFullPath((Join-Path $candidate "..\\..\\.."))
    if ($prefix -and (Test-Path $prefix)) {
      return $prefix
    }
  } catch {
  }
  return $null
}

function Test-QtPrefix([string]$Prefix) {
  if ([string]::IsNullOrWhiteSpace($Prefix)) {
    return $false
  }
  if (!(Test-Path $Prefix)) {
    return $false
  }

  $bin = Join-Path $Prefix "bin"
  if (!(Test-Path $bin)) {
    return $false
  }

  $qtCore = Get-ChildItem -Path $bin -Filter "Qt6Core*.dll" -File -ErrorAction SilentlyContinue | Select-Object -First 1
  $windeploy = Join-Path $bin "windeployqt.exe"
  if ($qtCore -or (Test-Path $windeploy)) {
    return $true
  }

  return $false
}

function Find-QtPrefix([string]$ExplicitQtPrefix) {
  $candidates = New-Object System.Collections.Generic.List[string]

  if (-not [string]::IsNullOrWhiteSpace($ExplicitQtPrefix)) {
    $candidates.Add($ExplicitQtPrefix.Trim())
  }

  foreach ($envVar in @("Qt6_DIR", "QT6_DIR")) {
    $val = [string]([System.Environment]::GetEnvironmentVariable($envVar))
    $prefix = Get-QtPrefixFromQt6Dir $val
    if ($prefix) {
      $candidates.Add($prefix)
    }
  }

  foreach ($envVar in @("Qt_DIR", "QTDIR", "QT_DIR")) {
    $val = [string]([System.Environment]::GetEnvironmentVariable($envVar))
    if (-not [string]::IsNullOrWhiteSpace($val)) {
      $candidates.Add($val.Trim())
    }
  }

  foreach ($p in (Split-SemicolonList ([string]$env:CMAKE_PREFIX_PATH))) {
    $candidates.Add($p)
  }

  $qtRoots = @()
  foreach ($drive in (Get-PSDrive -PSProvider FileSystem)) {
    if (-not $drive.Root) {
      continue
    }
    $root = $drive.Root.TrimEnd('\\')
    if ($root.Length -eq 0) {
      continue
    }
    $candidate = "$root\\Qt"
    if (Test-Path $candidate) {
      $qtRoots += $candidate
    }
  }

  $qtRoots = @($qtRoots | Select-Object -Unique)
  foreach ($qtRoot in $qtRoots) {
    $found = Get-ChildItem -Path $qtRoot -Directory -ErrorAction SilentlyContinue | ForEach-Object {
      $verDir = $_.FullName
      Get-ChildItem -Path $verDir -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "msvc*_64" } |
        ForEach-Object { $_.FullName }
    }

    foreach ($p in @($found)) {
      $candidates.Add($p)
    }
  }

  $valid = @()
  foreach ($p in $candidates) {
    if (Test-QtPrefix $p) {
      try {
        $valid += (Resolve-Path $p).Path
      } catch {
        $valid += $p
      }
    }
  }

  if ($valid.Count -eq 0) {
    return $null
  }

  # Prefer the highest version if installed under C:\Qt\<ver>\msvc*_64, otherwise first valid.
  $best = $null
  $bestVer = $null
  foreach ($p in $valid) {
    $norm = $p.TrimEnd('\\')
    $verDir = Split-Path -Parent $norm
    $verName = Split-Path -Leaf $verDir
    $qtRoot = Split-Path -Parent $verDir
    $ver = $null
    if ($qtRoot -and ((Split-Path -Leaf $qtRoot) -ieq "Qt") -and ([version]::TryParse($verName, [ref]$ver))) {
      if ($null -eq $best -or $ver -gt $bestVer) {
        $best = $p
        $bestVer = $ver
      }
      continue
    }

    if ($null -eq $best) {
      $best = $p
    }
  }

  return $best
}

$repoRoot = Get-RepoRoot
$resolvedBuildDir = Resolve-PathFromRepo $repoRoot $BuildDir
if (!$resolvedBuildDir) {
  throw "Build dir not set."
}
if (!(Test-Path $resolvedBuildDir)) {
  New-Item -ItemType Directory -Force -Path $resolvedBuildDir | Out-Null
}

$qt = Find-QtPrefix $QtPrefix
if ($qt) {
  Write-Host "Qt prefix: $qt" -ForegroundColor DarkCyan
} else {
  Write-Host "Qt prefix not auto-detected. CMake will try to find Qt via environment." -ForegroundColor Yellow
  Write-Host "Tip: set CMAKE_PREFIX_PATH to your Qt install root (e.g. C:\\Qt\\6.6.2\\msvc2019_64)." -ForegroundColor Yellow
}

$args = @("-S", $repoRoot, "-B", $resolvedBuildDir)
if (-not [string]::IsNullOrWhiteSpace($Generator)) {
  $args += @("-G", $Generator)
}
if ($Generator -like "Visual Studio*" -and -not [string]::IsNullOrWhiteSpace($Arch)) {
  $args += @("-A", $Arch)
}
if ($qt) {
  $args += "-DCMAKE_PREFIX_PATH=$qt"
}
if ($CMakeArgs) {
  $args += $CMakeArgs
}

Write-Host "Configuring (CMake)..." -ForegroundColor Cyan
& cmake @args
