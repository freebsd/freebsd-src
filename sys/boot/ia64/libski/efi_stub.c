/*
 * Copyright (c) 2003 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <machine/bootinfo.h>
#include <efi.h>
#include <stand.h>
#include "libski.h"

extern void acpi_root;
extern void sal_systab;

extern void acpi_stub_init(void);
extern void sal_stub_init(void);

EFI_CONFIGURATION_TABLE efi_cfgtab[] = {
	{ ACPI_20_TABLE_GUID,		&acpi_root },
	{ SAL_SYSTEM_TABLE_GUID,	&sal_systab }
};


static EFI_STATUS GetTime(EFI_TIME *, EFI_TIME_CAPABILITIES *);
static EFI_STATUS SetTime(EFI_TIME *);
static EFI_STATUS GetWakeupTime(BOOLEAN *, BOOLEAN *, EFI_TIME *);
static EFI_STATUS SetWakeupTime(BOOLEAN, EFI_TIME *);

static EFI_STATUS SetVirtualAddressMap(UINTN, UINTN, UINT32,
    EFI_MEMORY_DESCRIPTOR*);
static EFI_STATUS ConvertPointer(UINTN, VOID **);

static EFI_STATUS GetVariable(CHAR16 *, EFI_GUID *, UINT32 *, UINTN *, VOID *);
static EFI_STATUS GetNextVariableName(UINTN *, CHAR16 *, EFI_GUID *);
static EFI_STATUS SetVariable(CHAR16 *, EFI_GUID *, UINT32, UINTN, VOID *);

static EFI_STATUS GetNextHighMonotonicCount(UINT32 *);
static EFI_STATUS ResetSystem(EFI_RESET_TYPE, EFI_STATUS, UINTN, CHAR16 *);

EFI_RUNTIME_SERVICES efi_rttab = {
	/* Header. */
	{	EFI_RUNTIME_SERVICES_SIGNATURE,
		EFI_RUNTIME_SERVICES_REVISION,
		0,			/* XXX HeaderSize */
		0,			/* XXX CRC32 */
	},

	/* Time services */
	GetTime,
	SetTime,
	GetWakeupTime,
	SetWakeupTime,

	/* Virtual memory services */
	SetVirtualAddressMap,
	ConvertPointer,

	/* Variable services */
	GetVariable,
	GetNextVariableName,
	SetVariable,

	/* Misc */
	GetNextHighMonotonicCount,
	ResetSystem
};

EFI_SYSTEM_TABLE efi_systab = {
	/* Header. */
	{	EFI_SYSTEM_TABLE_SIGNATURE,
		EFI_SYSTEM_TABLE_REVISION,
		0,	/* XXX HeaderSize */
		0,	/* XXX CRC32 */
	},

	/* Firmware info. */
	L"FreeBSD", 0,

	/* Console stuff. */
	NULL, NULL,
	NULL, NULL,
	NULL, NULL,

	/* Services (runtime first). */
	&efi_rttab,
	NULL,

	/* Configuration tables. */
	sizeof(efi_cfgtab)/sizeof(EFI_CONFIGURATION_TABLE),
	efi_cfgtab
};

static EFI_STATUS
unsupported(const char *func)
{
	printf("EFI: %s not supported\n", func);
	return (EFI_UNSUPPORTED);
}

static EFI_STATUS
GetTime(EFI_TIME *time, EFI_TIME_CAPABILITIES *caps)
{
	UINT32 comps[8];

	ssc((UINT64)comps, 0, 0, 0, SSC_GET_RTC);
	time->Year = comps[0] + 1900;
	time->Month = comps[1] + 1;
	time->Day = comps[2];
	time->Hour = comps[3];
	time->Minute = comps[4];
	time->Second = comps[5];
	time->Pad1 = time->Pad2 = 0;
	time->Nanosecond = 0;
	time->TimeZone = 0;
	time->Daylight = 0;
	return (EFI_SUCCESS);
}

static EFI_STATUS
SetTime(EFI_TIME *time)
{
	return (EFI_SUCCESS);
}

