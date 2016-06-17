/*
 *      c 2001 PowerPC 64 Team, IBM Corp
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */
#ifndef _PPC_KERNEL_LOCAL_IRQ_H
#define _PPC_KERNEL_LOCAL_IRQ_H

#include <linux/kernel_stat.h>
#include <linux/interrupt.h>
#include <linux/cache.h>
#include <linux/spinlock.h>
#include <linux/irq.h>

void ppc_irq_dispatch_handler(struct pt_regs *regs, int irq);

#define NR_MASK_WORDS	((NR_IRQS + 63) / 64)

extern int ppc_spurious_interrupts;
extern int ppc_second_irq;
extern struct irqaction *ppc_irq_action[NR_IRQS];

#endif /* _PPC_KERNEL_LOCAL_IRQ_H */
