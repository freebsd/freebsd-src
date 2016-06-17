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

#include <asm/mips-boards/msc01_pci.h>

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

static int msc_config_access(unsigned char access_type,
	struct pci_dev *dev, unsigned char where, u32 *data)
{
	unsigned char bus = dev->bus->number;
	unsigned char dev_fn = dev->devfn;
	unsigned char type;
	u32 intr, dummy;
	u64 pci_addr;

	if ((bus == 0) && (PCI_SLOT(dev_fn) == 0))
	        return -1;

	/* Clear status register bits. */
	MSC_WRITE(MSC01_PCI_INTSTAT,
		  (MSC01_PCI_INTCFG_MA_BIT | MSC01_PCI_INTCFG_TA_BIT));

	/* Setup address */
	if (bus == 0)
		type = 0;  /* Type 0 */
	else
		type = 1;  /* Type 1 */

	MSC_WRITE(MSC01_PCI_CFGADDR,
		  ((bus              << MSC01_PCI_CFGADDR_BNUM_SHF) |
		   (PCI_SLOT(dev_fn) << MSC01_PCI_CFGADDR_DNUM_SHF) |
		   (PCI_FUNC(dev_fn) << MSC01_PCI_CFGADDR_FNUM_SHF) |
		   ((where /4 )      << MSC01_PCI_CFGADDR_RNUM_SHF) |
		   (type)));

	/* Perform access */
	if (access_type == PCI_ACCESS_WRITE) {
	        MSC_WRITE(MSC01_PCI_CFGDATA, *data);
	} else {
		MSC_READ(MSC01_PCI_CFGDATA, *data);
	}

	/* Detect Master/Target abort */
	MSC_READ(MSC01_PCI_INTSTAT, intr);
	if (intr & (MSC01_PCI_INTCFG_MA_BIT | MSC01_PCI_INTCFG_TA_BIT)) {
	        /* Error occurred */

	        /* Clear bits */
		MSC_READ(MSC01_PCI_INTSTAT, intr);
		MSC_WRITE(MSC01_PCI_INTSTAT,
			  (MSC01_PCI_INTCFG_MA_BIT |
			   MSC01_PCI_INTCFG_TA_BIT));

		return -1;
	}

	return 0;
}


/*
 * We can't address 8 and 16 bit words directly.  Instead we have to
 * read/write a 32bit word and mask/modify the data we actually want.
 */
static int msc_read_config_byte (struct pci_dev *dev, int where,
	u8 *val)
{
	u32 data = 0;

	if (msc_config_access(PCI_ACCESS_READ, dev, where, &data))
		return -1;

	*val = (data >> ((where & 3) << 3)) & 0xff;

	return PCIBIOS_SUCCESSFUL;
}

static int msc_read_config_word (struct pci_dev *dev, int where,
	u16 *val)
{
	u32 data = 0;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (msc_config_access(PCI_ACCESS_READ, dev, where, &data))
	       return -1;

	*val = (data >> ((where & 3) << 3)) & 0xffff;

	return PCIBIOS_SUCCESSFUL;
}

static int msc_read_config_dword (struct pci_dev *dev, int where,
	u32 *val)
{
	u32 data = 0;

	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (msc_config_access(PCI_ACCESS_READ, dev, where, &data))
		return -1;

	*val = data;

	return PCIBIOS_SUCCESSFUL;
}

static int msc_write_config_byte (struct pci_dev *dev, int where,
	u8 val)
{
	u32 data = 0;

	if (msc_config_access(PCI_ACCESS_READ, dev, where, &data))
		return -1;

	data = (data & ~(0xff << ((where & 3) << 3))) |
	       (val << ((where & 3) << 3));

	if (msc_config_access(PCI_ACCESS_WRITE, dev, where, &data))
		return -1;

	return PCIBIOS_SUCCESSFUL;
}

static int msc_write_config_word (struct pci_dev *dev, int where,
	u16 val)
{
        u32 data = 0;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

        if (msc_config_access(PCI_ACCESS_READ, dev, where, &data))
	       return -1;

	data = (data & ~(0xffff << ((where & 3) << 3))) |
	       (val << ((where & 3) << 3));

	if (msc_config_access(PCI_ACCESS_WRITE, dev, where, &data))
	       return -1;


	return PCIBIOS_SUCCESSFUL;
}

static int msc_write_config_dword(struct pci_dev *dev, int where,
	u32 val)
{
	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (msc_config_access(PCI_ACCESS_WRITE, dev, where, &val))
	       return -1;

	return PCIBIOS_SUCCESSFUL;
}

struct pci_ops msc_pci_ops = {
	msc_read_config_byte,
        msc_read_config_word,
	msc_read_config_dword,
	msc_write_config_byte,
	msc_write_config_word,
	msc_write_config_dword
};
