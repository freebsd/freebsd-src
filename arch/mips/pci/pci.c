/*
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/pci.h>

#include <asm/pci_channel.h>

void
pcibios_update_resource(struct pci_dev *dev, struct resource *root,
			struct resource *res, int resource)
{
	u32 new, check;
	int reg;

	new = res->start | (res->flags & PCI_REGION_FLAG_MASK);
	if (resource < 6) {
		reg = PCI_BASE_ADDRESS_0 + 4*resource;
	} else if (resource == PCI_ROM_RESOURCE) {
		res->flags |= PCI_ROM_ADDRESS_ENABLE;
		new |= PCI_ROM_ADDRESS_ENABLE;
		reg = dev->rom_base_reg;
	} else {
		/* Somebody might have asked allocation of a non-standard resource */
		return;
	}
	
	pci_write_config_dword(dev, reg, new);
	pci_read_config_dword(dev, reg, &check);
	if ((new ^ check) & ((new & PCI_BASE_ADDRESS_SPACE_IO) ? PCI_BASE_ADDRESS_IO_MASK : PCI_BASE_ADDRESS_MEM_MASK)) {
		printk(KERN_ERR "PCI: Error while updating region "
		       "%s/%d (%08x != %08x)\n", dev->slot_name, resource,
		       new, check);
	}
}

/*
 * We need to avoid collisions with `mirrored' VGA ports
 * and other strange ISA hardware, so we always want the
 * addresses to be allocated in the 0x000-0x0ff region
 * modulo 0x400.
 *
 * Why? Because some silly external IO cards only decode
 * the low 10 bits of the IO address. The 0x00-0xff region
 * is reserved for motherboard devices that decode all 16
 * bits, so it's ok to allocate at, say, 0x2800-0x28ff,
 * but we want to try to avoid allocating at 0x2900-0x2bff
 * which might have be mirrored at 0x0100-0x03ff..
 */
void
pcibios_align_resource(void *data, struct resource *res,
		       unsigned long size, unsigned long align)
{
	if (res->flags & IORESOURCE_IO) {
		unsigned long start = res->start;

		if (start & 0x300) {
			start = (start + 0x3ff) & ~0x3ff;
			res->start = start;
		}
	}
}

/*
 *  If we set up a device for bus mastering, we need to check the latency
 *  timer as certain crappy BIOSes forget to set it properly.
 */
unsigned int pcibios_max_latency = 255;

void pcibios_set_master(struct pci_dev *dev)
{
	u8 lat;
	pci_read_config_byte(dev, PCI_LATENCY_TIMER, &lat);
	if (lat < 16)
		lat = (64 <= pcibios_max_latency) ? 64 : pcibios_max_latency;
	else if (lat > pcibios_max_latency)
		lat = pcibios_max_latency;
	else
		return;
	printk(KERN_DEBUG "PCI: Setting latency timer of device %s to %d\n", dev->slot_name, lat);
	pci_write_config_byte(dev, PCI_LATENCY_TIMER, lat);
}

char * __devinit pcibios_setup(char *str)
{
	return str;
}

