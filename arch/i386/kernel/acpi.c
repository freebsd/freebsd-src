/*
 *  acpi.c - Architecture-Specific Low-Level ACPI Support
 *
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *  Copyright (C) 2001 Jun Nakajima <jun.nakajima@intel.com>
 *  Copyright (C) 2001 Patrick Mochel <mochel@osdl.org>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/bootmem.h>
#include <linux/irq.h>
#include <linux/acpi.h>
#include <asm/mpspec.h>
#include <asm/io.h>
#include <asm/apic.h>
#include <asm/apicdef.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/io_apic.h>
#include <asm/acpi.h>
#include <asm/save_state.h>
#include <asm/smpboot.h>


#define PREFIX			"ACPI: "

int acpi_lapic;
int acpi_ioapic;
int acpi_strict;

acpi_interrupt_flags acpi_sci_flags __initdata;
int acpi_sci_override_gsi __initdata;
/* --------------------------------------------------------------------------
                              Boot-time Configuration
   -------------------------------------------------------------------------- */

int acpi_noirq __initdata = 0;  /* skip ACPI IRQ initialization */
int acpi_ht __initdata = 1;     /* enable HT */

enum acpi_irq_model_id		acpi_irq_model;


/*
 * Temporarily use the virtual area starting from FIX_IO_APIC_BASE_END,
 * to map the target physical address. The problem is that set_fixmap()
 * provides a single page, and it is possible that the page is not
 * sufficient.
 * By using this area, we can map up to MAX_IO_APICS pages temporarily,
 * i.e. until the next __va_range() call.
 *
 * Important Safety Note:  The fixed I/O APIC page numbers are *subtracted*
 * from the fixed base.  That's why we start at FIX_IO_APIC_BASE_END and
 * count idx down while incrementing the phys address.
 */
char *__acpi_map_table(unsigned long phys, unsigned long size)
{
	unsigned long base, offset, mapped_size;
	int idx;

	if (phys + size < 8*1024*1024) 
		return __va(phys); 

	offset = phys & (PAGE_SIZE - 1);
	mapped_size = PAGE_SIZE - offset;
	set_fixmap(FIX_ACPI_END, phys);
	base = fix_to_virt(FIX_ACPI_END);

	/*
	 * Most cases can be covered by the below.
	 */
	idx = FIX_ACPI_END;
	while (mapped_size < size) {
		if (--idx < FIX_ACPI_BEGIN)
			return 0;	/* cannot handle this */
		phys += PAGE_SIZE;
		set_fixmap(idx, phys);
		mapped_size += PAGE_SIZE;
	}

	return ((unsigned char *) base + offset);
}


#ifdef CONFIG_X86_LOCAL_APIC

static u64 acpi_lapic_addr __initdata = APIC_DEFAULT_PHYS_BASE;


static int __init
acpi_parse_madt (
	unsigned long		phys_addr,
	unsigned long		size)
{
	struct acpi_table_madt	*madt = NULL;

	if (!phys_addr || !size)
		return -EINVAL;

	madt = (struct acpi_table_madt *) __acpi_map_table(phys_addr, size);
	if (!madt) {
		printk(KERN_WARNING PREFIX "Unable to map MADT\n");
		return -ENODEV;
	}

	if (madt->lapic_address)
		acpi_lapic_addr = (u64) madt->lapic_address;

	printk(KERN_INFO PREFIX "Local APIC address 0x%08x\n",
		madt->lapic_address);

	detect_clustered_apic(madt->header.oem_id, madt->header.oem_table_id);

	return 0;
}


static int __init
acpi_parse_lapic (
	acpi_table_entry_header *header)
{
	struct acpi_table_lapic	*processor = NULL;

	processor = (struct acpi_table_lapic*) header;
	if (!processor)
		return -EINVAL;

	acpi_table_print_madt_entry(header);

	mp_register_lapic (
		processor->id,					   /* APIC ID */
		processor->flags.enabled);			  /* Enabled? */

	return 0;
}


