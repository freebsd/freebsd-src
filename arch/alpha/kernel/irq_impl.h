/*
 *	linux/arch/alpha/kernel/irq_impl.h
 *
 *	Copyright (C) 1995 Linus Torvalds
 *	Copyright (C) 1998, 2000 Richard Henderson
 *
 * This file contains declarations and inline functions for interfacing
 * with the IRQ handling routines in irq.c.
 */

#include <linux/interrupt.h>
#include <linux/irq.h>


#define RTC_IRQ    8

extern void isa_device_interrupt(unsigned long, struct pt_regs *);
extern void isa_no_iack_sc_device_interrupt(unsigned long, struct pt_regs *);
extern void srm_device_interrupt(unsigned long, struct pt_regs *);
extern void pyxis_device_interrupt(unsigned long, struct pt_regs *);

extern struct irqaction timer_irqaction;
extern struct irqaction isa_cascade_irqaction;
extern struct irqaction timer_cascade_irqaction;
extern struct irqaction halt_switch_irqaction;

extern void init_srm_irqs(long, unsigned long);
extern void init_pyxis_irqs(unsigned long);
extern void init_rtc_irq(void);

extern void common_init_isa_dma(void);

extern void i8259a_enable_irq(unsigned int);
extern void i8259a_disable_irq(unsigned int);
extern void i8259a_mask_and_ack_irq(unsigned int);
extern unsigned int i8259a_startup_irq(unsigned int);
extern void i8259a_end_irq(unsigned int);
extern struct hw_interrupt_type i8259a_irq_type;
extern void init_i8259a_irqs(void);

extern void handle_irq(int irq, struct pt_regs * regs);

extern unsigned long prof_cpu_mask;

static inline void
alpha_do_profile(unsigned long pc)
{
	extern char _stext;

	if (!prof_buffer)
		return;

	/*
	 * Only measure the CPUs specified by /proc/irq/prof_cpu_mask.
	 * (default is all CPUs.)
	 */
	if (!((1<<smp_processor_id()) & prof_cpu_mask))
		return;

	pc -= (unsigned long) &_stext;
	pc >>= prof_shift;
	/*
	 * Don't ignore out-of-bounds PC values silently,
	 * put them into the last histogram slot, so if
	 * present, they will show up as a sharp peak.
	 */
	if (pc > prof_len - 1)
		pc = prof_len - 1;
	atomic_inc((atomic_t *)&prof_buffer[pc]);
}
