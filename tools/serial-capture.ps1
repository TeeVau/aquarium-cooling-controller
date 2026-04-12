param(
    [string]$Port = "COM3",
    [int]$Baud = 115200,
    [int]$DurationSec = 180,
    [string]$OutputPath = "",
    [switch]$ToggleDtr
)

$serial = New-Object System.IO.Ports.SerialPort $Port, $Baud, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
$serial.NewLine = "`n"
$serial.ReadTimeout = 500
$serial.DtrEnable = $false
$serial.RtsEnable = $false

try {
    $serial.Open()

    if ($ToggleDtr) {
        $serial.DtrEnable = $true
        Start-Sleep -Milliseconds 200
        $serial.DtrEnable = $false
    }

    $writer = $null
    if ($OutputPath) {
        $outputDir = Split-Path -Parent $OutputPath
        if ($outputDir) {
            New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
        }
        $writer = [System.IO.StreamWriter]::new($OutputPath, $false, [System.Text.Encoding]::UTF8)
    }

    Write-Host "Serial monitor active on $Port at $Baud baud for $DurationSec s"
    $deadline = (Get-Date).AddSeconds($DurationSec)

    while ((Get-Date) -lt $deadline) {
        try {
            $line = $serial.ReadLine().TrimEnd("`r")
            Write-Host $line
            if ($writer) {
                $writer.WriteLine($line)
                $writer.Flush()
            }
        } catch [System.TimeoutException] {
            continue
        }
    }
}
finally {
    if ($writer) {
        $writer.Dispose()
    }

    if ($serial.IsOpen) {
        $serial.Close()
    }
}
