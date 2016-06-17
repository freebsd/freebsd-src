/*
 * Compaq Hot Plug Controller Driver
 *
 * Copyright (C) 1995,2001 Compaq Computer Corporation
 * Copyright (C) 2001 Greg Kroah-Hartman (greg@kroah.com)
 * Copyright (C) 2001 IBM Corp.
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
 * Send feedback to <greg@kroah.com>
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/pci.h>
#include "cpqphp.h"
#include "cpqphp_nvram.h"
#include "../../arch/i386/kernel/pci-i386.h"	/* horrible hack showing how processor dependant we are... */


u8 cpqhp_nic_irq;
u8 cpqhp_disk_irq;

static u16 unused_IRQ;


static int is_pci_dev_in_use(struct pci_dev* dev) 
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


static int pci_hp_remove_device(struct pci_dev *dev)
{
	if (is_pci_dev_in_use(dev)) {
		err("***Cannot safely power down device -- "
		       "it appears to be in use***\n");
		return -EBUSY;
	}
	pci_remove_device(dev);
	return 0;
}


/*
 * detect_HRT_floating_pointer
 *
 * find the Hot Plug Resource Table in the specified region of memory.
 *
 */
static void *detect_HRT_floating_pointer(void *begin, void *end)
{
	void *fp;
	void *endp;
	u8 temp1, temp2, temp3, temp4;
	int status = 0;

	endp = (end - sizeof(struct hrt) + 1);

	for (fp = begin; fp <= endp; fp += 16) {
		temp1 = readb(fp + SIG0);
		temp2 = readb(fp + SIG1);
		temp3 = readb(fp + SIG2);
		temp4 = readb(fp + SIG3);
		if (temp1 == '$' &&
		    temp2 == 'H' &&
		    temp3 == 'R' &&
		    temp4 == 'T') {
			status = 1;
			break;
		}
	}

	if (!status)
		fp = NULL;

	dbg("Discovered Hotplug Resource Table at %p\n", fp);
	return fp;
}

static int configure_visit_pci_dev (struct pci_dev_wrapped *wrapped_dev, struct pci_bus_wrapped *wrapped_bus) 
{
	struct pci_bus* bus = wrapped_bus->bus;
	struct pci_dev* dev = wrapped_dev->dev;
	struct pci_func *temp_func;
	int i=0;

	//We need to fix up the hotplug function representation with the linux representation
	do {
		temp_func = cpqhp_slot_find(dev->bus->number, dev->devfn >> 3, i++);
	} while (temp_func && (temp_func->function != (dev->devfn & 0x07)));

	if (temp_func) {
		temp_func->pci_dev = dev;
	} else {
		//We did not even find a hotplug rep of the function, create it
		//This code might be taken out if we can guarantee the creation of functions
		//in parallel (hotplug and Linux at the same time).
		dbg("@@@@@@@@@@@ cpqhp_slot_create in %s\n", __FUNCTION__);
		temp_func = cpqhp_slot_create(bus->number);
		if (temp_func == NULL)
			return -ENOMEM;
		temp_func->pci_dev = dev;
	}

	//Create /proc/bus/pci proc entry for this device and bus device is on
	//Notify the drivers of the change
	if (temp_func->pci_dev) {
		pci_proc_attach_device(temp_func->pci_dev);
		pci_announce_device_to_drivers(temp_func->pci_dev);
	}

	return 0;
}


static int unconfigure_visit_pci_dev_phase2 (struct pci_dev_wrapped *wrapped_dev, struct pci_bus_wrapped *wrapped_bus) 
{
	struct pci_dev* dev = wrapped_dev->dev;

	struct pci_func *temp_func;
	int i=0;

	//We need to remove the hotplug function representation with the linux representation
	do {
		temp_func = cpqhp_slot_find(dev->bus->number, dev->devfn >> 3, i++);
		if (temp_func) {
			dbg("temp_func->function = %d\n", temp_func->function);
		}
	} while (temp_func && (temp_func->function != (dev->devfn & 0x07)));

	//Now, remove the Linux Representation
	if (dev) {
		if (pci_hp_remove_device(dev) == 0) {
			kfree(dev); //Now, remove
		} else {
			return -1; // problems while freeing, abort visitation
		}
	}

	if (temp_func) {
		temp_func->pci_dev = NULL;
	} else {
		dbg("No pci_func representation for bus, devfn = %d, %x\n", dev->bus->number, dev->devfn);
	}

	return 0;
}


static int unconfigure_visit_pci_bus_phase2 (struct pci_bus_wrapped *wrapped_bus, struct pci_dev_wrapped *wrapped_dev) 
{
	struct pci_bus* bus = wrapped_bus->bus;

	//The cleanup code for proc entries regarding buses should be in the kernel...
	if (bus->procdir)
		dbg("detach_pci_bus %s\n", bus->procdir->name);
	pci_proc_detach_bus(bus);
	// The cleanup code should live in the kernel...
	bus->self->subordinate = NULL;
	// unlink from parent bus
	list_del(&bus->node);

	// Now, remove
	if (bus)
		kfree(bus);

	return 0;
}


static int unconfigure_visit_pci_dev_phase1 (struct pci_dev_wrapped *wrapped_dev, struct pci_bus_wrapped *wrapped_bus) 
{
	struct pci_dev* dev = wrapped_dev->dev;

	dbg("attempting removal of driver for device (%x, %x, %x)\n", dev->bus->number, PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn));
	//Now, remove the Linux Driver Representation 
	if (dev->driver) {
		if (dev->driver->remove) {
			dev->driver->remove(dev);
			dbg("driver was properly removed\n");
		}
		dev->driver = NULL;
	}

	return is_pci_dev_in_use(dev);
}


static struct pci_visit configure_functions = {
	visit_pci_dev:		configure_visit_pci_dev,
};


static struct pci_visit unconfigure_functions_phase1 = {
	post_visit_pci_dev:	unconfigure_visit_pci_dev_phase1
};

static struct pci_visit unconfigure_functions_phase2 = {
	post_visit_pci_bus:	unconfigure_visit_pci_bus_phase2,               
	post_visit_pci_dev:	unconfigure_visit_pci_dev_phase2
};


