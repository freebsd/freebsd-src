/*
 * Support for PCI bridges found on Power Macintoshes.
 * At present the "bandit" and "chaos" bridges are supported.
 * Fortunately you access configuration space in the same
 * way with either bridge.
 *
 * Copyright (C) 1997 Paul Mackerras (paulus@cs.anu.edu.au)
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

#include <asm/sections.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/pci-bridge.h>
#include <asm/machdep.h>
#include <asm/pmac_feature.h>

#undef DEBUG

static void add_bridges(struct device_node *dev);

/* XXX Could be per-controller, but I don't think we risk anything by
 * assuming we won't have both UniNorth and Bandit */
static int has_uninorth;

/*
 * Magic constants for enabling cache coherency in the bandit/PSX bridge.
 */
#define BANDIT_DEVID_2	8
#define BANDIT_REVID	3

#define BANDIT_DEVNUM	11
#define BANDIT_MAGIC	0x50
#define BANDIT_COHERENT	0x40

static int __init
fixup_one_level_bus_range(struct device_node *node, int higher)
{
	for (; node != 0;node = node->sibling) {
		int * bus_range;
		unsigned int *class_code;
		int len;

		/* For PCI<->PCI bridges or CardBus bridges, we go down */
		class_code = (unsigned int *) get_property(node, "class-code", 0);
		if (!class_code || ((*class_code >> 8) != PCI_CLASS_BRIDGE_PCI &&
			(*class_code >> 8) != PCI_CLASS_BRIDGE_CARDBUS))
			continue;
		bus_range = (int *) get_property(node, "bus-range", &len);
		if (bus_range != NULL && len > 2 * sizeof(int)) {
			if (bus_range[1] > higher)
				higher = bus_range[1];
		}
		higher = fixup_one_level_bus_range(node->child, higher);
	}
	return higher;
}

/* This routine fixes the "bus-range" property of all bridges in the
 * system since they tend to have their "last" member wrong on macs
 *
 * Note that the bus numbers manipulated here are OF bus numbers, they
 * are not Linux bus numbers.
 */
static void __init
fixup_bus_range(struct device_node *bridge)
{
	int * bus_range;
	int len;

	/* Lookup the "bus-range" property for the hose */
	bus_range = (int *) get_property(bridge, "bus-range", &len);
	if (bus_range == NULL || len < 2 * sizeof(int)) {
		printk(KERN_WARNING "Can't get bus-range for %s\n",
			       bridge->full_name);
		return;
	}
	bus_range[1] = fixup_one_level_bus_range(bridge->child, bus_range[1]);
}

/*
 * Apple MacRISC (UniNorth, Bandit, Chaos) PCI controllers.
 *
 * The "Bandit" version is present in all early PCI PowerMacs,
 * and up to the first ones using Grackle. Some machines may
 * have 2 bandit controllers (2 PCI busses).
 *
 * "Chaos" is used in some "Bandit"-type machines as a bridge
 * for the separate display bus. It is accessed the same
 * way as bandit, but cannot be probed for devices. It therefore
 * has its own config access functions.
 *
 * The "UniNorth" version is present in all Core99 machines
 * (iBook, G4, new IMacs, and all the recent Apple machines).
 * It contains 3 controllers in one ASIC.
 */

#define MACRISC_CFA0(devfn, off)	\
	((1 << (unsigned long)PCI_SLOT(dev_fn)) \
	| (((unsigned long)PCI_FUNC(dev_fn)) << 8) \
	| (((unsigned long)(off)) & 0xFCUL))

#define MACRISC_CFA1(bus, devfn, off)	\
	((((unsigned long)(bus)) << 16) \
	|(((unsigned long)(devfn)) << 8) \
	|(((unsigned long)(off)) & 0xFCUL) \
	|1UL)

