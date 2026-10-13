#pragma once

#include <windows.h>
#include <stdio.h>

#pragma comment(lib, "advapi32.lib")

inline void PtpLog(const WCHAR* format, ...)
{
    WCHAR buffer[1024];
    va_list args;
    va_start(args, format);
    _vsnwprintf_s(buffer, _countof(buffer), _TRUNCATE, format, args);
    va_end(args);

    // 1. OutputDebugString for DebugView / WinDbg
    OutputDebugStringW(buffer);

    // 2. Windows Event Log (Application log under source "VirtualPtpDriver")
    HANDLE hEventSource = RegisterEventSourceW(NULL, L"VirtualPtpDriver");
    if (hEventSource != NULL) {
        LPCWSTR strings[1] = { buffer };
        ReportEventW(
            hEventSource,
            EVENTLOG_INFORMATION_TYPE,
            0,
            0,
            NULL,
            1,
            0,
            strings,
            NULL
        );
        DeregisterEventSource(hEventSource);
    }

    // 3. Persistent File Log in C:\ProgramData\LogitechT650\VirtualPtpDriver.log
    CreateDirectoryW(L"C:\\ProgramData", NULL);
    CreateDirectoryW(L"C:\\ProgramData\\LogitechT650", NULL);
    FILE* f = NULL;
    if (_wfopen_s(&f, L"C:\\ProgramData\\LogitechT650\\VirtualPtpDriver.log", L"a, ccs=UTF-8") == 0 && f != NULL) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fwprintf(f, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] %s",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buffer);
        fclose(f);
    }
}
