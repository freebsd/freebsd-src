/*-
 * Copyright (c) 2003 John Baldwin <jhb@FreeBSD.org>
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <machine/apicreg.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/apicvar.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

#include "acpi.h"
#include <dev/acpica/acpivar.h>
#include <dev/acpica/madt.h>
#include <dev/pci/pcivar.h>

#define	NIOAPICS		16	/* Max number of I/O APICs */
#define	NLAPICS			16	/* Max number of local APICs */

typedef	void madt_entry_handler(APIC_HEADER *entry, void *arg);

/* These two arrays are indexed by APIC IDs. */
struct ioapic_info {
	void *io_apic;
	UINT32 io_vector;
} ioapics[NIOAPICS];

struct lapic_info {
	u_int la_present:1;
	u_int la_enabled:1;
	u_int la_apic_id:8;
} lapics[NLAPICS + 1];

static APIC_TABLE *madt;
static vm_paddr_t madt_physaddr;
static vm_offset_t madt_length;

MALLOC_DEFINE(M_MADT, "MADT Table", "ACPI MADT Table Items");

static u_char	interrupt_polarity(UINT16 Polarity);
static u_char	interrupt_trigger(UINT16 TriggerMode);
static int	madt_find_cpu(u_int acpi_id, u_int *apic_id);
static int	madt_find_interrupt(int intr, void **apic, u_int *pin);
static void	*madt_map(vm_paddr_t pa, int offset, vm_offset_t length);
static void	*madt_map_table(vm_paddr_t pa, int offset, const char *sig);
static void	madt_parse_apics(APIC_HEADER *entry, void *arg);
static void	madt_parse_interrupt_override(INTERRUPT_SOURCE_OVERRIDE *intr);
static void	madt_parse_ints(APIC_HEADER *entry, void *arg __unused);
static void	madt_parse_local_nmi(LAPIC_NMI *nmi);
static void	madt_parse_nmi(NMI *nmi);
static int	madt_probe(void);
static int	madt_probe_cpus(void);
static void	madt_probe_cpus_handler(APIC_HEADER *entry, void *arg __unused);
static int	madt_probe_table(vm_paddr_t address);
static void	madt_register(void *dummy);
static int	madt_setup_local(void);
static int	madt_setup_io(void);
static void	madt_unmap(void *data, vm_offset_t length);
static void	madt_unmap_table(void *table);
static void	madt_walk_table(madt_entry_handler *handler, void *arg);

static struct apic_enumerator madt_enumerator = {
	"MADT",
	madt_probe,
	madt_probe_cpus,
	madt_setup_local,
	madt_setup_io
};

/*
 * Code to abuse the crashdump map to map in the tables for the early
 * probe.  We cheat and make the following assumptions about how we
 * use this KVA: page 0 is used to map in the first page of each table
 * found via the RSDT or XSDT and pages 1 to n are used to map in the
 * RSDT or XSDT.  The offset is in pages; the length is in bytes.
 */
static void *
madt_map(vm_paddr_t pa, int offset, vm_offset_t length)
{
	vm_offset_t va, off;
	void *data;

	off = pa & PAGE_MASK;
	length = roundup(length + off, PAGE_SIZE);
	pa = pa & PG_FRAME;
	va = (vm_offset_t)pmap_kenter_temporary(pa, offset) +
	    (offset * PAGE_SIZE);
	data = (void *)(va + off);
	length -= PAGE_SIZE;
	while (length > 0) {
		va += PAGE_SIZE;
		pa += PAGE_SIZE;
		length -= PAGE_SIZE;
		pmap_kenter(va, pa);
		invlpg(va);
	}
	return (data);
}

static void
madt_unmap(void *data, vm_offset_t length)
{
	vm_offset_t va, off;

	va = (vm_offset_t)data;
	off = va & PAGE_MASK;
	length = roundup(length + off, PAGE_SIZE);
	va &= ~PAGE_MASK;
	while (length > 0) {
		pmap_kremove(va);
		invlpg(va);
		va += PAGE_SIZE;
		length -= PAGE_SIZE;
	}
}

