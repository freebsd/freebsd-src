/*-
 * Copyright (c) 2000 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

#ifndef _MACHINE_EFI_H_
#define _MACHINE_EFI_H_

typedef u_int8_t	BOOLEAN;
typedef int32_t		INTN;
typedef u_int32_t	UINTN;
typedef int8_t		INT8;
typedef u_int8_t	UINT8;
typedef int16_t		INT16;
typedef u_int16_t	UINT16;
typedef int32_t		INT32;
typedef u_int32_t	UINT32;
typedef int64_t		INT64;
typedef u_int64_t	UINT64;
typedef UINT8		CHAR8;
typedef UINT16		CHAR16;

typedef struct _EFI_GUID {
	UINT32			Data1;
	UINT16			Data2;
	UINT16			Data3;
	UINT8			Data4[8];
} EFI_GUID;

typedef INTN		EFI_STATUS;
typedef void		*EFI_HANDLE;
typedef void		*EFI_EVENT;
typedef UINT64		EFI_LBA;
typedef UINTN		EFI_TPL;

/*
 * EFI_STATUS Error Codes.
 */
#define EFI_SUCCESS		0
#define EFI_LOAD_ERROR		1
#define EFI_INVALID_PARAMETER	2
#define EFI_UNSUPPORTED		3
#define EFI_BAD_BUFFER_SIZE	4
#define EFI_BUFFER_TOO_SMALL	5
#define EFI_NOT_READY		6
#define EFI_DEVICE_ERROR	7
#define EFI_WRITE_PROTECTED	8
#define EFI_OUT_OF_RESOURCES	9
#define EFI_VOLUME_CORRUPTED	10
#define EFI_VOLUME_FULL		11
#define EFI_NO_MEDIA		12
#define EFI_MEDIA_CHANGED	13
#define EFI_NOT_FOUND		14
#define EFI_ACCESS_DENIED	15
#define EFI_NO_RESPONSE		16
#define EFI_NO_MAPPING		17
#define EFI_TIMEOUT		18
#define EFI_NOT_STARTED		19
#define EFI_ALREADY_STARTED	20
#define EFI_ABORTED		21
#define EFI_ICMP_ERROR		22
#define EFI_TFTP_ERROR		23
#define EFI_PROTOCOL_ERROR	24

/*
 * EFI_STATUS Warning Codes.
 */
#define EFI_WARN_UNKNOWN_GLYPH	1
#define EFI_WARN_DELETE_FAILURE	2
#define EFI_WARN_WRITE_FAILURE	3
#define EFI_WARN_BUFFER_TOO_SMALL 4


typedef struct _EFI_MAC_ADDRESS {
	CHAR8		Address[32];
} EFI_MAC_ADDRESS;

typedef struct _EFI_IPv4_ADDRESS {
	CHAR8		Address[4];
} EFI_IPv4_ADDRESS;

typedef struct _EFI_IPv6_ADDRESS {
	CHAR8		Address[16];
} EFI_IPv6_ADDRESS, EFI_IP_ADDRESS;

typedef struct _EFI_TIME {
	UINT16		Year;		/* 1998 - 20xx */
	UINT8		Month;		/* 1 - 12 */
	UINT8		Day;		/* 1 - 31 */
	UINT8		Hour;		/* 0 - 23 */
	UINT8		Minute;		/* 0 - 59 */
	UINT8		Second;		/* 0 - 59 */
	UINT8		Pad1;
	UINT32		Nanosecond;	/* 0 - 999,999,999 */
	INT16		TimeZone;	/* -1440 - 1440 or 2047 */
	UINT8		Daylight;
	UINT8		Pad2;
} EFI_TIME;

typedef struct _EFI_TIME_CAPABILITIES {
	UINT32		Resolution;
	UINT32		Accuracy;
	BOOLEAN		SetsToZero;
} EFI_TIME_CAPABILITIES;

/*
 * Reset types.
 */
typedef enum _EFI_RESET_TYPE {
	EfiResetCold,
	EfiResetWarm
} EFI_RESET_TYPE;

/*
 * Allocate Types.
 */
typedef enum _EFI_ALLOCATE_TYPE {
	AllocateAnyPages,
	AllocateMaxAddress,
	AllocateAddress,
	MaxAllocateType
} EFI_ALLOCATE_TYPE;

/*
 * Memory types.
 */
