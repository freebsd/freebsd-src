/*======================================================================
  
    Cardbus device configuration
    
    cardbus.c 1.87 2002/10/24 06:11:41

    The contents of this file are subject to the Mozilla Public
    License Version 1.1 (the "License"); you may not use this file
    except in compliance with the License. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS
    IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
    implied. See the License for the specific language governing
    rights and limitations under the License.

    The initial developer of the original code is David A. Hinds
    <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
    are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.

    Alternatively, the contents of this file may be used under the
    terms of the GNU General Public License version 2 (the "GPL"), in which
    case the provisions of the GPL are applicable instead of the
    above.  If you wish to allow the use of your version of this file
    only under the terms of the GPL and not to allow others to use
    your version of this file under the MPL, indicate your decision
    by deleting the provisions above and replace them with the notice
    and other provisions required by the GPL.  If you do not delete
    the provisions above, a recipient may use your version of this
    file under either the MPL or the GPL.
    
    These routines handle allocating resources for Cardbus cards, as
    well as setting up and shutting down Cardbus sockets.  They are
    called from cs.c in response to Request/ReleaseConfiguration and
    Request/ReleaseIO calls.

======================================================================*/

/*
 * This file is going away.  Cardbus handling has been re-written to be
 * more of a PCI bridge thing, and the PCI code basically does all the
 * resource handling. This has wrappers to make the rest of the PCMCIA
 * subsystem not notice that it's not here any more.
 *
 *		Linus, Jan 2000
 */


#define __NO_VERSION__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <asm/irq.h>
#include <asm/io.h>

#define IN_CARD_SERVICES
#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/ss.h>
#include <pcmcia/cs.h>
#include <pcmcia/bulkmem.h>
#include <pcmcia/cistpl.h>
#include "cs_internal.h"

#ifdef PCMCIA_DEBUG
static int pc_debug = PCMCIA_DEBUG;
#endif

/*====================================================================*/

#define FIND_FIRST_BIT(n)	((n) - ((n) & ((n)-1)))

#define pci_readb		pci_read_config_byte
#define pci_writeb		pci_write_config_byte
#define pci_readw		pci_read_config_word
#define pci_writew		pci_write_config_word
#define pci_readl		pci_read_config_dword
#define pci_writel		pci_write_config_dword

/* Offsets in the Expansion ROM Image Header */
#define ROM_SIGNATURE		0x0000	/* 2 bytes */
#define ROM_DATA_PTR		0x0018	/* 2 bytes */

/* Offsets in the CardBus PC Card Data Structure */
#define PCDATA_SIGNATURE	0x0000	/* 4 bytes */
#define PCDATA_VPD_PTR		0x0008	/* 2 bytes */
#define PCDATA_LENGTH		0x000a	/* 2 bytes */
#define PCDATA_REVISION		0x000c
#define PCDATA_IMAGE_SZ		0x0010	/* 2 bytes */
#define PCDATA_ROM_LEVEL	0x0012	/* 2 bytes */
#define PCDATA_CODE_TYPE	0x0014
#define PCDATA_INDICATOR	0x0015

typedef struct cb_config_t {
	struct pci_dev dev;
} cb_config_t;

/*=====================================================================

    Expansion ROM's have a special layout, and pointers specify an
    image number and an offset within that image.  xlate_rom_addr()
    converts an image/offset address to an absolute offset from the
    ROM's base address.
    
=====================================================================*/

static u_int xlate_rom_addr(u_char * b, u_int addr)
{
	u_int img = 0, ofs = 0, sz;
	u_short data;
	while ((readb(b) == 0x55) && (readb(b + 1) == 0xaa)) {
		if (img == (addr >> 28))
			return (addr & 0x0fffffff) + ofs;
		data = readb(b + ROM_DATA_PTR) + (readb(b + ROM_DATA_PTR + 1) << 8);
		sz = 512 * (readb(b + data + PCDATA_IMAGE_SZ) +
			    (readb(b + data + PCDATA_IMAGE_SZ + 1) << 8));
		if ((sz == 0) || (readb(b + data + PCDATA_INDICATOR) & 0x80))
			break;
		b += sz;
		ofs += sz;
		img++;
	}
	return 0;
}

