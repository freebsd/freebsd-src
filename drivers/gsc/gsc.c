/*
 * Interrupt management for most GSC and related devices.
 *
 * (c) Copyright 1999 Alex deVries for The Puffin Group
 * (c) Copyright 1999 Grant Grundler for Hewlett-Packard
 * (c) Copyright 1999 Matthew Wilcox
 * (c) Copyright 2000 Helge Deller
 * (c) Copyright 2001 Matthew Wilcox for Hewlett-Packard
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 */

#include <linux/bitops.h>
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <asm/gsc.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>

#include "busdevice.h"

#undef DEBUG

#ifdef DEBUG
#define DEBPRINTK printk
#else
#define DEBPRINTK(x,...)
#endif

int gsc_alloc_irq(struct gsc_irq *i)
{
	int irq = txn_alloc_irq();
	if (irq < 0) {
		printk("cannot get irq\n");
		return irq;
	}

	i->txn_addr = txn_alloc_addr(irq);
	i->txn_data = txn_alloc_data(irq, GSC_EIM_WIDTH);
	i->irq = irq;

	return irq;
}


int gsc_claim_irq(struct gsc_irq *i, int irq)
{
	int c = irq;

	irq += IRQ_FROM_REGION(CPU_IRQ_REGION); /* virtualize the IRQ first */

	irq = txn_claim_irq(irq);
	if (irq < 0) {
		printk("cannot claim irq %d\n", c);
		return irq;
	}

	i->txn_addr = txn_alloc_addr(irq);
	i->txn_data = txn_alloc_data(irq, GSC_EIM_WIDTH);
	i->irq = irq;

	return irq;
}


/* IRQ bits must be numbered from Most Significant Bit */
#define GSC_FIX_IRQ(x)	(31-(x))
#define GSC_MASK_IRQ(x)	(1<<(GSC_FIX_IRQ(x)))

/* Common interrupt demultiplexer used by Asp, Lasi & Wax.  */
void busdev_barked(int busdev_irq, void *dev, struct pt_regs *regs)
{
	unsigned long irq;
	struct busdevice *busdev = (struct busdevice *) dev;

	/* 
	    Don't need to protect OFFSET_IRR with spinlock since this is
	    the only place it's touched.
	    Protect busdev_region by disabling this region's interrupts,
	    modifying the region, and then re-enabling the region.
	*/

	irq = gsc_readl(busdev->hpa+OFFSET_IRR);
	if (irq == 0) {
		printk(KERN_ERR "%s: barking without apparent reason.\n", busdev->name);
	} else {
		DEBPRINTK ("%s (0x%x) barked, mask=0x%x, irq=%d\n", 
		    busdev->name, busdev->busdev_region->data.irqbase, 
		    irq, GSC_FIX_IRQ(ffs(irq))+1 );

		do_irq_mask(irq, busdev->busdev_region, regs);
	}
}

static void
busdev_disable_irq(void *irq_dev, int irq)
{
	/* Disable the IRQ line by clearing the bit in the IMR */
	u32 imr = gsc_readl(BUSDEV_DEV(irq_dev)->hpa+OFFSET_IMR);
	imr &= ~(GSC_MASK_IRQ(irq));

	DEBPRINTK( KERN_WARNING "%s(%p, %d) %s: IMR 0x%x\n", 
		    __FUNCTION__, irq_dev, irq, BUSDEV_DEV(irq_dev)->name, imr);

	gsc_writel(imr, BUSDEV_DEV(irq_dev)->hpa+OFFSET_IMR);
}


static void
busdev_enable_irq(void *irq_dev, int irq)
{
	/* Enable the IRQ line by setting the bit in the IMR */
	unsigned long addr = BUSDEV_DEV(irq_dev)->hpa + OFFSET_IMR;
	u32 imr = gsc_readl(addr);
	imr |= GSC_MASK_IRQ(irq);

	DEBPRINTK (KERN_WARNING "%s(%p, %d) %s: IMR 0x%x\n", 
		    __FUNCTION__, irq_dev, irq, BUSDEV_DEV(irq_dev)->name, imr);

	gsc_writel(imr, addr);
//	gsc_writel(~0L, addr);

/* FIXME: read IPR to make sure the IRQ isn't already pending.
**   If so, we need to read IRR and manually call do_irq_mask().
**   This code should be shared with busdev_unmask_irq().
*/
}

static void
busdev_mask_irq(void *irq_dev, int irq)
{
/* FIXME: Clear the IMR bit in busdev for that IRQ */
}

static void
busdev_unmask_irq(void *irq_dev, int irq)
{
/* FIXME: Read IPR. Set the IMR bit in busdev for that IRQ.
   call do_irq_mask() if IPR is non-zero
*/
}

struct irq_region_ops busdev_irq_ops = {
	disable_irq:	busdev_disable_irq,
	enable_irq:	busdev_enable_irq,
	mask_irq:	busdev_mask_irq,
	unmask_irq:	busdev_unmask_irq
};


int gsc_common_irqsetup(struct parisc_device *parent, struct busdevice *busdev)
{
	struct resource *res;

	busdev->gsc = parent;

	/* the IRQs we simulate */
	busdev->busdev_region = alloc_irq_region(32, &busdev_irq_ops,
						 busdev->name, busdev);
	if (!busdev->busdev_region)
		return -ENOMEM;

	/* allocate resource region */
	res = request_mem_region(busdev->hpa, 0x100000, busdev->name);
	if (res) {
		res->flags = IORESOURCE_MEM; 	/* do not mark it busy ! */
	}

#if 0
	printk(KERN_WARNING "%s IRQ %d EIM 0x%x", busdev->name,
			busdev->parent_irq, busdev->eim);
	if (gsc_readl(busdev->hpa + OFFSET_IMR))
		printk("  IMR is non-zero! (0x%x)",
				gsc_readl(busdev->hpa + OFFSET_IMR));
	printk("\n");
#endif

	return 0;
}

extern struct parisc_driver lasi_driver;
extern struct parisc_driver asp_driver;
extern struct parisc_driver wax_driver;

void __init gsc_init(void)
{
#ifdef CONFIG_GSC_LASI
	register_parisc_driver(&lasi_driver);
	register_parisc_driver(&asp_driver);
#endif
#ifdef CONFIG_GSC_WAX
	register_parisc_driver(&wax_driver);
#endif
}
