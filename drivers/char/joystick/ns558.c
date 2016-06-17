/*
 * $Id: ns558.c,v 1.29 2001/04/24 07:48:56 vojtech Exp $
 *
 *  Copyright (c) 1999-2001 Vojtech Pavlik
 *  Copyright (c) 1999 Brian Gerst
 *
 *  Sponsored by SuSE
 */

/*
 * NS558 based standard IBM game port driver for Linux
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * 
 * Should you need to contact me, the author, you can do so either by
 * e-mail - mail your message to <vojtech@ucw.cz>, or by paper mail:
 * Vojtech Pavlik, Ucitelska 1576, Prague 8, 182 00 Czech Republic
 */

#include <asm/io.h>

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/gameport.h>
#include <linux/slab.h>
#include <linux/isapnp.h>

MODULE_AUTHOR("Vojtech Pavlik <vojtech@ucw.cz>");
MODULE_LICENSE("GPL");

#define NS558_ISA	1
#define NS558_PNP	2

static int ns558_isa_portlist[] = { 0x200, 0x201, 0x202, 0x203, 0x204, 0x205, 0x207, 0x209,
				    0x20b, 0x20c, 0x20e, 0x20f, 0x211, 0x219, 0x101, 0 };

struct ns558 {
	int type;
	int size;
	struct pci_dev *dev;
	struct ns558 *next;
	struct gameport gameport;
};
	
static struct ns558 *ns558;

/*
 * ns558_isa_probe() tries to find an isa gameport at the
 * specified address, and also checks for mirrors.
 * A joystick must be attached for this to work.
 */

static struct ns558* ns558_isa_probe(int io, struct ns558 *next)
{
	int i, j, b;
	unsigned char c, u, v;
	struct ns558 *port;

/*
 * No one should be using this address.
 */

	if (check_region(io, 1))
		return next;

/*
 * We must not be able to write arbitrary values to the port.
 * The lower two axis bits must be 1 after a write.
 */

	c = inb(io);
	outb(~c & ~3, io);
	if (~(u = v = inb(io)) & 3) {
		outb(c, io);
		return next;
	}
/*
 * After a trigger, there must be at least some bits changing.
 */

	for (i = 0; i < 1000; i++) v &= inb(io);

	if (u == v) {
		outb(c, io);
		return next;
	}
	wait_ms(3);
/*
 * After some time (4ms) the axes shouldn't change anymore.
 */

	u = inb(io);
	for (i = 0; i < 1000; i++)
		if ((u ^ inb(io)) & 0xf) {
			outb(c, io);
			return next;
		}
/* 
 * And now find the number of mirrors of the port.
 */

	for (i = 1; i < 5; i++) {

		if (check_region(io & (-1 << i), (1 << i)))	/* Don't disturb anyone */
			break;

		outb(0xff, io & (-1 << i));
		for (j = b = 0; j < 1000; j++)
			if (inb(io & (-1 << i)) != inb((io & (-1 << i)) + (1 << i) - 1)) b++;
		wait_ms(3);

		if (b > 300)					/* We allow 30% difference */
			break;
	}

	i--;

	if (!(port = kmalloc(sizeof(struct ns558), GFP_KERNEL))) {
		printk(KERN_ERR "ns558: Memory allocation failed.\n");
		return next;
	}
       	memset(port, 0, sizeof(struct ns558));
	
	port->next = next;
	port->type = NS558_ISA;
	port->size = (1 << i);
	port->gameport.io = io & (-1 << i);

	request_region(port->gameport.io, (1 << i), "ns558-isa");

	gameport_register_port(&port->gameport);

	printk(KERN_INFO "gameport%d: NS558 ISA at %#x", port->gameport.number, port->gameport.io);
	if (port->size > 1) printk(" size %d", port->size);
	printk(" speed %d kHz\n", port->gameport.speed);

	return port;
}

#if defined(CONFIG_ISAPNP) || (defined(CONFIG_ISAPNP_MODULE) && defined(MODULE))
#define NSS558_ISAPNP
#endif

#ifdef NSS558_ISAPNP

