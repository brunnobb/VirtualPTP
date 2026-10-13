# Virtual Precision Touchpad (VirtualPTP) Driver - AI Agent Guide

> **Project:** Virtual Precision Touchpad (VirtualPTP) Driver  
> **Repository:** `git@github.com:brunnobb/VirtualPTP.git`  
> **Target Framework:** UMDF 2.15 (User-Mode Driver Framework)  
> **Language:** C++20 / Windows Driver Kit (WDK)

---

## 1. Project Overview & Architecture

`VirtualPTP` is a high-performance Windows User-Mode Driver Framework (UMDF 2) virtual HID minidriver that registers a fully compliant Microsoft Precision Touchpad (PTP) device with the Windows Touch Stack.

It decouples input hardware from the Windows OS input subsystem, providing a universal virtual bridge for trackpad hardware bridges (such as the Logitech T650 bridge daemon) to inject multi-touch contact frames directly into Windows 10 and 11.

```text
┌────────────────────────────────────────────────────────┐
│  Hardware Bridge Daemon (User Space)                   │
│  - Streams PTP frames over Named Pipe or IOCTL         │
└──────────────────────────┬─────────────────────────────┘
                           │ Named Pipe: \\.\pipe\T650VirtualPtpPipe
                           │ or IOCTL_PTP_INJECT_REPORT (0x00222004)
┌──────────────────────────▼─────────────────────────────┐
│  VirtualPTP Driver (UMDF 2.15 in WUDFHost.exe)         │
│  - Microsoft PTP TLC Report Descriptor (565 bytes)     │
│  - 3 Top-Level Collections: PTP Touch, Mouse, Config   │
│  - Device Interface: GUID_DEVINTERFACE_T650_VIRTUAL_PTP│
└──────────────────────────┬─────────────────────────────┘
                           │ HID Miniport Interface
┌──────────────────────────▼─────────────────────────────┐
│  Windows Touch Stack (mshidumdf / hidclass.sys)        │
│  - Native Windows Gestures, Inking & Edge Gestures     │
└────────────────────────────────────────────────────────┘
```

---

## 2. Technical Specifications & Critical Rules

### Framework Version: UMDF 2.15 Target (DO NOT CHANGE)
* **Framework Target:** The driver targets **UMDF 2.15** (`UmdfLibraryVersion = 2.15.0`).
* **Why UMDF 2.15:** UMDF 2.15 is universally present on **all versions of Windows 10 (1507 through 22H2) and Windows 11 (21H2 through 24H2)**. Targeting higher versions like UMDF 2.35 causes older OS builds to fail loading at level 0 with `STATUS_INVALID_PARAMETER` (`3489660941` / `0xD000000D`).
* **C Runtime:** Configured with `/MT` (Multi-threaded static CRT) so the driver DLL has zero dependencies on external Visual C++ Redistributable packages.

### Dual IPC Interface
1. **Named Pipe (`\\.\pipe\T650VirtualPtpPipe`):**
   * High-speed, low-latency named pipe server implemented inside `PipeServer.cpp`.
   * Security Descriptor: Configured with standard user access permissions (`D:(A;;GA;;;WD)`) to allow non-elevated daemons to connect, completely bypassing Windows HIDCLASS device symbolic link restrictions.
2. **Device IOCTL (`0x00222004`):**
   * `IOCTL_PTP_INJECT_REPORT = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)`.
   * Device Interface: `GUID_DEVINTERFACE_T650_VIRTUAL_PTP` (`{A50E5B44-5D88-4A56-BE34-F18A56DF762B}`).

### HID Report Descriptor (3-TLC Architecture)
* Defined in `Driver/PtpDescriptor.h` (565 bytes):
  1. **TLC 1: Windows Precision Touchpad (Usage Page: Digitizer `0x0D`, Usage: Touchpad `0x05`)**
     * Supports up to 5 concurrent contacts.
     * Report ID `0x01`: Contact reports (Tip switch, Confidence, Contact ID, X/Y coordinates, Scan Time).
     * Feature Reports: Maximum Count (`0x02`), PTP Configuration (`0x03`).
  2. **TLC 2: Standard Mouse Collection (Usage Page: Generic Desktop `0x01`, Usage: Mouse `0x02`)**
     * Report ID `0x04`: 5 buttons, X/Y relative motion, vertical and horizontal wheels.
  3. **TLC 3: Device Certification Collection (Usage Page: Digitizer `0x0D`, Usage: Device Cert `0x0E`)**
     * Report ID `0x05`: Microsoft cryptographic certification blob.