static int __init
acpi_parse_lapic_addr_ovr (
	acpi_table_entry_header *header)
{
	struct acpi_table_lapic_addr_ovr *lapic_addr_ovr = NULL;

	lapic_addr_ovr = (struct acpi_table_lapic_addr_ovr*) header;
	if (!lapic_addr_ovr)
		return -EINVAL;

	acpi_lapic_addr = lapic_addr_ovr->address;

	return 0;
}

static int __init
acpi_parse_lapic_nmi (
	acpi_table_entry_header *header)
{
	struct acpi_table_lapic_nmi *lapic_nmi = NULL;

	lapic_nmi = (struct acpi_table_lapic_nmi*) header;
	if (!lapic_nmi)
		return -EINVAL;

	acpi_table_print_madt_entry(header);

	if (lapic_nmi->lint != 1)
		printk(KERN_WARNING PREFIX "NMI not connected to LINT 1!\n");

	return 0;
}

#endif /*CONFIG_X86_LOCAL_APIC*/

#if defined(CONFIG_X86_IO_APIC) && defined(CONFIG_ACPI_INTERPRETER)

static int __init
acpi_parse_ioapic (
	acpi_table_entry_header *header)
{
	struct acpi_table_ioapic *ioapic = NULL;

	ioapic = (struct acpi_table_ioapic*) header;
	if (!ioapic)
		return -EINVAL;
 
	acpi_table_print_madt_entry(header);

	mp_register_ioapic (
		ioapic->id,
		ioapic->address,
		ioapic->global_irq_base);
 
	return 0;
}

/*
 * Parse Interrupt Source Override for the ACPI SCI
 */
static void
acpi_sci_ioapic_setup(u32 gsi, u16 polarity, u16 trigger)
{
	if (trigger == 0)	/* compatible SCI trigger is level */
		trigger = 3;

	if (polarity == 0)	/* compatible SCI polarity is low */
		polarity = 3;

	/* Command-line over-ride via acpi_sci= */
	if (acpi_sci_flags.trigger)
		trigger = acpi_sci_flags.trigger;

	if (acpi_sci_flags.polarity)
		polarity = acpi_sci_flags.polarity;

	/*
 	 * mp_config_acpi_legacy_irqs() already setup IRQs < 16
	 * If GSI is < 16, this will update its flags,
	 * else it will create a new mp_irqs[] entry.
	 */
	mp_override_legacy_irq(gsi, polarity, trigger, gsi);

	/*
	 * stash over-ride to indicate we've been here
	 * and for later update of acpi_fadt
	 */
	acpi_sci_override_gsi = gsi;
	return;
}

static int __init
acpi_parse_fadt(unsigned long phys, unsigned long size)
{
        struct fadt_descriptor_rev2 *fadt =0;

        fadt = (struct fadt_descriptor_rev2*) __acpi_map_table(phys,size);
        if (!fadt) {
                printk(KERN_WARNING PREFIX "Unable to map FADT\n");
                return 0;
        }

#ifdef  CONFIG_ACPI_INTERPRETER
        /* initialize sci_int early for INT_SRC_OVR MADT parsing */
        acpi_fadt.sci_int = fadt->sci_int;
#endif

        return 0;
}


static int __init
acpi_parse_int_src_ovr (
	acpi_table_entry_header *header)
{
	struct acpi_table_int_src_ovr *intsrc = NULL;

	intsrc = (struct acpi_table_int_src_ovr*) header;
	if (!intsrc)
		return -EINVAL;

	acpi_table_print_madt_entry(header);

	if (intsrc->bus_irq == acpi_fadt.sci_int) {
		acpi_sci_ioapic_setup(intsrc->global_irq,
			intsrc->flags.polarity, intsrc->flags.trigger);
		return 0;
	}

	mp_override_legacy_irq (
		intsrc->bus_irq,
		intsrc->flags.polarity,
		intsrc->flags.trigger,
		intsrc->global_irq);

	return 0;
}


static int __init
acpi_parse_nmi_src (
	acpi_table_entry_header *header)
{
	struct acpi_table_nmi_src *nmi_src = NULL;

	nmi_src = (struct acpi_table_nmi_src*) header;
	if (!nmi_src)
		return -EINVAL;

	acpi_table_print_madt_entry(header);

	/* TBD: Support nimsrc entries? */

	return 0;
}

