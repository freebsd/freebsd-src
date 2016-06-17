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
 * Jan 12, 2003 -	Added 66/100/133MHz PCI-X support, 
 * 			Torben Mathiasen <torben.mathiasen@hp.com>
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include "cpqphp.h"
#include "cpqphp_nvram.h"
#include "../../arch/i386/kernel/pci-i386.h"	/* horrible hack showing how processor dependant we are... */


/* Global variables */
int cpqhp_debug;
struct controller *cpqhp_ctrl_list;	/* = NULL */
struct pci_func *cpqhp_slot_list[256];

/* local variables */
static void *smbios_table;
static void *smbios_start;
static void *cpqhp_rom_start;
static u8 power_mode;
static int debug;

#define DRIVER_VERSION	"0.9.7"
#define DRIVER_AUTHOR	"Dan Zink <dan.zink@compaq.com>, Greg Kroah-Hartman <greg@kroah.com>"
#define DRIVER_DESC	"Compaq Hot Plug PCI Controller Driver"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

MODULE_PARM(power_mode, "b");
MODULE_PARM_DESC(power_mode, "Power mode enabled or not");

MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debugging mode enabled or not");

#define CPQHPC_MODULE_MINOR 208

static int one_time_init (void);
static int set_attention_status (struct hotplug_slot *slot, u8 value);
static int process_SI		(struct hotplug_slot *slot);
static int process_SS		(struct hotplug_slot *slot);
static int hardware_test	(struct hotplug_slot *slot, u32 value);
static int get_power_status	(struct hotplug_slot *slot, u8 *value);
static int get_attention_status	(struct hotplug_slot *slot, u8 *value);
static int get_latch_status	(struct hotplug_slot *slot, u8 *value);
static int get_adapter_status	(struct hotplug_slot *slot, u8 *value);
static int get_max_bus_speed	(struct hotplug_slot *slot, enum pci_bus_speed *value);
static int get_cur_bus_speed	(struct hotplug_slot *slot, enum pci_bus_speed *value);

static struct hotplug_slot_ops cpqphp_hotplug_slot_ops = {
	.owner =		THIS_MODULE,
	.set_attention_status =	set_attention_status,
	.enable_slot =		process_SI,
	.disable_slot =		process_SS,
	.hardware_test =	hardware_test,
	.get_power_status =	get_power_status,
	.get_attention_status =	get_attention_status,
	.get_latch_status =	get_latch_status,
	.get_adapter_status =	get_adapter_status,
 	.get_max_bus_speed =	get_max_bus_speed,
 	.get_cur_bus_speed =	get_cur_bus_speed,
};


static inline int is_slot64bit (struct slot *slot)
{
	if (!slot || !slot->p_sm_slot)
		return 0;

	if (readb(slot->p_sm_slot + SMBIOS_SLOT_WIDTH) == 0x06)
		return 1;

	return 0;
}

static inline int is_slot66mhz (struct slot *slot)
{
	if (!slot || !slot->p_sm_slot)
		return 0;

	if (readb(slot->p_sm_slot + SMBIOS_SLOT_TYPE) == 0x0E)
		return 1;

	return 0;
}

/**
 * detect_SMBIOS_pointer - find the system Management BIOS Table in the specified region of memory.
 *
 * @begin: begin pointer for region to be scanned.
 * @end: end pointer for region to be scanned.
 *
 * Returns pointer to the head of the SMBIOS tables (or NULL)
 *
 */
static void * detect_SMBIOS_pointer(void *begin, void *end)
{
	void *fp;
	void *endp;
	u8 temp1, temp2, temp3, temp4;
	int status = 0;

	endp = (end - sizeof(u32) + 1);

	for (fp = begin; fp <= endp; fp += 16) {
		temp1 = readb(fp);
		temp2 = readb(fp+1);
		temp3 = readb(fp+2);
		temp4 = readb(fp+3);
		if (temp1 == '_' &&
		    temp2 == 'S' &&
		    temp3 == 'M' &&
		    temp4 == '_') {
			status = 1;
			break;
		}
	}
	
	if (!status)
		fp = NULL;

	dbg("Discovered SMBIOS Entry point at %p\n", fp);

	return fp;
}

/**
 * init_SERR - Initializes the per slot SERR generation.
 *
 * For unexpected switch opens
 *
 */
static int init_SERR(struct controller * ctrl)
{
	u32 tempdword;
	u32 number_of_slots;
	u8 physical_slot;

	if (!ctrl)
		return 1;

	tempdword = ctrl->first_slot;

	number_of_slots = readb(ctrl->hpc_reg + SLOT_MASK) & 0x0F;
	// Loop through slots
	while (number_of_slots) {
		physical_slot = tempdword;
		writeb(0, ctrl->hpc_reg + SLOT_SERR);
		tempdword++;
		number_of_slots--;
	}

	return 0;
}


/* nice debugging output */
static int pci_print_IRQ_route (void)
{
	struct irq_routing_table *routing_table;
	int len;
	int loop;

	u8 tbus, tdevice, tslot;

	routing_table = pcibios_get_irq_routing_table();
	if (routing_table == NULL) {
		err("No BIOS Routing Table??? Not good\n");
		return -ENOMEM;
	}

	len = (routing_table->size - sizeof(struct irq_routing_table)) / sizeof(struct irq_info);
	// Make sure I got at least one entry
	if (len == 0) {
		kfree(routing_table);
		return -1;
	}

	dbg("bus dev func slot\n");

	for (loop = 0; loop < len; ++loop) {
		tbus = routing_table->slots[loop].bus;
		tdevice = routing_table->slots[loop].devfn;
		tslot = routing_table->slots[loop].slot;
		dbg("%d %d %d %d\n", tbus, tdevice >> 3, tdevice & 0x7, tslot);

	}
	kfree(routing_table);
	return 0;
}


/*
 * get_subsequent_smbios_entry
 *
 * Gets the first entry if previous == NULL
 * Otherwise, returns the next entry
 * Uses global SMBIOS Table pointer
 *
 * @curr: %NULL or pointer to previously returned structure
 *
 * returns a pointer to an SMBIOS structure or NULL if none found
 */
