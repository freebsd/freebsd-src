/*
 *	linux/include/asm-parisc/irq.h
 *
 *	(C) 1992, 1993 Linus Torvalds, (C) 1997 Ingo Molnar,
 *		Copyright 1999 SuSE GmbH
 *
 *	IRQ/IPI changes taken from work by Thomas Radke
 *	<tomsoft@informatik.tu-chemnitz.de>
 */

#ifndef _ASM_PARISC_IRQ_H
#define _ASM_PARISC_IRQ_H

#include <asm/ptrace.h>
#include <asm/types.h>

#include <linux/string.h>
#include <linux/interrupt.h>


#define CPU_IRQ_REGION		1
#define TIMER_IRQ		(IRQ_FROM_REGION(CPU_IRQ_REGION) | 0)
#define	IPI_IRQ			(IRQ_FROM_REGION(CPU_IRQ_REGION) | 1)

/* This should be 31 for PA1.1 binaries and 63 for PA-2.0 wide mode */
#define MAX_CPU_IRQ		(BITS_PER_LONG - 1)

#if BITS_PER_LONG == 32
#  define IRQ_REGION_SHIFT 	5
#else
#  define IRQ_REGION_SHIFT 	6
#endif

#define IRQ_PER_REGION		(1 << IRQ_REGION_SHIFT)
#define NR_IRQ_REGS		16
#define NR_IRQS			(NR_IRQ_REGS * IRQ_PER_REGION)

#define IRQ_REGION(irq) 	((irq) >> IRQ_REGION_SHIFT)
#define IRQ_OFFSET(irq)		((irq) & ((1<<IRQ_REGION_SHIFT)-1))
#define	IRQ_FROM_REGION(reg)	((reg) << IRQ_REGION_SHIFT)

#define EISA_IRQ_REGION		0 /* region 0 needs to be reserved for EISA */
#define EISA_MAX_IRQS		16 /* max. (E)ISA irq line */

struct irq_region_ops {
	void (*disable_irq)(void *dev, int irq);
	void (* enable_irq)(void *dev, int irq);
	void (*   mask_irq)(void *dev, int irq);
	void (* unmask_irq)(void *dev, int irq);
};

struct irq_region_data {
	void *dev;
	const char *name;
	int irqbase;
	unsigned int status[IRQ_PER_REGION]; /* IRQ status */
};

struct irq_region {
	struct irq_region_ops ops;
	struct irq_region_data data;

	struct irqaction *action;
};

extern struct irq_region *irq_region[NR_IRQ_REGS];

static __inline__ int irq_cannonicalize(int irq)
{
#ifdef CONFIG_EISA
	return (irq == (IRQ_FROM_REGION(EISA_IRQ_REGION)+2) 
		? (IRQ_FROM_REGION(EISA_IRQ_REGION)+9) : irq);
#else
	return irq;
#endif
}

extern void disable_irq(int);
#define disable_irq_nosync(i) disable_irq(i)
extern void enable_irq(int);

extern void do_irq(struct irqaction *a, int i, struct pt_regs *p);
extern void do_irq_mask(unsigned long mask, struct irq_region *region,
	struct pt_regs *regs);

extern struct irq_region *alloc_irq_region(int count, struct irq_region_ops *ops,
	const char *name, void *dev);

extern int txn_alloc_irq(void);
extern int txn_claim_irq(int);
extern unsigned int txn_alloc_data(int, unsigned int);
extern unsigned long txn_alloc_addr(int);

/* soft power switch support (power.c) */
extern struct tasklet_struct power_tasklet;

#endif	/* _ASM_PARISC_IRQ_H */