static struct isapnp_device_id pnp_devids[] = {
	{ ISAPNP_ANY_ID, ISAPNP_ANY_ID, ISAPNP_VENDOR('@','P','@'), ISAPNP_DEVICE(0x0001), 0 },
	{ ISAPNP_ANY_ID, ISAPNP_ANY_ID, ISAPNP_VENDOR('@','P','@'), ISAPNP_DEVICE(0x2001), 0 },
	{ ISAPNP_ANY_ID, ISAPNP_ANY_ID, ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x7001), 0 },
	{ ISAPNP_ANY_ID, ISAPNP_ANY_ID, ISAPNP_VENDOR('C','T','L'), ISAPNP_DEVICE(0x7002), 0 },
	{ ISAPNP_ANY_ID, ISAPNP_ANY_ID, ISAPNP_VENDOR('C','S','C'), ISAPNP_DEVICE(0x0010), 0 },
	{ ISAPNP_ANY_ID, ISAPNP_ANY_ID, ISAPNP_VENDOR('C','S','C'), ISAPNP_DEVICE(0x0110), 0 },
	{ ISAPNP_ANY_ID, ISAPNP_ANY_ID, ISAPNP_VENDOR('C','S','C'), ISAPNP_DEVICE(0x0b35), 0 },
	{ ISAPNP_ANY_ID, ISAPNP_ANY_ID, ISAPNP_VENDOR('C','S','C'), ISAPNP_DEVICE(0x0010), 0 },
	{ ISAPNP_ANY_ID, ISAPNP_ANY_ID, ISAPNP_VENDOR('C','S','C'), ISAPNP_DEVICE(0x0110), 0 },
	{ ISAPNP_ANY_ID, ISAPNP_ANY_ID, ISAPNP_VENDOR('P','N','P'), ISAPNP_DEVICE(0xb02f), 0 },
	{ 0, },
};

MODULE_DEVICE_TABLE(isapnp, pnp_devids);

static struct ns558* ns558_pnp_probe(struct pci_dev *dev, struct ns558 *next)
{
	int ioport, iolen;
	struct ns558 *port;

	if (dev->prepare && dev->prepare(dev) < 0)
		return next;

	if (!(dev->resource[0].flags & IORESOURCE_IO)) {
		printk(KERN_WARNING "ns558: No i/o ports on a gameport? Weird\n");
		return next;
	}

	if (dev->activate && dev->activate(dev) < 0) {
		printk(KERN_ERR "ns558: PnP resource allocation failed\n");
		return next;
	}
	
	ioport = pci_resource_start(dev, 0);
	iolen = pci_resource_len(dev, 0);

	if (!request_region(ioport, iolen, "ns558-pnp"))
		goto deactivate;

	if (!(port = kmalloc(sizeof(struct ns558), GFP_KERNEL))) {
		printk(KERN_ERR "ns558: Memory allocation failed.\n");
		goto deactivate;
	}
	memset(port, 0, sizeof(struct ns558));

	port->next = next;
	port->type = NS558_PNP;
	port->gameport.io = ioport;
	port->size = iolen;
	port->dev = dev;

	gameport_register_port(&port->gameport);

	printk(KERN_INFO "gameport%d: NS558 PnP at %#x", port->gameport.number, port->gameport.io);
	if (iolen > 1) printk(" size %d", iolen);
	printk(" speed %d kHz\n", port->gameport.speed);

	return port;

deactivate:
	if (dev->deactivate)
		dev->deactivate(dev);
	return next;
}
#endif

int __init ns558_init(void)
{
	int i = 0;
#ifdef NSS558_ISAPNP
	struct isapnp_device_id *devid;
	struct pci_dev *dev = NULL;
#endif

/*
 * Probe for ISA ports.
 */

	while (ns558_isa_portlist[i]) 
		ns558 = ns558_isa_probe(ns558_isa_portlist[i++], ns558);

/*
 * Probe for PnP ports.
 */

#ifdef NSS558_ISAPNP
	for (devid = pnp_devids; devid->vendor; devid++) {
		while ((dev = isapnp_find_dev(NULL, devid->vendor, devid->function, dev))) {
			ns558 = ns558_pnp_probe(dev, ns558);
		}
	}
#endif

	return ns558 ? 0 : -ENODEV;
}

void __exit ns558_exit(void)
{
	struct ns558 *next, *port = ns558;

	while (port) {
		gameport_unregister_port(&port->gameport);
		switch (port->type) {

#ifdef NSS558_ISAPNP
			case NS558_PNP:
				if (port->dev->deactivate)
					port->dev->deactivate(port->dev);
				/* fall through */
#endif

			case NS558_ISA:
				release_region(port->gameport.io, port->size);
				break;
		
			default:
				break;
		}
		
		next = port->next;
		kfree(port);
		port = next;
	}
}

module_init(ns558_init);
module_exit(ns558_exit);
