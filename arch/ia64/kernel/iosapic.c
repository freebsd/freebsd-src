/*
 * I/O SAPIC support.
 *
 * Copyright (C) 1999 Intel Corp.
 * Copyright (C) 1999 Asit Mallick <asit.k.mallick@intel.com>
 * Copyright (C) 2000-2002 J.I. Lee <jung-ik.lee@intel.com>
 * Copyright (C) 1999-2000, 2002-2003 Hewlett-Packard Co.
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 * Copyright (C) 1999 VA Linux Systems
 * Copyright (C) 1999,2000 Walt Drummond <drummond@valinux.com>
 *
 * 00/04/19	D. Mosberger	Rewritten to mirror more closely the x86 I/O APIC code.
 *				In particular, we now have separate handlers for edge
 *				and level triggered interrupts.
 * 00/10/27	Asit Mallick, Goutham Rao <goutham.rao@intel.com> IRQ vector allocation
 *				PCI to vector mapping, shared PCI interrupts.
 * 00/10/27	D. Mosberger	Document things a bit more to make them more understandable.
 *				Clean up much of the old IOSAPIC cruft.
 * 01/07/27	J.I. Lee	PCI irq routing, Platform/Legacy interrupts and fixes for
 *				ACPI S5(SoftOff) support.
 * 02/01/23	J.I. Lee	iosapic pgm fixes for PCI irq routing from _PRT
 * 02/01/07     E. Focht        <efocht@ess.nec.de> Redirectable interrupt vectors in
 *                              iosapic_set_affinity(), initializations for
 *                              /proc/irq/#/smp_affinity
 * 02/04/02	P. Diefenbaugh	Cleaned up ACPI PCI IRQ routing.
 * 02/04/18	J.I. Lee	bug fix in iosapic_init_pci_irq
 * 02/04/30	J.I. Lee	bug fix in find_iosapic to fix ACPI PCI IRQ to IOSAPIC mapping error
 * 02/07/29	T. Kochi	Allocate interrupt vectors dynamically
 * 02/08/04	T. Kochi	Cleaned up terminology (irq, global system interrupt, vector, etc.)
 * 02/08/13	B. Helgaas	Support PCI segments
 * 03/02/19	B. Helgaas	Make pcat_compat system-wide, not per-IOSAPIC.
 *				Remove iosapic_address & gsi_base from external interfaces.
 *				Rationalize __init/__devinit attributes.
 */
/*
 * Here is what the interrupt logic between a PCI device and the CPU looks like:
 *
 * (1) A PCI device raises one of the four interrupt pins (INTA, INTB, INTC, INTD).  The
 *     device is uniquely identified by its segment--, bus--, and slot-number (the function
 *     number does not matter here because all functions share the same interrupt
 *     lines).
 *
 * (2) The motherboard routes the interrupt line to a pin on a IOSAPIC controller.
 *     Multiple interrupt lines may have to share the same IOSAPIC pin (if they're level
 *     triggered and use the same polarity).  Each interrupt line has a unique Global
 *     System Interrupt (GSI) number which can be calculated as the sum of the controller's
 *     base GSI number and the IOSAPIC pin number to which the line connects.
 *
 * (3) The IOSAPIC uses internal routing table entries (RTEs) to map the IOSAPIC pin
 *     to the IA-64 interrupt vector.  This interrupt vector is then sent to the CPU.
 *
 * (4) The kernel recognizes an interrupt as an IRQ.  The IRQ interface
 *     is an architecture-independent interrupt handling mechanism in
 *     Linux.  An IRQ is a number, so we need a mapping between IRQ
 *     numbers and IA-64 vectors.  The platform_irq_to_vector(irq) and
 *     platform_local_vector_to_irq(vector) APIs can define platform-
 *     specific mappings.
 *
 * To sum up, there are three levels of mappings involved:
 *
 *	PCI pin -> global system interrupt (GSI) -> IA-64 vector <-> IRQ
 *
 * Note: The term "IRQ" is loosely used everywhere in the Linux kernel
 * to describe interrupts.  In this module, "IRQ" refers only to Linux
 * IRQ numbers ("isa_irq" is an exception to this rule).
 */
#include <linux/config.h>

#include <linux/acpi.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/string.h>

