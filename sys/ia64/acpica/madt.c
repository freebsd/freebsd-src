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

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/actables.h>

#include <machine/md_var.h>

extern u_int64_t ia64_lapic_address;

struct sapic *sapic_create(int, int, u_int64_t);

static void
print_entry(APIC_HEADER *entry)
{

	switch (entry->Type) {
	case APIC_XRUPT_OVERRIDE: {
		MADT_INTERRUPT_OVERRIDE *iso =
		    (MADT_INTERRUPT_OVERRIDE *)entry;
		printf("\tInterrupt source override entry\n");
		printf("\t\tBus=%d, Source=%d, Irq=0x%x\n", iso->Bus,
		    iso->Source, iso->Interrupt);
		break;
	}

	case APIC_IO:
		printf("\tI/O APIC entry\n");
		break;

	case APIC_IO_SAPIC: {
		MADT_IO_SAPIC *sapic = (MADT_IO_SAPIC *)entry;
		printf("\tI/O SAPIC entry\n");
		printf("\t\tId=0x%x, InterruptBase=0x%x, Address=0x%lx\n",
		    sapic->IoSapicId, sapic->InterruptBase, sapic->Address);
		break;
	}

	case APIC_LOCAL_NMI:
		printf("\tLocal APIC NMI entry\n");
		break;

	case APIC_ADDRESS_OVERRIDE: {
		MADT_ADDRESS_OVERRIDE *lapic = (MADT_ADDRESS_OVERRIDE *)entry;
		printf("\tLocal APIC override entry\n");
		printf("\t\tLocal APIC address=0x%jx\n", lapic->Address);
		break;
	}

	case APIC_LOCAL_SAPIC: {
		MADT_LOCAL_SAPIC *sapic = (MADT_LOCAL_SAPIC *)entry;
		printf("\tLocal SAPIC entry\n");
		printf("\t\tProcessorId=0x%x, Id=0x%x, Eid=0x%x",
		    sapic->ProcessorId, sapic->LocalSapicId,
		    sapic->LocalSapicEid);
		if (!sapic->ProcessorEnabled)
			printf(" (disabled)");
		printf("\n");
		break;
	}

	case APIC_NMI:
		printf("\tNMI entry\n");
		break;

	case APIC_XRUPT_SOURCE: {
		MADT_INTERRUPT_SOURCE *pis = (MADT_INTERRUPT_SOURCE *)entry;
		printf("\tPlatform interrupt entry\n");
		printf("\t\tPolarity=%d, TriggerMode=%d, Id=0x%x, "
		    "Eid=0x%x, Vector=0x%x, Irq=%d\n",
		    pis->Polarity, pis->TriggerMode, pis->ProcessorId,
		    pis->ProcessorEid, pis->IoSapicVector, pis->Interrupt);
		break;
	}

	case APIC_PROCESSOR:
		printf("\tLocal APIC entry\n");
		break;

	default:
		printf("\tUnknown type %d entry\n", entry->Type);
		break;
	}
}

void
ia64_probe_sapics(void)
{
	ACPI_POINTER rsdp_ptr;
	APIC_HEADER *entry;
	MULTIPLE_APIC_TABLE *table;
	RSDP_DESCRIPTOR *rsdp;
	XSDT_DESCRIPTOR *xsdt;
	char *end, *p;
	int t, tables;

	if (AcpiOsGetRootPointer(ACPI_LOGICAL_ADDRESSING, &rsdp_ptr) != AE_OK)
		return;

	rsdp = (RSDP_DESCRIPTOR *)IA64_PHYS_TO_RR7(rsdp_ptr.Pointer.Physical);
	xsdt = (XSDT_DESCRIPTOR *)IA64_PHYS_TO_RR7(rsdp->XsdtPhysicalAddress);

	tables = (UINT64 *)((char *)xsdt + xsdt->Length) -
	    xsdt->TableOffsetEntry;

	for (t = 0; t < tables; t++) {
		table = (MULTIPLE_APIC_TABLE *)
		    IA64_PHYS_TO_RR7(xsdt->TableOffsetEntry[t]);

		if (bootverbose)
			printf("Table '%c%c%c%c' at %p\n",
			    table->Signature[0], table->Signature[1],
			    table->Signature[2], table->Signature[3], table);

		if (strncmp(table->Signature, APIC_SIG, 4) != 0 ||
		    ACPI_FAILURE(AcpiTbVerifyTableChecksum((void *)table)))
			continue;

		/* Save the address of the processor interrupt block. */
		if (bootverbose)
			printf("\tLocal APIC address=0x%x\n",
			    table->LocalApicAddress);
		ia64_lapic_address = table->LocalApicAddress;

		end = (char *)table + table->Length;
		p = (char *)(table + 1);
		while (p < end) {
			entry = (APIC_HEADER *)p;

			if (bootverbose)
				print_entry(entry);

			switch (entry->Type) {
			case APIC_IO_SAPIC: {
				MADT_IO_SAPIC *sapic = (MADT_IO_SAPIC *)entry;
				sapic_create(sapic->IoSapicId,
				    sapic->InterruptBase, sapic->Address);
				break;
			}

			case APIC_ADDRESS_OVERRIDE: {
				MADT_ADDRESS_OVERRIDE *lapic =
				    (MADT_ADDRESS_OVERRIDE*)entry;
				ia64_lapic_address = lapic->Address;
				break;
			}

#ifdef SMP
			case APIC_LOCAL_SAPIC: {
				MADT_LOCAL_SAPIC *sapic =
				    (MADT_LOCAL_SAPIC *)entry;
				if (sapic->ProcessorEnabled)
					cpu_mp_add(sapic->ProcessorId,
					    sapic->LocalSapicId,
					    sapic->LocalSapicEid);
				break;
			}
#endif

			default:
				break;
			}

			p += entry->Length;
		}
	}
}

/*
 * Count the number of local SAPIC entries in the APIC table. Every enabled
 * entry corresponds to a processor.
 */
int
ia64_count_cpus(void)
{
	ACPI_POINTER rsdp_ptr;
	MULTIPLE_APIC_TABLE *table;
	MADT_LOCAL_SAPIC *entry;
	RSDP_DESCRIPTOR *rsdp;
	XSDT_DESCRIPTOR *xsdt;
	char *end, *p;
	int cpus, t, tables;

	if (AcpiOsGetRootPointer(ACPI_LOGICAL_ADDRESSING, &rsdp_ptr) != AE_OK)
		return (0);

	rsdp = (RSDP_DESCRIPTOR *)IA64_PHYS_TO_RR7(rsdp_ptr.Pointer.Physical);
	xsdt = (XSDT_DESCRIPTOR *)IA64_PHYS_TO_RR7(rsdp->XsdtPhysicalAddress);

	tables = (UINT64 *)((char *)xsdt + xsdt->Length) -
	    xsdt->TableOffsetEntry;

	cpus = 0;

	for (t = 0; t < tables; t++) {
		table = (MULTIPLE_APIC_TABLE *)
		    IA64_PHYS_TO_RR7(xsdt->TableOffsetEntry[t]);

		if (strncmp(table->Signature, APIC_SIG, 4) != 0 ||
		    ACPI_FAILURE(AcpiTbVerifyTableChecksum((void *)table)))
			continue;

		end = (char *)table + table->Length;
		p = (char *)(table + 1);
		while (p < end) {
			entry = (MADT_LOCAL_SAPIC *)p;

			if (entry->Type == APIC_LOCAL_SAPIC &&
			    entry->ProcessorEnabled)
				cpus++;

			p += entry->Length;
		}
	}

	return (cpus);
}
