# compile.ps1 - Windows 11 build script for Analog_Clock-esp32c3
#
# Usage (from the sketch folder, in PowerShell):
#   .\compile.ps1                      # build + flash on COM3
#   .\compile.ps1 -Port COM5           # use a different COM port
#   .\compile.ps1 -NoWrite             # build only, no flashing
#
# If scripts are blocked:
#   powershell -ExecutionPolicy Bypass -File .\compile.ps1 -Port COM5
#
# Requirements: Git and Python 3 on PATH (winget install Git.Git Python.Python.3.12)

param(
    [string]$Port = "COM3",     # check Device Manager > Ports (COM & LPT)
    [switch]$NoWrite
)

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

function Invoke-Native {
    # Run a native command and stop the script if it fails
    param([string]$Exe, [string[]]$Arguments)
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed ($LASTEXITCODE): $Exe" }
}

# --- arduino-cli -------------------------------------------------------------
$BinDir = Join-Path $PSScriptRoot "bin"
$CliExe = Join-Path $BinDir "arduino-cli.exe"
if (-not (Test-Path $CliExe)) {
    Write-Host "Downloading and installing the arduino-cli..."
    New-Item -ItemType Directory -Force -Path $BinDir | Out-Null
    $zip = Join-Path $env:TEMP "arduino-cli.zip"
    Invoke-WebRequest -UseBasicParsing `
        -Uri "https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip" `
        -OutFile $zip
    Expand-Archive -Path $zip -DestinationPath $BinDir -Force
    Remove-Item $zip
}

# --- Total cache erase -------------------------------------------------------
Write-Host "Erasing all Arduino caches..."
$CachePaths = @(
    (Join-Path $env:TEMP "arduino\sketches"),
    (Join-Path $env:TEMP "arduino\cores"),
    (Join-Path $PSScriptRoot "build_cache")
)
foreach ($p in $CachePaths) {
    if (Test-Path $p) { Remove-Item -Recurse -Force $p }
}

# --- Isolated local storage for tools ---------------------------------------
# Kept short to avoid Windows' 260-character path limit
$LocalData = "C:\arduino_clock_cli"
New-Item -ItemType Directory -Force -Path $LocalData | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $PSScriptRoot "build_cache") | Out-Null
$BuildPath = Join-Path $PSScriptRoot "build_cache"
$CliConfig = @("--config-dir", $LocalData)

# --- ESP32 core 2.0.17 -------------------------------------------------------
$CoreDir = Join-Path $LocalData "packages\esp32\hardware\esp32\2.0.17"
if (-not (Test-Path $CoreDir)) {
    Write-Host "Installing private ESP32 Core 2.0.17..."
    Invoke-Native $CliExe ($CliConfig + @("core", "update-index"))
    Invoke-Native $CliExe ($CliConfig + @("core", "install", "esp32:esp32@2.0.17"))
}

# --- Third-party libraries ---------------------------------------------------
$ArduinoLibs = Join-Path $LocalData "libraries"
New-Item -ItemType Directory -Force -Path $ArduinoLibs | Out-Null

$RequiredLibs = [ordered]@{
    "Adafruit_GFX_Library" = "https://github.com/adafruit/Adafruit-GFX-Library.git"
    "Adafruit_BusIO"       = "https://github.com/adafruit/Adafruit_BusIO.git"
    "Adafruit_GC9A01A"     = "https://github.com/adafruit/Adafruit_GC9A01A.git"
    "ArduinoJson"          = "https://github.com/bblanchon/ArduinoJson.git"
}

foreach ($name in $RequiredLibs.Keys) {
    $libPath = Join-Path $ArduinoLibs $name
    if (-not (Test-Path $libPath)) {
        Write-Host "Installing library $name..."
        Invoke-Native "git" @("clone", "--depth", "1", $RequiredLibs[$name], $libPath)
    }
}

# --- Data image (LittleFS in the SPIFFS partition) ---------------------------
Write-Host "Creating data image for SPIFFS partition..."
$MkLittleFs = Get-ChildItem -Path (Join-Path $LocalData "packages\esp32\tools\mklittlefs") `
    -Filter "mklittlefs.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $MkLittleFs) { throw "mklittlefs.exe not found under $LocalData" }

Invoke-Native $MkLittleFs.FullName @(
    "--page", "256",
    "--size", "0x1d0000",
    "--block", "4096",
    "--create", "data", "data.img"
)

if (-not $NoWrite) {
    Write-Host "Write data image to SPIFFS partition on $Port..."
    $EspToolDir = Get-ChildItem -Path (Join-Path $LocalData "packages\esp32\tools\esptool_py") -Directory |
        Select-Object -First 1
    $EspToolExe = Join-Path $EspToolDir.FullName "esptool.exe"
    $EspToolArgs = @("--port", $Port, "--baud", "921600", "write_flash", "0x230000", "data.img")

    if (Test-Path $EspToolExe) {
        Invoke-Native $EspToolExe $EspToolArgs
    } else {
        Invoke-Native "python" (@((Join-Path $EspToolDir.FullName "esptool.py")) + $EspToolArgs)
    }
} else {
    Write-Host "Skipping SPIFFS flash (-NoWrite enabled)"
}

# --- Compile -----------------------------------------------------------------
Write-Host "Compiling..."
Invoke-Native $CliExe ($CliConfig + @(
    "compile",
    "--fqbn", "esp32:esp32:esp32c3",
    "-j", "0",
    "--build-path", $BuildPath,
    "--libraries", $ArduinoLibs,
    "--build-property", "build.extra_flags=-D CORE_DEBUG_LEVEL=0 -D BitOrder=uint8_t",
    "--build-property", "build.partitions=partitions",
    ".\Analog_Clock-esp32c3.ino"
))

# --- Upload ------------------------------------------------------------------
if (-not $NoWrite) {
    Write-Host "Uploading firmware..."
    Invoke-Native $CliExe ($CliConfig + @(
        "upload",
        "--fqbn", "esp32:esp32:esp32c3",
        "--port", $Port,
        "--input-dir", $BuildPath
    ))
} else {
    Write-Host "Skipping firmware flash (-NoWrite enabled)"
}

Write-Host "Done."
