/*
 *  arch/mips/philips/nino/irq.c
 *
 *  Copyright (C) 2001 Steven J. Hill (sjhill@realitydiluted.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Interrupt service routines for Philips Nino
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <asm/mipsregs.h>
#include <asm/tx3912.h>

#define ALLINTS (IE_IRQ0 | IE_IRQ1 | IE_IRQ2 | IE_IRQ3 | IE_IRQ4 | IE_IRQ5)

static void enable_irq6(unsigned int irq)
{
	if(irq == 0) {
		outl(inl(TX3912_INT6_ENABLE) |
			TX3912_INT6_ENABLE_PRIORITYMASK_PERINT,
			TX3912_INT6_ENABLE);
		outl(inl(TX3912_INT5_ENABLE) | TX3912_INT5_PERINT,
			TX3912_INT5_ENABLE);
	}
	if(irq == 3) {
		outl(inl(TX3912_INT6_ENABLE) |
			TX3912_INT6_ENABLE_PRIORITYMASK_UARTARXINT,
			TX3912_INT6_ENABLE);
		outl(inl(TX3912_INT2_ENABLE) | TX3912_INT2_UARTA_RX_BITS,
			TX3912_INT2_ENABLE);
	}
}

static unsigned int startup_irq6(unsigned int irq)
{
	enable_irq6(irq);

	return 0;		/* Never anything pending  */
}

static void disable_irq6(unsigned int irq)
{
	if(irq == 0) {
		outl(inl(TX3912_INT6_ENABLE) &
			~TX3912_INT6_ENABLE_PRIORITYMASK_PERINT,
			TX3912_INT6_ENABLE);
		outl(inl(TX3912_INT5_ENABLE) & ~TX3912_INT5_PERINT,
			TX3912_INT5_ENABLE);
		outl(inl(TX3912_INT5_CLEAR) | TX3912_INT5_PERINT,
			TX3912_INT5_CLEAR);
	}
	if(irq == 3) {
		outl(inl(TX3912_INT6_ENABLE) &
			~TX3912_INT6_ENABLE_PRIORITYMASK_UARTARXINT,
			TX3912_INT6_ENABLE);
		outl(inl(TX3912_INT2_ENABLE) & ~TX3912_INT2_UARTA_RX_BITS,
			TX3912_INT2_ENABLE);
	}
}

#define shutdown_irq6		disable_irq6
#define mask_and_ack_irq6	disable_irq6

