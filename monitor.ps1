# Otwiera monitor portu szeregowego WT32-ETH01.
# Użycie:  .\monitor.ps1                 (COM3, 115200)
#          .\monitor.ps1 -Port COM5      (inny port)
#          .\monitor.ps1 -Baud 9600      (inna prędkość)
# Wyjście z monitora: Ctrl+C

param(
    [string]$Port = "COM3",
    [int]$Baud = 115200
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

# Szukanie PlatformIO: w PATH albo w domyślnej lokalizacji
$pio = (Get-Command pio -ErrorAction SilentlyContinue).Source
if (-not $pio) {
    $pio = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\pio.exe"
    if (-not (Test-Path $pio)) {
        Write-Host "Nie znaleziono PlatformIO (pio)." -ForegroundColor Red
        exit 1
    }
}

if (-not ([System.IO.Ports.SerialPort]::GetPortNames() -contains $Port)) {
    Write-Host "Port $Port nie jest dostępny. Dostępne porty: $([System.IO.Ports.SerialPort]::GetPortNames() -join ', ')" -ForegroundColor Red
    exit 1
}

Write-Host "Monitor $Port @ $Baud (Ctrl+C - wyjście)" -ForegroundColor Cyan
# esp32_exception_decoder dekoduje ewentualne zrzuty błędów (backtrace) na nazwy funkcji
& $pio device monitor --port $Port --baud $Baud --filter direct --filter esp32_exception_decoder
