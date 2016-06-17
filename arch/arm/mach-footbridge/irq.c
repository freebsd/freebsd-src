/*
 *  linux/arch/arm/mach-footbridge/irq.c
 *
 *  Copyright (C) 1996-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   22-Aug-1998 RMK	Restructured IRQ routines
 *   03-Sep-1998 PJB	Merged CATS support
 *   20-Jan-1998 RMK	Started merge of EBSA286, CATS and NetWinder
 *   26-Jan-1999 PJB	Don't use IACK on CATS
 *   16-Mar-1999 RMK	Added autodetect of ISA PICs
 */
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/timer.h>

#include <asm/mach/irq.h>

#include <asm/hardware.h>
#include <asm/hardware/dec21285.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/mach-types.h>

/*
 * Footbridge IRQ translation table
 *  Converts from our IRQ numbers into FootBridge masks
 */
static const int fb_irq_mask[] = {
	IRQ_MASK_UART_RX,	/*  0 */
	IRQ_MASK_UART_TX,	/*  1 */
	IRQ_MASK_TIMER1,	/*  2 */
	IRQ_MASK_TIMER2,	/*  3 */
	IRQ_MASK_TIMER3,	/*  4 */
	IRQ_MASK_IN0,		/*  5 */
	IRQ_MASK_IN1,		/*  6 */
	IRQ_MASK_IN2,		/*  7 */
	IRQ_MASK_IN3,		/*  8 */
	IRQ_MASK_DOORBELLHOST,	/*  9 */
	IRQ_MASK_DMA1,		/* 10 */
	IRQ_MASK_DMA2,		/* 11 */
	IRQ_MASK_PCI,		/* 12 */
	IRQ_MASK_SDRAMPARITY,	/* 13 */
	IRQ_MASK_I2OINPOST,	/* 14 */
	IRQ_MASK_PCI_ABORT,	/* 15 */
	IRQ_MASK_PCI_SERR,	/* 16 */
	IRQ_MASK_DISCARD_TIMER,	/* 17 */
	IRQ_MASK_PCI_DPERR,	/* 18 */
	IRQ_MASK_PCI_PERR,	/* 19 */
};

static void fb_mask_irq(unsigned int irq)
{
	*CSR_IRQ_DISABLE = fb_irq_mask[_DC21285_INR(irq)];
}

static void fb_unmask_irq(unsigned int irq)
{
	*CSR_IRQ_ENABLE = fb_irq_mask[_DC21285_INR(irq)];
}

static void __init __fb_init_irq(void)
{
	int irq;

	/*
	 * setup DC21285 IRQs
	 */
	*CSR_IRQ_DISABLE = -1;
	*CSR_FIQ_DISABLE = -1;

	for (irq = _DC21285_IRQ(0); irq < _DC21285_IRQ(20); irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= fb_mask_irq;
		irq_desc[irq].mask	= fb_mask_irq;
		irq_desc[irq].unmask	= fb_unmask_irq;
	}
}

extern int isa_irq;

static void isa_mask_pic_lo_irq(unsigned int irq)
{
	unsigned int mask = 1 << (irq & 7);

	outb(inb(PIC_MASK_LO) | mask, PIC_MASK_LO);
}

static void isa_mask_ack_pic_lo_irq(unsigned int irq)
{
	unsigned int mask = 1 << (irq & 7);

	outb(inb(PIC_MASK_LO) | mask, PIC_MASK_LO);
	outb(0x20, PIC_LO);
}

static void isa_unmask_pic_lo_irq(unsigned int irq)
{
	unsigned int mask = 1 << (irq & 7);

	outb(inb(PIC_MASK_LO) & ~mask, PIC_MASK_LO);
}

static void isa_mask_pic_hi_irq(unsigned int irq)
{
	unsigned int mask = 1 << (irq & 7);

	outb(inb(PIC_MASK_HI) | mask, PIC_MASK_HI);
}

static void isa_mask_ack_pic_hi_irq(unsigned int irq)
{
	unsigned int mask = 1 << (irq & 7);

	outb(inb(PIC_MASK_HI) | mask, PIC_MASK_HI);
	outb(0x62, PIC_LO);
	outb(0x20, PIC_HI);
}