typedef enum _EFI_MEMORY_TYPE {
	EfiReservedMemoryType,
	EfiLoaderCode,
	EfiLoaderData,
	EfiBootServicesCode,
	EfiBootServicesData,
	EfiRuntimeServicesCode,
	EfiRuntimeServicesData,
	EfiConventionalMemory,
	EfiUnusableMemory,
	EfiAcpiReclaimMemory,
	EfiAcpiMemoryNvs,
	EfiMemoryMappedIo,
	EfiMemoryMappedIoPortSpace,
	EfiPalCode,
	EfiMaxMemoryType
} EFI_MEMORY_TYPE;

/*
 * Physical Address.
 */
typedef UINT64	EFI_PHYSICAL_ADDRESS;

/*
 * Virtual Address.
 */
typedef UINT64	EFI_VIRTUAL_ADDRESS;


/*
 * Memory Descriptor.
 */
typedef struct _EFI_MEMORY_DESCRIPTOR {
	UINT32			Type;
	EFI_PHYSICAL_ADDRESS	PhysicalStart;
	EFI_VIRTUAL_ADDRESS	VirtualStart;
	UINT64			NumberOfPages;
	UINT64			Attribute;
} EFI_MEMORY_DESCRIPTOR;

#define EFI_MEMORY_DESCRIPTOR_VERSION	1

/*
 * Memory Attribute Definitions.
 */
#define EFI_MEMORY_UC		0x0000000000000001
#define EFI_MEMORY_WC		0x0000000000000002
#define EFI_MEMORY_WT		0x0000000000000004
#define EFI_MEMORY_WB		0x0000000000000008
#define EFI_MEMORY_UCE		0x0000000000000010
#define EFI_MEMORY_WP		0x0000000000001000
#define EFI_MEMORY_RP		0x0000000000002000
#define EFI_MEMORY_XP		0x0000000000004000
#define EFI_MEMORY_RUNTIME	0x8000000000000000

/*
 * Variable Attributes.
 */
#define EFI_VARIABLE_NON_VOLATILE	0x0000000000000001
#define EFI_VARIABLE_BOOTSERVICE_ACCESS	0x0000000000000002
#define EFI_VARIABLE_RUNTIME_ACCESS	0x0000000000000004

/*
 * Task Priority Levels.
 */
#define TPL_APPLICATION		4
#define TPL_CALLBACK		8
#define TPL_NOTIFY		16
#define TPL_HIGH_LEVEL		31

/*
 * Event Types.
 */
#define EVT_TIMER		0x80000000
#define EVT_RUNTIME		0x40000000
#define EVT_RUNTIME_CONTEXT	0x20000000

#define EVT_NOTIFY_WAIT		0x00000100
#define EVT_NOTIFY_SIGNAL	0x00000200
#define EVT_SIGNAL_EXIT_BOOT_SERVICES 0x00000201
#define EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE 0x60000202

/*
 * Event Notification Function.
 */
typedef void (*EFI_EVENT_NOTIFY)(EFI_EVENT Event,
				 void *Context);
/*
 * Timer Delay
 */
typedef enum _EFI_TIMER_DELAY {
	TimerCancel,
	TimerPeriodic,
	TimerRelative
} EFI_TIMER_DELAY;

/*
 * Interface Types.
 */
typedef enum _EFI_INTERFACE_TYPE {
	EFI_NATIVE_INTERFACE,
	EFI_PCODE_INTERFACE
} EFI_INTERFACE_TYPE;

/*
 * Search Type.
 */
typedef enum _EFI_LOCATE_SEARCH_TYPE {
	AllHandles,
	ByRegisterNotify,
	ByProtocol
} EFI_LOCATE_SEARCH_TYPE;

/*
 * Device Path.
 */
typedef struct _EFI_DEVICE_PATH {
	UINT8		Type;
	UINT8		SubType;
	UINT8		Length[2];
} EFI_DEVICE_PATH;

/*
 * SIMPLE_INPUT Protocol
 */
#define SIMPLE_INPUT_PROTOCOL \
	{ 0x387477c1,0x69c7,0x11d2,0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b }

typedef struct _EFI_INPUT_KEY {
	UINT16		ScanCode;
	CHAR16		UnicodeChar;
} EFI_INPUT_KEY;

typedef struct _SIMPLE_INPUT_INTERFACE SIMPLE_INPUT_INTERFACE;
struct _SIMPLE_INPUT_INTERFACE {
	EFI_STATUS	(*Reset)
		(SIMPLE_INPUT_INTERFACE *This,
		 BOOLEAN	ExtendedVerification);
	EFI_STATUS	(*ReadKey)
		(SIMPLE_INPUT_INTERFACE *This,
		 EFI_INPUT_KEY	*Key);
	EFI_EVENT	WaitForKey;
};

