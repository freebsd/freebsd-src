/*
 * Copyright 2002 Momentum Computer Inc.
 * Author: Matthew Dharm <mdharm@momenco.com>
 *
 * Based on work for the Linux port to the Ocelot board, which is
 * Copyright 2001 MontaVista Software Inc.
 * Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 *
 * arch/mips/momentum/ocelot_g/pci.c
 *     Board-specific PCI routines for gt64240 controller.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <asm/pci.h>


void __init gt64240_board_pcibios_fixup_bus(struct pci_bus *bus)
{
	struct pci_bus *current_bus = bus;
	struct pci_dev *devices;
	struct list_head *devices_link;
	u16 cmd;

	/* loop over all known devices on this bus */
	list_for_each(devices_link, &(current_bus->devices)) {

		devices = pci_dev_b(devices_link);
		if (devices == NULL)
			continue;

		if ((current_bus->number == 0) &&
				PCI_SLOT(devices->devfn) == 1) {
			/* Intel 82543 Gigabit MAC */
			devices->irq = 2;       /* irq_nr is 2 for INT0 */
		} else if ((current_bus->number == 0) &&
				PCI_SLOT(devices->devfn) == 2) {
			/* Intel 82543 Gigabit MAC */
			devices->irq = 3;       /* irq_nr is 3 for INT1 */
		} else if ((current_bus->number == 1) &&
				PCI_SLOT(devices->devfn) == 3) {
			/* Intel 21555 bridge */
			devices->irq = 5;       /* irq_nr is 8 for INT6 */
		} else if ((current_bus->number == 1) &&
				PCI_SLOT(devices->devfn) == 4) {
			/* PMC Slot */
			devices->irq = 9;       /* irq_nr is 9 for INT7 */
		} else {
			/* We don't have assign interrupts for other devices. */
			devices->irq = 0xff;
		}

		/* Assign an interrupt number for the device */
		bus->ops->write_byte(devices, PCI_INTERRUPT_LINE, devices->irq);

		/* enable master for everything but the GT-64240 */
		if (((current_bus->number != 0) && (current_bus->number != 1))
				|| (PCI_SLOT(devices->devfn) != 0)) {
			bus->ops->read_word(devices, PCI_COMMAND, &cmd);
			cmd |= PCI_COMMAND_MASTER;
			bus->ops->write_word(devices, PCI_COMMAND, cmd);
		}
	}
}
