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

#include <asm/gt64120/gt64120.h>

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

#define GT_ERR_BITS (GT_INTRCAUSE_MASABORT0_BIT | GT_INTRCAUSE_TARABORT0_BIT)

static int gt64120_config_access(unsigned int access_type,
	struct pci_dev *dev, unsigned char where, u32 *data)
{
	unsigned char bus = dev->bus->number;
	unsigned char dev_fn = dev->devfn;
	u32 intr, ret;

	/*
	 * Because of a bug in the galileo (for slot 31) and support for the
	 * 2nd bus of the Galileo is broken anyway ...
	 */
	if ((bus == 0) && (dev_fn >= PCI_DEVFN(31,0)))
		return PCIBIOS_DEVICE_NOT_FOUND;

	/* Clear cause register bits */
	intr = GT_READ(GT_INTRCAUSE_OFS);
	GT_WRITE(GT_INTRCAUSE_OFS, intr & ~GT_ERR_BITS);

	/* Setup address */
	GT_WRITE(GT_PCI0_CFGADDR_OFS,
		 (bus    << GT_PCI0_CFGADDR_BUSNUM_SHF)   |
		 (dev_fn << GT_PCI0_CFGADDR_FUNCTNUM_SHF) |
		 (where  &  ~0x3) |
		 GT_PCI0_CFGADDR_CONFIGEN_BIT);

	if (access_type == PCI_ACCESS_WRITE) {
		if (bus == 0 && PCI_SLOT(dev_fn) == 0) {
			/*
			 * The Galileo system controller is acting
			 * differently than other devices.
			 */
			GT_WRITE(GT_PCI0_CFGDATA_OFS, *data);
		} else
			__GT_WRITE(GT_PCI0_CFGDATA_OFS, *data);
	} else {
		if (bus == 0 && PCI_SLOT(dev_fn) == 0) {
			/*
			 * The Galileo system controller is acting
			 * differently than other devices.
			 */
			ret = GT_READ(GT_PCI0_CFGDATA_OFS);
		} else
			ret = __GT_READ(GT_PCI0_CFGDATA_OFS);
	}

	/* Check for master or target abort */
	intr = GT_READ(GT_INTRCAUSE_OFS);

	if (intr & GT_ERR_BITS) {	/* Error occured */

		/* Clear bits */
		intr = GT_READ(GT_INTRCAUSE_OFS);
		GT_WRITE(GT_INTRCAUSE_OFS, intr & ~GT_ERR_BITS);

		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	*data = ret;

	return 0;
}


/*
 * We can't address 8 and 16 bit words directly.  Instead we have to
 * read/write a 32bit word and mask/modify the data we actually want.
 */
static int gt64120_read_config_byte (struct pci_dev *dev, int where, u8 *val)
{
	u32 data;

	if (gt64120_config_access(PCI_ACCESS_READ, dev, where, &data))
		return PCIBIOS_DEVICE_NOT_FOUND;

	*val = data >> ((where & 3) << 3);

	return PCIBIOS_SUCCESSFUL;
}

static int gt64120_read_config_word (struct pci_dev *dev, int where, u16 *val)
{
	u32 data;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (gt64120_config_access(PCI_ACCESS_READ, dev, where, &data))
	       return PCIBIOS_DEVICE_NOT_FOUND;

	*val = data >> ((where & 3) << 3);

	return PCIBIOS_SUCCESSFUL;
}

static int gt64120_read_config_dword (struct pci_dev *dev, int where, u32 *val)
{
	u32 data;

	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (gt64120_config_access(PCI_ACCESS_READ, dev, where, &data))
		return PCIBIOS_DEVICE_NOT_FOUND;

	*val = data;

	return PCIBIOS_SUCCESSFUL;
}

static int gt64120_write_config_byte (struct pci_dev *dev, int where, u8 val)
{
	u32 data = 0;

	if (gt64120_config_access(PCI_ACCESS_READ, dev, where, &data))
		return PCIBIOS_DEVICE_NOT_FOUND;

	data = (data & ~(0xff << ((where & 3) << 3))) |
	       (val << ((where & 3) << 3));

	if (gt64120_config_access(PCI_ACCESS_WRITE, dev, where, &data))
		return PCIBIOS_DEVICE_NOT_FOUND;

	return PCIBIOS_SUCCESSFUL;
}

static int gt64120_write_config_word (struct pci_dev *dev, int where, u16 val)
{
        u32 data = 0;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

        if (gt64120_config_access(PCI_ACCESS_READ, dev, where, &data))
	       return PCIBIOS_DEVICE_NOT_FOUND;

	data = (data & ~(0xffff << ((where & 3) << 3))) |
	       (val << ((where & 3) << 3));

	if (gt64120_config_access(PCI_ACCESS_WRITE, dev, where, &data))
	       return PCIBIOS_DEVICE_NOT_FOUND;

	return PCIBIOS_SUCCESSFUL;
}

static int gt64120_write_config_dword(struct pci_dev *dev, int where, u32 val)
{
	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (gt64120_config_access(PCI_ACCESS_WRITE, dev, where, &val))
	       return PCIBIOS_DEVICE_NOT_FOUND;

	return PCIBIOS_SUCCESSFUL;
}

struct pci_ops gt64120_pci_ops = {
	gt64120_read_config_byte,
        gt64120_read_config_word,
	gt64120_read_config_dword,
	gt64120_write_config_byte,
	gt64120_write_config_word,
	gt64120_write_config_dword
};