#include <asm/delay.h>
#include <asm/hw_irq.h>
#include <asm/io.h>
#include <asm/iosapic.h>
#include <asm/machvec.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/system.h>


#undef DEBUG_INTERRUPT_ROUTING

#ifdef DEBUG_INTERRUPT_ROUTING
#define DBG(fmt...)	printk(fmt)
#else
#define DBG(fmt...)
#endif

static spinlock_t iosapic_lock = SPIN_LOCK_UNLOCKED;

/* PCI pin to GSI routing information.  This info typically comes from ACPI. */

static struct {
	int num_routes;
	struct pci_vector_struct *route;
} pci_irq;

/* These tables map IA-64 vectors to the IOSAPIC pin that generates this vector. */

static struct iosapic_intr_info {
	char		*addr;		/* base address of IOSAPIC */
	unsigned int	gsi_base;	/* first GSI assigned to this IOSAPIC */
	char		rte_index;	/* IOSAPIC RTE index (-1 => not an IOSAPIC interrupt) */
	unsigned char	dmode	: 3;	/* delivery mode (see iosapic.h) */
	unsigned char 	polarity: 1;	/* interrupt polarity (see iosapic.h) */
	unsigned char	trigger	: 1;	/* trigger mode (see iosapic.h) */
} iosapic_intr_info[IA64_NUM_VECTORS];

static struct iosapic {
	char		*addr;		/* base address of IOSAPIC */
	unsigned int 	gsi_base;	/* first GSI assigned to this IOSAPIC */
	unsigned short 	num_rte;	/* number of RTE in this IOSAPIC */
} iosapic_lists[256];

static int num_iosapic;

static unsigned char pcat_compat __initdata;	/* 8259 compatibility flag */


/*
 * Find an IOSAPIC associated with a GSI
 */
static inline int
find_iosapic (unsigned int gsi)
{
	int i;

	for (i = 0; i < num_iosapic; i++) {
		if ((unsigned) (gsi - iosapic_lists[i].gsi_base) < iosapic_lists[i].num_rte)
			return i;
	}

	return -1;
}

/*
 * Translate GSI number to the corresponding IA-64 interrupt vector.  If no
 * entry exists, return -1.
 */
int
gsi_to_vector (unsigned int gsi)
{
	int vector;

	for (vector = 0; vector < IA64_NUM_VECTORS; vector++)
		if (iosapic_intr_info[vector].gsi_base + iosapic_intr_info[vector].rte_index == gsi)
			return vector;
	return -1;
}

static void
set_rte (unsigned int vector, unsigned int dest)
{
	unsigned long pol, trigger, dmode;
	u32 low32, high32;
	char *addr;
	int rte_index;
	char redir;

	DBG(KERN_DEBUG "IOSAPIC: routing vector %d to 0x%x\n", vector, dest);

	rte_index = iosapic_intr_info[vector].rte_index;
	if (rte_index < 0)
		return;		/* not an IOSAPIC interrupt */

	addr    = iosapic_intr_info[vector].addr;
	pol     = iosapic_intr_info[vector].polarity;
	trigger = iosapic_intr_info[vector].trigger;
	dmode   = iosapic_intr_info[vector].dmode;

	redir = (dmode == IOSAPIC_LOWEST_PRIORITY) ? 1 : 0;
#ifdef CONFIG_SMP
	{
		unsigned int irq;

		for (irq = 0; irq < NR_IRQS; ++irq)
			if (irq_to_vector(irq) == vector) {
				set_irq_affinity_info(irq, (int)(dest & 0xffff), redir);
				break;
			}
	}
#endif

	low32 = ((pol << IOSAPIC_POLARITY_SHIFT) |
		 (trigger << IOSAPIC_TRIGGER_SHIFT) |
		 (dmode << IOSAPIC_DELIVERY_SHIFT) |
		 vector);

	/* dest contains both id and eid */
	high32 = (dest << IOSAPIC_DEST_SHIFT);

	writel(IOSAPIC_RTE_HIGH(rte_index), addr + IOSAPIC_REG_SELECT);
	writel(high32, addr + IOSAPIC_WINDOW);
	writel(IOSAPIC_RTE_LOW(rte_index), addr + IOSAPIC_REG_SELECT);
	writel(low32, addr + IOSAPIC_WINDOW);
}

