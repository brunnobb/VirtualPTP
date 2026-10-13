#include <windows.h>
#include <initguid.h>
#include "Device.h"
#include "Queue.h"
#include "PipeServer.h"
#include "Trace.h"

VOID EvtDeviceFileCreate(
    _In_ WDFDEVICE     Device,
    _In_ WDFREQUEST    Request,
    _In_ WDFFILEOBJECT FileObject
)
{
    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(FileObject);
    PtpLog(L"[VirtualPtpDriver] EvtDeviceFileCreate: client opened handle to Virtual PTP\n");
    WdfRequestComplete(Request, STATUS_SUCCESS);
}

VOID EvtFileClose(
    _In_ WDFFILEOBJECT FileObject
)
{
    UNREFERENCED_PARAMETER(FileObject);
    PtpLog(L"[VirtualPtpDriver] EvtFileClose: client closed handle\n");
}

VOID EvtFileCleanup(
    _In_ WDFFILEOBJECT FileObject
)
{
    UNREFERENCED_PARAMETER(FileObject);
    PtpLog(L"[VirtualPtpDriver] EvtFileCleanup: client handle cleanup\n");
}

VOID EvtDeviceContextCleanup(
    _In_ WDFOBJECT Object
)
{
    UNREFERENCED_PARAMETER(Object);
    PtpLog(L"[VirtualPtpDriver] EvtDeviceContextCleanup: stopping Named Pipe server...\n");
    StopPipeServer();
}

NTSTATUS DeviceCreate(_Inout_ PWDFDEVICE_INIT DeviceInit)
{
    WDF_OBJECT_ATTRIBUTES deviceAttributes;
    WDFDEVICE device;
    NTSTATUS status;
    PDEVICE_CONTEXT context;
    WDF_FILEOBJECT_CONFIG fileConfig;

    PtpLog(L"[VirtualPtpDriver] DeviceCreate: Setting filter...\n");
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);
    deviceAttributes.EvtCleanupCallback = EvtDeviceContextCleanup;

    // Filter driver under MsHidUmdf.sys
    WdfFdoInitSetFilter(DeviceInit);

    // Configure File Object handling so user-mode CreateFile succeeds
    WDF_FILEOBJECT_CONFIG_INIT(
        &fileConfig,
        EvtDeviceFileCreate,
        EvtFileClose,
        EvtFileCleanup
    );
    fileConfig.AutoForwardCleanupClose = WdfFalse;

    WdfDeviceInitSetFileObjectConfig(
        DeviceInit,
        &fileConfig,
        WDF_NO_OBJECT_ATTRIBUTES
    );

    status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);
    PtpLog(L"[VirtualPtpDriver] DeviceCreate: WdfDeviceCreate status=0x%08X\n", status);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    context = DeviceGetContext(device);
    context->Device = device;
    context->InputMode = 0x03; // Default to Precision Touchpad mode
    context->FunctionSwitch = 0x03; // Default to Surface contacts + Button state enabled

    // Register device interface so the user-space Bridge Daemon can send IOCTLs
    status = WdfDeviceCreateDeviceInterface(
        device,
        &GUID_DEVINTERFACE_T650_VIRTUAL_PTP,
        NULL
    );
    PtpLog(L"[VirtualPtpDriver] DeviceCreate: WdfDeviceCreateDeviceInterface status=0x%08X\n", status);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = QueueInitialize(device);
    PtpLog(L"[VirtualPtpDriver] DeviceCreate: QueueInitialize status=0x%08X\n", status);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    StartPipeServer(context);
    return STATUS_SUCCESS;
}
