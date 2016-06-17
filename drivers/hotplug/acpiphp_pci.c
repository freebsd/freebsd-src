/*
 * ACPI PCI HotPlug PCI configuration space management
 *
 * Copyright (C) 1995,2001 Compaq Computer Corporation
 * Copyright (C) 2001 Greg Kroah-Hartman (greg@kroah.com)
 * Copyright (C) 2001,2002 IBM Corp.
 * Copyright (C) 2002 Takayoshi Kochi (t-kochi@bq.jp.nec.com)
 * Copyright (C) 2002 Hiroshi Aono (h-aono@ap.jp.nec.com)
 * Copyright (C) 2002 NEC Corporation
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Send feedback to <t-kochi@bq.jp.nec.com>
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include "pci_hotplug.h"
#include "acpiphp.h"

#define MY_NAME "acpiphp_pci"

static void acpiphp_configure_irq (struct pci_dev *dev);


/* allocate mem/pmem/io resource to a new function */
static int init_config_space (struct acpiphp_func *func)
{
	u32 bar, len;
	u32 address[] = {
		PCI_BASE_ADDRESS_0,
		PCI_BASE_ADDRESS_1,
		PCI_BASE_ADDRESS_2,
		PCI_BASE_ADDRESS_3,
		PCI_BASE_ADDRESS_4,
		PCI_BASE_ADDRESS_5,
		0
	};
	int count;
	struct acpiphp_bridge *bridge;
	struct pci_resource *res;
	struct pci_bus *bus;
	int devfn;

	bridge = func->slot->bridge;
	bus = bridge->pci_bus;
	devfn = PCI_DEVFN(func->slot->device, func->function);

	for (count = 0; address[count]; count++) {	/* for 6 BARs */
		pci_bus_write_config_dword(bus, devfn, address[count], 0xFFFFFFFF);
		pci_bus_read_config_dword(bus, devfn, address[count], &bar);

		if (!bar)	/* This BAR is not implemented */
			continue;

		dbg("Device %02x.%d BAR %d wants %x\n", PCI_SLOT(devfn),
				PCI_FUNC(devfn), count, bar);

		if (bar & PCI_BASE_ADDRESS_SPACE_IO) {
			/* This is IO */

			len = bar & 0xFFFFFFFC;
			len = ~len + 1;

			dbg("len in IO %x, BAR %d\n", len, count);

			spin_lock(&bridge->res_lock);
			res = acpiphp_get_io_resource(&bridge->io_head, len);
			spin_unlock(&bridge->res_lock);

			if (!res) {
				err("cannot allocate requested io for %02x:%02x.%d len %x\n",
				    bus->number, PCI_SLOT(devfn), PCI_FUNC(devfn), len);
				return -1;
			}
			pci_bus_write_config_dword(bus, devfn, address[count], (u32)res->base);
			res->next = func->io_head;
			func->io_head = res;

		} else {
			/* This is Memory */
			if (bar & PCI_BASE_ADDRESS_MEM_PREFETCH) {
				/* pfmem */

				len = bar & 0xFFFFFFF0;
				len = ~len + 1;

				dbg("len in PFMEM %x, BAR %d\n", len, count);

				spin_lock(&bridge->res_lock);
				res = acpiphp_get_resource(&bridge->p_mem_head, len);
				spin_unlock(&bridge->res_lock);

				if (!res) {
					err("cannot allocate requested pfmem for %02x:%02x.%d len %x\n",
					    bus->number, PCI_SLOT(devfn), PCI_FUNC(devfn), len);
					return -1;
				}

				pci_bus_write_config_dword(bus, devfn, address[count], (u32)res->base);

				if (bar & PCI_BASE_ADDRESS_MEM_TYPE_64) {	/* takes up another dword */
					dbg("inside the pfmem 64 case, count %d\n", count);
					count += 1;
					pci_bus_write_config_dword(bus, devfn, address[count], (u32)(res->base >> 32));
				}

				res->next = func->p_mem_head;
				func->p_mem_head = res;

			} else {
				/* regular memory */

				len = bar & 0xFFFFFFF0;
				len = ~len + 1;

				dbg("len in MEM %x, BAR %d\n", len, count);

				spin_lock(&bridge->res_lock);
				res = acpiphp_get_resource(&bridge->mem_head, len);
				spin_unlock(&bridge->res_lock);

				if (!res) {
					err("cannot allocate requested pfmem for %02x:%02x.%d len %x\n",
					    bus->number, PCI_SLOT(devfn), PCI_FUNC(devfn), len);
					return -1;
				}

				pci_bus_write_config_dword(bus, devfn, address[count], (u32)res->base);

				if (bar & PCI_BASE_ADDRESS_MEM_TYPE_64) {
					/* takes up another dword */
					dbg("inside mem 64 case, reg. mem, count %d\n", count);
					count += 1;
					pci_bus_write_config_dword(bus, devfn, address[count], (u32)(res->base >> 32));
				}

				res->next = func->mem_head;
				func->mem_head = res;

			}
		}
	}

	/* disable expansion rom */
	pci_bus_write_config_dword(bus, devfn, PCI_ROM_ADDRESS, 0x00000000);

	return 0;
}


