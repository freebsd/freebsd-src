/*
 * Copyright (C) 1999 Cort Dougan <cort@cs.nmt.edu>
 *
 * Use inline IRQs where possible - Anton Blanchard <anton@au.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifdef __KERNEL__
#ifndef _PPC64_HW_IRQ_H
#define _PPC64_HW_IRQ_H

#include <linux/config.h>
#include <asm/irq.h>

int timer_interrupt(struct pt_regs *);

#ifdef CONFIG_PPC_ISERIES

extern void __no_use_sti(void);
extern void __no_use_cli(void);
extern void __no_use_restore_flags(unsigned long);
extern unsigned long __no_use_save_flags(void);
extern void __no_use_set_lost(unsigned long);
extern void __no_lpq_restore_flags(unsigned long);

#define __cli()			__no_use_cli()
#define __sti()			__no_use_sti()
#define __save_flags(flags)	((flags) = __no_use_save_flags())
#define __restore_flags(flags)	__no_use_restore_flags((unsigned long)flags)
#define __save_and_cli(flags)	({__save_flags(flags);__cli();})
#define __save_and_sti(flags)	({__save_flags(flags);__sti();})

#else

#define __save_flags(flags)	((flags) = mfmsr())
#define __restore_flags(flags) do { \
	__asm__ __volatile__("": : :"memory"); \
	mtmsrd(flags); \
} while(0)

static inline void __cli(void)
{
	unsigned long msr;
	msr = mfmsr();
	mtmsrd(msr & ~MSR_EE);
	__asm__ __volatile__("": : :"memory");
}

static inline void __sti(void)
{
	unsigned long msr;
	__asm__ __volatile__("": : :"memory");
	msr = mfmsr();
	mtmsrd(msr | MSR_EE);
}

static inline void __do_save_and_cli(unsigned long *flags)
{
	unsigned long msr;
	msr = mfmsr();
	*flags = msr;
	mtmsrd(msr & ~MSR_EE);
	__asm__ __volatile__("": : :"memory");
}

#define __save_and_cli(flags)          __do_save_and_cli(&flags)
#define __save_and_sti(flags)		({__save_flags(flags);__sti();})

#endif /* CONFIG_PPC_ISERIES */

#define mask_irq(irq) ({ 				\
	irq_desc_t *desc = irqdesc(irq);		\
	if (desc->handler && desc->handler->disable)	\
		desc->handler->disable(irq);		\
})
#define unmask_irq(irq) ({				\
	irq_desc_t *desc = irqdesc(irq);		\
	if (desc->handler && desc->handler->enable)	\
		desc->handler->enable(irq);		\
})
#define ack_irq(irq) ({					\
	irq_desc_t *desc = irqdesc(irq);		\
	if (desc->handler && desc->handler->ack)	\
		desc->handler->ack(irq);		\
})

/* Should we handle this via lost interrupts and IPIs or should we don't care like
 * we do now ? --BenH.
 */
struct hw_interrupt_type;
static inline void hw_resend_irq(struct hw_interrupt_type *h, unsigned int i) {}

#endif /* _PPC64_HW_IRQ_H */
#endif /* __KERNEL__ */
