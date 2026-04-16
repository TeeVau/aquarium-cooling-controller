<#
.SYNOPSIS
Runs mosquitto_sub or mosquitto_pub for the aquarium controller without storing
MQTT credentials in the synced project workspace.

.DESCRIPTION
By default, this script prompts for the MQTT username and password on each run.
If -SaveCredential is used, the credential is stored with Windows DPAPI in
%LOCALAPPDATA%\aquarium-cooling-controller\mqtt, outside the OneDrive project.

Mosquitto's command line tools require the password as a process argument. This
script does not echo the command, write secrets to logs, or create secret files
inside the repository.
#>

[CmdletBinding()]
param(
    [ValidateSet("sub", "pub")]
    [string]$Mode = "sub",

    [Alias("Server")]
    [string]$BrokerHost = $env:AQ_MQTT_HOST,

    [int]$Port = $(if ($env:AQ_MQTT_PORT) { [int]$env:AQ_MQTT_PORT } else { 1883 }),

    [string]$RootTopic = $(if ($env:AQ_MQTT_ROOT_TOPIC) { $env:AQ_MQTT_ROOT_TOPIC } else { "aquarium/cooling" }),

    [string]$Topic = "",

    [string]$Message = "",

    [string]$Username = $env:AQ_MQTT_USERNAME,

    [string]$ClientId = "",

    [ValidateRange(0, 2)]
    [int]$Qos = 0,

    [int]$Count = 0,

    [switch]$Retain,

    [switch]$NoCredential,

    [switch]$SaveCredential,

    [switch]$ForgetCredential,

    [switch]$VerboseMqtt,

    [string]$CredentialName = "default"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-MosquittoTool {
    param([Parameter(Mandatory = $true)][string]$BaseName)

    $command = Get-Command "$BaseName.exe" -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $programFilesPath = Join-Path $env:ProgramFiles "mosquitto\$BaseName.exe"
    if (Test-Path -LiteralPath $programFilesPath) {
        return $programFilesPath
    }

    throw "$BaseName.exe was not found. Add C:\Program Files\mosquitto to PATH or reinstall the Mosquitto clients."
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

$credentialPath = Get-CredentialStorePath -Name $CredentialName

if ($ForgetCredential -and $PSBoundParameters.Count -eq 1) {
    if (Test-Path -LiteralPath $credentialPath) {
        Remove-Item -LiteralPath $credentialPath
        Write-Host "Removed stored MQTT credential: $credentialPath"
    } else {
        Write-Host "No stored MQTT credential found at: $credentialPath"
    }

    return
}

if ([string]::IsNullOrWhiteSpace($BrokerHost)) {
    $BrokerHost = Read-Host "MQTT broker host"
}

$rootTopicClean = $RootTopic.TrimEnd("/")
if ([string]::IsNullOrWhiteSpace($Topic)) {
    if ($Mode -eq "sub") {
        $Topic = "$rootTopicClean/#"
    } else {
        $Topic = "$rootTopicClean/diagnostic/windows_client_test"
    }
}

if ($Mode -eq "pub" -and [string]::IsNullOrWhiteSpace($Message)) {
    $Message = "windows mqtt client test $(Get-Date -Format "yyyy-MM-ddTHH:mm:ssK")"
}

if ($ForgetCredential) {
    if (Test-Path -LiteralPath $credentialPath) {
        Remove-Item -LiteralPath $credentialPath
        Write-Host "Removed stored MQTT credential: $credentialPath"
    } else {
        Write-Host "No stored MQTT credential found at: $credentialPath"
    }
}

$credential = $null
if (-not $NoCredential) {
    if (Test-Path -LiteralPath $credentialPath) {
        $credential = Import-Clixml -LiteralPath $credentialPath
        Write-Host "Using DPAPI-protected credential from: $credentialPath"
    } else {
        $credential = Read-MqttCredential -DefaultUsername $Username

        if ($SaveCredential) {
            $credentialDir = Split-Path -Parent $credentialPath
            New-Item -ItemType Directory -Force -Path $credentialDir | Out-Null
            $credential | Export-Clixml -LiteralPath $credentialPath
            Write-Host "Saved DPAPI-protected credential to: $credentialPath"
        }
    }
}

$toolName = if ($Mode -eq "sub") { "mosquitto_sub" } else { "mosquitto_pub" }
$toolPath = Resolve-MosquittoTool -BaseName $toolName
$mqttArgs = @("-h", $BrokerHost, "-p", $Port.ToString(), "-t", $Topic, "-q", $Qos.ToString())
$plainPassword = $null

try {
    if (-not [string]::IsNullOrWhiteSpace($ClientId)) {
        $mqttArgs += @("-i", $ClientId)
    }

    if ($credential) {
        $plainPassword = Convert-SecureStringToPlainText -SecureString $credential.Password
        $mqttArgs += @("-u", $credential.UserName, "-P", $plainPassword)
    }

    if ($VerboseMqtt) {
        $mqttArgs += "-d"
    }

    if ($Mode -eq "sub") {
        if ($Count -gt 0) {
            $mqttArgs += @("-C", $Count.ToString())
        }

        $mqttArgs += "-v"
        Write-Host "Subscribing to '$Topic' on $BrokerHost`:$Port. Press Ctrl+C to stop."
    } else {
        if ($Retain) {
            $mqttArgs += "-r"
        }

        $mqttArgs += @("-m", $Message)
        Write-Host "Publishing to '$Topic' on $BrokerHost`:$Port."
    }

    & $toolPath @mqttArgs
    exit $LASTEXITCODE
}
finally {
    $plainPassword = $null
}
