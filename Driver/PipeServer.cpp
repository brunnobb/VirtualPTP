#include "PipeServer.h"
#include "Queue.h"
#include "PtpTypes.h"
#include "Trace.h"
#include <sddl.h>

#pragma comment(lib, "advapi32.lib")

static HANDLE g_hPipeThread = NULL;
static HANDLE g_hStopEvent = NULL;
static volatile BOOL g_Running = FALSE;

static DWORD WINAPI PipeThreadProc(LPVOID lpParam)
{
    PDEVICE_CONTEXT context = (PDEVICE_CONTEXT)lpParam;
    PtpLog(L"[VirtualPtpDriver] Named Pipe server starting on \\\\.\\pipe\\T650VirtualPtpPipe...\n");

    // Security descriptor: allow Everyone (WD), Admins (BA), SYSTEM (SY)
    // Mandatory Label: Medium Integrity (ME) -> strictly blocks sandboxed browser processes & low-integrity malware
    PSECURITY_DESCRIPTOR pSD = NULL;
    BOOL sddlOk = ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;WD)S:(ML;;NW;;;ME)",
        SDDL_REVISION_1,
        &pSD,
        NULL
    );
    if (!sddlOk) {
        PtpLog(L"[VirtualPtpDriver] ConvertStringSecurityDescriptor failed: %lu\n", GetLastError());
    }

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), pSD, FALSE };

    while (g_Running) {
        // Hardened pipe creation:
        // - FILE_FLAG_FIRST_PIPE_INSTANCE: prevents rogue processes from pre-creating or squatting on the pipe
        // - PIPE_REJECT_REMOTE_CLIENTS: rejects any remote/network connections (local machine only)
        // - nMaxInstances = 1: exclusive single-client lock; bridge holds connection exclusively
        HANDLE hPipe = CreateNamedPipeW(
            L"\\\\.\\pipe\\T650VirtualPtpPipe",
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            1,
            4096,
            4096,
            0,
            &sa
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            PtpLog(L"[VirtualPtpDriver] CreateNamedPipe failed with error %lu\n", GetLastError());
            Sleep(1000);
            continue;
        }

        OVERLAPPED ovConnect = { 0 };
        ovConnect.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        BOOL connected = ConnectNamedPipe(hPipe, &ovConnect);
        if (!connected) {
            DWORD lastErr = GetLastError();
            if (lastErr == ERROR_IO_PENDING) {
                HANDLE waitHandles[2] = { ovConnect.hEvent, g_hStopEvent };
                DWORD waitRes = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
                if (waitRes == WAIT_OBJECT_0 + 1) { // Stop requested
                    CloseHandle(ovConnect.hEvent);
                    CloseHandle(hPipe);
                    break;
                }
            } else if (lastErr != ERROR_PIPE_CONNECTED) {
                PtpLog(L"[VirtualPtpDriver] ConnectNamedPipe failed: %lu\n", lastErr);
                CloseHandle(ovConnect.hEvent);
                CloseHandle(hPipe);
                continue;
            }
        }
        CloseHandle(ovConnect.hEvent);

        // Security check: verify client belongs to active console session (blocks remote/background session hijacking)
        if (ImpersonateNamedPipeClient(hPipe)) {
            HANDLE hToken = NULL;
            if (OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &hToken)) {
                DWORD clientSessionId = 0;
                DWORD retLen = 0;
                if (GetTokenInformation(hToken, TokenSessionId, &clientSessionId, sizeof(clientSessionId), &retLen)) {
                    DWORD activeConsole = WTSGetActiveConsoleSessionId();
                    if (activeConsole != 0xFFFFFFFF && clientSessionId != activeConsole) {
                        PtpLog(L"[VirtualPtpDriver] Security Rejection: client session %lu does not match active console %lu.\n",
                            clientSessionId, activeConsole);
                        CloseHandle(hToken);
                        RevertToSelf();
                        DisconnectNamedPipe(hPipe);
                        CloseHandle(hPipe);
                        continue;
                    }
                }
                CloseHandle(hToken);
            }
            RevertToSelf();
        }

        PtpLog(L"[VirtualPtpDriver] T650 Bridge connected to Named Pipe! Live touch injection active.\n");

        // Report Read Loop
        PTP_TOUCH_REPORT report = { 0 };
        DWORD bytesRead = 0;
        OVERLAPPED ovRead = { 0 };
        ovRead.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

        while (g_Running) {
            ResetEvent(ovRead.hEvent);
            bytesRead = 0;
            BOOL readOk = ReadFile(hPipe, &report, sizeof(PTP_TOUCH_REPORT), &bytesRead, &ovRead);
            if (!readOk) {
                DWORD err = GetLastError();
                if (err == ERROR_IO_PENDING) {
                    HANDLE waitHandles[2] = { ovRead.hEvent, g_hStopEvent };
                    DWORD waitRes = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
                    if (waitRes == WAIT_OBJECT_0 + 1) {
                        CancelIo(hPipe);
                        break;
                    }
                    if (!GetOverlappedResult(hPipe, &ovRead, &bytesRead, FALSE)) {
                        PtpLog(L"[VirtualPtpDriver] Named pipe read ended (error %lu).\n", GetLastError());
                        break;
                    }
                } else {
                    // Client disconnected
                    PtpLog(L"[VirtualPtpDriver] T650 Bridge disconnected from Named Pipe (error %lu).\n", err);
                    break;
                }
            }

            if (bytesRead >= sizeof(UCHAR)) {
                InjectTouchReport(context, &report, bytesRead);
            }
        }

        CloseHandle(ovRead.hEvent);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }

    if (pSD != NULL) {
        LocalFree(pSD);
    }

    PtpLog(L"[VirtualPtpDriver] Pipe server thread terminated.\n");
    return 0;
}

VOID StartPipeServer(PDEVICE_CONTEXT Context)
{
    if (g_Running) return;
    g_Running = TRUE;
    g_hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    g_hPipeThread = CreateThread(NULL, 0, PipeThreadProc, Context, 0, NULL);
}

VOID StopPipeServer()
{
    if (!g_Running) return;
    g_Running = FALSE;
    if (g_hStopEvent != NULL) {
        SetEvent(g_hStopEvent);
    }
    if (g_hPipeThread != NULL) {
        WaitForSingleObject(g_hPipeThread, 2000);
        CloseHandle(g_hPipeThread);
        g_hPipeThread = NULL;
    }
    if (g_hStopEvent != NULL) {
        CloseHandle(g_hStopEvent);
        g_hStopEvent = NULL;
    }
}
