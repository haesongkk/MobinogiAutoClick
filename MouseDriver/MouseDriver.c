// 
// TODO: 실제 매크로를 구현하는 새 프로젝트 생성
// 드라이버를 통해 마우스 입력을 받아온다
// 인증서 등록 -> 드라이버 등록 -> <매크로> -> 드라이버 해제 -> 인증서 해제
//
// 마우스 위치는 독점 아마 못할 것...
// 하지만 입력값도 커널을 통해 넣어야할 수 잇음
//
// sc query MobinogiAutoClick: 드라이버 추가 확인
// bcdedit /set testsigning on(off): 테스트 드라이버 허용 모드 (재부팅 필요)
// 
// 테스트 모드보다는 직접 .sys.파일에 서명하고 
// 컴퓨터에 인증서를 등록/해제하는 방식으로 진행하는게 좋을듯
// 워터마크도 안 남고, 재부팅도 필요없고, 그냥 깔끔함
// 

#include <ntddk.h>
#include <ntddmou.h>

// 원래 무슨 mouclass.h가 있었다는데 없어져서 따로 정의
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

// 마우스 콜백 저장용 전역 변수
CONNECT_DATA g_OriginalConnect;
volatile BOOLEAN g_LastLeftClick = FALSE;

NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);  // <<< 이거 추가
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status = STATUS_SUCCESS;
    ULONG outLen = 0;

    if (stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_GET_CLICK_STATE) {
        if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(BOOLEAN)) {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        else {
            *(BOOLEAN*)Irp->AssociatedIrp.SystemBuffer = g_LastLeftClick;
            g_LastLeftClick = FALSE;  // 한 번 응답 후 리셋
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




// 마우스 입력 가로채는 후킹 함수
VOID MouseServiceCallback(
    PDEVICE_OBJECT DeviceObject,
    PMOUSE_INPUT_DATA InputDataStart,
    ULONG InputDataCount,
    PULONG InputDataConsumed
) {
    for (ULONG i = 0; i < InputDataCount; i++) {
        if (InputDataStart[i].ButtonFlags & MOUSE_LEFT_BUTTON_DOWN) {
            KdPrint(("왼쪽 클릭 감지됨!\n"));
            g_LastLeftClick = TRUE;  // ← 클릭 여부 저장
        }
    }

    // 원래 마우스 서비스 호출
    ((PSERVICE_CALLBACK_ROUTINE)g_OriginalConnect.ClassService)(
        DeviceObject,
        InputDataStart,
        InputDataCount,
        InputDataConsumed
        );


}



// IRP_MJ_INTERNAL_DEVICE_CONTROL 처리 함수
NTSTATUS DispatchInternalDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

    if (stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_INTERNAL_MOUSE_CONNECT) {
        KdPrint(("IOCTL_INTERNAL_MOUSE_CONNECT 감지됨\n"));

        PCONNECT_DATA userConnect =
            (PCONNECT_DATA)stack->Parameters.DeviceIoControl.Type3InputBuffer;

        // 원본 콜백 저장
        g_OriginalConnect = *userConnect;

        // 후킹
        userConnect->ClassService = (PVOID)MouseServiceCallback;

        KdPrint(("마우스 서비스 후킹 완료\n"));
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

    // 필터 드라이버용 장치 생성
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

    // 하위 장치에 붙기
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
    return IoCallDriver(DeviceObject, Irp);  // DeviceExtension 없이 호출 (안 붙은 상태니까)
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
