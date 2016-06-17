/*
 * arch/ppc/platforms/lopec_pci.c
 *
 * PCI setup routines for the Motorola LoPEC.
 *
 * Author: Dan Cox
 *         danc@mvista.com (or, alternately, source@mvista.com)
 *
 * Copyright 2001 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>

#include <asm/machdep.h>
#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/pci-bridge.h>
#include <asm/open_pic.h>
#include <asm/mpc10x.h>

static inline int __init
lopec_map_irq(struct pci_dev *dev, unsigned char idsel, unsigned char pin)
{
	int irq;
	static char pci_irq_table[][4] = {
		{16, 0, 0, 0}, /* ID 11 - Winbond */
		{22, 0, 0, 0}, /* ID 12 - SCSI */
		{0, 0, 0, 0}, /* ID 13 - nothing */
		{17, 0, 0, 0}, /* ID 14 - 82559 Ethernet */
		{27, 0, 0, 0}, /* ID 15 - USB */
		{23, 0, 0, 0}, /* ID 16 - PMC slot 1 */
		{24, 0, 0, 0}, /* ID 17 - PMC slot 2 */
		{25, 0, 0, 0}, /* ID 18 - PCI slot */
		{0, 0, 0, 0}, /* ID 19 - nothing */
		{0, 0, 0, 0}, /* ID 20 - nothing */
		{0, 0, 0, 0}, /* ID 21 - nothing */
		{0, 0, 0, 0}, /* ID 22 - nothing */
		{0, 0, 0, 0}, /* ID 23 - nothing */
		{0, 0, 0, 0}, /* ID 24 - PMC slot 1b */
		{0, 0, 0, 0}, /* ID 25 - nothing */
		{0, 0, 0, 0}  /* ID 26 - PMC Slot 2b */
	};
	const long min_idsel = 11, max_idsel = 26, irqs_per_slot = 4;

	irq = PCI_IRQ_TABLE_LOOKUP;
	if (!irq)
		return 0;

	return irq;
}

void __init
lopec_setup_winbond_83553(struct pci_controller *hose)
{
	int devfn;

	devfn = PCI_DEVFN(11,0);

	/* IDE interrupt routing (primary 14, secondary 15) */
	early_write_config_byte(hose, 0, devfn, 0x43, 0xef);
	/* PCI interrupt routing */
	early_write_config_word(hose, 0, devfn, 0x44, 0x0000);

	/* ISA-PCI address decoder */
	early_write_config_byte(hose, 0, devfn, 0x48, 0xf0);

	/* RTC, kb, not used in PPC */
	early_write_config_byte(hose, 0, devfn, 0x4d, 0x00);
	early_write_config_byte(hose, 0, devfn, 0x4e, 0x04);
	devfn = PCI_DEVFN(11, 1);
	early_write_config_byte(hose, 0, devfn, 0x09, 0x8f);
	early_write_config_dword(hose, 0, devfn, 0x40, 0x00ff0011);
}

void __init
lopec_find_bridges(void)
{
	struct pci_controller *hose;

	hose = pcibios_alloc_controller();
	if (!hose)
		return;

	hose->first_busno = 0;
	hose->last_busno = 0xff;

	if (mpc10x_bridge_init(hose,
			       MPC10X_MEM_MAP_B,
			       MPC10X_MEM_MAP_B,
			       MPC10X_MAPB_EUMB_BASE) == 0) {

		hose->mem_resources[0].end = 0xffffffff;
		lopec_setup_winbond_83553(hose);
		hose->last_busno = pciauto_bus_scan(hose, hose->first_busno);
		ppc_md.pci_swizzle = common_swizzle;
		ppc_md.pci_map_irq = lopec_map_irq;
	}
}