static void *
madt_map_table(vm_paddr_t pa, int offset, const char *sig)
{
	ACPI_TABLE_HEADER *header;
	vm_offset_t length;

	header = madt_map(pa, offset, sizeof(ACPI_TABLE_HEADER));
	if (strncmp(header->Signature, sig, 4) != 0) {
		madt_unmap(header, sizeof(ACPI_TABLE_HEADER));
		return (NULL);
	}
	length = header->Length;
	madt_unmap(header, sizeof(ACPI_TABLE_HEADER));
	return (madt_map(pa, offset, length));
}

static void
madt_unmap_table(void *table)
{
	ACPI_TABLE_HEADER *header;

	header = (ACPI_TABLE_HEADER *)table;
	madt_unmap(table, header->Length);
}

/*
 * Look for an ACPI Multiple APIC Description Table ("APIC")
 */
static int
madt_probe(void)
{
	ACPI_POINTER rsdp_ptr;
	RSDP_DESCRIPTOR *rsdp;
	RSDT_DESCRIPTOR *rsdt;
	XSDT_DESCRIPTOR *xsdt;
	int i, count;

	if (resource_disabled("acpi", 0))
		return (ENXIO);

	/*
	 * Map in the RSDP.  Since ACPI uses AcpiOsMapMemory() which in turn
	 * calls pmap_mapdev() to find the RSDP, we assume that we can use
	 * pmap_mapdev() to map the RSDP.
	 */
	if (AcpiOsGetRootPointer(ACPI_LOGICAL_ADDRESSING, &rsdp_ptr) != AE_OK)
		return (ENXIO);
	KASSERT(rsdp_ptr.Pointer.Physical < KERNLOAD, ("RSDP too high"));
	rsdp = pmap_mapdev(rsdp_ptr.Pointer.Physical, sizeof(RSDP_DESCRIPTOR));
	if (rsdp == NULL) {
		if (bootverbose)
			printf("MADT: Failed to map RSDP\n");
		return (ENXIO);
	}

	/*
	 * For ACPI < 2.0, use the RSDT.  For ACPI >= 2.0, use the XSDT.
	 * We map the XSDT and RSDT at page 1 in the crashdump area.
	 * Page 0 is used to map in the headers of candidate ACPI tables.
	 */
	if (rsdp->Revision >= 2) {
		xsdt = madt_map_table(rsdp->XsdtPhysicalAddress, 1, XSDT_SIG);
		if (xsdt == NULL) {
			if (bootverbose)
				printf("MADT: Failed to map XSDT\n");
			return (ENXIO);
		}
		count = (xsdt->Header.Length - sizeof(ACPI_TABLE_HEADER)) /
		    sizeof(UINT64);
		for (i = 0; i < count; i++)
			if (madt_probe_table(xsdt->TableOffsetEntry[i]))
				break;
		madt_unmap_table(xsdt);
	} else {
		rsdt = madt_map_table(rsdp->RsdtPhysicalAddress, 1, RSDT_SIG);
		if (rsdt == NULL) {
			if (bootverbose)
				printf("MADT: Failed to map RSDT\n");
			return (ENXIO);
		}
		count = (rsdt->Header.Length - sizeof(ACPI_TABLE_HEADER)) /
		    sizeof(UINT32);
		for (i = 0; i < count; i++)
			if (madt_probe_table(rsdt->TableOffsetEntry[i]))
				break;
		madt_unmap_table(rsdt);
	}
	pmap_unmapdev((vm_offset_t)rsdp, sizeof(RSDP_DESCRIPTOR));
	if (madt_physaddr == 0) {
		if (bootverbose)
			printf("MADT: No MADT table found\n");
		return (ENXIO);
	}
	if (bootverbose)
		printf("MADT: Found table at 0x%jx\n",
		    (uintmax_t)madt_physaddr);

	return (0);
}

/*
 * See if a given ACPI table is the MADT.
 */