static EFI_STATUS
GetWakeupTime(BOOLEAN *enabled, BOOLEAN *pending, EFI_TIME *time)
{
	return (unsupported(__func__));
}

static EFI_STATUS
SetWakeupTime(BOOLEAN enable, EFI_TIME *time)
{
	return (unsupported(__func__));
}

static void
Reloc(void *addr, UINT64 delta)
{
	UINT64 **fpp = addr;

	*fpp[0] += delta;
	*fpp[1] += delta;
	*fpp += delta >> 3;
}

static EFI_STATUS
SetVirtualAddressMap(UINTN mapsz, UINTN descsz, UINT32 version,
    EFI_MEMORY_DESCRIPTOR *memmap)
{
	UINT64 delta;

	delta = memmap->VirtualStart - memmap->PhysicalStart;
	Reloc(&efi_rttab.GetTime, delta);
	Reloc(&efi_rttab.SetTime, delta);
	return (EFI_SUCCESS);		/* Hah... */
}

static EFI_STATUS
ConvertPointer(UINTN debug, VOID **addr)
{
	return (unsupported(__func__));
}

static EFI_STATUS
GetVariable(CHAR16 *name, EFI_GUID *vendor, UINT32 *attrs, UINTN *datasz,
    VOID *data)
{
	return (unsupported(__func__));
}

static EFI_STATUS
GetNextVariableName(UINTN *namesz, CHAR16 *name, EFI_GUID *vendor)
{
	return (unsupported(__func__));
}

static EFI_STATUS
SetVariable(CHAR16 *name, EFI_GUID *vendor, UINT32 attrs, UINTN datasz,
    VOID *data)
{
	return (unsupported(__func__));
}

static EFI_STATUS
GetNextHighMonotonicCount(UINT32 *high)
{
	static UINT32 counter = 0;

	*high = counter++;
	return (EFI_SUCCESS);
}

static EFI_STATUS
ResetSystem(EFI_RESET_TYPE type, EFI_STATUS status, UINTN datasz,
    CHAR16 *data)
{
	return (unsupported(__func__));
}

int
ski_init_stubs(struct bootinfo *bi)
{
	EFI_MEMORY_DESCRIPTOR *memp;

	/* Describe the SKI memory map. */
	bi->bi_memmap = (u_int64_t)(bi + 1);
	bi->bi_memmap_size = 4 * sizeof(EFI_MEMORY_DESCRIPTOR);
	bi->bi_memdesc_size = sizeof(EFI_MEMORY_DESCRIPTOR);
	bi->bi_memdesc_version = 1;

	memp = (EFI_MEMORY_DESCRIPTOR *)bi->bi_memmap;

	memp[0].Type = EfiPalCode;
	memp[0].PhysicalStart = 0x100000;
	memp[0].VirtualStart = 0;
	memp[0].NumberOfPages = (4L*1024*1024)>>12;
	memp[0].Attribute = EFI_MEMORY_WB | EFI_MEMORY_RUNTIME;

	memp[1].Type = EfiConventionalMemory;
	memp[1].PhysicalStart = 5L*1024*1024;
	memp[1].VirtualStart = 0;
	memp[1].NumberOfPages = (128L*1024*1024)>>12;
	memp[1].Attribute = EFI_MEMORY_WB;

	memp[2].Type = EfiConventionalMemory;
	memp[2].PhysicalStart = 4L*1024*1024*1024;
	memp[2].VirtualStart = 0;
	memp[2].NumberOfPages = (64L*1024*1024)>>12;
	memp[2].Attribute = EFI_MEMORY_WB;

	memp[3].Type = EfiMemoryMappedIOPortSpace;
	memp[3].PhysicalStart = 0xffffc000000;
	memp[3].VirtualStart = 0;
	memp[3].NumberOfPages = (64L*1024*1024)>>12;
	memp[3].Attribute = EFI_MEMORY_UC;

	bi->bi_systab = (u_int64_t)&efi_systab;

	sal_stub_init();
	acpi_stub_init();

	return (0);
}
