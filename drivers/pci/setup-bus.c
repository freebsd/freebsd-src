/*
 *	drivers/pci/setup-bus.c
 *
 * Extruded from code written by
 *      Dave Rusling (david.rusling@reo.mts.dec.com)
 *      David Mosberger (davidm@cs.arizona.edu)
 *	David Miller (davem@redhat.com)
 *
 * Support routines for initializing a PCI subsystem.
 */

/*
 * Nov 2000, Ivan Kokshaysky <ink@jurassic.park.msu.ru>
 *	     PCI-PCI bridges cleanup, sorted resource allocation.
 * Feb 2002, Ivan Kokshaysky <ink@jurassic.park.msu.ru>
 *	     Converted to allocation in 3 passes, which gives
 *	     tighter packing. Prefetchable range support.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/cache.h>
#include <linux/slab.h>


#define DEBUG_CONFIG 1
#if DEBUG_CONFIG
# define DBGC(args)     printk args
#else
# define DBGC(args)
#endif

#define ROUND_UP(x, a)		(((x) + (a) - 1) & ~((a) - 1))

static int __init
pbus_assign_resources_sorted(struct pci_bus *bus)
{
	struct list_head *ln;
	struct resource *res;
	struct resource_list head, *list, *tmp;
	int idx, found_vga = 0;

	head.next = NULL;
	for (ln=bus->devices.next; ln != &bus->devices; ln=ln->next) {
		struct pci_dev *dev = pci_dev_b(ln);
		u16 class = dev->class >> 8;
		u16 cmd;

		/* First, disable the device to avoid side
		   effects of possibly overlapping I/O and
		   memory ranges.
		   Leave VGA enabled - for obvious reason. :-)
		   Same with all sorts of bridges - they may
		   have VGA behind them.  */
		if (class == PCI_CLASS_DISPLAY_VGA
				|| class == PCI_CLASS_NOT_DEFINED_VGA)
			found_vga = 1;
		else if (class >> 8 != PCI_BASE_CLASS_BRIDGE) {
			pci_read_config_word(dev, PCI_COMMAND, &cmd);
			cmd &= ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY
						| PCI_COMMAND_MASTER);
			pci_write_config_word(dev, PCI_COMMAND, cmd);
		}

		pdev_sort_resources(dev, &head);
	}

	for (list = head.next; list;) {
		res = list->res;
		idx = res - &list->dev->resource[0];
		pci_assign_resource(list->dev, idx);
		tmp = list;
		list = list->next;
		kfree(tmp);
	}

	return found_vga;
}

/* Initialize bridges with base/limit values we have collected.
   PCI-to-PCI Bridge Architecture Specification rev. 1.1 (1998)
   requires that if there is no I/O ports or memory behind the
   bridge, corresponding range must be turned off by writing base
   value greater than limit to the bridge's base/limit registers.  */
static void __init
pci_setup_bridge(struct pci_bus *bus)
{
	struct pbus_set_ranges_data ranges;
	struct pci_dev *bridge = bus->self;
	u32 l;

	if (!bridge || (bridge->class >> 8) != PCI_CLASS_BRIDGE_PCI)
		return;

	ranges.io_start = bus->resource[0]->start;
	ranges.io_end = bus->resource[0]->end;
	ranges.mem_start = bus->resource[1]->start;
	ranges.mem_end = bus->resource[1]->end;
	ranges.prefetch_start = bus->resource[2]->start;
	ranges.prefetch_end = bus->resource[2]->end;
	pcibios_fixup_pbus_ranges(bus, &ranges);

	DBGC((KERN_INFO "PCI: Bus %d, bridge: %s\n",
			bus->number, bridge->name));

	/* Set up the top and bottom of the PCI I/O segment for this bus. */
	if (bus->resource[0]->flags & IORESOURCE_IO) {
		pci_read_config_dword(bridge, PCI_IO_BASE, &l);
		l &= 0xffff0000;
		l |= (ranges.io_start >> 8) & 0x00f0;
		l |= ranges.io_end & 0xf000;
		/* Set up upper 16 bits of I/O base/limit. */
		pci_write_config_word(bridge, PCI_IO_BASE_UPPER16,
				      ranges.io_start >> 16);
		pci_write_config_word(bridge, PCI_IO_LIMIT_UPPER16,
				      ranges.io_end >> 16);
		DBGC((KERN_INFO "  IO window: %04lx-%04lx\n",
				ranges.io_start, ranges.io_end));
	}
	else {
		/* Clear upper 16 bits of I/O base/limit. */
		pci_write_config_dword(bridge, PCI_IO_BASE_UPPER16, 0);
		l = 0x00f0;
		DBGC((KERN_INFO "  IO window: disabled.\n"));
	}
	pci_write_config_dword(bridge, PCI_IO_BASE, l);

	/* Set up the top and bottom of the PCI Memory segment
	   for this bus. */
	if (bus->resource[1]->flags & IORESOURCE_MEM) {
		l = (ranges.mem_start >> 16) & 0xfff0;
		l |= ranges.mem_end & 0xfff00000;
		DBGC((KERN_INFO "  MEM window: %08lx-%08lx\n",
				ranges.mem_start, ranges.mem_end));
	}
	else {
		l = 0x0000fff0;
		DBGC((KERN_INFO "  MEM window: disabled.\n"));
	}
	pci_write_config_dword(bridge, PCI_MEMORY_BASE, l);

	/* Clear out the upper 32 bits of PREF base/limit. */
	pci_write_config_dword(bridge, PCI_PREF_BASE_UPPER32, 0);
	pci_write_config_dword(bridge, PCI_PREF_LIMIT_UPPER32, 0);

	/* Set up PREF base/limit. */
	if (bus->resource[2]->flags & IORESOURCE_PREFETCH) {
		l = (ranges.prefetch_start >> 16) & 0xfff0;
		l |= ranges.prefetch_end & 0xfff00000;
		DBGC((KERN_INFO "  PREFETCH window: %08lx-%08lx\n",
				ranges.prefetch_start, ranges.prefetch_end));
	}
	else {
		l = 0x0000fff0;
		DBGC((KERN_INFO "  PREFETCH window: disabled.\n"));
	}
	pci_write_config_dword(bridge, PCI_PREF_MEMORY_BASE, l);

	/* Check if we have VGA behind the bridge.
	   Enable ISA in either case (FIXME!). */
	l = (bus->resource[0]->flags & IORESOURCE_BUS_HAS_VGA) ? 0x0c : 0x04;
	pci_write_config_word(bridge, PCI_BRIDGE_CONTROL, l);
}

