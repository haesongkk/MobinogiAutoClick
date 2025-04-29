#include <ntddk.h>

VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    DbgPrint("Driver unloaded.\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    DbgPrint("Driver loaded.\n");
    DriverObject->DriverUnload = DriverUnload;
    return STATUS_SUCCESS;
}
