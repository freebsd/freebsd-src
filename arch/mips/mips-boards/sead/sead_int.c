/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2002 MIPS Technologies, Inc.  All rights reserved.
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * Routines for generic manipulation of the interrupts found on the MIPS
 * Sead board.
 *
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>

#include <asm/mips-boards/sead.h>
#include <asm/mips-boards/seadint.h>

extern asmlinkage void mipsIRQ(void);

void disable_sead_irq(unsigned int irq_nr)
{
	if (irq_nr == SEADINT_UART0)
		clear_c0_status(0x00000400);
	else
		if (irq_nr == SEADINT_UART1)
			clear_c0_status(0x00000800);
}

void enable_sead_irq(unsigned int irq_nr)
{
	if (irq_nr == SEADINT_UART0)
		set_c0_status(0x00000400);
	else
		if (irq_nr == SEADINT_UART1)
			set_c0_status(0x00000800);
}

static unsigned int startup_sead_irq(unsigned int irq)
{
	enable_sead_irq(irq);
	return 0; /* never anything pending */
}

#define shutdown_sead_irq	disable_sead_irq

#define mask_and_ack_sead_irq disable_sead_irq

static void end_sead_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		enable_sead_irq(irq);
}

static struct hw_interrupt_type sead_irq_type = {
	"SEAD",
	startup_sead_irq,
	shutdown_sead_irq,
	enable_sead_irq,
	disable_sead_irq,
	mask_and_ack_sead_irq,
	end_sead_irq,
	NULL
};

void sead_hw0_irqdispatch(struct pt_regs *regs)
{
	do_IRQ(0, regs);
}

void sead_hw1_irqdispatch(struct pt_regs *regs)
{
	do_IRQ(1, regs);
}

void __init init_IRQ(void)
{
	int i;

        /*
         * Mask out all interrupt
	 */
	clear_c0_status(0x0000ff00);

	/* Now safe to set the exception vector. */
	set_except_vector(0, mipsIRQ);

	init_generic_irq();

	for (i = 0; i < SEADINT_END; i++) {
		irq_desc[i].status	= IRQ_DISABLED;
		irq_desc[i].action	= NULL;
		irq_desc[i].depth	= 1;
		irq_desc[i].lock	= SPIN_LOCK_UNLOCKED;
		irq_desc[i].handler	= &sead_irq_type;
	}
}