static void * get_subsequent_smbios_entry(void *smbios_start, void *smbios_table, void *curr)
{
	u8 bail = 0;
	u8 previous_byte = 1;
	void *p_temp;
	void *p_max;

	if (!smbios_table || !curr)
		return(NULL);

	// set p_max to the end of the table
	p_max = smbios_start + readw(smbios_table + ST_LENGTH);

	p_temp = curr;
	p_temp += readb(curr + SMBIOS_GENERIC_LENGTH);

	while ((p_temp < p_max) && !bail) {
		// Look for the double NULL terminator
		// The first condition is the previous byte and the second is the curr
		if (!previous_byte && !(readb(p_temp))) {
			bail = 1;
		}

		previous_byte = readb(p_temp);
		p_temp++;
	}

	if (p_temp < p_max) {
		return p_temp;
	} else {
		return NULL;
	}
}


/**
 * get_SMBIOS_entry
 *
 * @type:SMBIOS structure type to be returned
 * @previous: %NULL or pointer to previously returned structure
 *
 * Gets the first entry of the specified type if previous == NULL
 * Otherwise, returns the next entry of the given type.
 * Uses global SMBIOS Table pointer
 * Uses get_subsequent_smbios_entry
 *
 * returns a pointer to an SMBIOS structure or %NULL if none found
 */
static void *get_SMBIOS_entry (void *smbios_start, void *smbios_table, u8 type, void * previous)
{
	if (!smbios_table)
		return NULL;

	if (!previous) {		  
		previous = smbios_start;
	} else {
		previous = get_subsequent_smbios_entry(smbios_start, smbios_table, previous);
	}

	while (previous) {
	       	if (readb(previous + SMBIOS_GENERIC_TYPE) != type) {
			previous = get_subsequent_smbios_entry(smbios_start, smbios_table, previous);
		} else {
			break;
		}
	}

	return previous;
}


static int ctrl_slot_setup (struct controller * ctrl, void *smbios_start, void *smbios_table)
{
	struct slot *new_slot;
	u8 number_of_slots;
	u8 slot_device;
	u8 slot_number;
	u8 ctrl_slot;
	u32 tempdword;
	void *slot_entry= NULL;
	int result;

	dbg("%s\n", __FUNCTION__);

	tempdword = readl(ctrl->hpc_reg + INT_INPUT_CLEAR);

	number_of_slots = readb(ctrl->hpc_reg + SLOT_MASK) & 0x0F;
	slot_device = readb(ctrl->hpc_reg + SLOT_MASK) >> 4;
	slot_number = ctrl->first_slot;

	while (number_of_slots) {
		new_slot = (struct slot *) kmalloc(sizeof(struct slot), GFP_KERNEL);
		if (!new_slot)
			return -ENOMEM;

		memset(new_slot, 0, sizeof(struct slot));
		new_slot->hotplug_slot = kmalloc (sizeof (struct hotplug_slot), GFP_KERNEL);
		if (!new_slot->hotplug_slot) {
			kfree (new_slot);
			return -ENOMEM;
		}
		memset(new_slot->hotplug_slot, 0, sizeof (struct hotplug_slot));

		new_slot->hotplug_slot->info = kmalloc (sizeof (struct hotplug_slot_info), GFP_KERNEL);
		if (!new_slot->hotplug_slot->info) {
			kfree (new_slot->hotplug_slot);
			kfree (new_slot);
			return -ENOMEM;
		}
		memset(new_slot->hotplug_slot->info, 0, sizeof (struct hotplug_slot_info));
		new_slot->hotplug_slot->name = kmalloc (SLOT_NAME_SIZE, GFP_KERNEL);
		if (!new_slot->hotplug_slot->name) {
			kfree (new_slot->hotplug_slot->info);
			kfree (new_slot->hotplug_slot);
			kfree (new_slot);
			return -ENOMEM;
		}

		new_slot->magic = SLOT_MAGIC;
		new_slot->ctrl = ctrl;
		new_slot->bus = ctrl->bus;
		new_slot->device = slot_device;
		new_slot->number = slot_number;
		dbg("slot->number = %d\n",new_slot->number);

		slot_entry = get_SMBIOS_entry(smbios_start, smbios_table, 9, slot_entry);

		while (slot_entry && (readw(slot_entry + SMBIOS_SLOT_NUMBER) != new_slot->number)) {
			slot_entry = get_SMBIOS_entry(smbios_start, smbios_table, 9, slot_entry);
		}

		new_slot->p_sm_slot = slot_entry;

		init_timer(&new_slot->task_event);
		new_slot->task_event.expires = jiffies + 5 * HZ;
		new_slot->task_event.function = cpqhp_pushbutton_thread;

		//FIXME: these capabilities aren't used but if they are
		//       they need to be correctly implemented
		new_slot->capabilities |= PCISLOT_REPLACE_SUPPORTED;
		new_slot->capabilities |= PCISLOT_INTERLOCK_SUPPORTED;

		if (is_slot64bit(new_slot))
			new_slot->capabilities |= PCISLOT_64_BIT_SUPPORTED;
		if (is_slot66mhz(new_slot))
			new_slot->capabilities |= PCISLOT_66_MHZ_SUPPORTED;
		if (ctrl->speed == PCI_SPEED_66MHz)
			new_slot->capabilities |= PCISLOT_66_MHZ_OPERATION;

		ctrl_slot = slot_device - (readb(ctrl->hpc_reg + SLOT_MASK) >> 4);

		// Check presence
		new_slot->capabilities |= ((((~tempdword) >> 23) | ((~tempdword) >> 15)) >> ctrl_slot) & 0x02;
		// Check the switch state
		new_slot->capabilities |= ((~tempdword & 0xFF) >> ctrl_slot) & 0x01;
		// Check the slot enable
		new_slot->capabilities |= ((read_slot_enable(ctrl) << 2) >> ctrl_slot) & 0x04;

		/* register this slot with the hotplug pci core */
		new_slot->hotplug_slot->private = new_slot;
		make_slot_name (new_slot->hotplug_slot->name, SLOT_NAME_SIZE, new_slot);
		new_slot->hotplug_slot->ops = &cpqphp_hotplug_slot_ops;
		
		new_slot->hotplug_slot->info->power_status = get_slot_enabled(ctrl, new_slot);
		new_slot->hotplug_slot->info->attention_status = cpq_get_attention_status(ctrl, new_slot);
		new_slot->hotplug_slot->info->latch_status = cpq_get_latch_status(ctrl, new_slot);
		new_slot->hotplug_slot->info->adapter_status = get_presence_status(ctrl, new_slot);
		
		dbg ("registering bus %d, dev %d, number %d, ctrl->slot_device_offset %d, slot %d\n", 
		     new_slot->bus, new_slot->device, new_slot->number, ctrl->slot_device_offset, slot_number);
		result = pci_hp_register (new_slot->hotplug_slot);
		if (result) {
			err ("pci_hp_register failed with error %d\n", result);
			kfree (new_slot->hotplug_slot->info);
			kfree (new_slot->hotplug_slot->name);
			kfree (new_slot->hotplug_slot);
			kfree (new_slot);
			return result;
		}
		
		new_slot->next = ctrl->slot;
		ctrl->slot = new_slot;

		number_of_slots--;
		slot_device++;
		slot_number++;
	}

	return(0);
}