static unsigned int __pmac
macrisc_cfg_access(struct pci_controller* hose, u8 bus, u8 dev_fn, u8 offset)
{
	unsigned int caddr;

	if (bus == hose->first_busno) {
		if (dev_fn < (11 << 3))
			return 0;
		caddr = MACRISC_CFA0(dev_fn, offset);
	} else
		caddr = MACRISC_CFA1(bus, dev_fn, offset);

	/* Uninorth will return garbage if we don't read back the value ! */
	do {
		out_le32(hose->cfg_addr, caddr);
	} while(in_le32(hose->cfg_addr) != caddr);

	offset &= has_uninorth ? 0x07 : 0x03;
	return (unsigned int)(hose->cfg_data) + (unsigned int)offset;
}

#define cfg_read(val, addr, type, op, op2)	\
	*val = op((type)(addr))
#define cfg_write(val, addr, type, op, op2)	\
	op((type *)(addr), (val)); (void) op2((type *)(addr))

#define cfg_read_bad(val, size)		*val = bad_##size;
#define cfg_write_bad(val, size)

#define bad_byte	0xff
#define bad_word	0xffff
#define bad_dword	0xffffffffU

#define MACRISC_PCI_OP(rw, size, type, op, op2)				    \
static int __pmac							    \
macrisc_##rw##_config_##size(struct pci_dev *dev, int off, type val)	    \
{									    \
	struct pci_controller *hose = dev->sysdata;			    \
	unsigned int addr;						    \
									    \
	addr = macrisc_cfg_access(hose, dev->bus->number, dev->devfn, off); \
	if (!addr) {							    \
		cfg_##rw##_bad(val, size)				    \
		return PCIBIOS_DEVICE_NOT_FOUND;			    \
	}								    \
	cfg_##rw(val, addr, type, op, op2);				    \
	return PCIBIOS_SUCCESSFUL;					    \
}

MACRISC_PCI_OP(read, byte, u8 *, in_8, x)
MACRISC_PCI_OP(read, word, u16 *, in_le16, x)
MACRISC_PCI_OP(read, dword, u32 *, in_le32, x)
MACRISC_PCI_OP(write, byte, u8, out_8, in_8)
MACRISC_PCI_OP(write, word, u16, out_le16, in_le16)
MACRISC_PCI_OP(write, dword, u32, out_le32, in_le32)

static struct pci_ops macrisc_pci_ops =
{
	macrisc_read_config_byte,
	macrisc_read_config_word,
	macrisc_read_config_dword,
	macrisc_write_config_byte,
	macrisc_write_config_word,
	macrisc_write_config_dword
};

/*
 * Verifiy that a specific (bus, dev_fn) exists on chaos
 */
static int __pmac
chaos_validate_dev(struct pci_dev *dev, int offset)
{
	if(pci_device_to_OF_node(dev) == 0)
		return PCIBIOS_DEVICE_NOT_FOUND;
	if((dev->vendor == 0x106b) && (dev->device == 3) && (offset >= 0x10) &&
	    (offset != 0x14) && (offset != 0x18) && (offset <= 0x24)) {
		return PCIBIOS_BAD_REGISTER_NUMBER;
	}
	return PCIBIOS_SUCCESSFUL;
}

#define CHAOS_PCI_OP(rw, size, type)					\
static int __pmac							\
chaos_##rw##_config_##size(struct pci_dev *dev, int off, type val)	\
{									\
	int result = chaos_validate_dev(dev, off);			\
	if(result == PCIBIOS_BAD_REGISTER_NUMBER) {			\
		cfg_##rw##_bad(val, size)				\
		return PCIBIOS_BAD_REGISTER_NUMBER;			\
	}								\
	if(result == PCIBIOS_SUCCESSFUL)				\
		return macrisc_##rw##_config_##size(dev, off, val);	\
	return result;							\
}

CHAOS_PCI_OP(read, byte, u8 *)
CHAOS_PCI_OP(read, word, u16 *)
CHAOS_PCI_OP(read, dword, u32 *)
CHAOS_PCI_OP(write, byte, u8)
CHAOS_PCI_OP(write, word, u16)
CHAOS_PCI_OP(write, dword, u32)

