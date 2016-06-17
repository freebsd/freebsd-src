/*
 *  acpi_ac.c - ACPI AC Adapter Driver ($Revision: 26 $)
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
#include <linux/compatmac.h>
#include <linux/proc_fs.h>
#include <acpi/acpi_bus.h>
#include <acpi/acpi_drivers.h>


#define _COMPONENT		ACPI_AC_COMPONENT
ACPI_MODULE_NAME		("acpi_ac")

MODULE_AUTHOR("Paul Diefenbaugh");
MODULE_DESCRIPTION(ACPI_AC_DRIVER_NAME);
MODULE_LICENSE("GPL");

#define PREFIX			"ACPI: "


int acpi_ac_add (struct acpi_device *device);
int acpi_ac_remove (struct acpi_device *device, int type);

static struct acpi_driver acpi_ac_driver = {
	.name =		ACPI_AC_DRIVER_NAME,
	.class =	ACPI_AC_CLASS,
	.ids =		ACPI_AC_HID,
	.ops =		{
				.add =		acpi_ac_add,
				.remove =	acpi_ac_remove,
			},
};

struct acpi_ac {
	acpi_handle		handle;
	unsigned long		state;
};


/* --------------------------------------------------------------------------
                               AC Adapter Management
   -------------------------------------------------------------------------- */

static int
acpi_ac_get_state (
	struct acpi_ac		*ac)
{
	acpi_status		status = AE_OK;

	ACPI_FUNCTION_TRACE("acpi_ac_get_state");

	if (!ac)
		return_VALUE(-EINVAL);

	status = acpi_evaluate_integer(ac->handle, "_PSR", NULL, &ac->state);
	if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Error reading AC Adapter state\n"));
		ac->state = ACPI_AC_STATUS_UNKNOWN;
		return_VALUE(-ENODEV);
	}
	
	return_VALUE(0);
}


/* --------------------------------------------------------------------------
                              FS Interface (/proc)
   -------------------------------------------------------------------------- */

struct proc_dir_entry		*acpi_ac_dir;

static int
acpi_ac_read_state (
	char			*page,
	char			**start,
	off_t			off,
	int 			count,
	int 			*eof,
	void			*data)
{
	struct acpi_ac		*ac = (struct acpi_ac *) data;
	char			*p = page;
	int			len = 0;

	ACPI_FUNCTION_TRACE("acpi_ac_read_state");

	if (!ac || (off != 0))
		goto end;

	if (acpi_ac_get_state(ac)) {
		p += sprintf(p, "ERROR: Unable to read AC Adapter state\n");
		goto end;
	}

	p += sprintf(p, "state:                   ");
	switch (ac->state) {
	case ACPI_AC_STATUS_OFFLINE:
		p += sprintf(p, "off-line\n");
		break;
	case ACPI_AC_STATUS_ONLINE:
		p += sprintf(p, "on-line\n");
		break;
	default:
		p += sprintf(p, "unknown\n");
		break;
	}

end:
	len = (p - page);
	if (len <= off+count) *eof = 1;
	*start = page + off;
	len -= off;
	if (len>count) len = count;
	if (len<0) len = 0;

	return_VALUE(len);
}


static int
acpi_ac_add_fs (
	struct acpi_device	*device)
{
	struct proc_dir_entry	*entry = NULL;

	ACPI_FUNCTION_TRACE("acpi_ac_add_fs");

	if (!acpi_device_dir(device)) {
		acpi_device_dir(device) = proc_mkdir(acpi_device_bid(device),
			acpi_ac_dir);
		if (!acpi_device_dir(device))
			return_VALUE(-ENODEV);
	}

	/* 'state' [R] */
	entry = create_proc_entry(ACPI_AC_FILE_STATE,
		S_IRUGO, acpi_device_dir(device));
	if (!entry)
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Unable to create '%s' fs entry\n",
			ACPI_AC_FILE_STATE));
	else {
		entry->read_proc = acpi_ac_read_state;
		entry->data = acpi_driver_data(device);
	}

	return_VALUE(0);
}


static int
acpi_ac_remove_fs (
	struct acpi_device	*device)
{
	ACPI_FUNCTION_TRACE("acpi_ac_remove_fs");