static int ctrl_slot_cleanup (struct controller * ctrl)
{
	struct slot *old_slot, *next_slot;

	old_slot = ctrl->slot;
	ctrl->slot = NULL;

	while (old_slot) {
		next_slot = old_slot->next;
		pci_hp_deregister (old_slot->hotplug_slot);
		kfree(old_slot->hotplug_slot->info);
		kfree(old_slot->hotplug_slot->name);
		kfree(old_slot->hotplug_slot);
		kfree(old_slot);
		old_slot = next_slot;
	}

	//Free IRQ associated with hot plug device
	free_irq(ctrl->interrupt, ctrl);
	//Unmap the memory
	iounmap(ctrl->hpc_reg);
	//Finally reclaim PCI mem
	release_mem_region(pci_resource_start(ctrl->pci_dev, 0),
			   pci_resource_len(ctrl->pci_dev, 0));

	return(0);
}


//============================================================================
// function:	get_slot_mapping
//
// Description: Attempts to determine a logical slot mapping for a PCI
//		device.  Won't work for more than one PCI-PCI bridge
//		in a slot.
//
// Input:	u8 bus_num - bus number of PCI device
//		u8 dev_num - device number of PCI device
//		u8 *slot - Pointer to u8 where slot number will
//			be returned
//
// Output:	SUCCESS or FAILURE
//=============================================================================
static int get_slot_mapping (struct pci_ops *ops, u8 bus_num, u8 dev_num, u8 *slot)
{
	struct irq_routing_table *PCIIRQRoutingInfoLength;
	u32 work;
	long len;
	long loop;

	u8 tbus, tdevice, tslot, bridgeSlot;

	dbg("%s %p, %d, %d, %p\n", __FUNCTION__, ops, bus_num, dev_num, slot);

	bridgeSlot = 0xFF;

	PCIIRQRoutingInfoLength = pcibios_get_irq_routing_table();

	len = (PCIIRQRoutingInfoLength->size -
	       sizeof(struct irq_routing_table)) / sizeof(struct irq_info);
	// Make sure I got at least one entry
	if (len == 0) {
		if (PCIIRQRoutingInfoLength != NULL) kfree(PCIIRQRoutingInfoLength );
		return -1;
	}


	for (loop = 0; loop < len; ++loop) {
		tbus = PCIIRQRoutingInfoLength->slots[loop].bus;
		tdevice = PCIIRQRoutingInfoLength->slots[loop].devfn >> 3;
		tslot = PCIIRQRoutingInfoLength->slots[loop].slot;

		if ((tbus == bus_num) && (tdevice == dev_num)) {
			*slot = tslot;

			if (PCIIRQRoutingInfoLength != NULL) kfree(PCIIRQRoutingInfoLength );
			return 0;
		} else {
			// Didn't get a match on the target PCI device. Check if the
			// current IRQ table entry is a PCI-to-PCI bridge device.  If so,
			// and it's secondary bus matches the bus number for the target 
			// device, I need to save the bridge's slot number.  If I can't 
			// find an entry for the target device, I will have to assume it's 
			// on the other side of the bridge, and assign it the bridge's slot.
			pci_read_config_dword_nodev (ops, tbus, tdevice, 0, PCI_REVISION_ID, &work);

			if ((work >> 8) == PCI_TO_PCI_BRIDGE_CLASS) {
				pci_read_config_dword_nodev (ops, tbus, tdevice, 0, PCI_PRIMARY_BUS, &work);
				// See if bridge's secondary bus matches target bus.
				if (((work >> 8) & 0x000000FF) == (long) bus_num) {
					bridgeSlot = tslot;
				}
			}
		}

	}


	// If we got here, we didn't find an entry in the IRQ mapping table 
	// for the target PCI device.  If we did determine that the target 
	// device is on the other side of a PCI-to-PCI bridge, return the 
	// slot number for the bridge.
	if (bridgeSlot != 0xFF) {
		*slot = bridgeSlot;
		if (PCIIRQRoutingInfoLength != NULL) kfree(PCIIRQRoutingInfoLength );
		return 0;
	}
	if (PCIIRQRoutingInfoLength != NULL) kfree(PCIIRQRoutingInfoLength );
	// Couldn't find an entry in the routing table for this PCI device
	return -1;
}


/**
 * cpqhp_set_attention_status - Turns the Amber LED for a slot on or off
 *
 */
static int cpqhp_set_attention_status (struct controller *ctrl, struct pci_func *func, u32 status)
{
	u8 hp_slot;

	hp_slot = func->device - ctrl->slot_device_offset;

	if (func == NULL)
		return(1);

	// Wait for exclusive access to hardware
	down(&ctrl->crit_sect);

	if (status == 1) {
		amber_LED_on (ctrl, hp_slot);
	} else if (status == 0) {
		amber_LED_off (ctrl, hp_slot);
	} else {
		// Done with exclusive hardware access
		up(&ctrl->crit_sect);
		return(1);
	}

	set_SOGO(ctrl);

	// Wait for SOBS to be unset
	wait_for_ctrl_irq (ctrl);

	// Done with exclusive hardware access
	up(&ctrl->crit_sect);

	return(0);
}


/**
 * set_attention_status - Turns the Amber LED for a slot on or off
 *
 */
