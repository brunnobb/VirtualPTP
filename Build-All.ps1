# ==============================================================================
# Build Script: Compile, Sign, and Package Virtual PTP Driver
# Usage: pwsh -File "Build-All.ps1" [-Version "1.1.6"]
# ==============================================================================

param(
    [string]$Version
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$repoRoot = Resolve-Path $scriptDir

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Building Virtual PTP Driver & Signing Package" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Locate MSBuild
$msbuild = "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $msbuild)) {
    $msbuild = "MSBuild.exe"
}

# 2. Compile C++ UMDF 2 Driver
Write-Host "`n[1/4] Compiling Virtual PTP Driver (Release|x64)..." -ForegroundColor Yellow
$projPath = Join-Path $repoRoot "Driver\VirtualPtpDriver.vcxproj"
& $msbuild "$projPath" /p:Configuration=Release /p:Platform=x64 /t:Rebuild
if ($LASTEXITCODE -ne 0) {
    Write-Host "[Error] Driver build failed!" -ForegroundColor Red
    Exit 1
}

# 3. Stage compiled binary into Package
Write-Host "`n[2/4] Staging compiled binary into Package/..." -ForegroundColor Yellow
$builtDll = Join-Path $repoRoot "bin\Release\VirtualPtpDriver.dll"
if (-not (Test-Path $builtDll)) {
    $builtDll = Join-Path $repoRoot "Driver\bin\Release\VirtualPtpDriver.dll"
}
$packageDir = Join-Path $repoRoot "Package"
$pkgDll = Join-Path $packageDir "VirtualPtpDriver.dll"
Copy-Item $builtDll $pkgDll -Force
Write-Host "  Copied $builtDll -> $pkgDll" -ForegroundColor Gray

# 4. Generate Catalog and Sign Driver Package
Write-Host "`n[3/4] Generating Catalog and Test-Signing Driver..." -ForegroundColor Yellow
$inf2cat = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x86\Inf2Cat.exe"
if (Test-Path $inf2cat) {
    & $inf2cat /driver:"$packageDir" /os:10_X64,Server10_X64
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[Error] Inf2Cat failed!" -ForegroundColor Red
        Exit 1
    }
} else {
    Write-Host "[Warning] Inf2Cat not found at '$inf2cat', skipping catalog generation." -ForegroundColor Yellow
}

$signtool = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"
$thumbprint = "737386A00937ED1A64A90628A02C8850D1FA5A40"
if (Test-Path $signtool) {
    $catFile = Join-Path $packageDir "virtualptpdriver.cat"
    Write-Host "Signing driver DLL and catalog with thumbprint $thumbprint..." -ForegroundColor Cyan
    & $signtool sign /sha1 $thumbprint /fd SHA256 /v "$pkgDll" "$catFile"
} else {
    Write-Host "[Warning] signtool not found at '$signtool', skipping signing." -ForegroundColor Yellow
}

# 5. Build WiX MSI Package
Write-Host "`n[4/4] Building MSI Installer..." -ForegroundColor Yellow
$installerScript = Join-Path $repoRoot "Installer\Build-Driver-Installer.ps1"
$passArgs = @()
if ($Version) {
    $passArgs = @("-Version", $Version)
}
& pwsh -File "$installerScript" @passArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host "[Error] Installer build failed!" -ForegroundColor Red
    Exit 1
}

Write-Host "`n==========================================================" -ForegroundColor Green
Write-Host " Virtual PTP Driver Build & Packaging Complete!" -ForegroundColor Green
Write-Host " Artifacts located in: $(Join-Path $repoRoot 'Dist')" -ForegroundColor White
Write-Host "==========================================================" -ForegroundColor Green