int cpqhp_configure_device (struct controller* ctrl, struct pci_func* func)  
{
	unsigned char bus;
	struct pci_dev dev0;
	struct pci_bus *child;
	struct pci_dev* temp;
	int rc = 0;

	struct pci_dev_wrapped wrapped_dev;
	struct pci_bus_wrapped wrapped_bus;
	memset(&wrapped_dev, 0, sizeof(struct pci_dev_wrapped));
	memset(&wrapped_bus, 0, sizeof(struct pci_bus_wrapped));

	memset(&dev0, 0, sizeof(struct pci_dev));

	if (func->pci_dev == NULL)
		func->pci_dev = pci_find_slot(func->bus, (func->device << 3) | (func->function & 0x7));

	//Still NULL ? Well then scan for it !
	if (func->pci_dev == NULL) {
		dbg("INFO: pci_dev still null\n");
		dev0.bus = ctrl->pci_dev->bus;
		dev0.devfn = (func->device << 3) + (func->function & 0x7);
		dev0.sysdata = ctrl->pci_dev->sysdata;

		//this will generate pci_dev structures for all functions, but we will only call this case when lookup fails
		func->pci_dev = pci_scan_slot(&dev0);
		if (func->pci_dev == NULL) {
			dbg("ERROR: pci_dev still null\n");
			return 0;
		}
	}

	if (func->pci_dev->hdr_type == PCI_HEADER_TYPE_BRIDGE) {
		pci_read_config_byte(func->pci_dev, PCI_SECONDARY_BUS, &bus);
		child = (struct pci_bus*) pci_add_new_bus(func->pci_dev->bus, (func->pci_dev), bus);
		pci_do_scan_bus(child);

	}

	temp = func->pci_dev;

	if (temp) {
		wrapped_dev.dev = temp;
		wrapped_bus.bus = temp->bus;
		rc = pci_visit_dev(&configure_functions, &wrapped_dev, &wrapped_bus);
	}
	return rc;
}


int cpqhp_unconfigure_device(struct pci_func* func) 
{
	int rc = 0;
	int j;
	struct pci_dev_wrapped wrapped_dev;
	struct pci_bus_wrapped wrapped_bus;
	
	memset(&wrapped_dev, 0, sizeof(struct pci_dev_wrapped));
	memset(&wrapped_bus, 0, sizeof(struct pci_bus_wrapped));

	dbg("%s: bus/dev/func = %x/%x/%x\n", __FUNCTION__, func->bus, func->device, func->function);

	for (j=0; j<8 ; j++) {
		struct pci_dev* temp = pci_find_slot(func->bus, (func->device << 3) | j);
		if (temp) {
			wrapped_dev.dev = temp;
			wrapped_bus.bus = temp->bus;
			rc = pci_visit_dev(&unconfigure_functions_phase1, &wrapped_dev, &wrapped_bus);
			if (rc)
				break;

			rc = pci_visit_dev(&unconfigure_functions_phase2, &wrapped_dev, &wrapped_bus);
			if (rc)
				break;
		}
	}
	return rc;
}

static int PCI_RefinedAccessConfig(struct pci_ops *ops, u8 bus, u8 device, u8 function, u8 offset, u32 *value)
{
	u32 vendID = 0;

	if (pci_read_config_dword_nodev (ops, bus, device, function, PCI_VENDOR_ID, &vendID) == -1)
		return -1;
	if (vendID == 0xffffffff)
		return -1;
	return pci_read_config_dword_nodev (ops, bus, device, function, offset, value);
}


/*
 * cpqhp_set_irq
 *
 * @bus_num: bus number of PCI device
 * @dev_num: device number of PCI device
 * @slot: pointer to u8 where slot number will be returned
 */
int cpqhp_set_irq (u8 bus_num, u8 dev_num, u8 int_pin, u8 irq_num)
{
	int rc;
	u16 temp_word;
	struct pci_dev fakedev;
	struct pci_bus fakebus;

	fakedev.devfn = dev_num << 3;
	fakedev.bus = &fakebus;
	fakebus.number = bus_num;
	dbg("%s: dev %d, bus %d, pin %d, num %d\n",
	    __FUNCTION__, dev_num, bus_num, int_pin, irq_num);
	rc = pcibios_set_irq_routing(&fakedev, int_pin - 0x0a, irq_num);
	dbg("%s:rc %d\n", __FUNCTION__, rc);
	if (!rc)
		return !rc;

	// set the Edge Level Control Register (ELCR)
	temp_word = inb(0x4d0);
	temp_word |= inb(0x4d1) << 8;

	temp_word |= 0x01 << irq_num;

	// This should only be for x86 as it sets the Edge Level Control Register
	outb((u8) (temp_word & 0xFF), 0x4d0);
	outb((u8) ((temp_word & 0xFF00) >> 8), 0x4d1);

	return 0;
}


/*
 * WTF??? This function isn't in the code, yet a function calls it, but the 
 * compiler optimizes it away?  strange.  Here as a placeholder to keep the 
 * compiler happy.
 */
static int PCI_ScanBusNonBridge (u8 bus, u8 device)
{
	return 0;
}

static int PCI_ScanBusForNonBridge(struct controller *ctrl, u8 bus_num, u8 * dev_num)
{
	u8 tdevice;
	u32 work;
	u8 tbus;

	for (tdevice = 0; tdevice < 0x100; tdevice++) {
		//Scan for access first
		if (PCI_RefinedAccessConfig(ctrl->pci_ops, bus_num, tdevice >> 3, tdevice & 0x7, 0x08, &work) == -1)
			continue;
		dbg("Looking for nonbridge bus_num %d dev_num %d\n", bus_num, tdevice);
		//Yep we got one. Not a bridge ?
		if ((work >> 8) != PCI_TO_PCI_BRIDGE_CLASS) {
			*dev_num = tdevice;
			dbg("found it !\n");
			return 0;
		}
	}
	for (tdevice = 0; tdevice < 0x100; tdevice++) {
		//Scan for access first
		if (PCI_RefinedAccessConfig(ctrl->pci_ops, bus_num, tdevice >> 3, tdevice & 0x7, 0x08, &work) == -1)
			continue;
		dbg("Looking for bridge bus_num %d dev_num %d\n", bus_num, tdevice);
		//Yep we got one. bridge ?
		if ((work >> 8) == PCI_TO_PCI_BRIDGE_CLASS) {
			pci_read_config_byte_nodev (ctrl->pci_ops, tbus, tdevice, 0, PCI_SECONDARY_BUS, &tbus);
			dbg("Recurse on bus_num %d tdevice %d\n", tbus, tdevice);
			if (PCI_ScanBusNonBridge(tbus, tdevice) == 0)
				return 0;
		}
	}

	return -1;
}