/* Check whether the bridge supports optional I/O and
   prefetchable memory ranges. If not, the respective
   base/limit registers must be read-only and read as 0. */
static void __init
pci_bridge_check_ranges(struct pci_bus *bus)
{
	u16 io;
	u32 pmem;
	struct pci_dev *bridge = bus->self;
	struct resource *b_res;

	if (!bridge || (bridge->class >> 8) != PCI_CLASS_BRIDGE_PCI)
		return;

	b_res = &bridge->resource[PCI_BRIDGE_RESOURCES];
	b_res[1].flags |= IORESOURCE_MEM;

	pci_read_config_word(bridge, PCI_IO_BASE, &io);
	if (!io) {
		pci_write_config_word(bridge, PCI_IO_BASE, 0xf0f0);
		pci_read_config_word(bridge, PCI_IO_BASE, &io);
 		pci_write_config_word(bridge, PCI_IO_BASE, 0x0);
 	}
 	if (io)
		b_res[0].flags |= IORESOURCE_IO;
	/*  DECchip 21050 pass 2 errata: the bridge may miss an address
	    disconnect boundary by one PCI data phase.
	    Workaround: do not use prefetching on this device. */
	if (bridge->vendor == PCI_VENDOR_ID_DEC && bridge->device == 0x0001)
		return;
	pci_read_config_dword(bridge, PCI_PREF_MEMORY_BASE, &pmem);
	if (!pmem) {
		pci_write_config_dword(bridge, PCI_PREF_MEMORY_BASE,
					       0xfff0fff0);
		pci_read_config_dword(bridge, PCI_PREF_MEMORY_BASE, &pmem);
		pci_write_config_dword(bridge, PCI_PREF_MEMORY_BASE, 0x0);
	}
	if (pmem)
		b_res[2].flags |= IORESOURCE_MEM | IORESOURCE_PREFETCH;
}

/* Sizing the IO windows of the PCI-PCI bridge is trivial,
   since these windows have 4K granularity and the IO ranges
   of non-bridge PCI devices are limited to 256 bytes.
   We must be careful with the ISA aliasing though. */
static void __init
pbus_size_io(struct pci_bus *bus)
{
	struct list_head *ln;
	struct resource *b_res = bus->resource[0];
	unsigned long size = 0, size1 = 0;

	if (!(b_res->flags & IORESOURCE_IO))
		return;

	for (ln=bus->devices.next; ln != &bus->devices; ln=ln->next) {
		struct pci_dev *dev = pci_dev_b(ln);
		int i;
		
		for (i = 0; i < PCI_NUM_RESOURCES; i++) {
			struct resource *r = &dev->resource[i];
			unsigned long r_size;

			if (r->parent || !(r->flags & IORESOURCE_IO))
				continue;
			r_size = r->end - r->start + 1;

			if (r_size < 0x400)
				/* Might be re-aligned for ISA */
				size += r_size;
			else
				size1 += r_size;
		}
		/* ??? Reserve some resources for CardBus. */
		if ((dev->class >> 8) == PCI_CLASS_BRIDGE_CARDBUS)
			size1 += 4*1024;
	}
/* To be fixed in 2.5: we should have sort of HAVE_ISA
   flag in the struct pci_bus. */
#if defined(CONFIG_ISA) || defined(CONFIG_EISA)
	size = (size & 0xff) + ((size & ~0xffUL) << 2);
#endif
	size = ROUND_UP(size + size1, 4096);
	if (!size) {
		b_res->flags = 0;
		return;
	}
	/* Alignment of the IO window is always 4K */
	b_res->start = 4096;
	b_res->end = b_res->start + size - 1;
}

