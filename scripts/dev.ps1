param(
  [ValidateSet("Debug", "Release")]
  [string]$Config = "Debug",
  [string[]]$Args = @(),
  [string]$QtPrefix = "",
  [string]$Generator = "Visual Studio 17 2022",
  [string]$Arch = "x64",
  [string[]]$CMakeArgs = @(),
  [bool]$Detach = $true,
  [bool]$StopRunningBuildOutput = $true
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $repoRoot "build"
$cachePath = Join-Path $buildDir "CMakeCache.txt"

function Stop-BuildOutputXBrowser([string]$BuildDir, [string]$Config) {
  $targets = @(
    (Join-Path $BuildDir (Join-Path $Config "xbrowser.exe")),
    (Join-Path $BuildDir "xbrowser.exe")
  )

  $normalizedTargets = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
  foreach ($t in $targets) {
    if ([string]::IsNullOrWhiteSpace($t)) {
      continue
    }
    try {
      [void]$normalizedTargets.Add([System.IO.Path]::GetFullPath($t))
    } catch {
      [void]$normalizedTargets.Add($t)
    }
  }

  $candidates = @()
  try {
    $candidates = Get-CimInstance Win32_Process -Filter "Name='xbrowser.exe'" -ErrorAction SilentlyContinue
  } catch {
    return
  }

  $toStop = @()
  foreach ($p in $candidates) {
    $path = $p.ExecutablePath
    if ([string]::IsNullOrWhiteSpace($path)) {
      continue
    }
    $full = $path
    try {
      $full = [System.IO.Path]::GetFullPath($path)
    } catch {
    }
    if ($normalizedTargets.Contains($full)) {
      $toStop += $p
    }
  }

  if ($toStop.Count -eq 0) {
    return
  }

  Write-Host "Stopping running xbrowser.exe from build output to avoid Windows file locks (LNK1104)..." -ForegroundColor Yellow
  foreach ($p in $toStop) {
    $procId = [int]$p.ProcessId
    $proc = Get-Process -Id $procId -ErrorAction SilentlyContinue
    if ($proc) {
      try { $null = $proc.CloseMainWindow() } catch {}
    }
  }

  Start-Sleep -Milliseconds 800

  foreach ($p in $toStop) {
    $procId = [int]$p.ProcessId
    $proc = Get-Process -Id $procId -ErrorAction SilentlyContinue
    if ($proc -and !$proc.HasExited) {
      Stop-Process -Id $procId -Force -ErrorAction SilentlyContinue
    }
  }
}

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

if ($StopRunningBuildOutput) {
  Stop-BuildOutputXBrowser -BuildDir $buildDir -Config $Config
}

Write-Host "Building xbrowser ($Config)..." -ForegroundColor Cyan
cmake --build $buildDir --config $Config --target xbrowser
if ($LASTEXITCODE -ne 0) {
  if ($StopRunningBuildOutput) {
    Write-Warning "Build failed (exit code $LASTEXITCODE). Retrying once after stopping any remaining build-output xbrowser.exe..."
    Stop-BuildOutputXBrowser -BuildDir $buildDir -Config $Config
    cmake --build $buildDir --config $Config --target xbrowser
  }

  if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE (close xbrowser.exe if it is running, then retry)."
  }
}

$runParams = @{
  Config = $Config
  Args = $Args
}
if ($Detach) {
  $runParams.Detach = $true
}

& (Join-Path $PSScriptRoot "run.ps1") @runParams
exit $LASTEXITCODE
