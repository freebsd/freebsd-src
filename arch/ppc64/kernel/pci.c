/*
 * Port for PPC64 David Engebretsen, IBM Corp.
 *   Contains common pci routines for ppc64 platform, pSeries and iSeries brands. 
 * 
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/capability.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/bootmem.h>

#include <asm/processor.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/pci-bridge.h>
#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/flight_recorder.h>
#include <asm/ppcdebug.h>
#include <asm/naca.h>
#include <asm/pci_dma.h>
#include <asm/machdep.h>

#include "pci.h"

/* pci_io_base -- the base address from which io bars are offsets.
 * This is the lowest I/O base address (so bar values are always positive),
 * and it *must* be the start of ISA space if an ISA bus exists because
 * ISA drivers use hard coded offsets.  If no ISA bus exists a dummy
 * page is mapped and isa_io_limit prevents access to it.
 */
unsigned long isa_io_base     = 0;	/* NULL if no ISA bus */
unsigned long pci_io_base     = 0;
unsigned long isa_mem_base    = 0;
unsigned long pci_dram_offset = 0;

/******************************************************************
 * Forward declare of prototypes
 ******************************************************************/
static void pcibios_fixup_resources(struct pci_dev* dev);
static void fixup_broken_pcnet32(struct pci_dev* dev);
static void fixup_windbond_82c105(struct pci_dev* dev);
void        fixup_resources(struct pci_dev* dev);

void   iSeries_pcibios_init(void);
void   pSeries_pcibios_init(void);

int    pci_assign_all_busses = 0;
struct pci_controller* hose_head;
struct pci_controller** hose_tail = &hose_head;

LIST_HEAD(iSeries_Global_Device_List);

/*******************************************************************
 * Counters and control flags. 
 *******************************************************************/
long   Pci_Io_Read_Count  = 0;
long   Pci_Io_Write_Count = 0;
long   Pci_Cfg_Read_Count = 0;
long   Pci_Cfg_Write_Count= 0;
long   Pci_Error_Count    = 0;

int    Pci_Retry_Max      = 7;	/* Retry set to 7 times  */	
int    Pci_Error_Flag     = 1;	/* Set Retry Error on. */
int    Pci_Trace_Flag     = 0;

/******************************************************************
 * 
 ******************************************************************/
int  global_phb_number    = 0;           /* Global phb counter    */
int  Pci_Large_Bus_System = 0;
int  Pci_Set_IOA_Address  = 0;
int  Pci_Manage_Phb_Space = 0;
struct pci_controller *phbtab[PCI_MAX_PHB];

static int pci_bus_count;

/* Cached ISA bridge dev. */
struct pci_dev *ppc64_isabridge_dev = NULL;

struct pci_fixup pcibios_fixups[] = {
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_TRIDENT,	PCI_ANY_ID, fixup_broken_pcnet32 },
	{ PCI_FIXUP_HEADER,	PCI_VENDOR_ID_WINBOND,	PCI_DEVICE_ID_WINBOND_82C105, fixup_windbond_82c105 },
	{ PCI_FIXUP_HEADER, PCI_ANY_ID,	PCI_ANY_ID, pcibios_fixup_resources },
 	{ 0 }
};

static void fixup_broken_pcnet32(struct pci_dev* dev)
{
	if ((dev->class>>8 == PCI_CLASS_NETWORK_ETHERNET)) {
		dev->vendor = PCI_VENDOR_ID_AMD;
		pci_write_config_word(dev, PCI_VENDOR_ID, PCI_VENDOR_ID_AMD);
		pci_name_device(dev);
	}
}

static void fixup_windbond_82c105(struct pci_dev* dev)
{
	/* Assume the windbond 82c105 is the IDE controller on a
	 * p610.  We should probably be more careful in case
	 * someone tries to plug in a similar adapter.
	 */
	unsigned int reg;

	printk("Using INTC for W82c105 IDE controller.\n");
	pci_read_config_dword(dev, 0x40, &reg);
	/* Enable LEGIRQ to use INTC instead of ISA interrupts */
	pci_write_config_dword(dev, 0x40, reg | (1<<11));
}


/* Given an mmio phys address, find a pci device that implements
 * this address.  This is of course expensive, but only used
 * for device initialization or error paths.
 * For io BARs it is assumed the pci_io_base has already been added
 * into addr.
 *
 * Bridges are ignored although they could be used to optimize the search.
 */
struct pci_dev *pci_find_dev_by_addr(unsigned long addr)
{
	struct pci_dev *dev;
	int i;
	unsigned long ioaddr;

	ioaddr = (addr > _IO_BASE) ? addr - _IO_BASE : 0;