static void
nop (unsigned int vector)
{
	/* do nothing... */
}

static void
mask_irq (unsigned int irq)
{
	unsigned long flags;
	char *addr;
	u32 low32;
	int rte_index;
	ia64_vector vec = irq_to_vector(irq);

	addr = iosapic_intr_info[vec].addr;
	rte_index = iosapic_intr_info[vec].rte_index;

	if (rte_index < 0)
		return;			/* not an IOSAPIC interrupt! */

	spin_lock_irqsave(&iosapic_lock, flags);
	{
		writel(IOSAPIC_RTE_LOW(rte_index), addr + IOSAPIC_REG_SELECT);
		low32 = readl(addr + IOSAPIC_WINDOW);

		low32 |= (1 << IOSAPIC_MASK_SHIFT);    /* set only the mask bit */
		writel(low32, addr + IOSAPIC_WINDOW);
	}
	spin_unlock_irqrestore(&iosapic_lock, flags);
}

static void
unmask_irq (unsigned int irq)
{
	unsigned long flags;
	char *addr;
	u32 low32;
	int rte_index;
	ia64_vector vec = irq_to_vector(irq);

	addr = iosapic_intr_info[vec].addr;
	rte_index = iosapic_intr_info[vec].rte_index;
	if (rte_index < 0)
		return;			/* not an IOSAPIC interrupt! */

	spin_lock_irqsave(&iosapic_lock, flags);
	{
		writel(IOSAPIC_RTE_LOW(rte_index), addr + IOSAPIC_REG_SELECT);
		low32 = readl(addr + IOSAPIC_WINDOW);

		low32 &= ~(1 << IOSAPIC_MASK_SHIFT);    /* clear only the mask bit */
		writel(low32, addr + IOSAPIC_WINDOW);
	}
	spin_unlock_irqrestore(&iosapic_lock, flags);
}


static void
iosapic_set_affinity (unsigned int irq, unsigned long mask)
{
#ifdef CONFIG_SMP
	unsigned long flags;
	u32 high32, low32;
	int dest, rte_index;
	char *addr;
	int redir = (irq & IA64_IRQ_REDIRECTED) ? 1 : 0;
	ia64_vector vec;

	irq &= (~IA64_IRQ_REDIRECTED);
	vec = irq_to_vector(irq);

	mask &= (1UL << smp_num_cpus) - 1;

	if (!mask || vec >= IA64_NUM_VECTORS)
		return;

	dest = cpu_physical_id(ffz(~mask));

	rte_index = iosapic_intr_info[vec].rte_index;
	addr = iosapic_intr_info[vec].addr;

	if (rte_index < 0)
		return;			/* not an IOSAPIC interrupt */

	set_irq_affinity_info(irq, dest, redir);

	/* dest contains both id and eid */
	high32 = dest << IOSAPIC_DEST_SHIFT;

	spin_lock_irqsave(&iosapic_lock, flags);
	{
		/* get current delivery mode by reading the low32 */
		writel(IOSAPIC_RTE_LOW(rte_index), addr + IOSAPIC_REG_SELECT);
		low32 = readl(addr + IOSAPIC_WINDOW);

		low32 &= ~(7 << IOSAPIC_DELIVERY_SHIFT);
		if (redir)
		        /* change delivery mode to lowest priority */
			low32 |= (IOSAPIC_LOWEST_PRIORITY << IOSAPIC_DELIVERY_SHIFT);
		else
		        /* change delivery mode to fixed */
			low32 |= (IOSAPIC_FIXED << IOSAPIC_DELIVERY_SHIFT);

		writel(IOSAPIC_RTE_HIGH(rte_index), addr + IOSAPIC_REG_SELECT);
		writel(high32, addr + IOSAPIC_WINDOW);
		writel(IOSAPIC_RTE_LOW(rte_index), addr + IOSAPIC_REG_SELECT);
		writel(low32, addr + IOSAPIC_WINDOW);
	}
	spin_unlock_irqrestore(&iosapic_lock, flags);
#endif
}

/*
 * Handlers for level-triggered interrupts.
 */

static unsigned int
iosapic_startup_level_irq (unsigned int irq)
{
	unmask_irq(irq);
	return 0;
}

