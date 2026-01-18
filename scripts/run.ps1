param(
  [ValidateSet("Debug", "Release")]
  [string]$Config = "Debug",
  [string[]]$Args = @(),
  [switch]$InPlace,
  [switch]$Detach
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $repoRoot "build"
$cachePath = Join-Path $buildDir "CMakeCache.txt"

if (!(Test-Path $buildDir)) {
  throw "Build dir not found: $buildDir (run CMake configure/build first)"
}

function Get-QtPrefixFromCache([string]$Path) {
  if (!$Path -or !(Test-Path $Path)) {
    return $null
  }

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

function Set-QtEnv([string]$QtPrefix) {
  if ([string]::IsNullOrWhiteSpace($QtPrefix) -or !(Test-Path $QtPrefix)) {
    return
  }

  $qtBin = Join-Path $QtPrefix "bin"
  if (Test-Path $qtBin) {
    $env:Path = "$qtBin;$env:Path"
  }

  $plugins = Join-Path $QtPrefix "plugins"
  $qml = Join-Path $QtPrefix "qml"
  $platforms = Join-Path $QtPrefix "plugins\\platforms"

  if (Test-Path $plugins) {
    $env:QT_PLUGIN_PATH = $plugins
  }
  if (Test-Path $qml) {
    $env:QML2_IMPORT_PATH = $qml
  }
  if (Test-Path $platforms) {
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $platforms
  }
}

function New-RunInstanceDir([string]$BuildDir, [string]$Config) {
  $base = Join-Path $BuildDir (Join-Path "run" $Config)
  New-Item -ItemType Directory -Force -Path $base | Out-Null

  $stamp = (Get-Date).ToString("yyyyMMdd-HHmmss")
  $rand = Get-Random -Minimum 1000 -Maximum 9999
  $dir = Join-Path $base ("run-{0}-{1}" -f $stamp, $rand)
  New-Item -ItemType Directory -Force -Path $dir | Out-Null
  return $dir
}

function Copy-RunArtifacts([string]$SourceExe, [string]$DestDir) {
  if (!(Test-Path $SourceExe)) {
    throw "Source exe not found: $SourceExe"
  }
  if (!(Test-Path $DestDir)) {
    New-Item -ItemType Directory -Force -Path $DestDir | Out-Null
  }

  $destExe = Join-Path $DestDir "xbrowser.exe"
  Copy-Item -Path $SourceExe -Destination $destExe -Force

  $pdb = [System.IO.Path]::ChangeExtension($SourceExe, ".pdb")
  if (Test-Path $pdb) {
    Copy-Item -Path $pdb -Destination (Join-Path $DestDir "xbrowser.pdb") -Force
  }

  return $destExe
}

function To-CommandLine([string[]]$Args) {
  if (!$Args) {
    return ""
  }
  $escaped = @()
  foreach ($arg in $Args) {
    if ($null -eq $arg) {
      continue
    }
    if ($arg -match '[\s"]') {
      $escaped += ('"{0}"' -f ($arg -replace '"', '\\"'))
    } else {
      $escaped += $arg
    }
  }
  return ($escaped -join ' ')
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
    Where-Object { $_.FullName -match "\\$Config\\xbrowser\.exe$" } |
    Select-Object -First 1
  if ($exe) {
    $exePath = $exe.FullName
  }
}

if (!$exePath) {
  throw "xbrowser.exe not found under $buildDir for config '$Config' (build it first)."
}

$exeDir = Split-Path -Parent $exePath

if (-not $InPlace) {
  if (!(Test-Path $cachePath)) {
    throw "CMake cache not found: $cachePath (run CMake configure first, or use -InPlace)."
  }

  $qtPrefix = Get-QtPrefixFromCache $cachePath
  if (!$qtPrefix) {
    throw "Unable to determine Qt prefix from $cachePath (missing Qt6_DIR / CMAKE_PREFIX_PATH). Use -InPlace to run from the deployed build output."
  }

  Set-QtEnv $qtPrefix

  $runDir = New-RunInstanceDir $buildDir $Config
  $stagedExe = Copy-RunArtifacts $exePath $runDir

  Write-Host "Running (staged): $stagedExe" -ForegroundColor Cyan
  if ($Detach) {
    $argLine = To-CommandLine $Args
    if ([string]::IsNullOrWhiteSpace($argLine)) {
      Start-Process -FilePath $stagedExe -WorkingDirectory $runDir | Out-Null
    } else {
      Start-Process -FilePath $stagedExe -WorkingDirectory $runDir -ArgumentList $argLine | Out-Null
    }
    exit 0
  }

  Push-Location $runDir
  try {
    & $stagedExe @Args
    exit $LASTEXITCODE
  } finally {
    Pop-Location
  }
}

function Test-DeployedQtRuntime([string]$Dir) {
  if (!$Dir -or !(Test-Path $Dir)) {
    return $false
  }

  $requiredDllPatterns = @(
    "Qt6Core*.dll",
    "Qt6Gui*.dll",
    "Qt6Qml*.dll",
    "Qt6Quick*.dll",
    "Qt6QuickControls2*.dll",
    "Qt6Network*.dll",
    "Qt6OpenGL*.dll"
  )

  foreach ($pattern in $requiredDllPatterns) {
    $dll = Get-ChildItem -Path $Dir -Filter $pattern -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if (!$dll) {
      return $false
    }
  }

  $platformsDir = Join-Path $Dir "platforms"
  if (!(Test-Path $platformsDir)) {
    return $false
  }

  $qwindows = Get-ChildItem -Path $platformsDir -Filter "qwindows*.dll" -File -ErrorAction SilentlyContinue | Select-Object -First 1
  if (!$qwindows) {
    return $false
  }

  $qmlDir = Join-Path $Dir "qml"
  if (!(Test-Path $qmlDir)) {
    return $false
  }

  return $true
}

function Get-DeployStampPath([string]$ExeDir, [string]$Config) {
  if (!$ExeDir) {
    return $null
  }
  return (Join-Path $ExeDir (".xbrowser_qt_deploy_{0}.stamp" -f $Config))
}

function Test-DeployStampUpToDate([string]$ExePath, [string]$ExeDir, [string]$Config) {
  if (!$ExePath -or !(Test-Path $ExePath)) {
    return $false
  }

  $stamp = Get-DeployStampPath $ExeDir $Config
  if (!$stamp -or !(Test-Path $stamp)) {
    return $false
  }

  $exeTime = (Get-Item $ExePath).LastWriteTimeUtc
  $stampTime = (Get-Item $stamp).LastWriteTimeUtc
  return $stampTime -ge $exeTime
}

function Ensure-QtConf([string]$ExeDir) {
  $qtConfSource = Join-Path $repoRoot "cmake\\qt.conf"
  $qtConfDest = Join-Path $ExeDir "qt.conf"
  if (Test-Path $qtConfSource) {
    Copy-Item -Path $qtConfSource -Destination $qtConfDest -Force
  }
}

if ((Test-DeployedQtRuntime $exeDir) -and (Test-DeployStampUpToDate $exePath $exeDir $Config)) {
  Ensure-QtConf $exeDir
  Write-Host "Running (deployed Qt): $exePath" -ForegroundColor Cyan
  if ($Detach) {
    $argLine = To-CommandLine $Args
    if ([string]::IsNullOrWhiteSpace($argLine)) {
      Start-Process -FilePath $exePath -WorkingDirectory $exeDir | Out-Null
    } else {
      Start-Process -FilePath $exePath -WorkingDirectory $exeDir -ArgumentList $argLine | Out-Null
    }
    exit 0
  }

  & $exePath @Args
  exit $LASTEXITCODE
}

$deployScript = Join-Path $PSScriptRoot "deploy.ps1"
if (Test-Path $deployScript) {
  try {
    Write-Host "Qt runtime not deployed, running deploy..." -ForegroundColor Yellow
    & $deployScript -Config $Config
  } catch {
    Write-Warning "Qt deploy failed: $($_.Exception.Message)"
  }
}

if ((Test-DeployedQtRuntime $exeDir) -and (Test-DeployStampUpToDate $exePath $exeDir $Config)) {
  Ensure-QtConf $exeDir
  Write-Host "Running (deployed Qt): $exePath" -ForegroundColor Cyan
  if ($Detach) {
    $argLine = To-CommandLine $Args
    if ([string]::IsNullOrWhiteSpace($argLine)) {
      Start-Process -FilePath $exePath -WorkingDirectory $exeDir | Out-Null
    } else {
      Start-Process -FilePath $exePath -WorkingDirectory $exeDir -ArgumentList $argLine | Out-Null
    }
    exit 0
  }

  & $exePath @Args
  exit $LASTEXITCODE
}

if (!(Test-Path $cachePath)) {
  throw "CMake cache not found: $cachePath (run CMake configure first)"
}

$qtPrefix = Get-QtPrefixFromCache $cachePath
if (!$qtPrefix) {
  throw "Unable to determine Qt prefix from $cachePath (missing Qt6_DIR / CMAKE_PREFIX_PATH)."
}

$qtBin = Join-Path $qtPrefix "bin"
if (!(Test-Path $qtBin)) {
  throw "Qt bin dir not found: $qtBin"
}

Set-QtEnv $qtPrefix

Write-Host "Running: $exePath" -ForegroundColor Cyan
if ($Detach) {
  $argLine = To-CommandLine $Args
  if ([string]::IsNullOrWhiteSpace($argLine)) {
    Start-Process -FilePath $exePath -WorkingDirectory $exeDir | Out-Null
  } else {
    Start-Process -FilePath $exePath -WorkingDirectory $exeDir -ArgumentList $argLine | Out-Null
  }
  exit 0
}

& $exePath @Args
