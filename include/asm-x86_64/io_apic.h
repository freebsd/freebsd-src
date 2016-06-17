#ifndef __ASM_IO_APIC_H
#define __ASM_IO_APIC_H

#include <linux/config.h>
#include <asm/types.h>

/*
 * Intel IO-APIC support for SMP and UP systems.
 *
 * Copyright (C) 1997, 1998, 1999, 2000 Ingo Molnar
 */

#ifdef CONFIG_X86_IO_APIC

#define APIC_MISMATCH_DEBUG

#define IO_APIC_BASE(idx) \
		((volatile int *)(__fix_to_virt(FIX_IO_APIC_BASE_0 + idx) \
		+ (mp_ioapics[idx].mpc_apicaddr & ~PAGE_MASK)))

/*
 * The structure of the IO-APIC:
 */
struct IO_APIC_reg_00 {
	__u32	__reserved_2	: 24,
		ID		:  4,
		__reserved_1	:  4;
} __attribute__ ((packed));

struct IO_APIC_reg_01 {
	__u32	version		:  8,
		__reserved_2	:  7,
		PRQ		:  1,
		entries		:  8,
		__reserved_1	:  8;
} __attribute__ ((packed));

struct IO_APIC_reg_02 {
	__u32	__reserved_2	: 24,
		arbitration	:  4,
		__reserved_1	:  4;
} __attribute__ ((packed));

/*
 * # of IO-APICs and # of IRQ routing registers
 */
extern int nr_ioapics;
extern int nr_ioapic_registers[MAX_IO_APICS];

enum ioapic_irq_destination_types {
	dest_Fixed = 0,
	dest_LowestPrio = 1,
	dest_SMI = 2,
	dest__reserved_1 = 3,
	dest_NMI = 4,
	dest_INIT = 5,
	dest__reserved_2 = 6,
	dest_ExtINT = 7
};

struct IO_APIC_route_entry {
	__u32	vector		:  8,
		delivery_mode	:  3,	/* 000: FIXED
					 * 001: lowest prio
					 * 111: ExtINT
					 */
		dest_mode	:  1,	/* 0: physical, 1: logical */
		delivery_status	:  1,
		polarity	:  1,
		irr		:  1,
		trigger		:  1,	/* 0: edge, 1: level */
		mask		:  1,	/* 0: enabled, 1: disabled */
		__reserved_2	: 15;

	union {		struct { __u32
					__reserved_1	: 24,
					physical_dest	:  4,
					__reserved_2	:  4;
			} physical;

			struct { __u32
					__reserved_1	: 24,
					logical_dest	:  8;
			} logical;
	} dest;

} __attribute__ ((packed));

/*
 * MP-BIOS irq configuration table structures:
 */

/* I/O APIC entries */
extern struct mpc_config_ioapic mp_ioapics[MAX_IO_APICS];

/* # of MP IRQ source entries */
extern int mp_irq_entries;

/* MP IRQ source entries */
extern struct mpc_config_intsrc mp_irqs[MAX_IRQ_SOURCES];

/* non-0 if default (table-less) MP configuration */
extern int mpc_default_type;

static inline unsigned int io_apic_read(unsigned int apic, unsigned int reg)
{
	*IO_APIC_BASE(apic) = reg;
	return *(IO_APIC_BASE(apic)+4);
}

static inline void io_apic_write(unsigned int apic, unsigned int reg, unsigned int value)
{
	*IO_APIC_BASE(apic) = reg;
	*(IO_APIC_BASE(apic)+4) = value;
}

/*
 * Re-write a value: to be used for read-modify-write
 * cycles where the read already set up the index register.
 */
static inline void io_apic_modify(unsigned int apic, unsigned int value)
{
	*(IO_APIC_BASE(apic)+4) = value;
}

/*
 * Synchronize the IO-APIC and the CPU by doing
 * a dummy read from the IO-APIC
 */
static inline void io_apic_sync(unsigned int apic)
{
	(void) *(IO_APIC_BASE(apic)+4);
}

/* 1 if "noapic" boot option passed */
extern int skip_ioapic_setup;

/*
 * If we use the IO-APIC for IRQ routing, disable automatic
 * assignment of PCI IRQ's.
 */
#define io_apic_assign_pci_irqs (mp_irq_entries && !skip_ioapic_setup)

extern int skip_ioapic_setup;	/* 1 for "noapic" */

static inline void disable_ioapic_setup(void)
{
	skip_ioapic_setup = 1;
}

static inline int ioapic_setup_disabled(void)
{
	return skip_ioapic_setup;
}

#else	/* !CONFIG_X86_IO_APIC */

#define io_apic_assign_pci_irqs 0

static inline void disable_ioapic_setup(void)
{ }

#endif	/* !CONFIG_X86_IO_APIC */

extern int io_apic_get_unique_id (int ioapic, int apic_id);
extern int io_apic_get_version (int ioapic);
extern int io_apic_get_redir_entries (int ioapic);
extern int io_apic_set_pci_routing (int ioapic, int pin, int irq, int, int);

#endif