static int PCI_GetBusDevHelper(struct controller *ctrl, u8 *bus_num, u8 *dev_num, u8 slot, u8 nobridge)
{
	struct irq_routing_table *PCIIRQRoutingInfoLength;
	long len;
	long loop;
	u32 work;

	u8 tbus, tdevice, tslot;

	PCIIRQRoutingInfoLength = pcibios_get_irq_routing_table();

	len = (PCIIRQRoutingInfoLength->size -
	       sizeof(struct irq_routing_table)) / sizeof(struct irq_info);
	// Make sure I got at least one entry
	if (len == 0) {
		if (PCIIRQRoutingInfoLength != NULL)
			kfree(PCIIRQRoutingInfoLength );
		return -1;
	}

	for (loop = 0; loop < len; ++loop) {
		tbus = PCIIRQRoutingInfoLength->slots[loop].bus;
		tdevice = PCIIRQRoutingInfoLength->slots[loop].devfn;
		tslot = PCIIRQRoutingInfoLength->slots[loop].slot;

		if (tslot == slot) {
			*bus_num = tbus;
			*dev_num = tdevice;
			pci_read_config_dword_nodev (ctrl->pci_ops, *bus_num, *dev_num >> 3, *dev_num & 0x7, PCI_VENDOR_ID, &work);
			if (!nobridge || (work == 0xffffffff)) {
				if (PCIIRQRoutingInfoLength != NULL)
					kfree(PCIIRQRoutingInfoLength );
				return 0;
			}

			dbg("bus_num %d dev_num %d func_num %d\n", *bus_num, *dev_num >> 3, *dev_num & 0x7);
			pci_read_config_dword_nodev (ctrl->pci_ops, *bus_num, *dev_num >> 3, *dev_num & 0x7, PCI_CLASS_REVISION, &work);
			dbg("work >> 8 (%x) = BRIDGE (%x)\n", work >> 8, PCI_TO_PCI_BRIDGE_CLASS);

			if ((work >> 8) == PCI_TO_PCI_BRIDGE_CLASS) {
				pci_read_config_byte_nodev (ctrl->pci_ops, *bus_num, *dev_num >> 3, *dev_num & 0x7, PCI_SECONDARY_BUS, &tbus);
				dbg("Scan bus for Non Bridge: bus %d\n", tbus);
				if (PCI_ScanBusForNonBridge(ctrl, tbus, dev_num) == 0) {
					*bus_num = tbus;
					if (PCIIRQRoutingInfoLength != NULL)
						kfree(PCIIRQRoutingInfoLength );
					return 0;
				}
			} else {
				if (PCIIRQRoutingInfoLength != NULL)
					kfree(PCIIRQRoutingInfoLength );
				return 0;
			}

		}
	}
	if (PCIIRQRoutingInfoLength != NULL)
		kfree(PCIIRQRoutingInfoLength );
	return -1;
}


int cpqhp_get_bus_dev (struct controller *ctrl, u8 * bus_num, u8 * dev_num, u8 slot)
{
	return PCI_GetBusDevHelper(ctrl, bus_num, dev_num, slot, 0);	//plain (bridges allowed)
}


/* More PCI configuration routines; this time centered around hotplug controller */


/*
 * cpqhp_save_config
 *
 * Reads configuration for all slots in a PCI bus and saves info.
 *
 * Note:  For non-hot plug busses, the slot # saved is the device #
 *
 * returns 0 if success
 */
int cpqhp_save_config(struct controller *ctrl, int busnumber, int is_hot_plug)
{
	long rc;
	u8 class_code;
	u8 header_type;
	u32 ID;
	u8 secondary_bus;
	struct pci_func *new_slot;
	int sub_bus;
	int FirstSupported;
	int LastSupported;
	int max_functions;
	int function;
	u8 DevError;
	int device = 0;
	int cloop = 0;
	int stop_it;
	int index;

	//              Decide which slots are supported

	if (is_hot_plug) {
		//*********************************
		// is_hot_plug is the slot mask
		//*********************************
		FirstSupported = is_hot_plug >> 4;
		LastSupported = FirstSupported + (is_hot_plug & 0x0F) - 1;
	} else {
		FirstSupported = 0;
		LastSupported = 0x1F;
	}

	//     Save PCI configuration space for all devices in supported slots

	for (device = FirstSupported; device <= LastSupported; device++) {
		ID = 0xFFFFFFFF;
		rc = pci_read_config_dword_nodev (ctrl->pci_ops, busnumber, device, 0, PCI_VENDOR_ID, &ID);

		if (ID != 0xFFFFFFFF) {	  //  device in slot
			rc = pci_read_config_byte_nodev (ctrl->pci_ops, busnumber, device, 0, 0x0B, &class_code);
			if (rc)
				return rc;

			rc = pci_read_config_byte_nodev (ctrl->pci_ops, busnumber, device, 0, PCI_HEADER_TYPE, &header_type);
			if (rc)
				return rc;

			// If multi-function device, set max_functions to 8
			if (header_type & 0x80)
				max_functions = 8;
			else
				max_functions = 1;

			function = 0;

			do {
				DevError = 0;

				if ((header_type & 0x7F) == PCI_HEADER_TYPE_BRIDGE) {   // P-P Bridge
					//  Recurse the subordinate bus
					//  get the subordinate bus number
					rc = pci_read_config_byte_nodev (ctrl->pci_ops, busnumber, device, function, PCI_SECONDARY_BUS, &secondary_bus);
					if (rc) {
						return rc;
					} else {
						sub_bus = (int) secondary_bus;

						// Save secondary bus cfg spc
						// with this recursive call.
						rc = cpqhp_save_config(ctrl, sub_bus, 0);

						if (rc)
							return rc;
					}
				}

				index = 0;
				new_slot = cpqhp_slot_find(busnumber, device, index++);
				while (new_slot && 
				       (new_slot->function != (u8) function))
					new_slot = cpqhp_slot_find(busnumber, device, index++);

				if (!new_slot) {
					// Setup slot structure.
					new_slot = cpqhp_slot_create(busnumber);

					if (new_slot == NULL)
						return(1);
				}

				new_slot->bus = (u8) busnumber;
				new_slot->device = (u8) device;
				new_slot->function = (u8) function;
				new_slot->is_a_board = 1;
				new_slot->switch_save = 0x10;
				// In case of unsupported board
				new_slot->status = DevError;
				new_slot->pci_dev = pci_find_slot(new_slot->bus, (new_slot->device << 3) | new_slot->function);

				for (cloop = 0; cloop < 0x20; cloop++) {
					rc = pci_read_config_dword_nodev (ctrl->pci_ops, busnumber, device, function, cloop << 2, (u32 *) & (new_slot-> config_space [cloop]));
					if (rc)
						return rc;
				}

				function++;

				stop_it = 0;

				//  this loop skips to the next present function
				//  reading in Class Code and Header type.

				while ((function < max_functions)&&(!stop_it)) {
					rc = pci_read_config_dword_nodev (ctrl->pci_ops, busnumber, device, function, PCI_VENDOR_ID, &ID);
					if (ID == 0xFFFFFFFF) {	 // nothing there.
						function++;
					} else {  // Something there
						rc = pci_read_config_byte_nodev (ctrl->pci_ops, busnumber, device, function, 0x0B, &class_code);
						if (rc)
							return rc;

						rc = pci_read_config_byte_nodev (ctrl->pci_ops, busnumber, device, function, PCI_HEADER_TYPE, &header_type);
						if (rc)
							return rc;

						stop_it++;
					}
				}

			} while (function < max_functions);
		}		// End of IF (device in slot?)
		else if (is_hot_plug) {
			// Setup slot structure with entry for empty slot
			new_slot = cpqhp_slot_create(busnumber);

			if (new_slot == NULL) {
				return(1);
			}

			new_slot->bus = (u8) busnumber;
			new_slot->device = (u8) device;
			new_slot->function = 0;
			new_slot->is_a_board = 0;
			new_slot->presence_save = 0;
			new_slot->switch_save = 0;
		}
	}			// End of FOR loop

	return(0);
}


