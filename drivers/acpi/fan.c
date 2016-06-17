/*
 *  acpi_fan.c - ACPI Fan Driver ($Revision: 28 $)
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


#define _COMPONENT		ACPI_FAN_COMPONENT
ACPI_MODULE_NAME		("acpi_fan")

MODULE_AUTHOR("Paul Diefenbaugh");
MODULE_DESCRIPTION(ACPI_FAN_DRIVER_NAME);
MODULE_LICENSE("GPL");

#define PREFIX			"ACPI: "


int acpi_fan_add (struct acpi_device *device);
int acpi_fan_remove (struct acpi_device *device, int type);

static struct acpi_driver acpi_fan_driver = {
	.name =		ACPI_FAN_DRIVER_NAME,
	.class =	ACPI_FAN_CLASS,
	.ids =		ACPI_FAN_HID,
	.ops =		{
				.add =		acpi_fan_add,
				.remove =	acpi_fan_remove,
			},
};

struct acpi_fan {
	acpi_handle		handle;
};


/* --------------------------------------------------------------------------
                              FS Interface (/proc)
   -------------------------------------------------------------------------- */

struct proc_dir_entry		*acpi_fan_dir;


static int
acpi_fan_read_state (
	char			*page,
	char			**start,
	off_t			off,
	int 			count,
	int 			*eof,
	void			*data)
{
	struct acpi_fan		*fan = (struct acpi_fan *) data;
	char			*p = page;
	int			len = 0;
	int			state = 0;

	ACPI_FUNCTION_TRACE("acpi_fan_read_state");

	if (!fan || (off != 0))
		goto end;

	if (acpi_bus_get_power(fan->handle, &state))
		goto end;

	p += sprintf(p, "status:                  %s\n",
		!state?"on":"off");

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
acpi_fan_write_state (
	struct file		*file,
	const char		*buffer,
	unsigned long		count,
	void			*data)
{
	int			result = 0;
	struct acpi_fan		*fan = (struct acpi_fan *) data;
	char			state_string[12] = {'\0'};

	ACPI_FUNCTION_TRACE("acpi_fan_write_state");

	if (!fan || (count > sizeof(state_string) - 1))
		return_VALUE(-EINVAL);
	
	if (copy_from_user(state_string, buffer, count))
		return_VALUE(-EFAULT);
	
	state_string[count] = '\0';
	
	result = acpi_bus_set_power(fan->handle, 
		simple_strtoul(state_string, NULL, 0));
	if (result)
		return_VALUE(result);

	return_VALUE(count);
}


static int
acpi_fan_add_fs (
	struct acpi_device	*device)
{
	struct proc_dir_entry	*entry = NULL;

	ACPI_FUNCTION_TRACE("acpi_fan_add_fs");

	if (!device)
		return_VALUE(-EINVAL);

	if (!acpi_device_dir(device)) {
		acpi_device_dir(device) = proc_mkdir(acpi_device_bid(device),
			acpi_fan_dir);
		if (!acpi_device_dir(device))
			return_VALUE(-ENODEV);
	}

	/* 'status' [R/W] */
	entry = create_proc_entry(ACPI_FAN_FILE_STATE,
		S_IFREG|S_IRUGO|S_IWUSR, acpi_device_dir(device));
	if (!entry)
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Unable to create '%s' fs entry\n",
			ACPI_FAN_FILE_STATE));
	else {
		entry->read_proc = acpi_fan_read_state;
		entry->write_proc = acpi_fan_write_state;
		entry->data = acpi_driver_data(device);
	}

	return_VALUE(0);
}


static int
acpi_fan_remove_fs (
	struct acpi_device	*device)
{
	ACPI_FUNCTION_TRACE("acpi_fan_remove_fs");

	if (acpi_device_dir(device)) {
		remove_proc_entry(acpi_device_bid(device), acpi_fan_dir);
		acpi_device_dir(device) = NULL;
	}

	return_VALUE(0);
}


/* --------------------------------------------------------------------------
                                 Driver Interface
   -------------------------------------------------------------------------- */

int
acpi_fan_add (
	struct acpi_device	*device)
{
	int			result = 0;
	struct acpi_fan		*fan = NULL;
	int			state = 0;

	ACPI_FUNCTION_TRACE("acpi_fan_add");

	if (!device)
		return_VALUE(-EINVAL);

	fan = kmalloc(sizeof(struct acpi_fan), GFP_KERNEL);
	if (!fan)
		return_VALUE(-ENOMEM);
	memset(fan, 0, sizeof(struct acpi_fan));

	fan->handle = device->handle;
	sprintf(acpi_device_name(device), "%s", ACPI_FAN_DEVICE_NAME);
	sprintf(acpi_device_class(device), "%s", ACPI_FAN_CLASS);
	acpi_driver_data(device) = fan;

	result = acpi_bus_get_power(fan->handle, &state);
	if (result) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Error reading power state\n"));
		goto end;
	}

	result = acpi_fan_add_fs(device);
	if (result)
		goto end;

	printk(KERN_INFO PREFIX "%s [%s] (%s)\n",
		acpi_device_name(device), acpi_device_bid(device),
		!device->power.state?"on":"off");

end:
	if (result)
		kfree(fan);

	return_VALUE(result);
}


int
acpi_fan_remove (
	struct acpi_device	*device,
	int			type)
{
	struct acpi_fan		*fan = NULL;

	ACPI_FUNCTION_TRACE("acpi_fan_remove");

	if (!device || !acpi_driver_data(device))
		return_VALUE(-EINVAL);

	fan = (struct acpi_fan *) acpi_driver_data(device);

	acpi_fan_remove_fs(device);

	kfree(fan);

	return_VALUE(0);
}


int __init
acpi_fan_init (void)
{
	int			result = 0;

	ACPI_FUNCTION_TRACE("acpi_fan_init");

	acpi_fan_dir = proc_mkdir(ACPI_FAN_CLASS, acpi_root_dir);
	if (!acpi_fan_dir)
		return_VALUE(-ENODEV);

	result = acpi_bus_register_driver(&acpi_fan_driver);
	if (result < 0) {
		remove_proc_entry(ACPI_FAN_CLASS, acpi_root_dir);
		return_VALUE(-ENODEV);
	}

	return_VALUE(0);
}


void __exit
acpi_fan_exit (void)
{
	ACPI_FUNCTION_TRACE("acpi_fan_exit");

	acpi_bus_unregister_driver(&acpi_fan_driver);

	remove_proc_entry(ACPI_FAN_CLASS, acpi_root_dir);

	return_VOID;
}


module_init(acpi_fan_init);
module_exit(acpi_fan_exit);

