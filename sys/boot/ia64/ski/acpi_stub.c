/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/boot/ia64/ski/acpi_stub.c,v 1.8.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <contrib/dev/acpica/acpi.h>

#define APIC_IO_SAPIC                   6
#define APIC_LOCAL_SAPIC                7

#pragma pack(1)

typedef struct  /* LOCAL SAPIC */
{
	ACPI_SUBTABLE_HEADER Header;
	UINT8		ProcessorId;		/* ACPI processor id */
	UINT8		LocalSapicId;		/* Processor local SAPIC id */
	UINT8		LocalSapicEid;		/* Processor local SAPIC eid */
	UINT8		Reserved[3];
	UINT32		ProcessorEnabled: 1;
	UINT32		FlagsReserved: 31;
} LOCAL_SAPIC;

typedef struct  /* IO SAPIC */
{
	ACPI_SUBTABLE_HEADER Header;
	UINT8		IoSapicId;		/* I/O SAPIC ID */
	UINT8		Reserved;		/* reserved - must be zero */
	UINT32		Vector;			/* interrupt base */
	UINT64		IoSapicAddress;		/* SAPIC's physical address */
} IO_SAPIC;

/*
 */

struct {
	ACPI_TABLE_MADT		MADT;
	ACPI_MADT_LOCAL_SAPIC	cpu0;
	ACPI_MADT_LOCAL_SAPIC	cpu1;
	ACPI_MADT_LOCAL_SAPIC	cpu2;
	ACPI_MADT_LOCAL_SAPIC	cpu3;
	ACPI_MADT_IO_SAPIC	sapic;
} apic = {
	/* Header. */
	{
		ACPI_SIG_MADT,			/* Signature. */
		sizeof(apic),			/* Length of table. */
		0,				/* ACPI minor revision. */
		0,				/* Checksum. */
		"FBSD",				/* OEM Id. */
		"SKI",				/* OEM table Id. */
		0,				/* OEM revision. */
		"FBSD",				/* ASL compiler Id. */
		0,				/* ASL revision. */
		0xfee00000,
	},
	/* cpu0. */
	{
		APIC_LOCAL_SAPIC,		/* Type. */
		sizeof(apic.cpu0),		/* Length. */
		0,				/* ACPI processor id */
		0,				/* Processor local SAPIC id */
		0,				/* Processor local SAPIC eid */
		{ 0, 0, 0 },
		1,				/* FL: Enabled. */
	},
	/* cpu1. */
	{
		APIC_LOCAL_SAPIC,		/* Type. */
		sizeof(apic.cpu1),		/* Length. */
		1,				/* ACPI processor id */
		0,				/* Processor local SAPIC id */
		1,				/* Processor local SAPIC eid */
		{ 0, 0, 0 },
		1,				/* FL: Enabled. */
	},
	/* cpu2. */
	{
		APIC_LOCAL_SAPIC,		/* Type. */
		sizeof(apic.cpu2),		/* Length. */
		2,				/* ACPI processor id */
		1,				/* Processor local SAPIC id */
		0,				/* Processor local SAPIC eid */
		{ 0, 0, 0 },
		0,				/* FL: Enabled. */
	},
	/* cpu3. */
	{
		APIC_LOCAL_SAPIC,		/* Type. */
		sizeof(apic.cpu3),		/* Length. */
		3,				/* ACPI processor id */
		1,				/* Processor local SAPIC id */
		1,				/* Processor local SAPIC eid */
		{ 0, 0, 0 },
		0,				/* FL: Enabled. */
	},
	/* sapic. */
	{
		APIC_IO_SAPIC,			/* Type. */
		sizeof(apic.sapic),		/* Length. */
		4,				/* IO SAPIC id. */
		0,
		16,				/* Interrupt base. */
		0xfec00000			/* IO SAPIC address. */
	}
};

struct {
	ACPI_TABLE_HEADER	Header;
	UINT64			apic_tbl;
} xsdt = {
	{
		ACPI_SIG_XSDT,		/* Signature. */
		sizeof(xsdt),		/* Length of table. */
		0,			/* ACPI minor revision. */
		0,			/* XXX checksum. */
		"FBSD",			/* OEM Id. */
		"SKI",			/* OEM table Id. */
		0,			/* OEM revision. */
		"FBSD",			/* ASL compiler Id. */
		0			/* ASL revision. */
	},
	0UL				/* XXX APIC table address. */
};

ACPI_TABLE_RSDP acpi_root = {
	ACPI_SIG_RSDP,
	0,				/* XXX checksum. */
	"FBSD",
	2,				/* ACPI Rev 2.0. */
	0UL,
	sizeof(xsdt),			/* XSDT length. */
	0UL,				/* XXX PA of XSDT. */
	0,				/* XXX Extended checksum. */
};

static void
cksum(void *addr, int sz, UINT8 *sum)
{
	UINT8 *p, s;

	p = addr;
	s = 0;
	while (sz--)
		s += *p++;
	*sum = -s;
}

void
acpi_stub_init(void)
{
	acpi_root.XsdtPhysicalAddress = (UINT64)&xsdt;
	cksum(&acpi_root, 20, &acpi_root.Checksum);
	cksum(&acpi_root, sizeof(acpi_root), &acpi_root.ExtendedChecksum);

	cksum(&apic, sizeof(apic), &apic.MADT.Header.Checksum);
	xsdt.apic_tbl = (UINT32)&apic;
	cksum(&xsdt, sizeof(xsdt), &xsdt.Header.Checksum);
}
