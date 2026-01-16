param(
  [string]$ReferenceDir = "",
  [string]$DevelopingDir = "",
  [string]$OutDir = "",
  [switch]$IncludeLayout
)

$ErrorActionPreference = "Stop"

function Get-RepoRoot() {
  return (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

function Get-PriorityKey([string]$Priority) {
  if ([string]::IsNullOrWhiteSpace($Priority)) {
    return 50
  }
  $p = $Priority.Trim()
  if ($p -match '^P(\d+)$') {
    return [int]$Matches[1]
  }
  switch ($p.ToUpperInvariant()) {
    "META" { return -100 }
    "MVP" { return -10 }
    default { return 40 }
  }
}

function Normalize-Row($Row, [string]$SourceCsv, [string]$SourceKind) {
  $area = $Row.Area
  if ([string]::IsNullOrWhiteSpace($area)) {
    $area = $Row.Category
  }

  $title = $Row.Feature
  $itemType = "Feature"
  if ([string]::IsNullOrWhiteSpace($title)) {
    $title = $Row.Interaction
    $itemType = "Interaction"
  }

  $suggestedFiles = $Row.Suggested_Files
  if ([string]::IsNullOrWhiteSpace($suggestedFiles)) {
    $suggestedFiles = $Row.XBrowser_Suggested_Files
  }

  $dependsOn = $Row.DependsOn
  if ([string]::IsNullOrWhiteSpace($dependsOn)) {
    $dependsOn = $Row.XBrowser_DependsOn
  }

  $status = $Row.Status
  $priority = $Row.Priority
  $milestone = $Row.Milestone

  $bucket = "developing"
  if (-not [string]::IsNullOrWhiteSpace($status) -and $status.Trim().ToLowerInvariant() -eq "done") {
    $bucket = "developed"
  }

  $obj = [PSCustomObject]@{
    Bucket         = $bucket
    Area           = if ([string]::IsNullOrWhiteSpace($area)) { "Uncategorized" } else { $area.Trim() }
    ID             = [string]$Row.ID
    ItemType       = $itemType
    Title          = [string]$title
    Priority       = [string]$priority
    Milestone      = [string]$milestone
    Status         = [string]$status
    XBrowser_Current = [string]$Row.XBrowser_Current
    XBrowser_Task  = [string]$Row.XBrowser_Task
    Suggested_Files = [string]$suggestedFiles
    DependsOn      = [string]$dependsOn
    Done_Criteria  = [string]$Row.Done_Criteria
    Test_Steps     = [string]$Row.Test_Steps
    SourceCsv      = $SourceCsv
    SourceKind     = $SourceKind
  }

  return $obj
}

function Write-Csv([string]$Path, $Rows, $SchemaRow) {
  if ($Rows -and @($Rows).Count -gt 0) {
    $Rows | Export-Csv -Path $Path -NoTypeInformation -Encoding UTF8
    return
  }

  if (!$SchemaRow) {
    throw "Cannot write CSV header-only without schema row: $Path"
  }

  $names = @($SchemaRow.PSObject.Properties.Name)
  $header = '"' + ($names -join '","') + '"'
  $utf8Bom = New-Object System.Text.UTF8Encoding($true)
  [System.IO.File]::WriteAllText($Path, ($header + "`r`n"), $utf8Bom)
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

function Read-PlanCsv([string]$CsvPath, [string]$SourceCsv, [string]$SourceKind, [switch]$IncludeLayout) {
  $rows = $null
  try {
    $rows = Import-Csv -Path $CsvPath
  } catch {
    Write-Warning "Failed to parse CSV: $CsvPath ($($_.Exception.Message))"
    return @()
  }

  if (!$rows -or @($rows).Count -eq 0) {
    return @()
  }

  $props = @($rows[0].PSObject.Properties.Name)
  $hasStatus = $props -contains "Status"
  $isLayout = ($props -contains "Category") -and ($props -contains "Feature") -and (-not $hasStatus)

  if (-not $hasStatus -and -not ($IncludeLayout -and $isLayout)) {
    return @()
  }

  $out = @()
  foreach ($row in $rows) {
    if (-not $row.ID) {
      continue
    }
    $out += (Normalize-Row $row $SourceCsv $SourceKind)
  }
  return $out
}

$repoRoot = Get-RepoRoot

if ([string]::IsNullOrWhiteSpace($ReferenceDir)) {
  $ReferenceDir = (Join-Path $repoRoot "reference\\logs")
}

$resolvedLogsDir = Resolve-PathFromRepo $repoRoot $ReferenceDir
if (!$resolvedLogsDir) {
  throw "Logs dir not set."
}
if (!(Test-Path $resolvedLogsDir)) {
  throw "Logs dir not found: $resolvedLogsDir"
}

if ([string]::IsNullOrWhiteSpace($DevelopingDir)) {
  $DevelopingDir = (Join-Path $repoRoot "reference\\developing")
}
$resolvedDevelopingDir = Resolve-PathFromRepo $repoRoot $DevelopingDir

if ([string]::IsNullOrWhiteSpace($OutDir)) {
  $OutDir = (Join-Path $repoRoot "reference")
}
$resolvedOutDir = Resolve-PathFromRepo $repoRoot $OutDir
if (!$resolvedOutDir) {
  throw "Out dir not set."
}
if (!(Test-Path $resolvedOutDir)) {
  New-Item -ItemType Directory -Force -Path $resolvedOutDir | Out-Null
}

$recordsById = @{}

$inputFiles = @()
$inputFiles += Get-ChildItem -Path $resolvedLogsDir -Filter "*.csv" -File | Where-Object { $_.Name -notlike "zen_browser_merged_*" }
if ($resolvedDevelopingDir -and (Test-Path $resolvedDevelopingDir)) {
  $inputFiles += Get-ChildItem -Path $resolvedDevelopingDir -Filter "*.csv" -File | Where-Object { $_.Name -notlike "zen_browser_merged_*" }
}

foreach ($file in $inputFiles) {
  $kind = if ($resolvedDevelopingDir -and (Test-Path $resolvedDevelopingDir) -and ($file.FullName -like "$resolvedDevelopingDir*")) { "developing" } else { "logs" }
  $sourceCsv = if ($kind -eq "developing") { "developing/$($file.Name)" } else { "logs/$($file.Name)" }

  $rows = Read-PlanCsv -CsvPath $file.FullName -SourceCsv $sourceCsv -SourceKind $kind -IncludeLayout:$IncludeLayout
  foreach ($rec in $rows) {
    if (-not $rec.ID) {
      continue
    }

    if (-not $recordsById.ContainsKey($rec.ID)) {
      $recordsById[$rec.ID] = $rec
      continue
    }

    $existing = $recordsById[$rec.ID]
    if ($rec.SourceKind -eq "developing" -and $existing.SourceKind -ne "developing") {
      $recordsById[$rec.ID] = $rec
      continue
    }

    if ($rec.SourceKind -ne $existing.SourceKind) {
      continue
    }

    $existingHasStatus = -not [string]::IsNullOrWhiteSpace($existing.Status)
    $recHasStatus = -not [string]::IsNullOrWhiteSpace($rec.Status)
    if (-not $existingHasStatus -and $recHasStatus) {
      $recordsById[$rec.ID] = $rec
    }
  }
}

$all = $recordsById.Values

$sorted = $all | Sort-Object `
  @{ Expression = { $_.Area }; Ascending = $true }, `
  @{ Expression = { Get-PriorityKey $_.Priority }; Ascending = $true }, `
  @{ Expression = { $_.ID }; Ascending = $true }

$developed = $sorted | Where-Object { $_.Bucket -eq "developed" }
$developing = $sorted | Where-Object { $_.Bucket -eq "developing" }

$outAll = Join-Path $resolvedOutDir "zen_browser_merged_all.csv"
$outDeveloped = Join-Path $resolvedOutDir "zen_browser_merged_developed.csv"
$outDeveloping = Join-Path $resolvedOutDir "zen_browser_merged_developing.csv"
$outCounts = Join-Path $resolvedOutDir "zen_browser_merged_counts_by_area.csv"
$outAreas = Join-Path $resolvedOutDir "zen_browser_merged_areas.csv"

$schema = $sorted | Select-Object -First 1
Write-Csv -Path $outAll -Rows $sorted -SchemaRow $schema
Write-Csv -Path $outDeveloped -Rows $developed -SchemaRow $schema
Write-Csv -Path $outDeveloping -Rows $developing -SchemaRow $schema

$counts = $sorted | Group-Object -Property Bucket, Area | Sort-Object Name | ForEach-Object {
  $parts = $_.Name -split ','
  [PSCustomObject]@{
    Bucket = ($parts[0]).Trim()
    Area = ($parts[1]).Trim()
    Count = $_.Count
  }
}
$counts | Export-Csv -Path $outCounts -NoTypeInformation -Encoding UTF8

$areas = $sorted | Group-Object -Property Area | Sort-Object Name | ForEach-Object {
  $areaName = $_.Name
  $doneCount = @($developed | Where-Object { $_.Area -eq $areaName }).Count
  $todoCount = @($developing | Where-Object { $_.Area -eq $areaName }).Count
  [PSCustomObject]@{
    Area = $areaName
    Total = $_.Count
    Developed = $doneCount
    Developing = $todoCount
  }
}
$areas | Export-Csv -Path $outAreas -NoTypeInformation -Encoding UTF8

Write-Host "Merged: $($sorted.Count) total" -ForegroundColor Green
Write-Host "  Developed : $($developed.Count) -> $outDeveloped" -ForegroundColor Cyan
Write-Host "  Developing: $($developing.Count) -> $outDeveloping" -ForegroundColor Cyan
Write-Host "  All       : $outAll" -ForegroundColor Cyan
Write-Host "  Counts    : $outCounts" -ForegroundColor Cyan
Write-Host "  Areas     : $outAreas" -ForegroundColor Cyan