/*
 * SIMPLE_TEXT_OUTPUT Protocol.
 */
#define SIMPLE_TEXT_OUTPUT_PROTOCOL \
	{ 0x387477c2,0x69c7,0x11d2,0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b }

typedef struct _SIMPLE_TEXT_OUTPUT_MODE {
	INT32		MaxMode;
	/* current settings */
	INT32		Mode;
	INT32		Attribute;
	INT32		CursorColumn;
	INT32		CursorRow;
	BOOLEAN		CursorVisible;
} SIMPLE_TEXT_OUTPUT_MODE;

typedef struct _SIMPLE_TEXT_OUTPUT_INTERFACE SIMPLE_TEXT_OUTPUT_INTERFACE;
struct _SIMPLE_TEXT_OUTPUT_INTERFACE {
	EFI_STATUS	(*Reset)
		(SIMPLE_TEXT_OUTPUT_INTERFACE *This,
		 BOOLEAN	ExtendedVerification);
	EFI_STATUS	(*OutputString)
		(SIMPLE_TEXT_OUTPUT_INTERFACE *This,
		 CHAR16		*String);
	EFI_STATUS	(*TestString)
		(SIMPLE_TEXT_OUTPUT_INTERFACE *This,
		 CHAR16		*String);
	EFI_STATUS	(*QueryMode)
		(SIMPLE_TEXT_OUTPUT_INTERFACE *This,
		 UINTN		ModeNumber,
		 UINTN		*Columns,
		 UINTN		*Rows);
	EFI_STATUS	(*SetMode)
		(SIMPLE_TEXT_OUTPUT_INTERFACE *This,
		 UINTN		ModeNumber);
	EFI_STATUS	(*SetAttribute)
		(SIMPLE_TEXT_OUTPUT_INTERFACE *This,
		 UINTN		Attribute);
	EFI_STATUS	(*ClearScreen)
		(SIMPLE_TEXT_OUTPUT_INTERFACE *This);
	EFI_STATUS	(*SetCursorPosition)
		(SIMPLE_TEXT_OUTPUT_INTERFACE *This,
		 UINTN		Column,
		 UINTN		Row);
	EFI_STATUS	(*EnableCursor)
		(SIMPLE_TEXT_OUTPUT_INTERFACE *This,
		 BOOLEAN	Visible);
	SIMPLE_TEXT_OUTPUT_MODE Mode;
};

/*
 * Standard EFI table header.
 */
typedef struct _EFI_TABLE_HEADER {
	u_int64_t		Signature;
	u_int32_t		Revision;
	u_int32_t		HeaderSize;
	u_int32_t		CRC32;
	u_int32_t		Reserved;
} EFI_TABLE_HEADER;

/*
 * EFI Runtime Services Table.
 */
#define EFI_RUNTIME_SERVICES_SIGNATURE 0x56524553544e5552
#define EFI_RUNTIME_SERVICES_REVISION ((1<<16) | 99)

typedef struct _EFI_RUNTIME_SERVICES {
	EFI_TABLE_HEADER	Hdr;

	/*
	 * Time Services.
	 */
	EFI_STATUS	(*GetTime)
		(EFI_TIME	*Time,
		 EFI_TIME_CAPABILITIES *Capabilities);
	EFI_STATUS	(*SetTime)
		(EFI_TIME *Time);
	EFI_STATUS	(*GetWakeupTime)
		(BOOLEAN	*Enabled,
		 BOOLEAN	*Pending,
		 EFI_TIME	*Time);
	EFI_STATUS	(*SetWakeupTime)
		(BOOLEAN	Enable,
		 EFI_TIME	*Time);

	/*
	 * Virtual Memory Services.
	 */
	EFI_STATUS	(*SetVirtualAddressMap)
		(UINTN		MemoryMapSize,
		 UINTN		DescriptorSize,
		 UINT32		DescriptorVersion,
		 EFI_MEMORY_DESCRIPTOR *VirtualMap);
	EFI_STATUS	(*ConvertPointer)
		(UINTN		DebugDisposition,
		 void		**Address);

	/*
	 * Variable Services.
	 */
	EFI_STATUS	(*GetVariable)
		(CHAR16	*VariableName,
		 EFI_GUID	*VendorGuid,
		 UINT32		*Attributes,
		 UINTN		*DataSize,
		 void		*Data);
	EFI_STATUS	(*GetNextVariableName)
		(UINTN		*VariableNameSize,
		 CHAR16		*VariableName,
		 EFI_GUID	*VendorGuid);
	EFI_STATUS	(*SetVariable)
		(CHAR16	*VariableName,
		 EFI_GUID	*VendorGuid,
		 UINT32		Attributes,
		 UINTN		DataSize,
		 void		*Data);

	/*
	 * Miscellaneous Services.
	 */
	EFI_STATUS	(*GetNextHighMonotonicCount)
		(UINT32		*HighCount);
	EFI_STATUS	(*ResetSystem)
		(EFI_RESET_TYPE	ResetType,
		 EFI_STATUS	ResetStatus,
		 UINTN		DataSize,
		 CHAR16		*ResetData);

} EFI_RUNTIME_SERVICES;

