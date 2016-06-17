/* 
 * Driver for PLX Technology PCI9000-series host bridge.
 *
 * Copyright (C) 1997, 1998, 1999, 2000 FutureTV Labs Ltd
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/sched.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/ptrace.h>
#include <asm/irq.h>
#include <asm/mach/pci.h>

/*
 * Since the following functions are all very similar, the common parts
 * are pulled out into these macros.
 */

#define PLX_CLEAR_CONFIG						\
	__raw_writel(0, PLX_BASE + 0xac);				\
	local_irq_restore(flags); }

#define PLX_SET_CONFIG							\
	{ unsigned long flags;						\
	local_irq_save(flags);						\
	__raw_writel((1<<31 | (dev->bus->number << 16)			\
		| (dev->devfn << 8) | (where & ~3)			\
		| ((dev->bus->number == 0)?0:1)), PLX_BASE + 0xac);	\

#define PLX_CONFIG_WRITE(size)						\
	PLX_SET_CONFIG							\
	__raw_write##size(value, PCIO_BASE + (where & 3));		\
	if (__raw_readw(PLX_BASE + 0x6) & 0x2000)			\
		__raw_writew(0x2000, PLX_BASE + 0x6);			\
	PLX_CLEAR_CONFIG						\
	return PCIBIOS_SUCCESSFUL;

#define PLX_CONFIG_READ(size)						\
	PLX_SET_CONFIG							\
	*value = __raw_read##size(PCIO_BASE + (where & 3));		\
	if (__raw_readw(PLX_BASE + 0x6) & 0x2000) {			\
		__raw_writew(0x2000, PLX_BASE + 0x6);			\
		*value = 0xffffffffUL;					\
	}								\
	PLX_CLEAR_CONFIG						\
	return PCIBIOS_SUCCESSFUL;

/* Configuration space access routines */

static int
plx90x0_read_config_byte (struct pci_dev *dev,
			  int where, u8 *value)
{
	PLX_CONFIG_READ(b)
}

static int
plx90x0_read_config_word (struct pci_dev *dev,
			  int where, u16 *value)
{
	PLX_CONFIG_READ(w)
}

static int 
plx90x0_read_config_dword (struct pci_dev *dev,
			   int where, u32 *value)
{
	PLX_CONFIG_READ(l)
}

static int 
plx90x0_write_config_byte (struct pci_dev *dev,
			   int where, u8 value)
{
	PLX_CONFIG_WRITE(b)
}

static int 
plx90x0_write_config_word (struct pci_dev *dev,
			   int where, u16 value)
{
	PLX_CONFIG_WRITE(w)
}

static int 
plx90x0_write_config_dword (struct pci_dev *dev,
			    int where, u32 value)
{
	PLX_CONFIG_WRITE(l)
}

static void 
plx_syserr_handler(int irq, void *handle, struct pt_regs *regs)
{
	printk("PLX90x0: machine check %04x (pc=%08lx)\n", 
	       readw(PLX_BASE + 6), regs->ARM_pc);
	__raw_writew(0xf000, PLX_BASE + 6);
}

static struct pci_ops 
plx90x0_ops = 
{
	plx90x0_read_config_byte,
	plx90x0_read_config_word,
	plx90x0_read_config_dword,
	plx90x0_write_config_byte,
	plx90x0_write_config_word,
	plx90x0_write_config_dword,
};

/*
 * Initialise the PCI system.
 */

void __init
plx90x0_init(struct arm_sysdata *sysdata)
{
	static const unsigned long int base = PLX_BASE;
	char *what;
	unsigned long bar = (unsigned long)virt_to_bus((void *)PAGE_OFFSET);

	/* Have a sniff around and see which PLX device is present. */
	unsigned long id = __raw_readl(base + 0xf0);
	
#if 0
	/* This check was a good idea, but can fail.  The PLX9060 puts no
	   default value in these registers unless NB# is asserted (which it
	   isn't on these cards).  */
	if ((id & 0xffff) != PCI_VENDOR_ID_PLX)
		return;		/* Nothing found */
#endif

	/* Found one - now work out what it is. */
	switch (id >> 16) {
	case 0:		/* PCI_DEVICE_ID_PLX_9060 */
		what = "PCI9060";
		break;
	case PCI_DEVICE_ID_PLX_9060ES:
		what = "PCI9060ES";
		break;
	case PCI_DEVICE_ID_PLX_9060SD:
		what = "PCI9060SD";		/* uhuhh.. */
		break;
	case PCI_DEVICE_ID_PLX_9080:
		what = "PCI9080";
		break;
	default:
		printk("PCI: Unknown PLX device %04lx found -- ignored.\n",
		       id >> 16);
		return;
	}
	
	printk("PCI: PLX Technology %s host bridge found.\n", what);
	
	/* Now set it up for both master and slave accesses. */
	__raw_writel(0xffff0147,	base + 0x4);
	__raw_writeb(32,		base + 0xd);
	__raw_writel(0x8 | bar,		base + 0x18);
	__raw_writel(0xf8000008,	base + 0x80);
	__raw_writel(0x40000001,	base + 0x84);
	__raw_writel(0,			base + 0x88);
	__raw_writel(0,			base + 0x8c);
	__raw_writel(0x11,		base + 0x94);
	__raw_writel(0xC3 + (4 << 28)
		+ (8 << 11) + (1 << 10)
		     + (1 << 24),	base + 0x98);
	__raw_writel(0xC0000000,	base + 0x9c);
	__raw_writel(PLX_MEM_START,	base + 0xa0);
	__raw_writel(PLX_IO_START,	base + 0xa4);
	__raw_writel(0x3,		base + 0xa8);
	__raw_writel(0,			base + 0xac);
	__raw_writel(0x10001,		base + 0xe8);
	__raw_writel(0x8000767e,	base + 0xec);
	
	request_irq(IRQ_SYSERR, plx_syserr_handler, 0, 
		    "system error", NULL);

	pci_scan_bus(0, &plx90x0_ops, sysdata);
}