static void
iosapic_end_level_irq (unsigned int irq)
{
	ia64_vector vec = irq_to_vector(irq);

	writel(vec, iosapic_intr_info[vec].addr + IOSAPIC_EOI);
}

#define iosapic_shutdown_level_irq	mask_irq
#define iosapic_enable_level_irq	unmask_irq
#define iosapic_disable_level_irq	mask_irq
#define iosapic_ack_level_irq		nop

struct hw_interrupt_type irq_type_iosapic_level = {
	.typename =	"IO-SAPIC-level",
	.startup =	iosapic_startup_level_irq,
	.shutdown =	iosapic_shutdown_level_irq,
	.enable =	iosapic_enable_level_irq,
	.disable =	iosapic_disable_level_irq,
	.ack =		iosapic_ack_level_irq,
	.end =		iosapic_end_level_irq,
	.set_affinity =	iosapic_set_affinity
};

/*
 * Handlers for edge-triggered interrupts.
 */

static unsigned int
iosapic_startup_edge_irq (unsigned int irq)
{
	unmask_irq(irq);
	/*
	 * IOSAPIC simply drops interrupts pended while the
	 * corresponding pin was masked, so we can't know if an
	 * interrupt is pending already.  Let's hope not...
	 */
	return 0;
}

static void
iosapic_ack_edge_irq (unsigned int irq)
{
	irq_desc_t *idesc = irq_desc(irq);
	/*
	 * Once we have recorded IRQ_PENDING already, we can mask the
	 * interrupt for real. This prevents IRQ storms from unhandled
	 * devices.
	 */
	if ((idesc->status & (IRQ_PENDING|IRQ_DISABLED)) == (IRQ_PENDING|IRQ_DISABLED))
		mask_irq(irq);
}

#define iosapic_enable_edge_irq		unmask_irq
#define iosapic_disable_edge_irq	nop
#define iosapic_end_edge_irq		nop

struct hw_interrupt_type irq_type_iosapic_edge = {
	.typename =	"IO-SAPIC-edge",
	.startup =	iosapic_startup_edge_irq,
	.shutdown =	iosapic_disable_edge_irq,
	.enable =	iosapic_enable_edge_irq,
	.disable =	iosapic_disable_edge_irq,
	.ack =		iosapic_ack_edge_irq,
	.end =		iosapic_end_edge_irq,
	.set_affinity =	iosapic_set_affinity
};

unsigned int
iosapic_version (char *addr)
{
	/*
	 * IOSAPIC Version Register return 32 bit structure like:
	 * {
	 *	unsigned int version   : 8;
	 *	unsigned int reserved1 : 8;
	 *	unsigned int max_redir : 8;
	 *	unsigned int reserved2 : 8;
	 * }
	 */
	writel(IOSAPIC_VERSION, addr + IOSAPIC_REG_SELECT);
	return readl(IOSAPIC_WINDOW + addr);
}

/*
 * if the given vector is already owned by other,
 *  assign a new vector for the other and make the vector available
 */
static void __init
iosapic_reassign_vector (int vector)
{
	int new_vector;

	if (iosapic_intr_info[vector].rte_index >= 0 || iosapic_intr_info[vector].addr
	    || iosapic_intr_info[vector].gsi_base || iosapic_intr_info[vector].dmode
	    || iosapic_intr_info[vector].polarity || iosapic_intr_info[vector].trigger)
	{
		new_vector = ia64_alloc_vector();
		printk(KERN_INFO "Reassigning vector %d to %d\n", vector, new_vector);
		memcpy(&iosapic_intr_info[new_vector], &iosapic_intr_info[vector],
		       sizeof(struct iosapic_intr_info));
		memset(&iosapic_intr_info[vector], 0, sizeof(struct iosapic_intr_info));
		iosapic_intr_info[vector].rte_index = -1;
	}
}