/*
 * cpqhp_save_slot_config
 *
 * Saves configuration info for all PCI devices in a given slot
 * including subordinate busses.
 *
 * returns 0 if success
 */
int cpqhp_save_slot_config (struct controller *ctrl, struct pci_func * new_slot)
{
	long rc;
	u8 class_code;
	u8 header_type;
	u32 ID;
	u8 secondary_bus;
	int sub_bus;
	int max_functions;
	int function;
	int cloop = 0;
	int stop_it;

	ID = 0xFFFFFFFF;

	pci_read_config_dword_nodev (ctrl->pci_ops, new_slot->bus, new_slot->device, 0, PCI_VENDOR_ID, &ID);

	if (ID != 0xFFFFFFFF) {	  //  device in slot
		pci_read_config_byte_nodev (ctrl->pci_ops, new_slot->bus, new_slot->device, 0, 0x0B, &class_code);

		pci_read_config_byte_nodev (ctrl->pci_ops, new_slot->bus, new_slot->device, 0, PCI_HEADER_TYPE, &header_type);

		if (header_type & 0x80)	// Multi-function device
			max_functions = 8;
		else
			max_functions = 1;

		function = 0;

		do {
			if ((header_type & 0x7F) == PCI_HEADER_TYPE_BRIDGE) {	  // PCI-PCI Bridge
				//  Recurse the subordinate bus
				pci_read_config_byte_nodev (ctrl->pci_ops, new_slot->bus, new_slot->device, function, PCI_SECONDARY_BUS, &secondary_bus);

				sub_bus = (int) secondary_bus;

				// Save the config headers for the secondary bus.
				rc = cpqhp_save_config(ctrl, sub_bus, 0);

				if (rc)
					return(rc);

			}	// End of IF

			new_slot->status = 0;

			for (cloop = 0; cloop < 0x20; cloop++) {
				pci_read_config_dword_nodev (ctrl->pci_ops, new_slot->bus, new_slot->device, function, cloop << 2, (u32 *) & (new_slot-> config_space [cloop]));
			}

			function++;

			stop_it = 0;

			//  this loop skips to the next present function
			//  reading in the Class Code and the Header type.

			while ((function < max_functions) && (!stop_it)) {
				pci_read_config_dword_nodev (ctrl->pci_ops, new_slot->bus, new_slot->device, function, PCI_VENDOR_ID, &ID);

				if (ID == 0xFFFFFFFF) {	 // nothing there.
					function++;
				} else {  // Something there
					pci_read_config_byte_nodev (ctrl->pci_ops, new_slot->bus, new_slot->device, function, 0x0B, &class_code);

					pci_read_config_byte_nodev (ctrl->pci_ops, new_slot->bus, new_slot->device, function, PCI_HEADER_TYPE, &header_type);

					stop_it++;
				}
			}

		} while (function < max_functions);
	}			// End of IF (device in slot?)
	else {
		return(2);
	}

	return(0);
}


/*
 * cpqhp_save_base_addr_length
 *
 * Saves the length of all base address registers for the
 * specified slot.  this is for hot plug REPLACE
 *
 * returns 0 if success
 */
int cpqhp_save_base_addr_length(struct controller *ctrl, struct pci_func * func)
{
	u8 cloop;
	u8 header_type;
	u8 secondary_bus;
	u8 type;
	int sub_bus;
	u32 temp_register;
	u32 base;
	u32 rc;
	struct pci_func *next;
	int index = 0;

	func = cpqhp_slot_find(func->bus, func->device, index++);

	while (func != NULL) {

		// Check for Bridge
		pci_read_config_byte_nodev (ctrl->pci_ops, func->bus, func->device, func->function, PCI_HEADER_TYPE, &header_type);

		if ((header_type & 0x7F) == PCI_HEADER_TYPE_BRIDGE) {
			// PCI-PCI Bridge
			pci_read_config_byte_nodev (ctrl->pci_ops, func->bus, func->device, func->function, PCI_SECONDARY_BUS, &secondary_bus);

			sub_bus = (int) secondary_bus;

			next = cpqhp_slot_list[sub_bus];

			while (next != NULL) {
				rc = cpqhp_save_base_addr_length(ctrl, next);

				if (rc)
					return(rc);

				next = next->next;
			}

			//FIXME: this loop is duplicated in the non-bridge case.  The two could be rolled together
			// Figure out IO and memory base lengths
			for (cloop = 0x10; cloop <= 0x14; cloop += 4) {
				temp_register = 0xFFFFFFFF;
				pci_write_config_dword_nodev(ctrl->pci_ops, func->bus, func->device, func->function, cloop, temp_register);
				pci_read_config_dword_nodev (ctrl->pci_ops, func->bus, func->device, func->function, cloop, &base);

				if (base) {  // If this register is implemented
					if (base & 0x01L) {
						// IO base
						// set base = amount of IO space requested
						base = base & 0xFFFFFFFE;
						base = (~base) + 1;

						type = 1;
					} else {
						// memory base
						base = base & 0xFFFFFFF0;
						base = (~base) + 1;

						type = 0;
					}
				} else {
					base = 0x0L;
					type = 0;
				}

				// Save information in slot structure
				func->base_length[(cloop - 0x10) >> 2] =
				base;
				func->base_type[(cloop - 0x10) >> 2] = type;

			}	// End of base register loop


		} else if ((header_type & 0x7F) == 0x00) {	  // PCI-PCI Bridge
			// Figure out IO and memory base lengths
			for (cloop = 0x10; cloop <= 0x24; cloop += 4) {
				temp_register = 0xFFFFFFFF;
				pci_write_config_dword_nodev(ctrl->pci_ops, func->bus, func->device, func->function, cloop, temp_register);
				pci_read_config_dword_nodev (ctrl->pci_ops, func->bus, func->device, func->function, cloop, &base);

				if (base) {  // If this register is implemented
					if (base & 0x01L) {
						// IO base
						// base = amount of IO space requested
						base = base & 0xFFFFFFFE;
						base = (~base) + 1;

						type = 1;
					} else {
						// memory base
						// base = amount of memory space requested
						base = base & 0xFFFFFFF0;
						base = (~base) + 1;

						type = 0;
					}
				} else {
					base = 0x0L;
					type = 0;
				}

				// Save information in slot structure
				func->base_length[(cloop - 0x10) >> 2] = base;
				func->base_type[(cloop - 0x10) >> 2] = type;

			}	// End of base register loop

		} else {	  // Some other unknown header type
		}

		// find the next device in this slot
		func = cpqhp_slot_find(func->bus, func->device, index++);
	}

	return(0);
}


