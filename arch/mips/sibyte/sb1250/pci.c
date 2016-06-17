/*
 * Copyright (C) 2001,2002 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*
 * BCM1250-specific PCI support
 *
 * This module provides the glue between Linux's PCI subsystem
 * and the hardware.  We basically provide glue for accessing
 * configuration space, and set up the translation for I/O
 * space accesses.
 *
 * To access configuration space, we use ioremap.  In the 32-bit
 * kernel, this consumes either 4 or 8 page table pages, and 16MB of
 * kernel mapped memory.  Hopefully neither of these should be a huge
 * problem.
 */
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/console.h>

#include <asm/sibyte/sb1250_defs.h>
#include <asm/sibyte/sb1250_regs.h>
#include <asm/sibyte/sb1250_scd.h>
#include <asm/io.h>

/*
 * Macros for calculating offsets into config space given a device
 * structure or dev/fun/reg
 */
#define CFGOFFSET(bus,devfn,where) (((bus)<<16)+((devfn)<<8)+(where))
#define CFGADDR(dev,where)         CFGOFFSET((dev)->bus->number,(dev)->devfn,where)

static void *cfg_space;

#define PCI_BUS_ENABLED	1
#define LDT_BUS_ENABLED	2
#define PCI_DEVICE_MODE	4

static int sb1250_bus_status = 0;

#define PCI_BRIDGE_DEVICE  0
#define LDT_BRIDGE_DEVICE  1

#ifdef CONFIG_SIBYTE_HAS_LDT
/*
 * HT's level-sensitive interrupts require EOI, which is generated
 * through a 4MB memory-mapped region
 */
unsigned long ldt_eoi_space;
#endif

/*
 * Read/write 32-bit values in config space.
 */
static inline u32 READCFG32(u32 addr)
{
	return *(u32 *)(cfg_space + (addr&~3));
}

static inline void WRITECFG32(u32 addr, u32 data)
{
	*(u32 *)(cfg_space + (addr & ~3)) = data;
}

/*
 * Some checks before doing config cycles:
 * In PCI Device Mode, hide everything on bus 0 except the LDT host
 * bridge.  Otherwise, access is controlled by bridge MasterEn bits.
 */
static int
sb1250_pci_can_access(struct pci_dev *dev)
{
	u32 devno;

	if (!(sb1250_bus_status & (PCI_BUS_ENABLED | PCI_DEVICE_MODE)))
		return 0;

	if (dev->bus->number == 0) {
		devno = PCI_SLOT(dev->devfn);
		if (devno == LDT_BRIDGE_DEVICE)
		        return (sb1250_bus_status & LDT_BUS_ENABLED) != 0;
 		else if (sb1250_bus_status & PCI_DEVICE_MODE)
			return 0;
		else
			return 1;
	} else
		return 1;
}

/*
 * Read/write access functions for various sizes of values
 * in config space.  Return all 1's for disallowed accesses
 * for a kludgy but adequate simulation of master aborts.
 */

static int
sb1250_pci_read_config_byte(struct pci_dev *dev, int where, u8 * val)
{
	u32 data = 0;
	u32 cfgaddr = CFGADDR(dev, where);

	if (sb1250_pci_can_access(dev))
		data = READCFG32(cfgaddr);
	else
		data = 0xFFFFFFFF;

	*val = (data >> ((where & 3) << 3)) & 0xff;

	return PCIBIOS_SUCCESSFUL;
}

static int
sb1250_pci_read_config_word(struct pci_dev *dev, int where, u16 * val)
{
	u32 data = 0;
	u32 cfgaddr = CFGADDR(dev, where);

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (sb1250_pci_can_access(dev))
		data = READCFG32(cfgaddr);
	else
		data = 0xFFFFFFFF;

	*val = (data >> ((where & 3) << 3)) & 0xffff;

	return PCIBIOS_SUCCESSFUL;
}

static int
sb1250_pci_read_config_dword(struct pci_dev *dev, int where, u32 * val)
{
	u32 data = 0;
	u32 cfgaddr = CFGADDR(dev, where);

	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (sb1250_pci_can_access(dev))
		data = READCFG32(cfgaddr);
	else
		data = 0xFFFFFFFF;

	*val = data;

	return PCIBIOS_SUCCESSFUL;
}


