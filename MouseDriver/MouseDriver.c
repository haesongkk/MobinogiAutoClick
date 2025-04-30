// 
// TODO: ���� ��ũ�θ� �����ϴ� �� ������Ʈ ����
// ����̹��� ���� ���콺 �Է��� �޾ƿ´�
// ������ ��� -> ����̹� ��� -> <��ũ��> -> ����̹� ���� -> ������ ����
//
// ���콺 ��ġ�� ���� �Ƹ� ���� ��...
// ������ �Է°��� Ŀ���� ���� �־���� �� ����
//
// sc query MobinogiAutoClick: ����̹� �߰� Ȯ��
// bcdedit /set testsigning on(off): �׽�Ʈ ����̹� ��� ��� (����� �ʿ�)
// 
// �׽�Ʈ ��庸�ٴ� ���� .sys.���Ͽ� �����ϰ� 
// ��ǻ�Ϳ� �������� ���/�����ϴ� ������� �����ϴ°� ������
// ���͸�ũ�� �� ����, ����õ� �ʿ����, �׳� �����
// 

#include <ntddk.h>
#include <ntddmou.h>

// ���� ���� mouclass.h�� �־��ٴµ� �������� ���� ����
#define IOCTL_INTERNAL_MOUSE_CONNECT CTL_CODE(FILE_DEVICE_MOUSE, 0x0A, METHOD_NEITHER, FILE_ANY_ACCESS)

typedef struct _DEVICE_EXTENSION {
    PDEVICE_OBJECT LowerDeviceObject;
} DEVICE_EXTENSION, * PDEVICE_EXTENSION;

typedef struct _CONNECT_DATA {
    PDEVICE_OBJECT ClassDeviceObject;
    PVOID ClassService;
} CONNECT_DATA, * PCONNECT_DATA;

typedef VOID(*PSERVICE_CALLBACK_ROUTINE)(
    PDEVICE_OBJECT,
    PMOUSE_INPUT_DATA,
    ULONG,
    PULONG
    );
#define IOCTL_GET_CLICK_STATE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

// ���콺 �ݹ� ����� ���� ����
CONNECT_DATA g_OriginalConnect;
volatile BOOLEAN g_LastLeftClick = FALSE;

NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);  // <<< �̰� �߰�
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_SUCCESS;
    ULONG outLen = 0;

    if (stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_GET_CLICK_STATE) {
        if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(BOOLEAN)) {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        else {
            *(BOOLEAN*)Irp->AssociatedIrp.SystemBuffer = g_LastLeftClick;
            g_LastLeftClick = FALSE;  // �� �� ���� �� ����
            outLen = sizeof(BOOLEAN);
        }
    }
    else {
        status = STATUS_INVALID_DEVICE_REQUEST;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = outLen;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}




// ���콺 �Է� ����ä�� ��ŷ �Լ�
VOID MouseServiceCallback(
    PDEVICE_OBJECT DeviceObject,
    PMOUSE_INPUT_DATA InputDataStart,
    ULONG InputDataCount,
    PULONG InputDataConsumed
) {
    for (ULONG i = 0; i < InputDataCount; i++) {
        if (InputDataStart[i].ButtonFlags & MOUSE_LEFT_BUTTON_DOWN) {
            KdPrint(("���� Ŭ�� ������!\n"));
            g_LastLeftClick = TRUE;  // �� Ŭ�� ���� ����
        }
    }

    // ���� ���콺 ���� ȣ��
    ((PSERVICE_CALLBACK_ROUTINE)g_OriginalConnect.ClassService)(
        DeviceObject,
        InputDataStart,
        InputDataCount,
        InputDataConsumed
        );


}



// IRP_MJ_INTERNAL_DEVICE_CONTROL ó�� �Լ�
NTSTATUS DispatchInternalDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

    if (stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_MOUSE_CONNECT) {
        KdPrint(("IOCTL_INTERNAL_MOUSE_CONNECT ������\n"));

        PCONNECT_DATA userConnect =
            (PCONNECT_DATA)stack->Parameters.DeviceIoControl.Type3InputBuffer;

        // ���� �ݹ� ����
        g_OriginalConnect = *userConnect;

        // ��ŷ
        userConnect->ClassService = (PVOID)MouseServiceCallback;

        KdPrint(("���콺 ���� ��ŷ �Ϸ�\n"));
    }

    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(((PDEVICE_EXTENSION)DeviceObject->DeviceExtension)->LowerDeviceObject, Irp);
}


NTSTATUS AddDevice(
    PDRIVER_OBJECT DriverObject,
    PDEVICE_OBJECT PhysicalDeviceObject
) {
    NTSTATUS status;
    PDEVICE_OBJECT DeviceObject = NULL;
    PDEVICE_EXTENSION devExt;

    UNICODE_STRING usDosDeviceName, usDeviceName;

    RtlInitUnicodeString(&usDeviceName, L"\\Device\\MobinogiAutoClick");
    RtlInitUnicodeString(&usDosDeviceName, L"\\DosDevices\\MobinogiAutoClick");

    // ���� ����̹��� ��ġ ����
    status = IoCreateDevice(
        DriverObject,
        sizeof(DEVICE_EXTENSION),
        &usDeviceName,
        FILE_DEVICE_MOUSE,
        0,
        FALSE,
        &DeviceObject
    );

    if (!NT_SUCCESS(status)) {
        KdPrint(("MobinogiAutoClick: IoCreateDevice failed: 0x%x\n", status));
        return status;
    }

    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;

    // ���� ��ġ�� �ٱ�
    devExt = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    devExt->LowerDeviceObject = IoAttachDeviceToDeviceStack(DeviceObject, PhysicalDeviceObject);

    DeviceObject->Flags |= DO_BUFFERED_IO;
    DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    KdPrint(("MobinogiAutoClick: AddDevice success!\n"));

    status = IoCreateSymbolicLink(&usDosDeviceName, &usDeviceName);
    if (!NT_SUCCESS(status)) {
        KdPrint(("MobinogiAutoClick: Symbolic link creation failed: 0x%x\n", status));
        IoDeleteDevice(DeviceObject);
        return status;
    }


    return STATUS_SUCCESS;
}


NTSTATUS DispatchPassThrough(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(DeviceObject, Irp);  // DeviceExtension ���� ȣ�� (�� ���� ���´ϱ�)
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);
    KdPrint(("MobinogiAutoClick Driver Unloaded\n"));
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);

    DriverObject->DriverExtension->AddDevice = AddDevice;
    DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = DispatchInternalDeviceControl;

    KdPrint(("MobinogiAutoClick Driver Loaded\n"));

    for (int i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
        DriverObject->MajorFunction[i] = DispatchPassThrough;

    DriverObject->DriverUnload = DriverUnload;

    UNICODE_STRING usDosDeviceName;
    RtlInitUnicodeString(&usDosDeviceName, L"\\DosDevices\\MobinogiAutoClick");
    IoDeleteSymbolicLink(&usDosDeviceName);

    return STATUS_SUCCESS;
}