static int set_attention_status (struct hotplug_slot *hotplug_slot, u8 status)
{
	struct pci_func *slot_func;
	struct slot *slot = get_slot (hotplug_slot, __FUNCTION__);
	struct controller *ctrl;
	u8 bus;
	u8 devfn;
	u8 device;
	u8 function;
	
	if (slot == NULL)
		return -ENODEV;
	
	dbg("%s - physical_slot = %s\n", __FUNCTION__, hotplug_slot->name);

	ctrl = slot->ctrl;
	if (ctrl == NULL)
		return -ENODEV;
	
	if (cpqhp_get_bus_dev(ctrl, &bus, &devfn, slot->number) == -1)
		return -ENODEV;

	device = devfn >> 3;
	function = devfn & 0x7;
	dbg("bus, dev, fn = %d, %d, %d\n", bus, device, function);

	slot_func = cpqhp_slot_find(bus, device, function);
	if (!slot_func) {
		return -ENODEV;
	}

	return cpqhp_set_attention_status(ctrl, slot_func, status);
}


static int process_SI (struct hotplug_slot *hotplug_slot)
{
	struct pci_func *slot_func;
	struct slot *slot = get_slot (hotplug_slot, __FUNCTION__);
	struct controller *ctrl;
	u8 bus;
	u8 devfn;
	u8 device;
	u8 function;
	
	if (slot == NULL)
		return -ENODEV;
	
	dbg("%s - physical_slot = %s\n", __FUNCTION__, hotplug_slot->name);

	ctrl = slot->ctrl;
	if (ctrl == NULL)
		return -ENODEV;
	
	if (cpqhp_get_bus_dev(ctrl, &bus, &devfn, slot->number) == -1)
		return -ENODEV;

	device = devfn >> 3;
	function = devfn & 0x7;
	dbg("bus, dev, fn = %d, %d, %d\n", bus, device, function);

	slot_func = cpqhp_slot_find(bus, device, function);
	if (!slot_func) {
		return -ENODEV;
	}

	slot_func->bus = bus;
	slot_func->device = device;
	slot_func->function = function;
	slot_func->configured = 0;
	dbg("board_added(%p, %p)\n", slot_func, ctrl);
	return cpqhp_process_SI(ctrl, slot_func);
}


static int process_SS (struct hotplug_slot *hotplug_slot)
{
	struct pci_func *slot_func;
	struct slot *slot = get_slot (hotplug_slot, __FUNCTION__);
	struct controller *ctrl;
	u8 bus;
	u8 devfn;
	u8 device;
	u8 function;
	
	if (slot == NULL)
		return -ENODEV;
	
	dbg("%s - physical_slot = %s\n", __FUNCTION__, hotplug_slot->name);

	ctrl = slot->ctrl;
	if (ctrl == NULL)
		return -ENODEV;
	
	if (cpqhp_get_bus_dev(ctrl, &bus, &devfn, slot->number) == -1)
		return -ENODEV;

	device = devfn >> 3;
	function = devfn & 0x7;
	dbg("bus, dev, fn = %d, %d, %d\n", bus, device, function);

	slot_func = cpqhp_slot_find(bus, device, function);
	if (!slot_func) {
		return -ENODEV;
	}
	
	dbg("In power_down_board, slot_func = %p, ctrl = %p\n", slot_func, ctrl);
	return cpqhp_process_SS(ctrl, slot_func);
}


static int hardware_test (struct hotplug_slot *hotplug_slot, u32 value)
{
	struct slot *slot = get_slot (hotplug_slot, __FUNCTION__);
	struct controller *ctrl;

	dbg("%s\n", __FUNCTION__);

	if (slot == NULL)
		return -ENODEV;

	ctrl = slot->ctrl;
	if (ctrl == NULL)
		return -ENODEV;

	return cpqhp_hardware_test (ctrl, value);	
}


static int get_power_status (struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct slot *slot = get_slot (hotplug_slot, __FUNCTION__);
	struct controller *ctrl;
	
	if (slot == NULL)
		return -ENODEV;
	
	dbg("%s - physical_slot = %s\n", __FUNCTION__, hotplug_slot->name);

	ctrl = slot->ctrl;
	if (ctrl == NULL)
		return -ENODEV;
	
	*value = get_slot_enabled(ctrl, slot);
	return 0;
}

static int get_attention_status (struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct slot *slot = get_slot (hotplug_slot, __FUNCTION__);
	struct controller *ctrl;
	
	if (slot == NULL)
		return -ENODEV;
	
	dbg("%s - physical_slot = %s\n", __FUNCTION__, hotplug_slot->name);

	ctrl = slot->ctrl;
	if (ctrl == NULL)
		return -ENODEV;
	
	*value = cpq_get_attention_status(ctrl, slot);
	return 0;
}

static int get_latch_status (struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct slot *slot = get_slot (hotplug_slot, __FUNCTION__);
	struct controller *ctrl;
	
	if (slot == NULL)
		return -ENODEV;
	
	dbg("%s - physical_slot = %s\n", __FUNCTION__, hotplug_slot->name);

	ctrl = slot->ctrl;
	if (ctrl == NULL)
		return -ENODEV;
	
	*value = cpq_get_latch_status (ctrl, slot);

	return 0;
}

static int get_adapter_status (struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct slot *slot = get_slot (hotplug_slot, __FUNCTION__);
	struct controller *ctrl;
	
	if (slot == NULL)
		return -ENODEV;
	
	dbg("%s - physical_slot = %s\n", __FUNCTION__, hotplug_slot->name);

	ctrl = slot->ctrl;
	if (ctrl == NULL)
		return -ENODEV;
	
	*value = get_presence_status (ctrl, slot);

	return 0;
}

static int get_max_bus_speed (struct hotplug_slot *hotplug_slot, enum pci_bus_speed *value)
{
	struct slot *slot = get_slot (hotplug_slot, __FUNCTION__);
	struct controller *ctrl;
	
	if (slot == NULL)
		return -ENODEV;

	dbg("%s - physical_slot = %s\n", __FUNCTION__, hotplug_slot->name);

	ctrl = slot->ctrl;
	if (ctrl == NULL)
		return -ENODEV;
	
	*value = ctrl->speed_capability;

	return 0;
}

