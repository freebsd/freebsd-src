/*
 * include/asm-arm/arch-l7200/irq.h
 *
 * Copyright (C) 2000 Rob Scott (rscott@mtrob.fdns.ne
 *                    Steve Hill (sjhill@cotw.com)
 *
 * Changelog:
 *   01-02-2000	RS	Created l7200 version, derived from ebsa110 code
 *   04-15-2000 RS      Made dependent on hardware.h
 *   05-05-2000 SJH     Complete rewrite
 */

#include <asm/arch/hardware.h>

/*
 * IRQ base register
 */
#define	IRQ_BASE	(IO_BASE_2 + 0x1000)

/* 
 * Normal IRQ registers
 */
#define IRQ_STATUS	(*(volatile unsigned long *) (IRQ_BASE + 0x000))
#define IRQ_RAWSTATUS	(*(volatile unsigned long *) (IRQ_BASE + 0x004))
#define IRQ_ENABLE	(*(volatile unsigned long *) (IRQ_BASE + 0x008))
#define IRQ_ENABLECLEAR	(*(volatile unsigned long *) (IRQ_BASE + 0x00c))
#define IRQ_SOFT	(*(volatile unsigned long *) (IRQ_BASE + 0x010))
#define IRQ_SOURCESEL	(*(volatile unsigned long *) (IRQ_BASE + 0x018))

/* 
 * Fast IRQ registers
 */
#define FIQ_STATUS	(*(volatile unsigned long *) (IRQ_BASE + 0x100))
#define FIQ_RAWSTATUS	(*(volatile unsigned long *) (IRQ_BASE + 0x104))
#define FIQ_ENABLE	(*(volatile unsigned long *) (IRQ_BASE + 0x108))
#define FIQ_ENABLECLEAR	(*(volatile unsigned long *) (IRQ_BASE + 0x10c))
#define FIQ_SOFT	(*(volatile unsigned long *) (IRQ_BASE + 0x110))
#define FIQ_SOURCESEL	(*(volatile unsigned long *) (IRQ_BASE + 0x118))

#define fixup_irq(x) (x)

static void l7200_mask_irq(unsigned int irq)
{
	IRQ_ENABLECLEAR = 1 << irq;
}

static void l7200_unmask_irq(unsigned int irq)
{
	IRQ_ENABLE = 1 << irq;
}
 
static __inline__ void irq_init_irq(void)
{
	int irq;

	IRQ_ENABLECLEAR = 0xffffffff;	/* clear all interrupt enables */
	FIQ_ENABLECLEAR = 0xffffffff;	/* clear all fast interrupt enables */

	for (irq = 0; irq < NR_IRQS; irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= l7200_mask_irq;
		irq_desc[irq].mask	= l7200_mask_irq;
		irq_desc[irq].unmask	= l7200_unmask_irq;
	}

	init_FIQ();
}
