#include "kbdFilter.h"

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	//Common variable represent the 'status'
	NTSTATUS status;

	//Set the MajorFunction for IRP
	for(ULONG i = 0; i < IRP_MJ_MAXNUM_FUNCTION; i++)
	{
		DriverObject->MajorFunction[i] = DefaultDispatch;
	}

	//Set the specified IRP dispatch functions
	DriverObject->MajorFunction[IRP_MJ_PNP] = PnpDispatch;             
	DriverObject->MajorFunction[IRP_MJ_POWER] = PowerDispatch;

	//*Now I do not need the "AddDevice"
	//Set the Filter attached function(I want to use the Pnp attach)
//*	DriverObject->DriverExtension->AddDevice = AddFilter;

	//Set the driver unload function 
	//(when we unload filter manually, we need to ensure that there is no IRP don't return)
	DriverObject->DriverUnload = FilterUnload;

	//*Now I need a function to add the device just like NT driver, and I use the old name
	AddFilter(DriverObject, RegistryPath);
}

NTSTATUS AddFilter(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	NTSTATUS status;

	UNICODE_STRING NameString;
	PKBD_FILTER_EXTENSION filterExt;
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
		IoDriverObjectType,
		KernelMode,
		NULL,
		&kbdDriverObject);
	if(!NT_SUCCESS(status))
	{
		return status;
	}
	else
	{
		//I do not understand it actually
		ObDereferenceObject(DriverObject);
	}


	kbdDevice = kbdDriverObject->DeviceObject;
	while(kbdDevice)
	{
		status = IoCreateDevice(
			kbdFilter,
			sizeof(KBD_FILTER_EXTENSION),
			NULL,
			kbdDevice->DeviceType,
			kbdDevice->Characteristics,
			TRUE,
			&kbdFilter);
		if(!NT_SUCCESS(status))
		{
			return status;
		}
		filterExt = (PKBD_FILTER_EXTENSION)kbdFilter->DeviceExtension;

		IoAttachDeviceToDevicestackSafe(
			kbdFilter,
			kbdDevice,
			AttachedDevice);

		//Set the DeviceExtension of the filter 
		filterExt->DeviceObject = kbdFilter;
		filterExt->LowerDeviceObject = AttachedDevice;
		filterExt->Pdo = kbdDevice;

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
	IoSkipCurrentIrpStackLocation(Irp);

	return IoCallDriver((PKBD_FILTER_EXTENSION)(DeviceObject->DeviceExtension)->LowerDeviceObject，Irp);
}

NTSTATUS PnpDispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS status;
	
	PKBD_FILTER_EXTENSION filterExt;
	filterExt = (PKBD_FILTER_EXTENSION)DeviceObject->DeviceExtension;

	PIO_STACK_LOACTION IoStack = IoGetCurrentIrpStackLoacation(Irp);

	switch(IoStack->MinorFunction)
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

	status = PoCallDriver(((PKBD_FILTER_EXTENSION)DeviceObject->DeviceExtension)->LowerDeviceObject, Irp);
	return status;
}

//Now I need to process the manually unload of the NT type filter 
VOID FilterUnload(PDRIVER_OBJECT DriverObject)
{
	PDEVICE_OBJECT kbdFilter;
	PKBD_FILTER_EXTENSION filterExt;
	//The process of Delay is copied from the book
	LARGE_INTEGER lDelay;
	lDelay = RtlConvertLongToLargeInteger(100 * DELAY_ONE_MILLISECOND);

	//Detach and delete the device
	kbdFilter = DriverObject->DeviceObject;
	while(kbdFilter)
	{
		filterExt = (PKBD_FILTER_EXTENSION)kbdFilter->DeviceExtension;

		//First detach the device so that it no longer receive the IRP 
		status = IoDetachDevice(filterExt->LowerDeviceObject);
		if(!NT_SUCCESS(status))
		{
			KdPrint(("failed"));
		}

		//Set the next device before deleting this device
		kbdFilter = kbdFilter->NextDevice;

		//wait for final IRP's finish aobut this device
		while(filterExt->CountNum)
		{
			KeDelayExecutionThraed(KernelMode, FALSE, &lDelay);
		}

		///Delete the filter device with the DeviceExtension
		status = IoDeleteDevice(filterExt->DeviceObject);
		if(!NT_SUCCESS(status))
		{
			KdPrint(("failed"));
		}
	}
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
	NTSTATUS status;
	PKBD_FILTER_EXTENSION filterExt;
	filterExt = (PKBD_FILTER_EXTENSION)DeviceObject->DeviceExtension;

	(filterExt->CountNum)++;

	IoCopyCurrentIrpStackLocationToNext(Irp);
	IoSetCompletionRoutine(Irp, ReadComplete, DeviceObject, TRUE, TRUE, TRUE);

	status = IoCallDriver(filterExt->LowerDeviceObject, Irp);
	return status;
}

NTSTATUS ReadComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
	PUCHAR buf;
	ULONG buf_len;

	if(NT_SUCCESS(Irp->IoStatus.Status))
	{
		//Get buffer
		buf = Irp->AssociatedIrp.SystemBuffer;
		buf_len = Irp->IoStatus.Information;

		//Process the buffer
		for(ULONG i = 0; i < buf_len, i++)
		{
			DbgPrint("The Key is: %2x\r\n", buf[i]);
		}
	}

	if(Irp->PendingReturned)
	{
		IoMarkIrpPending(Irp);
	}

	return Irp->IoStatus.Status;
}