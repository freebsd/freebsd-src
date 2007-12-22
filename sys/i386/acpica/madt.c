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

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/actables.h>
#include <dev/acpica/acpivar.h>
#include <dev/pci/pcivar.h>

typedef	void madt_entry_handler(ACPI_SUBTABLE_HEADER *entry, void *arg);

/* These two arrays are indexed by APIC IDs. */
struct ioapic_info {
	void *io_apic;
	UINT32 io_vector;
} ioapics[MAX_APIC_ID + 1];

struct lapic_info {
	u_int la_enabled:1;
	u_int la_acpi_id:8;
} lapics[MAX_APIC_ID + 1];

static int madt_found_sci_override;
static ACPI_TABLE_MADT *madt;
static vm_paddr_t madt_physaddr;
static vm_offset_t madt_length;

MALLOC_DEFINE(M_MADT, "madt_table", "ACPI MADT Table Items");

static enum intr_polarity interrupt_polarity(UINT16 IntiFlags, UINT8 Source);
static enum intr_trigger interrupt_trigger(UINT16 IntiFlags, UINT8 Source);
static int	madt_find_cpu(u_int acpi_id, u_int *apic_id);
static int	madt_find_interrupt(int intr, void **apic, u_int *pin);
static void	*madt_map(vm_paddr_t pa, int offset, vm_offset_t length);
static void	*madt_map_table(vm_paddr_t pa, int offset, const char *sig);
static void	madt_parse_apics(ACPI_SUBTABLE_HEADER *entry, void *arg);
static void	madt_parse_interrupt_override(
		    ACPI_MADT_INTERRUPT_OVERRIDE *intr);
static void	madt_parse_ints(ACPI_SUBTABLE_HEADER *entry,
		    void *arg __unused);
