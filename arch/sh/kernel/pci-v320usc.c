/****************************************************************************/
/*
 * pci.c: V3 Hurricane specific PCI support.
 *
 * Copyright (C) 1999,2000 Dan Aizenstros (dan@vcubed.com)
 * Copyright (C) 2001, Lineo Inc., <davidm@moreton.com.au>
 *               SH3 port.
 */
/****************************************************************************/

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/autoconf.h>
#include <asm/byteorder.h>
#include <asm/pci.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <asm/byteorder.h>
#include <asm/io.h>

#include "pci-v320usc.h"

/****************************************************************************/
#ifdef CONFIG_PCI
/****************************************************************************/

#ifndef CONFIG_PCMCIA
#define V320USC_MEM_SPACE	0xB8000000 /* V320 sees this as AC000000 ! */
#define V320USC_IO_SPACE	0xB4000000
#define V320USC_CONF_SPACE	0xB4000000
#else
#define V320USC_MEM_SPACE	0xab000000
#define V320USC_IO_SPACE	0xaa000000
#define V320USC_CONF_SPACE	0xaa000000
#endif
#define	V320USC_BASE		0xb1fd0000

#ifdef __LITTLE_ENDIAN
#define reg08(x)	(V320USC_BASE + (V320USC_##x))
#define reg16(x)	(V320USC_BASE + (V320USC_##x))
#else
#define reg08(x)	(V320USC_BASE + ((V320USC_##x)^3))
#define reg16(x)	(V320USC_BASE + ((V320USC_##x)^2))
#endif

#define reg32(x)	(V320USC_BASE + (V320USC_##x))

#define v320usc_inb(addr)			readb(reg08(addr)
#define v320usc_outb(value, addr)	writeb(value, reg08(addr))
#define v320usc_inw(addr)			readw(reg16(addr))
#define v320usc_outw(value, addr)	writew(value, reg16(addr))
#define v320usc_inl(addr)			readl(reg32(addr))
#define v320usc_outl(value, addr)	writel(value, reg32(addr))

/****************************************************************************/

/* Set the LB_PCI_BASE0 register to allow PCI Memory cycles to be used */

static void set_io_cycles(void)
{
	u32 tempscratch;

	v320usc_outl(
		(v320usc_inl(LB_PCI_BASE0) &
		~(LB_PCI_BASEX_PCI_CMD_MASK | LB_PCI_BASEX_ALOW_MASK))
		| LB_PCI_BASEX_IO, LB_PCI_BASE0);

	/* do a read of the register to flush the posting buffer */
	tempscratch = v320usc_inl(LB_PCI_BASE0);
}


/* Set the LB_PCI_BASE0 register to allow PCI Config cycles to be used */
static void set_config_cycles(int alow)
{
	u32 tempscratch;

	tempscratch = v320usc_inl(LB_PCI_BASE0);

	v320usc_outl((tempscratch & 
		~(LB_PCI_BASEX_PCI_CMD_MASK | LB_PCI_BASEX_ALOW_MASK))
		| LB_PCI_BASEX_CONFIG | alow, LB_PCI_BASE0);

	/* do a read of the register to flush the posting buffer */
	tempscratch = v320usc_inl(LB_PCI_BASE0);
}


static int mkaddr (unsigned char bus,
		unsigned char devfn,
		unsigned char where)
{
	int addr, alow;

	if (bus) {
		addr =  ((bus    & 0xff) << 0x10) | 
			((devfn & 0xff) << 0x08) |
			 (where  & 0xff);

		/* set alow for type 1 configuration cycle */
		alow = 1;
	} else {
		int device, function;

		if (devfn >= PCI_DEVFN(12, 0))
			return -1;

		device = PCI_SLOT(devfn);
		function = PCI_FUNC(devfn);

		/* This next line assumes that the PCI backplane connects  */
		/* the IDSEL line for each slot to a Address line with a   */
		/* small value resistor.  Note: The PCI spec. suggests     */
		/* several options here.  This code assumes the following  */
		/* IDSEL to Address line mapping.                          */

		/* Slot  Address Line */
		/*  0  - AD11 */
		/*  1  - AD12 */
		/*  2  - AD13 */
		/*  3  - AD14 */
		/*  4  - AD15 */

		/* 0x800 is AD11 set, shift left by the slot number (device) */
		addr = (0x800 << device) | (function << 8) | where;
		
		/* set alow for type 0 configuration cycle */
		alow = 0;
	}

	set_config_cycles(alow);

	return addr;
}


