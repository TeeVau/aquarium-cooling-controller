[CmdletBinding()]
param(
  [string]$FirmwarePath,
  [string]$BrokerHost = $env:AQ_MQTT_HOST,
  [int]$Port = $(if ($env:AQ_MQTT_PORT) { [int]$env:AQ_MQTT_PORT } else { 0 }),
  [string]$RootTopic = $env:AQ_MQTT_ROOT_TOPIC,
  [string]$Username = $env:AQ_MQTT_USERNAME,
  [string]$CredentialName = "default",
  [switch]$NoCredential,
  [switch]$SaveCredential,
  [int]$UrlWaitTimeoutSec = 60,
  [int]$VerifyTimeoutSec = 180,
  [string]$UploadUrl,
  [switch]$SkipVerify
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($UrlWaitTimeoutSec -lt 1) {
  throw "UrlWaitTimeoutSec must be at least 1."
}

if ($VerifyTimeoutSec -lt 1) {
  throw "VerifyTimeoutSec must be at least 1."
}

function Resolve-ToolPath {
  param(
    [Parameter(Mandatory = $true)][string]$CommandName,
    [string]$ProgramFilesFallback
  )

  $command = Get-Command $CommandName -ErrorAction SilentlyContinue
  if ($command) {
    return $command.Source
  }

  if (-not [string]::IsNullOrWhiteSpace($ProgramFilesFallback)) {
    $fallbackPath = Join-Path $env:ProgramFiles $ProgramFilesFallback
    if (Test-Path -LiteralPath $fallbackPath) {
      return $fallbackPath
    }
  }

  throw "$CommandName was not found."
}

function Get-CredentialStorePath {
  param([Parameter(Mandatory = $true)][string]$Name)

  $safeName = $Name -replace "[^A-Za-z0-9._-]", "_"
  $storeDir = Join-Path $env:LOCALAPPDATA "aquarium-cooling-controller\mqtt"
  Join-Path $storeDir "$safeName.credential.xml"
}

function Convert-SecureStringToPlainText {
  param([Parameter(Mandatory = $true)][securestring]$SecureString)

  $bstr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($SecureString)
  try {
    [Runtime.InteropServices.Marshal]::PtrToStringBSTR($bstr)
  }
  finally {
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($bstr)
  }
}

function Read-MqttCredential {
  param([string]$DefaultUsername)

  $promptUsername = $DefaultUsername
  if ([string]::IsNullOrWhiteSpace($promptUsername)) {
    $promptUsername = Read-Host "MQTT username"
  }

  $promptPassword = Read-Host "MQTT password" -AsSecureString
  [pscredential]::new($promptUsername, $promptPassword)
}

function Get-ConfigMacroValue {
  param(
    [Parameter(Mandatory = $true)][string]$Path,
    [Parameter(Mandatory = $true)][string]$MacroName
  )

  if (-not (Test-Path -LiteralPath $Path)) {
    return $null
  }

  $pattern = '^\s*#define\s+' + [regex]::Escape($MacroName) + '\s+(.+?)\s*$'
  $matches = @(Select-String -Path $Path -Pattern $pattern)
  if ($matches.Count -eq 0) {
    return $null
  }

  $rawValue = $matches[-1].Matches[0].Groups[1].Value.Trim()
  if ($rawValue.StartsWith('"') -and $rawValue.EndsWith('"')) {
    return $rawValue.Trim('"')
  }

  return $rawValue
}

function Resolve-ConfigValue {
  param(
    [AllowEmptyString()][string]$CurrentValue,
    [Parameter(Mandatory = $true)][string]$MacroName,
    [Parameter(Mandatory = $true)][string[]]$ConfigPaths,
    [string]$FallbackValue = ""
  )

  if (-not [string]::IsNullOrWhiteSpace($CurrentValue)) {
    return $CurrentValue
  }

  foreach ($configPath in $ConfigPaths) {
    $value = Get-ConfigMacroValue -Path $configPath -MacroName $MacroName
    if (-not [string]::IsNullOrWhiteSpace($value)) {
      return $value
    }
  }

  return $FallbackValue
}

function Get-NewestFirmwareFile {
  param([Parameter(Mandatory = $true)][string]$BinDirectory)

  $candidate = Get-ChildItem -LiteralPath $BinDirectory -Filter "aq-cooling-controller-*.bin" -File |
    Sort-Object -Property @(
      @{ Expression = "LastWriteTimeUtc"; Descending = $true },
      @{ Expression = "Name"; Descending = $true }
    ) |
    Select-Object -First 1

  if ($null -eq $candidate) {
    throw "No versioned firmware BIN matching 'aq-cooling-controller-*.bin' was found in $BinDirectory."
  }

  return $candidate.FullName
}

function Get-FirmwareVersionFromPath {
  param([Parameter(Mandatory = $true)][string]$Path)

  $fileName = Split-Path -Leaf $Path
  $match = [regex]::Match($fileName, '^aq-cooling-controller-(.+)\.bin$')
  if (-not $match.Success) {
    throw "Firmware filename does not match the expected pattern: $fileName"
  }

  return $match.Groups[1].Value
}

function Format-CommandArgument {
  param([AllowNull()][string]$Value)

  if ($null -eq $Value) {
    return '""'
  }

  if ($Value -notmatch '[\s"]') {
    return $Value
  }

  return '"' + ($Value -replace '"', '\"') + '"'
}

function Join-CommandArguments {
  param([Parameter(Mandatory = $true)][string[]]$Arguments)

  return (($Arguments | ForEach-Object { Format-CommandArgument $_ }) -join " ")
}

function Invoke-ProcessCapture {
  param(
    [Parameter(Mandatory = $true)][string]$FilePath,
    [Parameter(Mandatory = $true)][string[]]$Arguments
  )

  $startInfo = New-Object System.Diagnostics.ProcessStartInfo
  $startInfo.FileName = $FilePath
  $startInfo.Arguments = Join-CommandArguments -Arguments $Arguments
  $startInfo.UseShellExecute = $false
  $startInfo.RedirectStandardOutput = $true
  $startInfo.RedirectStandardError = $true
  $startInfo.CreateNoWindow = $true

  $process = New-Object System.Diagnostics.Process
  $process.StartInfo = $startInfo
  [void]$process.Start()
  $stdout = $process.StandardOutput.ReadToEnd()
  $stderr = $process.StandardError.ReadToEnd()
  $process.WaitForExit()

  $combinedOutput = @()
  if (-not [string]::IsNullOrEmpty($stdout)) {
    $combinedOutput += ($stdout -split "\r?\n")
  }

  if (-not [string]::IsNullOrEmpty($stderr)) {
    $combinedOutput += ($stderr -split "\r?\n")
  }

  return [pscustomobject]@{
    ExitCode = $process.ExitCode
    Output = @($combinedOutput | Where-Object { $_ -ne "" })
    StdOut = $stdout
    StdErr = $stderr
  }
}

function Join-Topic {
  param(
    [Parameter(Mandatory = $true)][string]$Root,
    [Parameter(Mandatory = $true)][string]$Suffix
  )

  return ($Root.TrimEnd('/') + $Suffix)
}

function Test-OtaUploadUrl {
  param([string]$Value)

  if ([string]::IsNullOrWhiteSpace($Value)) {
    return $false
  }

  $uri = $null
  if (-not [Uri]::TryCreate($Value, [UriKind]::Absolute, [ref]$uri)) {
    return $false
  }

  return (($uri.Scheme -eq "http") -or ($uri.Scheme -eq "https")) -and ($uri.AbsolutePath -eq "/update")
}

function Build-MqttArgsBase {
  param(
    [Parameter(Mandatory = $true)][string]$BrokerHostValue,
    [Parameter(Mandatory = $true)][int]$BrokerPort,
    [AllowEmptyString()][string]$PlainPasswordValue = "",
    [pscredential]$Credential,
    [switch]$UseCredential
  )

  $args = @("-h", $BrokerHostValue, "-p", $BrokerPort.ToString())
  if ($UseCredential -and $null -ne $Credential) {
    $args += @("-u", $Credential.UserName, "-P", $PlainPasswordValue)
  }

  return $args
}

function Invoke-MqttPublish {
  param(
    [Parameter(Mandatory = $true)][string]$ToolPath,
    [Parameter(Mandatory = $true)][string[]]$BaseArgs,
    [Parameter(Mandatory = $true)][string]$Topic,
    [Parameter(Mandatory = $true)][string]$Message
  )

  $args = @($BaseArgs + @("-t", $Topic, "-m", $Message, "-q", "0"))
  $result = Invoke-ProcessCapture -FilePath $ToolPath -Arguments $args

  return [pscustomobject]@{
    ExitCode = $result.ExitCode
    Output = @($result.Output)
  }
}

function Get-MqttRetainedValue {
  param(
    [Parameter(Mandatory = $true)][string]$ToolPath,
    [Parameter(Mandatory = $true)][string[]]$BaseArgs,
    [Parameter(Mandatory = $true)][string]$Topic,
    [Parameter(Mandatory = $true)][int]$TimeoutSec
  )

  $args = @($BaseArgs + @("-t", $Topic, "-C", "1", "-W", $TimeoutSec.ToString(), "-v"))
  $result = Invoke-ProcessCapture -FilePath $ToolPath -Arguments $args

  if ($result.ExitCode -ne 0) {
    return [pscustomobject]@{
      Topic = $Topic
      Value = $null
      ExitCode = $result.ExitCode
      Output = @($result.Output)
    }
  }

  $value = $null
  foreach ($line in $result.Output) {
    $text = [string]$line
    if ($text.StartsWith("$Topic ")) {
      $value = $text.Substring($Topic.Length + 1)
    } elseif ($text -eq $Topic) {
      $value = ""
    }
  }

  return [pscustomobject]@{
    Topic = $Topic
    Value = $value
    ExitCode = $result.ExitCode
    Output = @($result.Output)
  }
}

function Get-MqttStatusSnapshot {
  param(
    [Parameter(Mandatory = $true)][string]$ToolPath,
    [Parameter(Mandatory = $true)][string[]]$BaseArgs,
    [Parameter(Mandatory = $true)][string]$RootTopicValue,
    [Parameter(Mandatory = $true)][int]$TimeoutSec
  )

  $suffixes = [ordered]@{
    ota_window_active = "/status/ota_window_active"
    ota_upload_url = "/status/ota_upload_url"
    ota_state = "/status/ota_state"
    ota_message = "/status/ota_message"
    network_ip = "/status/network_ip"
    firmware_version = "/status/firmware_version"
    availability = "/status/availability"
  }

  $snapshot = [ordered]@{}
  foreach ($entry in $suffixes.GetEnumerator()) {
    $topic = Join-Topic -Root $RootTopicValue -Suffix $entry.Value
    $result = Get-MqttRetainedValue -ToolPath $ToolPath -BaseArgs $BaseArgs -Topic $topic -TimeoutSec $TimeoutSec
    $snapshot[$entry.Key] = $result.Value
    $snapshot["$($entry.Key)_topic"] = $topic
    $snapshot["$($entry.Key)_exit_code"] = $result.ExitCode
    $snapshot["$($entry.Key)_output"] = @($result.Output)
  }

  return [pscustomobject]$snapshot
}

function Write-Diagnostics {
  param([Parameter(Mandatory = $true)]$Snapshot)

  Write-Host "[ota] Latest MQTT diagnostics:"
  Write-Host "[ota]   ota_window_active: $($Snapshot.ota_window_active)"
  Write-Host "[ota]   ota_upload_url: $($Snapshot.ota_upload_url)"
  Write-Host "[ota]   ota_state: $($Snapshot.ota_state)"
  Write-Host "[ota]   ota_message: $($Snapshot.ota_message)"
  Write-Host "[ota]   network_ip: $($Snapshot.network_ip)"
  Write-Host "[ota]   firmware_version: $($Snapshot.firmware_version)"
  Write-Host "[ota]   availability: $($Snapshot.availability)"

  foreach ($key in @("ota_window_active", "ota_upload_url", "ota_state", "ota_message", "network_ip", "firmware_version", "availability")) {
    $exitCodeProperty = "${key}_exit_code"
    $outputProperty = "${key}_output"
    $topicProperty = "${key}_topic"
    $exitCode = $Snapshot.$exitCodeProperty
    if (($null -ne $exitCode) -and ($exitCode -ne 0)) {
      $tail = @(Get-LastLines -Lines @($Snapshot.$outputProperty | ForEach-Object { [string]$_ }))
      $tailText = if ($tail.Count -gt 0) { $tail -join " | " } else { "no additional output" }
      Write-Host "[ota]   $($Snapshot.$topicProperty) read failed (exit $exitCode): $tailText"
    }
  }
}

function Get-LastLines {
  param(
    [Parameter(Mandatory = $true)][string[]]$Lines,
    [int]$Count = 20
  )

  $normalizedLines = @($Lines | ForEach-Object { [string]$_ })

  if ($normalizedLines.Count -le $Count) {
    return $normalizedLines
  }

  return $normalizedLines[($normalizedLines.Count - $Count)..($normalizedLines.Count - 1)]
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$binDirectory = Join-Path $repoRoot "bin"
$configPaths = @(
  (Join-Path $repoRoot "firmware/controller/network_config.local.h"),
  (Join-Path $repoRoot "firmware/controller/network_config.h")
)

$resolvedFirmwarePath = if ([string]::IsNullOrWhiteSpace($FirmwarePath)) {
  Get-NewestFirmwareFile -BinDirectory $binDirectory
} else {
  $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($FirmwarePath)
}

if (-not (Test-Path -LiteralPath $resolvedFirmwarePath)) {
  throw "Firmware BIN not found: $resolvedFirmwarePath"
}

$targetVersion = Get-FirmwareVersionFromPath -Path $resolvedFirmwarePath
$resolvedRootTopic = Resolve-ConfigValue -CurrentValue $RootTopic -MacroName "AQ_MQTT_ROOT_TOPIC" -ConfigPaths $configPaths -FallbackValue "aquarium/cooling"
$resolvedBrokerHost = Resolve-ConfigValue -CurrentValue $BrokerHost -MacroName "AQ_MQTT_HOST" -ConfigPaths $configPaths
$resolvedUsername = Resolve-ConfigValue -CurrentValue $Username -MacroName "AQ_MQTT_USERNAME" -ConfigPaths $configPaths

if ($Port -le 0) {
  $resolvedPortText = Resolve-ConfigValue -CurrentValue "" -MacroName "AQ_MQTT_PORT" -ConfigPaths $configPaths -FallbackValue "1883"
  $resolvedPort = [int]$resolvedPortText
} else {
  $resolvedPort = $Port
}

$mqttRequired = [string]::IsNullOrWhiteSpace($UploadUrl) -or (-not $SkipVerify)
if ($mqttRequired -and [string]::IsNullOrWhiteSpace($resolvedBrokerHost)) {
  throw "MQTT broker host could not be resolved from parameters, environment, or firmware config."
}

$mosquittoPubPath = $null
$mosquittoSubPath = $null
$credential = $null
$plainPassword = $null
$mqttBaseArgs = @()

try {
  if ($mqttRequired) {
    $mosquittoPubPath = Resolve-ToolPath -CommandName "mosquitto_pub.exe" -ProgramFilesFallback "mosquitto\mosquitto_pub.exe"
    $mosquittoSubPath = Resolve-ToolPath -CommandName "mosquitto_sub.exe" -ProgramFilesFallback "mosquitto\mosquitto_sub.exe"

    if (-not $NoCredential) {
      $credentialPath = Get-CredentialStorePath -Name $CredentialName
      if (Test-Path -LiteralPath $credentialPath) {
        $credential = Import-Clixml -LiteralPath $credentialPath
        Write-Host "[ota] Using DPAPI-protected credential from: $credentialPath"
      } else {
        $credential = Read-MqttCredential -DefaultUsername $resolvedUsername
        if ($SaveCredential) {
          $credentialDir = Split-Path -Parent $credentialPath
          New-Item -ItemType Directory -Force -Path $credentialDir | Out-Null
          $credential | Export-Clixml -LiteralPath $credentialPath
          Write-Host "[ota] Saved DPAPI-protected credential to: $credentialPath"
        }
      }

      $plainPassword = Convert-SecureStringToPlainText -SecureString $credential.Password
    }

    $mqttBaseArgs = Build-MqttArgsBase -BrokerHostValue $resolvedBrokerHost -BrokerPort $resolvedPort -PlainPasswordValue $plainPassword -Credential $credential -UseCredential:(-not $NoCredential)
  }

  $curlPath = Resolve-ToolPath -CommandName "curl.exe"

  Write-Host "[ota] Firmware: $resolvedFirmwarePath"
  Write-Host "[ota] Target version: $targetVersion"
  if ($mqttRequired) {
    Write-Host "[ota] Broker: $resolvedBrokerHost`:$resolvedPort"
    Write-Host "[ota] Root topic: $resolvedRootTopic"
  }

  $currentSnapshot = $null
  $resolvedUploadUrl = $UploadUrl
  if ([string]::IsNullOrWhiteSpace($resolvedUploadUrl)) {
    $currentSnapshot = Get-MqttStatusSnapshot -ToolPath $mosquittoSubPath -BaseArgs $mqttBaseArgs -RootTopicValue $resolvedRootTopic -TimeoutSec 5
    if (($currentSnapshot.ota_window_active -eq "true") -and (Test-OtaUploadUrl -Value $currentSnapshot.ota_upload_url)) {
      $resolvedUploadUrl = $currentSnapshot.ota_upload_url
      Write-Host "[ota] Reusing active OTA upload URL: $resolvedUploadUrl"
    } else {
      $enableTopic = Join-Topic -Root $resolvedRootTopic -Suffix "/set/ota_enable"
      Write-Host "[ota] Enabling OTA upload window via MQTT..."
      $publishResult = Invoke-MqttPublish -ToolPath $mosquittoPubPath -BaseArgs $mqttBaseArgs -Topic $enableTopic -Message "true"
      if ($publishResult.ExitCode -ne 0) {
        $currentSnapshot = Get-MqttStatusSnapshot -ToolPath $mosquittoSubPath -BaseArgs $mqttBaseArgs -RootTopicValue $resolvedRootTopic -TimeoutSec 3
        Write-Diagnostics -Snapshot $currentSnapshot
        $publishTail = Get-LastLines -Lines $publishResult.Output
        throw "Failed to publish OTA enable command to $enableTopic.`n$($publishTail -join [Environment]::NewLine)"
      }

      $deadline = (Get-Date).AddSeconds($UrlWaitTimeoutSec)
      while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 2
        $currentSnapshot = Get-MqttStatusSnapshot -ToolPath $mosquittoSubPath -BaseArgs $mqttBaseArgs -RootTopicValue $resolvedRootTopic -TimeoutSec 5
        if (($currentSnapshot.ota_window_active -eq "true") -and (Test-OtaUploadUrl -Value $currentSnapshot.ota_upload_url)) {
          $resolvedUploadUrl = $currentSnapshot.ota_upload_url
          break
        }
      }

      if ([string]::IsNullOrWhiteSpace($resolvedUploadUrl)) {
        Write-Diagnostics -Snapshot $currentSnapshot
        throw "Timed out waiting for the OTA upload URL."
      }
    }
  } else {
    Write-Host "[ota] Using manual upload URL override: $resolvedUploadUrl"
  }

  if (-not (Test-OtaUploadUrl -Value $resolvedUploadUrl)) {
    throw "UploadUrl is not a valid OTA update URL: $resolvedUploadUrl"
  }

  $curlArgs = @("-s", "-S", "-i", "-F", "firmware=@$resolvedFirmwarePath", $resolvedUploadUrl)
  Write-Host "[ota] Uploading firmware..."
  $curlResult = Invoke-ProcessCapture -FilePath $curlPath -Arguments $curlArgs
  $curlExitCode = $curlResult.ExitCode
  $curlLines = @($curlResult.Output | ForEach-Object { [string]$_ })
  $curlText = $curlLines -join [Environment]::NewLine
  $statusMatches = [regex]::Matches($curlText, 'HTTP/\d+(?:\.\d+)?\s+(\d{3})')
  $httpStatus = $null
  if ($statusMatches.Count -gt 0) {
    $httpStatus = [int]$statusMatches[$statusMatches.Count - 1].Groups[1].Value
  }

  if (($curlExitCode -ne 0) -or ($null -eq $httpStatus) -or ($httpStatus -lt 200) -or ($httpStatus -ge 300)) {
    if ($mqttRequired -and $null -eq $currentSnapshot) {
      $currentSnapshot = Get-MqttStatusSnapshot -ToolPath $mosquittoSubPath -BaseArgs $mqttBaseArgs -RootTopicValue $resolvedRootTopic -TimeoutSec 5
    }

    if ($null -ne $currentSnapshot) {
      Write-Diagnostics -Snapshot $currentSnapshot
    }

    $curlTail = Get-LastLines -Lines $curlLines
    throw "OTA upload failed. curl exit=$curlExitCode http_status=$httpStatus`n$($curlTail -join [Environment]::NewLine)"
  }

  Write-Host "[ota] Upload accepted with HTTP status $httpStatus."
  if ($SkipVerify) {
    Write-Host "[ota] Verification skipped."
    return
  }

  Write-Host "[ota] Waiting for MQTT verification..."
  $verifyDeadline = (Get-Date).AddSeconds($VerifyTimeoutSec)
  $verifySnapshot = $null
  while ((Get-Date) -lt $verifyDeadline) {
    Start-Sleep -Seconds 5
    $verifySnapshot = Get-MqttStatusSnapshot -ToolPath $mosquittoSubPath -BaseArgs $mqttBaseArgs -RootTopicValue $resolvedRootTopic -TimeoutSec 5
    if (($verifySnapshot.firmware_version -eq $targetVersion) -and ($verifySnapshot.availability -eq "online")) {
      Write-Diagnostics -Snapshot $verifySnapshot
      Write-Host "[ota] OTA verification succeeded."
      return
    }
  }

  if ($null -ne $verifySnapshot) {
    Write-Diagnostics -Snapshot $verifySnapshot
  }

  throw "Timed out waiting for OTA verification. Expected firmware_version=$targetVersion and availability=online."
}
finally {
  $plainPassword = $null
}
