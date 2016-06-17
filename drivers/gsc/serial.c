/*
 *	Serial Device Initialisation for LASI/ASP/WAX/DINO
 *
 *	(c) Copyright 2000 The Puffin Group Inc.
 *	(c) Copyright 2000-2001 Helge Deller <deller@gmx.de>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/signal.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/ioport.h>
#include <linux/timex.h>
#include <linux/module.h>
#include <linux/serial.h>
#include <linux/slab.h>

#include <asm/serial.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/gsc.h>

#include "busdevice.h"

static void setup_parisc_serial(struct serial_struct *serial,
		unsigned long address, int irq, int line)
{
	memset(serial, 0, sizeof(struct serial_struct));

	/* autoconfig() sets state->type.  This sets info->type */
	serial->type = PORT_16550A;

	serial->line = line;
	serial->iomem_base = ioremap(address, 0x8);

	serial->irq = irq;
	serial->io_type = SERIAL_IO_MEM;	/* define access method */
	serial->flags = 0;
	serial->xmit_fifo_size = 16;
	serial->custom_divisor = 0;
	serial->baud_base = LASI_BASE_BAUD;
}

static int __init 
serial_init_chip(struct parisc_device *dev)
{
	static int serial_line_nr;
	unsigned long address;

	struct serial_struct *serial;

	if (!dev->irq) {
		if (dev->parent->id.hw_type != HPHW_IOA) {
			printk(KERN_INFO "Serial: device 0x%lx not configured.\n"
				"Enable support for Wax, Lasi, Asp or Dino.\n", dev->hpa);
		}
		return -ENODEV;
	}

	serial = kmalloc(sizeof(*serial), GFP_KERNEL);
	if (!serial)
		return -ENOMEM;

	address = dev->hpa;
	if (dev->id.sversion != 0x8d) {
		address += 0x800;
	}

	setup_parisc_serial(serial, address, dev->irq, serial_line_nr++);

	if (register_serial(serial) < 0) {
		printk(KERN_WARNING "register_serial returned error\n");
		kfree(serial);
		return -ENODEV;
	}

	return 0;
}

static struct parisc_device_id serial_tbl[] = {
	{ HPHW_FIO, HVERSION_REV_ANY_ID, HVERSION_ANY_ID, 0x00075 },
	{ HPHW_FIO, HVERSION_REV_ANY_ID, HVERSION_ANY_ID, 0x0008c },
	{ HPHW_FIO, HVERSION_REV_ANY_ID, HVERSION_ANY_ID, 0x0008d },
	{ 0 }
};

/* Hack.  Dino's serial port will get listed first on some machines.
 * So we register this driver first which knows about Lasi's serial port.
 * This needs to get fixed properly somehow.
 */
static struct parisc_device_id serial1_tbl[] = {
	{ HPHW_FIO, HVERSION_REV_ANY_ID, 0x03B, 0x0008C }, /* C1xx/C1xxL */
	{ HPHW_FIO, HVERSION_REV_ANY_ID, 0x03C, 0x0008C }, /* B132L */
	{ HPHW_FIO, HVERSION_REV_ANY_ID, 0x03D, 0x0008C }, /* B160L */
	{ HPHW_FIO, HVERSION_REV_ANY_ID, 0x03E, 0x0008C }, /* B132L+ */
	{ HPHW_FIO, HVERSION_REV_ANY_ID, 0x03F, 0x0008C }, /* B180L+ */
	{ HPHW_FIO, HVERSION_REV_ANY_ID, 0x046, 0x0008C }, /* Rocky2 120 */
	{ HPHW_FIO, HVERSION_REV_ANY_ID, 0x047, 0x0008C }, /* Rocky2 150 */
	{ HPHW_FIO, HVERSION_REV_ANY_ID, 0x04E, 0x0008C }, /* Kiji L2 132 */
	{ HPHW_FIO, HVERSION_REV_ANY_ID, 0x056, 0x0008C }, /* Raven+ */
	{ 0 }
};


MODULE_DEVICE_TABLE(parisc, serial_tbl);

static struct parisc_driver serial1_driver = {
	name:		"Serial RS232",
	id_table:	serial1_tbl,
	probe:		serial_init_chip,
};

static struct parisc_driver serial_driver = {
	name:		"Serial RS232",
	id_table:	serial_tbl,
	probe:		serial_init_chip,
};

void __init probe_serial_gsc(void)
{
	register_parisc_driver(&serial1_driver);
	register_parisc_driver(&serial_driver);
}
