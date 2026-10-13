#pragma once

#include <windows.h>
#include <wdf.h>

NTSTATUS QueueInitialize(_In_ WDFDEVICE Device);
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControl;

NTSTATUS InjectTouchReport(PVOID Context, PVOID ReportBuffer, size_t ReportSize);