/* enable pci_dev */
static int configure_pci_dev (struct pci_dev_wrapped *wrapped_dev, struct pci_bus_wrapped *wrapped_bus)
{
	u16 tmp;
	struct acpiphp_func *func;
	struct acpiphp_bridge *bridge;
	struct pci_dev *dev;

	func = (struct acpiphp_func *)wrapped_dev->data;
	bridge = (struct acpiphp_bridge *)wrapped_bus->data;
	dev = wrapped_dev->dev;

	/* TBD: support PCI-to-PCI bridge case */
	if (!func || !bridge)
		return 0;

	pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, bridge->hpp.cache_line_size);
	pci_write_config_byte(dev, PCI_LATENCY_TIMER, bridge->hpp.latency_timer);

	pci_read_config_word(dev, PCI_COMMAND, &tmp);
	if (bridge->hpp.enable_SERR)
		tmp |= PCI_COMMAND_SERR;
	if (bridge->hpp.enable_PERR)
		tmp |= PCI_COMMAND_PARITY;
	//tmp |= (PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
	pci_write_config_word(dev, PCI_COMMAND, tmp);

	acpiphp_configure_irq(dev);
#ifdef CONFIG_PROC_FS
	pci_proc_attach_device(dev);
#endif
	pci_announce_device_to_drivers(dev);
	info("Device %s configured\n", dev->slot_name);

	return 0;
}


static int is_pci_dev_in_use (struct pci_dev* dev)
{
	/*
	 * dev->driver will be set if the device is in use by a new-style
	 * driver -- otherwise, check the device's regions to see if any
	 * driver has claimed them
	 */

	int i, inuse=0;

	if (dev->driver) return 1; //assume driver feels responsible

	for (i = 0; !dev->driver && !inuse && (i < 6); i++) {
		if (!pci_resource_start(dev, i))
			continue;

		if (pci_resource_flags(dev, i) & IORESOURCE_IO)
			inuse = check_region(pci_resource_start(dev, i),
					     pci_resource_len(dev, i));
		else if (pci_resource_flags(dev, i) & IORESOURCE_MEM)
			inuse = check_mem_region(pci_resource_start(dev, i),
						 pci_resource_len(dev, i));
	}

	return inuse;
}


static int pci_hp_remove_device (struct pci_dev *dev)
{
	if (is_pci_dev_in_use(dev)) {
		err("***Cannot safely power down device -- "
		       "it appears to be in use***\n");
		return -EBUSY;
	}
	pci_remove_device(dev);
	return 0;
}


/* remove device driver */
static int unconfigure_pci_dev_driver (struct pci_dev_wrapped *wrapped_dev, struct pci_bus_wrapped *wrapped_bus)
{
	struct pci_dev *dev = wrapped_dev->dev;

	dbg("attempting removal of driver for device %s\n", dev->slot_name);

	/* Now, remove the Linux Driver Representation */
	if (dev->driver) {
		if (dev->driver->remove) {
			dev->driver->remove(dev);
			dbg("driver was properly removed\n");
		}
		dev->driver = NULL;
	}

	return is_pci_dev_in_use(dev);
}


/* remove pci_dev itself from system */
static int unconfigure_pci_dev (struct pci_dev_wrapped *wrapped_dev, struct pci_bus_wrapped *wrapped_bus)
{
	struct pci_dev *dev = wrapped_dev->dev;

	/* Now, remove the Linux Representation */
	if (dev) {
		if (pci_hp_remove_device(dev) == 0) {
			info("Device %s removed\n", dev->slot_name);
			kfree(dev); /* Now, remove */
		} else {
			return -1; /* problems while freeing, abort visitation */
		}
	}

	return 0;
}