static struct pci_ops chaos_pci_ops =
{
	chaos_read_config_byte,
	chaos_read_config_word,
	chaos_read_config_dword,
	chaos_write_config_byte,
	chaos_write_config_word,
	chaos_write_config_dword
};


/*
 * For a bandit bridge, turn on cache coherency if necessary.
 * N.B. we could clean this up using the hose ops directly.
 */
static void __init
init_bandit(struct pci_controller *bp)
{
	unsigned int vendev, magic;
	int rev;

	/* read the word at offset 0 in config space for device 11 */
	out_le32(bp->cfg_addr, (1UL << BANDIT_DEVNUM) + PCI_VENDOR_ID);
	udelay(2);
	vendev = in_le32((volatile unsigned int *)bp->cfg_data);
	if (vendev == (PCI_DEVICE_ID_APPLE_BANDIT << 16) +
			PCI_VENDOR_ID_APPLE) {
		/* read the revision id */
		out_le32(bp->cfg_addr,
			 (1UL << BANDIT_DEVNUM) + PCI_REVISION_ID);
		udelay(2);
		rev = in_8(bp->cfg_data);
		if (rev != BANDIT_REVID)
			printk(KERN_WARNING
			       "Unknown revision %d for bandit\n", rev);
	} else if (vendev != (BANDIT_DEVID_2 << 16) + PCI_VENDOR_ID_APPLE) {
		printk(KERN_WARNING "bandit isn't? (%x)\n", vendev);
		return;
	}

	/* read the word at offset 0x50 */
	out_le32(bp->cfg_addr, (1UL << BANDIT_DEVNUM) + BANDIT_MAGIC);
	udelay(2);
	magic = in_le32((volatile unsigned int *)bp->cfg_data);
	if ((magic & BANDIT_COHERENT) != 0)
		return;
	magic |= BANDIT_COHERENT;
	udelay(2);
	out_le32((volatile unsigned int *)bp->cfg_data, magic);
	printk(KERN_INFO "Cache coherency enabled for bandit/PSX\n");
}


/*
 * Tweak the PCI-PCI bridge chip on the blue & white G3s.
 */
static void __init
init_p2pbridge(void)
{
	struct device_node *p2pbridge;
	struct pci_controller* hose;
	u8 bus, devfn;
	u16 val;

	/* XXX it would be better here to identify the specific
	   PCI-PCI bridge chip we have. */
	if ((p2pbridge = find_devices("pci-bridge")) == 0
	    || p2pbridge->parent == NULL
	    || strcmp(p2pbridge->parent->name, "pci") != 0)
		return;
	if (pci_device_from_OF_node(p2pbridge, &bus, &devfn) < 0) {
#ifdef DEBUG
		printk("Can't find PCI infos for PCI<->PCI bridge\n");
#endif
		return;
	}
	/* Warning: At this point, we have not yet renumbered all busses.
	 * So we must use OF walking to find out hose
	 */
	hose = pci_find_hose_for_OF_device(p2pbridge);
	if (!hose) {
#ifdef DEBUG
		printk("Can't find hose for PCI<->PCI bridge\n");
#endif
		return;
	}
	if (early_read_config_word(hose, bus, devfn,
				   PCI_BRIDGE_CONTROL, &val) < 0) {
		printk(KERN_ERR "init_p2pbridge: couldn't read bridge control\n");
		return;
	}
	val &= ~PCI_BRIDGE_CTL_MASTER_ABORT;
	early_write_config_word(hose, bus, devfn, PCI_BRIDGE_CONTROL, val);
}

/*
 * Some Apple desktop machines have a NEC PD720100A USB2 controller
 * on the motherboard. Open Firmware, on these, will disable the
 * EHCI part of it so it behaves like a pair of OHCI's. This fixup
 * code re-enables it ;)
 */
