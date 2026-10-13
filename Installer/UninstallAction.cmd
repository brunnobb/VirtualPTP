@echo off
setlocal
set SCRIPTDIR=%~dp0
pushd "%SCRIPTDIR%"

echo ==========================================================
echo  Logitech T650 Virtual PTP Driver Uninstallation
echo ==========================================================

echo [1/3] Removing Virtual PTP device node...
if exist "%SCRIPTDIR%devcon.exe" (
    "%SCRIPTDIR%devcon.exe" remove "Root\T650VirtualPtp" >nul 2>&1
)

echo [2/3] Removing driver from Windows DriverStore...
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$drivers = pnputil.exe /enum-drivers; if ($drivers -match 'virtualptpdriver.inf') { $publishedName = ($drivers | Select-String 'oem\d+\.inf' | Select-Object -First 1).Matches.Value; if ($publishedName) { pnputil.exe /delete-driver $publishedName /uninstall /force } }" >nul 2>&1

echo [3/3] Removing test certificate...
certutil.exe -delstore "TrustedPublisher" "T650 Virtual PTP Test" >nul 2>&1
certutil.exe -delstore "Root" "T650 Virtual PTP Test" >nul 2>&1

popd
endlocal
exit /b 0