/* remove pci_bus itself from system */
static int unconfigure_pci_bus (struct pci_bus_wrapped *wrapped_bus, struct pci_dev_wrapped *wrapped_dev)
{
	struct pci_bus *bus = wrapped_bus->bus;

#ifdef CONFIG_PROC_FS
	/* Now, remove the Linux Representation */
	if (bus->procdir) {
		pci_proc_detach_bus(bus);
	}
#endif
	/* the cleanup code should live in the kernel ... */
	bus->self->subordinate = NULL;
	/* unlink from parent bus */
	list_del(&bus->node);

	/* Now, remove */
	if (bus)
		kfree(bus);

	return 0;
}


/* detect_used_resource - subtract resource under dev from bridge */
static int detect_used_resource (struct acpiphp_bridge *bridge, struct pci_dev *dev)
{
	u32 bar, len;
	u64 base;
	u32 address[] = {
		PCI_BASE_ADDRESS_0,
		PCI_BASE_ADDRESS_1,
		PCI_BASE_ADDRESS_2,
		PCI_BASE_ADDRESS_3,
		PCI_BASE_ADDRESS_4,
		PCI_BASE_ADDRESS_5,
		0
	};
	int count;
	struct pci_resource *res;

	dbg("Device %s\n", dev->slot_name);

	for (count = 0; address[count]; count++) {	/* for 6 BARs */
		pci_read_config_dword(dev, address[count], &bar);

		if (!bar)	/* This BAR is not implemented */
			continue;

		pci_write_config_dword(dev, address[count], 0xFFFFFFFF);
		pci_read_config_dword(dev, address[count], &len);

		if (len & PCI_BASE_ADDRESS_SPACE_IO) {
			/* This is IO */
			base = bar & 0xFFFFFFFC;
			len &= 0xFFFFFFFC;
			len = ~len + 1;

			dbg("BAR[%d] %08x - %08x (IO)\n", count, (u32)base, (u32)base + len - 1);

			spin_lock(&bridge->res_lock);
			res = acpiphp_get_resource_with_base(&bridge->io_head, base, len);
			spin_unlock(&bridge->res_lock);
			if (res)
				kfree(res);
		} else {
			/* This is Memory */
			base = bar & 0xFFFFFFF0;
			if (len & PCI_BASE_ADDRESS_MEM_PREFETCH) {
				/* pfmem */

				len &= 0xFFFFFFF0;
				len = ~len + 1;

				if (len & PCI_BASE_ADDRESS_MEM_TYPE_64) {	/* takes up another dword */
					dbg("prefetch mem 64\n");
					count += 1;
				}
				dbg("BAR[%d] %08x - %08x (PMEM)\n", count, (u32)base, (u32)base + len - 1);
				spin_lock(&bridge->res_lock);
				res = acpiphp_get_resource_with_base(&bridge->p_mem_head, base, len);
				spin_unlock(&bridge->res_lock);
				if (res)
					kfree(res);
			} else {
				/* regular memory */

				len &= 0xFFFFFFF0;
				len = ~len + 1;

				if (len & PCI_BASE_ADDRESS_MEM_TYPE_64) {
					/* takes up another dword */
					dbg("mem 64\n");
					count += 1;
				}
				dbg("BAR[%d] %08x - %08x (MEM)\n", count, (u32)base, (u32)base + len - 1);
				spin_lock(&bridge->res_lock);
				res = acpiphp_get_resource_with_base(&bridge->mem_head, base, len);
				spin_unlock(&bridge->res_lock);
				if (res)
					kfree(res);
			}
		}

		pci_write_config_dword(dev, address[count], bar);
	}

	return 0;
}


/* detect_pci_resource_bus - subtract resource under pci_bus */
static void detect_used_resource_bus(struct acpiphp_bridge *bridge, struct pci_bus *bus)
{
	struct list_head *l;
	struct pci_dev *dev;

	list_for_each (l, &bus->devices) {
		dev = pci_dev_b(l);
		detect_used_resource(bridge, dev);
		/* XXX recursive call */
		if (dev->subordinate)
			detect_used_resource_bus(bridge, dev->subordinate);
	}
}


/**
 * acpiphp_detect_pci_resource - detect resources under bridge
 * @bridge: detect all resources already used under this bridge
 *
 * collect all resources already allocated for all devices under a bridge.
 */
