/*-
 * Copyright (c) 2001 Doug Rabson
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
 * $FreeBSD$
 */

#include "acpi.h"

void cpu_mp_add(uint, uint, uint);
struct sapic *sapic_create(int, int, u_int64_t);

#pragma pack(1)

#define	APIC_INTERRUPT_SOURCE_OVERRIDE	2
#define	APIC_NMI			3
#define	APIC_LOCAL_APIC_NMI		4
#define	APIC_LOCAL_APIC_OVERRIDE	5
#define	APIC_IO_SAPIC			6
#define	APIC_LOCAL_SAPIC		7
#define	APIC_PLATFORM_INTERRUPT		8
 
typedef struct	/* Interrupt Source Override */
{
	APIC_HEADER	header;
	UINT8		Bus;
	UINT8		Source;
	UINT32		GlobalSystemInterrupt;
	UINT16		Flags;
} INTERRUPT_SOURCE_OVERRIDE;

typedef struct	/* IO SAPIC */
{
	APIC_HEADER	header;
	UINT8		IoSapicId;		/* I/O SAPIC ID */
	UINT8		Reserved;		/* reserved - must be zero */
	UINT32		Vector;			/* interrupt base */
	UINT64		IoSapicAddress;		/* SAPIC's physical address */
} IO_SAPIC;

typedef struct  /* LOCAL SAPIC */
{
	APIC_HEADER	header;
	UINT8		ProcessorId;		/* ACPI processor id */
	UINT8		LocalSapicId;		/* Processor local SAPIC id */
	UINT8		LocalSapicEid;		/* Processor local SAPIC eid */
	UINT8		Reserved[3];
	UINT32		ProcessorEnabled: 1;
	UINT32		FlagsReserved: 31;
} LOCAL_SAPIC;

typedef struct  /* PLATFORM INTERRUPT SOURCE */
{
	APIC_HEADER	header;
	UINT16		Polarity   : 2;		/* Polarity of input signal */
	UINT16		TriggerMode: 2;		/* Trigger mode of input signal */
	UINT16		Reserved1  : 12;
	UINT8		InterruptType;		/* 1-PMI, 2-INIT, 3-Error */
	UINT8		ProcessorId;		/* Processor ID of destination */
	UINT8		ProcessorEid;		/* Processor EID of destination */
	UINT8		IoSapicVector;		/* Value for redirection table */
	UINT32		GlobalSystemInterrupt;	/* Global System Interrupt */
	UINT32		Reserved2;
} PLATFORM_INTERRUPT_SOURCE;

#pragma pack()

static void
parse_interrupt_override(INTERRUPT_SOURCE_OVERRIDE *override)
{
	if (bootverbose)
		printf("\t\tBus=%d, Source=%d, Irq=0x%x\n", override->Bus,
		    override->Source, override->GlobalSystemInterrupt);
}

static void
parse_io_sapic(IO_SAPIC *sapic)
{
	if (bootverbose)
		printf("\t\tId=0x%x, Vector=0x%x, Address=0x%lx\n",
		    sapic->IoSapicId, sapic->Vector, sapic->IoSapicAddress);
	sapic_create(sapic->IoSapicId, sapic->Vector, sapic->IoSapicAddress);
}

static void
parse_local_sapic(LOCAL_SAPIC *sapic)
{
	if (bootverbose) {
		printf("\t\tProcessorId=0x%x, Id=0x%x, Eid=0x%x",
		    sapic->ProcessorId, sapic->LocalSapicId,
		    sapic->LocalSapicEid);
		if (!sapic->ProcessorEnabled)
			printf(" (disabled)");
		printf("\n");
	}
#ifdef SMP
	if (sapic->ProcessorEnabled)
		cpu_mp_add(sapic->ProcessorId, sapic->LocalSapicId,
		    sapic->LocalSapicEid);
#endif
}

static void
parse_platform_interrupt(PLATFORM_INTERRUPT_SOURCE *source)
{
	if (bootverbose)
		printf("\t\tPolarity=%d, TriggerMode=%d, Id=0x%x, "
		    "Eid=0x%x, Vector=0x%x, Irq=%d\n", source->Polarity,
		    source->TriggerMode, source->ProcessorId,
		    source->ProcessorEid, source->IoSapicVector,
		    source->GlobalSystemInterrupt);
}

static void
parse_madt(APIC_TABLE *madt)
{
	char			*p, *end;

	/*
	 * MADT header is followed by a number of variable length
	 * structures.
	 */
	end = (char *) madt + madt->Header.Length;
	for (p = (char *) (madt + 1); p < end; ) {
		APIC_HEADER *head = (APIC_HEADER *) p;

		if (bootverbose)
			printf("\t");
		switch (head->Type) {
		case APIC_PROC:
			if (bootverbose)
				printf("Local APIC entry\n");
			break;

		case APIC_IO:
			if (bootverbose)
				printf("I/O APIC entry\n");
			break;

		case APIC_INTERRUPT_SOURCE_OVERRIDE:
			if (bootverbose)
				printf("Interrupt source override entry\n");
			parse_interrupt_override
				((INTERRUPT_SOURCE_OVERRIDE *) head);
			break;

		case APIC_NMI:
			if (bootverbose)
				printf("NMI entry\n");
			break;

		case APIC_LOCAL_APIC_NMI:
			if (bootverbose)
				printf("Local APIC NMI entry\n");
			break;


		case APIC_LOCAL_APIC_OVERRIDE:
			if (bootverbose)
				printf("Local APIC override entry\n");
			break;

		case APIC_IO_SAPIC:
			if (bootverbose)
				printf("I/O SAPIC entry\n");
			parse_io_sapic((IO_SAPIC *) head);
			break;

		case APIC_LOCAL_SAPIC:
			if (bootverbose)
				printf("Local SAPIC entry\n");
			parse_local_sapic((LOCAL_SAPIC *) head);
			break;

		case APIC_PLATFORM_INTERRUPT:
			if (bootverbose)
				printf("Platform interrupt entry\n");
			parse_platform_interrupt
				((PLATFORM_INTERRUPT_SOURCE *) head);
			break;

		default:
			if (bootverbose)
				printf("Unknown type %d entry\n", head->Type);
			break;
		}

		p = p + head->Length;
	}
}

void
ia64_probe_sapics(void)
{
	ACPI_PHYSICAL_ADDRESS	rsdp_phys;
	RSDP_DESCRIPTOR		*rsdp;
	XSDT_DESCRIPTOR		*xsdt;
	ACPI_TABLE_HEADER	*table;
	int			i, count;

	if (AcpiOsGetRootPointer(0, &rsdp_phys) != AE_OK)
		return;

	rsdp = (RSDP_DESCRIPTOR *)IA64_PHYS_TO_RR7(rsdp_phys);
	xsdt = (XSDT_DESCRIPTOR *)IA64_PHYS_TO_RR7(rsdp->XsdtPhysicalAddress);

	count = (UINT64 *)((char *)xsdt + xsdt->Header.Length)
	    - xsdt->TableOffsetEntry;
	for (i = 0; i < count; i++) {
		table = (ACPI_TABLE_HEADER *)
			IA64_PHYS_TO_RR7(xsdt->TableOffsetEntry[i]);

		if (bootverbose)
			printf("Table '%c%c%c%c' at %p\n", table->Signature[0],
			    table->Signature[1], table->Signature[2],
			    table->Signature[3], table);

		if (!strncmp(table->Signature, APIC_SIG, 4))
			parse_madt((APIC_TABLE *) table);
	}
}
