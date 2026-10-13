#include <windows.h>
#include "HidMinip.h"
#include "Queue.h"
#include "Device.h"
#include "PtpTypes.h"
#include "PtpDescriptor.h"
#include "Trace.h"

// String descriptors for device identification
static const WCHAR g_ManufacturerString[] = L"Logitech";
static const WCHAR g_ProductString[] = L"Logitech T650 Precision Touchpad (Virtual PTP)";
static const WCHAR g_SerialNumberString[] = L"00000001";

// Microsoft Windows Precision Touchpad Hardware Quality Assurance (PTPHQA) default blob
static const UCHAR g_PtpCertificationBlob[256] = {
    0xfc, 0x28, 0xfe, 0x84, 0x40, 0xcb, 0x9a, 0x87, 0x0d, 0xbe, 0x57, 0x3c, 0xb6, 0x70, 0x09, 0x88, 0x07,
    0x97, 0x2d, 0x2b, 0xe3, 0x38, 0x34, 0xb6, 0x6c, 0xed, 0xb0, 0xf7, 0xe5, 0x9c, 0xf6, 0xc2, 0x2e, 0x84,
    0x1b, 0xe8, 0xb4, 0x51, 0x78, 0x43, 0x1f, 0x28, 0x4b, 0x7c, 0x2d, 0x53, 0xaf, 0xfc, 0x47, 0x70, 0x1b,
    0x59, 0x6f, 0x74, 0x43, 0xc4, 0xf3, 0x47, 0x18, 0x53, 0x1a, 0xa2, 0xa1, 0x71, 0xc7, 0x95, 0x0e, 0x31,
    0x55, 0x21, 0xd3, 0xb5, 0x1e, 0xe9, 0x0c, 0xba, 0xec, 0xb8, 0x89, 0x19, 0x3e, 0xb3, 0xaf, 0x75, 0x81,
    0x9d, 0x53, 0xb9, 0x41, 0x57, 0xf4, 0x6d, 0x39, 0x25, 0x29, 0x7c, 0x87, 0xd9, 0xb4, 0x98, 0x45, 0x7d,
    0xa7, 0x26, 0x9c, 0x65, 0x3b, 0x85, 0x68, 0x89, 0xd7, 0x3b, 0xbd, 0xff, 0x14, 0x67, 0xf2, 0x2b, 0xf0,
    0x2a, 0x41, 0x54, 0xf0, 0xfd, 0x2c, 0x66, 0x7c, 0xf8, 0xc0, 0x8f, 0x33, 0x13, 0x03, 0xf1, 0xd3, 0xc1, 0x0b,
    0x89, 0xd9, 0x1b, 0x62, 0xcd, 0x51, 0xb7, 0x80, 0xb8, 0xaf, 0x3a, 0x10, 0xc1, 0x8a, 0x5b, 0xe8, 0x8a,
    0x56, 0xf0, 0x8c, 0xaa, 0xfa, 0x35, 0xe9, 0x42, 0xc4, 0xd8, 0x55, 0xc3, 0x38, 0xcc, 0x2b, 0x53, 0x5c,
    0x69, 0x52, 0xd5, 0xc8, 0x73, 0x02, 0x38, 0x7c, 0x73, 0xb6, 0x41, 0xe7, 0xff, 0x05, 0xd8, 0x2b, 0x79,
    0x9a, 0xe2, 0x34, 0x60, 0x8f, 0xa3, 0x32, 0x1f, 0x09, 0x78, 0x62, 0xbc, 0x80, 0xe3, 0x0f, 0xbd, 0x65,
    0x20, 0x08, 0x13, 0xc1, 0xe2, 0xee, 0x53, 0x2d, 0x86, 0x7e, 0xa7, 0x5a, 0xc5, 0xd3, 0x7d, 0x98, 0xbe,
    0x31, 0x48, 0x1f, 0xfb, 0xda, 0xaf, 0xa2, 0xa8, 0x6a, 0x89, 0xd6, 0xbf, 0xf2, 0xd3, 0x32, 0x2a, 0x9a,
    0xe4, 0xcf, 0x17, 0xb7, 0xb8, 0xf4, 0xe1, 0x33, 0x08, 0x24, 0x8b, 0xc4, 0x43, 0xa5, 0xe5, 0x24, 0xc2
};

