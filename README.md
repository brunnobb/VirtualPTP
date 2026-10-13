# Virtual Precision Touchpad (VirtualPTP) Driver

[![Windows UMDF 2.15](https://img.shields.io/badge/Windows-UMDF%202.15-0078D6.svg)](https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/user-mode-driver-framework-architecture)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

Virtual Precision Touchpad (`VirtualPTP`) is a high-performance Windows User-Mode Driver Framework (UMDF 2) virtual HID minidriver that registers a fully compliant Microsoft Precision Touchpad (PTP) device with the Windows Touch Stack.

It allows user-mode daemons, bridge applications, or hardware adapters to inject raw multi-touch contact frames directly into Windows 10 and 11, giving any trackpad device native multi-touch gestures (pinching, two-finger scroll, 3/4-finger desktop switching, swipe navigation) without needing custom gesture emulation hacks.

---

## Architecture Overview

```text
┌────────────────────────────────────────────────────────┐
│  User-Mode Touch Daemon / Hardware Bridge              │
│  - Reads raw touch reports from hardware               │
│  - Streams frames over Named Pipe or IOCTL             │
└──────────────────────────┬─────────────────────────────┘
                           │ Named Pipe: \\.\pipe\T650VirtualPtpPipe
                           │ or IOCTL_PTP_INJECT_REPORT (0x00222004)
┌──────────────────────────▼─────────────────────────────┐
│  VirtualPTP Driver (UMDF 2.15 in WUDFHost.exe)         │
│  - Microsoft PTP TLC Report Descriptor (565 bytes)     │
│  - Top-Level Collections: PTP Touch, Mouse, Config     │
│  - Exposes GUID_DEVINTERFACE_T650_VIRTUAL_PTP          │
└──────────────────────────┬─────────────────────────────┘
                           │ HID Miniport Interface
┌──────────────────────────▼─────────────────────────────┐
│  Windows Touch Stack (mshidumdf / hidclass.sys)        │
│  - Native Windows Gestures, Inking & Edge Gestures     │
└────────────────────────────────────────────────────────┘
```

### Key Technical Characteristics
* **Target Framework:** **UMDF 2.15** (`UmdfLibraryVersion = 2.15.0`). Universally supported on all Windows 10 releases (1507 through 22H2) and Windows 11 (21H2 through 24H2) without version mismatches (`STATUS_INVALID_PARAMETER` / `3489660941`).
* **Zero Runtime Dependencies:** Statically linked with C Runtime (`/MT`) for zero VC++ Redistributable requirements.
* **Dual IPC Interface:**
  1. **Named Pipe (`\\.\pipe\T650VirtualPtpPipe`):** Ultra-low-latency named pipe server with standard user access permissions, bypassing Windows HIDCLASS device-link restrictions.
  2. **Device IOCTL (`0x00222004`):** Direct buffered IOCTL injection via device interface `GUID_DEVINTERFACE_T650_VIRTUAL_PTP` (`{A50E5B44-5D88-4A56-BE34-F18A56DF762B}`).
* **Built-in Tracing:** Real-time debug logging via `OutputDebugStringW` for easy monitoring with Sysinternals DebugView.

---

## Directory Structure

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
│   └── Build-Driver-Installer.ps1 # MSI compilation script
├── Build-All.ps1               # Full build, sign, and package pipeline
├── Bump-Version.ps1            # Automated SemVer bumper
└── Dist/                       # Built MSIs and SHA-256 checksums
```

---

## Prerequisites

1. **Visual Studio 2022** (Desktop development with C++)
2. **Windows Driver Kit (WDK) 10/11** (compatible with VS 2022)
3. **WiX Toolset v3.14** (`C:\Program Files (x86)\WiX Toolset v3.14\bin`)
4. **PowerShell 7** (`pwsh`)

---

## Building and Packaging

### Automated One-Step Build & Package
To compile the driver, generate the signed catalog, and build the MSI installer in one command:
```powershell
pwsh -File "Build-All.ps1"
```

### Version Bumping
To bump the version across all driver source files, INF, and WiX installers:
```powershell
pwsh -File "Bump-Version.ps1" -NewVersion "1.1.7"
```

Output artifacts will be generated in `Dist/`:
* `VirtualPtpDriver-Setup-v<Version>.msi`
* `VirtualPtpDriver-Setup.msi`
* `SHA256SUMS.txt`

---

## Installation & Testing

### 1. Enable Test-Signing on the Test Machine
Because the driver uses a development code-signing certificate, enable Windows test mode:
```powershell
bcdedit /set testsigning on
Restart-Computer
```

### 2. Install via MSI
Run `VirtualPtpDriver-Setup-v<Version>.msi` as Administrator. The installer will:
1. Import `T650TestCert.cer` into `Root` and `TrustedPublisher` stores.
2. Stage `VirtualPtpDriver.inf` into the Windows DriverStore via `pnputil`.
3. Create the virtual device node `Root\T650VirtualPtp` via `devcon.exe`.

### 3. Verify in Device Manager
Under **Human Interface Devices**, verify that **Virtual Precision Touchpad** is active without yellow exclamation marks.

---

## Diagnostics & Debugging

* **DebugView:** Run Sysinternals `Dbgview.exe` as Administrator, enable **Capture Global Win32**, and filter for `[VirtualPtpDriver]`.
* **UMDF Operational Log:**
  ```powershell
  wevtutil sl Microsoft-Windows-DriverFrameworks-UserMode/Operational /e:true
  Get-WinEvent -LogName "Microsoft-Windows-DriverFrameworks-UserMode/Operational" -MaxEvents 30
  ```

---

## License

MIT License. See [LICENSE](LICENSE) for details.
