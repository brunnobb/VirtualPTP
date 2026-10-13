#pragma once

#include <windows.h>
#include <wdf.h>
#include "Device.h"

VOID StartPipeServer(PDEVICE_CONTEXT Context);
VOID StopPipeServer();
