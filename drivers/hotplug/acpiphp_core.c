/*
 * ACPI PCI Hot Plug Controller Driver
 *
 * Copyright (C) 1995,2001 Compaq Computer Corporation
 * Copyright (C) 2001 Greg Kroah-Hartman (greg@kroah.com)
 * Copyright (C) 2001 IBM Corp.
 * Copyright (C) 2002 Hiroshi Aono (h-aono@ap.jp.nec.com)
 * Copyright (C) 2002,2003 Takayoshi Kochi (t-kochi@bq.jp.nec.com)
 * Copyright (C) 2002,2003 NEC Corporation
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
 * Send feedback to <gregkh@us.ibm.com>,
 *		    <t-kochi@bq.jp.nec.com>
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include "pci_hotplug.h"
#include "acpiphp.h"

static LIST_HEAD(slot_list);

#if !defined(CONFIG_HOTPLUG_PCI_ACPI_MODULE)
	#define MY_NAME	"acpiphp"
#else
	#define MY_NAME	THIS_MODULE->name
#endif

static int debug;
int acpiphp_debug;

/* local variables */
static int num_slots;

#define DRIVER_VERSION	"0.4"
#define DRIVER_AUTHOR	"Greg Kroah-Hartman <gregkh@us.ibm.com>, Takayoshi Kochi <t-kochi@bq.jp.nec.com>"
#define DRIVER_DESC	"ACPI Hot Plug PCI Controller Driver"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debugging mode enabled or not");

static int enable_slot		(struct hotplug_slot *slot);
static int disable_slot		(struct hotplug_slot *slot);
static int set_attention_status (struct hotplug_slot *slot, u8 value);
static int hardware_test	(struct hotplug_slot *slot, u32 value);
static int get_power_status	(struct hotplug_slot *slot, u8 *value);
static int get_attention_status	(struct hotplug_slot *slot, u8 *value);
static int get_latch_status	(struct hotplug_slot *slot, u8 *value);
static int get_adapter_status	(struct hotplug_slot *slot, u8 *value);
static int get_max_bus_speed	(struct hotplug_slot *hotplug_slot, enum pci_bus_speed *value);
static int get_cur_bus_speed	(struct hotplug_slot *hotplug_slot, enum pci_bus_speed *value);

static struct hotplug_slot_ops acpi_hotplug_slot_ops = {
	.owner			= THIS_MODULE,
	.enable_slot		= enable_slot,
	.disable_slot		= disable_slot,
	.set_attention_status	= set_attention_status,
	.hardware_test		= hardware_test,
	.get_power_status	= get_power_status,
	.get_attention_status	= get_attention_status,
	.get_latch_status	= get_latch_status,
	.get_adapter_status	= get_adapter_status,
	.get_max_bus_speed	= get_max_bus_speed,
	.get_cur_bus_speed	= get_cur_bus_speed,
};


/* Inline functions to check the sanity of a pointer that is passed to us */
static inline int slot_paranoia_check (struct slot *slot, const char *function)
{
	if (!slot) {
		dbg("%s - slot == NULL\n", function);
		return -1;
	}
	if (slot->magic != SLOT_MAGIC) {
		dbg("%s - bad magic number for slot\n", function);
		return -1;
	}
	if (!slot->hotplug_slot) {
		dbg("%s - slot->hotplug_slot == NULL!\n", function);
		return -1;
	}
	return 0;
}


static inline struct slot *get_slot (struct hotplug_slot *hotplug_slot, const char *function)
{
	struct slot *slot;

	if (!hotplug_slot) {
		dbg("%s - hotplug_slot == NULL\n", function);
		return NULL;
	}

	slot = (struct slot *)hotplug_slot->private;
	if (slot_paranoia_check(slot, function))
                return NULL;
	return slot;
}


/**
 * enable_slot - power on and enable a slot
 * @hotplug_slot: slot to enable
 *
 * Actual tasks are done in acpiphp_enable_slot()
 *
 */
static int enable_slot (struct hotplug_slot *hotplug_slot)
{
	struct slot *slot = get_slot(hotplug_slot, __FUNCTION__);
	int retval = 0;

	if (slot == NULL)
		return -ENODEV;

	dbg("%s - physical_slot = %s\n", __FUNCTION__, hotplug_slot->name);

	/* enable the specified slot */
	retval = acpiphp_enable_slot(slot->acpi_slot);

	return retval;
}


/**
 * disable_slot - disable and power off a slot
 * @hotplug_slot: slot to disable
 *
 * Actual tasks are done in acpiphp_disable_slot()
 *
 */
