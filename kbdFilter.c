#include "kbdFilter.h"
//int PendingKey = 0;

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	KdPrint(("Enter the DriverEntry"));

	//Common variable represent the 'status'
	NTSTATUS status;

	//Set the MajorFunction for IRP
	for (ULONG i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		DriverObject->MajorFunction[i] = DefaultDispatch;
	}

	//Set the specified IRP dispatch functions
	DriverObject->MajorFunction[IRP_MJ_PNP] = PnpDispatch;
	DriverObject->MajorFunction[IRP_MJ_POWER] = PowerDispatch;
	DriverObject->MajorFunction[IRP_MJ_READ] = ReadDispatch;

	//*Now I do not need the "AddDevice"
	//Set the Filter attached function(I want to use the Pnp attach)
	//*	DriverObject->DriverExtension->AddDevice = AddFilter;

	//Set the driver unload function 
	//(when we unload filter manually, we need to ensure that there is no IRP don't return)
	DriverObject->DriverUnload = FilterUnload;

	//*Now I need a function to add the device just like NT driver, and I use the old name
	AddFilter(DriverObject, RegistryPath);
	status = STATUS_SUCCESS;
	return status;
}

NTSTATUS AddFilter(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	KdPrint(("Enter the AddFilter"));

	NTSTATUS status;

	UNICODE_STRING NameString;
	PKBD_FILTER_EXT filterExt;
	PDEVICE_OBJECT kbdFilter;
	PDEVICE_OBJECT AttachedDevice;
	PDEVICE_OBJECT kbdDevice;

	PDRIVER_OBJECT kbdDriverObject;

	RtlInitUnicodeString(&NameString, KBD_DRIVER_NAME);
	status = ObReferenceObjectByName(
		&NameString,
		OBJ_CASE_INSENSITIVE,
		NULL,
		0,
		*IoDriverObjectType,
		KernelMode,
		NULL,
		&kbdDriverObject);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("failed to reference the Object"));
		return status;
	}
	else
	{
		KdPrint(("succeed to reference the Object"));
		//I do not understand it actually
		ObDereferenceObject(kbdDriverObject);
	}
	
	kbdDevice = kbdDriverObject->DeviceObject;
	while (kbdDevice)
	{
		status = IoCreateDevice(
			DriverObject,
			sizeof(KBD_FILTER_EXT),
			NULL,
			kbdDevice->DeviceType,
			kbdDevice->Characteristics,
			TRUE,
			&kbdFilter);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("failed to create the Device"));
			return status;
		}
		filterExt = (PKBD_FILTER_EXT)kbdFilter->DeviceExtension;

		status = IoAttachDeviceToDeviceStackSafe(
			kbdFilter,
			kbdDevice,
			&AttachedDevice);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("failed to attach the device"));
			return status;
		}

		//Set the DeviceExtension of the filter 
		filterExt->DeviceObject = kbdFilter;
		filterExt->LowerDeviceObject = AttachedDevice;
		filterExt->Pdo = kbdDevice;
		filterExt->CountNum = 0;

		//Set the other aspects of the filter
		kbdFilter->DeviceType = AttachedDevice->DeviceType;
		kbdFilter->Characteristics = AttachedDevice->Characteristics;
		kbdFilter->StackSize = AttachedDevice->StackSize + 1;
		//Set the flags of the filter device 
		kbdFilter->Flags |= AttachedDevice->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE);
		kbdFilter->Flags &= ~DO_DEVICE_INITIALIZING;

		kbdDevice = kbdDevice->NextDevice;
	}

	return status;
}

/*
//Create a filter device object, and attach it to the device stack
NTSTATUS AddFilter(PDRIVER_OBJECT DirverObject, PDEVICE_OBJCET pdo)
{
NTSTATUS status;
//Device object which will be created
PDEVICE_OBJECT kbdFilter;
//Device object which be attached
PDEVICE_OBJECT AttachedDevice;
//	PUNICODE_STRING filterName;

//Create the filter device object
status = IoCreateDevice(
DriverObject,
sizeof(KDB_FILTER_EXTENSION),
//System will create a default name, and I do not think
//whether I need to create a Symbolic name for it well
NULL,
pdo->DeviceType,
pdo->Characteristics,
FALSE,
&kbdFilter);
//Set extension of the filter device
PKBD_FILTER_EXTENSION filterExt;
filterExt = (PKBD_FILTER_EXTENSION)kbdFilter->DeviceExtension;
//Attach the filter device to the device stack
IoAttachDeviceToDeviceStackSafe(
kbdFilter,
pdo,
AttachedDevice);
//fill the filter's extension with these device objects
filterExt->DeviceObject = kbdFilter;
filterExt->LowerDeviceObject = AttachedDevice;
filterExt->Pdo = pdo;
//Set the Flags of filter object as the attached device
kbdFilter->Flags |= LowerDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE);
kbdFilter->Flags &= ~DO_DEVICE_INITIALIZING;
//Set the other aspects of device object
kbdFilter->DeviceType = LowerDeviceObject->DeviceType;
kbdFilter->Characteristics = LowerDeviceObject->Characteristics;
kbdFilter->StackSize = LowerDeviceObject->StackSize + 1;

//Return the status, but I do not consider the failure of 'create & attach'
return status;
}
*/

