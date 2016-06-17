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
 */
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/mips-boards/bonito64.h>

#define PCI_ACCESS_READ  0
#define PCI_ACCESS_WRITE 1

/*
 *  PCI configuration cycle AD bus definition
 */
/* Type 0 */
#define PCI_CFG_TYPE0_REG_SHF           0
#define PCI_CFG_TYPE0_FUNC_SHF          8

/* Type 1 */
#define PCI_CFG_TYPE1_REG_SHF           0
#define PCI_CFG_TYPE1_FUNC_SHF          8
#define PCI_CFG_TYPE1_DEV_SHF           11
#define PCI_CFG_TYPE1_BUS_SHF           16

static int bonito64_config_access(unsigned char access_type,
	struct pci_dev *dev, unsigned char where, u32 *data)
{
	unsigned char bus = dev->bus->number;
	unsigned char dev_fn = dev->devfn;
	unsigned char type;
	u32 intr, dummy;
	u64 pci_addr;

	if ((bus == 0) && (PCI_SLOT(dev_fn) == 0))
	        return -1;

	/* Clear cause register bits */
	BONITO_PCICMD |= (BONITO_PCICMD_MABORT_CLR | BONITO_PCICMD_MTABORT_CLR);

	/*
	 * Setup pattern to be used as PCI "address" for Type 0 cycle
	 */
	if (bus == 0) {
	        /* IDSEL */
	        pci_addr = (u64)1 << (PCI_SLOT(dev_fn) + 10);
	} else {
	        /* Bus number */
	        pci_addr = bus << PCI_CFG_TYPE1_BUS_SHF;

		/* Device number */
		pci_addr |= PCI_SLOT(dev_fn) << PCI_CFG_TYPE1_DEV_SHF;
	}

	/* Function (same for Type 0/1) */
	pci_addr |= PCI_FUNC(dev_fn) << PCI_CFG_TYPE0_FUNC_SHF;

	/* Register number (same for Type 0/1) */
	pci_addr |= (where & ~0x3) << PCI_CFG_TYPE0_REG_SHF;

	if (bus == 0) {
	        /* Type 0 */
	        BONITO_PCIMAP_CFG = pci_addr >> 16;
	} else {
	        /* Type 1 */
	        BONITO_PCIMAP_CFG = (pci_addr >> 16) | 0x10000;
	}

	/* Flush Bonito register block */
	dummy = BONITO_PCIMAP_CFG;
	iob();    /* sync */

	/* Perform access */
	if (access_type == PCI_ACCESS_WRITE) {
	        *(volatile u32 *)(KSEG1ADDR(BONITO_PCICFG_BASE +
				  (pci_addr & 0xffff))) = *(u32 *)data;

		/* Wait till done */
		while (BONITO_PCIMSTAT & 0xF)
		        ;
	} else {
	        *(u32 *)data =
			  *(volatile u32 *)(KSEG1ADDR(BONITO_PCICFG_BASE +
				    (pci_addr & 0xffff)));
	}

	/* Detect Master/Target abort */
	if (BONITO_PCICMD & (BONITO_PCICMD_MABORT_CLR |
			     BONITO_PCICMD_MTABORT_CLR)) {
	        /* Error occurred */

	        /* Clear bits */
	        BONITO_PCICMD |= (BONITO_PCICMD_MABORT_CLR |
				  BONITO_PCICMD_MTABORT_CLR);

		return -1;
	}

	return 0;
}


/*
 * We can't address 8 and 16 bit words directly.  Instead we have to
 * read/write a 32bit word and mask/modify the data we actually want.
 */
static int bonito64_read_config_byte (struct pci_dev *dev, int where,
	u8 *val)
{
	u32 data = 0;

	if (bonito64_config_access(PCI_ACCESS_READ, dev, where, &data))
		return -1;

	*val = (data >> ((where & 3) << 3)) & 0xff;

	return PCIBIOS_SUCCESSFUL;
}

static int bonito64_read_config_word (struct pci_dev *dev, int where,
	u16 *val)
{
	u32 data = 0;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (bonito64_config_access(PCI_ACCESS_READ, dev, where, &data))
	       return -1;

	*val = (data >> ((where & 3) << 3)) & 0xffff;

	return PCIBIOS_SUCCESSFUL;
}

static int bonito64_read_config_dword (struct pci_dev *dev, int where,
	u32 *val)
{
	u32 data = 0;

	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (bonito64_config_access(PCI_ACCESS_READ, dev, where, &data))
		return -1;

	*val = data;

	return PCIBIOS_SUCCESSFUL;
}

static int bonito64_write_config_byte (struct pci_dev *dev, int where,
	u8 val)
{
	u32 data = 0;

	if (bonito64_config_access(PCI_ACCESS_READ, dev, where, &data))
		return -1;

	data = (data & ~(0xff << ((where & 3) << 3))) |
	       (val << ((where & 3) << 3));

	if (bonito64_config_access(PCI_ACCESS_WRITE, dev, where, &data))
		return -1;

	return PCIBIOS_SUCCESSFUL;
}

static int bonito64_write_config_word (struct pci_dev *dev, int where,
	u16 val)
{
        u32 data = 0;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

        if (bonito64_config_access(PCI_ACCESS_READ, dev, where, &data))
	       return -1;

	data = (data & ~(0xffff << ((where & 3) << 3))) |
	       (val << ((where & 3) << 3));

	if (bonito64_config_access(PCI_ACCESS_WRITE, dev, where, &data))
	       return -1;


	return PCIBIOS_SUCCESSFUL;
}

static int bonito64_write_config_dword(struct pci_dev *dev, int where,
	u32 val)
{
	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (bonito64_config_access(PCI_ACCESS_WRITE, dev, where, &val))
	       return -1;

	return PCIBIOS_SUCCESSFUL;
}

struct pci_ops bonito64_pci_ops = {
	bonito64_read_config_byte,
        bonito64_read_config_word,
	bonito64_read_config_dword,
	bonito64_write_config_byte,
	bonito64_write_config_word,
	bonito64_write_config_dword
};
