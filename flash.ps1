# Kompiluje i wgrywa firmware na WT32-ETH01.
# Użycie:  .\flash.ps1                 (COM3)
#          .\flash.ps1 -Port COM5      (inny port)
#          .\flash.ps1 -Monitor        (po wgraniu otwiera monitor portu)
#          .\flash.ps1 -Erase          (najpierw kasuje całą pamięć flash, w tym ustawienia)

param(
    [string]$Port = "COM3",
    [switch]$Monitor,
    [switch]$Erase
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

Write-Host "WT32-ETH01: zewrzyj GPIO0 z GND i zresetuj płytkę (tryb bootloadera)." -ForegroundColor Yellow

if ($Erase) {
    Write-Host "Kasowanie pamięci flash na $Port..." -ForegroundColor Cyan
    & $pio run -t erase --upload-port $Port
    if ($LASTEXITCODE -ne 0) { Write-Host "Kasowanie nie powiodło się." -ForegroundColor Red; exit $LASTEXITCODE }
}

Write-Host "Kompilacja i wgrywanie na $Port..." -ForegroundColor Cyan
& $pio run -t upload --upload-port $Port
if ($LASTEXITCODE -ne 0) {
    Write-Host "Wgrywanie nie powiodło się." -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "Gotowe. Rozewrzyj GPIO0 i zresetuj płytkę." -ForegroundColor Green

if ($Monitor) {
    & $pio device monitor --port $Port --baud 115200
}