static void end_irq6(unsigned int irq)
{
	if(!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		enable_irq6(irq);
}

static struct hw_interrupt_type irq6_type = {
	"MIPS",
	startup_irq6,
	shutdown_irq6,
	enable_irq6,
	disable_irq6,
	mask_and_ack_irq6,
	end_irq6,
	NULL
};

void irq6_dispatch(struct pt_regs *regs)
{
	int irq = -1;

	if((inl(TX3912_INT6_STATUS) & TX3912_INT6_STATUS_INTVEC_UARTARXINT) ==
		TX3912_INT6_STATUS_INTVEC_UARTARXINT) {
		irq = 3;
		goto done;
	}
	if ((inl(TX3912_INT6_STATUS) & TX3912_INT6_STATUS_INTVEC_PERINT) ==
		TX3912_INT6_STATUS_INTVEC_PERINT) {
		irq = 0;
		goto done;
	}

	/* if irq == -1, then interrupt was cleared or is invalid */
	if (irq == -1) {
		panic("Unhandled High Priority PR31700 Interrupt = 0x%08x",
			inl(TX3912_INT6_STATUS));
	}

done:
	do_IRQ(irq, regs);
}

static void enable_irq4(unsigned int irq)
{
	set_c0_status(STATUSF_IP4);
	if (irq == 2) {
		outl(inl(TX3912_INT2_CLEAR) | TX3912_INT2_UARTA_TX_BITS,
			TX3912_INT2_CLEAR);
		outl(inl(TX3912_INT2_ENABLE) | TX3912_INT2_UARTA_TX_BITS,
			TX3912_INT2_ENABLE);
	}
}

static unsigned int startup_irq4(unsigned int irq)
{
	enable_irq4(irq);

	return 0;		/* Never anything pending  */
}

static void disable_irq4(unsigned int irq)
{
	clear_c0_status(STATUSF_IP4);
}

#define shutdown_irq4		disable_irq4
#define mask_and_ack_irq4	disable_irq4

static void end_irq4(unsigned int irq)
{
	if(!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		enable_irq4(irq);
}

static struct hw_interrupt_type irq4_type = {
	"MIPS",
	startup_irq4,
	shutdown_irq4,
	enable_irq4,
	disable_irq4,
	mask_and_ack_irq4,
	end_irq4,
	NULL
};

void irq4_dispatch(struct pt_regs *regs)
{
	int irq = -1;

	if(inl(TX3912_INT2_STATUS) & TX3912_INT2_UARTA_TX_BITS) {
		irq = 2;
		goto done;
	}

	/* if irq == -1, then interrupt was cleared or is invalid */
	if (irq == -1) {
		printk("PR31700 Interrupt Status Register 1 = 0x%08x\n",
			inl(TX3912_INT1_STATUS));
		printk("PR31700 Interrupt Status Register 2 = 0x%08x\n",
			inl(TX3912_INT2_STATUS));
		printk("PR31700 Interrupt Status Register 3 = 0x%08x\n",
			inl(TX3912_INT3_STATUS));
		printk("PR31700 Interrupt Status Register 4 = 0x%08x\n",
			inl(TX3912_INT4_STATUS));
		printk("PR31700 Interrupt Status Register 5 = 0x%08x\n",
			inl(TX3912_INT5_STATUS));
		panic("Unhandled Low Priority PR31700 Interrupt");
	}

done:
	do_IRQ(irq, regs);
	return;
}

void irq_bad(struct pt_regs *regs)
{
	/* This should never happen */
	printk(" CAUSE register = 0x%08lx\n", regs->cp0_cause);
	printk("STATUS register = 0x%08lx\n", regs->cp0_status);
	printk("   EPC register = 0x%08lx\n", regs->cp0_epc);
	panic("Stray interrupt, spinning...");
}

void __init nino_irq_setup(void)
{
	extern asmlinkage void ninoIRQ(void);

	unsigned int i;

	/* Disable all hardware interrupts */
	change_c0_status(ST0_IM, 0x00);

	/* Clear interrupts */
	outl(0xffffffff, TX3912_INT1_CLEAR);
	outl(0xffffffff, TX3912_INT2_CLEAR);
	outl(0xffffffff, TX3912_INT3_CLEAR);
	outl(0xffffffff, TX3912_INT4_CLEAR);
	outl(0xffffffff, TX3912_INT5_CLEAR);

	/*
	 * Disable all PR31700 interrupts. We let the various
	 * device drivers in the system register themselves
	 * and set the proper hardware bits.
	 */
	outl(0x00000000, TX3912_INT1_ENABLE);
	outl(0x00000000, TX3912_INT2_ENABLE);
	outl(0x00000000, TX3912_INT3_ENABLE);
	outl(0x00000000, TX3912_INT4_ENABLE);
	outl(0x00000000, TX3912_INT5_ENABLE);

	/* Initialize IRQ vector table */
	init_generic_irq();

	/* Initialize IRQ action handlers */
	for (i = 0; i < 16; i++) {
		hw_irq_controller *handler = NULL;
		if (i == 0 || i == 3)
			handler		= &irq6_type;
		else
			handler		= &irq4_type;

		irq_desc[i].status	= IRQ_DISABLED;
		irq_desc[i].action	= 0;
		irq_desc[i].depth	= 1;
		irq_desc[i].handler	= handler;
	}

	/* Set up the external interrupt exception vector */
	set_except_vector(0, ninoIRQ);

	/* Enable high priority interrupts */
	outl(TX3912_INT6_ENABLE_GLOBALEN | TX3912_INT6_ENABLE_HIGH_PRIORITY,
		TX3912_INT6_ENABLE);

	/* Enable all interrupts */
	change_c0_status(ST0_IM, ALLINTS);
}

void (*irq_setup)(void);

void __init init_IRQ(void)
{
#ifdef CONFIG_KGDB
	extern void breakpoint(void);
	extern void set_debug_traps(void);

	printk("Wait for gdb client connection ...\n");
	set_debug_traps();
	breakpoint();
#endif

	/* Invoke board-specific irq setup */
	irq_setup();
}