/*
 * cpqhp_save_used_resources
 *
 * Stores used resource information for existing boards.  this is
 * for boards that were in the system when this driver was loaded.
 * this function is for hot plug ADD
 *
 * returns 0 if success
 */
int cpqhp_save_used_resources (struct controller *ctrl, struct pci_func * func)
{
	u8 cloop;
	u8 header_type;
	u8 secondary_bus;
	u8 temp_byte;
	u8 b_base;
	u8 b_length;
	u16 command;
	u16 save_command;
	u16 w_base;
	u16 w_length;
	u32 temp_register;
	u32 save_base;
	u32 base;
	int index = 0;
	struct pci_resource *mem_node;
	struct pci_resource *p_mem_node;
	struct pci_resource *io_node;
	struct pci_resource *bus_node;

	func = cpqhp_slot_find(func->bus, func->device, index++);

	while ((func != NULL) && func->is_a_board) {
		// Save the command register
		pci_read_config_word_nodev (ctrl->pci_ops, func->bus, func->device, func->function, PCI_COMMAND, &save_command);

		// disable card
		command = 0x00;
		pci_write_config_word_nodev(ctrl->pci_ops, func->bus, func->device, func->function, PCI_COMMAND, command);

		// Check for Bridge
		pci_read_config_byte_nodev (ctrl->pci_ops, func->bus, func->device, func->function, PCI_HEADER_TYPE, &header_type);

		if ((header_type & 0x7F) == PCI_HEADER_TYPE_BRIDGE) {	  // PCI-PCI Bridge
			// Clear Bridge Control Register
			command = 0x00;
			pci_write_config_word_nodev(ctrl->pci_ops, func->bus, func->device, func->function, PCI_BRIDGE_CONTROL, command);

			pci_read_config_byte_nodev (ctrl->pci_ops, func->bus, func->device, func->function, PCI_SECONDARY_BUS, &secondary_bus);

			pci_read_config_byte_nodev (ctrl->pci_ops, func->bus, func->device, func->function, PCI_SUBORDINATE_BUS, &temp_byte);

			bus_node =(struct pci_resource *) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);
			if (!bus_node)
				return -ENOMEM;

			bus_node->base = secondary_bus;
			bus_node->length = temp_byte - secondary_bus + 1;

			bus_node->next = func->bus_head;
			func->bus_head = bus_node;

			// Save IO base and Limit registers
			pci_read_config_byte_nodev (ctrl->pci_ops, func->bus, func->device, func->function, PCI_IO_BASE, &b_base);

			pci_read_config_byte_nodev (ctrl->pci_ops, func->bus, func->device, func->function, PCI_IO_LIMIT, &b_length);

			if ((b_base <= b_length) && (save_command & 0x01)) {
				io_node = (struct pci_resource *) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);
				if (!io_node)
					return -ENOMEM;

				io_node->base = (b_base & 0xF0) << 8;
				io_node->length = (b_length - b_base + 0x10) << 8;

				io_node->next = func->io_head;
				func->io_head = io_node;
			}
			// Save memory base and Limit registers
			pci_read_config_word_nodev (ctrl->pci_ops, func->bus, func->device, func->function, PCI_MEMORY_BASE, &w_base);

			pci_read_config_word_nodev (ctrl->pci_ops, func->bus, func->device, func->function, PCI_MEMORY_LIMIT, &w_length);

			if ((w_base <= w_length) && (save_command & 0x02)) {
				mem_node = (struct pci_resource *) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);
				if (!mem_node)
					return -ENOMEM;

				mem_node->base = w_base << 16;
				mem_node->length = (w_length - w_base + 0x10) << 16;

				mem_node->next = func->mem_head;
				func->mem_head = mem_node;
			}
			// Save prefetchable memory base and Limit registers
			pci_read_config_word_nodev (ctrl->pci_ops, func->bus, func->device, func->function, PCI_PREF_MEMORY_BASE, &w_base);

			pci_read_config_word_nodev (ctrl->pci_ops, func->bus, func->device, func->function, PCI_PREF_MEMORY_LIMIT, &w_length);

			if ((w_base <= w_length) && (save_command & 0x02)) {
				p_mem_node = (struct pci_resource *) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);
				if (!p_mem_node)
					return -ENOMEM;

				p_mem_node->base = w_base << 16;
				p_mem_node->length = (w_length - w_base + 0x10) << 16;

				p_mem_node->next = func->p_mem_head;
				func->p_mem_head = p_mem_node;
			}
			// Figure out IO and memory base lengths
			for (cloop = 0x10; cloop <= 0x14; cloop += 4) {
				pci_read_config_dword_nodev (ctrl->pci_ops, func->bus, func->device, func->function, cloop, &save_base);

				temp_register = 0xFFFFFFFF;
				pci_write_config_dword_nodev(ctrl->pci_ops, func->bus, func->device, func->function, cloop, temp_register);

				pci_read_config_dword_nodev (ctrl->pci_ops, func->bus, func->device, func->function, cloop, &base);

				temp_register = base;

				if (base) {  // If this register is implemented
					if (((base & 0x03L) == 0x01)
					    && (save_command & 0x01)) {
						// IO base
						// set temp_register = amount of IO space requested
						temp_register = base & 0xFFFFFFFE;
						temp_register = (~temp_register) + 1;

						io_node = (struct pci_resource *) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);
						if (!io_node)
							return -ENOMEM;

						io_node->base =
						save_base & (~0x03L);
						io_node->length = temp_register;

						io_node->next = func->io_head;
						func->io_head = io_node;
					} else
						if (((base & 0x0BL) == 0x08)
						    && (save_command & 0x02)) {
						// prefetchable memory base
						temp_register = base & 0xFFFFFFF0;
						temp_register = (~temp_register) + 1;

						p_mem_node = (struct pci_resource *) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);
						if (!p_mem_node)
							return -ENOMEM;

						p_mem_node->base = save_base & (~0x0FL);
						p_mem_node->length = temp_register;

						p_mem_node->next = func->p_mem_head;
						func->p_mem_head = p_mem_node;
					} else
						if (((base & 0x0BL) == 0x00)
						    && (save_command & 0x02)) {
						// prefetchable memory base
						temp_register = base & 0xFFFFFFF0;
						temp_register = (~temp_register) + 1;

						mem_node = (struct pci_resource *) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);
						if (!mem_node)
							return -ENOMEM;

						mem_node->base = save_base & (~0x0FL);
						mem_node->length = temp_register;

						mem_node->next = func->mem_head;
						func->mem_head = mem_node;
					} else
						return(1);
				}
			}	// End of base register loop
		} else if ((header_type & 0x7F) == 0x00) {	  // Standard header
			// Figure out IO and memory base lengths
			for (cloop = 0x10; cloop <= 0x24; cloop += 4) {
				pci_read_config_dword_nodev (ctrl->pci_ops, func->bus, func->device, func->function, cloop, &save_base);

				temp_register = 0xFFFFFFFF;
				pci_write_config_dword_nodev(ctrl->pci_ops, func->bus, func->device, func->function, cloop, temp_register);

				pci_read_config_dword_nodev (ctrl->pci_ops, func->bus, func->device, func->function, cloop, &base);

				temp_register = base;

				if (base) {	  // If this register is implemented
					if (((base & 0x03L) == 0x01)
					    && (save_command & 0x01)) {
						// IO base
						// set temp_register = amount of IO space requested
						temp_register = base & 0xFFFFFFFE;
						temp_register = (~temp_register) + 1;

						io_node = (struct pci_resource *) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);
						if (!io_node)
							return -ENOMEM;

						io_node->base = save_base & (~0x01L);
						io_node->length = temp_register;

						io_node->next = func->io_head;
						func->io_head = io_node;
					} else
						if (((base & 0x0BL) == 0x08)
						    && (save_command & 0x02)) {
						// prefetchable memory base
						temp_register = base & 0xFFFFFFF0;
						temp_register = (~temp_register) + 1;

						p_mem_node = (struct pci_resource *) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);
						if (!p_mem_node)
							return -ENOMEM;

						p_mem_node->base = save_base & (~0x0FL);
						p_mem_node->length = temp_register;

						p_mem_node->next = func->p_mem_head;
						func->p_mem_head = p_mem_node;
					} else
						if (((base & 0x0BL) == 0x00)
						    && (save_command & 0x02)) {
						// prefetchable memory base
						temp_register = base & 0xFFFFFFF0;
						temp_register = (~temp_register) + 1;

						mem_node = (struct pci_resource *) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);
						if (!mem_node)
							return -ENOMEM;

						mem_node->base = save_base & (~0x0FL);
						mem_node->length = temp_register;

						mem_node->next = func->mem_head;
						func->mem_head = mem_node;
					} else
						return(1);
				}
			}	// End of base register loop
		} else {	  // Some other unknown header type
		}

		// find the next device in this slot
		func = cpqhp_slot_find(func->bus, func->device, index++);
	}

	return(0);
}


