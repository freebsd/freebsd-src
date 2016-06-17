/*
 * Copyright 2001 MontaVista Software Inc.
 * Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 *
 * This file define the irq handler for MIPS CPU interrupts.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

/*
 * Almost all MIPS CPUs define 8 interrupt sources.  They are typically
 * level triggered (i.e., cannot be cleared from CPU; must be cleared from
 * device).  The first two are software interrupts.  The last one is
 * usually the CPU timer interrupt if counter register is present or, for
 * CPUs with an external FPU, by convention it's the FPU exception interrupt.
 *
 * This file exports one global function:
 *	void mips_cpu_irq_init(int irq_base);
 */
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>

#include <asm/irq_cpu.h>
#include <asm/mipsregs.h>

static int mips_cpu_irq_base = -1;

static void mips_cpu_irq_enable(unsigned int irq)
{
	clear_c0_cause( 1 << (irq - mips_cpu_irq_base + 8));
	set_c0_status(1 << (irq - mips_cpu_irq_base + 8));
}

static void mips_cpu_irq_disable(unsigned int irq)
{
	clear_c0_status(1 << (irq - mips_cpu_irq_base + 8));
}

static unsigned int mips_cpu_irq_startup(unsigned int irq)
{
	mips_cpu_irq_enable(irq);

	return 0;
}

#define	mips_cpu_irq_shutdown	mips_cpu_irq_disable

static void mips_cpu_irq_ack(unsigned int irq)
{
	/* although we attempt to clear the IP bit in cause register, I think
	 * usually it is cleared by device (irq source)
	 */
	clear_c0_cause(1 << (irq - mips_cpu_irq_base + 8));

	/* disable this interrupt - so that we safe proceed to the handler */
	mips_cpu_irq_disable(irq);
}

static void mips_cpu_irq_end(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		mips_cpu_irq_enable(irq);
}

static hw_irq_controller mips_cpu_irq_controller = {
	"MIPS",
	mips_cpu_irq_startup,
	mips_cpu_irq_shutdown,
	mips_cpu_irq_enable,
	mips_cpu_irq_disable,
	mips_cpu_irq_ack,
	mips_cpu_irq_end,
	NULL			/* no affinity stuff for UP */
};


void __init mips_cpu_irq_init(int irq_base)
{
	int i;

	for (i = irq_base; i < irq_base + 8; i++) {
		irq_desc[i].status = IRQ_DISABLED;
		irq_desc[i].action = NULL;
		irq_desc[i].depth = 1;
		irq_desc[i].handler = &mips_cpu_irq_controller;
	}

	mips_cpu_irq_base = irq_base;
}