static int v320usc_pcibios_read_config_byte (
						struct pci_dev *dev,
						int where,
						u8 *val)
{
	int retVal = PCIBIOS_SUCCESSFUL;
	unsigned long flags;
	int addr;

	save_and_cli(flags);

	/* Clear status bits */
	v320usc_outw(v320usc_inw(PCI_STAT_W), PCI_STAT_W);

	if ((addr = mkaddr(dev->bus->number, dev->devfn, where)) == -1)
		retVal = -1;
	else {
		// *val = readb(V320USC_CONF_SPACE + addr);
		*val = * (unsigned char *) (V320USC_CONF_SPACE + addr);

		/* Check for master abort */
		if (v320usc_inw(PCI_STAT_W) & PCI_STAT_W_M_ABORT) {
			v320usc_outw(0xffff, PCI_STAT_W);
			// printk("Master abort byte\n");
			*val = 0xff;
		}
	}

	set_io_cycles();
	restore_flags(flags);
	return retVal;
}


static int v320usc_pcibios_read_config_word (
						struct pci_dev *dev,
						int where,
						u16 *val)
{
	int retVal = PCIBIOS_SUCCESSFUL;
	unsigned long flags;
	int addr;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	save_and_cli(flags);

	/* Clear status bits */
	v320usc_outw(v320usc_inw(PCI_STAT_W), PCI_STAT_W);

	if ((addr = mkaddr(dev->bus->number, dev->devfn, where)) == -1)
		retVal = -1;
	else {
		*val = readw(V320USC_CONF_SPACE + addr);

		/* Check for master abort */
		if (v320usc_inw(PCI_STAT_W) & PCI_STAT_W_M_ABORT) {
			v320usc_outw(0xffff, PCI_STAT_W);
			// printk("Master abort word\n");
			*val = 0xffff;
		}
	}

	set_io_cycles();

	restore_flags(flags);

	return retVal;
}


static int v320usc_pcibios_read_config_dword (
						struct pci_dev *dev,
						int where,
						u32 *val)
{
	int retVal = PCIBIOS_SUCCESSFUL;
	unsigned long flags;
	int addr;

	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	save_and_cli(flags);

	/* Clear status bits */
	v320usc_outw(v320usc_inw(PCI_STAT_W), PCI_STAT_W);

	if ((addr = mkaddr(dev->bus->number, dev->devfn, where)) == -1)
		retVal = -1;
	else {
		*val = readl(V320USC_CONF_SPACE + addr);

		/* Check for master abort */
		if (v320usc_inw(PCI_STAT_W) & PCI_STAT_W_M_ABORT) {
			v320usc_outw(0xffff, PCI_STAT_W);
			// printk("Master abort long\n");
			*val = 0xffffffff;
		}
	}

	set_io_cycles();

	restore_flags(flags);

	return retVal;
}


static int v320usc_pcibios_write_config_byte (
						struct pci_dev *dev,
						int where,
						u8 val)
{
	int retVal = PCIBIOS_SUCCESSFUL;
	unsigned long flags;
	int addr;

	save_and_cli(flags);

	if ((addr = mkaddr(dev->bus->number, dev->devfn, where)) == -1)
		retVal = -1;
	else
		writeb(val, V320USC_CONF_SPACE + addr);

	/* wait for write FIFO to empty */
	v320usc_inw(PCI_STAT_W);

	set_io_cycles();

	restore_flags(flags);

	return retVal;
}


