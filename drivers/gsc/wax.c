/*
 *	WAX Device Driver
 *
 *	(c) Copyright 2000 The Puffin Group Inc.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *	by Helge Deller <deller@gmx.de>
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/gsc.h>
#include <asm/irq.h>

#include "busdevice.h"

#define WAX_GSC_IRQ	7	/* Hardcoded Interrupt for GSC */
#define WAX_GSC_NMI_IRQ	29

static int wax_choose_irq(struct parisc_device *dev)
{
	int irq = -1;

	switch (dev->id.sversion) {
		case 0x73:	irq = 30; break; /* HIL */
		case 0x8c:	irq = 25; break; /* RS232 */
		case 0x90:	irq = 21; break; /* WAX EISA BA */
	}

	return irq;
}

static void __init
wax_init_irq(struct busdevice *wax)
{
	unsigned long base = wax->hpa;

	/* Stop WAX barking for a bit */
	gsc_writel(0x00000000, base+OFFSET_IMR);

	/* clear pending interrupts */
	(volatile u32) gsc_readl(base+OFFSET_IRR);

	/* We're not really convinced we want to reset the onboard
         * devices. Firmware does it for us...
	 */

	/* Resets */
//	gsc_writel(0xFFFFFFFF, base+0x1000); /* HIL */
//	gsc_writel(0xFFFFFFFF, base+0x2000); /* RS232-B on Wax */
	
	/* Ok we hit it on the head with a hammer, our Dog is now
	** comatose and muzzled.  Devices will now unmask WAX
	** interrupts as they are registered as irq's in the WAX range.
	*/
}

int __init
wax_init_chip(struct parisc_device *dev)
{
	struct busdevice *wax;
	struct gsc_irq gsc_irq;
	int irq, ret;

	wax = kmalloc(sizeof(struct busdevice), GFP_KERNEL);
	if (!wax)
		return -ENOMEM;

	wax->name = "Wax";
	wax->hpa = dev->hpa;

	wax->version = 0;   /* gsc_readb(wax->hpa+WAX_VER); */
	printk(KERN_INFO "%s at 0x%lx found.\n", wax->name, wax->hpa);

	/* Stop wax hissing for a bit */
	wax_init_irq(wax);

	/* the IRQ wax should use */
	irq = gsc_claim_irq(&gsc_irq, WAX_GSC_IRQ);
	if (irq < 0) {
		printk(KERN_ERR "%s(): cannot get GSC irq\n",
				__FUNCTION__);
		kfree(wax);
		return -EBUSY;
	}

	ret = request_irq(gsc_irq.irq, busdev_barked, 0, "wax", wax);
	if (ret < 0) {
		kfree(wax);
		return ret;
	}

	/* Save this for debugging later */
	wax->parent_irq = gsc_irq.irq;
	wax->eim = ((u32) gsc_irq.txn_addr) | gsc_irq.txn_data;

	/* enable IRQ's for devices below WAX */
	gsc_writel(wax->eim, wax->hpa + OFFSET_IAR);

	/* Done init'ing, register this driver */
	ret = gsc_common_irqsetup(dev, wax);
	if (ret) {
		kfree(wax);
		return ret;
	}

	fixup_child_irqs(dev, wax->busdev_region->data.irqbase,
			wax_choose_irq);
	/* On 715-class machines, Wax EISA is a sibling of Wax, not a child. */
	if (dev->parent->id.hw_type != HPHW_IOA) {
		fixup_child_irqs(dev->parent, wax->busdev_region->data.irqbase,
				wax_choose_irq);
	}

	return ret;
}

static struct parisc_device_id wax_tbl[] = {
  	{ HPHW_BA, HVERSION_REV_ANY_ID, HVERSION_ANY_ID, 0x0008e },
	{ 0, }
};

MODULE_DEVICE_TABLE(parisc, wax_tbl);

struct parisc_driver wax_driver = {
	name:		"Wax",
	id_table:	wax_tbl,
	probe:		wax_init_chip,
};
