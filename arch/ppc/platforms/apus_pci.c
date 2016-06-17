/*
 * Copyright (C) Michel Dänzer <michdaen@iiic.ethz.ch>
 *
 * APUS PCI routines.
 *
 * Currently, only B/CVisionPPC cards (Permedia2) are supported.
 *
 * Thanks to Geert Uytterhoeven for the idea:
 * Read values from given config space(s) for the first devices, -1 otherwise
 *
 */

#include <linux/config.h>
#ifdef CONFIG_AMIGA

#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/pci-bridge.h>
#include <asm/machdep.h>

#include "apus_pci.h"


/* These definitions are mostly adapted from pm2fb.c */

#undef APUS_PCI_MASTER_DEBUG
#ifdef APUS_PCI_MASTER_DEBUG
#define DPRINTK(a,b...)	printk(KERN_DEBUG "apus_pci: %s: " a, __FUNCTION__ , ## b)
#else
#define DPRINTK(a,b...)
#endif

/*
 * The _DEFINITIVE_ memory mapping/unmapping functions.
 * This is due to the fact that they're changing soooo often...
 */
#define DEFW()		wmb()
#define DEFR()		rmb()
#define DEFRW()		mb()

#define DEVNO(d)	((d)>>3)
#define FNNO(d)		((d)&7)


extern unsigned long powerup_PCI_present;

static struct pci_controller *apus_hose;


void *pci_io_base(unsigned int bus)
{
	return 0;
}


#define cfg_read(val, addr, type, op)	*val = op((type)(addr))
#define cfg_write(val, addr, type, op)	op((val), (type *)(addr)); DEFW()
#define cfg_read_bad	*val = ~0;
#define cfg_write_bad	;
#define cfg_read_val(val)	*val
#define cfg_write_val(val)	val

#define APUS_PCI_OP(rw, size, type, op, mask)					\
int									\
apus_pcibios_##rw##_config_##size(struct pci_dev *dev, int offset, type val)	\
{										\
	int fnno = FNNO(dev->devfn);						\
	int devno = DEVNO(dev->devfn);						\
										\
	if (dev->bus->number > 0 || devno != 1) {				\
		cfg_##rw##_bad;							\
		return PCIBIOS_DEVICE_NOT_FOUND;				\
	}									\
	/* base address + function offset + offset ^ endianness conversion */	\
	cfg_##rw(val, apus_hose->cfg_data + (fnno<<5) + (offset ^ mask),	\
		 type, op);							\
										\
	DPRINTK(#op " b: 0x%x, d: 0x%x, f: 0x%x, o: 0x%x, v: 0x%x\n",		\
		dev->bus->number, dev->devfn>>3, dev->devfn&7,			\
		offset, cfg_##rw##_val(val));					\
	return PCIBIOS_SUCCESSFUL;						\
}

APUS_PCI_OP(read, byte, u8 *, readb, 3)
APUS_PCI_OP(read, word, u16 *, readw, 2)
APUS_PCI_OP(read, dword, u32 *, readl, 0)
APUS_PCI_OP(write, byte, u8, writeb, 3)
APUS_PCI_OP(write, word, u16, writew, 2)
APUS_PCI_OP(write, dword, u32, writel, 0)


static struct pci_ops apus_pci_ops = {
	apus_pcibios_read_config_byte,
	apus_pcibios_read_config_word,
	apus_pcibios_read_config_dword,
	apus_pcibios_write_config_byte,
	apus_pcibios_write_config_word,
	apus_pcibios_write_config_dword
};

static struct resource pci_mem = { "B/CVisionPPC PCI mem", CVPPC_FB_APERTURE_ONE, CVPPC_PCI_CONFIG, IORESOURCE_MEM };

void __init
apus_pcibios_fixup(void)
{
/*	struct pci_dev *dev = pci_find_slot(0, 1<<3);
	unsigned int reg, val, offset;*/

	/* FIXME: interrupt? */
	/*dev->interrupt = xxx;*/

        request_resource(&iomem_resource, &pci_mem);
    	printk("%s: PCI mem resource requested\n", __FUNCTION__);
}

static void __init apus_pcibios_fixup_bus(struct pci_bus *bus)
{
        bus->resource[1] = &pci_mem;
}


/*
 * This is from pm2fb.c again
 *
 * Check if PCI (B/CVisionPPC) is available, initialize it and set up
 * the pcibios_* pointers
 */


void __init
apus_setup_pci_ptrs(void)
{
	if (!powerup_PCI_present) {
		DPRINTK("no PCI bridge detected\n");
		return;
	}
	DPRINTK("Phase5 B/CVisionPPC PCI bridge detected.\n");

	apus_hose = pcibios_alloc_controller();
	if (!apus_hose) {
		printk("apus_pci: Can't allocate PCI controller structure\n");
		return;
	}

	if (!(apus_hose->cfg_data = ioremap(CVPPC_PCI_CONFIG, 256))) {
		printk("apus_pci: unable to map PCI config region\n");
		return;
	}

	if (!(apus_hose->cfg_addr = ioremap(CSPPC_PCI_BRIDGE, 256))) {
		printk("apus_pci: unable to map PCI bridge\n");
		return;
	}

	writel(CSPPCF_BRIDGE_BIG_ENDIAN, apus_hose->cfg_addr + CSPPC_BRIDGE_ENDIAN);
	DEFW();

	writel(CVPPC_REGS_REGION,  apus_hose->cfg_data+ PCI_BASE_ADDRESS_0);
	DEFW();
	writel(CVPPC_FB_APERTURE_ONE, apus_hose->cfg_data + PCI_BASE_ADDRESS_1);
	DEFW();
	writel(CVPPC_FB_APERTURE_TWO, apus_hose->cfg_data + PCI_BASE_ADDRESS_2);
	DEFW();
	writel(CVPPC_ROM_ADDRESS, apus_hose->cfg_data + PCI_ROM_ADDRESS);
	DEFW();

	writel(0xef000000 | PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
		PCI_COMMAND_MASTER, apus_hose->cfg_data + PCI_COMMAND);
	DEFW();

	apus_hose->first_busno = 0;
	apus_hose->last_busno = 0;
	apus_hose->ops = &apus_pci_ops;
	ppc_md.pcibios_fixup = apus_pcibios_fixup;
	ppc_md.pcibios_fixup_bus = apus_pcibios_fixup_bus;

	return;
}

#endif /* CONFIG_AMIGA */
