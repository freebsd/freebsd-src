/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 Linus Torvalds
 * Copyright (C) 1994 - 2000 Ralf Baechle
 */
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>

#include <asm/i8259.h>
#include <asm/io.h>
#include <asm/sni.h>

spinlock_t pciasic_lock = SPIN_LOCK_UNLOCKED;

extern asmlinkage void sni_rm200_pci_handle_int(void);

static void enable_pciasic_irq(unsigned int irq);

static unsigned int startup_pciasic_irq(unsigned int irq)
{
	enable_pciasic_irq(irq);
	return 0; /* never anything pending */
}

#define shutdown_pciasic_irq	disable_pciasic_irq

void disable_pciasic_irq(unsigned int irq)
{
	unsigned int mask = ~(1 << (irq - PCIMT_IRQ_INT2));
	unsigned long flags;

	spin_lock_irqsave(&pciasic_lock, flags);
	*(volatile u8 *) PCIMT_IRQSEL &= mask;
	spin_unlock_irqrestore(&pciasic_lock, flags);
}

static void enable_pciasic_irq(unsigned int irq)
{
	unsigned int mask = 1 << (irq - PCIMT_IRQ_INT2);
	unsigned long flags;

	spin_lock_irqsave(&pciasic_lock, flags);
	*(volatile u8 *) PCIMT_IRQSEL |= mask;
	spin_unlock_irqrestore(&pciasic_lock, flags);
}

#define mask_and_ack_pciasic_irq disable_pciasic_irq

static void end_pciasic_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		enable_pciasic_irq(irq);
}

static struct hw_interrupt_type pciasic_irq_type = {
	"PCIASIC",
	startup_pciasic_irq,
	shutdown_pciasic_irq,
	enable_pciasic_irq,
	disable_pciasic_irq,
	mask_and_ack_pciasic_irq,
	end_pciasic_irq,
	NULL
};

/*
 * hwint0 should deal with MP agent, ASIC PCI, EISA NMI and debug
 * button interrupts.  Later ...
 */
void pciasic_hwint0(struct pt_regs *regs)
{
	panic("Received int0 but no handler yet ...");
}

/* This interrupt was used for the com1 console on the first prototypes.  */
void pciasic_hwint2(struct pt_regs *regs)
{
	/* I think this shouldn't happen on production machines.  */
	panic("hwint2 and no handler yet");
}

/* hwint5 is the r4k count / compare interrupt  */
void pciasic_hwint5(struct pt_regs *regs)
{
	panic("hwint5 and no handler yet");
}

static inline int ls1bit8(unsigned int x)
{
	int b = 8, s;

	x <<= 24;
	s = 4; if ((x & 0x0f) == 0) s = 0; b -= s; x <<= s;
	s = 2; if ((x & 0x03) == 0) s = 0; b -= s; x <<= s;
	s = 1; if ((x & 0x01) == 0) s = 0; b -= s;

	return b;
}

/*
 * hwint 1 deals with EISA and SCSI interrupts,
 * hwint 3 should deal with the PCI A - D interrupts,
 * hwint 4 is used for only the onboard PCnet 32.
 */
void pciasic_hwint134(struct pt_regs *regs)
{
	u8 pend = *(volatile char *)PCIMT_CSITPEND;
	int irq;

	irq = PCIMT_IRQ_INT2 + ls1bit8(pend);
	if (irq == PCIMT_IRQ_EISA) {
		pend = *(volatile char *)PCIMT_INT_ACKNOWLEDGE;
		if (!(pend ^ 0xff))
			return;
	}
	do_IRQ(irq, regs);
	return;
}

void __init init_pciasic(void)
{
	unsigned long flags;

	spin_lock_irqsave(&pciasic_lock, flags);
	* (volatile u8 *) PCIMT_IRQSEL =
		IT_EISA | IT_INTA | IT_INTB | IT_INTC | IT_INTD;
	spin_unlock_irqrestore(&pciasic_lock, flags);
}

/*
 * On systems with i8259-style interrupt controllers we assume for
 * driver compatibility reasons interrupts 0 - 15 to be the i8295
 * interrupts even if the hardware uses a different interrupt numbering.
 */
void __init init_IRQ (void)
{
	int i;

	set_except_vector(0, sni_rm200_pci_handle_int);

	init_generic_irq();
	init_i8259_irqs();			/* Integrated i8259  */
	init_pciasic();

	/* Actually we've got more interrupts to handle ...  */
	for (i = PCIMT_IRQ_INT2; i <= PCIMT_IRQ_ETHERNET; i++) {
		irq_desc[i].status     = IRQ_DISABLED;
		irq_desc[i].action     = 0;
		irq_desc[i].depth      = 1;
		irq_desc[i].handler    = &pciasic_irq_type;
	}
}