/*
 * cpqhp_configure_board
 *
 * Copies saved configuration information to one slot.
 * this is called recursively for bridge devices.
 * this is for hot plug REPLACE!
 *
 * returns 0 if success
 */
int cpqhp_configure_board(struct controller *ctrl, struct pci_func * func)
{
	int cloop;
	u8 header_type;
	u8 secondary_bus;
	int sub_bus;
	struct pci_func *next;
	u32 temp;
	u32 rc;
	int index = 0;

	func = cpqhp_slot_find(func->bus, func->device, index++);

	while (func != NULL) {
		// Start at the top of config space so that the control
		// registers are programmed last
		for (cloop = 0x3C; cloop > 0; cloop -= 4) {
			pci_write_config_dword_nodev(ctrl->pci_ops, func->bus, func->device, func->function, cloop, func->config_space[cloop >> 2]);
		}

		pci_read_config_byte_nodev (ctrl->pci_ops, func->bus, func->device, func->function, PCI_HEADER_TYPE, &header_type);

		// If this is a bridge device, restore subordinate devices
		if ((header_type & 0x7F) == PCI_HEADER_TYPE_BRIDGE) {	  // PCI-PCI Bridge
			pci_read_config_byte_nodev (ctrl->pci_ops, func->bus, func->device, func->function, PCI_SECONDARY_BUS, &secondary_bus);

			sub_bus = (int) secondary_bus;

			next = cpqhp_slot_list[sub_bus];

			while (next != NULL) {
				rc = cpqhp_configure_board(ctrl, next);

				if (rc)
					return rc;

				next = next->next;
			}
		} else {

			// Check all the base Address Registers to make sure
			// they are the same.  If not, the board is different.

			for (cloop = 16; cloop < 40; cloop += 4) {
				pci_read_config_dword_nodev (ctrl->pci_ops, func->bus, func->device, func->function, cloop, &temp);

				if (temp != func->config_space[cloop >> 2]) {
					dbg("Config space compare failure!!! offset = %x\n", cloop);
					dbg("bus = %x, device = %x, function = %x\n", func->bus, func->device, func->function);
					dbg("temp = %x, config space = %x\n\n", temp, func->config_space[cloop]);
					return 1;
				}
			}
		}

		func->configured = 1;

		func = cpqhp_slot_find(func->bus, func->device, index++);
	}

	return 0;
}


/*
 * cpqhp_valid_replace
 *
 * this function checks to see if a board is the same as the
 * one it is replacing.  this check will detect if the device's
 * vendor or device id's are the same
 *
 * returns 0 if the board is the same nonzero otherwise
 */
int cpqhp_valid_replace(struct controller *ctrl, struct pci_func * func)
{
	u8 cloop;
	u8 header_type;
	u8 secondary_bus;
	u8 type;
	u32 temp_register = 0;
	u32 base;
	u32 rc;
	struct pci_func *next;
	int index = 0;

	if (!func->is_a_board)
		return(ADD_NOT_SUPPORTED);

	func = cpqhp_slot_find(func->bus, func->device, index++);

	while (func != NULL) {
		pci_read_config_dword_nodev (ctrl->pci_ops, func->bus, func->device, func->function, PCI_VENDOR_ID, &temp_register);

		// No adapter present
		if (temp_register == 0xFFFFFFFF)
			return(NO_ADAPTER_PRESENT);

		if (temp_register != func->config_space[0])
			return(ADAPTER_NOT_SAME);

		// Check for same revision number and class code
		pci_read_config_dword_nodev (ctrl->pci_ops, func->bus, func->device, func->function, PCI_CLASS_REVISION, &temp_register);

		// Adapter not the same
		if (temp_register != func->config_space[0x08 >> 2])
			return(ADAPTER_NOT_SAME);

		// Check for Bridge
		pci_read_config_byte_nodev (ctrl->pci_ops, func->bus, func->device, func->function, PCI_HEADER_TYPE, &header_type);

		if ((header_type & 0x7F) == PCI_HEADER_TYPE_BRIDGE) {	  // PCI-PCI Bridge
			// In order to continue checking, we must program the
			// bus registers in the bridge to respond to accesses
			// for it's subordinate bus(es)

			temp_register = func->config_space[0x18 >> 2];
			pci_write_config_dword_nodev(ctrl->pci_ops, func->bus, func->device, func->function, PCI_PRIMARY_BUS, temp_register);

			secondary_bus = (temp_register >> 8) & 0xFF;

			next = cpqhp_slot_list[secondary_bus];

			while (next != NULL) {
				rc = cpqhp_valid_replace(ctrl, next);

				if (rc)
					return(rc);

				next = next->next;
			}

		}
		// Check to see if it is a standard config header
		else if ((header_type & 0x7F) == PCI_HEADER_TYPE_NORMAL) {
			// Check subsystem vendor and ID
			pci_read_config_dword_nodev (ctrl->pci_ops, func->bus, func->device, func->function, PCI_SUBSYSTEM_VENDOR_ID, &temp_register);

			if (temp_register != func->config_space[0x2C >> 2]) {
				// If it's a SMART-2 and the register isn't filled
				// in, ignore the difference because
				// they just have an old rev of the firmware

				if (!((func->config_space[0] == 0xAE100E11)
				      && (temp_register == 0x00L)))
					return(ADAPTER_NOT_SAME);
			}
			// Figure out IO and memory base lengths
			for (cloop = 0x10; cloop <= 0x24; cloop += 4) {
				temp_register = 0xFFFFFFFF;
				pci_write_config_dword_nodev(ctrl->pci_ops, func->bus, func->device, func->function, cloop, temp_register);

				pci_read_config_dword_nodev (ctrl->pci_ops, func->bus, func->device, func->function, cloop, &base);

				if (base) {	  // If this register is implemented
					if (base & 0x01L) {
						// IO base
						// set base = amount of IO space requested
						base = base & 0xFFFFFFFE;
						base = (~base) + 1;

						type = 1;
					} else {
						// memory base
						base = base & 0xFFFFFFF0;
						base = (~base) + 1;

						type = 0;
					}
				} else {
					base = 0x0L;
					type = 0;
				}

				// Check information in slot structure
				if (func->base_length[(cloop - 0x10) >> 2] != base)
					return(ADAPTER_NOT_SAME);

				if (func->base_type[(cloop - 0x10) >> 2] != type)
					return(ADAPTER_NOT_SAME);

			}	// End of base register loop

		}		// End of (type 0 config space) else
		else {
			// this is not a type 0 or 1 config space header so
			// we don't know how to do it
			return(DEVICE_TYPE_NOT_SUPPORTED);
		}

		// Get the next function
		func = cpqhp_slot_find(func->bus, func->device, index++);
	}


	return(0);
}


