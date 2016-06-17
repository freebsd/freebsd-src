/* $Id: pci.c,v 1.6 2000/01/29 00:12:05 grundler Exp $
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1997, 1998 Ralf Baechle
 * Copyright (C) 1999 SuSE GmbH
 * Copyright (C) 1999-2001 Hewlett-Packard Company
 * Copyright (C) 1999-2001 Grant Grundler
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>		/* for __init and __devinit */
#include <linux/pci.h>
#include <linux/slab.h>

#include <asm/io.h>
#include <asm/system.h>
#include <asm/cache.h>		/* for L1_CACHE_BYTES */

#define DEBUG_RESOURCES 0
#define DEBUG_CONFIG 0

#if DEBUG_CONFIG
# define DBGC(x...)     printk(KERN_DEBUG x)
#else
# define DBGC(x...)
#endif


#if DEBUG_RESOURCES
#define DBG_RES(x...)	printk(KERN_DEBUG x)
#else
#define DBG_RES(x...)
#endif

/* To be used as: mdelay(pci_post_reset_delay);
**
** post_reset is the time the kernel should stall to prevent anyone from
** accessing the PCI bus once #RESET is de-asserted. 
** PCI spec somewhere says 1 second but with multi-PCI bus systems,
** this makes the boot time much longer than necessary.
** 20ms seems to work for all the HP PCI implementations to date.
*/
int pci_post_reset_delay = 50;

struct pci_port_ops *pci_port;
struct pci_bios_ops *pci_bios;

int pci_hba_count = 0;

/*
** parisc_pci_hba used by pci_port->in/out() ops to lookup bus data.
*/
#define PCI_HBA_MAX 32
struct pci_hba_data *parisc_pci_hba[PCI_HBA_MAX];


/********************************************************************
**
** I/O port space support
**
*********************************************************************/

/* EISA port numbers and PCI port numbers share the same interface.  Some
 * machines have both EISA and PCI adapters installed.  Rather than turn
 * pci_port into an array, we reserve bus 0 for EISA and call the EISA
 * routines if the access is to a port on bus 0.  We don't want to fix
 * EISA and ISA drivers which assume port space is <= 0xffff.
 */

#ifdef CONFIG_EISA
#define EISA_IN(size) if (EISA_bus && (b == 0)) return eisa_in##size(addr)
#define EISA_OUT(size) if (EISA_bus && (b == 0)) return eisa_out##size(d, addr)
#else
#define EISA_IN(size)
#define EISA_OUT(size)
#endif

