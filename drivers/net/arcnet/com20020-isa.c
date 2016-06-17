/*
 * Linux ARCnet driver - COM20020 chipset support
 * 
 * Written 1997 by David Woodhouse.
 * Written 1994-1999 by Avery Pennarun.
 * Written 1999-2000 by Martin Mares <mj@ucw.cz>.
 * Derived from skeleton.c by Donald Becker.
 *
 * Special thanks to Contemporary Controls, Inc. (www.ccontrols.com)
 *  for sponsoring the further development of this driver.
 *
 * **********************
 *
 * The original copyright of skeleton.c was as follows:
 *
 * skeleton.c Written 1993 by Donald Becker.
 * Copyright 1993 United States Government as represented by the
 * Director, National Security Agency.  This software may only be used
 * and distributed according to the terms of the GNU General Public License as
 * modified by SRC, incorporated herein by reference.
 *
 * **********************
 *
 * For more details, see drivers/net/arcnet.c
 *
 * **********************
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/arcdevice.h>
#include <linux/com20020.h>

#include <asm/io.h>


#define VERSION "arcnet: COM20020 ISA support (by David Woodhouse et al.)\n"


/*
 * We cannot (yet) probe for an IO mapped card, although we can check that
 * it's where we were told it was, and even do autoirq.
 */
static int __init com20020isa_probe(struct net_device *dev)
{
	int ioaddr;
	unsigned long airqmask;
	struct arcnet_local *lp = dev->priv;

#ifndef MODULE
	arcnet_init();
#endif

	BUGLVL(D_NORMAL) printk(VERSION);

	ioaddr = dev->base_addr;
	if (!ioaddr) {
		BUGMSG(D_NORMAL, "No autoprobe (yet) for IO mapped cards; you "
		       "must specify the base address!\n");
		return -ENODEV;
	}
	if (check_region(ioaddr, ARCNET_TOTAL_SIZE)) {
		BUGMSG(D_NORMAL, "IO region %xh-%xh already allocated.\n",
		       ioaddr, ioaddr + ARCNET_TOTAL_SIZE - 1);
		return -ENXIO;
	}
	if (ASTATUS() == 0xFF) {
		BUGMSG(D_NORMAL, "IO address %x empty\n", ioaddr);
		return -ENODEV;
	}
	if (com20020_check(dev))
		return -ENODEV;

	if (!dev->irq) {
		/* if we do this, we're sure to get an IRQ since the
		 * card has just reset and the NORXflag is on until
		 * we tell it to start receiving.
		 */
		BUGMSG(D_INIT_REASONS, "intmask was %02Xh\n", inb(_INTMASK));
		outb(0, _INTMASK);
		airqmask = probe_irq_on();
		outb(NORXflag, _INTMASK);
		udelay(1);
		outb(0, _INTMASK);
		dev->irq = probe_irq_off(airqmask);

		if (dev->irq <= 0) {
			BUGMSG(D_INIT_REASONS, "Autoprobe IRQ failed first time\n");
			airqmask = probe_irq_on();
			outb(NORXflag, _INTMASK);
			udelay(5);
			outb(0, _INTMASK);
			dev->irq = probe_irq_off(airqmask);
			if (dev->irq <= 0) {
				BUGMSG(D_NORMAL, "Autoprobe IRQ failed.\n");
				return -ENODEV;
			}
		}
	}

	lp->card_name = "ISA COM20020";
	return com20020_found(dev, 0);
}


#ifdef MODULE

static struct net_device *my_dev;

/* Module parameters */

static int node = 0;
static int io = 0x0;		/* <--- EDIT THESE LINES FOR YOUR CONFIGURATION */
static int irq = 0;		/* or use the insmod io= irq= shmem= options */
static char *device;		/* use eg. device="arc1" to change name */
static int timeout = 3;
static int backplane = 0;
static int clockp = 0;
static int clockm = 0;

MODULE_PARM(node, "i");
MODULE_PARM(io, "i");
MODULE_PARM(irq, "i");
MODULE_PARM(device, "s");
MODULE_PARM(timeout, "i");
MODULE_PARM(backplane, "i");
MODULE_PARM(clockp, "i");
MODULE_PARM(clockm, "i");
MODULE_LICENSE("GPL");

static void com20020isa_open_close(struct net_device *dev, bool open)
{
	if (open)
		MOD_INC_USE_COUNT;
	else
		MOD_DEC_USE_COUNT;
}

int init_module(void)
{
	struct net_device *dev;
	struct arcnet_local *lp;
	int err;

	dev = dev_alloc(device ? : "arc%d", &err);
	if (!dev)
		return err;
	lp = dev->priv = kmalloc(sizeof(struct arcnet_local), GFP_KERNEL);
	if (!lp)
		return -ENOMEM;
	memset(lp, 0, sizeof(struct arcnet_local));

	if (node && node != 0xff)
		dev->dev_addr[0] = node;

	lp->backplane = backplane;
	lp->clockp = clockp & 7;
	lp->clockm = clockm & 3;
	lp->timeout = timeout & 3;
	lp->hw.open_close_ll = com20020isa_open_close;

	dev->base_addr = io;
	dev->irq = irq;

	if (dev->irq == 2)
		dev->irq = 9;

	if (com20020isa_probe(dev))
		return -EIO;

	my_dev = dev;
	return 0;
}

void cleanup_module(void)
{
	com20020_remove(my_dev);
}

#else

static int __init com20020isa_setup(char *s)
{
	struct net_device *dev;
	struct arcnet_local *lp;
	int ints[8];

	s = get_options(s, 8, ints);
	if (!ints[0])
		return 1;
	dev = alloc_bootmem(sizeof(struct net_device) + sizeof(struct arcnet_local));
	memset(dev, 0, sizeof(struct net_device) + sizeof(struct arcnet_local));
	lp = dev->priv = (struct arcnet_local *) (dev + 1);
	dev->init = com20020isa_probe;

	switch (ints[0]) {
	default:		/* ERROR */
		printk("com90xx: Too many arguments.\n");
	case 6:		/* Timeout */
		lp->timeout = ints[6];
	case 5:		/* CKP value */
		lp->clockp = ints[5];
	case 4:		/* Backplane flag */
		lp->backplane = ints[4];
	case 3:		/* Node ID */
		dev->dev_addr[0] = ints[3];
	case 2:		/* IRQ */
		dev->irq = ints[2];
	case 1:		/* IO address */
		dev->base_addr = ints[1];
	}
	if (*s)
		strncpy(dev->name, s, 9);
	else
		strcpy(dev->name, "arc%d");
	if (register_netdev(dev))
		printk(KERN_ERR "com20020: Cannot register arcnet device\n");

	return 1;
}

__setup("com20020=", com20020isa_setup);

#endif				/* MODULE */