static void __init
fixup_nec_usb2(void)
{
	struct device_node *nec;

	for (nec = find_devices("usb"); nec != NULL; nec = nec->next) {
		struct pci_controller *hose;
		u32 data, *prop;
		u8 bus, devfn;
		
		prop = (u32 *)get_property(nec, "vendor-id", NULL);
		if (prop == NULL)
			continue;
		if (0x1033 != *prop)
			continue;
		prop = (u32 *)get_property(nec, "device-id", NULL);
		if (prop == NULL)
			continue;
		if (0x0035 != *prop)
			continue;
		prop = (u32 *)get_property(nec, "reg", 0);
		if (prop == NULL)
			continue;
		devfn = (prop[0] >> 8) & 0xff;
		bus = (prop[0] >> 16) & 0xff;
		if (PCI_FUNC(devfn) != 0)
			continue;
		hose = pci_find_hose_for_OF_device(nec);
		if (!hose)
			continue;
		printk("Found NEC PD720100A USB2 chip, enabling EHCI...\n");
		early_read_config_dword(hose, bus, devfn, 0xe4, &data);
		data &= ~1UL;
		early_write_config_dword(hose, bus, devfn, 0xe4, data);
		early_write_config_byte(hose, bus, devfn | 2, PCI_INTERRUPT_LINE,
			nec->intrs[0].line);
	}
}

void __init
pmac_find_bridges(void)
{
	add_bridges(find_devices("bandit"));
	add_bridges(find_devices("chaos"));
	add_bridges(find_devices("pci"));
	init_p2pbridge();
	fixup_nec_usb2();
}

#define GRACKLE_CFA(b, d, o)	(0x80 | ((b) << 8) | ((d) << 16) \
				 | (((o) & ~3) << 24))

#define GRACKLE_PICR1_STG		0x00000040
#define GRACKLE_PICR1_LOOPSNOOP		0x00000010

/* N.B. this is called before bridges is initialized, so we can't
   use grackle_pcibios_{read,write}_config_dword. */
static inline void grackle_set_stg(struct pci_controller* bp, int enable)
{
	unsigned int val;

	out_be32(bp->cfg_addr, GRACKLE_CFA(0, 0, 0xa8));
	val = in_le32((volatile unsigned int *)bp->cfg_data);
	val = enable? (val | GRACKLE_PICR1_STG) :
		(val & ~GRACKLE_PICR1_STG);
	out_be32(bp->cfg_addr, GRACKLE_CFA(0, 0, 0xa8));
	out_le32((volatile unsigned int *)bp->cfg_data, val);
	(void)in_le32((volatile unsigned int *)bp->cfg_data);
}

static inline void grackle_set_loop_snoop(struct pci_controller *bp, int enable)
{
	unsigned int val;

	out_be32(bp->cfg_addr, GRACKLE_CFA(0, 0, 0xa8));
	val = in_le32((volatile unsigned int *)bp->cfg_data);
	val = enable? (val | GRACKLE_PICR1_LOOPSNOOP) :
		(val & ~GRACKLE_PICR1_LOOPSNOOP);
	out_be32(bp->cfg_addr, GRACKLE_CFA(0, 0, 0xa8));
	out_le32((volatile unsigned int *)bp->cfg_data, val);
	(void)in_le32((volatile unsigned int *)bp->cfg_data);
}

static int __init
setup_uninorth(struct pci_controller* hose, struct reg_property* addr)
{
	pci_assign_all_busses = 1;
	has_uninorth = 1;
	hose->ops = &macrisc_pci_ops;
	hose->cfg_addr = ioremap(addr->address + 0x800000, 0x1000);
	hose->cfg_data = ioremap(addr->address + 0xc00000, 0x1000);
	/* We "know" that the bridge at f2000000 has the PCI slots. */
	return addr->address == 0xf2000000;
}

static void __init
setup_bandit(struct pci_controller* hose, struct reg_property* addr)
{
	hose->ops = &macrisc_pci_ops;
	hose->cfg_addr = (volatile unsigned int *)
		ioremap(addr->address + 0x800000, 0x1000);
	hose->cfg_data = (volatile unsigned char *)
		ioremap(addr->address + 0xc00000, 0x1000);
	init_bandit(hose);
}

