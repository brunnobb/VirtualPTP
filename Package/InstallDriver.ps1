# ===================================================================
# Logitech T650 Virtual Precision Touchpad Driver - Automated Installer
# Run in an Elevated (Administrator) PowerShell prompt
# ===================================================================

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $scriptDir

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Logitech T650 Virtual PTP Driver (UMDF 2) Installer" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Verify Administrative Privileges
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "[Error] This script must be run as Administrator!" -ForegroundColor Red
    Write-Host "Right-click PowerShell -> 'Run as administrator', then re-run this script." -ForegroundColor Yellow
    Exit 1
}

# 2. Trust the Test Signing Certificate
Write-Host "`n[1/4] Installing Test Signing Certificate to Trusted Stores..." -ForegroundColor Yellow
$certFile = Join-Path $scriptDir "T650TestCert.cer"
if (Test-Path $certFile) {
    certutil.exe -addstore "TrustedPublisher" $certFile | Out-Null
    certutil.exe -addstore "Root" $certFile | Out-Null
    Write-Host "  -> Test Certificate successfully added to TrustedPublisher and Root." -ForegroundColor Green
} else {
    Write-Host "  -> [Warning] T650TestCert.cer not found!" -ForegroundColor Red
}

# 3. Check / Configure Testsigning
Write-Host "`n[2/4] Checking Windows Test-Signing Status..." -ForegroundColor Yellow
try {
    $bcd = bcdedit.exe /enum '{current}'
    if ($bcd -match "testsigning\s+Yes") {
        Write-Host "  -> Windows Test-Signing is already ENABLED." -ForegroundColor Green
    } else {
        Write-Host "  -> Enabling Windows Test-Signing..." -ForegroundColor Yellow
        bcdedit.exe /set testsigning on | Out-Null
        Write-Host "  -> Test-signing has been turned ON." -ForegroundColor Yellow
        Write-Host "  -> NOTE: If drivers fail to start, a one-time Windows reboot may be required." -ForegroundColor Magenta
    }
} catch {
    Write-Host "  -> Notice: Could not query BCD. Assuming testsigning is configured." -ForegroundColor Gray
}

# 4. Add Driver via pnputil
Write-Host "`n[3/4] Adding driver to Windows Driver Store via pnputil..." -ForegroundColor Yellow
$infFile = Join-Path $scriptDir "VirtualPtpDriver.inf"
$pnpResult = pnputil.exe /add-driver $infFile /install
Write-Host $pnpResult -ForegroundColor Gray

# 5. Create Root Device Node via devcon
Write-Host "`n[4/4] Creating Virtual PTP Device Node (Root\T650VirtualPtp)..." -ForegroundColor Yellow
$devcon = Join-Path $scriptDir "devcon.exe"
if (-not (Test-Path $devcon)) {
    $wdkDevcon = "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.26100.0\x64\devcon.exe"
    if (Test-Path $wdkDevcon) {
        $devcon = $wdkDevcon
    }
}

if (Test-Path $devcon) {
    & $devcon remove "Root\T650VirtualPtp" | Out-Null
    & $devcon remove "@ROOT\HIDCLASS\*" | Out-Null
    & $devcon install $infFile "Root\T650VirtualPtp"
    Write-Host "  -> Devcon device installation complete!" -ForegroundColor Green
} else {
    Write-Host "  -> devcon.exe not found in package directory or WDK tools." -ForegroundColor Red
}

Write-Host "`n==========================================================" -ForegroundColor Green
Write-Host " Installation Complete!" -ForegroundColor Green
Write-Host " Check Device Manager -> Human Interface Devices for:" -ForegroundColor Cyan
Write-Host " 'Logitech T650 Precision Touchpad (Virtual PTP)'" -ForegroundColor White
Write-Host "==========================================================" -ForegroundColor Green
