/*
 *  linux/drivers/acorn/char/serial-card.c
 *
 *  Copyright (C) 1996-1999 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * A generic handler of serial expansion cards that use 16550s or
 * the like.
 *
 * Definitions:
 *  MY_PRODS		Product numbers to identify this card by
 *  MY_MANUS		Manufacturer numbers to identify this card by
 *  MY_NUMPORTS		Number of ports per card
 *  MY_BAUD_BASE	Baud base for the card
 *  MY_INIT		Initialisation routine name
 *  MY_BASE_ADDRESS(ec)	Return base address for ports
 *  MY_PORT_ADDRESS
 *	(port,cardaddr)	Return address for port using base address
 *			from above.
 *
 * Changelog:
 *  30-07-1996	RMK	Created
 *  22-04-1998	RMK	Removed old register_pre_init_serial
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/tty.h>
#include <linux/serial_core.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/ecard.h>
#include <asm/string.h>

struct serial_card_info {
	unsigned int	num_ports;
	int		ports[MAX_PORTS];
};

static inline int
serial_register_onedev(unsigned long baddr, void *vaddr, int irq, unsigned int baud_base)
{
	struct serial_struct req;

	memset(&req, 0, sizeof(req));
	req.irq 		= irq;
	req.flags		= UPF_AUTOPROBE | UPF_RESOURCES |
				  UPF_SHARE_IRQ;
	req.baud_base		= baud_base;
	req.io_type		= UPIO_MEM;
	req.iomem_base		= vaddr;
	req.iomem_reg_shift	= 2;
	req.iomap_base		= baddr;

	return register_serial(&req);
}

static int __devinit
serial_card_probe(struct expansion_card *ec, const struct ecard_id *id)
{
	struct serial_card_info *info;
	struct serial_card_type *type = id->data;
	unsigned long bus_addr;
	unsigned char *virt_addr;
	unsigned int port;

	info = kmalloc(sizeof(struct serial_card_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	memset(info, 0, sizeof(struct serial_card_info));
	info->num_ports = type->num_ports;

	ecard_set_drvdata(ec, info);

	bus_addr = ec->resource[type->type].start;
	virt_addr = ioremap(bus_addr, ec->resource[type->type].end - bus_addr + 1);
	if (!virt_addr) {
		kfree(info);
		return -ENOMEM;
	}

	for (port = 0; port < info->num_ports; port ++) {
		unsigned long baddr = bus_addr + type->offset[port];
		unsigned char *vaddr = virt_addr + type->offset[port];

		info->ports[port] = serial_register_onedev(baddr, vaddr,
						ec->irq, type->baud_base);
	}

	return 0;
}

static void __devexit serial_card_remove(struct expansion_card *ec)
{
	struct serial_card_info *info = ecard_get_drvdata(ec);
	int i;

	ecard_set_drvdata(ec, NULL);

	for (i = 0; i < info->num_ports; i++)
		if (info->ports[i] > 0)
			unregister_serial(info->ports[i]);

	kfree(info);
}

static struct ecard_driver serial_card_driver = {
	.probe		= serial_card_probe,
	.remove 	= __devexit_p(serial_card_remove),
	.id_table	= serial_cids,
};

static int __init serial_card_init(void)
{
	return ecard_register_driver(&serial_card_driver);
}

static void __exit serial_card_exit(void)
{
	ecard_remove_driver(&serial_card_driver);
}

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("Russell King");
MODULE_LICENSE("GPL");

module_init(serial_card_init);
module_exit(serial_card_exit);