#endif /*CONFIG_X86_IO_APIC && CONFIG_ACPI_INTERPRETER*/


static unsigned long __init
acpi_scan_rsdp (
	unsigned long		start,
	unsigned long		length)
{
	unsigned long		offset = 0;
	unsigned long		sig_len = sizeof("RSD PTR ") - 1;

	/*
	 * Scan all 16-byte boundaries of the physical memory region for the
	 * RSDP signature.
	 */
	for (offset = 0; offset < length; offset += 16) {
		if (strncmp((char *) (start + offset), "RSD PTR ", sig_len))
			continue;
		return (start + offset);
	}

	return 0;
}


unsigned long __init
acpi_find_rsdp (void)
{
	unsigned long		rsdp_phys = 0;

	/*
	 * Scan memory looking for the RSDP signature. First search EBDA (low
	 * memory) paragraphs and then search upper memory (E0000-FFFFF).
	 */
	rsdp_phys = acpi_scan_rsdp (0, 0x400);
	if (!rsdp_phys)
		rsdp_phys = acpi_scan_rsdp (0xE0000, 0xFFFFF);

	return rsdp_phys;
}


/*
 * acpi_boot_init()
 *  called from setup_arch(), always.
 *	1. maps ACPI tables for later use
 *	2. enumerates lapics
 *	3. enumerates io-apics
 *
 * side effects:
 * 	acpi_lapic = 1 if LAPIC found
 *	acpi_ioapic = 1 if IOAPIC found
 *	if (acpi_lapic && acpi_ioapic) smp_found_config = 1;
 *	if acpi_blacklisted() disable_acpi()
 *	acpi_irq_model=...
 *	...
 *
 * return value: (currently ignored)
 *	0: success
 *	!0: failure
 */
