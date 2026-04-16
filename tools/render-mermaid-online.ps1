param(
  [ValidateSet("svg", "png")]
  [string[]] $Format = @("svg", "png"),

  [string] $OutputDir = "docs\design\rendered"
)

$ErrorActionPreference = "Stop"

function ConvertTo-Base64Url {
  param([string] $Text)

  $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
  $base64 = [Convert]::ToBase64String($bytes)
  return $base64.TrimEnd("=").Replace("+", "-").Replace("/", "_")
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

  $source = Get-Content -Raw $diagram
  $encoded = ConvertTo-Base64Url $source
  $baseName = [System.IO.Path]::GetFileNameWithoutExtension($diagram)

  foreach ($extension in $Format) {
    $outputPath = Join-Path $OutputDir "$baseName.$extension"
    $uri = if ($extension -eq "svg") { "https://mermaid.ink/svg/$encoded" } else { "https://mermaid.ink/img/$encoded" }

    Write-Host "Rendering $diagram -> $outputPath"
    if ($extension -eq "png") {
      Add-Type -AssemblyName System.Drawing
      $tempRasterPath = Join-Path $OutputDir "$baseName.render-tmp"
      Invoke-WebRequest -Uri $uri -OutFile $tempRasterPath
      $image = [System.Drawing.Image]::FromFile((Resolve-Path $tempRasterPath))
      try {
        $image.Save((Join-Path (Resolve-Path $OutputDir) "$baseName.png"),
                    [System.Drawing.Imaging.ImageFormat]::Png)
      } finally {
        $image.Dispose()
        Remove-Item -LiteralPath $tempRasterPath -Force
      }
    } else {
      Invoke-WebRequest -Uri $uri -OutFile $outputPath
    }

    if (-not (Test-Path $outputPath) -or ((Get-Item $outputPath).Length -eq 0)) {
      throw "Online render produced an empty file: $outputPath"
    }
  }
}

Write-Host "Rendered Mermaid diagrams to $OutputDir via mermaid.ink"