static void __init
setup_chaos(struct pci_controller* hose, struct reg_property* addr)
{
	/* assume a `chaos' bridge */
	hose->ops = &chaos_pci_ops;
	hose->cfg_addr = (volatile unsigned int *)
		ioremap(addr->address + 0x800000, 0x1000);
	hose->cfg_data = (volatile unsigned char *)
		ioremap(addr->address + 0xc00000, 0x1000);
}

void __init
setup_grackle(struct pci_controller *hose)
{
	setup_indirect_pci(hose, 0xfec00000, 0xfee00000);
	if (machine_is_compatible("AAPL,PowerBook1998"))
		grackle_set_loop_snoop(hose, 1);
#if 0	/* Disabled for now, HW problems ??? */
	grackle_set_stg(hose, 1);
#endif
}

/*
 * We assume that if we have a G3 powermac, we have one bridge called
 * "pci" (a MPC106) and no bandit or chaos bridges, and contrariwise,
 * if we have one or more bandit or chaos bridges, we don't have a MPC106.
 */
static void __init
add_bridges(struct device_node *dev)
{
	int len;
	struct pci_controller *hose;
	struct reg_property *addr;
	char* disp_name;
	int *bus_range;
	int first = 1, primary;

	for (; dev != NULL; dev = dev->next) {
		addr = (struct reg_property *) get_property(dev, "reg", &len);
		if (addr == NULL || len < sizeof(*addr)) {
			printk(KERN_WARNING "Can't use %s: no address\n",
			       dev->full_name);
			continue;
		}
		bus_range = (int *) get_property(dev, "bus-range", &len);
		if (bus_range == NULL || len < 2 * sizeof(int)) {
			printk(KERN_WARNING "Can't get bus-range for %s, assume bus 0\n",
				       dev->full_name);
		}

		hose = pcibios_alloc_controller();
		if (!hose)
			continue;
		hose->arch_data = dev;
		hose->first_busno = bus_range ? bus_range[0] : 0;
		hose->last_busno = bus_range ? bus_range[1] : 0xff;

		disp_name = NULL;
		primary = first;
		if (device_is_compatible(dev, "uni-north")) {
			primary = setup_uninorth(hose, addr);
			disp_name = "UniNorth";
		} else if (strcmp(dev->name, "pci") == 0) {
			/* XXX assume this is a mpc106 (grackle) */
			setup_grackle(hose);
			disp_name = "Grackle (MPC106)";
		} else if (strcmp(dev->name, "bandit") == 0) {
			setup_bandit(hose, addr);
			disp_name = "Bandit";
		} else if (strcmp(dev->name, "chaos") == 0) {
			setup_chaos(hose, addr);
			disp_name = "Chaos";
			primary = 0;
		}
		printk(KERN_INFO "Found %s PCI host bridge at 0x%08x. Firmware bus number: %d->%d\n",
			disp_name, addr->address, hose->first_busno, hose->last_busno);
#ifdef DEBUG
		printk(" ->Hose at 0x%08lx, cfg_addr=0x%08lx,cfg_data=0x%08lx\n",
			hose, hose->cfg_addr, hose->cfg_data);
#endif

		/* Interpret the "ranges" property */
		/* This also maps the I/O region and sets isa_io/mem_base */
		pci_process_bridge_OF_ranges(hose, dev, primary);

		/* Fixup "bus-range" OF property */
		fixup_bus_range(dev);

		first &= !primary;
	}
}

static void __init
pcibios_fixup_OF_interrupts(void)
{
	struct pci_dev* dev;

	/*
	 * Open Firmware often doesn't initialize the
	 * PCI_INTERRUPT_LINE config register properly, so we
	 * should find the device node and apply the interrupt
	 * obtained from the OF device-tree
	 */
	pci_for_each_dev(dev) {
		struct device_node* node = pci_device_to_OF_node(dev);
		/* this is the node, see if it has interrupts */
		if (node && node->n_intrs > 0)
			dev->irq = node->intrs[0].line;
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
	}
}

void __init
pmac_pcibios_fixup(void)
{
	/* Fixup interrupts according to OF tree */
	pcibios_fixup_OF_interrupts();
}