/*
 * cpqhp_find_available_resources
 *
 * Finds available memory, IO, and IRQ resources for programming
 * devices which may be added to the system
 * this function is for hot plug ADD!
 *
 * returns 0 if success
 */  
int cpqhp_find_available_resources (struct controller *ctrl, void *rom_start)
{
	u8 temp;
	u8 populated_slot;
	u8 bridged_slot;
	void *one_slot;
	struct pci_func *func = NULL;
	int i = 10, index;
	u32 temp_dword, rc;
	struct pci_resource *mem_node;
	struct pci_resource *p_mem_node;
	struct pci_resource *io_node;
	struct pci_resource *bus_node;
	void *rom_resource_table;

	rom_resource_table = detect_HRT_floating_pointer(rom_start, rom_start+0xffff);
	dbg("rom_resource_table = %p\n", rom_resource_table);

	if (rom_resource_table == NULL) {
		return -ENODEV;
	}
	// Sum all resources and setup resource maps
	unused_IRQ = readl(rom_resource_table + UNUSED_IRQ);
	dbg("unused_IRQ = %x\n", unused_IRQ);

	temp = 0;
	while (unused_IRQ) {
		if (unused_IRQ & 1) {
			cpqhp_disk_irq = temp;
			break;
		}
		unused_IRQ = unused_IRQ >> 1;
		temp++;
	}

	dbg("cpqhp_disk_irq= %d\n", cpqhp_disk_irq);
	unused_IRQ = unused_IRQ >> 1;
	temp++;

	while (unused_IRQ) {
		if (unused_IRQ & 1) {
			cpqhp_nic_irq = temp;
			break;
		}
		unused_IRQ = unused_IRQ >> 1;
		temp++;
	}

	dbg("cpqhp_nic_irq= %d\n", cpqhp_nic_irq);
	unused_IRQ = readl(rom_resource_table + PCIIRQ);

	temp = 0;

	if (!cpqhp_nic_irq) {
		cpqhp_nic_irq = ctrl->interrupt;
	}

	if (!cpqhp_disk_irq) {
		cpqhp_disk_irq = ctrl->interrupt;
	}

	dbg("cpqhp_disk_irq, cpqhp_nic_irq= %d, %d\n", cpqhp_disk_irq, cpqhp_nic_irq);

	rc = compaq_nvram_load(rom_start, ctrl);
	if (rc)
		return rc;

	one_slot = rom_resource_table + sizeof (struct hrt);

	i = readb(rom_resource_table + NUMBER_OF_ENTRIES);
	dbg("number_of_entries = %d\n", i);

	if (!readb(one_slot + SECONDARY_BUS)) {
		return(1);
	}

	dbg("dev|IO base|length|Mem base|length|Pre base|length|PB SB MB\n");

	while (i && readb(one_slot + SECONDARY_BUS)) {
		u8 dev_func = readb(one_slot + DEV_FUNC);
		u8 primary_bus = readb(one_slot + PRIMARY_BUS);
		u8 secondary_bus = readb(one_slot + SECONDARY_BUS);
		u8 max_bus = readb(one_slot + MAX_BUS);
		u16 io_base = readw(one_slot + IO_BASE);
		u16 io_length = readw(one_slot + IO_LENGTH);
		u16 mem_base = readw(one_slot + MEM_BASE);
		u16 mem_length = readw(one_slot + MEM_LENGTH);
		u16 pre_mem_base = readw(one_slot + PRE_MEM_BASE);
		u16 pre_mem_length = readw(one_slot + PRE_MEM_LENGTH);

		dbg("%2.2x | %4.4x  | %4.4x | %4.4x   | %4.4x | %4.4x   | %4.4x |%2.2x %2.2x %2.2x\n",
		    dev_func, io_base, io_length, mem_base, mem_length, pre_mem_base, pre_mem_length,
		    primary_bus, secondary_bus, max_bus);

		// If this entry isn't for our controller's bus, ignore it
		if (primary_bus != ctrl->bus) {
			i--;
			one_slot += sizeof (struct slot_rt);
			continue;
		}
		// find out if this entry is for an occupied slot
		pci_read_config_dword_nodev (ctrl->pci_ops, primary_bus, dev_func >> 3, dev_func & 0x07, PCI_VENDOR_ID, &temp_dword);

		dbg("temp_D_word = %x\n", temp_dword);

		if (temp_dword != 0xFFFFFFFF) {
			index = 0;
			func = cpqhp_slot_find(primary_bus, dev_func >> 3, 0);

			while (func && (func->function != (dev_func & 0x07))) {
				dbg("func = %p (bus, dev, fun) = (%d, %d, %d)\n", func, primary_bus, dev_func >> 3, index);
				func = cpqhp_slot_find(primary_bus, dev_func >> 3, index++);
			}

			// If we can't find a match, skip this table entry
			if (!func) {
				i--;
				one_slot += sizeof (struct slot_rt);
				continue;
			}
			// this may not work and shouldn't be used
			if (secondary_bus != primary_bus)
				bridged_slot = 1;
			else
				bridged_slot = 0;

			populated_slot = 1;
		} else {
			populated_slot = 0;
			bridged_slot = 0;
		}


		// If we've got a valid IO base, use it

		temp_dword = io_base + io_length;

		if ((io_base) && (temp_dword < 0x10000)) {
			io_node = (struct pci_resource *) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);
			if (!io_node)
				return -ENOMEM;

			io_node->base = io_base;
			io_node->length = io_length;

			dbg("found io_node(base, length) = %x, %x\n", io_node->base, io_node->length);
			dbg("populated slot =%d \n", populated_slot);
			if (!populated_slot) {
				io_node->next = ctrl->io_head;
				ctrl->io_head = io_node;
			} else {
				io_node->next = func->io_head;
				func->io_head = io_node;
			}
		}

		// If we've got a valid memory base, use it
		temp_dword = mem_base + mem_length;
		if ((mem_base) && (temp_dword < 0x10000)) {
			mem_node = (struct pci_resource *) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);
			if (!mem_node)
				return -ENOMEM;

			mem_node->base = mem_base << 16;

			mem_node->length = mem_length << 16;

			dbg("found mem_node(base, length) = %x, %x\n", mem_node->base, mem_node->length);
			dbg("populated slot =%d \n", populated_slot);
			if (!populated_slot) {
				mem_node->next = ctrl->mem_head;
				ctrl->mem_head = mem_node;
			} else {
				mem_node->next = func->mem_head;
				func->mem_head = mem_node;
			}
		}

		// If we've got a valid prefetchable memory base, and
		// the base + length isn't greater than 0xFFFF
		temp_dword = pre_mem_base + pre_mem_length;
		if ((pre_mem_base) && (temp_dword < 0x10000)) {
			p_mem_node = (struct pci_resource *) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);
			if (!p_mem_node)
				return -ENOMEM;

			p_mem_node->base = pre_mem_base << 16;

			p_mem_node->length = pre_mem_length << 16;
			dbg("found p_mem_node(base, length) = %x, %x\n", p_mem_node->base, p_mem_node->length);
			dbg("populated slot =%d \n", populated_slot);

			if (!populated_slot) {
				p_mem_node->next = ctrl->p_mem_head;
				ctrl->p_mem_head = p_mem_node;
			} else {
				p_mem_node->next = func->p_mem_head;
				func->p_mem_head = p_mem_node;
			}
		}

		// If we've got a valid bus number, use it
		// The second condition is to ignore bus numbers on
		// populated slots that don't have PCI-PCI bridges
		if (secondary_bus && (secondary_bus != primary_bus)) {
			bus_node = (struct pci_resource *) kmalloc(sizeof(struct pci_resource), GFP_KERNEL);
			if (!bus_node)
				return -ENOMEM;

			bus_node->base = secondary_bus;
			bus_node->length = max_bus - secondary_bus + 1;
			dbg("found bus_node(base, length) = %x, %x\n", bus_node->base, bus_node->length);
			dbg("populated slot =%d \n", populated_slot);
			if (!populated_slot) {
				bus_node->next = ctrl->bus_head;
				ctrl->bus_head = bus_node;
			} else {
				bus_node->next = func->bus_head;
				func->bus_head = bus_node;
			}
		}

		i--;
		one_slot += sizeof (struct slot_rt);
	}

	// If all of the following fail, we don't have any resources for
	// hot plug add
	rc = 1;
	rc &= cpqhp_resource_sort_and_combine(&(ctrl->mem_head));
	rc &= cpqhp_resource_sort_and_combine(&(ctrl->p_mem_head));
	rc &= cpqhp_resource_sort_and_combine(&(ctrl->io_head));
	rc &= cpqhp_resource_sort_and_combine(&(ctrl->bus_head));

	return(rc);
}


