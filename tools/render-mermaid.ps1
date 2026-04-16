param(
  [ValidateSet("svg", "png")]
  [string[]] $Format = @("svg", "png"),

  [string] $OutputDir = "docs\design\rendered"
)

$ErrorActionPreference = "Stop"

$mmdc = Get-Command mmdc -ErrorAction SilentlyContinue
if (-not $mmdc) {
  throw "mmdc was not found. Install it with: npm install -g @mermaid-js/mermaid-cli"
}

$diagrams = @(
  "docs\design\controller-state-machine.mmd",
  "docs\design\controller-system-architecture.mmd",
  "docs\design\controller-cycle-sequence.mmd"
)

New-Item -ItemType Directory -Force $OutputDir | Out-Null

foreach ($diagram in $diagrams) {
  if (-not (Test-Path $diagram)) {
    throw "Diagram not found: $diagram"
  }

  $baseName = [System.IO.Path]::GetFileNameWithoutExtension($diagram)

  foreach ($extension in $Format) {
    $outputPath = Join-Path $OutputDir "$baseName.$extension"
    Write-Host "Rendering $diagram -> $outputPath"
    & $mmdc.Source -i $diagram -o $outputPath -b transparent

    if ($LASTEXITCODE -ne 0) {
      throw "mmdc failed for $diagram"
    }
  }
}

Write-Host "Rendered Mermaid diagrams to $OutputDir"
