/*-
 * Copyright (c) 2003,2004 Marcel Moolenaar
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <machine/bootinfo.h>
#include <machine/efi.h>
#include <stand.h>
#include "libski.h"

extern void acpi_root;
extern void sal_systab;

extern void acpi_stub_init(void);
extern void sal_stub_init(void);

struct efi_cfgtbl efi_cfgtab[] = {
	{ EFI_TABLE_ACPI20,	(intptr_t)&acpi_root },
	{ EFI_TABLE_SAL,	(intptr_t)&sal_systab }
};

static efi_status GetTime(struct efi_tm *, struct efi_tmcap *);
static efi_status SetTime(struct efi_tm *);
static efi_status GetWakeupTime(uint8_t *, uint8_t *, struct efi_tm *);
static efi_status SetWakeupTime(uint8_t, struct efi_tm *);

static efi_status SetVirtualAddressMap(u_long, u_long, uint32_t,
    struct efi_md*);
static efi_status ConvertPointer(u_long, void **);

static efi_status GetVariable(efi_char *, struct uuid *, uint32_t *, u_long *,
    void *);
static efi_status GetNextVariableName(u_long *, efi_char *, struct uuid *);
static efi_status SetVariable(efi_char *, struct uuid *, uint32_t, u_long,
    void *);

static efi_status GetNextHighMonotonicCount(uint32_t *);
static efi_status ResetSystem(enum efi_reset, efi_status, u_long, efi_char *);

struct efi_rt efi_rttab = {
	/* Header. */
	{	0,			/* XXX Signature */
		0,			/* XXX Revision */
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

struct efi_systbl efi_systab = {
	/* Header. */
	{	EFI_SYSTBL_SIG,
		0,			/* XXX Revision */
		0,			/* XXX HeaderSize */
		0,			/* XXX CRC32 */
	},

	/* Firmware info. */
	L"FreeBSD", 0, 0,

	/* Console stuff. */
	NULL, NULL,
	NULL, NULL,
	NULL, NULL,

	/* Services (runtime first). */
	(intptr_t)&efi_rttab,
	NULL,

	/* Configuration tables. */
	sizeof(efi_cfgtab)/sizeof(struct efi_cfgtbl),
	(intptr_t)efi_cfgtab
};

static efi_status
unsupported(const char *func)
{
	printf("EFI: %s not supported\n", func);
	return ((1UL << 63) + 3);
}

static efi_status
GetTime(struct efi_tm *time, struct efi_tmcap *caps)
{
	uint32_t comps[8];

	ssc((uint64_t)comps, 0, 0, 0, SSC_GET_RTC);
	time->tm_year = comps[0] + 1900;
	time->tm_mon = comps[1] + 1;
	time->tm_mday = comps[2];
	time->tm_hour = comps[3];
	time->tm_min = comps[4];
	time->tm_sec = comps[5];
	time->__pad1 = time->__pad2 = 0;
	time->tm_nsec = 0;
	time->tm_tz = 0;
	time->tm_dst = 0;
	return (0);
}

static efi_status
SetTime(struct efi_tm *time)
{
	return (0);
}

static efi_status
GetWakeupTime(uint8_t *enabled, uint8_t *pending, struct efi_tm *time)
{
	return (unsupported(__func__));
}

static efi_status
SetWakeupTime(uint8_t enable, struct efi_tm *time)
{
	return (unsupported(__func__));
}

static void
Reloc(void *addr, uint64_t delta)
{
	uint64_t **fpp = addr;

	*fpp[0] += delta;
	*fpp[1] += delta;
	*fpp += delta >> 3;
}

static efi_status
SetVirtualAddressMap(u_long mapsz, u_long descsz, uint32_t version,
    struct efi_md *memmap)
{
	uint64_t delta;

	delta = (uintptr_t)memmap->md_virt - memmap->md_phys;
	Reloc(&efi_rttab.rt_gettime, delta);
	Reloc(&efi_rttab.rt_settime, delta);
	return (0);		/* Hah... */
}

static efi_status
ConvertPointer(u_long debug, void **addr)
{
	return (unsupported(__func__));
}

static efi_status
GetVariable(efi_char *name, struct uuid *vendor, uint32_t *attrs,
    u_long *datasz, void *data)
{
	return (unsupported(__func__));
}

static efi_status
GetNextVariableName(u_long *namesz, efi_char *name, struct uuid *vendor)
{
	return (unsupported(__func__));
}

static efi_status
SetVariable(efi_char *name, struct uuid *vendor, uint32_t attrs, u_long datasz,
    void *data)
{
	return (unsupported(__func__));
}

static efi_status
GetNextHighMonotonicCount(uint32_t *high)
{
	static uint32_t counter = 0;

	*high = counter++;
	return (0);
}

static efi_status
ResetSystem(enum efi_reset type, efi_status status, u_long datasz,
    efi_char *data)
{
	return (unsupported(__func__));
}

int
ski_init_stubs(struct bootinfo *bi)
{
	struct efi_md *memp;

	/* Describe the SKI memory map. */
	bi->bi_memmap = (u_int64_t)(bi + 1);
	bi->bi_memmap_size = 4 * sizeof(struct efi_md);
	bi->bi_memdesc_size = sizeof(struct efi_md);
	bi->bi_memdesc_version = 1;

	memp = (struct efi_md *)bi->bi_memmap;

	memp[0].md_type = EFI_MD_TYPE_PALCODE;
	memp[0].md_phys = 0x100000;
	memp[0].md_virt = NULL;
	memp[0].md_pages = (4L*1024*1024)>>12;
	memp[0].md_attr = EFI_MD_ATTR_WB | EFI_MD_ATTR_RT;

	memp[1].md_type = EFI_MD_TYPE_FREE;
	memp[1].md_phys = 5L*1024*1024;
	memp[1].md_virt = NULL;
	memp[1].md_pages = (128L*1024*1024)>>12;
	memp[1].md_attr = EFI_MD_ATTR_WB;

	memp[2].md_type = EFI_MD_TYPE_FREE;
	memp[2].md_phys = 4L*1024*1024*1024;
	memp[2].md_virt = NULL;
	memp[2].md_pages = (64L*1024*1024)>>12;
	memp[2].md_attr = EFI_MD_ATTR_WB;

	memp[3].md_type = EFI_MD_TYPE_IOPORT;
	memp[3].md_phys = 0xffffc000000;
	memp[3].md_virt = NULL;
	memp[3].md_pages = (64L*1024*1024)>>12;
	memp[3].md_attr = EFI_MD_ATTR_UC;

	bi->bi_systab = (u_int64_t)&efi_systab;

	sal_stub_init();
	acpi_stub_init();

	return (0);
}
