/*
 *  linux/include/asm-arm/mach/irq.h
 *
 *  Copyright (C) 1995-2000 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_ARM_MACH_IRQ_H
#define __ASM_ARM_MACH_IRQ_H

struct irqdesc {
	unsigned int	 triggered: 1;		/* IRQ has occurred	      */
	unsigned int	 running  : 1;		/* IRQ is running             */
	unsigned int	 pending  : 1;		/* IRQ is pending	      */
	unsigned int	 probing  : 1;		/* IRQ in use for a probe     */
	unsigned int	 probe_ok : 1;		/* IRQ can be used for probe  */
	unsigned int	 valid    : 1;		/* IRQ claimable	      */
	unsigned int	 noautoenable : 1;	/* don't automatically enable IRQ */
	unsigned int	 unused   :25;
	unsigned int	 disable_depth;

	struct list_head pend;

	void (*mask_ack)(unsigned int irq);	/* Mask and acknowledge IRQ   */
	void (*mask)(unsigned int irq);		/* Mask IRQ		      */
	void (*unmask)(unsigned int irq);	/* Unmask IRQ		      */
	struct irqaction *action;

	/*
	 * IRQ lock detection
	 */
	unsigned int	 lck_cnt;
	unsigned int	 lck_pc;
	unsigned int	 lck_jif;
	int		 lck_warned;
	struct timer_list	lck_timer;
};

extern struct irqdesc irq_desc[];

extern void (*init_arch_irq)(void);
extern int setup_arm_irq(int, struct irqaction *);
extern int get_fiq_list(char *);
extern void init_FIQ(void);

#endif