int __pmac
pmac_pci_enable_device_hook(struct pci_dev *dev, int initial)
{
	struct device_node* node;
	int updatecfg = 0;
	int uninorth_child;

	node = pci_device_to_OF_node(dev);

	/* We don't want to enable USB controllers absent from the OF tree
	 * (iBook second controller)
	 */
	if (dev->vendor == PCI_VENDOR_ID_APPLE
	    && dev->device == PCI_DEVICE_ID_APPLE_KL_USB && !node)
		return -EINVAL;

	if (!node)
		return 0;

	uninorth_child = node->parent &&
		device_is_compatible(node->parent, "uni-north");

	/* Firewire & GMAC were disabled after PCI probe, the driver is
	 * claiming them, we must re-enable them now.
	 */
	if (uninorth_child && !strcmp(node->name, "firewire") &&
	    (device_is_compatible(node, "pci106b,18") ||
	     device_is_compatible(node, "pci106b,30") ||
	     device_is_compatible(node, "pci11c1,5811"))) {
		pmac_call_feature(PMAC_FTR_1394_CABLE_POWER, node, 0, 1);
		pmac_call_feature(PMAC_FTR_1394_ENABLE, node, 0, 1);
		updatecfg = 1;
	}
	if (uninorth_child && !strcmp(node->name, "ethernet") &&
	    device_is_compatible(node, "gmac")) {
		pmac_call_feature(PMAC_FTR_GMAC_ENABLE, node, 0, 1);
		updatecfg = 1;
	}

	if (updatecfg) {
		u16 cmd;

		/*
		 * Make sure PCI is correctly configured
		 *
		 * We use old pci_bios versions of the function since, by
		 * default, gmac is not powered up, and so will be absent
		 * from the kernel initial PCI lookup.
		 *
		 * Should be replaced by 2.4 new PCI mecanisms and really
		 * regiser the device.
		 */
		pci_read_config_word(dev, PCI_COMMAND, &cmd);
		cmd |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER | PCI_COMMAND_INVALIDATE;
    		pci_write_config_word(dev, PCI_COMMAND, cmd);
    		pci_write_config_byte(dev, PCI_LATENCY_TIMER, 16);
    		pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, 8);
	}

	return 0;
}

/* We power down some devices after they have been probed. They'll
 * be powered back on later on
 */
void __init
pmac_pcibios_after_init(void)
{
	struct device_node* nd;

#ifdef CONFIG_BLK_DEV_IDE
	struct pci_dev *dev;

	/* OF fails to initialize IDE controllers on macs
	 * (and maybe other machines)
	 *
	 * Ideally, this should be moved to the IDE layer, but we need
	 * to check specifically with Andre Hedrick how to do it cleanly
	 * since the common IDE code seem to care about the fact that the
	 * BIOS may have disabled a controller.
	 *
	 * -- BenH
	 */
	pci_for_each_dev(dev) {
		if ((dev->class >> 16) == PCI_BASE_CLASS_STORAGE)
			pci_enable_device(dev);
	}
#endif /* CONFIG_BLK_DEV_IDE */

	nd = find_devices("firewire");
	while (nd) {
		if (nd->parent && (device_is_compatible(nd, "pci106b,18") ||
				   device_is_compatible(nd, "pci106b,30") ||
				   device_is_compatible(nd, "pci11c1,5811"))
		    && device_is_compatible(nd->parent, "uni-north")) {
			pmac_call_feature(PMAC_FTR_1394_ENABLE, nd, 0, 0);
			pmac_call_feature(PMAC_FTR_1394_CABLE_POWER, nd, 0, 0);
		}
		nd = nd->next;
	}
	nd = find_devices("ethernet");
	while (nd) {
		if (nd->parent && device_is_compatible(nd, "gmac")
		    && device_is_compatible(nd->parent, "uni-north"))
			pmac_call_feature(PMAC_FTR_GMAC_ENABLE, nd, 0, 0);
		nd = nd->next;
	}
}