/* Calculate the size of the bus and minimal alignment which
   guarantees that all child resources fit in this size. */
static void __init
pbus_size_mem(struct pci_bus *bus, unsigned long mask, unsigned long type)
{
	struct list_head *ln;
	unsigned long min_align, align, size;
	unsigned long aligns[12];	/* Alignments from 1Mb to 2Gb */
	int order, max_order;
	struct resource *b_res = (type & IORESOURCE_PREFETCH) ?
				 bus->resource[2] : bus->resource[1];

	memset(aligns, 0, sizeof(aligns));
	max_order = 0;
	size = 0;

	for (ln=bus->devices.next; ln != &bus->devices; ln=ln->next) {
		struct pci_dev *dev = pci_dev_b(ln);
		int i;
		
		for (i = 0; i < PCI_NUM_RESOURCES; i++) {
			struct resource *r = &dev->resource[i];
			unsigned long r_size;

			if (r->parent || (r->flags & mask) != type)
				continue;
			r_size = r->end - r->start + 1;
			/* For bridges size != alignment */
			align = (i < PCI_BRIDGE_RESOURCES) ? r_size : r->start;
			order = ffz(~align) - 20;
			if (order > 11) {
				printk(KERN_WARNING "PCI: region %s/%d "
				       "too large: %lx-%lx\n",
				       dev->slot_name, i, r->start, r->end);
				r->flags = 0;
				continue;
			}
			size += r_size;
			if (order < 0)
				order = 0;
			/* Exclude ranges with size > align from
			   calculation of the alignment. */
			if (r_size == align)
				aligns[order] += align;
			if (order > max_order)
				max_order = order;
		}
		/* ??? Reserve some resources for CardBus. */
		if ((dev->class >> 8) == PCI_CLASS_BRIDGE_CARDBUS) {
			size += 1UL << 24;		/* 16 Mb */
			aligns[24 - 20] += 1UL << 24;
		}
	}

	align = 0;
	min_align = 0;
	for (order = 0; order <= max_order; order++) {
		unsigned long align1 = 1UL << (order + 20);

		if (!align)
			min_align = align1;
		else if (ROUND_UP(align + min_align, min_align) < align1)
			min_align = align1 >> 1;
		align += aligns[order];
	}
	size = ROUND_UP(size, min_align);
	if (!size) {
		b_res->flags = 0;
		return;
	}
	b_res->start = min_align;
	b_res->end = size + min_align - 1;
}

void __init
pbus_size_bridges(struct pci_bus *bus)
{
	struct list_head *ln;
	unsigned long mask, type;

	for (ln=bus->children.next; ln != &bus->children; ln=ln->next)
		pbus_size_bridges(pci_bus_b(ln));

	/* The root bus? */
	if (!bus->self)
		return;

	pci_bridge_check_ranges(bus);

	pbus_size_io(bus);

	mask = type = IORESOURCE_MEM;
	/* If the bridge supports prefetchable range, size it separately. */
	if (bus->resource[2] &&
	    bus->resource[2]->flags & IORESOURCE_PREFETCH) {
		pbus_size_mem(bus, IORESOURCE_PREFETCH, IORESOURCE_PREFETCH);
		mask |= IORESOURCE_PREFETCH;	/* Size non-prefetch only. */
	}
	pbus_size_mem(bus, mask, type);
}

void __init
pbus_assign_resources(struct pci_bus *bus)
{
	struct list_head *ln;
	int found_vga = pbus_assign_resources_sorted(bus);

	if (found_vga) {
		struct pci_bus *b;

		/* Propagate presence of the VGA to upstream bridges */
		for (b = bus; b->parent; b = b->parent) {
			b->resource[0]->flags |= IORESOURCE_BUS_HAS_VGA;
		}
	}
	for (ln=bus->children.next; ln != &bus->children; ln=ln->next) {
		struct pci_bus *b = pci_bus_b(ln);

		pbus_assign_resources(b);
		pci_setup_bridge(b);
	}
}

void __init
pci_assign_unassigned_resources(void)
{
	struct list_head *ln;
	struct pci_dev *dev;

	/* Depth first, calculate sizes and alignments of all
	   subordinate buses. */
	for(ln=pci_root_buses.next; ln != &pci_root_buses; ln=ln->next)
		pbus_size_bridges(pci_bus_b(ln));
	/* Depth last, allocate resources and update the hardware. */
	for(ln=pci_root_buses.next; ln != &pci_root_buses; ln=ln->next)
		pbus_assign_resources(pci_bus_b(ln));

	pci_for_each_dev(dev) {
		pdev_enable_device(dev);
	}
}