static int
sb1250_pci_write_config_byte(struct pci_dev *dev, int where, u8 val)
{
	u32 data = 0;
	u32 cfgaddr = CFGADDR(dev, where);

	if (sb1250_pci_can_access(dev)) {
		data = READCFG32(cfgaddr);

		data = (data & ~(0xff << ((where & 3) << 3))) |
		    (val << ((where & 3) << 3));

		WRITECFG32(cfgaddr, data);
	}

	return PCIBIOS_SUCCESSFUL;
}

static int
sb1250_pci_write_config_word(struct pci_dev *dev, int where, u16 val)
{
	u32 data = 0;
	u32 cfgaddr = CFGADDR(dev, where);

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (sb1250_pci_can_access(dev)) {
		data = READCFG32(cfgaddr);

		data = (data & ~(0xffff << ((where & 3) << 3))) |
		    (val << ((where & 3) << 3));

		WRITECFG32(cfgaddr, data);
	}

	return PCIBIOS_SUCCESSFUL;
}

static int
sb1250_pci_write_config_dword(struct pci_dev *dev, int where, u32 val)
{
	u32 cfgaddr = CFGADDR(dev, where);

	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (sb1250_pci_can_access(dev))
		WRITECFG32(cfgaddr, val);

	return PCIBIOS_SUCCESSFUL;
}

struct pci_ops sb1250_pci_ops = {
	sb1250_pci_read_config_byte,
	sb1250_pci_read_config_word,
	sb1250_pci_read_config_dword,
	sb1250_pci_write_config_byte,
	sb1250_pci_write_config_word,
	sb1250_pci_write_config_dword
};


void __init pcibios_init(void)
{
	uint32_t cmdreg;
	uint64_t reg;

	cfg_space = ioremap(A_PHYS_LDTPCI_CFG_MATCH_BITS, 16*1024*1024);

	/*
	 * See if the PCI bus has been configured by the firmware.
	 */
	reg = *((volatile uint64_t *) KSEG1ADDR(A_SCD_SYSTEM_CFG));
	if (!(reg & M_SYS_PCI_HOST)) {
		sb1250_bus_status |= PCI_DEVICE_MODE;
	} else {
		cmdreg = READCFG32(CFGOFFSET(0, PCI_DEVFN(PCI_BRIDGE_DEVICE, 0),
					     PCI_COMMAND));
		if (!(cmdreg & PCI_COMMAND_MASTER)) {
			printk
			    ("PCI: Skipping PCI probe.  Bus is not initialized.\n");
			iounmap(cfg_space);
			return;
		}
		sb1250_bus_status |= PCI_BUS_ENABLED;
	}

	/*
	 * Establish mappings in KSEG2 (kernel virtual) to PCI I/O
	 * space.  Use "match bytes" policy to make everything look
	 * little-endian.  So, you need to also set
	 * CONFIG_SWAP_IO_SPACE, but this is the combination that
	 * works correctly with most of Linux's drivers.
	 * XXX ehs: Should this happen in PCI Device mode?
	 */

	set_io_port_base((unsigned long)
		ioremap(A_PHYS_LDTPCI_IO_MATCH_BYTES, 65536));
	isa_slot_offset = (unsigned long)
		ioremap(A_PHYS_LDTPCI_IO_MATCH_BYTES_32, 1024*1024);

#ifdef CONFIG_SIBYTE_HAS_LDT
	/*
	 * Also check the LDT bridge's enable, just in case we didn't
	 * initialize that one.
	 */

	cmdreg = READCFG32(CFGOFFSET(0, PCI_DEVFN(LDT_BRIDGE_DEVICE, 0),
				     PCI_COMMAND));
	if (cmdreg & PCI_COMMAND_MASTER) {
		sb1250_bus_status |= LDT_BUS_ENABLED;

		/*
		 * Need bits 23:16 to convey vector number.  Note that
		 * this consumes 4MB of kernel-mapped memory
		 * (Kseg2/Kseg3) for 32-bit kernel.
		 */
		ldt_eoi_space = (unsigned long)
			ioremap(A_PHYS_LDT_SPECIAL_MATCH_BYTES, 4*1024*1024);
	}
#endif

	/* Probe for PCI hardware */

	printk("PCI: Probing PCI hardware on host bus 0.\n");
	pci_scan_bus(0, &sb1250_pci_ops, NULL);

#ifdef CONFIG_VGA_CONSOLE
	take_over_console(&vga_con,0,MAX_NR_CONSOLES-1,1);
#endif
}

struct pci_fixup pcibios_fixups[] = {
	{0}
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
