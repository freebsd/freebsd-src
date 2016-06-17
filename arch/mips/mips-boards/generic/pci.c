/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999, 2000 MIPS Technologies, Inc.  All rights reserved.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * MIPS boards specific PCI support.
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/mips-boards/generic.h>
#include <asm/gt64120/gt64120.h>
#include <asm/mips-boards/bonito64.h>
#ifdef CONFIG_MIPS_MALTA
#include <asm/mips-boards/malta.h>
#endif
#include <asm/mips-boards/msc01_pci.h>

extern struct pci_ops bonito64_pci_ops;
extern struct pci_ops gt64120_pci_ops;
extern struct pci_ops msc_pci_ops;

static void __init malta_fixup(void)
{
#ifdef CONFIG_MIPS_MALTA
	struct pci_dev *pdev;
	unsigned char reg_val;

	pci_for_each_dev(pdev) {
		if ((pdev->vendor == PCI_VENDOR_ID_INTEL)
		    && (pdev->device == PCI_DEVICE_ID_INTEL_82371AB)
		    && (PCI_SLOT(pdev->devfn) == 0x0a)) {
			/*
			 * IDE Decode enable.
			 */
			pci_read_config_byte(pdev, 0x41, &reg_val);
        		pci_write_config_byte(pdev, 0x41, reg_val | 0x80);
			pci_read_config_byte(pdev, 0x43, &reg_val);
        		pci_write_config_byte(pdev, 0x43, reg_val | 0x80);
		}

		if ((pdev->vendor == PCI_VENDOR_ID_INTEL)
		    && (pdev->device == PCI_DEVICE_ID_INTEL_82371AB_0)
		    && (PCI_SLOT(pdev->devfn) == 0x0a)) {
			/*
			 * Set top of main memory accessible by ISA or DMA
			 * devices to 16 Mb.
			 */
			pci_read_config_byte(pdev, 0x69, &reg_val);
			pci_write_config_byte(pdev, 0x69, reg_val | 0xf0);
		}
	}

	/*
	 * Activate Floppy Controller in the SMSC FDC37M817 Super I/O
	 * Controller.
	 * This should be done in the bios/bootprom and will be fixed in
         * a later revision of YAMON (the MIPS boards boot prom).
	 */
	/* Entering config state. */
	SMSC_WRITE(SMSC_CONFIG_ENTER, SMSC_CONFIG_REG);

	/* Activate floppy controller. */
	SMSC_WRITE(SMSC_CONFIG_DEVNUM, SMSC_CONFIG_REG);
	SMSC_WRITE(SMSC_CONFIG_DEVNUM_FLOPPY, SMSC_DATA_REG);
	SMSC_WRITE(SMSC_CONFIG_ACTIVATE, SMSC_CONFIG_REG);
	SMSC_WRITE(SMSC_CONFIG_ACTIVATE_ENABLE, SMSC_DATA_REG);

	/* Exit config state. */
	SMSC_WRITE(SMSC_CONFIG_EXIT, SMSC_CONFIG_REG);
#endif
}

void __init pcibios_init(void)
{
	printk("PCI: Probing PCI hardware on host bus 0.\n");

	switch (mips_revision_corid) {
	case MIPS_REVISION_CORID_QED_RM5261:
	case MIPS_REVISION_CORID_CORE_LV:
	case MIPS_REVISION_CORID_CORE_FPGA:
		/*
		 * Due to a bug in the Galileo system controller, we need
		 * to setup the PCI BAR for the Galileo internal registers.
		 * This should be done in the bios/bootprom and will be
		 * fixed in a later revision of YAMON (the MIPS boards
		 * boot prom).
		 */
		GT_WRITE(GT_PCI0_CFGADDR_OFS,
			 (0 << GT_PCI0_CFGADDR_BUSNUM_SHF) | /* Local bus */
			 (0 << GT_PCI0_CFGADDR_DEVNUM_SHF) | /* GT64120 dev */
			 (0 << GT_PCI0_CFGADDR_FUNCTNUM_SHF) | /* Function 0*/
			 ((0x20/4) << GT_PCI0_CFGADDR_REGNUM_SHF) | /* BAR 4*/
			 GT_PCI0_CFGADDR_CONFIGEN_BIT );

		/* Perform the write */
		GT_WRITE( GT_PCI0_CFGDATA_OFS, PHYSADDR(GT64120_BASE));

		pci_scan_bus(0, &gt64120_pci_ops, NULL);
		break;

	case MIPS_REVISION_CORID_BONITO64:
	case MIPS_REVISION_CORID_CORE_20K:
		pci_scan_bus(0, &bonito64_pci_ops, NULL);
		break;

	case MIPS_REVISION_CORID_CORE_MSC:
		pci_scan_bus(0, &msc_pci_ops, NULL);
		break;
	}

	malta_fixup();
}

struct pci_fixup pcibios_fixups[] = {
	{ 0 }
};

/*
 *  Called after each bus is probed, but before its children
 *  are examined.
 */
void __devinit pcibios_fixup_bus(struct pci_bus *b)
{
	pci_read_bridge_bases(b);
}

unsigned int pcibios_assign_all_busses(void)
{
	return 1;
}