static int v320usc_pcibios_write_config_word (
						struct pci_dev *dev,
						int where,
						u16 val)
{
	int retVal = PCIBIOS_SUCCESSFUL;
	unsigned long flags;
	int addr;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	save_and_cli(flags);

	if ((addr = mkaddr(dev->bus->number, dev->devfn, where)) == -1)
		retVal = -1;
	else
		writew(val, V320USC_CONF_SPACE + addr);

	/* wait for write FIFO to empty */
	v320usc_inw(PCI_STAT_W);

	set_io_cycles();

	restore_flags(flags);

	return retVal;
}


static int v320usc_pcibios_write_config_dword (
						struct pci_dev *dev,
						int where,
						u32 val)
{
	int retVal = PCIBIOS_SUCCESSFUL;
	unsigned long flags;
	int addr;

	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	save_and_cli(flags);

	if ((addr = mkaddr(dev->bus->number, dev->devfn, where)) == -1)
		retVal = -1;
	else
		writel(val, V320USC_CONF_SPACE + addr);

	/* wait for write FIFO to empty */
	v320usc_inw(PCI_STAT_W);

	set_io_cycles();

	restore_flags(flags);

	return retVal;
}


struct pci_ops pci_config_ops = {
	v320usc_pcibios_read_config_byte,
	v320usc_pcibios_read_config_word,
	v320usc_pcibios_read_config_dword,
	v320usc_pcibios_write_config_byte,
	v320usc_pcibios_write_config_word,
	v320usc_pcibios_write_config_dword
};

/****************************************************************************/

/* Everything hangs off this */
static struct pci_bus *pci_root_bus;

static u8 __init no_swizzle(struct pci_dev *dev, u8 * pin)
{
	return PCI_SLOT(dev->devfn);
}


static int __init map_keywest_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	if (pin >= 1 && pin <= 4)
		return(PINT_IRQ_BASE + 2 + pin - 1);
	return(-1);
}


char *
pcibios_setup(char *str)
{
	return(str);
}


void __init
pcibios_fixup_pbus_ranges(struct pci_bus *bus,
			  struct pbus_set_ranges_data *ranges)
{
	ranges->io_start -= bus->resource[0]->start;
	ranges->io_end -= bus->resource[0]->start;
	ranges->mem_start -= bus->resource[1]->start;
	ranges->mem_end -= bus->resource[1]->start;
}


