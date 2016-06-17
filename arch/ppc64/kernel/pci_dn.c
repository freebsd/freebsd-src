/*
 * pci_dn.c
 *
 * Copyright (C) 2001 Todd Inglett, IBM Corporation
 *
 * PCI manipulation via device_nodes.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *    
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/bootmem.h>

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/irq.h>
#include <asm/prom.h>
#include <asm/machdep.h>
#include <asm/init.h>
#include <asm/pci-bridge.h>
#include <asm/ppcdebug.h>
#include <asm/naca.h>
#include <asm/pci_dma.h>

#include "pci.h"

/* Traverse_func that inits the PCI fields of the device node.
 * NOTE: this *must* be done before read/write config to the device.
 */
static void * __init
update_dn_pci_info(struct device_node *dn, void *data)
{
	struct pci_controller *phb = (struct pci_controller *)data;
	u32 *regs;
	char *device_type = get_property(dn, "device_type", 0);
	char *status = get_property(dn, "status", 0);

	dn->phb = phb;
	if (device_type && strcmp(device_type, "pci") == 0 && get_property(dn, "class-code", 0) == 0) {
		/* special case for PHB's.  Sigh. */
		regs = (u32 *)get_property(dn, "bus-range", 0);
		dn->busno = regs[0];
		dn->devfn = 0;	/* assumption */
	} else {
		regs = (u32 *)get_property(dn, "reg", 0);
		if (regs) {
			/* First register entry is addr (00BBSS00)  */
			dn->busno = (regs[0] >> 16) & 0xff;
			dn->devfn = (regs[0] >> 8) & 0xff;
		}
	}
	if (status && strcmp(status, "ok") != 0) {
		char *name = get_property(dn, "name", 0);
		printk(KERN_ERR "PCI: %04x:%02x.%x %s (%s) has bad status from firmware! (%s)", dn->busno, PCI_SLOT(dn->devfn), PCI_FUNC(dn->devfn), name ? name : "<no name>", device_type ? device_type : "<unknown type>", status);
		dn->status = 1;
	}
	return NULL;
}

/*
 * Hit all the BARs of all the devices with values from OF.
 * This is unnecessary on most systems, but also harmless.
 */
static void * __init
write_OF_bars(struct device_node *dn, void *data)
{
#ifdef CONFIG_PPC_PSERIES
	int i;
	u32 oldbar, newbar, newbartest;
	u8  config_offset;
#endif
	char *name = get_property(dn, "name", 0);
	char *device_type = get_property(dn, "device_type", 0);
	char devname[128];
	sprintf(devname, "%04x:%02x.%x %s (%s)", dn->busno, PCI_SLOT(dn->devfn), PCI_FUNC(dn->devfn), name ? name : "<no name>", device_type ? device_type : "<unknown type>");

	if (device_type && strcmp(device_type, "pci") == 0 &&
	    get_property(dn, "class-code", 0) == 0)
		return NULL;	/* This is probably a phb.  Skip it. */

	if (dn->n_addrs == 0)
		return NULL;	/* This is normal for some adapters or bridges */

	if (dn->addrs == NULL) {
		/* This shouldn't happen. */
		printk(KERN_WARNING "write_OF_bars %s: device has %d BARs, but no addrs recorded\n", devname, dn->n_addrs);
		return NULL;
	}

#ifndef CONFIG_PPC_ISERIES 
	for (i = 0; i < dn->n_addrs; i++) {
		newbar = dn->addrs[i].address;
		config_offset = dn->addrs[i].space & 0xff;
		if (ppc_md.pcibios_read_config_dword(dn, config_offset, &oldbar) != PCIBIOS_SUCCESSFUL) {
			printk(KERN_WARNING "write_OF_bars %s: read BAR%d failed\n", devname, i);
			continue;
		}
		/* Need to update this BAR. */
		if (ppc_md.pcibios_write_config_dword(dn, config_offset, newbar) != PCIBIOS_SUCCESSFUL) {
			printk(KERN_WARNING "write_OF_bars %s: write BAR%d with 0x%08x failed (old was 0x%08x)\n", devname, i, newbar, oldbar);
			continue;
		}
		/* sanity check */
		if (ppc_md.pcibios_read_config_dword(dn, config_offset, &newbartest) != PCIBIOS_SUCCESSFUL) {
			printk(KERN_WARNING "write_OF_bars %s: sanity test read BAR%d failed?\n", devname, i);
			continue;
		}
		if ((newbar & PCI_BASE_ADDRESS_MEM_MASK) != (newbartest & PCI_BASE_ADDRESS_MEM_MASK)) {
			printk(KERN_WARNING "write_OF_bars %s: oops...BAR%d read back as 0x%08x%s!\n", devname, i, newbartest, (oldbar & PCI_BASE_ADDRESS_MEM_MASK) == (newbartest & PCI_BASE_ADDRESS_MEM_MASK) ? " (original value)" : "");
			continue;
		}
	}
#endif
	return NULL; 
}