int __init
acpi_boot_init (void)
{
	int			result = 0;

	if (acpi_disabled && !acpi_ht)
		return(1);

	/*
	 * The default interrupt routing model is PIC (8259).  This gets
	 * overriden if IOAPICs are enumerated (below).
	 */
	acpi_irq_model = ACPI_IRQ_MODEL_PIC;

	/* 
	 * Initialize the ACPI boot-time table parser.
	 */
	result = acpi_table_init();
	if (result) {
		disable_acpi();
		return result;
	}

	result = acpi_blacklisted();
	if (result) {
		printk(KERN_NOTICE PREFIX "BIOS listed in blacklist, disabling ACPI support\n");
		disable_acpi();
		return result;
	}

#ifdef CONFIG_X86_LOCAL_APIC

	/* 
	 * MADT
	 * ----
	 * Parse the Multiple APIC Description Table (MADT), if exists.
	 * Note that this table provides platform SMP configuration 
	 * information -- the successor to MPS tables.
	 */

	result = acpi_table_parse(ACPI_APIC, acpi_parse_madt);
	if (!result) {
		return 0;
	}
	else if (result < 0) {
		printk(KERN_ERR PREFIX "Error parsing MADT\n");
		return result;
	}
	else if (result > 1) 
		printk(KERN_WARNING PREFIX "Multiple MADT tables exist\n");

	/* 
	 * Local APIC
	 * ----------
	 * Note that the LAPIC address is obtained from the MADT (32-bit value)
	 * and (optionally) overriden by a LAPIC_ADDR_OVR entry (64-bit value).
	 */

	result = acpi_table_parse_madt(ACPI_MADT_LAPIC_ADDR_OVR, acpi_parse_lapic_addr_ovr);
	if (result < 0) {
		printk(KERN_ERR PREFIX "Error parsing LAPIC address override entry\n");
		return result;
	}

	mp_register_lapic_address(acpi_lapic_addr);

	result = acpi_table_parse_madt(ACPI_MADT_LAPIC, acpi_parse_lapic);
	if (!result) { 
		printk(KERN_ERR PREFIX "No LAPIC entries present\n");
		/* TBD: Cleanup to allow fallback to MPS */
		return -ENODEV;
	}
	else if (result < 0) {
		printk(KERN_ERR PREFIX "Error parsing LAPIC entry\n");
		/* TBD: Cleanup to allow fallback to MPS */
		return result;
	}

	result = acpi_table_parse_madt(ACPI_MADT_LAPIC_NMI, acpi_parse_lapic_nmi);
	if (result < 0) {
		printk(KERN_ERR PREFIX "Error parsing LAPIC NMI entry\n");
		/* TBD: Cleanup to allow fallback to MPS */
		return result;
	}

	acpi_lapic = 1;

#endif /*CONFIG_X86_LOCAL_APIC*/

#if defined(CONFIG_X86_IO_APIC) && defined(CONFIG_ACPI_INTERPRETER)

	/* 
	 * I/O APIC 
	 * --------
	 */

	/*
	 * ACPI interpreter is required to complete interrupt setup,
	 * so if it is off, don't enumerate the io-apics with ACPI.
	 * If MPS is present, it will handle them,
	 * otherwise the system will stay in PIC mode
	 */
	if (acpi_disabled || acpi_noirq) {
		return 1;
	}

	/*
	 * if "noapic" boot option, don't look for IO-APICs
	 */
	if (ioapic_setup_disabled()) {
		printk(KERN_INFO PREFIX "Skipping IOAPIC probe "
			"due to 'noapic' option.\n");
		return 1;
        }


	result = acpi_table_parse_madt(ACPI_MADT_IOAPIC, acpi_parse_ioapic);
	if (!result) { 
		printk(KERN_ERR PREFIX "No IOAPIC entries present\n");
		return -ENODEV;
	}
	else if (result < 0) {
		printk(KERN_ERR PREFIX "Error parsing IOAPIC entry\n");
		return result;
	}

	/* Build a default routing table for legacy (ISA) interrupts. */
	mp_config_acpi_legacy_irqs();

	/* Record sci_int for use when looking for MADT sci_int override */
	acpi_table_parse(ACPI_FADT, acpi_parse_fadt);

	result = acpi_table_parse_madt(ACPI_MADT_INT_SRC_OVR, acpi_parse_int_src_ovr);
	if (result < 0) {
		printk(KERN_ERR PREFIX "Error parsing interrupt source overrides entry\n");
		/* TBD: Cleanup to allow fallback to MPS */
		return result;
	}

	/*
	 * If BIOS did not supply an INT_SRC_OVR for the SCI
	 * pretend we got one so we can set the SCI flags.
	 */
	if (!acpi_sci_override_gsi)
		acpi_sci_ioapic_setup(acpi_fadt.sci_int, 0, 0);

	result = acpi_table_parse_madt(ACPI_MADT_NMI_SRC, acpi_parse_nmi_src);
	if (result < 0) {
		printk(KERN_ERR PREFIX "Error parsing NMI SRC entry\n");
		/* TBD: Cleanup to allow fallback to MPS */
		return result;
	}

	acpi_irq_model = ACPI_IRQ_MODEL_IOAPIC;

	acpi_irq_balance_set(NULL);

	acpi_ioapic = 1;

	if (acpi_lapic && acpi_ioapic)
		smp_found_config = 1;

#endif /*CONFIG_X86_IO_APIC && CONFIG_ACPI_INTERPRETER*/

	return 0;
}


#ifdef	CONFIG_ACPI_BUS
/*
 * acpi_pic_sci_set_trigger()
 *
 * use ELCR to set PIC-mode trigger type for SCI
 *
 * If a PIC-mode SCI is not recognized or gives spurious IRQ7's
 * it may require Edge Trigger -- use "acpi_sci=edge"
 *
 * Port 0x4d0-4d1 are ECLR1 and ECLR2, the Edge/Level Control Registers
 * for the 8259 PIC.  bit[n] = 1 means irq[n] is Level, otherwise Edge.
 * ECLR1 is IRQ's 0-7 (IRQ 0, 1, 2 must be 0)
 * ECLR2 is IRQ's 8-15 (IRQ 8, 13 must be 0)
 */