void __init pcibios_init(void)
{
	char *mode;

	if (sh_mv.mv_init_pci != NULL)
		sh_mv.mv_init_pci();

	if (v320usc_inw(PCI_VENDOR) != V3USC_PCI_VENDOR) {
		printk("V320USC: PCI bridge chip not found\n");
		return;
	}

	switch (v320usc_inw(PCI_DEVICE)) {
	case V3USC_PCI_DEVICE_MIPS_9: mode = "MIPS 9-bit"; break;
	case V3USC_PCI_DEVICE_MIPS_5: mode = "MIPS 5-bit"; break;
	case V3USC_PCI_DEVICE_SH3:    mode = "SH-3"; break;
	case V3USC_PCI_DEVICE_SH4:    mode = "SH-4"; break;
	default:                      mode = "unknown !"; break;
	}
	printk("V3 Semiconductor V320USC bridge in %s mode\n", mode);

	/*
	 * RST_OUT de asserted
	 */
	v320usc_outb(0x80, SYSTEM);

	/*
	 * 16x8 latency time, GRANT, Normal arbitration, RAM Disable,
	 * CLK_OUT Disable, SYNC_RDY, BREQ_NEG
	 */
	v320usc_outl(0x10120001, LB_BUS_CFG);

	/*
	 * System error enable, PCI bus master, Memory & IO Access enable
	 */
	v320usc_outw(0x0047, PCI_CMD_W);
	v320usc_outw(0xf100, PCI_STAT_W); /* clear any errors */

	/*
	 * 32x8 clocks as Latency Timer
	 */
	v320usc_outl(0x00008800, PCI_HDR_CFG);

	v320usc_outl(0x00005761, DRAM_BLK0);
	v320usc_outl(0x02005761, DRAM_BLK1);
	v320usc_outl(0x00000000, DRAM_BLK2);
	v320usc_outl(0x00000000, DRAM_BLK3);
	v320usc_outl(0x00000942, DRAM_CFG);
	v320usc_outl(0x80030066, PCI_BUS_CFG);

	v320usc_outl(v320usc_inl(INT_STAT), INT_STAT); /* clear any ints */

	v320usc_outl(0x00000000, INT_CFG0);
	v320usc_outl(0x00000000, INT_CFG1);
	v320usc_outl(0x00000000, INT_CFG2);
	v320usc_outl(0x00000000, INT_CFG3);

#if 0
	v320usc_outl(0x0c000000, PCI_I2O_BASE);
	v320usc_outl(0x0c010053, PCI_I2O_MAP);
#endif
	v320usc_outl(0x0c000001, PCI_MEM_BASE);
	v320usc_outl(0x00010063, PCI_MEM_MAP);

	v320usc_outl(0x00000000, LB_PCU_BASE); /* Disable PCU on SuperH */
#if 0
	v320usc_outl(0x00000081, PCI_PCU_BASE);
#endif

#ifndef CONFIG_PCMCIA
	v320usc_outl(0xB4002030, LB_PCI_BASE0); /* 64Mb */
	v320usc_outl(0xACB86030, LB_PCI_BASE1); /* 64Mb */
#else
	v320usc_outl(0x9e002010, LB_PCI_BASE0); /* 16Mb */
	v320usc_outl(0x9fAb6010, LB_PCI_BASE1); /* 16Mb */
#endif

	/*
	 * do the scan
	 */
	pci_root_bus = pci_scan_bus(0, &pci_config_ops, NULL);
	pci_assign_unassigned_resources();
	pci_fixup_irqs(no_swizzle, map_keywest_irq);

#ifndef CONFIG_PCMCIA
	v320usc_outl(0xB4002030, LB_PCI_BASE0); /* 64Mb */
	v320usc_outl(0xACB86030, LB_PCI_BASE1); /* 64Mb */
#else
	v320usc_outl(0x9e002010, LB_PCI_BASE0); /* 16Mb */
	v320usc_outl(0x9fAb6010, LB_PCI_BASE1); /* 16Mb */
#endif

	/*
	 * Lock the system registers
	 */
	v320usc_outb(0x60, SYSTEM);

	/*
	 * Map all the PCI IO space
	 */
	mach_port_map(PCIBIOS_MIN_IO, (64*1024) - PCIBIOS_MIN_IO,
			V320USC_IO_SPACE+PCIBIOS_MIN_IO, 0);
}

void __init pcibios_fixup_bus(struct pci_bus *bus)
{
	printk("%s,%d: %s()\n", __FILE__, __LINE__, __FUNCTION__);
}

static void __init pci_fixup_ide_bases(struct pci_dev *d)
{
	int i;

	/*
	 * PCI IDE controllers use non-standard I/O port decoding, respect it.
	 */
	if ((d->class >> 8) != PCI_CLASS_STORAGE_IDE)
		return;
	printk("PCI: IDE base address fixup for %s\n", d->slot_name);
	for(i=0; i<4; i++) {
		struct resource *r = &d->resource[i];
		if ((r->start & ~0x80) == 0x374) {
			r->start |= 2;
			r->end = r->start;
		}
	}
}

/* Add future fixups here... */
struct pci_fixup pcibios_fixups[] = {
	{ PCI_FIXUP_HEADER,	PCI_ANY_ID,	PCI_ANY_ID,	pci_fixup_ide_bases },
	{ 0 }
};

/****************************************************************************/
#endif /* CONFIG_PCI */
