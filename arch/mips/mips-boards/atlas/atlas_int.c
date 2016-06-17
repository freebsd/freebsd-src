/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
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
 * Atlas board.
 *
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>

#include <asm/irq.h>
#include <asm/mips-boards/atlas.h>
#include <asm/mips-boards/atlasint.h>
#include <asm/gdb-stub.h>


struct atlas_ictrl_regs *atlas_hw0_icregs
	= (struct atlas_ictrl_regs *)ATLAS_ICTRL_REGS_BASE;

extern asmlinkage void mipsIRQ(void);

#if 0
#define DEBUG_INT(x...) printk(x)
#else
#define DEBUG_INT(x...)
#endif

void disable_atlas_irq(unsigned int irq_nr)
{
	atlas_hw0_icregs->intrsten = (1 << irq_nr);
}

void enable_atlas_irq(unsigned int irq_nr)
{
	atlas_hw0_icregs->intseten = (1 << irq_nr);
}

static unsigned int startup_atlas_irq(unsigned int irq)
{
	enable_atlas_irq(irq);
	return 0; /* never anything pending */
}

#define shutdown_atlas_irq	disable_atlas_irq

#define mask_and_ack_atlas_irq disable_atlas_irq

static void end_atlas_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		enable_atlas_irq(irq);
}

static struct hw_interrupt_type atlas_irq_type = {
	"Atlas",
	startup_atlas_irq,
	shutdown_atlas_irq,
	enable_atlas_irq,
	disable_atlas_irq,
	mask_and_ack_atlas_irq,
	end_atlas_irq,
	NULL
};

static inline int ls1bit32(unsigned int x)
{
	int b = 31, s;

	s = 16; if (x << 16 == 0) s = 0; b -= s; x <<= s;
	s =  8; if (x <<  8 == 0) s = 0; b -= s; x <<= s;
	s =  4; if (x <<  4 == 0) s = 0; b -= s; x <<= s;
	s =  2; if (x <<  2 == 0) s = 0; b -= s; x <<= s;
	s =  1; if (x <<  1 == 0) s = 0; b -= s;

	return b;
}

void atlas_hw0_irqdispatch(struct pt_regs *regs)
{
	struct irqaction *action;
	unsigned long int_status;
	int irq, cpu = smp_processor_id();

	int_status = atlas_hw0_icregs->intstatus;

	/* if int_status == 0, then the interrupt has already been cleared */
	if (int_status == 0)
		return;

	irq = ls1bit32(int_status);
	action = irq_desc[irq].action;

	DEBUG_INT("atlas_hw0_irqdispatch: irq=%d\n", irq);

	/* if action == NULL, then we don't have a handler for the irq */
	if (action == NULL) {
		printk("No handler for hw0 irq: %i\n", irq);
		atomic_inc(&irq_err_count);
		return;
	}

	irq_enter(cpu, irq);
	kstat.irqs[0][irq]++;
	action->handler(irq, action->dev_id, regs);
	irq_exit(cpu, irq);

	return;
}

#ifdef CONFIG_KGDB
extern void breakpoint(void);
extern int remote_debug;
#endif

void __init init_IRQ(void)
{
	int i;

	/*
	 * Mask out all interrupt by writing "1" to all bit position in
	 * the interrupt reset reg.
	 */
	atlas_hw0_icregs->intrsten = 0xffffffff;

	/* Now safe to set the exception vector. */
	set_except_vector(0, mipsIRQ);

	for (i = 0; i <= ATLASINT_END; i++) {
		irq_desc[i].status	= IRQ_DISABLED;
		irq_desc[i].action	= 0;
		irq_desc[i].depth	= 1;
		irq_desc[i].handler	= &atlas_irq_type;
	}

#ifdef CONFIG_KGDB
	if (remote_debug) {
		set_debug_traps();
		breakpoint();
	}
#endif
}