// Helper: copy buffer to output memory of a WDF request (handles METHOD_NEITHER in UMDF 2)
static NTSTATUS RequestCopyFromBuffer(
    _In_ WDFREQUEST Request,
    _In_ PVOID SourceBuffer,
    _In_ size_t NumBytesToCopyFrom
)
{
    WDFMEMORY memory;
    size_t outputBufferLength = 0;

    NTSTATUS status = WdfRequestRetrieveOutputMemory(Request, &memory);
    if (!NT_SUCCESS(status)) {
        PtpLog(L"[VirtualPtpDriver] RequestCopyFromBuffer: WdfRequestRetrieveOutputMemory failed 0x%08X\n", status);
        return status;
    }

    WdfMemoryGetBuffer(memory, &outputBufferLength);
    if (outputBufferLength < NumBytesToCopyFrom) {
        PtpLog(L"[VirtualPtpDriver] RequestCopyFromBuffer: buffer too small (out=%zu, needed=%zu)\n",
            outputBufferLength, NumBytesToCopyFrom);
        return STATUS_INVALID_BUFFER_SIZE;
    }

    status = WdfMemoryCopyFromBuffer(memory, 0, SourceBuffer, NumBytesToCopyFrom);
    if (NT_SUCCESS(status)) {
        WdfRequestSetInformation(Request, NumBytesToCopyFrom);
    } else {
        PtpLog(L"[VirtualPtpDriver] RequestCopyFromBuffer: WdfMemoryCopyFromBuffer failed 0x%08X\n", status);
    }
    return status;
}

// Helper: extract HID transfer packet for reading from device in UMDF 2
// Input buffer contains Report ID; output buffer receives the report
static NTSTATUS RequestGetHidXferPacket_ToReadFromDevice(
    _In_  WDFREQUEST Request,
    _Out_ HID_XFER_PACKET* Packet
)
{
    WDFMEMORY inputMemory;
    WDFMEMORY outputMemory;
    size_t inputBufferLength = 0;
    size_t outputBufferLength = 0;
    PVOID inputBuffer = NULL;
    PVOID outputBuffer = NULL;

    NTSTATUS status = WdfRequestRetrieveInputMemory(Request, &inputMemory);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    inputBuffer = WdfMemoryGetBuffer(inputMemory, &inputBufferLength);
    if (inputBufferLength < sizeof(UCHAR)) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    Packet->reportId = *(PUCHAR)inputBuffer;

    status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    outputBuffer = WdfMemoryGetBuffer(outputMemory, &outputBufferLength);
    Packet->reportBuffer = (PUCHAR)outputBuffer;
    Packet->reportBufferLen = (ULONG)outputBufferLength;

    return STATUS_SUCCESS;
}

// Helper: extract HID transfer packet for writing to device in UMDF 2
// Report ID is encoded in output buffer length by MsHidUmdf.sys; input buffer contains report data
static NTSTATUS RequestGetHidXferPacket_ToWriteToDevice(
    _In_  WDFREQUEST Request,
    _Out_ HID_XFER_PACKET* Packet
)
{
    NTSTATUS status;
    WDFMEMORY inputMemory;
    WDFMEMORY outputMemory;
    size_t inputBufferLength = 0;
    size_t outputBufferLength = 0;
    PVOID inputBuffer = NULL;

    status = WdfRequestRetrieveOutputMemory(Request, &outputMemory);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    WdfMemoryGetBuffer(outputMemory, &outputBufferLength);
    Packet->reportId = (UCHAR)outputBufferLength;

    status = WdfRequestRetrieveInputMemory(Request, &inputMemory);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    inputBuffer = WdfMemoryGetBuffer(inputMemory, &inputBufferLength);

    Packet->reportBuffer = (PUCHAR)inputBuffer;
    Packet->reportBufferLen = (ULONG)inputBufferLength;

    return status;
}

