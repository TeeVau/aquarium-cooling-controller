param(
  [string]$Fqbn = "esp32:esp32:esp32",
  [string]$SketchDir = "firmware/controller",
  [string]$BuildRoot = ".arduino-build",
  [string]$OutputDir,
  [string]$ReleaseDir = "bin",
  [string]$LogDir,
  [int]$HeartbeatSeconds = 20,
  [int]$TailLinesOnError = 120,
  [string]$ArduinoCli
)

$ErrorActionPreference = "Stop"

if ($HeartbeatSeconds -lt 1) {
  Write-Error "HeartbeatSeconds muss mindestens 1 sein."
  exit 1
}

if ($TailLinesOnError -lt 1) {
  Write-Error "TailLinesOnError muss mindestens 1 sein."
  exit 1
}

function Normalize-ProcessPathVariables() {
  $processVariables = [System.Environment]::GetEnvironmentVariables("Process")
  if (-not ($processVariables.Contains("PATH") -and $processVariables.Contains("Path"))) {
    return
  }

  # Codex on Windows can expose both PATH and Path. Start-Process treats them
  # as duplicate keys, so normalize to the canonical Windows "Path" entry.
  $pathValue = [System.Environment]::GetEnvironmentVariable("Path", "Process")
  if (-not $pathValue) {
    $pathValue = [System.Environment]::GetEnvironmentVariable("PATH", "Process")
  }

  [System.Environment]::SetEnvironmentVariable("PATH", $null, "Process")
  [System.Environment]::SetEnvironmentVariable("Path", $pathValue, "Process")
}

function Resolve-ArduinoCliPath([string]$ExplicitPath) {
  if ($ExplicitPath) {
    $resolved = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ExplicitPath)
    if (-not (Test-Path -LiteralPath $resolved)) {
      Write-Error "arduino-cli wurde nicht gefunden: $resolved"
      exit 1
    }

    return $resolved
  }

  $command = Get-Command "arduino-cli" -ErrorAction SilentlyContinue
  if ($command) {
    return $command.Source
  }

  $fallback = "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
  if (Test-Path -LiteralPath $fallback) {
    return $fallback
  }

  Write-Error "arduino-cli wurde weder per Parameter, PATH noch Arduino-IDE-Standardpfad gefunden."
  exit 1
}

function Get-BuildPath([string]$BoardFqbn, [string]$Root) {
  $safeName = $BoardFqbn -replace '[^A-Za-z0-9._-]', '_'
  return Join-Path $Root $safeName
}

function Get-FirmwareVersion([string]$SketchFilePath) {
  if (-not (Test-Path -LiteralPath $SketchFilePath)) {
    Write-Error "Sketch-Datei nicht gefunden: $SketchFilePath"
    exit 1
  }

  $matches = Select-String -Path $SketchFilePath -Pattern '^\s*#define\s+AQ_FIRMWARE_VERSION\s+"([^"]+)"\s*$'
  if ($matches.Count -eq 0) {
    Write-Error "AQ_FIRMWARE_VERSION wurde in $SketchFilePath nicht gefunden."
    exit 1
  }

  if ($matches.Count -gt 1) {
    Write-Error "AQ_FIRMWARE_VERSION ist in $SketchFilePath mehrdeutig definiert."
    exit 1
  }

  return $matches[0].Matches[0].Groups[1].Value
}

function Format-Argument([string]$Value) {
  if ($null -eq $Value) {
    return '""'
  }

  if ($Value -notmatch '[\s"]') {
    return $Value
  }

  return '"' + ($Value -replace '"', '\"') + '"'
}

function Get-ArgumentString([string[]]$Arguments) {
  return (($Arguments | ForEach-Object { Format-Argument $_ }) -join " ")
}

