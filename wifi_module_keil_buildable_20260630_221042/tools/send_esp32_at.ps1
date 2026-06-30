param(
    [string] $Port = "COM14",
    [int] $Baud = 115200,
    [string] $Command = "AT",
    [int] $WaitMs = 1500
)

$serial = New-Object System.IO.Ports.SerialPort $Port, $Baud, "None", 8, "One"
$serial.Handshake = "None"
$serial.DtrEnable = $false
$serial.RtsEnable = $false
$serial.ReadTimeout = 200
$serial.WriteTimeout = 1000

try {
    $serial.Open()
    Write-Output ("[OPEN] {0} @ {1}" -f $Port, $Baud)
    Start-Sleep -Milliseconds 1200
    $serial.DiscardInBuffer()
    $serial.DiscardOutBuffer()

    $payload = $Command + "`r`n"
    $serial.Write($payload)
    Write-Output ("[WRITE] {0} bytes: {1}" -f $payload.Length, $Command)
    Start-Sleep -Milliseconds $WaitMs

    $response = ""
    while ($true) {
        try {
            $response += $serial.ReadExisting()
            Start-Sleep -Milliseconds 100
            if ($serial.BytesToRead -eq 0) {
                break
            }
        } catch {
            break
        }
    }

    if ($response.Length -gt 0) {
        Write-Output $response
    } else {
        Write-Output "<no response>"
    }
} finally {
    if ($serial.IsOpen) {
        $serial.Close()
    }
}