// Helper: extract string ID from IOCTL_HID_GET_STRING
static NTSTATUS GetStringId(
    _In_  WDFREQUEST Request,
    _Out_ ULONG* StringId,
    _Out_ ULONG* LanguageId
)
{
    WDFMEMORY inputMemory;
    size_t inputBufferLength = 0;

    NTSTATUS status = WdfRequestRetrieveInputMemory(Request, &inputMemory);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    PVOID inputBuffer = WdfMemoryGetBuffer(inputMemory, &inputBufferLength);
    if (inputBufferLength < sizeof(ULONG)) {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    ULONG inputValue = *(PULONG)inputBuffer;
    *StringId = (inputValue & 0xFFFF);
    *LanguageId = (inputValue >> 16);
    return STATUS_SUCCESS;
}

// Helper: handle GetFeature reports
// - Report 2: Device Capabilities (Max Contacts = 5, ClickPad)
// - Report 3: Input Mode (PTP mode = 3)
// - Report 4: Function Switch (Surface + Button = 3)
// - Report 7: PTPHQA Certification Blob (256 bytes)
static NTSTATUS HandleGetFeature(
    _In_ PDEVICE_CONTEXT Context,
    _In_ WDFREQUEST Request,
    _In_ ULONG IoControlCode
)
{
    HID_XFER_PACKET packet = { 0 };
    NTSTATUS status = STATUS_SUCCESS;

    if (IoControlCode == IOCTL_UMDF_HID_GET_FEATURE) {
        status = RequestGetHidXferPacket_ToReadFromDevice(Request, &packet);
    } else {
        // Kernel-mode / direct IOCTL_HID_GET_FEATURE
        PHID_XFER_PACKET pKernelPacket = NULL;
        status = WdfRequestRetrieveInputBuffer(Request, sizeof(HID_XFER_PACKET), (PVOID*)&pKernelPacket, NULL);
        if (NT_SUCCESS(status) && pKernelPacket != NULL) {
            packet = *pKernelPacket;
        }
    }

    if (!NT_SUCCESS(status)) {
        PtpLog(L"[VirtualPtpDriver] HandleGetFeature: RequestGetHidXferPacket failed status=0x%08X\n", status);
        return status;
    }

    if (packet.reportBuffer == NULL || packet.reportBufferLen == 0) {
        PtpLog(L"[VirtualPtpDriver] HandleGetFeature: invalid reportBuffer=NULL or len=0\n");
        return STATUS_INVALID_BUFFER_SIZE;
    }

    // Fallback: if reportId was not in input memory, check first byte of output buffer
    if (packet.reportId == 0 && packet.reportBufferLen > 0) {
        packet.reportId = packet.reportBuffer[0];
    }

    PtpLog(L"[VirtualPtpDriver] HandleGetFeature: reportId=0x%02X, bufLen=%lu\n",
        packet.reportId, packet.reportBufferLen);

    switch (packet.reportId)
    {
    case 0x02: // Capabilities (Max Contacts = 5, Pad Type = 0)
    {
        if (packet.reportBufferLen >= 2) {
            packet.reportBuffer[0] = 0x02; // Report ID
            packet.reportBuffer[1] = 0x05; // 5 contacts, ClickPad
            WdfRequestSetInformation(Request, 2);
            PtpLog(L"[VirtualPtpDriver] HandleGetFeature: Caps returning 2 bytes\n");
            return STATUS_SUCCESS;
        } else if (packet.reportBufferLen == 1) {
            packet.reportBuffer[0] = 0x05;
            WdfRequestSetInformation(Request, 1);
            PtpLog(L"[VirtualPtpDriver] HandleGetFeature: Caps returning 1 byte\n");
            return STATUS_SUCCESS;
        }
        PtpLog(L"[VirtualPtpDriver] HandleGetFeature: Caps buffer too small (%lu)\n", packet.reportBufferLen);
        return STATUS_INVALID_BUFFER_SIZE;
    }

    case 0x03: // Input Mode (3 = PTP Mode)
    {
        if (packet.reportBufferLen >= 2) {
            packet.reportBuffer[0] = 0x03; // Report ID
            packet.reportBuffer[1] = Context->InputMode;
            WdfRequestSetInformation(Request, 2);
            PtpLog(L"[VirtualPtpDriver] HandleGetFeature: InputMode returning 2 bytes (0x%02X)\n", Context->InputMode);
            return STATUS_SUCCESS;
        } else if (packet.reportBufferLen == 1) {
            packet.reportBuffer[0] = Context->InputMode;
            WdfRequestSetInformation(Request, 1);
            PtpLog(L"[VirtualPtpDriver] HandleGetFeature: InputMode returning 1 byte (0x%02X)\n", Context->InputMode);
            return STATUS_SUCCESS;
        }
        PtpLog(L"[VirtualPtpDriver] HandleGetFeature: InputMode buffer too small (%lu)\n", packet.reportBufferLen);
        return STATUS_INVALID_BUFFER_SIZE;
    }

    case 0x04: // Function Switch (3 = Surface + Button enabled)
    {
        if (packet.reportBufferLen >= 2) {
            packet.reportBuffer[0] = 0x04; // Report ID
            packet.reportBuffer[1] = Context->FunctionSwitch;
            WdfRequestSetInformation(Request, 2);
            PtpLog(L"[VirtualPtpDriver] HandleGetFeature: FunctionSwitch returning 2 bytes (0x%02X)\n", Context->FunctionSwitch);
            return STATUS_SUCCESS;
        } else if (packet.reportBufferLen == 1) {
            packet.reportBuffer[0] = Context->FunctionSwitch;
            WdfRequestSetInformation(Request, 1);
            PtpLog(L"[VirtualPtpDriver] HandleGetFeature: FunctionSwitch returning 1 byte (0x%02X)\n", Context->FunctionSwitch);
            return STATUS_SUCCESS;
        }
        PtpLog(L"[VirtualPtpDriver] HandleGetFeature: FunctionSwitch buffer too small (%lu)\n", packet.reportBufferLen);
        return STATUS_INVALID_BUFFER_SIZE;
    }

    case 0x07: // PTPHQA Certification Blob (256 bytes)
    {
        if (packet.reportBufferLen >= 257) {
            packet.reportBuffer[0] = 0x07; // Report ID
            RtlCopyMemory(&packet.reportBuffer[1], g_PtpCertificationBlob, sizeof(g_PtpCertificationBlob));
            WdfRequestSetInformation(Request, 257);
            PtpLog(L"[VirtualPtpDriver] HandleGetFeature: PTPHQA returning 257 bytes\n");
            return STATUS_SUCCESS;
        } else if (packet.reportBufferLen == 256) {
            RtlCopyMemory(packet.reportBuffer, g_PtpCertificationBlob, sizeof(g_PtpCertificationBlob));
            WdfRequestSetInformation(Request, 256);
            PtpLog(L"[VirtualPtpDriver] HandleGetFeature: PTPHQA returning 256 bytes\n");
            return STATUS_SUCCESS;
        }
        PtpLog(L"[VirtualPtpDriver] HandleGetFeature: PTPHQA buffer too small (%lu)\n", packet.reportBufferLen);
        return STATUS_INVALID_BUFFER_SIZE;
    }

    default:
        PtpLog(L"[VirtualPtpDriver] HandleGetFeature: Unknown reportId 0x%02X -> STATUS_INVALID_PARAMETER\n", packet.reportId);
        return STATUS_INVALID_PARAMETER;
    }
}

// Helper: handle SetFeature reports
// - Report 3: Input Mode (host selects Mouse or PTP mode)
// - Report 4: Function Switch (host selects Surface/Button reporting)
static NTSTATUS HandleSetFeature(
    _In_ PDEVICE_CONTEXT Context,
    _In_ WDFREQUEST Request,
    _In_ ULONG IoControlCode,
    _In_ size_t InputBufferLength
)
{
    UNREFERENCED_PARAMETER(InputBufferLength);
    HID_XFER_PACKET packet = { 0 };
    NTSTATUS status = STATUS_SUCCESS;

    if (IoControlCode == IOCTL_UMDF_HID_SET_FEATURE) {
        status = RequestGetHidXferPacket_ToWriteToDevice(Request, &packet);
    } else {
        PHID_XFER_PACKET pKernelPacket = NULL;
        status = WdfRequestRetrieveInputBuffer(Request, sizeof(HID_XFER_PACKET), (PVOID*)&pKernelPacket, NULL);
        if (NT_SUCCESS(status) && pKernelPacket != NULL) {
            packet = *pKernelPacket;
        }
    }

    if (!NT_SUCCESS(status)) {
        PtpLog(L"[VirtualPtpDriver] HandleSetFeature: RequestGetHidXferPacket failed status=0x%08X\n", status);
        return status;
    }

    if (packet.reportBuffer == NULL || packet.reportBufferLen == 0) {
        PtpLog(L"[VirtualPtpDriver] HandleSetFeature: invalid reportBuffer=NULL or len=0\n");
        return STATUS_INVALID_BUFFER_SIZE;
    }

    UCHAR reportId = packet.reportId;
    if (reportId == 0 && packet.reportBufferLen >= 1) {
        reportId = packet.reportBuffer[0];
    }

    UCHAR value = packet.reportBuffer[0];
    if (packet.reportBufferLen >= 2 && packet.reportBuffer[0] == reportId) {
        value = packet.reportBuffer[1];
    }

    PtpLog(L"[VirtualPtpDriver] HandleSetFeature: reportId=0x%02X, value=0x%02X, len=%lu\n",
        reportId, value, packet.reportBufferLen);

    if (reportId == 0x03) {
        Context->InputMode = value;
        PtpLog(L"[VirtualPtpDriver] HandleSetFeature: Set InputMode to 0x%02X\n", value);
    } else if (reportId == 0x04) {
        Context->FunctionSwitch = value;
        PtpLog(L"[VirtualPtpDriver] HandleSetFeature: Set FunctionSwitch to 0x%02X\n", value);
    } else {
        PtpLog(L"[VirtualPtpDriver] HandleSetFeature: Unknown reportId 0x%02X\n", reportId);
        return STATUS_INVALID_PARAMETER;
    }

    WdfRequestSetInformation(Request, packet.reportBufferLen);
    return STATUS_SUCCESS;
}

NTSTATUS QueueInitialize(_In_ WDFDEVICE Device)
{
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDFQUEUE queue;
    NTSTATUS status;
    PDEVICE_CONTEXT context = DeviceGetContext(Device);

    PtpLog(L"[VirtualPtpDriver] QueueInitialize: creating default parallel queue...\n");
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
    queueConfig.EvtIoDeviceControl = EvtIoDeviceControl;

    status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
    PtpLog(L"[VirtualPtpDriver] QueueInitialize: WdfIoQueueCreate (default) status=0x%08X\n", status);
    if (!NT_SUCCESS(status)) {
        return status;
    }
    context->DefaultQueue = queue;

    // Create a manual queue for pending HID read reports
    PtpLog(L"[VirtualPtpDriver] QueueInitialize: creating manual report queue...\n");
    WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchManual);
    status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &context->ManualReportQueue);
    PtpLog(L"[VirtualPtpDriver] QueueInitialize: WdfIoQueueCreate (manual) status=0x%08X\n", status);
    return status;
}