static int
madt_probe_table(vm_paddr_t address)
{
	ACPI_TABLE_HEADER *table;

	table = madt_map(address, 0, sizeof(ACPI_TABLE_HEADER));
	if (table == NULL) {
		if (bootverbose)
			printf("MADT: Failed to map table at 0x%jx\n",
			    (uintmax_t)address);
		return (0);
	}
	if (bootverbose)
		printf("Table '%.4s' at 0x%jx\n", table->Signature,
		    (uintmax_t)address);

	/* XXX: Verify checksum? */
	if (strncmp(table->Signature, APIC_SIG, 4) != 0) {
		madt_unmap(table, sizeof(ACPI_TABLE_HEADER));
		return (0);
	}
	madt_physaddr = address;
	madt_length = table->Length;
	madt_unmap(table, sizeof(ACPI_TABLE_HEADER));
	return (1);
}

/*
 * Run through the MP table enumerating CPUs.
 */
static int
madt_probe_cpus(void)
{

	madt = madt_map_table(madt_physaddr, 0, APIC_SIG);
	KASSERT(madt != NULL, ("Unable to re-map MADT"));
	madt_walk_table(madt_probe_cpus_handler, NULL);
	madt_unmap_table(madt);
	madt = NULL;
	return (0);
}

/*
 * Initialize the local APIC on the BSP.
 */
static int
madt_setup_local(void)
{

	madt = pmap_mapdev(madt_physaddr, madt_length);
	lapic_init((uintptr_t)madt->LocalApicAddress);
	printf("ACPI APIC Table: <%.*s %.*s>\n",
	    sizeof(madt->Header.OemId), madt->Header.OemId,
	    sizeof(madt->Header.OemTableId), madt->Header.OemTableId);

	/*
	 * We ignore 64-bit local APIC override entries.  Should we
	 * perhaps emit a warning here if we find one?
	 */
	return (0);
}

/*
 * Run through the MP table enumerating I/O APICs.
 */
static int
madt_setup_io(void)
{
	int i;

	/* First, we run through adding I/O APIC's. */
	madt_walk_table(madt_parse_apics, NULL);

	/* Second, we run through the table tweaking interrupt sources. */
	madt_walk_table(madt_parse_ints, NULL);

	/* Third, we register all the I/O APIC's. */
	for (i = 0; i < NIOAPICS; i++)
		if (ioapics[i].io_apic != NULL)
			ioapic_register(ioapics[i].io_apic);

	/* Finally, we throw the switch to enable the I/O APIC's. */
	acpi_SetDefaultIntrModel(ACPI_INTR_APIC);

	return (0);
}

static void
madt_register(void *dummy __unused)
{

	apic_register_enumerator(&madt_enumerator);
}
SYSINIT(madt_register, SI_SUB_TUNABLES - 1, SI_ORDER_FIRST,
    madt_register, NULL)

/*
 * Call the handler routine for each entry in the MADT table.
 */
static void
madt_walk_table(madt_entry_handler *handler, void *arg)
{
	APIC_HEADER *entry;
	u_char *p, *end;

	end = (u_char *)(madt) + madt->Header.Length;
	for (p = (u_char *)(madt + 1); p < end; ) {
		entry = (APIC_HEADER *)p;
		handler(entry, arg);
		p += entry->Length;
	}
}

static void
madt_probe_cpus_handler(APIC_HEADER *entry, void *arg)
{
	PROCESSOR_APIC *proc;
	struct lapic_info *la;

	switch (entry->Type) {
	case APIC_PROC:
		/*
		 * The MADT does not include a BSP flag, so we have to
		 * let the MP code figure out which CPU is the BSP on
		 * its own.
		 */
		proc = (PROCESSOR_APIC *)entry;
		if (bootverbose)
			printf("MADT: Found CPU APIC ID %d ACPI ID %d: %s\n",
			    proc->LocalApicId, proc->ProcessorApicId,
			    proc->ProcessorEnabled ? "enabled" : "disabled");
		if (proc->ProcessorApicId > NLAPICS)
			panic("%s: CPU ID %d too high", __func__,
			    proc->ProcessorApicId);
		la = &lapics[proc->ProcessorApicId];
		KASSERT(la->la_present == 0,
		    ("Duplicate local ACPI ID %d", proc->ProcessorApicId));
		la->la_present = 1;
		la->la_apic_id = proc->LocalApicId;
		if (proc->ProcessorEnabled) {
			la->la_enabled = 1;
			lapic_create(proc->LocalApicId, 0);
		}
		break;
	}
}


