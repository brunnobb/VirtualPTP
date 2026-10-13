# ==============================================================================
# Build Script: Virtual PTP Driver MSI Installer
# Uses WiX Toolset v3.14 to compile and package VirtualPtpDriver-Setup-v<Version>.msi
# ==============================================================================

param(
    [string]$Version
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot = Resolve-Path (Join-Path $scriptDir "..")
$driverPackageDir = Join-Path $repoRoot "Package"
$objDir = Join-Path $scriptDir "obj"
$distDir = Join-Path $repoRoot "Dist"

# Auto-detect version if not specified
if (-not $Version) {
    $wxsFile = Join-Path $scriptDir "VirtualPtpDriver.wxs"
    if (Test-Path $wxsFile) {
        $wxsContent = Get-Content $wxsFile -Raw
        if ($wxsContent -match 'Product\s+[^>]*Version="(\d+\.\d+\.\d+)') {
            $Version = $Matches[1]
        } else {
            $Version = "1.0.0"
        }
    } else {
        $Version = "1.0.0"
    }
}

$msiFileName = "VirtualPtpDriver-Setup-v$Version.msi"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Building Virtual PTP Driver Installer v$Version" -ForegroundColor Cyan
Write-Host " Target File: $msiFileName" -ForegroundColor White
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Locate WiX Toolset Binaries
$wixBin = "C:\Program Files (x86)\WiX Toolset v3.14\bin"
$candle = Join-Path $wixBin "candle.exe"
$light = Join-Path $wixBin "light.exe"

if (-not (Test-Path $candle) -or -not (Test-Path $light)) {
    Write-Host "[Error] WiX Toolset v3.14 binaries not found at '$wixBin'!" -ForegroundColor Red
    Exit 1
}

# 2. Verify driver package files exist
$requiredFiles = @(
    "VirtualPtpDriver.dll",
    "VirtualPtpDriver.inf",
    "virtualptpdriver.cat",
    "T650TestCert.cer",
    "devcon.exe"
)

foreach ($f in $requiredFiles) {
    $fullPath = Join-Path $driverPackageDir $f
    if (-not (Test-Path $fullPath)) {
        Write-Host "[Error] Required driver file '$f' missing from '$driverPackageDir'!" -ForegroundColor Red
        Exit 1
    }
}

# 3. Compile WiX Source (.wxs -> .wixobj)
Write-Host "`n[1/3] Compiling WiX Source with candle.exe..." -ForegroundColor Yellow
if (-not (Test-Path $objDir)) { New-Item -ItemType Directory -Path $objDir -Force | Out-Null }
$wxsFile = Join-Path $scriptDir "VirtualPtpDriver.wxs"
$wixObj = Join-Path $objDir "VirtualPtpDriver.wixobj"
$iconPath = Join-Path $scriptDir "VirtualPTP.ico"
$licenseRtf = Join-Path $scriptDir "License.rtf"

& $candle -nologo -arch x64 -dSourceDir="$driverPackageDir" -dActionDir="$scriptDir" -dIconPath="$iconPath" -dLicenseRtf="$licenseRtf" -out "$wixObj" -ext WixUIExtension "$wxsFile"
if ($LASTEXITCODE -ne 0) {
    Write-Host "[Error] candle.exe compilation failed!" -ForegroundColor Red
    Exit 1
}

# 4. Link WiX Objects (.wixobj -> .msi)
Write-Host "`n[2/3] Linking MSI Package with light.exe..." -ForegroundColor Yellow
if (-not (Test-Path $distDir)) { New-Item -ItemType Directory -Path $distDir -Force | Out-Null }
$msiOut = Join-Path $distDir $msiFileName

& $light -nologo -out "$msiOut" -ext WixUIExtension -sice:ICE69 -sice:ICE91 "$wixObj"
if ($LASTEXITCODE -ne 0) {
    Write-Host "[Error] light.exe linking failed!" -ForegroundColor Red
    Exit 1
}

# Keep a convenience copy without version string
Copy-Item -Path $msiOut -Destination (Join-Path $distDir "VirtualPtpDriver-Setup.msi") -Force

# 5. Compute SHA-256 Checksum
Write-Host "`n[3/3] Calculating SHA-256 Checksum..." -ForegroundColor Yellow
$hashInfo = Get-FileHash -Path $msiOut -Algorithm SHA256
$hashHex = $hashInfo.Hash.ToLower()
$shaFile = "$msiOut.sha256"
"$hashHex  $msiFileName" | Set-Content -Path $shaFile -Encoding ascii

# Update consolidated SHA256SUMS.txt
$shaSumsPath = Join-Path $distDir "SHA256SUMS.txt"
$shaLines = @()
Get-ChildItem -Path "$distDir\*.msi" | Sort-Object Name | ForEach-Object {
    $h = (Get-FileHash -Path $_.FullName -Algorithm SHA256).Hash.ToLower()
    $shaLines += "$h  $($_.Name)"
}
$shaLines | Set-Content -Path $shaSumsPath -Encoding ascii

$msiItem = Get-Item $msiOut
Write-Host "`n==========================================================" -ForegroundColor Green
Write-Host " Virtual PTP Driver MSI Built Successfully!" -ForegroundColor Green
Write-Host " Output File: $($msiItem.FullName)" -ForegroundColor White
Write-Host " Size:        $([math]::Round($msiItem.Length / 1KB, 2)) KB" -ForegroundColor Cyan
Write-Host " SHA-256:     $hashHex" -ForegroundColor Yellow
Write-Host " Hash File:   $shaFile" -ForegroundColor Gray
Write-Host "==========================================================" -ForegroundColor Green