---

## 3. Directory Structure

```text
VirtualPTP/
├── VirtualPTP.sln              # Visual Studio solution file
├── Driver/                     # C++ UMDF 2 Driver source code
│   ├── VirtualPtpDriver.vcxproj # MSBuild driver project
│   ├── VirtualPtpDriver.inx     # Driver INF template
│   ├── Driver.cpp / Driver.h   # DriverEntry & lifetime management
│   ├── Device.cpp / Device.h   # WDF device creation & HID minidriver dispatch
│   ├── Queue.cpp / Queue.h     # I/O queue handling & IOCTL processing
│   ├── PipeServer.cpp / .h     # Named pipe IPC server
│   ├── PtpDescriptor.h         # 3-TLC Microsoft PTP HID Report Descriptor
│   ├── PtpTypes.h              # Contact frame & feature report structures
│   └── Trace.h                 # Real-time debug tracing helpers
├── Package/                    # Driver store staging and registration files
│   ├── VirtualPtpDriver.inf    # Production INF with DriverVer timestamp
│   ├── VirtualPtpDriver.dll    # Compiled driver DLL
│   ├── virtualptpdriver.cat    # Catalog signed with Authenticode test cert
│   ├── T650TestCert.cer        # Test root certificate
│   ├── devcon.exe              # Windows Device Console binary
│   ├── InstallDriver.ps1       # PowerShell driver installation script
│   └── UninstallDriver.ps1     # PowerShell driver uninstallation script
├── Installer/                  # WiX Toolset v3.14 MSI installer project
│   ├── VirtualPtpDriver.wxs    # WiX installer definition
│   ├── InstallAction.cmd       # Automated driver registration custom action
│   ├── UninstallAction.cmd     # Automated driver cleanup custom action
│   ├── License.rtf             # Distribution license
│   ├── VirtualPTP.ico          # Application icon
│   └── Build-Driver-Installer.ps1 # MSI compilation script
├── Build-All.ps1               # Full build, sign, and package pipeline
├── Bump-Version.ps1            # Automated SemVer bumper
└── Dist/                       # Output MSIs and SHA-256 checksums
```

---

## 4. Versioning & Release Guidelines (AI Agent Reference)

When an AI prompt requests a release, version bump, or installer update for VirtualPTP:

### 1. Versioning Standard
* **Git Version (SemVer):** `MAJOR.MINOR.PATCH` (e.g. `1.1.7`).
* **DriverVer & WiX Version (4-Part):** `MAJOR.MINOR.PATCH.BUILD` (e.g. `1.1.7.0`).
* **Windows Installer Rule:** MSI only evaluates `MAJOR.MINOR.PATCH` for upgrade checks. The 4th digit is ignored. Every release must increment at least `PATCH` or `MINOR`.

### 2. Canonical Version File Locations
When changing versions, synchronize these files:

