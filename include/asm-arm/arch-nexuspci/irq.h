/*
 * include/asm-arm/arch-nexuspci/irq.h
 *
 * Copyright (C) 1998, 1999, 2000 Philip Blundell
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <asm/io.h>

#define fixup_irq(x) (x)

extern unsigned long soft_irq_mask;

static const unsigned char irq_cmd[] =
{
	INTCONT_IRQ_DUART,
	INTCONT_IRQ_PLX,
	INTCONT_IRQ_D,
	INTCONT_IRQ_C,
	INTCONT_IRQ_B,
	INTCONT_IRQ_A,
	INTCONT_IRQ_SYSERR
};

static void ftvpci_mask_irq(unsigned int irq)
{
	__raw_writel(irq_cmd[irq], INTCONT_BASE);
	soft_irq_mask &= ~(1<<irq);
}

static void ftvpci_unmask_irq(unsigned int irq)
{
	soft_irq_mask |= (1<<irq);
	__raw_writel(irq_cmd[irq] | 1, INTCONT_BASE);
}
 
static __inline__ void irq_init_irq(void)
{
	unsigned int i;

	/* Mask all FIQs */
	__raw_writel(INTCONT_FIQ_PLX, INTCONT_BASE);
	__raw_writel(INTCONT_FIQ_D, INTCONT_BASE);
	__raw_writel(INTCONT_FIQ_C, INTCONT_BASE);
	__raw_writel(INTCONT_FIQ_B, INTCONT_BASE);
	__raw_writel(INTCONT_FIQ_A, INTCONT_BASE);
	__raw_writel(INTCONT_FIQ_SYSERR, INTCONT_BASE);

	/* Disable all interrupts initially. */
	for (i = 0; i < NR_IRQS; i++) {
		if (i >= FIRST_IRQ && i <= LAST_IRQ) {
			irq_desc[i].valid	= 1;
			irq_desc[i].probe_ok	= 1;
			irq_desc[i].mask_ack	= ftvpci_mask_irq;
			irq_desc[i].mask	= ftvpci_mask_irq;
			irq_desc[i].unmask	= ftvpci_unmask_irq;
			ftvpci_mask_irq(i);
		} else {
			irq_desc[i].valid	= 0;
			irq_desc[i].probe_ok	= 0;
		}	
	}		
}