	pci_for_each_dev(dev) {
  	        if ((dev->class >> 16) == PCI_BASE_CLASS_BRIDGE)
			continue;
		for (i = 0; i < DEVICE_COUNT_RESOURCE; i++) {
			unsigned long start = pci_resource_start(dev,i);
			unsigned long end = pci_resource_end(dev,i);
			unsigned int flags = pci_resource_flags(dev,i);
			if (start == 0 || ~start == 0 ||
			    end == 0 || ~end == 0)
				continue;
			if ((flags & IORESOURCE_IO) &&
			    (ioaddr >= start && ioaddr <= end))
				return dev;
			else if ((flags & IORESOURCE_MEM) &&
				 (addr >= start && addr <= end))
				return dev;
		}
	}
	return NULL;
}

void pcibios_fixup_pbus_ranges(struct pci_bus *pbus,
				struct pbus_set_ranges_data *pranges)
{
}

void
pcibios_update_resource(struct pci_dev *dev, struct resource *root,
			     struct resource *res, int resource)
{
	u32 new, check;
	int reg;
	struct pci_controller* hose = PCI_GET_PHB_PTR(dev);
	
	new = res->start;
	if (hose && res->flags & IORESOURCE_MEM)
		new -= hose->pci_mem_offset;
	new |= (res->flags & PCI_REGION_FLAG_MASK);
	if (resource < 6) {
		reg = PCI_BASE_ADDRESS_0 + 4*resource;
	} else if (resource == PCI_ROM_RESOURCE) {
		res->flags |= PCI_ROM_ADDRESS_ENABLE;
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

static void
pcibios_fixup_resources(struct pci_dev* dev)
{
	fixup_resources(dev);
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
pcibios_align_resource(void *data, struct resource *res, unsigned long size,
		       unsigned long align)

{
	struct pci_dev *dev = data;

	if (res->flags & IORESOURCE_IO) {
		unsigned long start = res->start;

		if (size > 0x100) {
			printk(KERN_ERR "PCI: Can not align I/O Region %s %s because size %ld is too large.\n",
                                        dev->slot_name, res->name, size);
		}

		if (start & 0x300) {
			start = (start + 0x3ff) & ~0x3ff;
			res->start = start;
		}
	}
}

/*
 *  Handle resources of PCI devices.  If the world were perfect, we could
 *  just allocate all the resource regions and do nothing more.  It isn't.
 *  On the other hand, we cannot just re-allocate all devices, as it would
 *  require us to know lots of host bridge internals.  So we attempt to
 *  keep as much of the original configuration as possible, but tweak it
 *  when it's found to be wrong.
 *
 *  Known BIOS problems we have to work around:
 *	- I/O or memory regions not configured
 *	- regions configured, but not enabled in the command register
 *	- bogus I/O addresses above 64K used
 *	- expansion ROMs left enabled (this may sound harmless, but given
 *	  the fact the PCI specs explicitly allow address decoders to be
 *	  shared between expansion ROMs and other resource regions, it's
 *	  at least dangerous)
 *
 *  Our solution:
 *	(1) Allocate resources for all buses behind PCI-to-PCI bridges.
 *	    This gives us fixed barriers on where we can allocate.
 *	(2) Allocate resources for all enabled devices.  If there is
 *	    a collision, just mark the resource as unallocated. Also
 *	    disable expansion ROMs during this step.
 *	(3) Try to allocate resources for disabled devices.  If the
 *	    resources were assigned correctly, everything goes well,
 *	    if they weren't, they won't disturb allocation of other
 *	    resources.
 *	(4) Assign new addresses to resources which were either
 *	    not configured at all or misconfigured.  If explicitly
 *	    requested by the user, configure expansion ROM address
 *	    as well.
 */

static void __init
pcibios_allocate_bus_resources(struct list_head *bus_list)
{
	struct list_head *ln;
	struct pci_bus *bus;
	int i;
	struct resource *res, *pr;

	/* Depth-First Search on bus tree */
	for (ln=bus_list->next; ln != bus_list; ln=ln->next) {
		bus = pci_bus_b(ln);
		for (i = 0; i < 4; ++i) {
			if ((res = bus->resource[i]) == NULL || !res->flags)
				continue;
			if (bus->parent == NULL)
				pr = (res->flags & IORESOURCE_IO)?
					&ioport_resource: &iomem_resource;
			else
				pr = pci_find_parent_resource(bus->self, res);

			if (pr == res)
				continue;	/* transparent bus or undefined */
			if (pr && request_resource(pr, res) == 0)
				continue;
			printk(KERN_ERR "PCI: Cannot allocate resource region "
			       "%d of PCI bridge %x\n", i, bus->number);
			printk(KERN_ERR "PCI: resource is %lx..%lx (%lx), parent %p\n",
			    res->start, res->end, res->flags, pr);
		}
		pcibios_allocate_bus_resources(&bus->children);
	}
}

static void __init
pcibios_allocate_resources(int pass)
{
	struct pci_dev *dev;
	int idx, disabled;
	u16 command;
	struct resource *r, *pr;

	pci_for_each_dev(dev) {
		pci_read_config_word(dev, PCI_COMMAND, &command);
		for(idx = 0; idx < 6; idx++) {
			r = &dev->resource[idx];
			if (r->parent)		/* Already allocated */
				continue;
			if (!r->start)		/* Address not assigned at all */
				continue;

			if (r->flags & IORESOURCE_IO)
				disabled = !(command & PCI_COMMAND_IO);
			else
				disabled = !(command & PCI_COMMAND_MEMORY);
			if (pass == disabled) {
				PPCDBG(PPCDBG_PHBINIT,
				       "PCI: Resource %08lx-%08lx (f=%lx, d=%d, p=%d)\n",
				       r->start, r->end, r->flags, disabled, pass);
				pr = pci_find_parent_resource(dev, r);
				if (!pr || request_resource(pr, r) < 0) {
					PPCDBG(PPCDBG_PHBINIT,
					       "PCI: Cannot allocate resource region %d of device %s, pr = 0x%lx\n", idx, dev->slot_name, pr);
					if(pr) {
					PPCDBG(PPCDBG_PHBINIT,
					       "PCI: Cannot allocate resource 0x%lx\n", request_resource(pr,r));
					}
					/* We'll assign a new address later */
					r->end -= r->start;
					r->start = 0;
				}
			}
		}
		if (!pass) {
			r = &dev->resource[PCI_ROM_RESOURCE];
			if (r->flags & PCI_ROM_ADDRESS_ENABLE) {
				/* Turn the ROM off, leave the resource region, but keep it unregistered. */
				u32 reg;
				r->flags &= ~PCI_ROM_ADDRESS_ENABLE;
				pci_read_config_dword(dev, dev->rom_base_reg, &reg);
				pci_write_config_dword(dev, dev->rom_base_reg, reg & ~PCI_ROM_ADDRESS_ENABLE);
			}
		}
	}
}

static void __init
pcibios_assign_resources(void)
{
	struct pci_dev *dev;
	int idx;
	struct resource *r;

	pci_for_each_dev(dev) {
		int class = dev->class >> 8;

		/* Don't touch classless devices and host bridges */
		if (!class || class == PCI_CLASS_BRIDGE_HOST)
			continue;

		for(idx=0; idx<6; idx++) {
			r = &dev->resource[idx];

			/*
			 *  Don't touch IDE controllers and I/O ports of video cards!
			 */
			if ((class == PCI_CLASS_STORAGE_IDE && idx < 4) ||
			    (class == PCI_CLASS_DISPLAY_VGA && (r->flags & IORESOURCE_IO)))
				continue;

			/*
			 *  We shall assign a new address to this resource, either because
			 *  the BIOS forgot to do so or because we have decided the old
			 *  address was unusable for some reason.
			 */
			if (!r->start && r->end && ppc_md.pcibios_enable_device_hook &&
			    !ppc_md.pcibios_enable_device_hook(dev, 1))
				pci_assign_resource(dev, idx);
		}

		if (0) { /* don't assign ROMs */
			r = &dev->resource[PCI_ROM_RESOURCE];
			r->end -= r->start;
			r->start = 0;
			if (r->end)
				pci_assign_resource(dev, PCI_ROM_RESOURCE);
		}
	}
}


int
pcibios_enable_resources(struct pci_dev *dev, int mask)
{
	u16 cmd, old_cmd;
	int idx;
	struct resource *r;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	old_cmd = cmd;
	for(idx=0; idx<6; idx++) {
		if(!(mask & (1<<idx)))
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

/* 
 * Allocate pci_controller(phb) initialized common variables. 
 */
struct pci_controller * __init
pci_alloc_pci_controller(char *model, enum phb_types controller_type)
{
        struct pci_controller *hose;
        PPCDBG(PPCDBG_PHBINIT, "PCI: Allocate pci_controller for %s\n",model);
        hose = (struct pci_controller *)alloc_bootmem(sizeof(struct pci_controller));
        if(hose == NULL) {
                printk(KERN_ERR "PCI: Allocate pci_controller failed.\n");
                return NULL;
        }
        memset(hose, 0, sizeof(struct pci_controller));
        if(strlen(model) < 8)
		strcpy(hose->what,model);
        else
		memcpy(hose->what,model,7);
        hose->type = controller_type;
        hose->global_number = global_phb_number;
	phbtab[global_phb_number++] = hose;
        
        *hose_tail = hose;
        hose_tail = &hose->next;
        return hose;
}

/*
 * This fixup is arch independent and probably should go somewhere else.
 */
void __init
pcibios_generic_fixup(void)
{
	struct pci_dev *dev;

	/* Fix miss-identified vendor AMD pcnet32 adapters. */
	dev = NULL;
	while ((dev = pci_find_device(PCI_VENDOR_ID_TRIDENT, PCI_DEVICE_ID_AMD_LANCE, dev)) != NULL &&
	       dev->class == (PCI_CLASS_NETWORK_ETHERNET << 8))
		dev->vendor = PCI_VENDOR_ID_AMD;
}



/*********************************************************************** 
 *
 *
 * 
 ***********************************************************************/
void __init
pcibios_init(void)
{
	struct pci_controller *hose;
	struct pci_bus *bus;
	int    next_busno;

#ifndef CONFIG_PPC_ISERIES
	pSeries_pcibios_init();
#else
	iSeries_pcibios_init(); 
#endif

	ppc64_boot_msg(0x40, "PCI Probe");
	printk("PCI: Probing PCI hardware\n");
	PPCDBG(PPCDBG_BUSWALK,"PCI: Probing PCI hardware\n");

	/* Scan all of the recorded PCI controllers.  */
	for (next_busno = 0, hose = hose_head; hose; hose = hose->next) {
		hose->last_busno = 0xff;
		bus = pci_scan_bus(hose->first_busno, hose->ops, hose->arch_data);
		hose->bus = bus;
		hose->last_busno = bus->subordinate;
		if (pci_assign_all_busses || next_busno <= hose->last_busno)
			next_busno = hose->last_busno+1;
	}
	pci_bus_count = next_busno;

	/* Call machine dependant fixup */
	if (ppc_md.pcibios_fixup) {
		ppc_md.pcibios_fixup();
	}

	/* Generic fixups */
	pcibios_generic_fixup();

	/* Allocate and assign resources */
	pcibios_allocate_bus_resources(&pci_root_buses);
	pcibios_allocate_resources(0);
	pcibios_allocate_resources(1);
	pcibios_assign_resources();

#ifndef CONFIG_PPC_ISERIES
	pci_fix_bus_sysdata();

	create_tce_tables();
	PPCDBG(PPCDBG_BUSWALK,"pSeries create_tce_tables()\n");
#endif

	/* Cache the location of the ISA bridge (if we have one) */
	ppc64_isabridge_dev = pci_find_class(PCI_CLASS_BRIDGE_ISA << 8, NULL);
	if (ppc64_isabridge_dev != NULL )
		printk("ISA bridge at %s\n", ppc64_isabridge_dev->slot_name);

	printk("PCI: Probing PCI hardware done\n");
	PPCDBG(PPCDBG_BUSWALK,"PCI: Probing PCI hardware done.\n");
	ppc64_boot_msg(0x41, "PCI Done");

}

int __init
pcibios_assign_all_busses(void)
{
	return pci_assign_all_busses;
}

unsigned long resource_fixup(struct pci_dev * dev, struct resource * res,
			     unsigned long start, unsigned long size)
{
	return start;
}

void __init pcibios_fixup_bus(struct pci_bus *bus)
{
#ifndef CONFIG_PPC_ISERIES
	struct pci_controller *phb = PCI_GET_PHB_PTR(bus);
	struct resource *res;
	int i;

	if (bus->parent == NULL) {
		/* This is a host bridge - fill in its resources */
		phb->bus = bus;
		bus->resource[0] = res = &phb->io_resource;
		if (!res->flags)
			BUG();	/* No I/O resource for this PHB? */

		for (i = 0; i < 3; ++i) {
			res = &phb->mem_resources[i];
			if (!res->flags) {
				if (i == 0)
					BUG();	/* No memory resource for this PHB? */
			}
			bus->resource[i+1] = res;
		}
	} else {
		/* This is a subordinate bridge */
		pci_read_bridge_bases(bus);

		for (i = 0; i < 4; ++i) {
			if ((res = bus->resource[i]) == NULL)
				continue;
			if (!res->flags)
				continue;
			if (res == pci_find_parent_resource(bus->self, res)) {
				/* Transparent resource -- don't try to "fix" it. */
				continue;
			}
			if (res->flags & IORESOURCE_IO) {
				unsigned long offset = (unsigned long)phb->io_base_virt - pci_io_base;
				res->start += offset;
				res->end += offset;
			} else if (phb->pci_mem_offset
				   && (res->flags & IORESOURCE_MEM)) {
				if (res->start < phb->pci_mem_offset) {
					res->start += phb->pci_mem_offset;
					res->end += phb->pci_mem_offset;
				}
			}
		}
	}
#endif	
	if ( ppc_md.pcibios_fixup_bus )
		ppc_md.pcibios_fixup_bus(bus);
}

char __init *pcibios_setup(char *str)
{
	return str;
}

int pcibios_enable_device(struct pci_dev *dev, int mask)
{
	u16 cmd, old_cmd;
	int idx;
	struct resource *r;

	PPCDBG(PPCDBG_BUSWALK,"PCI: %s for device %s \n",__FUNCTION__,dev->slot_name);
	if (ppc_md.pcibios_enable_device_hook)
		if (ppc_md.pcibios_enable_device_hook(dev, 0))
			return -EINVAL;
			
	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	old_cmd = cmd;
	for (idx=0; idx<6; idx++) {
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
	if (cmd != old_cmd) {
		printk("PCI: Enabling device %s (%04x -> %04x)\n",
		       dev->slot_name, old_cmd, cmd);
		PPCDBG(PPCDBG_BUSWALK,"PCI: Enabling device %s \n",dev->slot_name);
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
	return 0;
}

struct pci_controller*
pci_bus_to_hose(int bus)
{
	struct pci_controller* hose = hose_head;

	for (; hose; hose = hose->next)
		if (bus >= hose->first_busno && bus <= hose->last_busno)
			return hose;
	return NULL;
}

void*
pci_bus_io_base(unsigned int bus)
{
	struct pci_controller *hose;

	hose = pci_bus_to_hose(bus);
	if (!hose)
		return NULL;
	return hose->io_base_virt;
}

unsigned long
pci_bus_io_base_phys(unsigned int bus)
{
	struct pci_controller *hose;

	hose = pci_bus_to_hose(bus);
	if (!hose)
		return 0;
	return hose->io_base_phys;
}

unsigned long
pci_bus_mem_base_phys(unsigned int bus)
{
	struct pci_controller *hose;

	hose = pci_bus_to_hose(bus);
	if (!hose)
		return 0;
	return hose->pci_mem_offset;
}

/*
 * Return the index of the PCI controller for device pdev.
 */
int pci_controller_num(struct pci_dev *dev)
{
	struct pci_controller *hose = PCI_GET_PHB_PTR(dev);

	return hose->global_number;
}

/*
 * Platform support for /proc/bus/pci/X/Y mmap()s,
 * modelled on the sparc64 implementation by Dave Miller.
 *  -- paulus.
 */

/*
 * Adjust vm_pgoff of VMA such that it is the physical page offset
 * corresponding to the 32-bit pci bus offset for DEV requested by the user.
 *
 * Basically, the user finds the base address for his device which he wishes
 * to mmap.  They read the 32-bit value from the config space base register,
 * add whatever PAGE_SIZE multiple offset they wish, and feed this into the
 * offset parameter of mmap on /proc/bus/pci/XXX for that device.
 *
 * Returns negative error code on failure, zero on success.
 */
static __inline__ int
__pci_mmap_make_offset(struct pci_dev *dev, struct vm_area_struct *vma,
		       enum pci_mmap_state mmap_state)
{
	struct pci_controller *hose = PCI_GET_PHB_PTR(dev);
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long io_offset = 0;
	int i, res_bit;

	if (hose == 0)
		return -EINVAL;		/* should never happen */

	/* If memory, add on the PCI bridge address offset */
	if (mmap_state == pci_mmap_mem) {
		offset += hose->pci_mem_offset;
		res_bit = IORESOURCE_MEM;
	} else {
		io_offset = (unsigned long)hose->io_base_virt;
		offset += io_offset;
		res_bit = IORESOURCE_IO;
	}

	/*
	 * Check that the offset requested corresponds to one of the
	 * resources of the device.
	 */
	for (i = 0; i <= PCI_ROM_RESOURCE; i++) {
		struct resource *rp = &dev->resource[i];
		int flags = rp->flags;

		/* treat ROM as memory (should be already) */
		if (i == PCI_ROM_RESOURCE)
			flags |= IORESOURCE_MEM;

		/* Active and same type? */
		if ((flags & res_bit) == 0)
			continue;

		/* In the range of this resource? */
		if (offset < (rp->start & PAGE_MASK) || offset > rp->end)
			continue;

		/* found it! construct the final physical address */
		if (mmap_state == pci_mmap_io)
			offset += hose->io_base_phys - io_offset;

		vma->vm_pgoff = offset >> PAGE_SHIFT;
		return 0;
	}

	return -EINVAL;
}

/*
 * Set vm_flags of VMA, as appropriate for this architecture, for a pci device
 * mapping.
 */
static __inline__ void
__pci_mmap_set_flags(struct pci_dev *dev, struct vm_area_struct *vma,
		     enum pci_mmap_state mmap_state)
{
	vma->vm_flags |= VM_SHM | VM_LOCKED | VM_IO;
}

/*
 * Set vm_page_prot of VMA, as appropriate for this architecture, for a pci
 * device mapping.
 */
static __inline__ void
__pci_mmap_set_pgprot(struct pci_dev *dev, struct vm_area_struct *vma,
		      enum pci_mmap_state mmap_state, int write_combine)
{
	long prot = pgprot_val(vma->vm_page_prot);

	/* XXX would be nice to have a way to ask for write-through */
	prot |= _PAGE_NO_CACHE;
	prot |= _PAGE_GUARDED;
	vma->vm_page_prot = __pgprot(prot);
}

/*
 * Perform the actual remap of the pages for a PCI device mapping, as
 * appropriate for this architecture.  The region in the process to map
 * is described by vm_start and vm_end members of VMA, the base physical
 * address is found in vm_pgoff.
 * The pci device structure is provided so that architectures may make mapping
 * decisions on a per-device or per-bus basis.
 *
 * Returns a negative error code on failure, zero on success.
 */
int pci_mmap_page_range(struct pci_dev *dev, struct vm_area_struct *vma,
			enum pci_mmap_state mmap_state,
			int write_combine)
{
	int ret;

	ret = __pci_mmap_make_offset(dev, vma, mmap_state);
	if (ret < 0)
		return ret;

	__pci_mmap_set_flags(dev, vma, mmap_state);
	__pci_mmap_set_pgprot(dev, vma, mmap_state, write_combine);

	ret = remap_page_range(vma->vm_start, vma->vm_pgoff << PAGE_SHIFT,
			       vma->vm_end - vma->vm_start, vma->vm_page_prot);

	return ret;
}

/* Provide information on locations of various I/O regions in physical
 * memory.  Do this on a per-card basis so that we choose the right
 * root bridge.
 * Note that the returned IO or memory base is a physical address
 */

long
sys_pciconfig_iobase(long which, unsigned long bus, unsigned long devfn)
{
	struct pci_controller* hose = pci_bus_to_hose(bus);
	long result = -EOPNOTSUPP;

	if (!hose)
		return -ENODEV;
	
	switch (which) {
	case IOBASE_BRIDGE_NUMBER:
		return (long)hose->first_busno;
	case IOBASE_MEMORY:
		return (long)hose->pci_mem_offset;
	case IOBASE_IO:
		return (long)hose->io_base_phys;
	case IOBASE_ISA_IO:
		return (long)isa_io_base;
	case IOBASE_ISA_MEM:
		return (long)isa_mem_base;
	}

	return result;
}
/************************************************************************/
/* Formats the device information and location for service.             */
/* - Pass in pci_dev* pointer to the device.                            */
/* - Pass in buffer to place the data.  Danger here is the buffer must  */
/*   be as big as the client says it is.   Should be at least 128 bytes.*/
/* Return will the length of the string data put in the buffer.         */
/* The brand specific method device_Location is called.                 */
/* Format:                                                              */
/* PCI: Bus  0, Device 26, Vendor 0x12AE  Frame  1, Card  C10  Ethernet */
/* PCI: Bus  0, Device 26, Vendor 0x12AE  Location U0.3-P1-I8  Ethernet */
/* For pSeries, see the Product Topology in the RS/6000 Architecture.   */
/* For iSeries, see the Service Manuals.                                */
/************************************************************************/
int  format_device_location(struct pci_dev* PciDev,char* BufPtr, int BufferSize)
{
	struct device_node* DevNode = (struct device_node*)PciDev->sysdata;
	int  LineLen = 0;
	if (DevNode != NULL && BufferSize >= 128) {
		LineLen += device_Location(PciDev,BufPtr+LineLen);
		LineLen += sprintf(BufPtr+LineLen," %12s",pci_class_name(PciDev->class >> 8) );
	}
	return LineLen;
}
/************************************************************************
 * Saves the config registers for a device.                             *
 ************************************************************************
 * Note: This does byte reads so the data may appear byte swapped,      *
 * The data returned in the pci_config_reg_save_area structure can be   *
 * used to the restore of the data.  If the save failed, the data       *
 * will not be restore.  Yes I know, you are most likey toast.          *
 ************************************************************************/
int pci_save_config_regs(struct pci_dev* PciDev,struct pci_config_reg_save_area* SaveArea)
{
	memset(SaveArea,0x00,sizeof(struct pci_config_reg_save_area) );
	SaveArea->PciDev    = PciDev;
	SaveArea->RCode     = 0;
	SaveArea->Register  = 0;
	/******************************************************************
	 * Save All the Regs,  NOTE: restore skips the first 16 bytes.    *
	 ******************************************************************/
	while (SaveArea->Register < REG_SAVE_SIZE && SaveArea->RCode == 0) {
		SaveArea->RCode = pci_read_config_byte(PciDev, SaveArea->Register, &SaveArea->Regs[SaveArea->Register]);
		++SaveArea->Register;
	}
	if (SaveArea->RCode != 0) { 	/* Ouch */
		SaveArea->Flags = 0x80;	
		printk("PCI: pci_restore_save_regs failed! %p\n 0x%04X",PciDev,SaveArea->RCode);
		PCIFR(      "pci_restore_save_regs failed! %p\n 0x%04X",PciDev,SaveArea->RCode);
	}
	else {
		SaveArea->Flags = 0x01;
	}
	return  SaveArea->RCode;
}

/************************************************************************
 * Restores the registers saved via the save function.  See the save    *
 * function for details.                                                *
 ************************************************************************/
int pci_restore_config_regs(struct pci_dev* PciDev,struct pci_config_reg_save_area* SaveArea)
{
 	if (SaveArea->PciDev != PciDev || SaveArea->Flags == 0x80 || SaveArea->RCode != 0) {
		printk("PCI: pci_restore_config_regs failed! %p\n",PciDev);
		return -1;
	}
	/******************************************************************
	 * Don't touch the Cmd or BIST regs, user must restore those.     *
	 * Restore PCI_VENDOR_ID & PCI_DEVICE_ID                          *
	 * Restore PCI_CACHE_LINE_SIZE & PCI_LATENCY_TIMER                *
	 * Restore Saved Regs from 0x10 to 0x3F                           * 
	 ******************************************************************/
	SaveArea->Register = 0;
	while(SaveArea->Register < REG_SAVE_SIZE && SaveArea->RCode == 0) {
		SaveArea->RCode = pci_write_config_byte(PciDev,SaveArea->Register,SaveArea->Regs[SaveArea->Register]);
		++SaveArea->Register;
		if (     SaveArea->Register == PCI_COMMAND)     SaveArea->Register = PCI_CACHE_LINE_SIZE;
		else if (SaveArea->Register == PCI_HEADER_TYPE) SaveArea->Register = PCI_BASE_ADDRESS_0;
	}
	if (SaveArea->RCode != 0) {
		printk("PCI: pci_restore_config_regs failed! %p\n 0x%04X",PciDev,SaveArea->RCode);
		PCIFR(      "pci_restore_config_regs failed! %p\n 0x%04X",PciDev,SaveArea->RCode);
	}
	return  SaveArea->RCode;
}

/************************************************************************/
/* Interface to toggle the reset line                                   */
/* Time is in .1 seconds, need for seconds.                             */
/************************************************************************/
int  pci_reset_device(struct pci_dev* PciDev, int AssertTime, int DelayTime)
{
	unsigned long AssertDelay, WaitDelay;
	int RtnCode;
	/********************************************************************
	 * Set defaults, Assert is .5 second, Wait is 3 seconds.
	 ********************************************************************/
	if (AssertTime == 0) AssertDelay = ( 5 * HZ)/10;
	else                 AssertDelay = (AssertTime*HZ)/10;
	if (WaitDelay == 0)  WaitDelay   = (30 * HZ)/10;
	else                 WaitDelay   = (DelayTime* HZ)/10;

	/********************************************************************
	 * Assert reset, wait, de-assert reset, wait for IOA to reset.
	 * - Don't waste the CPU time on jiffies.
	 ********************************************************************/
	RtnCode = pci_set_reset(PciDev,1);
	if (RtnCode == 0) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(AssertDelay);   /* Sleep for the time     */
		RtnCode = pci_set_reset(PciDev,0);
		set_current_state(TASK_UNINTERRUPTIBLE);  
		schedule_timeout(WaitDelay);
	}
	if (RtnCode == 0) {
		PCIFR(      "Bus%3d, Device%3d, Reset\n",PciDev->bus->number,PCI_SLOT(PciDev->devfn) );
	} 
	else {
		printk("PCI: Bus%3d, Device%3d, Reset Failed:0x%04X\n",PciDev->bus->number,PCI_SLOT(PciDev->devfn),RtnCode );
		PCIFR(      "Bus%3d, Device%3d, Reset Failed:0x%04X\n",PciDev->bus->number,PCI_SLOT(PciDev->devfn),RtnCode );
	}
	return RtnCode;
}

/*****************************************************
 * Dump Resource information
 *****************************************************/
void dumpResources(struct resource* Resource)
{
	if(Resource != NULL) {
		int Flags = 0x00000F00 & Resource->flags;
		if(Resource->start == 0 && Resource->end == 0) return;
		else if(Resource->start == Resource->end )     return;
		else {
			if     (Flags == IORESOURCE_IO)  udbg_printf("IO.:");
			else if(Flags == IORESOURCE_MEM) udbg_printf("MEM:");
			else if(Flags == IORESOURCE_IRQ) udbg_printf("IRQ:");
			else                             udbg_printf("0x%02X:",Resource->flags);

		}
		udbg_printf("0x%016LX / 0x%016LX (0x%08X)\n",
			    Resource->start, Resource->end, Resource->end - Resource->start);
	}
}

int  resourceSize(struct resource* Resource)
{
	if(Resource->start == 0 && Resource->end == 0) return 0;
	else if(Resource->start == Resource->end )     return 0;
	else return (Resource->end-1)-Resource->start;
}


/*****************************************************
 * Dump PHB information for Debug
 *****************************************************/
void dumpPci_Controller(struct pci_controller* phb)
{
	udbg_printf("\tpci_controller= 0x%016LX\n", phb);
	if (phb != NULL) {
		udbg_printf("\twhat & type   = %s 0x%02X\n ",phb->what,phb->type);
		udbg_printf("\tbus           = ");
		if (phb->bus != NULL) udbg_printf("0x%02X\n",   phb->bus->number);
		else                  udbg_printf("<NULL>\n");
		udbg_printf("\tarch_data     = 0x%016LX\n", phb->arch_data);
		udbg_printf("\tfirst_busno   = 0x%02X\n",   phb->first_busno);
		udbg_printf("\tlast_busno    = 0x%02X\n",   phb->last_busno);
		udbg_printf("\tio_base_virt* = 0x%016LX\n", phb->io_base_virt);
		udbg_printf("\tio_base_phys  = 0x%016LX\n", phb->io_base_phys);
		udbg_printf("\tpci_mem_offset= 0x%016LX\n", phb->pci_mem_offset);
		udbg_printf("\tpci_io_offset = 0x%016LX\n", phb->pci_io_offset);

		udbg_printf("\tcfg_addr      = 0x%016LX\n", phb->cfg_addr);
		udbg_printf("\tcfg_data      = 0x%016LX\n", phb->cfg_data);
		udbg_printf("\tphb_regs      = 0x%016LX\n", phb->phb_regs);
		udbg_printf("\tchip_regs     = 0x%016LX\n", phb->chip_regs);


		udbg_printf("\tResources\n");
		dumpResources(&phb->io_resource);
		if (phb->mem_resource_count >  0) dumpResources(&phb->mem_resources[0]);
		if (phb->mem_resource_count >  1) dumpResources(&phb->mem_resources[1]);
		if (phb->mem_resource_count >  2) dumpResources(&phb->mem_resources[2]);

		udbg_printf("\tglobal_num    = 0x%02X\n",   phb->global_number);
		udbg_printf("\tlocal_num     = 0x%02X\n",   phb->local_number);
	}
}

/*****************************************************
 * Dump PHB information for Debug
 *****************************************************/
void dumpPci_Bus(struct pci_bus* Pci_Bus)
{
	int i;
	udbg_printf("\tpci_bus         = 0x%016LX   \n",Pci_Bus);
	if (Pci_Bus != NULL) {

		udbg_printf("\tnumber          = 0x%02X     \n",Pci_Bus->number);
		udbg_printf("\tprimary         = 0x%02X     \n",Pci_Bus->primary);
		udbg_printf("\tsecondary       = 0x%02X     \n",Pci_Bus->secondary);
		udbg_printf("\tsubordinate     = 0x%02X     \n",Pci_Bus->subordinate);

		for (i=0;i<4;++i) {
			if(Pci_Bus->resource[i] == NULL) continue;
			if(Pci_Bus->resource[i]->start == 0 && Pci_Bus->resource[i]->end == 0) break;
			udbg_printf("\tResources[%d]",i);
			dumpResources(Pci_Bus->resource[i]);
		}
	}
}

/*****************************************************
 * Dump Device information for Debug
 *****************************************************/
void dumpPci_Dev(struct pci_dev* Pci_Dev)
{
	int i;
	udbg_printf("\tpci_dev*        = 0x%p\n",Pci_Dev);
	if ( Pci_Dev == NULL )  return;
	udbg_printf("\tname            = %s  \n",Pci_Dev->name);
	udbg_printf("\tbus*            = 0x%p\n",Pci_Dev->bus);
	udbg_printf("\tsysdata*        = 0x%p\n",Pci_Dev->sysdata);
	udbg_printf("\tDevice          = 0x%4X%02X:%02X.%02X 0x%04X:%04X\n",
		    PCI_GET_PHB_NUMBER(Pci_Dev),
		    PCI_GET_BUS_NUMBER(Pci_Dev),
		    PCI_SLOT(Pci_Dev->devfn),
		    PCI_FUNC(Pci_Dev->devfn),
		    Pci_Dev->vendor,
		    Pci_Dev->device);
	udbg_printf("\tHdr/Irq         = 0x%02X/0x%02X \n",Pci_Dev->hdr_type,Pci_Dev->irq);
	for (i=0;i<DEVICE_COUNT_RESOURCE;++i) {
		if (Pci_Dev->resource[i].start == 0 && Pci_Dev->resource[i].end == 0) continue;
		udbg_printf("\tResources[%d] ",i);
		dumpResources(&Pci_Dev->resource[i]);
	}
	dumpResources(&Pci_Dev->resource[i]);
}