static int get_cur_bus_speed (struct hotplug_slot *hotplug_slot, enum pci_bus_speed *value)
{
	struct slot *slot = get_slot (hotplug_slot, __FUNCTION__);
	struct controller *ctrl;
	
	if (slot == NULL)
		return -ENODEV;

	dbg("%s - physical_slot = %s\n", __FUNCTION__, hotplug_slot->name);

	ctrl = slot->ctrl;
	if (ctrl == NULL)
		return -ENODEV;
	
	*value = ctrl->speed;

	return 0;
}

static int cpqhpc_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	u8 num_of_slots = 0;
	u8 hp_slot = 0;
	u8 device;
	u8 rev;
	u8 bus_cap;
	u16 temp_word;
	u16 vendor_id;
	u16 subsystem_vid;
	u16 subsystem_deviceid;
	u32 rc;
	struct controller *ctrl;
	struct pci_func *func;

	// Need to read VID early b/c it's used to differentiate CPQ and INTC discovery
	rc = pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor_id);
	if (rc || ((vendor_id != PCI_VENDOR_ID_COMPAQ) && (vendor_id != PCI_VENDOR_ID_INTEL))) {
		err(msg_HPC_non_compaq_or_intel);
		return -ENODEV;
	}
	dbg("Vendor ID: %x\n", vendor_id);

	rc = pci_read_config_byte(pdev, PCI_REVISION_ID, &rev);
	dbg("revision: %d\n", rev);
	if (rc || ((vendor_id == PCI_VENDOR_ID_COMPAQ) && (!rev))) {
		err(msg_HPC_rev_error);
		return -ENODEV;
	}

	/* Check for the proper subsytem ID's
	 * Intel uses a different SSID programming model than Compaq.  
	 * For Intel, each SSID bit identifies a PHP capability.
	 * Also Intel HPC's may have RID=0.
	 */
	if ((rev > 2) || (vendor_id == PCI_VENDOR_ID_INTEL)) {
		// TODO: This code can be made to support non-Compaq or Intel subsystem IDs
		rc = pci_read_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID, &subsystem_vid);
		if (rc) {
			err("%s : pci_read_config_word failed\n", __FUNCTION__);
			return rc;
		}
		dbg("Subsystem Vendor ID: %x\n", subsystem_vid);
		if ((subsystem_vid != PCI_VENDOR_ID_COMPAQ) && (subsystem_vid != PCI_VENDOR_ID_INTEL)) {
			err(msg_HPC_non_compaq_or_intel);
			return -ENODEV;
		}

		ctrl = (struct controller *) kmalloc(sizeof(struct controller), GFP_KERNEL);
		if (!ctrl) {
			err("%s : out of memory\n", __FUNCTION__);
			return -ENOMEM;
		}
		memset(ctrl, 0, sizeof(struct controller));

		rc = pci_read_config_word(pdev, PCI_SUBSYSTEM_ID, &subsystem_deviceid);
		if (rc) {
			err("%s : pci_read_config_word failed\n", __FUNCTION__);
			goto err_free_ctrl;
		}

		info("Hot Plug Subsystem Device ID: %x\n", subsystem_deviceid);

		/* Set Vendor ID, so it can be accessed later from other functions */
		ctrl->vendor_id = vendor_id;

		switch (subsystem_vid) {
			case PCI_VENDOR_ID_COMPAQ:
				if (rev >= 0x13) { /* CIOBX */
					ctrl->push_flag = 1;
					ctrl->slot_switch_type = 1;		// Switch is present
					ctrl->push_button = 1;			// Pushbutton is present
					ctrl->pci_config_space = 1;		// Index/data access to working registers 0 = not supported, 1 = supported
					ctrl->defeature_PHP = 1;		// PHP is supported
					ctrl->pcix_support = 1;			// PCI-X supported
					ctrl->pcix_speed_capability = 1;
					pci_read_config_byte(pdev, 0x41, &bus_cap);
					if (bus_cap & 0x80) {
						dbg("bus max supports 133MHz PCI-X\n");
						ctrl->speed_capability = PCI_SPEED_133MHz_PCIX;
						break;
					}
					if (bus_cap & 0x40) {
						dbg("bus max supports 100MHz PCI-X\n");
						ctrl->speed_capability = PCI_SPEED_100MHz_PCIX;
						break;
					}
					if (bus_cap & 20) {
						dbg("bus max supports 66MHz PCI-X\n");
						ctrl->speed_capability = PCI_SPEED_66MHz_PCIX;
						break;
					}
					if (bus_cap & 10) {
						dbg("bus max supports 66MHz PCI\n");
						ctrl->speed_capability = PCI_SPEED_66MHz;
						break;
					}

					break;
				}

				switch (subsystem_deviceid) {
					case PCI_SUB_HPC_ID:
						/* Original 6500/7000 implementation */
						ctrl->slot_switch_type = 1;		// Switch is present
						ctrl->speed_capability = PCI_SPEED_33MHz;
						ctrl->push_button = 0;			// No pushbutton
						ctrl->pci_config_space = 1;		// Index/data access to working registers 0 = not supported, 1 = supported
						ctrl->defeature_PHP = 1;		// PHP is supported
						ctrl->pcix_support = 0;			// PCI-X not supported
						ctrl->pcix_speed_capability = 0;	// N/A since PCI-X not supported
						break;
					case PCI_SUB_HPC_ID2:
						/* First Pushbutton implementation */
						ctrl->push_flag = 1;
						ctrl->slot_switch_type = 1;		// Switch is present
						ctrl->speed_capability = PCI_SPEED_33MHz;
						ctrl->push_button = 1;			// Pushbutton is present
						ctrl->pci_config_space = 1;		// Index/data access to working registers 0 = not supported, 1 = supported
						ctrl->defeature_PHP = 1;		// PHP is supported
						ctrl->pcix_support = 0;			// PCI-X not supported
						ctrl->pcix_speed_capability = 0;	// N/A since PCI-X not supported
						break;
					case PCI_SUB_HPC_ID_INTC:
						/* Third party (6500/7000) */
						ctrl->slot_switch_type = 1;		// Switch is present
						ctrl->speed_capability = PCI_SPEED_33MHz;
						ctrl->push_button = 0;			// No pushbutton
						ctrl->pci_config_space = 1;		// Index/data access to working registers 0 = not supported, 1 = supported
						ctrl->defeature_PHP = 1;			// PHP is supported
						ctrl->pcix_support = 0;			// PCI-X not supported
						ctrl->pcix_speed_capability = 0;		// N/A since PCI-X not supported
						break;
					case PCI_SUB_HPC_ID3:
						/* First 66 Mhz implementation */
						ctrl->push_flag = 1;
						ctrl->slot_switch_type = 1;		// Switch is present
						ctrl->speed_capability = PCI_SPEED_66MHz;
						ctrl->push_button = 1;			// Pushbutton is present
						ctrl->pci_config_space = 1;		// Index/data access to working registers 0 = not supported, 1 = supported
						ctrl->defeature_PHP = 1;		// PHP is supported
						ctrl->pcix_support = 0;			// PCI-X not supported
						ctrl->pcix_speed_capability = 0;	// N/A since PCI-X not supported
						break;
					case PCI_SUB_HPC_ID4:
						/* First PCI-X implementation, 100MHz */
						ctrl->push_flag = 1;
						ctrl->slot_switch_type = 1;		// Switch is present
						ctrl->speed_capability = PCI_SPEED_100MHz_PCIX;
						ctrl->push_button = 1;			// Pushbutton is present
						ctrl->pci_config_space = 1;		// Index/data access to working registers 0 = not supported, 1 = supported
						ctrl->defeature_PHP = 1;		// PHP is supported
						ctrl->pcix_support = 1;			// PCI-X supported
						ctrl->pcix_speed_capability = 0;	
						break;
					default:
						err(msg_HPC_not_supported);
						rc = -ENODEV;
						goto err_free_ctrl;
				}
				break;

			case PCI_VENDOR_ID_INTEL:
				/* Check for speed capability (0=33, 1=66) */
				if (subsystem_deviceid & 0x0001) {
					ctrl->speed_capability = PCI_SPEED_66MHz;
				} else {
					ctrl->speed_capability = PCI_SPEED_33MHz;
				}

				/* Check for push button */
				if (subsystem_deviceid & 0x0002) {
					/* no push button */
					ctrl->push_button = 0;
				} else {
					/* push button supported */
					ctrl->push_button = 1;
				}

				/* Check for slot switch type (0=mechanical, 1=not mechanical) */
				if (subsystem_deviceid & 0x0004) {
					/* no switch */
					ctrl->slot_switch_type = 0;
				} else {
					/* switch */
					ctrl->slot_switch_type = 1;
				}

				/* PHP Status (0=De-feature PHP, 1=Normal operation) */
				if (subsystem_deviceid & 0x0008) {
					ctrl->defeature_PHP = 1;	// PHP supported
				} else {
					ctrl->defeature_PHP = 0;	// PHP not supported
				}

				/* Alternate Base Address Register Interface (0=not supported, 1=supported) */
				if (subsystem_deviceid & 0x0010) {
					ctrl->alternate_base_address = 1;	// supported
				} else {
					ctrl->alternate_base_address = 0;	// not supported
				}

				/* PCI Config Space Index (0=not supported, 1=supported) */
				if (subsystem_deviceid & 0x0020) {
					ctrl->pci_config_space = 1;		// supported
				} else {
					ctrl->pci_config_space = 0;		// not supported
				}

				/* PCI-X support */
				if (subsystem_deviceid & 0x0080) {
					/* PCI-X capable */
					ctrl->pcix_support = 1;
					/* Frequency of operation in PCI-X mode */
					if (subsystem_deviceid & 0x0040) {
						/* 133MHz PCI-X if bit 7 is 1 */
						ctrl->pcix_speed_capability = 1;
					} else {
						/* 100MHz PCI-X if bit 7 is 1 and bit 0 is 0, */
						/* 66MHz PCI-X if bit 7 is 1 and bit 0 is 1 */
						ctrl->pcix_speed_capability = 0;
					}
				} else {
					/* Conventional PCI */
					ctrl->pcix_support = 0;
					ctrl->pcix_speed_capability = 0;
				}
				break;

			default:
				err(msg_HPC_not_supported);
				rc = -ENODEV;
				goto err_free_ctrl;
		}

	} else {
		err(msg_HPC_not_supported);
		return -ENODEV;
	}

	// Tell the user that we found one.
	info("Initializing the PCI hot plug controller residing on PCI bus %d\n", pdev->bus->number);

	dbg ("Hotplug controller capabilities:\n");
	dbg ("    speed_capability       %d\n", ctrl->speed_capability);
	dbg ("    slot_switch_type       %s\n", ctrl->slot_switch_type == 0 ? "no switch" : "switch present");
	dbg ("    defeature_PHP          %s\n", ctrl->defeature_PHP == 0 ? "PHP not supported" : "PHP supported");
	dbg ("    alternate_base_address %s\n", ctrl->alternate_base_address == 0 ? "not supported" : "supported");
	dbg ("    pci_config_space       %s\n", ctrl->pci_config_space == 0 ? "not supported" : "supported");
	dbg ("    pcix_speed_capability  %s\n", ctrl->pcix_speed_capability == 0 ? "not supported" : "supported");
	dbg ("    pcix_support           %s\n", ctrl->pcix_support == 0 ? "not supported" : "supported");

	ctrl->pci_dev = pdev;
	ctrl->pci_ops = pdev->bus->ops;
	ctrl->bus = pdev->bus->number;
	ctrl->device = PCI_SLOT(pdev->devfn);
	ctrl->function = PCI_FUNC(pdev->devfn);
	ctrl->rev = rev;
	dbg("bus device function rev: %d %d %d %d\n", ctrl->bus, ctrl->device, ctrl->function, ctrl->rev);

	init_MUTEX(&ctrl->crit_sect);
	init_waitqueue_head(&ctrl->queue);

	/* initialize our threads if they haven't already been started up */
	rc = one_time_init();
	if (rc) {
		goto err_free_ctrl;
	}
	
	dbg("pdev = %p\n", pdev);
	dbg("pci resource start %lx\n", pci_resource_start(pdev, 0));
	dbg("pci resource len %lx\n", pci_resource_len(pdev, 0));

	if (!request_mem_region(pci_resource_start(pdev, 0),
				pci_resource_len(pdev, 0), MY_NAME)) {
		err("cannot reserve MMIO region\n");
		rc = -ENOMEM;
		goto err_free_ctrl;
	}

	ctrl->hpc_reg = ioremap(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));
	if (!ctrl->hpc_reg) {
		err("cannot remap MMIO region %lx @ %lx\n", pci_resource_len(pdev, 0), pci_resource_start(pdev, 0));
		rc = -ENODEV;
		goto err_free_mem_region;
	}

	// Check for 66Mhz and/or PCI-X operation
	ctrl->speed = get_controller_speed(ctrl);
	
	//**************************************************
	//
	//              Save configuration headers for this and
	//              subordinate PCI buses
	//
	//**************************************************

	// find the physical slot number of the first hot plug slot

	// Get slot won't work for devices behind bridges, but
	// in this case it will always be called for the "base"
	// bus/dev/func of a slot.
	// CS: this is leveraging the PCIIRQ routing code from the kernel (pci-pc.c: get_irq_routing_table)
	rc = get_slot_mapping(ctrl->pci_ops, pdev->bus->number, (readb(ctrl->hpc_reg + SLOT_MASK) >> 4), &(ctrl->first_slot));
	dbg("get_slot_mapping: first_slot = %d, returned = %d\n", ctrl->first_slot, rc);
	if (rc) {
		err(msg_initialization_err, rc);
		goto err_iounmap;
	}

	// Store PCI Config Space for all devices on this bus
	rc = cpqhp_save_config(ctrl, ctrl->bus, readb(ctrl->hpc_reg + SLOT_MASK));
	if (rc) {
		err("%s: unable to save PCI configuration data, error %d\n", __FUNCTION__, rc);
		goto err_iounmap;
	}

	/*
	 * Get IO, memory, and IRQ resources for new devices
	 */
	// The next line is required for cpqhp_find_available_resources
	ctrl->interrupt = pdev->irq;

	rc = cpqhp_find_available_resources(ctrl, cpqhp_rom_start);
	ctrl->add_support = !rc;
	if (rc) {
		dbg("cpqhp_find_available_resources = 0x%x\n", rc);
		err("unable to locate PCI configuration resources for hot plug add.\n");
		goto err_iounmap;
	}

	/*
	 * Finish setting up the hot plug ctrl device
	 */
	ctrl->slot_device_offset = readb(ctrl->hpc_reg + SLOT_MASK) >> 4;
	dbg("NumSlots %d \n", ctrl->slot_device_offset);

	ctrl->next_event = 0;

	/* Setup the slot information structures */
	rc = ctrl_slot_setup(ctrl, smbios_start, smbios_table);
	if (rc) {
		err(msg_initialization_err, 6);
		err("%s: unable to save PCI configuration data, error %d\n", __FUNCTION__, rc);
		goto err_iounmap;
	}
	
	/* Mask all general input interrupts */
	writel(0xFFFFFFFFL, ctrl->hpc_reg + INT_MASK);

	/* set up the interrupt */
	dbg("HPC interrupt = %d \n", ctrl->interrupt);
	if (request_irq(ctrl->interrupt,
			(void (*)(int, void *, struct pt_regs *)) &cpqhp_ctrl_intr,
			SA_SHIRQ, MY_NAME, ctrl)) {
		err("Can't get irq %d for the hotplug pci controller\n", ctrl->interrupt);
		rc = -ENODEV;
		goto err_iounmap;
	}

	/* Enable Shift Out interrupt and clear it, also enable SERR on power fault */
	temp_word = readw(ctrl->hpc_reg + MISC);
	temp_word |= 0x4006;
	writew(temp_word, ctrl->hpc_reg + MISC);

	// Changed 05/05/97 to clear all interrupts at start
	writel(0xFFFFFFFFL, ctrl->hpc_reg + INT_INPUT_CLEAR);

	ctrl->ctrl_int_comp = readl(ctrl->hpc_reg + INT_INPUT_CLEAR);

	writel(0x0L, ctrl->hpc_reg + INT_MASK);

	if (!cpqhp_ctrl_list) {
		cpqhp_ctrl_list = ctrl;
		ctrl->next = NULL;
	} else {
		ctrl->next = cpqhp_ctrl_list;
		cpqhp_ctrl_list = ctrl;
	}

	// turn off empty slots here unless command line option "ON" set
	// Wait for exclusive access to hardware
	down(&ctrl->crit_sect);

	num_of_slots = readb(ctrl->hpc_reg + SLOT_MASK) & 0x0F;

	// find first device number for the ctrl
	device = readb(ctrl->hpc_reg + SLOT_MASK) >> 4;

	while (num_of_slots) {
		dbg("num_of_slots: %d\n", num_of_slots);
		func = cpqhp_slot_find(ctrl->bus, device, 0);
		if (!func)
			break;

		hp_slot = func->device - ctrl->slot_device_offset;
		dbg("hp_slot: %d\n", hp_slot);

		// We have to save the presence info for these slots
		temp_word = ctrl->ctrl_int_comp >> 16;
		func->presence_save = (temp_word >> hp_slot) & 0x01;
		func->presence_save |= (temp_word >> (hp_slot + 7)) & 0x02;

		if (ctrl->ctrl_int_comp & (0x1L << hp_slot)) {
			func->switch_save = 0;
		} else {
			func->switch_save = 0x10;
		}

		if (!power_mode) {
			if (!func->is_a_board) {
				green_LED_off (ctrl, hp_slot);
				slot_disable (ctrl, hp_slot);
			}
		}

		device++;
		num_of_slots--;
	}

	if (!power_mode) {
		set_SOGO(ctrl);
		// Wait for SOBS to be unset
		wait_for_ctrl_irq (ctrl);
	}

	rc = init_SERR(ctrl);
	if (rc) {
		err("init_SERR failed\n");
		up(&ctrl->crit_sect);
		goto err_free_irq;
	}

	// Done with exclusive hardware access
	up(&ctrl->crit_sect);

	rc = cpqhp_proc_create_ctrl (ctrl);
	if (rc) {
		err("cpqhp_proc_create_ctrl failed\n");
		goto err_free_irq;
	}

	return 0;

