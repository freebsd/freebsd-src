/*
 * include/asm-arm/arch-cl7500/irq.h
 *
 * Copyright (C) 1996 Russell King
 * Copyright (C) 1999, 2001 Nexus Electronics Ltd.
 *
 * Changelog:
 *   10-10-1996	RMK	Brought up to date with arch-sa110eval
 *   22-08-1998	RMK	Restructured IRQ routines
 *   11-08-1999	PJB	Created ARM7500 version, derived from RiscPC code
 */

#include <asm/hardware/iomd.h>
#include <asm/io.h>

static inline int fixup_irq(unsigned int irq)
{
	if (irq == IRQ_ISA) {
		int isabits = *((volatile unsigned int *)0xe002b700);
		if (isabits == 0) {
			printk("Spurious ISA IRQ!\n");
			return irq;
		}
		irq = IRQ_ISA_BASE;
		while (!(isabits & 1)) {
			irq++;
			isabits >>= 1;
		}
	}

	return irq;
}

static void cl7500_mask_irq_ack_a(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << irq;
	val = iomd_readb(IOMD_IRQMASKA);
	iomd_writeb(val & ~mask, IOMD_IRQMASKA);
	iomd_writeb(mask, IOMD_IRQCLRA);
}

static void cl7500_mask_irq_a(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << irq;
	val = iomd_readb(IOMD_IRQMASKA);
	iomd_writeb(val & ~mask, IOMD_IRQMASKA);
}

static void cl7500_unmask_irq_a(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << irq;
	val = iomd_readb(IOMD_IRQMASKA);
	iomd_writeb(val | mask, IOMD_IRQMASKA);
}

static void cl7500_mask_irq_b(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_IRQMASKB);
	iomd_writeb(val & ~mask, IOMD_IRQMASKB);
}

static void cl7500_unmask_irq_b(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_IRQMASKB);
	iomd_writeb(val | mask, IOMD_IRQMASKB);
}

static void cl7500_mask_irq_c(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_IRQMASKC);
	iomd_writeb(val & ~mask, IOMD_IRQMASKC);
}

static void cl7500_unmask_irq_c(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_IRQMASKC);
	iomd_writeb(val | mask, IOMD_IRQMASKC);
}


static void cl7500_mask_irq_d(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_IRQMASKD);
	iomd_writeb(val & ~mask, IOMD_IRQMASKD);
}

static void cl7500_unmask_irq_d(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_IRQMASKD);
	iomd_writeb(val | mask, IOMD_IRQMASKD);
}

static void cl7500_mask_irq_dma(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_DMAMASK);
	iomd_writeb(val & ~mask, IOMD_DMAMASK);
}

static void cl7500_unmask_irq_dma(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_DMAMASK);
	iomd_writeb(val | mask, IOMD_DMAMASK);
}

static void cl7500_mask_irq_fiq(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_FIQMASK);
	iomd_writeb(val & ~mask, IOMD_FIQMASK);
}

static void cl7500_unmask_irq_fiq(unsigned int irq)
{
	unsigned int val, mask;

	mask = 1 << (irq & 7);
	val = iomd_readb(IOMD_FIQMASK);
	iomd_writeb(val | mask, IOMD_FIQMASK);
}

static void no_action(int cpl, void *dev_id, struct pt_regs *regs)
{
}

static struct irqaction irq_isa = { no_action, 0, 0, "isa", NULL, NULL };

static __inline__ void irq_init_irq(void)
{
	int irq;

	iomd_writeb(0, IOMD_IRQMASKA);
	iomd_writeb(0, IOMD_IRQMASKB);
	iomd_writeb(0, IOMD_FIQMASK);
	iomd_writeb(0, IOMD_DMAMASK);

	for (irq = 0; irq < NR_IRQS; irq++) {
		switch (irq) {
		case 0 ... 6:
			irq_desc[irq].probe_ok = 1;
		case 7:
			irq_desc[irq].valid    = 1;
			irq_desc[irq].mask_ack = cl7500_mask_irq_ack_a;
			irq_desc[irq].mask     = cl7500_mask_irq_a;
			irq_desc[irq].unmask   = cl7500_unmask_irq_a;
			break;

		case 9 ... 15:
			irq_desc[irq].probe_ok = 1;
		case 8:
			irq_desc[irq].valid    = 1;
			irq_desc[irq].mask_ack = cl7500_mask_irq_b;
			irq_desc[irq].mask     = cl7500_mask_irq_b;
			irq_desc[irq].unmask   = cl7500_unmask_irq_b;
			break;

		case 16 ... 22:
			irq_desc[irq].valid    = 1;
			irq_desc[irq].mask_ack = cl7500_mask_irq_dma;
			irq_desc[irq].mask     = cl7500_mask_irq_dma;
			irq_desc[irq].unmask   = cl7500_unmask_irq_dma;
			break;

		case 24 ... 31:
			irq_desc[irq].valid    = 1;
			irq_desc[irq].mask_ack = cl7500_mask_irq_c;
			irq_desc[irq].mask     = cl7500_mask_irq_c;
			irq_desc[irq].unmask   = cl7500_unmask_irq_c;
			break;

		case 40 ... 47:
			irq_desc[irq].valid    = 1;
			irq_desc[irq].mask_ack = cl7500_mask_irq_d;
			irq_desc[irq].mask     = cl7500_mask_irq_d;
			irq_desc[irq].unmask   = cl7500_unmask_irq_d;
			break;

		case 48 ... 55:
			irq_desc[irq].valid      = 1;
			irq_desc[irq].probe_ok   = 1;
			irq_desc[irq].mask_ack   = no_action;
			irq_desc[irq].mask       = no_action;
			irq_desc[irq].unmask     = no_action;
			break;

		case 64 ... 72:
			irq_desc[irq].valid    = 1;
			irq_desc[irq].mask_ack = cl7500_mask_irq_fiq;
			irq_desc[irq].mask     = cl7500_mask_irq_fiq;
			irq_desc[irq].unmask   = cl7500_unmask_irq_fiq;
			break;
		}
	}

	setup_arm_irq(IRQ_ISA, &irq_isa);
}