/*
 * Boot Services Table.
 */
#define EFI_BOOT_SERVICES_SIGNATURE     0x56524553544f4f42
#define EFI_BOOT_SERVICES_REVISION      ((0<<16) | 91)

typedef struct _EFI_BOOT_SERVICES {
	EFI_TABLE_HEADER	Hdr;

	/*
	 * Task Priority Services.
	 */
	EFI_TPL		(*RaiseTPL)
		(EFI_TPL	NewTpl);
	void		(*RestoreTPL)
		(EFI_TPL	OldTpl);

	/*
	 * Memory Services.
	 */
	EFI_STATUS	(*AllocatePages)
		(EFI_ALLOCATE_TYPE Type,
		 EFI_MEMORY_TYPE MemoryType,
		 UINTN		Pages,
		 EFI_PHYSICAL_ADDRESS *Memory);
	EFI_STATUS	(*FreePages)
		(EFI_PHYSICAL_ADDRESS Memory,
		 UINTN		Pages);
	EFI_STATUS	(*GetMemoryMap)
		(UINTN		*MemoryMapSize,
		 EFI_MEMORY_DESCRIPTOR *MemoryMap,
		 UINTN		*MapKey,
		 UINTN		*DescriptorSize,
		 UINT32		*DescriptorVersion);
	EFI_STATUS	(*AllocatePool)
		(EFI_MEMORY_TYPE *PoolType,
		 UINTN		Size,
		 void		**Buffer);
	EFI_STATUS	(*FreePool)
		(void		*Buffer);

	/*
	 * Event & Timer Services.
	 */
	EFI_STATUS	(*CreateEvent)
		(UINT32		Type,
		 EFI_TPL	NotifyTpl,
		 EFI_EVENT_NOTIFY NotifyFunction,
		 void		*NotifyContext,
		 EFI_EVENT	*Event);
	EFI_STATUS	(*SetTimer)
		(EFI_EVENT	Event,
		 EFI_TIMER_DELAY Type,
		 UINT64		TriggerTime);
	EFI_STATUS	(*WaitForEvent)
		(UINTN		NumberOfEvents,
		 EFI_EVENT	*Event,
		 UINTN		*Index);
	EFI_STATUS	(*SignalEvent)
		(EFI_EVENT	Event);
	EFI_STATUS	(*CloseEvent)
		(EFI_EVENT	Event);
	EFI_STATUS	(*CheckEvent)
		(EFI_EVENT	Event);

	/*
	 * Protocol Handler Services.
	 */
	EFI_STATUS	(*InstallProtocolInterface)
		(EFI_HANDLE	*Handle,
		 EFI_GUID	*Protocol,
		 EFI_INTERFACE_TYPE InterfaceType,
		 void		*Interface);
	EFI_STATUS	(*ReinstallProtocolInterface)
		(EFI_HANDLE	*Handle,
		 EFI_GUID	*Protocol,
		 void		*OldInterface,
		 void		*NewInterface);
	EFI_STATUS	(*UninstallProtocolInterface)
		(EFI_HANDLE	Handle,
		 EFI_GUID	*Protocol,
		 void		*Interface);
	EFI_STATUS	(*HandleProtocol)
		(EFI_HANDLE	Handle,
		 EFI_GUID	*Protocol,
		 void		**Interface);
	EFI_STATUS	(*PCHandleProtocol)
		(EFI_HANDLE	Handle,
		 EFI_GUID	*Protocol,
		 void		**Interface);
	EFI_STATUS	(*RegisterProtocolNotify)
		(EFI_GUID	*Protocol,
		 EFI_EVENT	Event,
		 void		**Registration);
	EFI_STATUS	(*LocateHandle)
		(EFI_LOCATE_SEARCH_TYPE SearchType,
		 EFI_GUID	*Protocol,
		 void		*SearchKey,
		 UINTN		*BufferSize,
		 EFI_HANDLE	*Buffer);
	EFI_STATUS	(*LocateDevicePath)
		(EFI_GUID	*Protocol,
		 EFI_DEVICE_PATH **DevicePath,
		 EFI_HANDLE	Device);
	EFI_STATUS	(*InstallConfigurationTable)
		(EFI_GUID	*Guid,
		 void		*Table);

	/*
	 * Image Services.
	 */
	EFI_STATUS	(*LoadImage)
		(BOOLEAN	BootPolicy,
		 EFI_HANDLE	ParentImageHandle,
		 EFI_DEVICE_PATH *FilePath,
		 void		*SourceBuffer,
		 UINTN		SourceSize,
		 EFI_HANDLE	*ImageHandle);
	EFI_STATUS	(*StartImage)
		(EFI_HANDLE	ImageHandle,
		 UINTN		*ExitDataSize,
		 CHAR16		*ExitData);
	EFI_STATUS	(*Exit)
		(EFI_HANDLE	ImageHandle,
		 EFI_STATUS	ExitStatus,
		 UINTN		ExitDataSize,
		 CHAR16		ExitData);
	EFI_STATUS	(*UnloadImage)
		(EFI_HANDLE	ImageHandle);
	EFI_STATUS	(*ExitBootServices)
		(EFI_HANDLE	ImageHandle,
		 UINTN		MapKey);

	/*
	 * Miscellaneous Services.
	 */
	EFI_STATUS	(*GetNextMonotonicCount)
		(UINT64		*Count);
	EFI_STATUS	(*Stall)
		(UINTN		Microseconds);
	EFI_STATUS	(*SetWatchdogTimer)
		(UINTN		Timeout,
		 UINT64		WatchdogCode,
		 UINTN		DataSize,
		 CHAR16		*WatchdogData);

} EFI_BOOT_SERVICES;