| File | Target Element / Property | Example Value |
| :--- | :--- | :--- |
| [`Driver/VirtualPtpDriver.inx`](file:///c:/Workspace/T650-Windows-PTP/VirtualPTP/Driver/VirtualPtpDriver.inx) | `DriverVer = MM/DD/YYYY,MAJOR.MINOR.PATCH.BUILD` | `DriverVer = 10/12/2026,1.1.7.0` |
| [`Package/VirtualPtpDriver.inf`](file:///c:/Workspace/T650-Windows-PTP/VirtualPTP/Package/VirtualPtpDriver.inf) | `DriverVer = MM/DD/YYYY,MAJOR.MINOR.PATCH.BUILD` | `DriverVer = 10/12/2026,1.1.7.0` |
| [`Installer/VirtualPtpDriver.wxs`](file:///c:/Workspace/T650-Windows-PTP/VirtualPTP/Installer/VirtualPtpDriver.wxs) | `<Product Version="..." ...>` | `Version="1.1.7.0"` |

### 3. Strict WiX Rules
1. **Dynamic Product Id:** Always keep `Product Id="*"` so WiX generates a new `ProductCode` GUID on every release.
2. **Never change UpgradeCode:**
   * Virtual Driver UpgradeCode: `71E9D2E0-72E7-4AD2-8498-27041365E996`
   * Changing `UpgradeCode` breaks seamless automatic upgrades.
3. **MajorUpgrade:** Keep `<MajorUpgrade DowngradeErrorMessage="A newer version of [ProductName] is already installed." />`.

### 4. Automated Version Bump Command
```powershell
pwsh -File "Bump-Version.ps1" -NewVersion "1.1.7"
```

---

## 5. Build, Signing, and Packaging Tutorial

### Step 5.1: Build Driver via MSBuild
```powershell
$msbuild = "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
& $msbuild "Driver\VirtualPtpDriver.vcxproj" /p:Configuration=Release /p:Platform=x64 /t:Rebuild
Copy-Item "Driver\bin\Release\VirtualPtpDriver.dll" "Package\VirtualPtpDriver.dll" -Force
```

### Step 5.2: Generate Catalog & Dual-Sign
```powershell
$inf2cat = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x86\Inf2Cat.exe"
& $inf2cat /driver:"Package" /os:10_X64,Server10_X64

$signtool = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"
$thumbprint = "737386A00937ED1A64A90628A02C8850D1FA5A40"
& $signtool sign /sha1 $thumbprint /fd SHA256 /v "Package\VirtualPtpDriver.dll" "Package\virtualptpdriver.cat"
```

### Step 5.3: Build WiX MSI Installer
```powershell
pwsh -File "Installer\Build-Driver-Installer.ps1" -Version "1.1.7"
```

Output artifacts in `Dist/`:
* `VirtualPtpDriver-Setup-v<Version>.msi`
* `VirtualPtpDriver-Setup.msi`
* `SHA256SUMS.txt`

---

## 6. Testing, Diagnostics & Debugging

### 1. Enable Test-Signing on Test Machine
Because the driver is test-signed, Windows requires test mode:
```powershell
bcdedit /set testsigning on
Restart-Computer
```

### 2. Manual Device Node Registration
To manually install or reinstall the driver node:
```powershell
cd Package
pwsh -File "InstallDriver.ps1"
```

### 3. Real-Time Tracing via Sysinternals DebugView
The driver binary emits real-time trace messages via `OutputDebugStringW`:
1. Run `Dbgview.exe` as **Administrator**.
2. Go to **Capture** and verify:
   * **Capture Win32** is checked.
   * **Capture Global Win32** is checked *(essential for user-mode drivers inside `WUDFHost.exe`)*.
3. Filter (Ctrl+L) for `[VirtualPtpDriver]`.
4. Observe driver lifecycle, pipe connections, and IOCTL dispatches:
   ```text
   [VirtualPtpDriver] DriverEntry called.
   [VirtualPtpDriver] EvtDeviceAdd called.
   [VirtualPtpDriver] Named Pipe server listening on \\.\pipe\T650VirtualPtpPipe
   [VirtualPtpDriver] Pipe client connected!
   ```

### 4. UMDF Event Viewer Log
If the driver fails to start:
```powershell
wevtutil sl Microsoft-Windows-DriverFrameworks-UserMode/Operational /e:true
Get-WinEvent -LogName "Microsoft-Windows-DriverFrameworks-UserMode/Operational" -MaxEvents 30 | Format-Table TimeCreated, Id, Message -Wrap
```
SetupAPI log:
```powershell
Get-Content C:\Windows\INF\setupapi.dev.log -Tail 150 | Select-String -Pattern "HIDCLASS|VirtualPtp|mshidumdf|WUDFRd|Exit status"
```