NTSTATUS DefaultDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	PKBD_FILTER_EXT filterExt;
	filterExt = (PKBD_FILTER_EXT)DeviceObject->DeviceExtension;
	IoSkipCurrentIrpStackLocation(Irp);

	return IoCallDriver(filterExt->LowerDeviceObject, Irp);
}

NTSTATUS PnpDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	KdPrint(("Enter the PnpDispatch"));

	NTSTATUS status;

	PKBD_FILTER_EXT filterExt;
	filterExt = (PKBD_FILTER_EXT)DeviceObject->DeviceExtension;

	PIO_STACK_LOCATION IoStack = IoGetCurrentIrpStackLocation(Irp);

	switch (IoStack->MinorFunction)
	{
	case IRP_MN_REMOVE_DEVICE:
		//Transfer the IRP at first
		IoSkipCurrentIrpStackLocation(Irp);
		IoCallDriver(filterExt->LowerDeviceObject, Irp);

		//Detach and delete the device
		IoDetachDevice(filterExt->LowerDeviceObject);
		IoDeleteDevice(DeviceObject);
		status = STATUS_SUCCESS;
	default:
		IoSkipCurrentIrpStackLocation(Irp);
		status = IoCallDriver(filterExt->LowerDeviceObject, Irp);
	}
	return status;
}

NTSTATUS PowerDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS status;

	PoStartNextPowerIrp(Irp);
	IoSkipCurrentIrpStackLocation(Irp);

	status = PoCallDriver(((PKBD_FILTER_EXT)DeviceObject->DeviceExtension)->LowerDeviceObject, Irp);
	return status;
}

//Now I need to process the manually unload of the NT type filter 
VOID FilterUnload(PDRIVER_OBJECT DriverObject)
{
	KdPrint(("Enter the FilterUnload"));

	PDEVICE_OBJECT kbdFilter;
	PKBD_FILTER_EXT filterExt;
	//The process of Delay is copied from the book
	LARGE_INTEGER lDelay;
	lDelay = RtlConvertLongToLargeInteger(1000 * DELAY_ONE_MILLISECOND);

	//Detach and delete the device
	kbdFilter = DriverObject->DeviceObject;
	while (kbdFilter != NULL)
	{
		KdPrint(("Detach and delete the device"));

		filterExt = (PKBD_FILTER_EXT)kbdFilter->DeviceExtension;

		//First detach the device so that it no longer receive the IRP 
		IoDetachDevice(filterExt->LowerDeviceObject);

		KdPrint(("Detach the device"));
	
		//Set the next device before deleting this device
		kbdFilter = kbdFilter->NextDevice;

		//wait for final IRP's finish aobut this device
		while (filterExt->CountNum)
		{
			KdPrint(("%d", filterExt->CountNum));
			KeDelayExecutionThread(KernelMode, FALSE, &lDelay);
		}
		
		KdPrint(("End the Pending"));

		///Delete the filter device with the DeviceExtension
		IoDeleteDevice(filterExt->DeviceObject);

		KdPrint(("Delete the device"));
	}
	KdPrint(("Complete the Unload!"));
}

/*
NTSTATUS FilterUnload(PDRIVER_OBJECT DriverObject)
{
NTSTATUS status;
//There is no need to process the unload?

status = STATUS_SUCCESS;
return status;
}
*/

NTSTATUS ReadDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	KdPrint(("Enter the ReadDispatch"));

	NTSTATUS status;
	PKBD_FILTER_EXT filterExt;
	filterExt = (PKBD_FILTER_EXT)DeviceObject->DeviceExtension;

	
	KdPrint(("%d,CountNum", filterExt->CountNum));
	(filterExt->CountNum)++;
//	PendingKey++;
	KdPrint(("%d,CountNum++", filterExt->CountNum));

	IoCopyCurrentIrpStackLocationToNext(Irp);
	IoSetCompletionRoutine(Irp, ReadComplete, NULL, TRUE, TRUE, TRUE);

	status = IoCallDriver(filterExt->LowerDeviceObject, Irp);
	return status;
}

NTSTATUS ReadComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
	KdPrint(("Enter the ReadComplete"));

	PKEYBOARD_INPUT_DATA buf;
	int i;


	if (NT_SUCCESS(Irp->IoStatus.Status))
	{
		//Get buffer
		buf = Irp->AssociatedIrp.SystemBuffer;

		//Process the buffer
		for ( i = 0; i < Irp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA); i++)
		{
			KdPrint(("The Key is: %2x\r\n", buf[i].MakeCode));
		}
	}

	if (Irp->PendingReturned)
	{
		IoMarkIrpPending(Irp);
	}

	
	KdPrint(("%d,CountNum", ((PKBD_FILTER_EXT)DeviceObject->DeviceExtension)->CountNum));
//	(((PKBD_FILTER_EXT)((PDEVICE_OBJECT)Context)->DeviceExtension)->CountNum)--;
//	PendingKey--;
	((PKBD_FILTER_EXT)DeviceObject->DeviceExtension)->CountNum--;
	KdPrint(("%d,CountNum--", ((PKBD_FILTER_EXT)DeviceObject->DeviceExtension)->CountNum));
	return Irp->IoStatus.Status;
}