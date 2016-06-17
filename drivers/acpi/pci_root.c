/*
 *  pci_root.c - ACPI PCI Root Bridge Driver ($Revision: 41 $)
 *
 *  Copyright (C) 2001, 2002 Andy Grover <andrew.grover@intel.com>
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/pm.h>
#include <linux/pci.h>
#include <linux/acpi.h>
#include <acpi/acpi_bus.h>
#include <acpi/acpi_drivers.h>


#define _COMPONENT		ACPI_PCI_COMPONENT
ACPI_MODULE_NAME		("pci_root")

extern struct pci_ops *pci_root_ops;

#define PREFIX			"ACPI: "

static int acpi_pci_root_add (struct acpi_device *device);
static int acpi_pci_root_remove (struct acpi_device *device, int type);

static struct acpi_driver acpi_pci_root_driver = {
	.name =		ACPI_PCI_ROOT_DRIVER_NAME,
	.class =	ACPI_PCI_ROOT_CLASS,
	.ids =		ACPI_PCI_ROOT_HID,
	.ops =		{
				.add =    acpi_pci_root_add,
				.remove = acpi_pci_root_remove,
			},
};

struct acpi_pci_root {
	struct list_head	node;
	acpi_handle		handle;
	struct acpi_pci_id	id;
	struct pci_bus		*bus;
};

static LIST_HEAD(acpi_pci_roots);

static struct acpi_pci_driver *sub_driver;

int acpi_pci_register_driver(struct acpi_pci_driver *driver)
{
	int n = 0;
	struct list_head *entry;

	struct acpi_pci_driver **pptr = &sub_driver;
	while (*pptr)
		pptr = &(*pptr)->next;
	*pptr = driver;

	if (!driver->add)
		return 0;

	list_for_each(entry, &acpi_pci_roots) {
		struct acpi_pci_root *root;
		root = list_entry(entry, struct acpi_pci_root, node);
		driver->add(root->handle);
		n++;
	}

	return n;
}

void acpi_pci_unregister_driver(struct acpi_pci_driver *driver)
{
	struct list_head *entry;

	struct acpi_pci_driver **pptr = &sub_driver;
	while (*pptr) {
		if (*pptr != driver)
			continue;
		*pptr = (*pptr)->next;
		break;
	}

	if (!driver->remove)
		return;

	list_for_each(entry, &acpi_pci_roots) {
		struct acpi_pci_root *root;
		root = list_entry(entry, struct acpi_pci_root, node);
		driver->remove(root->handle);
	}
}

static int
acpi_pci_root_add (
	struct acpi_device	*device)
{
	int			result = 0;
	struct acpi_pci_root	*root = NULL;
	acpi_status		status = AE_OK;
	unsigned long		value = 0;
	acpi_handle		handle = NULL;

	ACPI_FUNCTION_TRACE("acpi_pci_root_add");

	if (!device)
		return_VALUE(-EINVAL);

	root = kmalloc(sizeof(struct acpi_pci_root), GFP_KERNEL);
	if (!root)
		return_VALUE(-ENOMEM);
	memset(root, 0, sizeof(struct acpi_pci_root));

	root->handle = device->handle;
	sprintf(acpi_device_name(device), "%s", ACPI_PCI_ROOT_DEVICE_NAME);
	sprintf(acpi_device_class(device), "%s", ACPI_PCI_ROOT_CLASS);
	acpi_driver_data(device) = root;

	/*
	 * TBD: Doesn't the bus driver automatically set this?
	 */
	device->ops.bind = acpi_pci_bind;

	/* 
	 * Segment
	 * -------
	 * Obtained via _SEG, if exists, otherwise assumed to be zero (0).
	 */
	status = acpi_evaluate_integer(root->handle, METHOD_NAME__SEG, NULL, 
		&value);
	switch (status) {
	case AE_OK:
		root->id.segment = (u16) value;
		printk("_SEG exists! Unsupported. Abort.\n");
		BUG();
		break;
	case AE_NOT_FOUND:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, 
			"Assuming segment 0 (no _SEG)\n"));
		root->id.segment = 0;
		break;
	default:
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "Error evaluating _SEG\n"));
		result = -ENODEV;
		goto end;
	}

	/* 
	 * Bus
	 * ---
	 * Obtained via _BBN, if exists, otherwise assumed to be zero (0).
	 */
	status = acpi_evaluate_integer(root->handle, METHOD_NAME__BBN, NULL, 
		&value);
	switch (status) {
	case AE_OK:
		root->id.bus = (u16) value;
		break;
	case AE_NOT_FOUND:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Assuming bus 0 (no _BBN)\n"));
		root->id.bus = 0;
		break;
	default:
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "Error evaluating _BBN\n"));
		result = -ENODEV;
		goto end;
	}

	/*
	 * Device & Function
	 * -----------------
	 * Obtained from _ADR (which has already been evaluated for us).
	 */
	root->id.device = device->pnp.bus_address >> 16;
	root->id.function = device->pnp.bus_address & 0xFFFF;

	/*
	 * TBD: Need PCI interface for enumeration/configuration of roots.
	 */

 	/* TBD: Locking */
 	list_add_tail(&root->node, &acpi_pci_roots);

	printk(KERN_INFO PREFIX "%s [%s] (%02x:%02x)\n", 
		acpi_device_name(device), acpi_device_bid(device),
		root->id.segment, root->id.bus);

	/*
	 * Scan the Root Bridge
	 * --------------------
	 * Must do this prior to any attempt to bind the root device, as the
	 * PCI namespace does not get created until this call is made (and 
	 * thus the root bridge's pci_dev does not exist).
	 */
	root->bus = pcibios_scan_root(root->id.bus);
	if (!root->bus) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, 
			"Bus %02x:%02x not present in PCI namespace\n", 
			root->id.segment, root->id.bus));
		result = -ENODEV;
		goto end;
	}

	/*
	 * Attach ACPI-PCI Context
	 * -----------------------
	 * Thus binding the ACPI and PCI devices.
	 */
	result = acpi_pci_bind_root(device, &root->id, root->bus);
	if (result)
		goto end;

	/*
	 * PCI Routing Table
	 * -----------------
	 * Evaluate and parse _PRT, if exists.
	 */
	status = acpi_get_handle(root->handle, METHOD_NAME__PRT, &handle);
	if (ACPI_SUCCESS(status))
		result = acpi_pci_irq_add_prt(root->handle, root->id.segment,
			root->id.bus);

end:
	if (result)
		kfree(root);

	return_VALUE(result);
}


static int
acpi_pci_root_remove (
	struct acpi_device	*device,
	int			type)
{
	struct acpi_pci_root	*root = NULL;

	ACPI_FUNCTION_TRACE("acpi_pci_root_remove");

	if (!device || !acpi_driver_data(device))
		return_VALUE(-EINVAL);

	root = (struct acpi_pci_root *) acpi_driver_data(device);

	kfree(root);

	return_VALUE(0);
}


int __init
acpi_pci_root_init (void)
{
	ACPI_FUNCTION_TRACE("acpi_pci_root_init");

	/* DEBUG:
	acpi_dbg_layer = ACPI_PCI_COMPONENT;
	acpi_dbg_level = 0xFFFFFFFF;
	 */

	if (acpi_bus_register_driver(&acpi_pci_root_driver) < 0)
		return_VALUE(-ENODEV);

	return_VALUE(0);
}


void __exit
acpi_pci_root_exit (void)
{
	ACPI_FUNCTION_TRACE("acpi_pci_root_exit");

	acpi_bus_unregister_driver(&acpi_pci_root_driver);

	return_VOID;
}
