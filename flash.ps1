# Pico Game Controller - Auto Build and Flash PowerShell Script
# Advanced PowerShell script with automatic drive detection

param(
    [switch]$SkipBuild,
    [int]$Timeout = 30
)

$ErrorActionPreference = "Stop"

# Configuration
$ProjectRoot = $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"
$UF2Source = Join-Path $BuildDir "src\Pico_Game_Controller.uf2"
$UF2Backup = Join-Path $ProjectRoot "build_uf2\Pico_Game_Controller.uf2"
$NinjaPath = "$env:USERPROFILE\.pico-sdk\ninja\v1.12.1\ninja.exe"
$PicotoolPath = "$env:USERPROFILE\.pico-sdk\picotool\2.2.0\picotool\picotool.exe"

# Color functions
function Write-Step($Step, $Message) {
    Write-Host "[$Step] " -ForegroundColor Cyan -NoNewline
    Write-Host $Message -ForegroundColor White
}

function Write-Success($Message) {
    Write-Host "SUCCESS " -ForegroundColor Green -NoNewline
    Write-Host $Message -ForegroundColor White
}

function Write-Error-Custom($Message) {
    Write-Host "ERROR " -ForegroundColor Red -NoNewline
    Write-Host $Message -ForegroundColor White
}

function Write-Warning-Custom($Message) {
    Write-Host "WARNING " -ForegroundColor Yellow -NoNewline
    Write-Host $Message -ForegroundColor White
}

function Test-ProjectDirectory {
    $MainFile = Join-Path $ProjectRoot "src\pico_game_controller.c"
    return Test-Path $MainFile
}

function Invoke-Build {
    Write-Step "1/4" "Compiling project..."
    
    if (-not (Test-Path $BuildDir)) {
        Write-Error-Custom "Build directory not found: $BuildDir"
        return $false
    }
    
    if (-not (Test-Path $NinjaPath)) {
        Write-Error-Custom "Ninja not found at: $NinjaPath"
        return $false
    }
    
    try {
        Push-Location $BuildDir
        & $NinjaPath
        if ($LASTEXITCODE -eq 0) {
            Write-Success "Compilation successful"
            return $true
        } else {
            Write-Error-Custom "Compilation failed"
            return $false
        }
    }
    catch {
        Write-Error-Custom "Failed to run ninja: $_"
        return $false
    }
    finally {
        Pop-Location
    }
}

function Enter-BootselMode {
    Write-Step "2/4" "Entering BOOTSEL mode..."
    
    if (-not (Test-Path $PicotoolPath)) {
        Write-Error-Custom "Picotool not found at: $PicotoolPath"
        return $false
    }
    
    try {
        # Check if device is connected
        $InfoResult = & $PicotoolPath info 2>$null
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Device found, rebooting to BOOTSEL mode..."
            & $PicotoolPath reboot -f 2>$null
            Start-Sleep -Seconds 3
        } else {
            Write-Warning-Custom "Device not found in normal mode"
        }
        
        Write-Success "BOOTSEL mode command sent"
        return $true
    }
    catch {
        Write-Warning-Custom "Failed to communicate with device: $_"
        return $true  # Continue anyway
    }
}

function Find-BootselDrive {
    # Get all removable drives and check for RPI-RP2
    $Drives = Get-WmiObject -Class Win32_LogicalDisk | Where-Object { $_.DriveType -eq 2 }
    
    foreach ($Drive in $Drives) {
        $InfoFile = Join-Path $Drive.DeviceID "INFO_UF2.TXT"
        if (Test-Path $InfoFile) {
            $Content = Get-Content $InfoFile -Raw -ErrorAction SilentlyContinue
            if ($Content -match "RPI-RP2|Raspberry Pi") {
                return $Drive.DeviceID
            }
        }
    }
    
    return $null
}

function Wait-ForBootselDrive {
    Write-Step "3/4" "Waiting for BOOTSEL drive (timeout: $Timeout seconds)..."
    
    $StartTime = Get-Date
    $Found = $false
    
    do {
        $Drive = Find-BootselDrive
        if ($Drive) {
            Write-Success "BOOTSEL drive found: $Drive"
            return $Drive
        }
        
        Write-Host "." -NoNewline -ForegroundColor Yellow
        Start-Sleep -Seconds 1
        $Elapsed = (Get-Date) - $StartTime
    } while ($Elapsed.TotalSeconds -lt $Timeout)
    
    Write-Host ""  # New line
    Write-Error-Custom "BOOTSEL drive not found within timeout"
    return $null
}

