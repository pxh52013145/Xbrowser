param(
  [string]$ModulesCsv = "reference/logs/zen_browser_source_modules.csv",
  [string]$AreasCsv = "reference/zen_browser_merged_areas.csv",
  [string]$OutCsv = "reference/zen_browser_module_coverage.csv"
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

$repoRoot = Get-RepoRoot
$modulesPath = Resolve-PathFromRepo $repoRoot $ModulesCsv
$areasPath = Resolve-PathFromRepo $repoRoot $AreasCsv
$outPath = Resolve-PathFromRepo $repoRoot $OutCsv

if (!(Test-Path $modulesPath)) {
  throw "Modules CSV not found: $modulesPath"
}
if (!(Test-Path $areasPath)) {
  throw "Areas CSV not found: $areasPath (run scripts/merge_plans.ps1 first)"
}

$modules = Import-Csv -Path $modulesPath
$areas = Import-Csv -Path $areasPath

$areasByKey = @{}
foreach ($a in $areas) {
  $key = ([string]$a.Area).Trim().ToLowerInvariant()
  if (-not $areasByKey.ContainsKey($key)) {
    $areasByKey[$key] = $a
  }
}

function Get-AreaRecord([string]$AreaName) {
  if ([string]::IsNullOrWhiteSpace($AreaName)) {
    return $null
  }
  $key = $AreaName.Trim().ToLowerInvariant()
  if ($areasByKey.ContainsKey($key)) {
    return $areasByKey[$key]
  }
  return $null
}

function Get-ModuleAreas([string]$ModuleName) {
  if ([string]::IsNullOrWhiteSpace($ModuleName)) {
    return @()
  }

  $m = $ModuleName.Trim()
  switch ($m) {
    "@types"       { return @() }
    "common"       { return @("Core", "Core/UI", "Layering", "Layering/Popups", "Menubar", "Toast", "Notifications", "Startup") }
    "compact-mode" { return @("CompactMode") }
    "downloads"    { return @("Downloads") }
    "drag-and-drop" { return @("DnD") }
    "folders"      { return @("Folders") }
    "fonts"        { return @("Theme") }
    "glance"       { return @("Glance") }
    "images"       { return @("Theme") }
    "kbs"          { return @("Keyboard", "Shortcuts", "Input/Commands") }
    "media"        { return @("Media") }
    "mods"         { return @("Mods", "Theme", "Workspaces/Theme") }
    "sessionstore" { return @("Session", "WindowSync") }
    "share"        { return @("Share", "NativeUtils") }
    "split-view"   { return @("SplitView") }
    "tabs"         { return @("Tabs", "Essentials", "Core/Tabs", "Sidebar/Essentials") }
    "tests"        { return @("Meta/Process") }
    "toolkit"      { return @("NativeUtils", "Profiles", "Updates") }
    "urlbar"       { return @("Urlbar", "Omnibox", "SiteInfo", "SiteDataPanel") }
    "vendor"       { return @() }
    "welcome"      { return @("Welcome") }
    "workspaces"   { return @("Workspaces", "Workspace", "Workspaces/Theme") }
    default        { return @($m) }
  }
}

$outRows = @()
foreach ($mod in $modules) {
  $moduleName = [string]$mod.Module
  $areasForModule = @(Get-ModuleAreas $moduleName)

  $total = 0
  $developed = 0
  $developing = 0
  $matchedAreas = @()

  foreach ($areaName in $areasForModule) {
    $rec = Get-AreaRecord $areaName
    if ($null -eq $rec) {
      continue
    }
    $matchedAreas += [string]$rec.Area
    $total += [int]$rec.Total
    $developed += [int]$rec.Developed
    $developing += [int]$rec.Developing
  }

  $pct = ""
  if ($total -gt 0) {
    $pct = [math]::Round((100.0 * $developed) / $total, 1)
  }

  $outRows += [PSCustomObject]@{
    Module = $moduleName
    Zen_Path = [string]$mod.Zen_Path
    In_moz_build_DIRS = [string]$mod.In_moz_build_DIRS
    In_zen_assets_jar = [string]$mod.In_zen_assets_jar
    XBrowser_Areas = ($matchedAreas -join "; ")
    Total_Items = $total
    Developed_Items = $developed
    Developing_Items = $developing
    Coverage_Pct = $pct
  }
}

$outDir = Split-Path -Parent $outPath
if ($outDir -and !(Test-Path $outDir)) {
  New-Item -ItemType Directory -Force -Path $outDir | Out-Null
}

$outRows | Sort-Object Module | Export-Csv -Path $outPath -NoTypeInformation -Encoding UTF8

Write-Host "Wrote: $outPath" -ForegroundColor Green