#define PCI_PORT_IN(type, size) \
u##size in##type (int addr) \
{ \
	int b = PCI_PORT_HBA(addr); \
	u##size d = (u##size) -1; \
	EISA_IN(size); \
	ASSERT(pci_port); /* make sure services are defined */ \
	ASSERT(parisc_pci_hba[b]); /* make sure ioaddr are "fixed up" */ \
	if (parisc_pci_hba[b] == NULL) { \
		printk(KERN_WARNING "\nPCI or EISA Host Bus Adapter %d not registered. in" #size "(0x%x) returning -1\n", b, addr); \
	} else { \
		d = pci_port->in##type(parisc_pci_hba[b], PCI_PORT_ADDR(addr)); \
	} \
	return d; \
}

PCI_PORT_IN(b,  8)
PCI_PORT_IN(w, 16)
PCI_PORT_IN(l, 32)


#define PCI_PORT_OUT(type, size) \
void out##type (u##size d, int addr) \
{ \
	int b = PCI_PORT_HBA(addr); \
	EISA_OUT(size); \
	ASSERT(pci_port); \
	pci_port->out##type(parisc_pci_hba[b], PCI_PORT_ADDR(addr), d); \
}

PCI_PORT_OUT(b,  8)
PCI_PORT_OUT(w, 16)
PCI_PORT_OUT(l, 32)



/*
 * BIOS32 replacement.
 */
void pcibios_init(void)
{
	if (!pci_bios)
		return;

	if (pci_bios->init) {
		pci_bios->init();
	} else {
		printk(KERN_WARNING "pci_bios != NULL but init() is!\n");
	}
}


/* Called from pci_do_scan_bus() *after* walking a bus but before walking PPBs. */
void pcibios_fixup_bus(struct pci_bus *bus)
{
	ASSERT(pci_bios != NULL);

	if (pci_bios->fixup_bus) {
		pci_bios->fixup_bus(bus);
	} else {
		printk(KERN_WARNING "pci_bios != NULL but fixup_bus() is!\n");
	}
}


char *pcibios_setup(char *str)
{
	return str;
}


/*
** Used in drivers/pci/quirks.c
*/
struct pci_fixup pcibios_fixups[] = { {0} };


/*
** called by drivers/pci/setup.c:pdev_fixup_irq()
*/
void __devinit pcibios_update_irq(struct pci_dev *dev, int irq)
{
/*
** updates IRQ_LINE cfg register to reflect PCI-PCI bridge skewing.
**
** Calling path for Alpha is:
**  alpha/kernel/pci.c:common_init_pci(swizzle_func, pci_map_irq_func )
**	drivers/pci/setup.c:pci_fixup_irqs()
**	    drivers/pci/setup.c:pci_fixup_irq()	(for each PCI device)
**		invoke swizzle and map functions
**	        alpha/kernel/pci.c:pcibios_update_irq()
**
** Don't need this for PA legacy PDC systems.
**
** On PAT PDC systems, We only support one "swizzle" for any number
** of PCI-PCI bridges deep. That's how bit3 PCI expansion chassis
** are implemented. The IRQ lines are "skewed" for all devices but
** *NOT* routed through the PCI-PCI bridge. Ie any device "0" will
** share an IRQ line. Legacy PDC is expecting this IRQ line routing
** as well.
**
** Unfortunately, PCI spec allows the IRQ lines to be routed
** around the PCI bridge as long as the IRQ lines are skewed
** based on the device number...<sigh>...
**
** Lastly, dino.c might be able to use pci_fixup_irq() to
** support RS-232 and PS/2 children. Not sure how but it's
** something to think about.
*/
}


/* ------------------------------------
**
** Program one BAR in PCI config space.
**
** ------------------------------------
** PAT PDC systems need this routine. PA legacy PDC does not.
**
** When BAR's are configured by linux, this routine will update
** configuration space with the "normalized" address. "root" indicates
** where the range starts and res is some portion of that range.
**
** VCLASS: For all PA-RISC systems except V-class, root->start would be zero.
**
** PAT PDC can tell us which MMIO ranges are available or already in use.
** I/O port space and such are not memory mapped anyway for PA-Risc.
*/
void __devinit
pcibios_update_resource(
	struct pci_dev *dev,
	struct resource *root,
	struct resource *res,
	int barnum
	)
{
	int where;
	u32 barval = 0;

	DBG_RES("pcibios_update_resource(%s, ..., %d) [%lx,%lx]/%x\n",
		dev->slot_name,
		barnum, res->start, res->end, (int) res->flags);

	if (barnum >= PCI_BRIDGE_RESOURCES) {
		/* handled in PCI-PCI bridge specific support */
		return;
	}

	if (barnum == PCI_ROM_RESOURCE) {
		where = PCI_ROM_ADDRESS;
	} else {
		/* 0-5  standard PCI "regions" */
		where = PCI_BASE_ADDRESS_0 + (barnum * 4);
	}

	if (res->flags & IORESOURCE_IO) {
		barval = PCI_PORT_ADDR(res->start);
	} else if (res->flags & IORESOURCE_MEM) {
		barval = PCI_BUS_ADDR(HBA_DATA(dev->bus->sysdata), res->start);
	} else {
		panic("pcibios_update_resource() WTF? flags not IO or MEM");
	}

	pci_write_config_dword(dev, where, barval);

/* XXX FIXME - Elroy does support 64-bit (dual cycle) addressing.
** But at least one device (Symbios 53c896) which has 64-bit BAR
** doesn't actually work right with dual cycle addresses.
** So ignore the whole mess for now.
*/

	if ((res->flags & (PCI_BASE_ADDRESS_SPACE
			   | PCI_BASE_ADDRESS_MEM_TYPE_MASK))
	    == (PCI_BASE_ADDRESS_SPACE_MEMORY
		| PCI_BASE_ADDRESS_MEM_TYPE_64)) {
		pci_write_config_dword(dev, where+4, 0);
		DBGC("PCIBIOS: dev %s type 64-bit\n", dev->name);
	}
}

/*
** Called by pci_set_master() - a driver interface.
**
** Legacy PDC guarantees to set:
**      Map Memory BAR's into PA IO space.
**      Map Expansion ROM BAR into one common PA IO space per bus.
**      Map IO BAR's into PCI IO space.
**      Command (see below)
**      Cache Line Size
**      Latency Timer
**      Interrupt Line
**	PPB: secondary latency timer, io/mmio base/limit,
**		bus numbers, bridge control
**
*/
void
pcibios_set_master(struct pci_dev *dev)
{
	u8 lat;

	/* If someone already mucked with this, don't touch it. */
	pci_read_config_byte(dev, PCI_LATENCY_TIMER, &lat);
	if (lat >= 16) return;

	/*
	** HP generally has fewer devices on the bus than other architectures.
	** upper byte is PCI_LATENCY_TIMER.
	*/
        pci_write_config_word(dev, PCI_CACHE_LINE_SIZE,
				(0x80 << 8) | (L1_CACHE_BYTES / sizeof(u32)));
}


void __init
pcibios_init_bus(struct pci_bus *bus)
{
	struct pci_dev *dev = bus->self;

	/* We deal only with pci controllers and pci-pci bridges. */
	if (dev && (dev->class >> 8) != PCI_CLASS_BRIDGE_PCI)
		return;
	
	if (dev) {
		/* PCI-PCI bridge - set the cache line and default latency
		   (32) for primary and secondary buses. */
		pci_write_config_byte(dev, PCI_SEC_LATENCY_TIMER, 32);

		/* Read bridge control - force SERR/PERR on */
		pci_read_config_word(dev, PCI_BRIDGE_CONTROL, &bus->bridge_ctl);
		bus->bridge_ctl |= PCI_BRIDGE_CTL_PARITY | PCI_BRIDGE_CTL_SERR;
		pci_write_config_word(dev, PCI_BRIDGE_CONTROL, bus->bridge_ctl);
	}

	bus->bridge_ctl |= PCI_BRIDGE_CTL_PARITY | PCI_BRIDGE_CTL_SERR;
}


/*
** KLUGE: Link the child and parent resources - generic PCI didn't
*/
static void
pcibios_link_hba_resources( struct resource *hba_res, struct resource *r)
{
	if (!r->parent) {
		r->parent = hba_res;

		/* reverse link is harder *sigh*  */
		if (r->parent->child) {
			if (r->parent->sibling) {
				struct resource *next = r->parent->sibling;
				while (next->sibling)
					 next = next->sibling;
				next->sibling = r;
			} else {
				r->parent->sibling = r;
			}
		} else
			r->parent->child = r;
	}
}

/*
** called by drivers/pci/setup-res.c:pci_setup_bridge().
*/
void pcibios_fixup_pbus_ranges(
	struct pci_bus *bus,
	struct pbus_set_ranges_data *ranges
	)
{
	struct pci_hba_data *hba = HBA_DATA(bus->sysdata);

	/*
	** I/O space may see busnumbers here. Something
	** in the form of 0xbbxxxx where bb is the bus num
	** and xxxx is the I/O port space address.
	** Remaining address translation are done in the
	** PCI Host adapter specific code - ie dino_out8.
	*/
	ranges->io_start = PCI_PORT_ADDR(ranges->io_start);
	ranges->io_end   = PCI_PORT_ADDR(ranges->io_end);

	/* Convert MMIO addr to PCI addr (undo global virtualization) */
	ranges->mem_start = PCI_BUS_ADDR(hba, ranges->mem_start);
	ranges->mem_end   = PCI_BUS_ADDR(hba, ranges->mem_end);

	DBG_RES("pcibios_fixup_pbus_ranges(%02x, [%lx,%lx %lx,%lx])\n", bus->number,
		ranges->io_start, ranges->io_end,
		ranges->mem_start, ranges->mem_end);

	/* KLUGE ALERT
	** if this resource isn't linked to a "parent", then it seems
	** to be a child of the HBA - lets link it in.
	*/
	pcibios_link_hba_resources(&hba->io_space, bus->resource[0]);
	pcibios_link_hba_resources(&hba->lmmio_space, bus->resource[1]);
}

#define MAX(val1, val2)   ((val1) > (val2) ? (val1) : (val2))


/*
** pcibios align resources() is called everytime generic PCI code
** wants to generate a new address. The process of looking for
** an available address, each candidate is first "aligned" and
** then checked if the resource is available until a match is found.
**
** Since we are just checking candidates, don't use any fields other
** than res->start.
*/
void __devinit
pcibios_align_resource(void *data, struct resource *res,
			unsigned long size, unsigned long alignment)
{
	unsigned long mask, align;

	DBG_RES("pcibios_align_resource(%s, (%p) [%lx,%lx]/%x, 0x%lx, 0x%lx)\n",
		((struct pci_dev *) data)->slot_name,
		res->parent, res->start, res->end,
		(int) res->flags, size, alignment);

	/* has resource already been aligned/assigned? */
	if (res->parent)
		return;

	/* If it's not IO, then it's gotta be MEM */
	align = (res->flags & IORESOURCE_IO) ? PCIBIOS_MIN_IO : PCIBIOS_MIN_MEM;

	/* Align to largest of MIN or input size */
	mask = MAX(alignment, align) - 1;
	res->start += mask;
	res->start &= ~mask;

	/*
	** WARNING : caller is expected to update "end" field.
	** We can't since it might really represent the *size*.
	** The difference is "end = start + size" vs "end += start".
	*/
}


int __devinit
pcibios_enable_device(struct pci_dev *dev, int mask)
{
	u16 cmd;
	int idx;

	/*
	** The various platform PDC's (aka "BIOS" for PCs) don't
	** enable all the same bits. We just make sure they are here.
	*/
	pci_read_config_word(dev, PCI_COMMAND, &cmd);

	/*
	** See if any resources have been allocated
	** While "regular" PCI devices only use 0-5, Bridges use a few
	** beyond that for window registers.
	*/
        for (idx=0; idx<DEVICE_COUNT_RESOURCE; idx++) {
		struct resource *r = &dev->resource[idx];

                /* only setup requested resources */
		if (!(mask & (1<<idx)))
			continue;

		if (r->flags & IORESOURCE_IO)
			cmd |= PCI_COMMAND_IO;
		if (r->flags & IORESOURCE_MEM)
			cmd |= PCI_COMMAND_MEMORY;
	}

	/*
	** Enable System error and Parity Error reporting by default.
	** Devices that do NOT want those behaviors should clear them
	** (eg PCI graphics, possibly networking).
	** Interfaces like SCSI certainly should not. We want the
	** system to crash if a system or parity error is detected.
	** At least until the device driver can recover from such an error.
	*/
	cmd |= (PCI_COMMAND_SERR | PCI_COMMAND_PARITY);

#if 0
	/* If bridge/bus controller has FBB enabled, child must too. */
	if (dev->bus->bridge_ctl & PCI_BRIDGE_CTL_FAST_BACK)
		cmd |= PCI_COMMAND_FAST_BACK;
#endif

	DBGC("PCIBIOS: Enabling device %s cmd 0x%04x\n", dev->slot_name, cmd);
	pci_write_config_word(dev, PCI_COMMAND, cmd);
	return 0;
}

void __init
pcibios_setup_host_bridge(struct pci_bus *bus)
{
	ASSERT(pci_bios != NULL);

#if 0
	if (pci_bios)
	{
		if (pci_bios->setup_host_bridge) {
			(*pci_bios->setup_host_bridge)(bus);
		}
	}
#endif
}

static void __devinit
pcibios_enable_ppb(struct pci_bus *bus)
{
	struct list_head *list;

	/* find a leaf of the PCI bus tree. */
        list_for_each(list, &bus->children)
		pcibios_enable_ppb(pci_bus_b(list));

	if (bus->self && (bus->self->class >> 8) == PCI_CLASS_BRIDGE_PCI)
		pdev_enable_device(bus->self);
}


/*
** Mostly copied from drivers/pci/setup-bus.c:pci_assign_unassigned_resources()
*/
void __devinit
pcibios_assign_unassigned_resources(struct pci_bus *bus)
{
	/* from drivers/pci/setup-bus.c */
	extern void pbus_size_bridges(struct pci_bus *bus);
	extern void pbus_assign_resources(struct pci_bus *bus);

	pbus_size_bridges(bus);
	pbus_assign_resources(bus);

	pcibios_enable_ppb(bus);
}

/*
** PARISC specific (unfortunately)
*/
void pcibios_register_hba(struct pci_hba_data *hba)
{
	ASSERT(pci_hba_count < PCI_HBA_MAX);

	/* pci_port->in/out() uses parisc_pci_hba to lookup parameter. */
	parisc_pci_hba[pci_hba_count] = hba;
	hba->hba_num = pci_hba_count++;
}