/*=====================================================================

    These are similar to setup_cis_mem and release_cis_mem for 16-bit
    cards.  The "result" that is used externally is the cb_cis_virt
    pointer in the socket_info_t structure.
    
=====================================================================*/

void cb_release_cis_mem(socket_info_t * s)
{
	if (s->cb_cis_virt) {
		DEBUG(1, "cs: cb_release_cis_mem()\n");
		iounmap(s->cb_cis_virt);
		s->cb_cis_virt = NULL;
		s->cb_cis_res = 0;
	}
}

static int cb_setup_cis_mem(socket_info_t * s, struct pci_dev *dev, struct resource *res)
{
	unsigned int start, size;

	if (res == s->cb_cis_res)
		return 0;

	if (s->cb_cis_res)
		cb_release_cis_mem(s);

	start = res->start;
	size = res->end - start + 1;
	s->cb_cis_virt = ioremap(start, size);

	if (!s->cb_cis_virt)
		return -1;

	s->cb_cis_res = res;

	return 0;
}

/*=====================================================================

    This is used by the CIS processing code to read CIS information
    from a CardBus device.
    
=====================================================================*/

int read_cb_mem(socket_info_t * s, u_char fn, int space,
		u_int addr, u_int len, void *ptr)
{
	struct pci_dev *dev;
	struct resource *res;

	DEBUG(3, "cs: read_cb_mem(%d, %#x, %u)\n", space, addr, len);

	if (!s->cb_config)
		goto fail;

	dev = &s->cb_config[fn].dev;

	/* Config space? */
	if (space == 0) {
		if (addr + len > 0x100)
			goto fail;
		for (; len; addr++, ptr++, len--)
			pci_readb(dev, addr, (u_char *) ptr);
		return 0;
	}

	res = dev->resource + space - 1;
	if (!res->flags)
		goto fail;

	if (cb_setup_cis_mem(s, dev, res) != 0)
		goto fail;

	if (space == 7) {
		addr = xlate_rom_addr(s->cb_cis_virt, addr);
		if (addr == 0)
			goto fail;
	}

	if (addr + len > res->end - res->start)
		goto fail;

	memcpy_fromio(ptr, s->cb_cis_virt + addr, len);
	return 0;

fail:
	memset(ptr, 0xff, len);
	return -1;
}

struct pci_dev *cb_scan_slot(struct pci_dev *temp, struct list_head *list)
{
	struct pci_dev *dev;
	struct pci_dev *first_dev = NULL;
	int func = 0;
	int is_multi = 0;
	u8 hdr_type;

	for (func = 0; func < 8; func++, temp->devfn++) {
		if (func && !is_multi)		/* not a multi-function device */
			continue;
		if (pci_read_config_byte(temp, PCI_HEADER_TYPE, &hdr_type))
			continue;
		temp->hdr_type = hdr_type & 0x7f;

		dev = pci_scan_device(temp);
		if (!dev)
			continue;
		sprintf(dev->name, "PCI device %04x:%04x", dev->vendor, dev->device);
		if (!func) {
			is_multi = hdr_type & 0x80;
			first_dev = dev;
		}

		list_add_tail(&dev->global_list, list);
		/* Fix up broken headers */
//FIXME		pci_fixup_device(PCI_FIXUP_HEADER, dev);
	}
	return first_dev;
}

static unsigned int cb_scan_new_bus(struct pci_bus *bus, int irq);

