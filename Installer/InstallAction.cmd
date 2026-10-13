@echo off
setlocal
set SCRIPTDIR=%~dp0
pushd "%SCRIPTDIR%"

echo ==========================================================
echo  Logitech T650 Virtual PTP Driver Installation
echo ==========================================================

if not exist "C:\ProgramData\LogitechT650" mkdir "C:\ProgramData\LogitechT650" >nul 2>&1
set LOGFILE=C:\ProgramData\LogitechT650\InstallDriver.log
echo --- Starting Driver Installation [%DATE% %TIME%] --- > "%LOGFILE%"

echo [1/4] Installing Test Signing Certificate...
certutil.exe -addstore "TrustedPublisher" "%SCRIPTDIR%T650TestCert.cer" >> "%LOGFILE%" 2>&1
certutil.exe -addstore "Root" "%SCRIPTDIR%T650TestCert.cer" >> "%LOGFILE%" 2>&1

echo [2/4] Configuring Windows testsigning...
bcdedit.exe /set testsigning on >> "%LOGFILE%" 2>&1

echo [3/4] Adding driver to Windows DriverStore via pnputil...
pnputil.exe /add-driver "%SCRIPTDIR%VirtualPtpDriver.inf" /install >> "%LOGFILE%" 2>&1

echo [4/4] Creating or restarting Virtual PTP root device node via devcon...
if exist "%SCRIPTDIR%devcon.exe" (
    "%SCRIPTDIR%devcon.exe" update "%SCRIPTDIR%VirtualPtpDriver.inf" "Root\T650VirtualPtp" >> "%LOGFILE%" 2>&1
    "%SCRIPTDIR%devcon.exe" install "%SCRIPTDIR%VirtualPtpDriver.inf" "Root\T650VirtualPtp" >> "%LOGFILE%" 2>&1
    "%SCRIPTDIR%devcon.exe" restart "Root\T650VirtualPtp" >> "%LOGFILE%" 2>&1
)

echo --- Driver Installation Complete [%DATE% %TIME%] --- >> "%LOGFILE%"
popd
endlocal
exit /b 0
