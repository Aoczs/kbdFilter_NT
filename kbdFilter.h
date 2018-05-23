#include "ntddk.h"

//The process of Delay is copied from the book
#define DELAY_ONE_MICROSECOND (-10)
#define DELAY_ONE_MILLISECOND (DELAY_ONE_MICROSECOND * 1000)
#define DELAY_ONE_SECOND (DELAY_ONE_MILLISECOND * 1000)

//*Declare the function&variable used in this NT driver
extern POBJECT_TYPE IoDriverObjectType
#define KBD_DRIVER_NAME L"\\Driver\\Kbdclass"
NTSTATUS ObReferenceObjectByName(
	PUNICODE_STRING ObjectName,
	ULONG Attributes,
	PACCESS_STATE AccessState,
	ACCESS_MASK DesiredAccess,
	POBJECT_TYPE ObjectType,
	KPROCESSOR_MODE AccessMode,
	PVOID ParseContext,
	PVOID *Object
	)

//Filter-device's extension 
typedef struct _KBD_FILTER_EXT
{
	//This device's DeviceObject
	PDEVICE_OBJECT DeviceObject;
	//The next lower device's DeviceObject
	PDEVICE_OBJECT LowerDeviceObject;
	//The PDO of this Device_Stack
	PDEVICE_OBJECT Pdo;
	//Symbolic name
	UNICODE_STRING SymbolicName;
	//Number of the "I/RP_MJ_READ" Irp
	ULONG CountNum = 0;

}KBD_FILTER_EXT, *PKBD_FILTER_EXT;

//Define functions are used
NTSTATUS DefaultDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS PowerDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS PnpDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS ReadDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS ReadComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);

/*
NTSTATUS AddFilter(PDRIVER_OBJECT DriverObject, PDEIVE_OBJECT pdo);
*/
NTSTATUS AddFilter(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
VOID FilterUnload(PDRIVER_OBJECT DriverObject);