/*
 * Add an I/O APIC from an entry in the table.
 */
static void
madt_parse_apics(APIC_HEADER *entry, void *arg __unused)
{
	IO_APIC *apic;


	switch (entry->Type) {
	case APIC_IO:
		apic = (IO_APIC *)entry;
		if (bootverbose)
			printf("MADT: Found IO APIC ID %d, Vector %d at %p\n",
			    apic->IoApicId, apic->Vector,
			    (void *)apic->IoApicAddress);
		if (apic->IoApicId >= NIOAPICS)
			panic("%s: I/O APIC ID %d too high", __func__,
			    apic->IoApicId);
		if (ioapics[apic->IoApicId].io_apic != NULL)
			panic("%s: Double APIC ID %d", __func__,
			    apic->IoApicId);
		ioapics[apic->IoApicId].io_apic = ioapic_create(
			(uintptr_t)apic->IoApicAddress, apic->IoApicId,
			    apic->Vector);
		ioapics[apic->IoApicId].io_vector = apic->Vector;
		break;
	default:
		break;
	}
}

/*
 * Determine properties of an interrupt source.  Note that for ACPI,
 * these are only used for ISA interrupts, so we assume ISA bus values
 * (Active Hi, Edge Triggered) for conforming values.
 */
static u_char
interrupt_polarity(UINT16 Polarity)
{

	switch (Polarity) {
	case APIC_POLARITY_CONFORM:
	case APIC_POLARITY_ACTIVEHI:
		return (1);
	case APIC_POLARITY_ACTIVELO:
		return (0);
	default:
		panic("Bogus Interrupt Polarity");
	}
}

static u_char
interrupt_trigger(UINT16 TriggerMode)
{

	switch (TriggerMode) {
	case APIC_TRIGGER_CONFORM:
	case APIC_TRIGGER_EDGE:
		return (1);
	case APIC_TRIGGER_LEVEL:
		return (0);
	default:
		panic("Bogus Interrupt Trigger Mode");
	}
}

/*
 * Find the local APIC ID associated with a given ACPI Processor ID.
 */
static int
madt_find_cpu(u_int acpi_id, u_int *apic_id)
{

	if (!lapics[acpi_id].la_present)
		return (ENOENT);
	*apic_id = lapics[acpi_id].la_apic_id;
	if (lapics[acpi_id].la_enabled)
		return (0);
	else
		return (ENXIO);
}

/*
 * Find the IO APIC and pin on that APIC associated with a given global
 * interrupt.
 */
static int
madt_find_interrupt(int intr, void **apic, u_int *pin)
{
	int i, best;

	best = -1;
	for (i = 0; i < NIOAPICS; i++) {
		if (ioapics[i].io_apic == NULL ||
		    ioapics[i].io_vector > intr)
			continue;
		if (best == -1 ||
		    ioapics[best].io_vector < ioapics[i].io_vector)
			best = i;
	}
	if (best == -1)
		return (ENOENT);
	*apic = ioapics[best].io_apic;
	*pin = intr - ioapics[best].io_vector;
	if (*pin > 32)
		printf("WARNING: Found intpin of %u for vector %d\n", *pin,
		    intr);
	return (0);
}

/*
 * Parse an interrupt source override for an ISA interrupt.
 */
static void
madt_parse_interrupt_override(INTERRUPT_SOURCE_OVERRIDE *intr)
{
	void *ioapic;
	u_int pin;

	if (bootverbose)
		printf("MADT: intr override: source %u, irq %u\n",
		    intr->Source, intr->GlobalSystemInterrupt);
	KASSERT(intr->Bus == 0, ("bus for interrupt overrides must be zero"));
	if (madt_find_interrupt(intr->GlobalSystemInterrupt,
	    &ioapic, &pin) != 0) {
		printf("MADT: Could not find APIC for vector %d (IRQ %d)\n",
		    intr->GlobalSystemInterrupt, intr->Source);
		return;
	}

	if (intr->Source != intr->GlobalSystemInterrupt)
		ioapic_remap_vector(ioapic, pin, intr->Source);
	ioapic_set_triggermode(ioapic, pin,
	    interrupt_trigger(intr->TriggerMode));
	ioapic_set_polarity(ioapic, pin, interrupt_polarity(intr->Polarity));
}