int acpiphp_detect_pci_resource (struct acpiphp_bridge *bridge)
{
	detect_used_resource_bus(bridge, bridge->pci_bus);

	return 0;
}


/**
 * acpiphp_init_slot_resource - gather resource usage information of a slot
 * @slot: ACPI slot object to be checked, should have valid pci_dev member
 *
 * TBD: PCI-to-PCI bridge case
 *      use pci_dev->resource[]
 */
int acpiphp_init_func_resource (struct acpiphp_func *func)
{
	u64 base;
	u32 bar, len;
	u32 address[] = {
		PCI_BASE_ADDRESS_0,
		PCI_BASE_ADDRESS_1,
		PCI_BASE_ADDRESS_2,
		PCI_BASE_ADDRESS_3,
		PCI_BASE_ADDRESS_4,
		PCI_BASE_ADDRESS_5,
		0
	};
	int count;
	struct pci_resource *res;
	struct pci_dev *dev;

	dev = func->pci_dev;
	dbg("Hot-pluggable device %s\n", dev->slot_name);

	for (count = 0; address[count]; count++) {	/* for 6 BARs */
		pci_read_config_dword(dev, address[count], &bar);

		if (!bar)	/* This BAR is not implemented */
			continue;

		pci_write_config_dword(dev, address[count], 0xFFFFFFFF);
		pci_read_config_dword(dev, address[count], &len);

		if (len & PCI_BASE_ADDRESS_SPACE_IO) {
			/* This is IO */
			base = bar & 0xFFFFFFFC;
			len &= 0xFFFFFFFC;
			len = ~len + 1;

			dbg("BAR[%d] %08x - %08x (IO)\n", count, (u32)base, (u32)base + len - 1);

			res = acpiphp_make_resource(base, len);
			if (!res)
				goto no_memory;

			res->next = func->io_head;
			func->io_head = res;

		} else {
			/* This is Memory */
			base = bar & 0xFFFFFFF0;
			if (len & PCI_BASE_ADDRESS_MEM_PREFETCH) {
				/* pfmem */

				len &= 0xFFFFFFF0;
				len = ~len + 1;

				if (len & PCI_BASE_ADDRESS_MEM_TYPE_64) {	/* takes up another dword */
					dbg("prefetch mem 64\n");
					count += 1;
				}
				dbg("BAR[%d] %08x - %08x (PMEM)\n", count, (u32)base, (u32)base + len - 1);
				res = acpiphp_make_resource(base, len);
				if (!res)
					goto no_memory;

				res->next = func->p_mem_head;
				func->p_mem_head = res;

			} else {
				/* regular memory */

				len &= 0xFFFFFFF0;
				len = ~len + 1;

				if (len & PCI_BASE_ADDRESS_MEM_TYPE_64) {
					/* takes up another dword */
					dbg("mem 64\n");
					count += 1;
				}
				dbg("BAR[%d] %08x - %08x (MEM)\n", count, (u32)base, (u32)base + len - 1);
				res = acpiphp_make_resource(base, len);
				if (!res)
					goto no_memory;

				res->next = func->mem_head;
				func->mem_head = res;

			}
		}

		pci_write_config_dword(dev, address[count], bar);
	}
#if 1
	acpiphp_dump_func_resource(func);
#endif

	return 0;

 no_memory:
	err("out of memory\n");
	acpiphp_free_resource(&func->io_head);
	acpiphp_free_resource(&func->mem_head);
	acpiphp_free_resource(&func->p_mem_head);

	return -1;
}


/**
 * acpiphp_configure_slot - allocate PCI resources
 * @slot: slot to be configured
 *
 * initializes a PCI functions on a device inserted
 * into the slot
 *
 */
int acpiphp_configure_slot (struct acpiphp_slot *slot)
{
	struct acpiphp_func *func;
	struct list_head *l;
	u8 hdr;
	u32 dvid;
	int retval = 0;
	int is_multi = 0;

	pci_bus_read_config_byte(slot->bridge->pci_bus,
					PCI_DEVFN(slot->device, 0),
					PCI_HEADER_TYPE, &hdr);

	if (hdr & 0x80)
		is_multi = 1;

	list_for_each (l, &slot->funcs) {
		func = list_entry(l, struct acpiphp_func, sibling);
		if (is_multi || func->function == 0) {
			pci_bus_read_config_dword(slot->bridge->pci_bus,
						    PCI_DEVFN(slot->device,
								func->function),
						    PCI_VENDOR_ID, &dvid);
			if (dvid != 0xffffffff) {
				retval = init_config_space(func);
				if (retval)
					break;
			}
		}
	}

	return retval;
}


