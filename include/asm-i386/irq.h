#ifndef _ASM_IRQ_H
#define _ASM_IRQ_H

/*
 *	linux/include/asm/irq.h
 *
 *	(C) 1992, 1993 Linus Torvalds, (C) 1997 Ingo Molnar
 *
 *	IRQ/IPI changes taken from work by Thomas Radke
 *	<tomsoft@informatik.tu-chemnitz.de>
 */

#include <linux/config.h>

#define TIMER_IRQ 0

/*
 * 16 8259A IRQ's, 208 potential APIC interrupt sources.
 * Right now the APIC is mostly only used for SMP.
 * 256 vectors is an architectural limit. (we can have
 * more than 256 devices theoretically, but they will
 * have to use shared interrupts)
 * Since vectors 0x00-0x1f are used/reserved for the CPU,
 * the usable vector space is 0x20-0xff (224 vectors)
 */
#ifdef CONFIG_X86_IO_APIC
#define NR_IRQS 224
#else
#define NR_IRQS 16
#endif

static __inline__ int irq_cannonicalize(int irq)
{
	return ((irq == 2) ? 9 : irq);
}

extern void disable_irq(unsigned int);
extern void disable_irq_nosync(unsigned int);
extern void enable_irq(unsigned int);
extern void release_x86_irqs(struct task_struct *);

#ifdef CONFIG_X86_LOCAL_APIC
#define ARCH_HAS_NMI_WATCHDOG		/* See include/linux/nmi.h */
#endif

#endif /* _ASM_IRQ_H */