static int cb_scan_bridge(struct pci_bus *bus, struct pci_dev * dev, int max, int irq)
{
	unsigned int buses;
	unsigned short cr;
	struct pci_bus *child;
	int is_cardbus = (dev->hdr_type == PCI_HEADER_TYPE_CARDBUS);

	pci_read_config_dword(dev, PCI_PRIMARY_BUS, &buses);
	printk("Scanning behind PCI bridge %s, config %06x\n", dev->slot_name, buses & 0xffffff);
	/*
	 * We need to assign a number to this bus which we always
	 * do in the second pass. We also keep all address decoders
	 * on the bridge disabled during scanning.  FIXME: Why?
	 */
	pci_read_config_word(dev, PCI_COMMAND, &cr);
	pci_write_config_word(dev, PCI_COMMAND, 0x0000);
	pci_write_config_word(dev, PCI_STATUS, 0xffff);

	child = pci_add_new_bus(bus, dev, ++max);
	buses = (buses & 0xff000000)
	      | ((unsigned int)(child->primary)     <<  0)
	      | ((unsigned int)(child->secondary)   <<  8)
	      | ((unsigned int)(child->subordinate) << 16);
	/*
	 * We need to blast all three values with a single write.
	 */
	pci_write_config_dword(dev, PCI_PRIMARY_BUS, buses);
	if (!is_cardbus) {
		/* Now we can scan all subordinate buses... */
		max = cb_scan_new_bus(child, irq);
	} else {
		/*
		 * For CardBus bridges, we leave 4 bus numbers
		 * as cards with a PCI-to-PCI bridge can be
		 * inserted later.
		 */
		max += 3;
	}
	/*
	 * Set the subordinate bus number to its real value.
	 */
	child->subordinate = max;
	pci_write_config_byte(dev, PCI_SUBORDINATE_BUS, max);
	pci_write_config_word(dev, PCI_COMMAND, cr);
	sprintf(child->name, (is_cardbus ? "PCI CardBus #%02x" : "PCI Bus #%02x"), child->number);
	return max;
}

static unsigned int cb_scan_new_bus(struct pci_bus *bus, int irq)
{
	unsigned int devfn, max;
	struct list_head *ln, *ln_tmp;
	struct pci_dev *dev, dev0;
	struct list_head found;
	
	INIT_LIST_HEAD(&found);
	
	printk("Scanning bus %02x\n", bus->number);
	max = bus->secondary;

	/* Create a device template */
	memset(&dev0, 0, sizeof(dev0));
	dev0.bus = bus;
	dev0.sysdata = bus->sysdata;

	/* Go find them, Rover! */
	for (devfn = 0; devfn < 0x100; devfn += 8) {
		dev0.devfn = devfn;
		cb_scan_slot(&dev0, &found);
	}

	/*
	 * After performing arch-dependent fixup of the bus, look behind
	 * all PCI-to-PCI bridges on this bus.
	 */
	printk("Fixups for bus %02x\n", bus->number);
	pci_read_bridge_bases(bus);

	list_for_each_safe(ln, ln_tmp, &found)
	{
		int i;
		u8 irq_pin;
		dev = pci_dev_g(ln);
		pci_set_power_state(dev, 0);
		for(i=0;i<6;i++)
		{
			struct resource *r = dev->resource + i;
			if(!r->start && r->end)
				pci_assign_resource(dev, i);
		}
		/* Does this function have an interrupt at all? */
		pci_readb(dev, PCI_INTERRUPT_PIN, &irq_pin);
		if (irq_pin) {
			dev->irq = irq;
			pci_writeb(dev, PCI_INTERRUPT_LINE, irq);
		}

		pci_enable_device(dev); /* XXX check return */
		pci_insert_device(dev, bus);
		if (dev->hdr_type == PCI_HEADER_TYPE_BRIDGE || dev->hdr_type == PCI_HEADER_TYPE_CARDBUS)
			max = cb_scan_bridge(bus, dev, max, irq);
	}

	/*
	 * We've scanned the bus and so we know all about what's on
	 * the other side of any bridges that may be on this bus plus
	 * any devices.
	 *
	 * Return how far we've got finding sub-buses.
	 */
	printk("Bus scan for %02x returning with max=%02x\n", bus->number, max);
	return max;
}

static void program_bridge(struct pci_dev *bridge)
{
	u32 l;
	
	/* Set up the top and bottom of the PCI I/O segment for this bus. */
	pci_read_config_dword(bridge, PCI_IO_BASE, &l);
	l &= 0xffff0000;
	l |= (bridge->resource[7].start >> 8) & 0x00f0;
	l |= bridge->resource[7].end & 0xf000;
	pci_write_config_dword(bridge, PCI_IO_BASE, l);

	/* Clear upper 16 bits of I/O base/limit. */
	pci_write_config_dword(bridge, PCI_IO_BASE_UPPER16, 0);

	/* Clear out the upper 32 bits of PREF base/limit. */
	pci_write_config_dword(bridge, PCI_PREF_BASE_UPPER32, 0);
	pci_write_config_dword(bridge, PCI_PREF_LIMIT_UPPER32, 0);

	/* Set up the top and bottom of the PCI Memory segment
	   for this bus. */
	l = (bridge->resource[8].start >> 16) & 0xfff0;
	l |= bridge->resource[8].end & 0xfff00000;
	pci_write_config_dword(bridge, PCI_MEMORY_BASE, l);

	/* Set up PREF base/limit. */
	l = (bridge->resource[9].start >> 16) & 0xfff0;
	l |= bridge->resource[9].end & 0xfff00000;
	pci_write_config_dword(bridge, PCI_PREF_MEMORY_BASE, l);

	/* FIXME - 0x0c if our ISA VGA is behind it. It looks like X
	   can handle this itself - CHECK */
	l = 0x04;
	pci_write_config_word(bridge, PCI_BRIDGE_CONTROL, l);
}