err_free_irq:
	free_irq(ctrl->interrupt, ctrl);
err_iounmap:
	iounmap(ctrl->hpc_reg);
err_free_mem_region:
	release_mem_region(pci_resource_start(pdev, 0), pci_resource_len(pdev, 0));
err_free_ctrl:
	kfree(ctrl);
	return rc;
}


static int one_time_init(void)
{
	int loop;
	int retval = 0;
	static int initialized = 0;

	if (initialized)
		return 0;

	power_mode = 0;

	retval = pci_print_IRQ_route();
	if (retval)
		goto error;

	dbg("Initialize + Start the notification mechanism \n");

	retval = cpqhp_event_start_thread();
	if (retval)
		goto error;

	dbg("Initialize slot lists\n");
	for (loop = 0; loop < 256; loop++) {
		cpqhp_slot_list[loop] = NULL;
	}

	// FIXME: We also need to hook the NMI handler eventually.
	// this also needs to be worked with Christoph
	// register_NMI_handler();

	// Map rom address
	cpqhp_rom_start = ioremap(ROM_PHY_ADDR, ROM_PHY_LEN);
	if (!cpqhp_rom_start) {
		err ("Could not ioremap memory region for ROM\n");
		retval = -EIO;;
		goto error;
	}
	
	/* Now, map the int15 entry point if we are on compaq specific hardware */
	compaq_nvram_init(cpqhp_rom_start);
	
	/* Map smbios table entry point structure */
	smbios_table = detect_SMBIOS_pointer(cpqhp_rom_start, cpqhp_rom_start + ROM_PHY_LEN);
	if (!smbios_table) {
		err ("Could not find the SMBIOS pointer in memory\n");
		retval = -EIO;;
		goto error;
	}

	smbios_start = ioremap(readl(smbios_table + ST_ADDRESS), readw(smbios_table + ST_LENGTH));
	if (!smbios_start) {
		err ("Could not ioremap memory region taken from SMBIOS values\n");
		retval = -EIO;;
		goto error;
	}

	retval = cpqhp_proc_init_ctrl();
	if (retval)
		goto error;

	initialized = 1;

	return retval;

error:
	if (cpqhp_rom_start)
		iounmap(cpqhp_rom_start);
	if (smbios_start)
		iounmap(smbios_start);
	
	return retval;
}