static void
register_intr (unsigned int gsi, int vector, unsigned char delivery,
	       unsigned long polarity, unsigned long trigger)
{
	irq_desc_t *idesc;
	struct hw_interrupt_type *irq_type;
	int rte_index;
	int index;
	unsigned long gsi_base;
	char *iosapic_address;

	index = find_iosapic(gsi);
	if (index < 0) {
		printk(KERN_WARNING "%s: No IOSAPIC for GSI 0x%x\n", __FUNCTION__, gsi);
		return;
	}

	iosapic_address = iosapic_lists[index].addr;
	gsi_base = iosapic_lists[index].gsi_base;

	rte_index = gsi - gsi_base;
	iosapic_intr_info[vector].rte_index = rte_index;
	iosapic_intr_info[vector].polarity = polarity;
	iosapic_intr_info[vector].dmode    = delivery;
	iosapic_intr_info[vector].addr     = iosapic_address;
	iosapic_intr_info[vector].gsi_base = gsi_base;
	iosapic_intr_info[vector].trigger  = trigger;

	if (trigger == IOSAPIC_EDGE)
		irq_type = &irq_type_iosapic_edge;
	else
		irq_type = &irq_type_iosapic_level;

	idesc = irq_desc(vector);
	if (idesc->handler != irq_type) {
		if (idesc->handler != &no_irq_type)
			printk(KERN_WARNING "%s: changing vector %d from %s to %s\n",
			       __FUNCTION__, vector, idesc->handler->typename, irq_type->typename);
		idesc->handler = irq_type;
	}
}

/*
 * ACPI can describe IOSAPIC interrupts via static tables and namespace
 * methods.  This provides an interface to register those interrupts and
 * program the IOSAPIC RTE.
 */
int
iosapic_register_intr (unsigned int gsi,
		       unsigned long polarity, unsigned long trigger)
{
	int vector;
	unsigned int dest = (ia64_get_lid() >> 16) & 0xffff;

	vector = gsi_to_vector(gsi);
	if (vector < 0)
		vector = ia64_alloc_vector();

	register_intr(gsi, vector, IOSAPIC_LOWEST_PRIORITY,
		      polarity, trigger);

	printk(KERN_INFO "GSI 0x%x(%s,%s) -> CPU 0x%04x vector %d\n",
	       gsi, (polarity == IOSAPIC_POL_HIGH ? "high" : "low"),
	       (trigger == IOSAPIC_EDGE ? "edge" : "level"), dest, vector);

	/* program the IOSAPIC routing table */
	set_rte(vector, dest);
	return vector;
}

/*
 * ACPI calls this when it finds an entry for a platform interrupt.
 * Note that the irq_base and IOSAPIC address must be set in iosapic_init().
 */
int __init
iosapic_register_platform_intr (u32 int_type, unsigned int gsi,
				int iosapic_vector, u16 eid, u16 id,
				unsigned long polarity, unsigned long trigger)
{
	unsigned char delivery;
	int vector;
	unsigned int dest = ((id << 8) | eid) & 0xffff;

	switch (int_type) {
	      case ACPI_INTERRUPT_PMI:
		vector = iosapic_vector;
		/*
		 * since PMI vector is alloc'd by FW(ACPI) not by kernel,
		 * we need to make sure the vector is available
		 */
		iosapic_reassign_vector(vector);
		delivery = IOSAPIC_PMI;
		break;
	      case ACPI_INTERRUPT_INIT:
		vector = ia64_alloc_vector();
		delivery = IOSAPIC_INIT;
		break;
	      case ACPI_INTERRUPT_CPEI:
		vector = IA64_CPE_VECTOR;
		delivery = IOSAPIC_LOWEST_PRIORITY;
		break;
	      default:
		printk(KERN_ERR "%s: invalid interrupt type (%d)\n", __FUNCTION__,
			int_type);
		return -1;
	}

	register_intr(gsi, vector, delivery, polarity, trigger);

	printk(KERN_INFO "PLATFORM int 0x%x: GSI 0x%x(%s,%s) -> CPU 0x%04x vector %d\n",
	       int_type, gsi, (polarity == IOSAPIC_POL_HIGH ? "high" : "low"),
	       (trigger == IOSAPIC_EDGE ? "edge" : "level"), dest, vector);

	/* program the IOSAPIC routing table */
	set_rte(vector, dest);
	return vector;
}


/*
 * ACPI calls this when it finds an entry for a legacy ISA IRQ override.
 * Note that the gsi_base and IOSAPIC address must be set in iosapic_init().
 */