#if 0
/* Traverse_func that starts the BIST (self test) */
static void * __init
startBIST(struct device_node *dn, void *data)
{
	struct pci_controller *phb = (struct pci_controller *)data;
	u8 bist;

	char *name = get_property(dn, "name", 0);
	udbg_printf("startBIST: %s phb=%p, device=%p\n", name ? name : "<unknown>", phb, dn);

	if (ppc_md.pcibios_read_config_byte(dn, PCI_BIST, &bist) == PCIBIOS_SUCCESSFUL) {
		if (bist & PCI_BIST_CAPABLE) {
			udbg_printf("  -> is BIST capable!\n", phb, dn);
			/* Start bist here */
		}
	}
	return NULL;
}
#endif


/******************************************************************
 * Traverse a device tree stopping each PCI device in the tree.
 * This is done depth first.  As each node is processed, a "pre"
 * function is called, the children are processed recursively, and
 * then a "post" function is called.
 *
 * The "pre" and "post" funcs return a value.  If non-zero
 * is returned from the "pre" func, the traversal stops and this
 * value is returned.  The return value from "post" is not used.
 * This return value is useful when using traverse as
 * a method of finding a device.
 *
 * NOTE: we do not run the funcs for devices that do not appear to
 * be PCI except for the start node which we assume (this is good
 * because the start node is often a phb which may be missing PCI
 * properties).
 * We use the class-code as an indicator. If we run into
 * one of these nodes we also assume its siblings are non-pci for
 * performance.
 *
 ******************************************************************/
void *traverse_pci_devices(struct device_node *start, traverse_func pre, traverse_func post, void *data)
{
	struct device_node *dn, *nextdn;
	void *ret;

	if (pre && (ret = pre(start, data)) != NULL)
		return ret;
	for (dn = start->child; dn; dn = nextdn) {
		nextdn = NULL;
		if (get_property(dn, "class-code", 0)) {
			if (pre && (ret = pre(dn, data)) != NULL)
				return ret;
			if (dn->child) {
				/* Depth first...do children */
				nextdn = dn->child;
			} else if (dn->sibling) {
				/* ok, try next sibling instead. */
				nextdn = dn->sibling;
			} else {
				/* no more children or siblings...call "post" */
				if (post)
					post(dn, data);
			}
		}
		if (!nextdn) {
			/* Walk up to next valid sibling. */
			do {
				dn = dn->parent;
				if (dn == start)
					return NULL;
			} while (dn->sibling == NULL);
			nextdn = dn->sibling;
		}
	}
	return NULL;
}

/* Same as traverse_pci_devices except this does it for all phbs.
 */
void *traverse_all_pci_devices(traverse_func pre)
{
	struct pci_controller* phb;
	void *ret;
	for (phb=hose_head;phb;phb=phb->next)
		if ((ret = traverse_pci_devices((struct device_node *)phb->arch_data, pre, NULL, phb)) != NULL)
			return ret;
	return NULL;
}


/* Traversal func that looks for a <busno,devfcn> value.
 * If found, the device_node is returned (thus terminating the traversal).
 */
static void *
is_devfn_node(struct device_node *dn, void *data)
{
	int busno = ((unsigned long)data >> 8) & 0xff;
	int devfn = ((unsigned long)data) & 0xff;
	return (devfn == dn->devfn && busno == dn->busno) ? dn : NULL;
}

/* Same as is_devfn_node except ignore the "fn" part of the "devfn".
 */
static void *
is_devfn_sub_node(struct device_node *dn, void *data)
{
	int busno = ((unsigned long)data >> 8) & 0xff;
	int devfn = ((unsigned long)data) & 0xf8;
	return (devfn == (dn->devfn & 0xf8) && busno == dn->busno) ? dn : NULL;
}

/* Given an existing EADs (pci bridge) device node create a fake one
 * that will simulate function zero.  Make it a sibling of other_eads.
 */
static struct device_node *
create_eads_node(struct device_node *other_eads)
{
	struct device_node *eads = (struct device_node *)kmalloc(sizeof(struct device_node), GFP_KERNEL);

	if (!eads) return NULL;	/* huh? */
	*eads = *other_eads;
	eads->devfn &= ~7;	/* make it function zero */
	eads->tce_table = NULL;
	/*
	 * NOTE: share properties.  We could copy but for now this should
	 * suffice.  The full_name is also incorrect...but seems harmless.
	 */
	eads->child = NULL;
	eads->next = NULL;
	other_eads->allnext = eads;
	other_eads->sibling = eads;
	return eads;
}