VOID EvtIoDeviceControl(
    _In_ WDFQUEUE   Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength,
    _In_ size_t     InputBufferLength,
    _In_ ULONG      IoControlCode
)
{
    NTSTATUS status = STATUS_SUCCESS;
    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    PDEVICE_CONTEXT context = DeviceGetContext(device);
    BOOLEAN completeRequest = TRUE;

    if (IoControlCode != IOCTL_HID_READ_REPORT && IoControlCode != IOCTL_PTP_INJECT_REPORT) {
        PtpLog(L"[VirtualPtpDriver] EvtIoDeviceControl: IOCTL=0x%08X (InLen=%zu, OutLen=%zu)\n",
            IoControlCode, InputBufferLength, OutputBufferLength);
    }

    switch (IoControlCode)
    {
    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
    {
        HID_DESCRIPTOR hidDesc = { 0 };
        hidDesc.bLength = sizeof(HID_DESCRIPTOR);
        hidDesc.bDescriptorType = HID_HID_DESCRIPTOR_TYPE;
        hidDesc.bcdHID = 0x0100;
        hidDesc.bCountry = 0;
        hidDesc.bNumDescriptors = 1;
        hidDesc.DescriptorList[0].bReportType = HID_REPORT_DESCRIPTOR_TYPE;
        hidDesc.DescriptorList[0].wReportLength = (USHORT)g_PtpReportDescriptorSize;

        status = RequestCopyFromBuffer(Request, &hidDesc, hidDesc.bLength);
        PtpLog(L"[VirtualPtpDriver] -> IOCTL_HID_GET_DEVICE_DESCRIPTOR: descSize=%u, reportDescLen=%u, status=0x%08X\n",
            hidDesc.bLength, hidDesc.DescriptorList[0].wReportLength, status);
        break;
    }

    case IOCTL_HID_GET_REPORT_DESCRIPTOR:
    {
        status = RequestCopyFromBuffer(Request, (PVOID)g_PtpReportDescriptor, g_PtpReportDescriptorSize);
        PtpLog(L"[VirtualPtpDriver] -> IOCTL_HID_GET_REPORT_DESCRIPTOR: reportDescLen=%lu, status=0x%08X\n",
            g_PtpReportDescriptorSize, status);
        break;
    }

    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
    {
        HID_DEVICE_ATTRIBUTES attr = { 0 };
        attr.Size = sizeof(HID_DEVICE_ATTRIBUTES);
        attr.VendorID = 0x046D;  // Logitech VID
        attr.ProductID = 0xB008; // Logitech T650 Touchpad PID
        attr.VersionNumber = 0x0100;

        status = RequestCopyFromBuffer(Request, &attr, sizeof(HID_DEVICE_ATTRIBUTES));
        PtpLog(L"[VirtualPtpDriver] -> IOCTL_HID_GET_DEVICE_ATTRIBUTES: VID=0x%04X, PID=0x%04X, status=0x%08X\n",
            attr.VendorID, attr.ProductID, status);
        break;
    }

    case IOCTL_HID_READ_REPORT:
    {
        static LONG s_ReadCount = 0;
        if (InterlockedIncrement(&s_ReadCount) == 1) {
            PtpLog(L"[VirtualPtpDriver] -> First IOCTL_HID_READ_REPORT received from OS touch stack.\n");
        }
        // Forward read request to manual report queue
        // It stays pending until an input report is injected from the bridge
        status = WdfRequestForwardToIoQueue(Request, context->ManualReportQueue);
        if (!NT_SUCCESS(status)) {
            PtpLog(L"[VirtualPtpDriver] -> IOCTL_HID_READ_REPORT: WdfRequestForwardToIoQueue failed 0x%08X\n", status);
            WdfRequestComplete(Request, status);
        }
        return;
    }

    case IOCTL_UMDF_HID_GET_FEATURE:
    case IOCTL_HID_GET_FEATURE:
    {
        status = HandleGetFeature(context, Request, IoControlCode);
        PtpLog(L"[VirtualPtpDriver] -> GET_FEATURE: status=0x%08X\n", status);
        break;
    }

    case IOCTL_UMDF_HID_SET_FEATURE:
    case IOCTL_HID_SET_FEATURE:
    {
        status = HandleSetFeature(context, Request, IoControlCode, InputBufferLength);
        PtpLog(L"[VirtualPtpDriver] -> SET_FEATURE: status=0x%08X\n", status);
        break;
    }

    case IOCTL_UMDF_HID_GET_INPUT_REPORT:
    case IOCTL_HID_GET_INPUT_REPORT:
    {
        HID_XFER_PACKET packet = { 0 };
        if (IoControlCode == IOCTL_UMDF_HID_GET_INPUT_REPORT) {
            status = RequestGetHidXferPacket_ToReadFromDevice(Request, &packet);
        } else {
            PHID_XFER_PACKET pKernelPacket = NULL;
            status = WdfRequestRetrieveInputBuffer(Request, sizeof(HID_XFER_PACKET), (PVOID*)&pKernelPacket, NULL);
            if (NT_SUCCESS(status) && pKernelPacket != NULL) {
                packet = *pKernelPacket;
            }
        }
        if (NT_SUCCESS(status) && packet.reportBuffer != NULL && packet.reportBufferLen > 0) {
            if (packet.reportId == 0x01 && packet.reportBufferLen >= sizeof(PTP_TOUCH_REPORT)) {
                PTP_TOUCH_REPORT idleReport = { 0 };
                idleReport.ReportId = 0x01;
                RtlCopyMemory(packet.reportBuffer, &idleReport, sizeof(PTP_TOUCH_REPORT));
                WdfRequestSetInformation(Request, sizeof(PTP_TOUCH_REPORT));
                status = STATUS_SUCCESS;
            } else {
                RtlZeroMemory(packet.reportBuffer, packet.reportBufferLen);
                if (packet.reportId != 0 && packet.reportBufferLen > 0) {
                    packet.reportBuffer[0] = packet.reportId;
                }
                WdfRequestSetInformation(Request, packet.reportBufferLen);
                status = STATUS_SUCCESS;
            }
        }
        PtpLog(L"[VirtualPtpDriver] -> GET_INPUT_REPORT: reportId=0x%02X, status=0x%08X\n", packet.reportId, status);
        break;
    }

    case IOCTL_UMDF_HID_SET_OUTPUT_REPORT:
    case IOCTL_HID_SET_OUTPUT_REPORT:
    {
        WdfRequestSetInformation(Request, InputBufferLength);
        status = STATUS_SUCCESS;
        PtpLog(L"[VirtualPtpDriver] -> SET_OUTPUT_REPORT: InLen=%zu, status=0x%08X\n", InputBufferLength, status);
        break;
    }

    case IOCTL_HID_ACTIVATE_DEVICE:
    case IOCTL_HID_DEACTIVATE_DEVICE:
    {
        status = STATUS_SUCCESS;
        PtpLog(L"[VirtualPtpDriver] -> ACTIVATE/DEACTIVATE: status=0x%08X\n", status);
        break;
    }

    case IOCTL_HID_GET_STRING:
    case IOCTL_HID_GET_INDEXED_STRING:
    {
        ULONG stringId = 0;
        ULONG languageId = 0;
        status = GetStringId(Request, &stringId, &languageId);
        if (NT_SUCCESS(status)) {
            switch (stringId) {
            case 1: // Manufacturer
                status = RequestCopyFromBuffer(Request, (PVOID)g_ManufacturerString, sizeof(g_ManufacturerString));
                break;
            case 2: // Product
                status = RequestCopyFromBuffer(Request, (PVOID)g_ProductString, sizeof(g_ProductString));
                break;
            case 3: // Serial Number
                status = RequestCopyFromBuffer(Request, (PVOID)g_SerialNumberString, sizeof(g_SerialNumberString));
                break;
            default:
                status = STATUS_INVALID_PARAMETER;
                break;
            }
        }
        PtpLog(L"[VirtualPtpDriver] -> GET_STRING: stringId=%lu, status=0x%08X\n", stringId, status);
        break;
    }

    case IOCTL_PTP_INJECT_REPORT:
    {
        // Injection from user-mode bridge daemon
        if (InputBufferLength < sizeof(UCHAR)) {
            status = STATUS_BUFFER_TOO_SMALL;
            break;
        }

        PVOID reportBuffer = NULL;
        status = WdfRequestRetrieveInputBuffer(Request, InputBufferLength, &reportBuffer, NULL);
        if (NT_SUCCESS(status) && reportBuffer != NULL) {
            InjectTouchReport(context, reportBuffer, InputBufferLength);
            WdfRequestSetInformation(Request, InputBufferLength);
            status = STATUS_SUCCESS;
        }
        break;
    }

    default:
        status = STATUS_NOT_IMPLEMENTED;
        PtpLog(L"[VirtualPtpDriver] -> UNHANDLED IOCTL: 0x%08X (returning STATUS_NOT_IMPLEMENTED 0x%08X)\n",
            IoControlCode, status);
        break;
    }

    if (completeRequest) {
        PtpLog(L"[VirtualPtpDriver] <- Completing IOCTL 0x%08X: status=0x%08X, info=%zu\n",
            IoControlCode, status, WdfRequestGetInformation(Request));
        WdfRequestComplete(Request, status);
    }
}

NTSTATUS InjectTouchReport(PVOID ContextPtr, PVOID ReportBuffer, size_t ReportSize)
{
    PDEVICE_CONTEXT context = (PDEVICE_CONTEXT)ContextPtr;
    if (context == NULL || context->ManualReportQueue == NULL) {
        return STATUS_INVALID_DEVICE_STATE;
    }

    WDFREQUEST pendingReadRequest = NULL;
    NTSTATUS queueStatus = WdfIoQueueRetrieveNextRequest(context->ManualReportQueue, &pendingReadRequest);
    if (NT_SUCCESS(queueStatus) && pendingReadRequest != NULL) {
        static LONG s_InjectCount = 0;
        if (InterlockedIncrement(&s_InjectCount) == 1) {
            PtpLog(L"[VirtualPtpDriver] -> First touch report successfully injected into OS touch stack.\n");
        }
        NTSTATUS copyStatus = RequestCopyFromBuffer(pendingReadRequest, ReportBuffer, ReportSize);
        WdfRequestComplete(pendingReadRequest, copyStatus);
        return copyStatus;
    }
    return STATUS_SUCCESS;
}

