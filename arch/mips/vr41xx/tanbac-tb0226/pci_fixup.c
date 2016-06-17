/*
 * FILE NAME
 *	arch/mips/vr41xx/tanbac-tb0226/pci_fixup.c
 *
 * BRIEF MODULE DESCRIPTION
 *	The TANBAC TB0226 specific PCI fixups.
 *
 * Copyright 2002,2003 Yoichi Yuasa
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

#include <asm/vr41xx/tb0226.h>

void __init pcibios_fixup_resources(struct pci_dev *dev)
{
}

void __init pcibios_fixup(void)
{
}

void __init pcibios_fixup_irqs(void)
{
	struct pci_dev *dev;
	u8 slot, pin;

	pci_for_each_dev(dev) {
		slot = PCI_SLOT(dev->devfn);
		dev->irq = 0;

		switch (slot) {
		case 12:
			vr41xx_set_irq_trigger(GD82559_1_PIN, TRIGGER_LEVEL,
			                       SIGNAL_THROUGH);
			vr41xx_set_irq_level(GD82559_1_PIN, LEVEL_LOW);
			dev->irq = GD82559_1_IRQ;
			break;
		case 13:
			vr41xx_set_irq_trigger(GD82559_2_PIN, TRIGGER_LEVEL,
			                       SIGNAL_THROUGH);
			vr41xx_set_irq_level(GD82559_2_PIN, LEVEL_LOW);
			dev->irq = GD82559_2_IRQ;
			break;
		case 14:
			pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
			switch (pin) {
			case 1:
				vr41xx_set_irq_trigger(UPD720100_INTA_PIN,
				                       TRIGGER_LEVEL,
				                       SIGNAL_THROUGH);
				vr41xx_set_irq_level(UPD720100_INTA_PIN, LEVEL_LOW);
				dev->irq = UPD720100_INTA_IRQ;
				break;
			case 2:
				vr41xx_set_irq_trigger(UPD720100_INTB_PIN,
				                       TRIGGER_LEVEL,
				                       SIGNAL_THROUGH);
				vr41xx_set_irq_level(UPD720100_INTB_PIN, LEVEL_LOW);
				dev->irq = UPD720100_INTB_IRQ;
				break;
			case 3:
				vr41xx_set_irq_trigger(UPD720100_INTC_PIN,
				                       TRIGGER_LEVEL,
				                       SIGNAL_THROUGH);
				vr41xx_set_irq_level(UPD720100_INTC_PIN, LEVEL_LOW);
				dev->irq = UPD720100_INTC_IRQ;
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