/* This is the "slow" path for looking up a device_node from a
 * pci_dev.  It will hunt for the device under it's parent's
 * phb and then update sysdata for a future fastpath.
 *
 * It may also do fixups on the actual device since this happens
 * on the first read/write.
 *
 * Note that it also must deal with devices that don't exist.
 * In this case it may probe for real hardware ("just in case")
 * and add a device_node to the device tree if necessary.
 *
 */
struct device_node *fetch_dev_dn(struct pci_dev *dev)
{
	struct device_node *orig_dn = (struct device_node *)dev->sysdata;
	struct pci_controller *phb = orig_dn->phb; /* assume same phb as orig_dn */
	struct device_node *phb_dn;
	struct device_node *dn;
	unsigned long searchval = (dev->bus->number << 8) | dev->devfn;

	phb_dn = (struct device_node *)(phb->arch_data);
	dn = (struct device_node *)traverse_pci_devices(phb_dn, is_devfn_node, NULL, (void *)searchval);
	if (dn) {
		dev->sysdata = dn;
		/* ToDo: call some device init hook here */
	} else {
		/* Now it is very possible that we can't find the device
		 * because it is not the zero'th device of a mutifunction
		 * device and we don't have permission to read the zero'th
		 * device.  If this is the case, Linux would ordinarily skip
		 * all the other functions.
		 */
		if ((searchval & 0x7) == 0) {
			struct device_node *thisdevdn;
			/* Ok, we are looking for fn == 0.  Let's check for other functions. */
			thisdevdn = (struct device_node *)traverse_pci_devices(phb_dn, is_devfn_sub_node, NULL, (void *)searchval);
			if (thisdevdn) {
				/* Ah ha!  There does exist a sub function.
				 * Now this isn't an exact match for
				 * searchval, but in order to get Linux to
				 * believe the sub functions exist we will
				 * need to manufacture a fake device_node for
				 * this zero'th function.  To keept this
				 * simple for now we only handle pci bridges
				 * and we just hand back the found node which
				 * isn't correct, but Linux won't care.
				 */
				char *device_type = (char *)get_property(thisdevdn, "device_type", 0);
				if (device_type && strcmp(device_type, "pci") == 0) {
					return create_eads_node(thisdevdn);
				}
			}
		}
		/* ToDo: device not found...probe for it anyway with a fake dn?
		struct device_node fake_dn;
		memset(&fake_dn, 0, sizeof(fake_dn));
		fake_dn.phb = phb;
		fake_dn.busno = dev->bus->number;
		fake_dn.devfn = dev->devfn;
		... now do ppc_md.pcibios_read_config_dword(&fake_dn.....)
		 ... if ok, alloc a real device_node and dn = real_dn;
		 */
	}
	return dn;
}


/******************************************************************
 * Actually initialize the phbs.
 * The buswalk on this phb has not happened yet.
 ******************************************************************/
void __init
pci_devs_phb_init(void)
{
	/* This must be done first so the device nodes have valid pci info! */
	traverse_all_pci_devices(update_dn_pci_info);

	/* Hack for regatta which does not init the bars correctly */
	traverse_all_pci_devices(write_OF_bars);
#if 0
	traverse_all_pci_devices(startBIST);
	mdelay(5000);
	traverse_all_pci_devices(checkBIST);
#endif
}


static void __init
pci_fixup_bus_sysdata_list(struct list_head *bus_list)
{
	struct list_head *ln;
	struct pci_bus *bus;
	struct pci_controller *phb;
	int newnum;

	for (ln=bus_list->next; ln != bus_list; ln=ln->next) {
		bus = pci_bus_b(ln);
		if (bus->self) {
			bus->sysdata = bus->self->sysdata;
			/* Also fixup the bus number on large bus systems to
			 * include the PHB# in the next byte
			 */
			phb = PCI_GET_DN(bus)->phb;
			if (phb && phb->buid) {
				newnum = (phb->global_number << 8) | bus->number;
				bus->number = newnum;
				sprintf(bus->name, "PCI Bus #%x", bus->number);
			}
		}
		pci_fixup_bus_sysdata_list(&bus->children);
	}
}

/******************************************************************
 * Fixup the bus->sysdata ptrs to point to the bus' device_node.
 * This is done late in pcibios_init().  We do this mostly for
 * sanity, but pci_dma.c uses these at DMA time so they must be
 * correct.
 * To do this we recurse down the bus hierarchy.  Note that PHB's
 * have bus->self == NULL, but fortunately bus->sysdata is already
 * correct in this case.
 ******************************************************************/
void __init
pci_fix_bus_sysdata(void)
{
	pci_fixup_bus_sysdata_list(&pci_root_buses);
}
