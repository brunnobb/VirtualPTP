# ==============================================================================
# Bump Version Script: Virtual PTP Driver
# Usage: pwsh -File "Bump-Version.ps1" -NewVersion "1.1.7"
# ==============================================================================

param(
    [Parameter(Mandatory = $true)]
    [string]$NewVersion
)

$ErrorActionPreference = "Stop"

if ($NewVersion -notmatch '^\d+\.\d+\.\d+$') {
    Write-Host "[Error] Version must follow Semantic Versioning 'MAJOR.MINOR.PATCH' (e.g. 1.1.7)!" -ForegroundColor Red
    Exit 1
}

$fourPartVersion = "$NewVersion.0"
$todayDate = (Get-Date).ToString("MM'/'dd'/'yyyy")
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot = Resolve-Path $scriptDir

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Bumping Virtual PTP Driver -> $NewVersion ($fourPartVersion)" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Update VirtualPtpDriver.inx
$inxPath = Join-Path $repoRoot "Driver\VirtualPtpDriver.inx"
if (Test-Path $inxPath) {
    Write-Host "`n[1/3] Updating $inxPath..." -ForegroundColor Yellow
    $inxContent = Get-Content $inxPath -Raw
    $inxContent = [regex]::Replace($inxContent, 'DriverVer\s*=\s*[^,\r\n]+,[\d\.]+', "DriverVer   = $todayDate,$fourPartVersion")
    Set-Content -Path $inxPath -Value $inxContent -Encoding utf8
    Write-Host "  -> Updated DriverVer to $todayDate,$fourPartVersion" -ForegroundColor Green
}

# 2. Update Package\VirtualPtpDriver.inf
$infPath = Join-Path $repoRoot "Package\VirtualPtpDriver.inf"
if (Test-Path $infPath) {
    Write-Host "`n[2/3] Updating $infPath..." -ForegroundColor Yellow
    $infContent = Get-Content $infPath -Raw
    $infContent = [regex]::Replace($infContent, 'DriverVer\s*=\s*[^,\r\n]+,[\d\.]+', "DriverVer   = $todayDate,$fourPartVersion")
    Set-Content -Path $infPath -Value $infContent -Encoding utf8
    Write-Host "  -> Updated DriverVer to $todayDate,$fourPartVersion" -ForegroundColor Green
}

# 3. Update Installer\VirtualPtpDriver.wxs
$wxsPath = Join-Path $repoRoot "Installer\VirtualPtpDriver.wxs"
if (Test-Path $wxsPath) {
    Write-Host "`n[3/3] Updating $wxsPath..." -ForegroundColor Yellow
    $wxsContent = Get-Content $wxsPath -Raw
    $wxsContent = [regex]::Replace($wxsContent, '(?<!Installer)Version="[\d\.]+"', "Version=""$fourPartVersion""")
    Set-Content -Path $wxsPath -Value $wxsContent -Encoding utf8
    Write-Host "  -> Updated to Version $fourPartVersion" -ForegroundColor Green
}

# 4. Rebuild & Re-package
Write-Host "`n>>> Rebuilding and packaging Virtual PTP Driver v$NewVersion..." -ForegroundColor Cyan
$buildScript = Join-Path $repoRoot "Build-All.ps1"
& pwsh -File "$buildScript" -Version "$NewVersion"
if ($LASTEXITCODE -ne 0) {
    Write-Host "[Error] Build-All failed!" -ForegroundColor Red
    Exit 1
}

Write-Host "`n==========================================================" -ForegroundColor Green
Write-Host " Virtual PTP Driver version bump to $NewVersion complete!" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