void __init
iosapic_override_isa_irq (unsigned int isa_irq, unsigned int gsi,
			  unsigned long polarity, unsigned long trigger)
{
	int vector;
	unsigned int dest = (ia64_get_lid() >> 16) & 0xffff;

	vector = isa_irq_to_vector(isa_irq);

	register_intr(gsi, vector, IOSAPIC_LOWEST_PRIORITY, polarity, trigger);

	DBG("ISA: IRQ %u -> GSI 0x%x (%s,%s) -> CPU 0x%04x vector %d\n",
	    isa_irq, gsi,
	    polarity == IOSAPIC_POL_HIGH ? "high" : "low", trigger == IOSAPIC_EDGE ? "edge" : "level",
	    dest, vector);

	/* program the IOSAPIC routing table */
	set_rte(vector, dest);
}

/*
 * Map PCI pin to the corresponding GSI.
 * If no such mapping exists, return -1.
 */
static int
pci_pin_to_gsi (int segment, int bus, int slot, int pci_pin, unsigned int *gsi)
{
	struct pci_vector_struct *r;

	for (r = pci_irq.route; r < pci_irq.route + pci_irq.num_routes; r++)
		if (r->segment == segment && r->bus == bus &&
		    (r->pci_id >> 16) == slot && r->pin == pci_pin) {
			*gsi = r->irq;
			return 0;
		}

	return -1;
}

/*
 * Map PCI pin to the corresponding IA-64 interrupt vector.  If no such mapping exists,
 * try to allocate a new vector.  If it fails, return -1.
 */
static int
pci_pin_to_vector (int segment, int bus, int slot, int pci_pin)
{
	int vector;
	unsigned int gsi;

	if (pci_pin_to_gsi(segment, bus, slot, pci_pin, &gsi) < 0) {
		printk(KERN_ERR "PCI: no interrupt route for %02x:%02x:%02x pin %c\n",
			segment, bus, slot, 'A' + pci_pin);
		return -1;
	}

	vector = gsi_to_vector(gsi);

	if (vector < 0) {
		/* allocate a vector for this interrupt line */
		if (pcat_compat && (gsi < 16))
			vector = isa_irq_to_vector(gsi);
		else {
			/* new GSI; allocate a vector for it */
			vector = ia64_alloc_vector();
		}

		register_intr(gsi, vector, IOSAPIC_LOWEST_PRIORITY, IOSAPIC_POL_LOW, IOSAPIC_LEVEL);

		DBG("PCI: (%02x:%02x:%02x INT%c) -> GSI 0x%x -> vector %d\n",
		    segment, bus, slot, 'A' + pci_pin, gsi, vector);
	}

	return vector;
}

void __init
iosapic_system_init (int system_pcat_compat)
{
	int vector;

	for (vector = 0; vector < IA64_NUM_VECTORS; ++vector)
		iosapic_intr_info[vector].rte_index = -1;	/* mark as unused */

	pcat_compat = system_pcat_compat;
	if (pcat_compat) {
		/*
		 * Disable the compatibility mode interrupts (8259 style), needs IN/OUT support
		 * enabled.
		 */
		printk(KERN_INFO "%s: Disabling PC-AT compatible 8259 interrupts\n", __FUNCTION__);
		outb(0xff, 0xA1);
		outb(0xff, 0x21);
	}
}

void __init
iosapic_init (unsigned long phys_addr, unsigned int gsi_base)
{
	int num_rte;
	unsigned int isa_irq, ver;
	char *addr;

	addr = ioremap(phys_addr, 0);
	ver = iosapic_version(addr);

	/*
	 * The MAX_REDIR register holds the highest input pin
	 * number (starting from 0).
	 * We add 1 so that we can use it for number of pins (= RTEs)
	 */
	num_rte = ((ver >> 16) & 0xff) + 1;

	iosapic_lists[num_iosapic].addr = addr;
	iosapic_lists[num_iosapic].gsi_base = gsi_base;
	iosapic_lists[num_iosapic].num_rte = num_rte;
	num_iosapic++;

	printk(KERN_INFO "  IOSAPIC v%x.%x, address 0x%lx, GSIs 0x%x-0x%x\n",
	       (ver & 0xf0) >> 4, (ver & 0x0f), phys_addr, gsi_base, gsi_base + num_rte - 1);

	if ((gsi_base == 0) && pcat_compat) {

		/*
		 * Map the legacy ISA devices into the IOSAPIC data.  Some of these may
		 * get reprogrammed later on with data from the ACPI Interrupt Source
		 * Override table.
		 */
		for (isa_irq = 0; isa_irq < 16; ++isa_irq)
			iosapic_override_isa_irq(isa_irq, isa_irq, IOSAPIC_POL_HIGH, IOSAPIC_EDGE);
	}
}