static void isa_unmask_pic_hi_irq(unsigned int irq)
{
	unsigned int mask = 1 << (irq & 7);

	outb(inb(PIC_MASK_HI) & ~mask, PIC_MASK_HI);
}

static void no_action(int irq, void *dev_id, struct pt_regs *regs)
{
}

static struct irqaction irq_cascade = { handler: no_action, name: "cascade", };
static struct resource pic1_resource = { "pic1", 0x20, 0x3f };
static struct resource pic2_resource = { "pic2", 0xa0, 0xbf };

static void __init isa_init_irq(int irq)
{
	/*
	 * Setup, and then probe for an ISA PIC
	 * If the PIC is not there, then we
	 * ignore the PIC.
	 */
	outb(0x11, PIC_LO);
	outb(_ISA_IRQ(0), PIC_MASK_LO);	/* IRQ number		*/
	outb(0x04, PIC_MASK_LO);	/* Slave on Ch2		*/
	outb(0x01, PIC_MASK_LO);	/* x86			*/
	outb(0xf5, PIC_MASK_LO);	/* pattern: 11110101	*/

	outb(0x11, PIC_HI);
	outb(_ISA_IRQ(8), PIC_MASK_HI);	/* IRQ number		*/
	outb(0x02, PIC_MASK_HI);	/* Slave on Ch1		*/
	outb(0x01, PIC_MASK_HI);	/* x86			*/
	outb(0xfa, PIC_MASK_HI);	/* pattern: 11111010	*/

	outb(0x0b, PIC_LO);
	outb(0x0b, PIC_HI);

	if (inb(PIC_MASK_LO) == 0xf5 && inb(PIC_MASK_HI) == 0xfa) {
		outb(0xff, PIC_MASK_LO);/* mask all IRQs	*/
		outb(0xff, PIC_MASK_HI);/* mask all IRQs	*/
		isa_irq = irq;
	} else
		isa_irq = -1;

	if (isa_irq != -1) {
		for (irq = _ISA_IRQ(0); irq < _ISA_IRQ(8); irq++) {
			irq_desc[irq].valid	= 1;
			irq_desc[irq].probe_ok	= 1;
			irq_desc[irq].mask_ack	= isa_mask_ack_pic_lo_irq;
			irq_desc[irq].mask	= isa_mask_pic_lo_irq;
			irq_desc[irq].unmask	= isa_unmask_pic_lo_irq;
		}

		for (irq = _ISA_IRQ(8); irq < _ISA_IRQ(16); irq++) {
			irq_desc[irq].valid	= 1;
			irq_desc[irq].probe_ok	= 1;
			irq_desc[irq].mask_ack	= isa_mask_ack_pic_hi_irq;
			irq_desc[irq].mask	= isa_mask_pic_hi_irq;
			irq_desc[irq].unmask	= isa_unmask_pic_hi_irq;
		}

		request_resource(&ioport_resource, &pic1_resource);
		request_resource(&ioport_resource, &pic2_resource);
		setup_arm_irq(IRQ_ISA_CASCADE, &irq_cascade);
		setup_arm_irq(isa_irq, &irq_cascade);

		/*
		 * On the NetWinder, don't automatically
		 * enable ISA IRQ11 when it is requested.
		 * There appears to be a missing pull-up
		 * resistor on this line.
		 */
		if (machine_is_netwinder())
			irq_desc[_ISA_IRQ(11)].noautoenable = 1;
	}
}

void __init footbridge_init_irq(void)
{
	__fb_init_irq();

	if (!footbridge_cfn_mode())
		return;

	if (machine_is_ebsa285())
		/* The following is dependent on which slot
		 * you plug the Southbridge card into.  We
		 * currently assume that you plug it into
		 * the right-hand most slot.
		 */
		isa_init_irq(IRQ_PCI);

	if (machine_is_cats())
		isa_init_irq(IRQ_IN2);

	if (machine_is_netwinder())
		isa_init_irq(IRQ_IN3);
}
