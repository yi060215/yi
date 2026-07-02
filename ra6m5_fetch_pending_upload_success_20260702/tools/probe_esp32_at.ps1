param(
    [string] $Port = "COM14",
    [int] $Baud = 115200,
    [int] $WaitMs = 1200
)

$cases = @(
    @{Dtr = $false; Rts = $false},
    @{Dtr = $false; Rts = $true},
    @{Dtr = $true;  Rts = $false},
    @{Dtr = $true;  Rts = $true}
)

foreach ($case in $cases) {
    $serial = New-Object System.IO.Ports.SerialPort $Port, $Baud, "None", 8, "One"
    $serial.Handshake = "None"
    $serial.DtrEnable = $case.Dtr
    $serial.RtsEnable = $case.Rts
    $serial.ReadTimeout = 200
    $serial.WriteTimeout = 1000

    try {
        $serial.Open()
        Start-Sleep -Milliseconds 800
        $serial.DiscardInBuffer()
        $serial.DiscardOutBuffer()
        $serial.Write("AT`r`n")
        Start-Sleep -Milliseconds $WaitMs

        $response = $serial.ReadExisting()
        if ($response.Length -gt 0) {
            $escaped = $response.Replace("`r", "<CR>").Replace("`n", "<LF>")
            Write-Output ("[DTR={0},RTS={1}] {2}" -f $case.Dtr, $case.Rts, $escaped)
        } else {
            Write-Output ("[DTR={0},RTS={1}] <no response>" -f $case.Dtr, $case.Rts)
        }
    } catch {
        Write-Output ("[DTR={0},RTS={1}] ERROR: {2}" -f $case.Dtr, $case.Rts, $_.Exception.Message)
    } finally {
        if ($serial.IsOpen) {
            $serial.Close()
        }
    }
}