/*
 * Set allocated interrupt vector to dev->irq and
 * program IOSAPIC to deliver interrupts
 */
void
iosapic_fixup_pci_interrupt (struct pci_dev *dev)
{
	int segment;
	unsigned char pci_pin;
	int vector;
	unsigned int dest;
	struct hw_interrupt_type *irq_type;
	irq_desc_t *idesc;

	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pci_pin);
	if (pci_pin) {
		pci_pin--; /* interrupt pins are numberd starting from 1 */

		segment = PCI_SEGMENT(dev);
		vector = pci_pin_to_vector(segment, dev->bus->number, PCI_SLOT(dev->devfn), pci_pin);

		if (vector < 0 && dev->bus->parent) {
			/* go back to the bridge */
			struct pci_dev *bridge = dev->bus->self;

			if (bridge) {
				/* allow for multiple bridges on an adapter */
				do {
					/* do the bridge swizzle... */
					pci_pin = (pci_pin + PCI_SLOT(dev->devfn)) % 4;
					vector = pci_pin_to_vector(segment,
								   bridge->bus->number,
								   PCI_SLOT(bridge->devfn),
								   pci_pin);
				} while (vector < 0 && (bridge = bridge->bus->self));
			}
			if (vector >= 0)
				printk(KERN_WARNING
				       "PCI: using PPB (%s INT%c) to get vector %d\n",
				       dev->slot_name, 'A' + pci_pin,
				       vector);
			else
				printk(KERN_WARNING
				       "PCI: Couldn't map irq for (%s INT%c)\n",
				       dev->slot_name, 'A' + pci_pin);
		}

		if (vector >= 0) {
			dev->irq = vector;

			irq_type = &irq_type_iosapic_level;
			idesc = irq_desc(vector);
			if (idesc->handler != irq_type) {
				if (idesc->handler != &no_irq_type)
					printk(KERN_INFO "%s: changing vector %d from %s to %s\n",
					       __FUNCTION__, vector,
					       idesc->handler->typename,
					       irq_type->typename);
				idesc->handler = irq_type;
			}
#ifdef CONFIG_SMP
			/*
			 * For platforms that do not support interrupt redirect
			 * via the XTP interface, we can round-robin the PCI
			 * device interrupts to the processors
			 */
			if (!(smp_int_redirect & SMP_IRQ_REDIRECTION)) {
				static int cpu_index = 0;

				dest = cpu_physical_id(cpu_index) & 0xffff;

				cpu_index++;
				if (cpu_index >= smp_num_cpus)
					cpu_index = 0;
			} else {
				/*
				 * Direct the interrupt vector to the current cpu,
				 * platform redirection will distribute them.
				 */
				dest = (ia64_get_lid() >> 16) & 0xffff;
			}
#else
			/* direct the interrupt vector to the running cpu id */
			dest = (ia64_get_lid() >> 16) & 0xffff;
#endif

			printk(KERN_INFO "PCI->APIC IRQ transform: (%s INT%c) -> CPU 0x%04x vector %d\n",
			       dev->slot_name, 'A' + pci_pin, dest, vector);
			set_rte(vector, dest);
		}
	}
}


void
iosapic_pci_fixup (int phase)
{
	struct	pci_dev	*dev;

	if (phase == 0) {
		if (0 != acpi_get_prt(&pci_irq.route, &pci_irq.num_routes)) {
			printk(KERN_ERR "%s: acpi_get_prt failed\n", __FUNCTION__);
		}
		return;
	}

	if (phase != 1)
		return;

	pci_for_each_dev(dev) {
		/* fixup dev->irq and program IOSAPIC */
		iosapic_fixup_pci_interrupt(dev);

		/*
		 * Nothing to fixup
		 * Fix out-of-range IRQ numbers
		 */
		if (dev->irq >= IA64_NUM_VECTORS)
			dev->irq = 15;	/* Spurious interrupts */
	}
}