/*
 * cpqhp_return_board_resources
 *
 * this routine returns all resources allocated to a board to
 * the available pool.
 *
 * returns 0 if success
 */
int cpqhp_return_board_resources(struct pci_func * func, struct resource_lists * resources)
{
	int rc = 0;
	struct pci_resource *node;
	struct pci_resource *t_node;
	dbg("%s\n", __FUNCTION__);

	if (!func)
		return(1);

	node = func->io_head;
	func->io_head = NULL;
	while (node) {
		t_node = node->next;
		return_resource(&(resources->io_head), node);
		node = t_node;
	}

	node = func->mem_head;
	func->mem_head = NULL;
	while (node) {
		t_node = node->next;
		return_resource(&(resources->mem_head), node);
		node = t_node;
	}

	node = func->p_mem_head;
	func->p_mem_head = NULL;
	while (node) {
		t_node = node->next;
		return_resource(&(resources->p_mem_head), node);
		node = t_node;
	}

	node = func->bus_head;
	func->bus_head = NULL;
	while (node) {
		t_node = node->next;
		return_resource(&(resources->bus_head), node);
		node = t_node;
	}

	rc |= cpqhp_resource_sort_and_combine(&(resources->mem_head));
	rc |= cpqhp_resource_sort_and_combine(&(resources->p_mem_head));
	rc |= cpqhp_resource_sort_and_combine(&(resources->io_head));
	rc |= cpqhp_resource_sort_and_combine(&(resources->bus_head));

	return(rc);
}


/*
 * cpqhp_destroy_resource_list
 *
 * Puts node back in the resource list pointed to by head
 */
void cpqhp_destroy_resource_list (struct resource_lists * resources)
{
	struct pci_resource *res, *tres;

	res = resources->io_head;
	resources->io_head = NULL;

	while (res) {
		tres = res;
		res = res->next;
		kfree(tres);
	}

	res = resources->mem_head;
	resources->mem_head = NULL;

	while (res) {
		tres = res;
		res = res->next;
		kfree(tres);
	}

	res = resources->p_mem_head;
	resources->p_mem_head = NULL;

	while (res) {
		tres = res;
		res = res->next;
		kfree(tres);
	}

	res = resources->bus_head;
	resources->bus_head = NULL;

	while (res) {
		tres = res;
		res = res->next;
		kfree(tres);
	}
}


/*
 * cpqhp_destroy_board_resources
 *
 * Puts node back in the resource list pointed to by head
 */
void cpqhp_destroy_board_resources (struct pci_func * func)
{
	struct pci_resource *res, *tres;

	res = func->io_head;
	func->io_head = NULL;

	while (res) {
		tres = res;
		res = res->next;
		kfree(tres);
	}

	res = func->mem_head;
	func->mem_head = NULL;

	while (res) {
		tres = res;
		res = res->next;
		kfree(tres);
	}

	res = func->p_mem_head;
	func->p_mem_head = NULL;

	while (res) {
		tres = res;
		res = res->next;
		kfree(tres);
	}

	res = func->bus_head;
	func->bus_head = NULL;

	while (res) {
		tres = res;
		res = res->next;
		kfree(tres);
	}
}

