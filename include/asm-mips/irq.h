/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 by Waldorf GMBH, written by Ralf Baechle
 * Copyright (C) 1995, 96, 97, 98, 99, 2000, 01, 02 by Ralf Baechle
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 * Copyright (C) 2001 Kanoj Sarcar
 */
#ifndef _ASM_IRQ_H
#define _ASM_IRQ_H

#include <linux/config.h>
#include <linux/linkage.h>

#define NR_IRQS 128		/* Largest number of ints of all machines.  */

#ifdef CONFIG_I8259
static inline int irq_cannonicalize(int irq)
{
	return ((irq == 2) ? 9 : irq);
}
#else
#define irq_cannonicalize(irq) (irq)	/* Sane hardware, sane code ... */
#endif

extern void disable_irq(unsigned int);
extern void disable_irq_nosync(unsigned int);
extern void enable_irq(unsigned int);

struct pt_regs;
extern asmlinkage unsigned int do_IRQ(int irq, struct pt_regs *regs);

/* Machine specific interrupt initialization  */
extern void (*irq_setup)(void);

extern void init_generic_irq(void);

#endif /* _ASM_IRQ_H */