static int pcibios_enable_resources(struct pci_dev *dev, int mask)
{
	u16 cmd, old_cmd;
	int idx;
	struct resource *r;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	old_cmd = cmd;
	for(idx=0; idx<6; idx++) {
		/* Only set up the requested stuff */
		if (!(mask & (1<<idx)))
			continue;
			
		r = &dev->resource[idx];
		if (!r->start && r->end) {
			printk(KERN_ERR "PCI: Device %s not available because of resource collisions\n", dev->slot_name);
			return -EINVAL;
		}
		if (r->flags & IORESOURCE_IO)
			cmd |= PCI_COMMAND_IO;
		if (r->flags & IORESOURCE_MEM)
			cmd |= PCI_COMMAND_MEMORY;
	}
	if (dev->resource[PCI_ROM_RESOURCE].start)
		cmd |= PCI_COMMAND_MEMORY;
	if (cmd != old_cmd) {
		printk("PCI: Enabling device %s (%04x -> %04x)\n", dev->slot_name, old_cmd, cmd);
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
	return 0;
}

int pcibios_enable_device(struct pci_dev *dev, int mask)
{
	return pcibios_enable_resources(dev, mask);
}

#ifdef CONFIG_NEW_PCI
/*
 * Named PCI new and about to die before it's old :-)
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 *
 * Modified to be mips generic, ppopov@mvista.com
 */

/*
 * This file contains common PCI routines meant to be shared for
 * all MIPS machines.
 *
 * Strategies:
 *
 * . We rely on pci_auto.c file to assign PCI resources (MEM and IO)
 *   TODO: this shold be optional for some machines where they do have
 *   a real "pcibios" that does resource assignment.
 *
 * . We then use pci_scan_bus() to "discover" all the resources for
 *   later use by Linux.
 *
 * . We finally reply on a board supplied function, pcibios_fixup_irq(), to
 *   to assign the interrupts.  We may use setup-irq.c under drivers/pci
 *   later.
 *
 * . Specifically, we will *NOT* use pci_assign_unassigned_resources(),
 *   because we assume all PCI devices should have the resources correctly
 *   assigned and recorded.
 *
 * Limitations:
 *
 * . We "collapse" all IO and MEM spaces in sub-buses under a top-level bus
 *   into a contiguous range.
 *
 * . In the case of Memory space, the rnage is 1:1 mapping with CPU physical
 *   address space.
 *
 * . In the case of IO space, it starts from 0, and the beginning address
 *   is mapped to KSEG0ADDR(mips_io_port) in the CPU physical address.
 *
 * . These are the current MIPS limitations (by ioremap, etc).  In the
 *   future, we may remove them.
 *
 * Credits:
 *	Most of the code are derived from the pci routines from PPC and Alpha,
 *	which were mostly writtne by
 *		Cort Dougan, cort@fsmlabs.com
 *		Matt Porter, mporter@mvista.com
 *		Dave Rusling david.rusling@reo.mts.dec.com
 *		David Mosberger davidm@cs.arizona.edu
 */

extern void pcibios_fixup(void);
extern void pcibios_fixup_irqs(void);

struct pci_fixup pcibios_fixups[] = {
	{ PCI_FIXUP_HEADER, PCI_ANY_ID, PCI_ANY_ID, pcibios_fixup_resources },
	{ 0 }
};

extern int pciauto_assign_resources(int busno, struct pci_channel * hose);

void __init pcibios_init(void)
{
	struct pci_channel *p;
	struct pci_bus *bus;
	int busno;

#ifdef CONFIG_PCI_AUTO
	/* assign resources */
	busno=0;
	for (p= mips_pci_channels; p->pci_ops != NULL; p++) {
		busno = pciauto_assign_resources(busno, p) + 1;
	}
#endif

	/* scan the buses */
	busno = 0;
	for (p= mips_pci_channels; p->pci_ops != NULL; p++) {
		bus = pci_scan_bus(busno, p->pci_ops, p);
		busno = bus->subordinate+1;
	}

	/* machine dependent fixups */
	pcibios_fixup();
	/* fixup irqs (board specific routines) */
	pcibios_fixup_irqs();
}

unsigned long __init pci_bridge_check_io(struct pci_dev *bridge)
{
	u16 io;

	pci_read_config_word(bridge, PCI_IO_BASE, &io);
	if (!io) {
		pci_write_config_word(bridge, PCI_IO_BASE, 0xf0f0);
		pci_read_config_word(bridge, PCI_IO_BASE, &io);
		pci_write_config_word(bridge, PCI_IO_BASE, 0x0);
	}
	if (io)
		return IORESOURCE_IO;
	printk(KERN_WARNING "PCI: bridge %s does not support I/O forwarding!\n",
				bridge->name);
	return 0;
}

void __init pcibios_fixup_bus(struct pci_bus *bus)
{
	/* Propogate hose info into the subordinate devices.  */

	struct pci_channel *hose = bus->sysdata;
	struct pci_dev *dev = bus->self;

	if (!dev) {
		/* Root bus */
		bus->resource[0] = hose->io_resource;
		bus->resource[1] = hose->mem_resource;
	} else {
		/* This is a bridge. Do not care how it's initialized,
		   just link its resources to the bus ones */
		int i;

		for(i=0; i<3; i++) {
			bus->resource[i] =
				&dev->resource[PCI_BRIDGE_RESOURCES+i];
			bus->resource[i]->name = bus->name;
		}
		bus->resource[0]->flags |= pci_bridge_check_io(dev);
		bus->resource[1]->flags |= IORESOURCE_MEM;
		/* For now, propogate hose limits to the bus;
		   we'll adjust them later. */
		bus->resource[0]->end = hose->io_resource->end;
		bus->resource[1]->end = hose->mem_resource->end;
		/* Turn off downstream PF memory address range by default */
		bus->resource[2]->start = 1024*1024;
		bus->resource[2]->end = bus->resource[2]->start - 1;
	}
}
#endif