/* for pci_visit_dev() */
static struct pci_visit configure_functions = {
	.post_visit_pci_dev =	configure_pci_dev
};

static struct pci_visit unconfigure_functions_phase1 = {
	.post_visit_pci_dev =	unconfigure_pci_dev_driver
};

static struct pci_visit unconfigure_functions_phase2 = {
	.post_visit_pci_bus =	unconfigure_pci_bus,
	.post_visit_pci_dev =	unconfigure_pci_dev
};


/**
 * acpiphp_configure_function - configure PCI function
 * @func: function to be configured
 *
 * initializes a PCI functions on a device inserted
 * into the slot
 *
 */
int acpiphp_configure_function (struct acpiphp_func *func)
{
	int retval = 0;
	struct pci_dev_wrapped wrapped_dev;
	struct pci_bus_wrapped wrapped_bus;
	struct acpiphp_bridge *bridge;

	/* if pci_dev is NULL, ignore it */
	if (!func->pci_dev)
		goto err_exit;

	bridge = func->slot->bridge;

	memset(&wrapped_dev, 0, sizeof(struct pci_dev_wrapped));
	memset(&wrapped_bus, 0, sizeof(struct pci_bus_wrapped));
	wrapped_dev.dev = func->pci_dev;
	wrapped_dev.data = func;
	wrapped_bus.bus = bridge->pci_bus;
	wrapped_bus.data = bridge;

	retval = pci_visit_dev(&configure_functions, &wrapped_dev, &wrapped_bus);
	if (retval)
		goto err_exit;

 err_exit:
	return retval;
}


/**
 * acpiphp_unconfigure_function - unconfigure PCI function
 * @func: function to be unconfigured
 *
 */
int acpiphp_unconfigure_function (struct acpiphp_func *func)
{
	struct acpiphp_bridge *bridge;
	struct pci_dev_wrapped wrapped_dev;
	struct pci_bus_wrapped wrapped_bus;
	int retval = 0;

	/* if pci_dev is NULL, ignore it */
	if (!func->pci_dev)
		goto err_exit;

	memset(&wrapped_dev, 0, sizeof(struct pci_dev_wrapped));
	memset(&wrapped_bus, 0, sizeof(struct pci_bus_wrapped));
	wrapped_dev.dev = func->pci_dev;
	//wrapped_dev.data = func;
	wrapped_bus.bus = func->slot->bridge->pci_bus;
	//wrapped_bus.data = func->slot->bridge;

	retval = pci_visit_dev(&unconfigure_functions_phase1, &wrapped_dev, &wrapped_bus);
	if (retval)
		goto err_exit;

	retval = pci_visit_dev(&unconfigure_functions_phase2, &wrapped_dev, &wrapped_bus);
	if (retval)
		goto err_exit;

	/* free all resources */
	bridge = func->slot->bridge;

	spin_lock(&bridge->res_lock);
	acpiphp_move_resource(&func->io_head, &bridge->io_head);
	acpiphp_move_resource(&func->mem_head, &bridge->mem_head);
	acpiphp_move_resource(&func->p_mem_head, &bridge->p_mem_head);
	acpiphp_move_resource(&func->bus_head, &bridge->bus_head);
	spin_unlock(&bridge->res_lock);

 err_exit:
	return retval;
}


/*
 * acpiphp_configure_irq - configure PCI_INTERRUPT_PIN
 *
 * for x86 platforms, pcibios_enable_device calls pcibios_enable_irq,
 * which allocates irq for pci_dev
 *
 * for IA64 platforms, we have to program dev->irq from pci IRQ routing
 * information derived from ACPI table
 *
 * TBD:
 * separate architecture dependent part
 * (preferably, pci_enable_device() cares for allocating irq...)
 */
static void acpiphp_configure_irq (struct pci_dev *dev)
{
#if CONFIG_IA64		    /* XXX IA64 specific */
	extern void iosapic_fixup_pci_interrupt (struct pci_dev *dev);

	iosapic_fixup_pci_interrupt(dev);
	pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
#endif
}