static void unload_cpqphpd(void)
{
	struct pci_func *next;
	struct pci_func *TempSlot;
	int loop;
	u32 rc;
	struct controller *ctrl;
	struct controller *tctrl;
	struct pci_resource *res;
	struct pci_resource *tres;

	rc = compaq_nvram_store(cpqhp_rom_start);

	ctrl = cpqhp_ctrl_list;

	while (ctrl) {
		cpqhp_proc_remove_ctrl (ctrl);

		if (ctrl->hpc_reg) {
			u16 misc;
			rc = read_slot_enable (ctrl);
			
			writeb(0, ctrl->hpc_reg + SLOT_SERR);
			writel(0xFFFFFFC0L | ~rc, ctrl->hpc_reg + INT_MASK);
			
			misc = readw(ctrl->hpc_reg + MISC);
			misc &= 0xFFFD;
			writew(misc, ctrl->hpc_reg + MISC);
		}

		ctrl_slot_cleanup(ctrl);

		res = ctrl->io_head;
		while (res) {
			tres = res;
			res = res->next;
			kfree(tres);
		}

		res = ctrl->mem_head;
		while (res) {
			tres = res;
			res = res->next;
			kfree(tres);
		}

		res = ctrl->p_mem_head;
		while (res) {
			tres = res;
			res = res->next;
			kfree(tres);
		}

		res = ctrl->bus_head;
		while (res) {
			tres = res;
			res = res->next;
			kfree(tres);
		}

		tctrl = ctrl;
		ctrl = ctrl->next;
		kfree(tctrl);
	}

	for (loop = 0; loop < 256; loop++) {
		next = cpqhp_slot_list[loop];
		while (next != NULL) {
			res = next->io_head;
			while (res) {
				tres = res;
				res = res->next;
				kfree(tres);
			}

			res = next->mem_head;
			while (res) {
				tres = res;
				res = res->next;
				kfree(tres);
			}

			res = next->p_mem_head;
			while (res) {
				tres = res;
				res = res->next;
				kfree(tres);
			}

			res = next->bus_head;
			while (res) {
				tres = res;
				res = res->next;
				kfree(tres);
			}

			TempSlot = next;
			next = next->next;
			kfree(TempSlot);
		}
	}

	remove_proc_entry("hpc", 0);

	// Stop the notification mechanism
	cpqhp_event_stop_thread();

	//unmap the rom address
	if (cpqhp_rom_start)
		iounmap(cpqhp_rom_start);
	if (smbios_start)
		iounmap(smbios_start);
}