/*=====================================================================

    cb_alloc() and cb_free() allocate and free the kernel data
    structures for a Cardbus device, and handle the lowest level PCI
    device setup issues.
    
=====================================================================*/

int cb_alloc(socket_info_t * s)
{
	struct pci_bus *bus;
	struct pci_dev tmp;
	u_short vend, v, dev;
	u_char i, hdr, fn;
	cb_config_t *c;
	int irq;

	bus = s->cap.cb_dev->subordinate;
	memset(&tmp, 0, sizeof(tmp));
	tmp.bus = bus;
	tmp.sysdata = bus->sysdata;
	tmp.devfn = 0;

	pci_readw(&tmp, PCI_VENDOR_ID, &vend);
	pci_readw(&tmp, PCI_DEVICE_ID, &dev);
	printk(KERN_INFO "cs: cb_alloc(bus %d): vendor 0x%04x, "
	       "device 0x%04x\n", bus->number, vend, dev);

	pci_readb(&tmp, PCI_HEADER_TYPE, &hdr);
	fn = 1;
	if (hdr & 0x80) {
		do {
			tmp.devfn = fn;
			if (pci_readw(&tmp, PCI_VENDOR_ID, &v) || !v || v == 0xffff)
				break;
			fn++;
		} while (fn < 8);
	}
	s->functions = fn;

	c = kmalloc(fn * sizeof(struct cb_config_t), GFP_ATOMIC);
	if (!c)
		return CS_OUT_OF_RESOURCE;
	memset(c, 0, fn * sizeof(struct cb_config_t));

	irq = s->cap.pci_irq;
	for (i = 0; i < fn; i++) {
		struct pci_dev *dev = &c[i].dev;
		u8 irq_pin;
		int r;

		dev->bus = bus;
		dev->sysdata = bus->sysdata;
		dev->devfn = i;
		dev->vendor = vend;
		pci_readw(dev, PCI_DEVICE_ID, &dev->device);
		dev->hdr_type = hdr & 0x7f;

		pci_setup_device(dev);
		
		if(dev->hdr_type == 1)
		{
			int max = bus->secondary;
			int idx;
			struct resource *res, *pres;
			printk(KERN_INFO "Cardbus: Bridge found - we suck.\n");
			pci_read_bridge_bases(bus);
			for (idx = PCI_BRIDGE_RESOURCES; idx < PCI_BRIDGE_RESOURCES + 3; idx++) {
				res = &dev->resource[idx];
				/* This is ugly - pci_read_bridge_bases should have
				   done it for us. Need to find out why it doesnt
				   before fixing it */
				if(idx == PCI_BRIDGE_RESOURCES) 
					res->flags = IORESOURCE_IO | PCI_IO_RANGE_TYPE_32;
				if(idx == PCI_BRIDGE_RESOURCES+1)
					res->flags |= IORESOURCE_MEM;
				if(idx == PCI_BRIDGE_RESOURCES+2)
					res->flags |= IORESOURCE_MEM|IORESOURCE_PREFETCH;
				/* Ignore any existing values in the chip */
				res->start = 0;
				/* Find the parent resource */
				pres = pci_find_parent_resource(dev, res);
				if(!pres)
				{
					printk(KERN_ERR "No parent resource for %lx\n", res->flags);
					continue;
				}
				printk(KERN_ERR "Allocating for type %lx, in bus resource.\n", res->flags);
				/* Hog the entire space */
				res->start = pres->start;
				if(idx != PCI_BRIDGE_RESOURCES)
					res->end = pres->end;
				else	/* Still working this out - FIXME */
					res->end = res->start + 255;
				if (!pres || request_resource(pres, res) < 0)
					printk(KERN_ERR "PCI: Cannot allocate resource region %d of bridge %s\n", idx, dev->slot_name);
			}
			program_bridge(dev);
			max = cb_scan_bridge(bus, dev, max, irq);
			printk(KERN_INFO "Cardbus: Bridge scanned.\n");
		}
		else
		{			
			/* FIXME: Do we need to enable the expansion ROM? */
			for (r = 0; r < 7; r++) {
				struct resource *res = dev->resource + r;
				if (res->flags)
					pci_assign_resource(dev, r);
			}
		}
		/* Does this function have an interrupt at all? */
		pci_readb(dev, PCI_INTERRUPT_PIN, &irq_pin);
		if (irq_pin) {
			dev->irq = irq;
			pci_writeb(dev, PCI_INTERRUPT_LINE, irq);
		}

		pci_enable_device(dev); /* XXX check return */
		pci_insert_device(dev, bus);
	}

	s->cb_config = c;
	s->irq.AssignedIRQ = irq;
	return CS_SUCCESS;
}