	if (acpi_device_dir(device)) {
		remove_proc_entry(acpi_device_bid(device), acpi_ac_dir);
		acpi_device_dir(device) = NULL;
	}

	return_VALUE(0);
}


/* --------------------------------------------------------------------------
                                   Driver Model
   -------------------------------------------------------------------------- */

void
acpi_ac_notify (
	acpi_handle		handle,
	u32			event,
	void			*data)
{
	struct acpi_ac		*ac = (struct acpi_ac *) data;
	struct acpi_device	*device = NULL;

	ACPI_FUNCTION_TRACE("acpi_ac_notify");

	if (!ac)
		return;

	if (acpi_bus_get_device(ac->handle, &device))
		return_VOID;

	switch (event) {
	case ACPI_AC_NOTIFY_STATUS:
		acpi_ac_get_state(ac);
		acpi_bus_generate_event(device, event, (u32) ac->state);
		break;
	default:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			"Unsupported event [0x%x]\n", event));
		break;
	}

	return_VOID;
}


int
acpi_ac_add (
	struct acpi_device	*device)
{
	int			result = 0;
	acpi_status		status = AE_OK;
	struct acpi_ac		*ac = NULL;

	ACPI_FUNCTION_TRACE("acpi_ac_add");

	if (!device)
		return_VALUE(-EINVAL);

	ac = kmalloc(sizeof(struct acpi_ac), GFP_KERNEL);
	if (!ac)
		return_VALUE(-ENOMEM);
	memset(ac, 0, sizeof(struct acpi_ac));

	ac->handle = device->handle;
	sprintf(acpi_device_name(device), "%s", ACPI_AC_DEVICE_NAME);
	sprintf(acpi_device_class(device), "%s", ACPI_AC_CLASS);
	acpi_driver_data(device) = ac;

	result = acpi_ac_get_state(ac);
	if (result)
		goto end;

	result = acpi_ac_add_fs(device);
	if (result)
		goto end;

	status = acpi_install_notify_handler(ac->handle,
		ACPI_DEVICE_NOTIFY, acpi_ac_notify, ac);
	if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Error installing notify handler\n"));
		result = -ENODEV;
		goto end;
	}

	printk(KERN_INFO PREFIX "%s [%s] (%s)\n", 
		acpi_device_name(device), acpi_device_bid(device), 
		ac->state?"on-line":"off-line");

end:
	if (result) {
		acpi_ac_remove_fs(device);
		kfree(ac);
	}

	return_VALUE(result);
}


int
acpi_ac_remove (
	struct acpi_device	*device,
	int			type)
{
	acpi_status		status = AE_OK;
	struct acpi_ac		*ac = NULL;

	ACPI_FUNCTION_TRACE("acpi_ac_remove");

	if (!device || !acpi_driver_data(device))
		return_VALUE(-EINVAL);

	ac = (struct acpi_ac *) acpi_driver_data(device);

	status = acpi_remove_notify_handler(ac->handle,
		ACPI_DEVICE_NOTIFY, acpi_ac_notify);
	if (ACPI_FAILURE(status))
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Error removing notify handler\n"));

	acpi_ac_remove_fs(device);

	kfree(ac);

	return_VALUE(0);
}


int __init
acpi_ac_init (void)
{
	int			result = 0;

	ACPI_FUNCTION_TRACE("acpi_ac_init");

	acpi_ac_dir = proc_mkdir(ACPI_AC_CLASS, acpi_root_dir);
	if (!acpi_ac_dir)
		return_VALUE(-ENODEV);

	result = acpi_bus_register_driver(&acpi_ac_driver);
	if (result < 0) {
		remove_proc_entry(ACPI_AC_CLASS, acpi_root_dir);
		return_VALUE(-ENODEV);
	}

	return_VALUE(0);
}


void __exit
acpi_ac_exit (void)
{
	ACPI_FUNCTION_TRACE("acpi_ac_exit");

	acpi_bus_unregister_driver(&acpi_ac_driver);

	remove_proc_entry(ACPI_AC_CLASS, acpi_root_dir);

	return_VOID;
}


module_init(acpi_ac_init);
module_exit(acpi_ac_exit);
