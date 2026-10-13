#pragma once

#include <windows.h>
#include <wdf.h>

// GUID for T650 Virtual PTP Device Interface: {A50E5B44-5D88-4A56-BE34-F18A56DF762B}
DEFINE_GUID(GUID_DEVINTERFACE_T650_VIRTUAL_PTP,
    0xa50e5b44, 0x5d88, 0x4a56, 0xbe, 0x34, 0xf1, 0x8a, 0x56, 0xdf, 0x76, 0x2b);

typedef struct _DEVICE_CONTEXT {
    WDFDEVICE Device;
    WDFQUEUE DefaultQueue;
    WDFQUEUE ManualReportQueue;
    UCHAR InputMode;        // Report ID 3: 0 = Mouse, 3 = PTP (default 3)
    UCHAR FunctionSwitch;   // Report ID 4: 1 = Button, 2 = Surface, 3 = Both (default 3)
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

NTSTATUS DeviceCreate(_Inout_ PWDFDEVICE_INIT DeviceInit);

EVT_WDF_DEVICE_FILE_CREATE EvtDeviceFileCreate;
EVT_WDF_FILE_CLOSE         EvtFileClose;
EVT_WDF_FILE_CLEANUP       EvtFileCleanup;
