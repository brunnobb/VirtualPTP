#include "Driver.h"
#include "Device.h"
#include "Trace.h"

NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT  DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    WDF_DRIVER_CONFIG config;
    NTSTATUS status;

    PtpLog(L"[VirtualPtpDriver] DriverEntry called. RegistryPath=%s\n",
        (RegistryPath && RegistryPath->Buffer) ? RegistryPath->Buffer : L"(null)");

    WDF_DRIVER_CONFIG_INIT(&config, EvtDeviceAdd);

    status = WdfDriverCreate(
        DriverObject,
        RegistryPath,
        WDF_NO_OBJECT_ATTRIBUTES,
        &config,
        WDF_NO_HANDLE
    );

    PtpLog(L"[VirtualPtpDriver] WdfDriverCreate returned status=0x%08X\n", status);
    return status;
}

NTSTATUS EvtDeviceAdd(
    _In_    WDFDRIVER       Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
)
{
    UNREFERENCED_PARAMETER(Driver);
    PtpLog(L"[VirtualPtpDriver] EvtDeviceAdd called\n");
    NTSTATUS status = DeviceCreate(DeviceInit);
    PtpLog(L"[VirtualPtpDriver] DeviceCreate completed with status=0x%08X\n", status);
    return status;
}
