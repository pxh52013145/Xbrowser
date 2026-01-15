param(
  [string]$ReferenceDir = "",
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

function Normalize-Row($Row, [string]$SourceCsv) {
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

$repoRoot = Get-RepoRoot

if ([string]::IsNullOrWhiteSpace($ReferenceDir)) {
  $ReferenceDir = Join-Path $repoRoot "reference"
}
if ([string]::IsNullOrWhiteSpace($OutDir)) {
  $OutDir = $ReferenceDir
}

if (!(Test-Path $ReferenceDir)) {
  throw "Reference dir not found: $ReferenceDir"
}
if (!(Test-Path $OutDir)) {
  New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
}

$planFiles = @(
  "zen_browser_feature_impl_plan.csv",
  "zen_browser_interaction_plan.csv",
  "zen_browser_interaction_plan_v2.csv",
  "zen_browser_interaction_plan_v3.csv",
  "zen_browser_next_plan_v4.csv"
)

$layoutFiles = @("zen_browser_feature_layout.csv")

$recordsById = @{}

foreach ($name in $planFiles) {
  $path = Join-Path $ReferenceDir $name
  if (!(Test-Path $path)) {
    Write-Warning "Missing: $path"
    continue
  }
  $rows = Import-Csv -Path $path
  foreach ($row in $rows) {
    if (-not $row.ID) {
      continue
    }
    $rec = Normalize-Row $row $name
    if (-not $recordsById.ContainsKey($rec.ID)) {
      $recordsById[$rec.ID] = $rec
      continue
    }

    $existing = $recordsById[$rec.ID]
    $existingHasStatus = -not [string]::IsNullOrWhiteSpace($existing.Status)
    $recHasStatus = -not [string]::IsNullOrWhiteSpace($rec.Status)

    if (-not $existingHasStatus -and $recHasStatus) {
      $recordsById[$rec.ID] = $rec
    }
  }
}

if ($IncludeLayout) {
  foreach ($name in $layoutFiles) {
    $path = Join-Path $ReferenceDir $name
    if (!(Test-Path $path)) {
      Write-Warning "Missing: $path"
      continue
    }
    $rows = Import-Csv -Path $path
    foreach ($row in $rows) {
      if (-not $row.ID) {
        continue
      }
      if ($recordsById.ContainsKey([string]$row.ID)) {
        continue
      }
      $rec = Normalize-Row $row $name
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

$outAll = Join-Path $OutDir "zen_browser_merged_all.csv"
$outDeveloped = Join-Path $OutDir "zen_browser_merged_developed.csv"
$outDeveloping = Join-Path $OutDir "zen_browser_merged_developing.csv"
$outCounts = Join-Path $OutDir "zen_browser_merged_counts_by_area.csv"

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

Write-Host "Merged: $($sorted.Count) total" -ForegroundColor Green
Write-Host "  Developed : $($developed.Count) -> $outDeveloped" -ForegroundColor Cyan
Write-Host "  Developing: $($developing.Count) -> $outDeveloping" -ForegroundColor Cyan
Write-Host "  All       : $outAll" -ForegroundColor Cyan
Write-Host "  Counts    : $outCounts" -ForegroundColor Cyan
