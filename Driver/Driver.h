#pragma once

#include <windows.h>
#include <wdf.h>
#include "PtpTypes.h"
#include "PtpDescriptor.h"

EXTERN_C_START

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD EvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP EvtDriverContextCleanup;

EXTERN_C_END