void cb_free(socket_info_t * s)
{
	cb_config_t *c = s->cb_config;

	if (c) {
		int i;

		s->cb_config = NULL;
		for (i = 0 ; i < s->functions ; i++)
			pci_remove_device(&c[i].dev);

		kfree(c);
		printk(KERN_INFO "cs: cb_free(bus %d)\n", s->cap.cb_dev->subordinate->number);
	}
}

/*=====================================================================

    cb_config() has the job of allocating all system resources that
    a Cardbus card requires.  Rather than using the CIS (which seems
    to not always be present), it treats the card as an ordinary PCI
    device, and probes the base address registers to determine each
    function's IO and memory space needs.

    It is called from the RequestIO card service.
    
======================================================================*/

int cb_config(socket_info_t * s)
{
	return CS_SUCCESS;
}

/*======================================================================

    cb_release() releases all the system resources (IO and memory
    space, and interrupt) committed for a Cardbus card by a prior call
    to cb_config().

    It is called from the ReleaseIO() service.
    
======================================================================*/

void cb_release(socket_info_t * s)
{
	DEBUG(0, "cs: cb_release(bus %d)\n", s->cap.cb_dev->subordinate->number);
}

/*=====================================================================

    cb_enable() has the job of configuring a socket for a Cardbus
    card, and initializing the card's PCI configuration registers.

    It first sets up the Cardbus bridge windows, for IO and memory
    accesses.  Then, it initializes each card function's base address
    registers, interrupt line register, and command register.

    It is called as part of the RequestConfiguration card service.
    It should be called after a previous call to cb_config() (via the
    RequestIO service).
    
======================================================================*/

void cb_enable(socket_info_t * s)
{
	struct pci_dev *dev;
	u_char i;

	DEBUG(0, "cs: cb_enable(bus %d)\n", s->cap.cb_dev->subordinate->number);

	/* Configure bridge */
	cb_release_cis_mem(s);

	/* Set up PCI interrupt and command registers */
	for (i = 0; i < s->functions; i++) {
		dev = &s->cb_config[i].dev;
		pci_writeb(dev, PCI_COMMAND, PCI_COMMAND_MASTER |
			   PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
		pci_writeb(dev, PCI_CACHE_LINE_SIZE, L1_CACHE_BYTES / 4);
	}

	if (s->irq.AssignedIRQ) {
		for (i = 0; i < s->functions; i++) {
			dev = &s->cb_config[i].dev;
			pci_writeb(dev, PCI_INTERRUPT_LINE, s->irq.AssignedIRQ);
		}
		s->socket.io_irq = s->irq.AssignedIRQ;
		s->ss_entry->set_socket(s->sock, &s->socket);
	}
}

/*======================================================================

    cb_disable() unconfigures a Cardbus card previously set up by
    cb_enable().

    It is called from the ReleaseConfiguration service.
    
======================================================================*/

void cb_disable(socket_info_t * s)
{
	DEBUG(0, "cs: cb_disable(bus %d)\n", s->cap.cb_dev->subordinate->number);

	/* Turn off bridge windows */
	cb_release_cis_mem(s);
}
