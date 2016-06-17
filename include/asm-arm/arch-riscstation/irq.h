/*
 *  linux/include/asm-arm/arch-rpc/irq.h
 *
 *  Copyright (C) 1996 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   10-10-1996	RMK	Brought up to date with arch-sa110eval
 *   22-08-1998	RMK	Restructured IRQ routines
 */
#include <asm/hardware/iomd.h>
#include <asm/io.h>

#define fixup_irq(x) (x)

static void rpc_mask_irq_ack_a(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << irq;
	val = iomd_readb(IOMD_IRQMASKA);
	iomd_writeb(val & ~mask, IOMD_IRQMASKA);
	iomd_writeb(mask, IOMD_IRQCLRA);
}

static void rpc_mask_irq_a(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << irq;
	val = iomd_readb(IOMD_IRQMASKA);
	iomd_writeb(val & ~mask, IOMD_IRQMASKA);
}

static void rpc_unmask_irq_a(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << irq;
	val = iomd_readb(IOMD_IRQMASKA);
	iomd_writeb(val | mask, IOMD_IRQMASKA);
}

static void rpc_mask_irq_b(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_IRQMASKB);
	iomd_writeb(val & ~mask, IOMD_IRQMASKB);
}

static void rpc_unmask_irq_b(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_IRQMASKB);
	iomd_writeb(val | mask, IOMD_IRQMASKB);
}



static void rpc_mask_irq_c(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_IRQMASKC);
	iomd_writeb(val & ~mask, IOMD_IRQMASKC);
}

static void rpc_unmask_irq_c(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_IRQMASKC);
	iomd_writeb(val | mask, IOMD_IRQMASKC);
}

static void rpc_mask_irq_d(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_IRQMASKD);
	iomd_writeb(val & ~mask, IOMD_IRQMASKD);
}

static void rpc_unmask_irq_d(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_IRQMASKD);
	iomd_writeb(val | mask, IOMD_IRQMASKD);
}

static void rpc_mask_irq_dma(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_DMAMASK);
	iomd_writeb(val & ~mask, IOMD_DMAMASK);
}

static void rpc_unmask_irq_dma(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_DMAMASK);
	iomd_writeb(val | mask, IOMD_DMAMASK);
}

static void rpc_mask_irq_fiq(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_FIQMASK);
	iomd_writeb(val & ~mask, IOMD_FIQMASK);
}

static void rpc_unmask_irq_fiq(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_FIQMASK);
	iomd_writeb(val | mask, IOMD_FIQMASK);
}

static __inline__ void irq_init_irq(void)
{
	int irq;

	iomd_writeb(0, IOMD_IRQMASKA);
	iomd_writeb(0, IOMD_IRQMASKB);
	iomd_writeb(0, IOMD_IRQMASKC);
	iomd_writeb(0, IOMD_IRQMASKD);

	iomd_writeb(0xff, IOMD_IOLINES);

	iomd_writeb(0, IOMD_FIQMASK);
	iomd_writeb(0, IOMD_DMAMASK);

	for (irq = 0; irq < NR_IRQS; irq++) {
		switch (irq) {
		case 0 ... 6:
			irq_desc[irq].probe_ok = 1;
		case 7:
			irq_desc[irq].valid    = 1;
			irq_desc[irq].mask_ack = rpc_mask_irq_ack_a;
			irq_desc[irq].mask     = rpc_mask_irq_a;
			irq_desc[irq].unmask   = rpc_unmask_irq_a;
			break;

		case 9 ... 15:
			irq_desc[irq].probe_ok = 1;
		case 8:
			irq_desc[irq].valid    = 1;
			irq_desc[irq].mask_ack = rpc_mask_irq_b;
			irq_desc[irq].mask     = rpc_mask_irq_b;
			irq_desc[irq].unmask   = rpc_unmask_irq_b;
			break;

		case 16 ... 19:
		case 21:
			irq_desc[irq].noautoenable = 1;
		case 20:
			irq_desc[irq].valid    = 1;
			irq_desc[irq].mask_ack = rpc_mask_irq_dma;
			irq_desc[irq].mask     = rpc_mask_irq_dma;
			irq_desc[irq].unmask   = rpc_unmask_irq_dma;
			break;

		case 24 ... 31:
		        irq_desc[irq].valid     = 1;
			irq_desc[irq].mask_ack  = rpc_mask_irq_c;
			irq_desc[irq].mask 	= rpc_mask_irq_c;
			irq_desc[irq].unmask	= rpc_unmask_irq_c;
			break;

		case 40 ... 47:
		        irq_desc[irq].valid     = 1;
			irq_desc[irq].mask_ack  = rpc_mask_irq_d;
			irq_desc[irq].mask 	= rpc_mask_irq_d;
			irq_desc[irq].unmask	= rpc_unmask_irq_d;
			break;

		case 64 ... 71:
			irq_desc[irq].valid    = 1;
			irq_desc[irq].mask_ack = rpc_mask_irq_fiq;
			irq_desc[irq].mask     = rpc_mask_irq_fiq;
			irq_desc[irq].unmask   = rpc_unmask_irq_fiq;
			break;
		}
	}

	irq_desc[IRQ_KEYBOARDTX].noautoenable = 1;

	init_FIQ();
}
