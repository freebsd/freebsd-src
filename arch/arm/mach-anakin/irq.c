/*
 *  linux/arch/arm/mach-anakin/irq.c
 *
 *  Copyright (C) 2001 Aleph One Ltd. for Acunia N.V.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   10-Apr-2001 TTC	Created
 */

#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/timer.h>

#include <asm/irq.h>
#include <asm/mach/irq.h>

extern unsigned int anakin_irq_mask, anakin_active_irqs;
extern void do_IRQ(int, struct pt_regs *);

static void
anakin_mask_irq(unsigned int irq)
{
	anakin_irq_mask &= ~(1 << irq);
}

static void
anakin_unmask_irq(unsigned int irq)
{
	anakin_irq_mask |= (1 << irq);
}

/*
 * This is a faked interrupt to deal with parallel interrupt requests
 * on the Anakin.  Make sure that its interrupt number is not in any
 * way conflicting with the hardware interrupt numbers!  Check
 * IRQ_ANAKIN in linux/include/asm-arm/arch-anakin/irqs.h.
 */
static void
anakin_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	for (irq = 0; irq < NR_IRQS; irq++)
		if (anakin_active_irqs & (1 << irq))
			do_IRQ(irq, regs);
}

static struct irqaction anakin_irq = {
	.name		= "Anakin IRQ",
	.handler	= anakin_interrupt,
	.flags		= SA_INTERRUPT,
};
 
void __init
irq_init_irq(void)
{
	unsigned int irq;

	for (irq = 0; irq < NR_IRQS; irq++) {
		switch (irq) {
		case IRQ_UART0:
		case IRQ_UART1:
		case IRQ_UART2:
		case IRQ_TICK:
		case IRQ_CODEC:
		case IRQ_UART4:
		case IRQ_TOUCHSCREEN:
		case IRQ_UART3:
		case IRQ_FIFO:
		case IRQ_CAN:
		case IRQ_COMPACTFLASH:
		case IRQ_BOSH:
		case IRQ_ANAKIN:
			irq_desc[irq].valid = 1;
			irq_desc[irq].mask_ack = anakin_mask_irq;
			irq_desc[irq].mask = anakin_mask_irq;
			irq_desc[irq].unmask = anakin_unmask_irq;
		}
	}
	setup_arm_irq(IRQ_ANAKIN, &anakin_irq);
}