/*
 * Parse an entry for an NMI routed to an IO APIC.
 */
static void
madt_parse_nmi(NMI *nmi)
{
	void *ioapic;
	u_int pin;

	if (madt_find_interrupt(nmi->GlobalSystemInterrupt,
	    &ioapic, &pin) != 0) {
		printf("MADT: Could not find APIC for vector %d\n",
		    nmi->GlobalSystemInterrupt);
		return;
	}

	ioapic_set_nmi(ioapic, pin);
	if (nmi->TriggerMode != APIC_TRIGGER_CONFORM)
		ioapic_set_triggermode(ioapic, pin,
		    interrupt_trigger(nmi->TriggerMode));
	if (nmi->Polarity != APIC_TRIGGER_CONFORM)
		ioapic_set_polarity(ioapic, pin,
		    interrupt_polarity(nmi->Polarity));
}

/*
 * Parse an entry for an NMI routed to a local APIC LVT pin.
 */
static void
madt_parse_local_nmi(LAPIC_NMI *nmi)
{
	u_int apic_id, pin;

	if (nmi->ProcessorApicId == 0xff)
		apic_id = APIC_ID_ALL;
	else if (madt_find_cpu(nmi->ProcessorApicId, &apic_id) != 0) {
		if (bootverbose)
			printf("MADT: Ignoring local NMI routed to ACPI CPU %u\n",
			    nmi->ProcessorApicId);
		return;
	}
	if (nmi->LINTPin == 0)
		pin = LVT_LINT0;
	else
		pin = LVT_LINT1;
	lapic_set_lvt_mode(apic_id, pin, APIC_LVT_DM_NMI);
	if (nmi->TriggerMode != APIC_TRIGGER_CONFORM)
		lapic_set_lvt_triggermode(apic_id, pin,
		    interrupt_trigger(nmi->TriggerMode));
	if (nmi->Polarity != APIC_POLARITY_CONFORM)
		lapic_set_lvt_polarity(apic_id, pin,
		    interrupt_polarity(nmi->Polarity));
}

/*
 * Parse interrupt entries.
 */
static void
madt_parse_ints(APIC_HEADER *entry, void *arg __unused)
{

	switch (entry->Type) {
	case APIC_INTERRUPT_SOURCE_OVERRIDE:
		madt_parse_interrupt_override(
			(INTERRUPT_SOURCE_OVERRIDE *)entry);
		break;
	case APIC_NMI:
		madt_parse_nmi((NMI *)entry);
		break;
	case APIC_LOCAL_APIC_NMI:
		madt_parse_local_nmi((LAPIC_NMI *)entry);
		break;
	}
}

/*
 * Setup per-CPU ACPI IDs.
 */
static void
madt_set_ids(void *dummy)
{
	struct pcpu *pc;
	u_int i, j;

	if (madt == NULL)
		return;
	for (i = 0; i < MAXCPU; i++) {
		if (CPU_ABSENT(i))
			continue;
		pc = pcpu_find(i);
		KASSERT(pc != NULL, ("no pcpu data for CPU %d", i));
		for (j = 0; j < NLAPICS + 1; j++) {
			if (!lapics[j].la_present || !lapics[j].la_enabled)
				continue;
			if (lapics[j].la_apic_id == pc->pc_apic_id) {
				pc->pc_acpi_id = j;
				if (bootverbose)
					printf("APIC: CPU %u has ACPI ID %u\n",
					    i, j);
				break;
			}
		}
		if (j == NLAPICS + 1)
			panic("Unable to find ACPI ID for CPU %d", i);
	}
}
SYSINIT(madt_set_ids, SI_SUB_CPU, SI_ORDER_ANY, madt_set_ids, NULL)