static void	madt_parse_local_nmi(ACPI_MADT_LOCAL_APIC_NMI *nmi);
static void	madt_parse_nmi(ACPI_MADT_NMI_SOURCE *nmi);
static int	madt_probe(void);
static int	madt_probe_cpus(void);
static void	madt_probe_cpus_handler(ACPI_SUBTABLE_HEADER *entry,
		    void *arg __unused);
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
	void *table;

	header = madt_map(pa, offset, sizeof(ACPI_TABLE_HEADER));
	if (strncmp(header->Signature, sig, ACPI_NAME_SIZE) != 0) {
		madt_unmap(header, sizeof(ACPI_TABLE_HEADER));
		return (NULL);
	}
	length = header->Length;
	madt_unmap(header, sizeof(ACPI_TABLE_HEADER));
	table = madt_map(pa, offset, length);
	if (ACPI_FAILURE(AcpiTbChecksum(table, length))) {
		if (bootverbose)
			printf("MADT: Failed checksum for table %s\n", sig);
		madt_unmap(table, length);
		return (NULL);
	}
	return (table);
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
	ACPI_PHYSICAL_ADDRESS rsdp_ptr;
	ACPI_TABLE_RSDP *rsdp;
	ACPI_TABLE_RSDT *rsdt;
	ACPI_TABLE_XSDT *xsdt;
	int i, count;

	if (resource_disabled("acpi", 0))
		return (ENXIO);

	/*
	 * Map in the RSDP.  Since ACPI uses AcpiOsMapMemory() which in turn
	 * calls pmap_mapbios() to find the RSDP, we assume that we can use
	 * pmap_mapbios() to map the RSDP.
	 */
	if ((rsdp_ptr = AcpiOsGetRootPointer()) == 0)
		return (ENXIO);
	rsdp = pmap_mapbios(rsdp_ptr, sizeof(ACPI_TABLE_RSDP));
	if (rsdp == NULL) {
		if (bootverbose)
			printf("MADT: Failed to map RSDP\n");
		return (ENXIO);
	}

	/*
	 * For ACPI >= 2.0, use the XSDT if it is available.
	 * Otherwise, use the RSDT.  We map the XSDT or RSDT at page 1
	 * in the crashdump area.  Page 0 is used to map in the
	 * headers of candidate ACPI tables.
	 */
	if (rsdp->Revision >= 2 && rsdp->XsdtPhysicalAddress != 0) {
		/*
		 * AcpiOsGetRootPointer only verifies the checksum for
		 * the version 1.0 portion of the RSDP.  Version 2.0 has
		 * an additional checksum that we verify first.
		 */
		if (AcpiTbChecksum((UINT8 *)rsdp, ACPI_RSDP_XCHECKSUM_LENGTH)) {
			if (bootverbose)
				printf("MADT: RSDP failed extended checksum\n");
			return (ENXIO);
		}
		xsdt = madt_map_table(rsdp->XsdtPhysicalAddress, 1,
		    ACPI_SIG_XSDT);
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
		rsdt = madt_map_table(rsdp->RsdtPhysicalAddress, 1,
		    ACPI_SIG_RSDT);
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
	pmap_unmapbios((vm_offset_t)rsdp, sizeof(ACPI_TABLE_RSDP));
	if (madt_physaddr == 0) {
		if (bootverbose)
			printf("MADT: No MADT table found\n");
		return (ENXIO);
	}
	if (bootverbose)
		printf("MADT: Found table at 0x%jx\n",
		    (uintmax_t)madt_physaddr);

	/*
	 * Verify that we can map the full table and that its checksum is
	 * correct, etc.
	 */
	madt = madt_map_table(madt_physaddr, 0, ACPI_SIG_MADT);
	if (madt == NULL)
		return (ENXIO);
	madt_unmap_table(madt);
	madt = NULL;

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

	if (strncmp(table->Signature, ACPI_SIG_MADT, ACPI_NAME_SIZE) != 0) {
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

	madt = madt_map_table(madt_physaddr, 0, ACPI_SIG_MADT);
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

	madt = pmap_mapbios(madt_physaddr, madt_length);
	lapic_init(madt->Address);
	printf("ACPI APIC Table: <%.*s %.*s>\n",
	    (int)sizeof(madt->Header.OemId), madt->Header.OemId,
	    (int)sizeof(madt->Header.OemTableId), madt->Header.OemTableId);

	/*
	 * We ignore 64-bit local APIC override entries.  Should we
	 * perhaps emit a warning here if we find one?
	 */
	return (0);
}

/*
 * Enumerate I/O APICs and setup interrupt sources.
 */
static int
madt_setup_io(void)
{
	void *ioapic;
	u_int pin;
	int i;

	/* Try to initialize ACPI so that we can access the FADT. */
	i = acpi_Startup();
	if (ACPI_FAILURE(i)) {
		printf("MADT: ACPI Startup failed with %s\n",
		    AcpiFormatException(i));
		printf("Try disabling either ACPI or apic support.\n");
		panic("Using MADT but ACPI doesn't work");
	}
		    
	/* First, we run through adding I/O APIC's. */
	madt_walk_table(madt_parse_apics, NULL);

	/* Second, we run through the table tweaking interrupt sources. */
	madt_walk_table(madt_parse_ints, NULL);

	/*
	 * If there was not an explicit override entry for the SCI,
	 * force it to use level trigger and active-low polarity.
	 */
	if (!madt_found_sci_override) {
		if (madt_find_interrupt(AcpiGbl_FADT.SciInterrupt, &ioapic,
		    &pin) != 0)
			printf("MADT: Could not find APIC for SCI IRQ %u\n",
			    AcpiGbl_FADT.SciInterrupt);
		else {
			printf(
	"MADT: Forcing active-low polarity and level trigger for SCI\n");
			ioapic_set_polarity(ioapic, pin, INTR_POLARITY_LOW);
			ioapic_set_triggermode(ioapic, pin, INTR_TRIGGER_LEVEL);
		}
	}

	/* Third, we register all the I/O APIC's. */
	for (i = 0; i <= MAX_APIC_ID; i++)
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
SYSINIT(madt_register, SI_SUB_CPU - 1, SI_ORDER_SECOND, madt_register, NULL)

/*
 * Call the handler routine for each entry in the MADT table.
 */
static void
madt_walk_table(madt_entry_handler *handler, void *arg)
{
	ACPI_SUBTABLE_HEADER *entry;
	u_char *p, *end;

	end = (u_char *)(madt) + madt->Header.Length;
	for (p = (u_char *)(madt + 1); p < end; ) {
		entry = (ACPI_SUBTABLE_HEADER *)p;
		handler(entry, arg);
		p += entry->Length;
	}
}

static void
madt_probe_cpus_handler(ACPI_SUBTABLE_HEADER *entry, void *arg)
{
	ACPI_MADT_LOCAL_APIC *proc;
	struct lapic_info *la;

	switch (entry->Type) {
	case ACPI_MADT_TYPE_LOCAL_APIC:
		/*
		 * The MADT does not include a BSP flag, so we have to
		 * let the MP code figure out which CPU is the BSP on
		 * its own.
		 */
		proc = (ACPI_MADT_LOCAL_APIC *)entry;
		if (bootverbose)
			printf("MADT: Found CPU APIC ID %u ACPI ID %u: %s\n",
			    proc->Id, proc->ProcessorId,
			    (proc->LapicFlags & ACPI_MADT_ENABLED) ?
			    "enabled" : "disabled");
		if (!(proc->LapicFlags & ACPI_MADT_ENABLED))
			break;
		if (proc->Id > MAX_APIC_ID)
			panic("%s: CPU ID %u too high", __func__, proc->Id);
		la = &lapics[proc->Id];
		KASSERT(la->la_enabled == 0,
		    ("Duplicate local APIC ID %u", proc->Id));
		la->la_enabled = 1;
		la->la_acpi_id = proc->ProcessorId;
		lapic_create(proc->Id, 0);
		break;
	}
}


/*
 * Add an I/O APIC from an entry in the table.
 */
static void
madt_parse_apics(ACPI_SUBTABLE_HEADER *entry, void *arg __unused)
{
	ACPI_MADT_IO_APIC *apic;

	switch (entry->Type) {
	case ACPI_MADT_TYPE_IO_APIC:
		apic = (ACPI_MADT_IO_APIC *)entry;
		if (bootverbose)
			printf(
			    "MADT: Found IO APIC ID %u, Interrupt %u at %p\n",
			    apic->Id, apic->GlobalIrqBase,
			    (void *)(uintptr_t)apic->Address);
		if (apic->Id > MAX_APIC_ID)
			panic("%s: I/O APIC ID %u too high", __func__,
			    apic->Id);
		if (ioapics[apic->Id].io_apic != NULL)
			panic("%s: Double APIC ID %u", __func__, apic->Id);
		ioapics[apic->Id].io_apic = ioapic_create(apic->Address,
		    apic->Id, apic->GlobalIrqBase);
		ioapics[apic->Id].io_vector = apic->GlobalIrqBase;
		break;
	default:
		break;
	}
}

/*
 * Determine properties of an interrupt source.  Note that for ACPI these
 * functions are only used for ISA interrupts, so we assume ISA bus values
 * (Active Hi, Edge Triggered) for conforming values except for the ACPI
 * SCI for which we use Active Lo, Level Triggered.
 */
static enum intr_polarity
interrupt_polarity(UINT16 IntiFlags, UINT8 Source)
{

	switch (IntiFlags & ACPI_MADT_POLARITY_MASK) {
	case ACPI_MADT_POLARITY_CONFORMS:
		if (Source == AcpiGbl_FADT.SciInterrupt)
			return (INTR_POLARITY_LOW);
		else
			return (INTR_POLARITY_HIGH);
	case ACPI_MADT_POLARITY_ACTIVE_HIGH:
		return (INTR_POLARITY_HIGH);
	case ACPI_MADT_POLARITY_ACTIVE_LOW:
		return (INTR_POLARITY_LOW);
	default:
		panic("Bogus Interrupt Polarity");
	}
}

static enum intr_trigger
interrupt_trigger(UINT16 IntiFlags, UINT8 Source)
{

	switch (IntiFlags & ACPI_MADT_TRIGGER_MASK) {
	case ACPI_MADT_TRIGGER_CONFORMS:
		if (Source == AcpiGbl_FADT.SciInterrupt)
			return (INTR_TRIGGER_LEVEL);
		else
			return (INTR_TRIGGER_EDGE);
	case ACPI_MADT_TRIGGER_EDGE:
		return (INTR_TRIGGER_EDGE);
	case ACPI_MADT_TRIGGER_LEVEL:
		return (INTR_TRIGGER_LEVEL);
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
	int i;

	for (i = 0; i <= MAX_APIC_ID; i++) {
		if (!lapics[i].la_enabled)
			continue;
		if (lapics[i].la_acpi_id != acpi_id)
			continue;
		*apic_id = i;
		return (0);
	}
	return (ENOENT);
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
	for (i = 0; i <= MAX_APIC_ID; i++) {
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
madt_parse_interrupt_override(ACPI_MADT_INTERRUPT_OVERRIDE *intr)
{
	void *new_ioapic, *old_ioapic;
	u_int new_pin, old_pin;
	enum intr_trigger trig;
	enum intr_polarity pol;
	char buf[64];

	if (acpi_quirks & ACPI_Q_MADT_IRQ0 && intr->SourceIrq == 0 &&
	    intr->GlobalIrq == 2) {
		if (bootverbose)
			printf("MADT: Skipping timer override\n");
		return;
	}
	if (bootverbose)
		printf("MADT: Interrupt override: source %u, irq %u\n",
		    intr->SourceIrq, intr->GlobalIrq);
	KASSERT(intr->Bus == 0, ("bus for interrupt overrides must be zero"));
	if (madt_find_interrupt(intr->GlobalIrq, &new_ioapic, &new_pin) != 0) {
		printf("MADT: Could not find APIC for vector %u (IRQ %u)\n",
		    intr->GlobalIrq, intr->SourceIrq);
		return;
	}

	/*
	 * Lookup the appropriate trigger and polarity modes for this
	 * entry.
	 */
	trig = interrupt_trigger(intr->IntiFlags, intr->SourceIrq);
	pol = interrupt_polarity(intr->IntiFlags, intr->SourceIrq);
	
	/*
	 * If the SCI is identity mapped but has edge trigger and
	 * active-hi polarity or the force_sci_lo tunable is set,
	 * force it to use level/lo.
	 */
	if (intr->SourceIrq == AcpiGbl_FADT.SciInterrupt) {
		madt_found_sci_override = 1;
		if (getenv_string("hw.acpi.sci.trigger", buf, sizeof(buf))) {
			if (tolower(buf[0]) == 'e')
				trig = INTR_TRIGGER_EDGE;
			else if (tolower(buf[0]) == 'l')
				trig = INTR_TRIGGER_LEVEL;
			else
				panic(
				"Invalid trigger %s: must be 'edge' or 'level'",
				    buf);
			printf("MADT: Forcing SCI to %s trigger\n",
			    trig == INTR_TRIGGER_EDGE ? "edge" : "level");
		}
		if (getenv_string("hw.acpi.sci.polarity", buf, sizeof(buf))) {
			if (tolower(buf[0]) == 'h')
				pol = INTR_POLARITY_HIGH;
			else if (tolower(buf[0]) == 'l')
				pol = INTR_POLARITY_LOW;
			else
				panic(
				"Invalid polarity %s: must be 'high' or 'low'",
				    buf);
			printf("MADT: Forcing SCI to active %s polarity\n",
			    pol == INTR_POLARITY_HIGH ? "high" : "low");
		}
	}

	/* Remap the IRQ if it is mapped to a different interrupt vector. */
	if (intr->SourceIrq != intr->GlobalIrq) {
		/*
		 * If the SCI is remapped to a non-ISA global interrupt,
		 * then override the vector we use to setup and allocate
		 * the interrupt.
		 */
		if (intr->GlobalIrq > 15 &&
		    intr->SourceIrq == AcpiGbl_FADT.SciInterrupt)
			acpi_OverrideInterruptLevel(intr->GlobalIrq);
		else
			ioapic_remap_vector(new_ioapic, new_pin,
			    intr->SourceIrq);
		if (madt_find_interrupt(intr->SourceIrq, &old_ioapic,
		    &old_pin) != 0)
			printf("MADT: Could not find APIC for source IRQ %u\n",
			    intr->SourceIrq);
		else if (ioapic_get_vector(old_ioapic, old_pin) ==
		    intr->SourceIrq)
			ioapic_disable_pin(old_ioapic, old_pin);
	}

	/* Program the polarity and trigger mode. */
	ioapic_set_triggermode(new_ioapic, new_pin, trig);
	ioapic_set_polarity(new_ioapic, new_pin, pol);
}

/*
 * Parse an entry for an NMI routed to an IO APIC.
 */
static void
madt_parse_nmi(ACPI_MADT_NMI_SOURCE *nmi)
{
	void *ioapic;
	u_int pin;

	if (madt_find_interrupt(nmi->GlobalIrq, &ioapic, &pin) != 0) {
		printf("MADT: Could not find APIC for vector %u\n",
		    nmi->GlobalIrq);
		return;
	}

	ioapic_set_nmi(ioapic, pin);
	if (!(nmi->IntiFlags & ACPI_MADT_TRIGGER_CONFORMS))
		ioapic_set_triggermode(ioapic, pin,
		    interrupt_trigger(nmi->IntiFlags, 0));
	if (!(nmi->IntiFlags & ACPI_MADT_TRIGGER_CONFORMS))
		ioapic_set_polarity(ioapic, pin,
		    interrupt_polarity(nmi->IntiFlags, 0));
}

/*
 * Parse an entry for an NMI routed to a local APIC LVT pin.
 */
static void
madt_parse_local_nmi(ACPI_MADT_LOCAL_APIC_NMI *nmi)
{
	u_int apic_id, pin;

	if (nmi->ProcessorId == 0xff)
		apic_id = APIC_ID_ALL;
	else if (madt_find_cpu(nmi->ProcessorId, &apic_id) != 0) {
		if (bootverbose)
			printf("MADT: Ignoring local NMI routed to "
			    "ACPI CPU %u\n", nmi->ProcessorId);
		return;
	}
	if (nmi->Lint == 0)
		pin = LVT_LINT0;
	else
		pin = LVT_LINT1;
	lapic_set_lvt_mode(apic_id, pin, APIC_LVT_DM_NMI);
	if (!(nmi->IntiFlags & ACPI_MADT_TRIGGER_CONFORMS))
		lapic_set_lvt_triggermode(apic_id, pin,
		    interrupt_trigger(nmi->IntiFlags, 0));
	if (!(nmi->IntiFlags & ACPI_MADT_POLARITY_CONFORMS))
		lapic_set_lvt_polarity(apic_id, pin,
		    interrupt_polarity(nmi->IntiFlags, 0));
}

/*
 * Parse interrupt entries.
 */
static void
madt_parse_ints(ACPI_SUBTABLE_HEADER *entry, void *arg __unused)
{

	switch (entry->Type) {
	case ACPI_MADT_TYPE_INTERRUPT_OVERRIDE:
		madt_parse_interrupt_override(
			(ACPI_MADT_INTERRUPT_OVERRIDE *)entry);
		break;
	case ACPI_MADT_TYPE_NMI_SOURCE:
		madt_parse_nmi((ACPI_MADT_NMI_SOURCE *)entry);
		break;
	case ACPI_MADT_TYPE_LOCAL_APIC_NMI:
		madt_parse_local_nmi((ACPI_MADT_LOCAL_APIC_NMI *)entry);
		break;
	}
}

/*
 * Setup per-CPU ACPI IDs.
 */
static void
madt_set_ids(void *dummy)
{
	struct lapic_info *la;
	struct pcpu *pc;
	u_int i;

	if (madt == NULL)
		return;
	for (i = 0; i <= mp_maxid; i++) {
		if (CPU_ABSENT(i))
			continue;
		pc = pcpu_find(i);
		KASSERT(pc != NULL, ("no pcpu data for CPU %u", i));
		la = &lapics[pc->pc_apic_id];
		if (!la->la_enabled)
			panic("APIC: CPU with APIC ID %u is not enabled",
			    pc->pc_apic_id);
		pc->pc_acpi_id = la->la_acpi_id;
		if (bootverbose)
			printf("APIC: CPU %u has ACPI ID %u\n", i,
			    la->la_acpi_id);
	}
}
SYSINIT(madt_set_ids, SI_SUB_CPU, SI_ORDER_ANY, madt_set_ids, NULL)
