/*
 *  linux/arch/arm/mach-ebsa110/irq.c
 *
 *  Copyright (C) 1996-1998 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   22-08-1998	RMK	Restructured IRQ routines
 */
#include <linux/init.h>
#include <linux/stddef.h>
#include <linux/list.h>
#include <linux/timer.h>

#include <asm/mach/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

#include "hardware.h"

static void ebsa110_mask_irq(unsigned int irq)
{
	__raw_writeb(1 << irq, IRQ_MCLR);
}

static void ebsa110_unmask_irq(unsigned int irq)
{
	__raw_writeb(1 << irq, IRQ_MSET);
}
 
void __init ebsa110_init_irq(void)
{
	unsigned long flags;
	int irq;

	local_irq_save(flags);
	__raw_writeb(0xff, IRQ_MCLR);
	__raw_writeb(0x55, IRQ_MSET);
	__raw_writeb(0x00, IRQ_MSET);
	if (__raw_readb(IRQ_MASK) != 0x55)
		while (1);
	__raw_writeb(0xff, IRQ_MCLR);	/* clear all interrupt enables */
	local_irq_restore(flags);

	for (irq = 0; irq < NR_IRQS; irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= ebsa110_mask_irq;
		irq_desc[irq].mask	= ebsa110_mask_irq;
		irq_desc[irq].unmask	= ebsa110_unmask_irq;
	}
}

