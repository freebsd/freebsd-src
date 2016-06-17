/*
 * FILE NAME
 *	arch/mips/vr41xx/zao-capcella/pci_fixup.c
 *
 * BRIEF MODULE DESCRIPTION
 *	The ZAO Networks Capcella specific PCI fixups.
 *
 * Copyright 2002 Yoichi Yuasa
 *                yuasa@hh.iij4u.or.jp
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/pci.h>

#include <asm/vr41xx/capcella.h>

void __init pcibios_fixup_resources(struct pci_dev *dev)
{
}

void __init pcibios_fixup(void)
{
}

void __init pcibios_fixup_irqs(void)
{
	struct pci_dev *dev;
	u8 slot, func, pin;

	pci_for_each_dev(dev) {
		slot = PCI_SLOT(dev->devfn);
		func = PCI_FUNC(dev->devfn);
		dev->irq = 0;

		switch (slot) {
		case 11:
			dev->irq = RTL8139_1_IRQ;
			break;
		case 12:
			dev->irq = RTL8139_2_IRQ;
			break;
		case 14:
			pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
			switch (pin) {
			case 1:
				dev->irq = PC104PLUS_INTA_IRQ;
				break;
			case 2:
				dev->irq = PC104PLUS_INTB_IRQ;
				break;
			case 3:
				dev->irq = PC104PLUS_INTC_IRQ;
				break;
			case 4:
				dev->irq = PC104PLUS_INTD_IRQ;
				break;
			}
			break;
		}

		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
	}
}

unsigned int pcibios_assign_all_busses(void)
{
	return 0;
}