/*
 * EFI Configuration Table and GUID Declarations.
 */
#define MPS_TABLE_GUID \
	{0xeb9d2d2f,0x2d88,0x11d3,0x9a,0x16,0x00,0x90,0x27,0x3f,0xc1,0x4d}
#define ACPI_TABLE_GUID \
	{0xeb9d2d30,0x2d88,0x11d3,0x9a,0x16,0x00,0x90,0x27,0x3f,0xc1,0x4d}
#define ACPI_20_TABLE_GUID \
	{0x8868e871,0xe4f1,0x11d3,0xbc,0x22,0x00,x080,0xc7,0x3c,0x88,0x81}
#define SMBIOS_TABLE_GUID \
	{0xeb9d2d31,0x2d88,0x11d3,0x9a,0x16,0x00,0x90,0x27,0x3f,0xc1,0x4d}
#define SAL_TABLE_GUID \
	{0xeb9d2d32,0x2d88,0x11d3,0x9a,0x16,0x00,0x90,0x27,0x3f,0xc1,0x4d}

typedef struct _EFI_CONFIGURATION_TABLE {
	EFI_GUID	VendorGuid;
	void		*VendorTable;
} EFI_CONFIGURATION_TABLE;

/*
 * EFI System Table.
 */
typedef struct _EFI_SYSTEM_TABLE {
	EFI_TABLE_HEADER	Hdr;

	CHAR16			FirmwareVendor;
	UINT32			FirmwareRevision;

	EFI_HANDLE		ConsoleInHandle;
	SIMPLE_INPUT_INTERFACE	*ConIn;

	EFI_HANDLE		ConsoleOutHandle;
	SIMPLE_TEXT_OUTPUT_INTERFACE *ConOut;

	EFI_HANDLE		StandardErrorHandle;
	SIMPLE_TEXT_OUTPUT_INTERFACE *StdErr;

	EFI_RUNTIME_SERVICES	*RuntimeServices;
	EFI_BOOT_SERVICES	*BootServices;

	UINTN			NumberOfTableEntries;
	EFI_CONFIGURATION_TABLE	*ConfiguratioNTable;
} EFI_SYSTEM_TABLE;

#endif /* _MACHINE_EFI_H_ */
