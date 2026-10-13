# ===================================================================
# Logitech T650 Virtual Precision Touchpad Driver - Automated Uninstaller
# Run in an Elevated (Administrator) PowerShell prompt
# ===================================================================

$ErrorActionPreference = "Continue"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $scriptDir

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Logitech T650 Virtual PTP Driver Uninstaller" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Remove Devcon device node
$devcon = Join-Path $scriptDir "devcon.exe"
if (-not (Test-Path $devcon)) {
    $wdkDevcon = "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.26100.0\x64\devcon.exe"
    if (Test-Path $wdkDevcon) {
        $devcon = $wdkDevcon
    }
}

if (Test-Path $devcon) {
    Write-Host "`n[1/3] Removing Virtual PTP device node..." -ForegroundColor Yellow
    & $devcon remove "Root\T650VirtualPtp"
    & $devcon remove "@ROOT\HIDCLASS\*"
}

# 2. Delete driver from DriverStore
Write-Host "`n[2/3] Removing driver package from Windows DriverStore..." -ForegroundColor Yellow
$drivers = pnputil.exe /enum-drivers
if ($drivers -match "virtualptpdriver.inf") {
    $publishedName = ($drivers | Select-String -Pattern "oem\d+\.inf" | Select-Object -First 1).Matches.Value
    if ($publishedName) {
        pnputil.exe /delete-driver $publishedName /uninstall /force
        Write-Host "  -> Deleted driver: $publishedName" -ForegroundColor Green
    }
}

# 3. Remove test certificate
Write-Host "`n[3/3] Removing test certificate from trusted stores..." -ForegroundColor Yellow
$certFile = Join-Path $scriptDir "T650TestCert.cer"
if (Test-Path $certFile) {
    certutil.exe -delstore "TrustedPublisher" "T650 Virtual PTP Test" | Out-Null
    certutil.exe -delstore "Root" "T650 Virtual PTP Test" | Out-Null
}

Write-Host "`nUninstallation complete." -ForegroundColor Green
