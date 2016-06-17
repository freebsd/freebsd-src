/*
 * include/asm-arm/arch-tbox/irq.h
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

static void tbox_mask_irq(unsigned int irq)
{
	__raw_writel(0, INTCONT + (irq << 2));
	soft_irq_mask &= ~(1<<irq);
}

static void tbox_unmask_irq(unsigned int irq)
{
	soft_irq_mask |= (1<<irq);
	__raw_writel(1, INTCONT + (irq << 2));
}
 
static __inline__ void irq_init_irq(void)
{
	unsigned int i;

	/* Disable all interrupts initially. */
	for (i = 0; i < NR_IRQS; i++) {
		if (i <= 10 || (i >= 12 && i <= 13)) {
			irq_desc[i].valid	= 1;
			irq_desc[i].probe_ok	= 0;
			irq_desc[i].mask_ack	= tbox_mask_irq;
			irq_desc[i].mask	= tbox_mask_irq;
			irq_desc[i].unmask	= tbox_unmask_irq;
			tbox_mask_irq(i);
		} else {
			irq_desc[i].valid	= 0;
			irq_desc[i].probe_ok	= 0;
		}
	}
}
