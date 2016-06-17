/*
 * FILE NAME
 *	arch/mips/vr41xx/tanbac-tb0229/pci_fixup.c
 *
 * BRIEF MODULE DESCRIPTION
 *	The TANBAC TB0229(VR4131DIMM) specific PCI fixups.
 *
 * Copyright 2003 Megasolution Inc.
 *                matsu@megasolution.jp
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/pci.h>

#include <asm/vr41xx/tb0229.h>

void __init pcibios_fixup_resources(struct pci_dev *dev)
{
}

void __init pcibios_fixup(void)
{
}

void __init pcibios_fixup_irqs(void)
{
#ifdef CONFIG_TANBAC_TB0219
	struct pci_dev *dev;
	u8 slot;

	pci_for_each_dev(dev) {
		slot = PCI_SLOT(dev->devfn);
		dev->irq = 0;

		switch (slot) {
		case 12:
			vr41xx_set_irq_trigger(TB0219_PCI_SLOT1_PIN , TRIGGER_LEVEL,
			                       SIGNAL_THROUGH);
			vr41xx_set_irq_level(TB0219_PCI_SLOT1_PIN, LEVEL_LOW);
			dev->irq = TB0219_PCI_SLOT1_IRQ;
			break;
		case 13:
			vr41xx_set_irq_trigger(TB0219_PCI_SLOT2_PIN , TRIGGER_LEVEL,
			                       SIGNAL_THROUGH);
			vr41xx_set_irq_level(TB0219_PCI_SLOT2_PIN, LEVEL_LOW);
			dev->irq = TB0219_PCI_SLOT2_IRQ;
			break;
		case 14:
			vr41xx_set_irq_trigger(TB0219_PCI_SLOT3_PIN , TRIGGER_LEVEL,
			                       SIGNAL_THROUGH);
			vr41xx_set_irq_level(TB0219_PCI_SLOT3_PIN, LEVEL_LOW);
			dev->irq = TB0219_PCI_SLOT3_IRQ;
			break;
		default:
			break;
		}

		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
	}
#endif
}

unsigned int pcibios_assign_all_busses(void)
{
	return 0;
}