static int disable_slot (struct hotplug_slot *hotplug_slot)
{
	struct slot *slot = get_slot(hotplug_slot, __FUNCTION__);
	int retval = 0;

	if (slot == NULL)
		return -ENODEV;

	dbg("%s - physical_slot = %s\n", __FUNCTION__, hotplug_slot->name);

	/* disable the specified slot */
	retval = acpiphp_disable_slot(slot->acpi_slot);

	return retval;
}


/**
 * set_attention_status - set attention LED
 *
 * TBD:
 * ACPI doesn't have known method to manipulate
 * attention status LED.
 *
 */
static int set_attention_status (struct hotplug_slot *hotplug_slot, u8 status)
{
	int retval = 0;

	dbg("%s - physical_slot = %s\n", __FUNCTION__, hotplug_slot->name);

	switch (status) {
		case 0:
			/* FIXME turn light off */
			hotplug_slot->info->attention_status = 0;
			break;

		case 1:
		default:
			/* FIXME turn light on */
			hotplug_slot->info->attention_status = 1;
			break;
	}

	return retval;
}


/**
 * hardware_test - hardware test
 *
 * We have nothing to do for now...
 *
 */
static int hardware_test (struct hotplug_slot *hotplug_slot, u32 value)
{
	struct slot *slot = get_slot(hotplug_slot, __FUNCTION__);
	int retval = 0;

	if (slot == NULL)
		return -ENODEV;

	dbg("%s - physical_slot = %s\n", __FUNCTION__, hotplug_slot->name);

	err("No hardware tests are defined for this driver\n");
	retval = -ENODEV;

	return retval;
}


/**
 * get_power_status - get power status of a slot
 * @hotplug_slot: slot to get status
 * @value: pointer to store status
 *
 * Some platforms may not implement _STA method properly.
 * In that case, the value returned may not be reliable.
 *
 */
static int get_power_status (struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct slot *slot = get_slot(hotplug_slot, __FUNCTION__);
	int retval = 0;

	if (slot == NULL)
		return -ENODEV;

	dbg("%s - physical_slot = %s\n", __FUNCTION__, hotplug_slot->name);

	*value = acpiphp_get_power_status(slot->acpi_slot);

	return retval;
}


/**
 * get_attention_status - get attention LED status
 *
 * TBD:
 * ACPI doesn't provide any formal means to access attention LED status.
 *
 */
static int get_attention_status (struct hotplug_slot *hotplug_slot, u8 *value)
{
	int retval = 0;

	dbg("%s - physical_slot = %s\n", __FUNCTION__, hotplug_slot->name);

	*value = hotplug_slot->info->attention_status;

	return retval;
}


/**
 * get_latch_status - get latch status of a slot
 * @hotplug_slot: slot to get status
 * @value: pointer to store status
 *
 * ACPI doesn't provide any formal means to access latch status.
 * Instead, we fake latch status from _STA
 *
 */
static int get_latch_status (struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct slot *slot = get_slot(hotplug_slot, __FUNCTION__);
	int retval = 0;

	if (slot == NULL)
		return -ENODEV;

	dbg("%s - physical_slot = %s\n", __FUNCTION__, hotplug_slot->name);

	*value = acpiphp_get_latch_status(slot->acpi_slot);

	return retval;
}


/**
 * get_adapter_status - get adapter status of a slot
 * @hotplug_slot: slot to get status
 * @value: pointer to store status
 *
 * ACPI doesn't provide any formal means to access adapter status.
 * Instead, we fake adapter status from _STA
 *
 */
static int get_adapter_status (struct hotplug_slot *hotplug_slot, u8 *value)
{
	struct slot *slot = get_slot(hotplug_slot, __FUNCTION__);
	int retval = 0;

	if (slot == NULL)
		return -ENODEV;

	dbg("%s - physical_slot = %s\n", __FUNCTION__, hotplug_slot->name);

	*value = acpiphp_get_adapter_status(slot->acpi_slot);

	return retval;
}


/* return dummy value because ACPI doesn't provide any method... */
static int get_max_bus_speed (struct hotplug_slot *hotplug_slot, enum pci_bus_speed *value)
{
	struct slot *slot = get_slot(hotplug_slot, __FUNCTION__);

	if (slot == NULL)
		return -ENODEV;

	*value = PCI_SPEED_UNKNOWN;

	return 0;
}