function Write-LogTail([string]$Label, [string]$Path, [int]$TailLines) {
  if (-not (Test-Path -LiteralPath $Path)) {
    return
  }

  $content = Get-Content -LiteralPath $Path -Tail $TailLines -ErrorAction SilentlyContinue
  if ($null -eq $content -or $content.Count -eq 0) {
    return
  }

  Write-Host $Label
  $content | ForEach-Object { Write-Host $_ }
}

function Remove-FileIfPresent([string]$Path) {
  if (Test-Path -LiteralPath $Path) {
    Remove-Item -LiteralPath $Path -Force
  }
}

function Remove-DirectoryIfEmpty([string]$Path) {
  if (-not (Test-Path -LiteralPath $Path)) {
    return
  }

  $entries = Get-ChildItem -LiteralPath $Path -Force -ErrorAction SilentlyContinue
  if ($null -eq $entries -or $entries.Count -eq 0) {
    Remove-Item -LiteralPath $Path -Force
  }
}

function Remove-DirectoryIfWithinRoot([string]$Path, [string]$Root) {
  if (-not (Test-Path -LiteralPath $Path)) {
    return
  }

  $fullPath = [System.IO.Path]::GetFullPath($Path)
  $fullRoot = [System.IO.Path]::GetFullPath($Root)

  if (-not $fullRoot.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
    $fullRoot += [System.IO.Path]::DirectorySeparatorChar
  }

  if (-not $fullPath.StartsWith($fullRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    Write-Error "Pfad liegt ausserhalb des erwarteten Wurzelverzeichnisses: $fullPath"
    exit 1
  }

  Remove-Item -LiteralPath $Path -Recurse -Force
}

$repoRoot = Split-Path -Parent $PSScriptRoot
Normalize-ProcessPathVariables
$arduinoCliPath = Resolve-ArduinoCliPath -ExplicitPath $ArduinoCli
$sketchPath = Join-Path $repoRoot $SketchDir
$sketchArtifactPath = Join-Path $sketchPath "build"
$sketchName = Split-Path $sketchPath -Leaf
$sketchFilePath = Join-Path $sketchPath "$sketchName.ino"
$softwareVersion = Get-FirmwareVersion -SketchFilePath $sketchFilePath
$projectName = "aq-cooling-controller"
$buildRootPath = Join-Path $repoRoot $BuildRoot
$boardRootPath = Get-BuildPath -BoardFqbn $Fqbn -Root $buildRootPath
$outputDirPath = if ([string]::IsNullOrWhiteSpace($OutputDir)) {
  Join-Path $boardRootPath "output"
} else {
  Join-Path $repoRoot $OutputDir
}
$releaseDirPath = Join-Path $repoRoot $ReleaseDir
$logDirPath = if ([string]::IsNullOrWhiteSpace($LogDir)) {
  Join-Path $boardRootPath "logs"
} else {
  Join-Path $repoRoot $LogDir
}
$buildPath = $boardRootPath
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$stdoutLogFile = Join-Path $logDirPath "arduino-build-$timestamp.out.log"
$stderrLogFile = Join-Path $logDirPath "arduino-build-$timestamp.err.log"
$exitCodeFile = Join-Path $logDirPath "arduino-build-$timestamp.exitcode.txt"
$wrapperScriptFile = Join-Path $logDirPath "arduino-build-$timestamp.cmd"

if (-not (Test-Path -LiteralPath $sketchPath)) {
  Write-Error "Sketch-Pfad nicht gefunden: $sketchPath"
  exit 1
}

New-Item -ItemType Directory -Force -Path $buildPath | Out-Null
New-Item -ItemType Directory -Force -Path $outputDirPath | Out-Null
New-Item -ItemType Directory -Force -Path $releaseDirPath | Out-Null
New-Item -ItemType Directory -Force -Path $logDirPath | Out-Null

$compileArgs = @(
  "compile",
  "--fqbn", $Fqbn,
  "--build-path", $buildPath,
  "--output-dir", $outputDirPath
)

$compileArgs += $sketchPath
$argumentString = Get-ArgumentString -Arguments $compileArgs
$compileCommandLine = "{0} {1} 1>{2} 2>{3}" -f (Format-Argument $arduinoCliPath), $argumentString, (Format-Argument $stdoutLogFile), (Format-Argument $stderrLogFile)
$wrapperContent = @"
@echo off
$compileCommandLine
set EXIT_CODE=%ERRORLEVEL%
> $(Format-Argument $exitCodeFile) echo %EXIT_CODE%
exit /b %EXIT_CODE%
"@
Set-Content -LiteralPath $wrapperScriptFile -Value $wrapperContent -Encoding ASCII

Write-Host "[compile] Arduino CLI: $arduinoCliPath"
Write-Host "[compile] Sketch: $sketchPath"
Write-Host "[compile] FQBN: $Fqbn"
Write-Host "[compile] Firmware version: $softwareVersion"
Write-Host "[compile] Build path: $buildPath"
Write-Host "[compile] Output dir: $outputDirPath"
Write-Host "[compile] Release dir: $releaseDirPath"
Write-Host "[compile] Log stdout: $stdoutLogFile"
Write-Host "[compile] Log stderr: $stderrLogFile"
Write-Host "[compile] Starte Arduino-Compile..."

$process = Start-Process `
  -FilePath $wrapperScriptFile `
  -NoNewWindow `
  -WorkingDirectory $repoRoot `
  -PassThru

while (-not $process.HasExited) {
  Start-Sleep -Seconds $HeartbeatSeconds

  if (-not $process.HasExited) {
    Write-Host "[compile] laeuft noch... $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
  }
}

$process.WaitForExit()
$exitCode = $null
if (Test-Path -LiteralPath $exitCodeFile) {
  $parsedExitCode = Get-Content -LiteralPath $exitCodeFile -ErrorAction SilentlyContinue | Select-Object -Last 1
  if (-not [int]::TryParse($parsedExitCode, [ref]$exitCode)) {
    $exitCode = $null
  }
}

if ($null -eq $exitCode) {
  $process.Refresh()
  $exitCode = $process.ExitCode
}

if ($null -eq $exitCode) {
  $exitCode = 1
}

if ($exitCode -ne 0) {
  Write-Host "[compile] fehlgeschlagen."
  Write-Host "[compile] Exit-Code: $exitCode"
  Write-LogTail -Label "[compile] Letzte Standardausgabe:" -Path $stdoutLogFile -TailLines $TailLinesOnError
  Write-LogTail -Label "[compile] Letzte Fehlerausgabe:" -Path $stderrLogFile -TailLines $TailLinesOnError
  exit $exitCode
}

$firmwareFileName = "$sketchName.ino.bin"
$firmwarePath = Join-Path $outputDirPath $firmwareFileName
if (-not (Test-Path -LiteralPath $firmwarePath)) {
  Write-Error "Firmware-Datei nicht gefunden: $firmwarePath"
  exit 1
}

$versionedFirmwareName = "{0}-{1}.bin" -f $projectName, $softwareVersion
$versionedFirmwarePath = Join-Path $releaseDirPath $versionedFirmwareName
Copy-Item -LiteralPath $firmwarePath -Destination $versionedFirmwarePath -Force

Remove-FileIfPresent -Path $stdoutLogFile
Remove-FileIfPresent -Path $stderrLogFile
Remove-FileIfPresent -Path $exitCodeFile
Remove-FileIfPresent -Path $wrapperScriptFile
Remove-DirectoryIfEmpty -Path $logDirPath
Remove-DirectoryIfWithinRoot -Path $sketchArtifactPath -Root $sketchPath

Write-Host "[compile] erfolgreich abgeschlossen."
Write-Host "[compile] Raw firmware: $firmwarePath"
Write-Host "[compile] Versionierte Firmware: $versionedFirmwarePath"