static struct pci_device_id hpcd_pci_tbl[] __devinitdata = {
	{
	/* handle any PCI Hotplug controller */
	class:          ((PCI_CLASS_SYSTEM_PCI_HOTPLUG << 8) | 0x00),
	class_mask:     ~0,
	
	/* no matter who makes it */
	vendor:         PCI_ANY_ID,
	device:         PCI_ANY_ID,
	subvendor:      PCI_ANY_ID,
	subdevice:      PCI_ANY_ID,
	
	}, { /* end: all zeroes */ }
};

MODULE_DEVICE_TABLE(pci, hpcd_pci_tbl);



static struct pci_driver cpqhpc_driver = {
	name:		"pci_hotplug",
	id_table:	hpcd_pci_tbl,
	probe:		cpqhpc_probe,
	/* remove:	cpqhpc_remove_one, */
};



static int __init cpqhpc_init(void)
{
	int result;

	cpqhp_debug = debug;

	result = pci_module_init(&cpqhpc_driver);
	dbg("pci_module_init = %d\n", result);
	if (result)
		return result;
	info (DRIVER_DESC " version: " DRIVER_VERSION "\n");
	return 0;
}


static void __exit cpqhpc_cleanup(void)
{
	dbg("cleaning up proc entries\n");
	cpqhp_proc_destroy_ctrl();

	dbg("unload_cpqphpd()\n");
	unload_cpqphpd();

	dbg("pci_unregister_driver\n");
	pci_unregister_driver(&cpqhpc_driver);
}


module_init(cpqhpc_init);
module_exit(cpqhpc_cleanup);