/* return dummy value because ACPI doesn't provide any method... */
static int get_cur_bus_speed (struct hotplug_slot *hotplug_slot, enum pci_bus_speed *value)
{
	struct slot *slot = get_slot(hotplug_slot, __FUNCTION__);

	if (slot == NULL)
		return -ENODEV;

	*value = PCI_SPEED_UNKNOWN;

	return 0;
}


static int init_acpi (void)
{
	int retval;

	/* initialize internal data structure etc. */
	retval = acpiphp_glue_init();

	/* read initial number of slots */
	if (!retval) {
		num_slots = acpiphp_get_num_slots();
		if (num_slots == 0)
			retval = -ENODEV;
	}

	return retval;
}


/**
 * make_slot_name - make a slot name that appears in pcihpfs
 * @slot: slot to name
 *
 */
static void make_slot_name (struct slot *slot)
{
	snprintf(slot->hotplug_slot->name, SLOT_NAME_SIZE, "%u",
		 slot->acpi_slot->sun);
}

/**
 * init_slots - initialize 'struct slot' structures for each slot
 *
 */
static int init_slots (void)
{
	struct slot *slot;
	int retval = 0;
	int i;

	for (i = 0; i < num_slots; ++i) {
		slot = kmalloc(sizeof(struct slot), GFP_KERNEL);
		if (!slot)
			return -ENOMEM;
		memset(slot, 0, sizeof(struct slot));

		slot->hotplug_slot = kmalloc(sizeof(struct hotplug_slot), GFP_KERNEL);
		if (!slot->hotplug_slot) {
			kfree(slot);
			return -ENOMEM;
		}
		memset(slot->hotplug_slot, 0, sizeof(struct hotplug_slot));

		slot->hotplug_slot->info = kmalloc(sizeof(struct hotplug_slot_info), GFP_KERNEL);
		if (!slot->hotplug_slot->info) {
			kfree(slot->hotplug_slot);
			kfree(slot);
			return -ENOMEM;
		}
		memset(slot->hotplug_slot->info, 0, sizeof(struct hotplug_slot_info));

		slot->hotplug_slot->name = kmalloc(SLOT_NAME_SIZE, GFP_KERNEL);
		if (!slot->hotplug_slot->name) {
			kfree(slot->hotplug_slot->info);
			kfree(slot->hotplug_slot);
			kfree(slot);
			return -ENOMEM;
		}

		slot->magic = SLOT_MAGIC;
		slot->number = i;

		slot->hotplug_slot->private = slot;
		slot->hotplug_slot->ops = &acpi_hotplug_slot_ops;

		slot->acpi_slot = get_slot_from_id(i);
		slot->hotplug_slot->info->power_status = acpiphp_get_power_status(slot->acpi_slot);
		slot->hotplug_slot->info->attention_status = acpiphp_get_attention_status(slot->acpi_slot);
		slot->hotplug_slot->info->latch_status = acpiphp_get_latch_status(slot->acpi_slot);
		slot->hotplug_slot->info->adapter_status = acpiphp_get_adapter_status(slot->acpi_slot);

		make_slot_name(slot);

		retval = pci_hp_register(slot->hotplug_slot);
		if (retval) {
			err("pci_hp_register failed with error %d\n", retval);
			kfree(slot->hotplug_slot->info);
			kfree(slot->hotplug_slot->name);
			kfree(slot->hotplug_slot);
			kfree(slot);
			return retval;
		}

		/* add slot to our internal list */
		list_add(&slot->slot_list, &slot_list);
		info("Slot [%s] registered\n", slot->hotplug_slot->name);
	}

	return retval;
}


static void cleanup_slots (void)
{
	struct list_head *tmp, *n;
	struct slot *slot;

	list_for_each_safe (tmp, n, &slot_list) {
		slot = list_entry(tmp, struct slot, slot_list);
		list_del(&slot->slot_list);
		pci_hp_deregister(slot->hotplug_slot);
		kfree(slot->hotplug_slot->info);
		kfree(slot->hotplug_slot->name);
		kfree(slot->hotplug_slot);
		kfree(slot);
	}

	return;
}


static int __init acpiphp_init(void)
{
	int retval;

	info(DRIVER_DESC " version: " DRIVER_VERSION "\n");

	acpiphp_debug = debug;

	/* read all the ACPI info from the system */
	retval = init_acpi();
	if (retval)
		return retval;

	retval = init_slots();
	if (retval)
		return retval;

	return 0;
}


static void __exit acpiphp_exit(void)
{
	cleanup_slots();
	/* deallocate internal data structures etc. */
	acpiphp_glue_exit();
}

module_init(acpiphp_init);
module_exit(acpiphp_exit);
