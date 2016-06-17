/*
 * Support for indirect PCI bridges.
 *
 * Copyright (C) 1998 Gabriel Paubert.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/bootmem.h>

#include <asm/io.h>
#include <asm/prom.h>
#include <asm/pci-bridge.h>
#include <asm/machdep.h>

#define cfg_read(val, addr, type, op)	*val = op((type)(addr))
#define cfg_write(val, addr, type, op)	op((type *)(addr), (val))

#ifdef CONFIG_PPC_INDIRECT_PCI_BE
#define PCI_CFG_OUT out_be32
#else
#define PCI_CFG_OUT out_le32
#endif

#define INDIRECT_PCI_OP(rw, size, type, op, mask)			 \
static int								 \
indirect_##rw##_config_##size(struct pci_dev *dev, int offset, type val) \
{									 \
	struct pci_controller *hose = dev->sysdata;			 \
	u8 cfg_type = 0;						 \
									 \
	if (ppc_md.pci_exclude_device)					 \
		if (ppc_md.pci_exclude_device(dev->bus->number, dev->devfn)) \
			return PCIBIOS_DEVICE_NOT_FOUND;		 \
									 \
	if (hose->set_cfg_type)					 	 \
		if (dev->bus->number != hose->first_busno)		 \
			cfg_type = 1;					 \
									 \
	PCI_CFG_OUT(hose->cfg_addr, 					 \
		 (0x80000000 | ((dev->bus->number - hose->bus_offset) << 16) \
		  | (dev->devfn << 8) | ((offset & 0xfc) | cfg_type)));	 \
	cfg_##rw(val, hose->cfg_data + (offset & mask), type, op);	 \
	return PCIBIOS_SUCCESSFUL;    					 \
}

INDIRECT_PCI_OP(read, byte, u8 *, in_8, 3)
INDIRECT_PCI_OP(read, word, u16 *, in_le16, 2)
INDIRECT_PCI_OP(read, dword, u32 *, in_le32, 0)
INDIRECT_PCI_OP(write, byte, u8, out_8, 3)
INDIRECT_PCI_OP(write, word, u16, out_le16, 2)
INDIRECT_PCI_OP(write, dword, u32, out_le32, 0)

static struct pci_ops indirect_pci_ops =
{
	indirect_read_config_byte,
	indirect_read_config_word,
	indirect_read_config_dword,
	indirect_write_config_byte,
	indirect_write_config_word,
	indirect_write_config_dword
};

void __init
setup_indirect_pci(struct pci_controller* hose, u32 cfg_addr, u32 cfg_data)
{
	hose->ops = &indirect_pci_ops;
	hose->cfg_addr = (unsigned int *) ioremap(cfg_addr, 4);
	hose->cfg_data = (unsigned char *) ioremap(cfg_data, 4);
}