void __init
acpi_pic_sci_set_trigger(unsigned int irq, u16 trigger)
{
	unsigned char mask = 1 << (irq & 7);
	unsigned int port = 0x4d0 + (irq >> 3);
	unsigned char val = inb(port);


	printk(PREFIX "IRQ%d SCI:", irq);
	if (!(val & mask)) {
		printk(" Edge");

		if (trigger == 3) {
			printk(" set to Level");
			outb(val | mask, port);
		}
	} else {
		printk(" Level");

		if (trigger == 1) {
			printk(" set to Edge");
			outb(val & ~mask, port);
		}
	}
	printk(" Trigger.\n");
}

#endif /* CONFIG_ACPI_BUS */



#ifndef __HAVE_ARCH_CMPXCHG
#warning ACPI uses CMPXCHG, i486 and later hardware
#endif

/* --------------------------------------------------------------------------
                              Low-Level Sleep Support
   -------------------------------------------------------------------------- */

#ifdef CONFIG_ACPI_SLEEP

#define DEBUG

#ifdef DEBUG
#include <linux/serial.h>
#endif

/* address in low memory of the wakeup routine. */
unsigned long acpi_wakeup_address = 0;

/* new page directory that we will be using */
static pmd_t *pmd;

/* saved page directory */
static pmd_t saved_pmd;

/* page which we'll use for the new page directory */
static pte_t *ptep;

extern unsigned long FASTCALL(acpi_copy_wakeup_routine(unsigned long));

/*
 * acpi_create_identity_pmd
 *
 * Create a new, identity mapped pmd.
 *
 * Do this by creating new page directory, and marking all the pages as R/W
 * Then set it as the new Page Middle Directory.
 * And, of course, flush the TLB so it takes effect.
 *
 * We save the address of the old one, for later restoration.
 */
static void acpi_create_identity_pmd (void)
{
	pgd_t *pgd;
	int i;

	ptep = (pte_t*)__get_free_page(GFP_KERNEL);

	/* fill page with low mapping */
	for (i = 0; i < PTRS_PER_PTE; i++)
		set_pte(ptep + i, mk_pte_phys(i << PAGE_SHIFT, PAGE_SHARED));

	pgd = pgd_offset(current->active_mm, 0);
	pmd = pmd_alloc(current->mm,pgd, 0);

	/* save the old pmd */
	saved_pmd = *pmd;

	/* set the new one */
	set_pmd(pmd, __pmd(_PAGE_TABLE + __pa(ptep)));

	/* flush the TLB */
	local_flush_tlb();
}

/*
 * acpi_restore_pmd
 *
 * Restore the old pmd saved by acpi_create_identity_pmd and
 * free the page that said function alloc'd
 */
static void acpi_restore_pmd (void)
{
	set_pmd(pmd, saved_pmd);
	local_flush_tlb();
	free_page((unsigned long)ptep);
}

/**
 * acpi_save_state_mem - save kernel state
 *
 * Create an identity mapped page table and copy the wakeup routine to
 * low memory.
 */
int acpi_save_state_mem (void)
{
	acpi_create_identity_pmd();
	acpi_copy_wakeup_routine(acpi_wakeup_address);

	return 0;
}

/**
 * acpi_save_state_disk - save kernel state to disk
 *
 */
int acpi_save_state_disk (void)
{
	return 1;
}

/*
 * acpi_restore_state
 */
void acpi_restore_state_mem (void)
{
	acpi_restore_pmd();
}

/**
 * acpi_reserve_bootmem - do _very_ early ACPI initialisation
 *
 * We allocate a page in low memory for the wakeup
 * routine for when we come back from a sleep state. The
 * runtime allocator allows specification of <16M pages, but not
 * <1M pages.
 */
void __init acpi_reserve_bootmem(void)
{
	acpi_wakeup_address = (unsigned long)alloc_bootmem_low(PAGE_SIZE);
	if (!acpi_wakeup_address)
		printk(KERN_ERR "ACPI: Cannot allocate lowmem, S3 disabled.\n");
}

void do_suspend_lowlevel_s4bios(int resume)
{
	if (!resume) {
		save_processor_context();
		acpi_save_register_state((unsigned long)&&acpi_sleep_done);
		acpi_enter_sleep_state_s4bios();
		return;
	}
acpi_sleep_done:
	restore_processor_context();
}


#endif /*CONFIG_ACPI_SLEEP*/

