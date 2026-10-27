# Podbija wersję firmware (FW_VERSION) i kompiluje projekt.
# Użycie:  .\build.ps1             1.2.3 -> 1.2.4  (poprawka)
#          .\build.ps1 -Minor      1.2.3 -> 1.3.0  (nowa funkcja)
#          .\build.ps1 -Major      1.2.3 -> 2.0.0
#          .\build.ps1 -NoBump     kompilacja bez zmiany wersji
# Gdy kompilacja się nie powiedzie, wersja wraca do poprzedniej.

param(
    [switch]$Minor,
    [switch]$Major,
    [switch]$NoBump
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$mainPath   = Join-Path $PSScriptRoot "src\main.cpp"
$readmePath = Join-Path $PSScriptRoot "README.md"
$binPath    = Join-Path $PSScriptRoot ".pio\build\wt32-eth01\firmware.bin"
$utf8       = New-Object System.Text.UTF8Encoding $false

# Szukanie PlatformIO: w PATH albo w domyślnej lokalizacji
$pio = (Get-Command pio -ErrorAction SilentlyContinue).Source
if (-not $pio) {
    $pio = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\pio.exe"
    if (-not (Test-Path $pio)) {
        Write-Host "Nie znaleziono PlatformIO (pio)." -ForegroundColor Red
        exit 1
    }
}

$mainOrig   = [IO.File]::ReadAllText($mainPath)
$readmeOrig = if (Test-Path $readmePath) { [IO.File]::ReadAllText($readmePath) } else { $null }

$m = [regex]::Match($mainOrig, '#define FW_VERSION "(\d+)\.(\d+)\.(\d+)"')
if (-not $m.Success) {
    Write-Host "Nie znaleziono '#define FW_VERSION ""x.y.z""' w src/main.cpp." -ForegroundColor Red
    exit 1
}
$old = "$($m.Groups[1].Value).$($m.Groups[2].Value).$($m.Groups[3].Value)"
[int]$ma = $m.Groups[1].Value; [int]$mi = $m.Groups[2].Value; [int]$pa = $m.Groups[3].Value

if ($NoBump)     { $new = $old }
elseif ($Major)  { $new = "$($ma + 1).0.0" }
elseif ($Minor)  { $new = "$ma.$($mi + 1).0" }
else             { $new = "$ma.$mi.$($pa + 1)" }

if ($new -ne $old) {
    $oldEsc = [regex]::Escape($old)
    [IO.File]::WriteAllText($mainPath, ($mainOrig -replace "#define FW_VERSION `"$oldEsc`"", "#define FW_VERSION `"$new`""), $utf8)
    if ($readmeOrig) {
        $r = $readmeOrig -replace "\| ``FW_VERSION`` \| ``$oldEsc`` \|", "| ``FW_VERSION`` | ``$new`` |"
        $r = $r -replace "\(obecnie $oldEsc\)", "(obecnie $new)"
        [IO.File]::WriteAllText($readmePath, $r, $utf8)
    }
    Write-Host "Wersja: $old -> $new" -ForegroundColor Cyan
} else {
    Write-Host "Wersja bez zmian: $old" -ForegroundColor Cyan
}

Write-Host "Kompilacja..." -ForegroundColor Cyan
& $pio run
if ($LASTEXITCODE -ne 0) {
    if ($new -ne $old) {
        [IO.File]::WriteAllText($mainPath, $mainOrig, $utf8)
        if ($readmeOrig) { [IO.File]::WriteAllText($readmePath, $readmeOrig, $utf8) }
        Write-Host "Kompilacja nie powiodła się - przywrócono wersję $old." -ForegroundColor Red
    } else {
        Write-Host "Kompilacja nie powiodła się." -ForegroundColor Red
    }
    exit $LASTEXITCODE
}

$size = [math]::Round((Get-Item $binPath).Length / 1024)
Write-Host ""
Write-Host "Gotowe: firmware v$new ($size kB)" -ForegroundColor Green
Write-Host $binPath -ForegroundColor Green