function Copy-UF2File($BootselDrive) {
    Write-Step "4/4" "Flashing UF2 file..."
    
    if (-not (Test-Path $UF2Source)) {
        Write-Error-Custom "UF2 file not found: $UF2Source"
        return $false
    }
    
    $TargetPath = Join-Path $BootselDrive "Pico_Game_Controller.uf2"
    
    try {
        Write-Host "Copying $(Split-Path $UF2Source -Leaf) to $BootselDrive"
        Copy-Item $UF2Source $TargetPath -Force
        
        # Also update backup copy
        $BackupDir = Split-Path $UF2Backup -Parent
        if (-not (Test-Path $BackupDir)) {
            New-Item -ItemType Directory -Path $BackupDir -Force | Out-Null
        }
        Copy-Item $UF2Source $UF2Backup -Force
        
        Write-Success "UF2 file copied successfully"
        Write-Success "Device should reboot automatically"
        return $true
    }
    catch {
        Write-Error-Custom "Failed to copy UF2 file: $_"
        return $false
    }
}

function Wait-ForDeviceReconnect {
    Write-Host ""
    Write-Host "Waiting for device to reconnect..." -ForegroundColor Blue
    
    $StartTime = Get-Date
    $TimeoutSecs = 15
    
    do {
        try {
            $InfoResult = & $PicotoolPath info 2>$null
            if ($LASTEXITCODE -eq 0) {
                Write-Success "Device reconnected!"
                return $true
            }
        }
        catch {
            # Ignore errors
        }
        
        Start-Sleep -Seconds 1
        $Elapsed = (Get-Date) - $StartTime
    } while ($Elapsed.TotalSeconds -lt $TimeoutSecs)
    
    Write-Warning-Custom "Device did not reconnect (this is normal if working correctly)"
    return $false
}

# Main script
Write-Host "================================================================" -ForegroundColor Magenta
Write-Host "  Pico Game Controller - Auto Build and Flash Script" -ForegroundColor Magenta
Write-Host "================================================================" -ForegroundColor Magenta
Write-Host ""

# Check if we're in the right directory
if (-not (Test-ProjectDirectory)) {
    Write-Error-Custom "Not in the correct project directory"
    Write-Error-Custom "Please run this script from the project root"
    exit 1
}

try {
    # Step 1: Build (unless skipped)
    if (-not $SkipBuild) {
        if (-not (Invoke-Build)) {
            exit 1
        }
    } else {
        Write-Warning-Custom "Skipping build step"
    }
    
    # Step 2: Enter BOOTSEL mode
    if (-not (Enter-BootselMode)) {
        exit 1
    }
    
    # Step 3: Wait for BOOTSEL drive
    $BootselDrive = Wait-ForBootselDrive
    if (-not $BootselDrive) {
        Write-Error-Custom "Could not find BOOTSEL drive"
        Write-Host ""
        Write-Host "Please manually put the device in BOOTSEL mode:" -ForegroundColor Yellow
        Write-Host "1. Hold BOOTSEL button while connecting USB" -ForegroundColor Yellow
        Write-Host "2. Or hold BOOTSEL button and press RESET" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Press any key to try again, or Ctrl+C to cancel..." -ForegroundColor Yellow
        $null = Read-Host
        
        $BootselDrive = Wait-ForBootselDrive
        if (-not $BootselDrive) {
            exit 1
        }
    }
    
    # Step 4: Flash UF2
    if (-not (Copy-UF2File $BootselDrive)) {
        exit 1
    }
    
    # Wait for device to reconnect
    Wait-ForDeviceReconnect
    
    Write-Host ""
    Write-Host "================================================================" -ForegroundColor Green
    Write-Host "  Flash operation completed successfully!" -ForegroundColor Green
    Write-Host "================================================================" -ForegroundColor Green
}
catch {
    Write-Host ""
    Write-Error-Custom "Unexpected error: $_"
    exit 1
}

# Keep window open if run from GUI
if ($Host.Name -eq "ConsoleHost") {
    Write-Host ""
    Write-Host "Press any key to exit..." -ForegroundColor Gray
    Read-Host
}
