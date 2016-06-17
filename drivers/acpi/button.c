/*
 *  acpi_button.c - ACPI Button Driver ($Revision: 29 $)
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


#define _COMPONENT		ACPI_BUTTON_COMPONENT
ACPI_MODULE_NAME		("acpi_button")

MODULE_AUTHOR("Paul Diefenbaugh");
MODULE_DESCRIPTION(ACPI_BUTTON_DRIVER_NAME);
MODULE_LICENSE("GPL");

#define PREFIX			"ACPI: "


static int acpi_button_add (struct acpi_device *device);
static int acpi_button_remove (struct acpi_device *device, int type);

static struct acpi_driver acpi_button_driver = {
	.name =		ACPI_BUTTON_DRIVER_NAME,
	.class =	ACPI_BUTTON_CLASS,
	.ids =		"ACPI_FPB,ACPI_FSB,PNP0C0D,PNP0C0C,PNP0C0E",
	.ops =		{
				.add =		acpi_button_add,
				.remove =	acpi_button_remove,
			},
};

struct acpi_button {
	acpi_handle		handle;
	struct acpi_device	*device;	/* Fixed button kludge */
	u8			type;
	unsigned long		pushed;
};


/* --------------------------------------------------------------------------
                              FS Interface (/proc)
   -------------------------------------------------------------------------- */

static struct proc_dir_entry	*acpi_button_dir;

static int
acpi_button_read_info (
	char			*page,
	char			**start,
	off_t			off,
	int 			count,
	int 			*eof,
	void			*data)
{
	struct acpi_button	*button = (struct acpi_button *) data;
	char			*p = page;
	int			len = 0;

	ACPI_FUNCTION_TRACE("acpi_button_read_info");

	if (!button || !button->device || (off != 0))
		goto end;

	p += sprintf(p, "type:                    %s\n", 
		acpi_device_name(button->device));

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
acpi_button_lid_read_state(
	char			*page,
	char			**start,
	off_t			off,
	int 			count,
	int 			*eof,
	void			*data)
{
	struct acpi_button	*button = (struct acpi_button *) data;
	char			*p = page;
	int			len = 0;
	acpi_status		status=AE_OK;
	unsigned long		state;

	ACPI_FUNCTION_TRACE("acpi_button_lid_read_state");

	if (!button || !button->device || (off != 0))
		goto end;

	status=acpi_evaluate_integer(button->handle,"_LID",NULL,&state);
	if (ACPI_FAILURE(status)){
	    p += sprintf(p, "state:      unsupported\n");
	}
	else{
	    p += sprintf(p, "state:      %s\n", (state ? "open" : "closed")); 
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
acpi_button_add_fs (
	struct acpi_device	*device)
{
	struct proc_dir_entry	*entry = NULL;
	struct acpi_button	*button = NULL;

	ACPI_FUNCTION_TRACE("acpi_button_add_fs");

	if (!device || !acpi_driver_data(device))
		return_VALUE(-EINVAL);

	button = acpi_driver_data(device);

	switch (button->type) {
	case ACPI_BUTTON_TYPE_POWER:
	case ACPI_BUTTON_TYPE_POWERF:
			entry = proc_mkdir(ACPI_BUTTON_SUBCLASS_POWER, 
				acpi_button_dir);
		break;
	case ACPI_BUTTON_TYPE_SLEEP:
	case ACPI_BUTTON_TYPE_SLEEPF:
			entry = proc_mkdir(ACPI_BUTTON_SUBCLASS_SLEEP, 
				acpi_button_dir);
		break;
	case ACPI_BUTTON_TYPE_LID:
			entry = proc_mkdir(ACPI_BUTTON_SUBCLASS_LID, 
				acpi_button_dir);
		break;
	}

	acpi_device_dir(device) = proc_mkdir(acpi_device_bid(device), entry);
	if (!acpi_device_dir(device))
		return_VALUE(-ENODEV);

	/* 'info' [R] */
	entry = create_proc_entry(ACPI_BUTTON_FILE_INFO,
		S_IRUGO, acpi_device_dir(device));
	if (!entry)
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Unable to create '%s' fs entry\n",
			ACPI_BUTTON_FILE_INFO));
	else {
		entry->read_proc = acpi_button_read_info;
		entry->data = acpi_driver_data(device);
	}
	
	if (button->type==ACPI_BUTTON_TYPE_LID){
	    /* 'state' [R] */
	    entry = create_proc_entry(ACPI_BUTTON_FILE_STATE,
			S_IRUGO, acpi_device_dir(device));
	    if (!entry)
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			  "Unable to create '%s' fs entry\n",
			   ACPI_BUTTON_FILE_STATE));
	    else {
		entry->read_proc = acpi_button_lid_read_state;
		entry->data = acpi_driver_data(device);
	    }
	}

	return_VALUE(0);
}


static int
acpi_button_remove_fs (
	struct acpi_device	*device)
{
	ACPI_FUNCTION_TRACE("acpi_button_remove_fs");

	if (acpi_device_dir(device)) {
		remove_proc_entry(acpi_device_bid(device), acpi_button_dir);
		acpi_device_dir(device) = NULL;
	}

	return_VALUE(0);
}


/* --------------------------------------------------------------------------
                                Driver Interface
   -------------------------------------------------------------------------- */

void
acpi_button_notify (
	acpi_handle		handle,
	u32			event,
	void			*data)
{
	struct acpi_button	*button = (struct acpi_button *) data;

	ACPI_FUNCTION_TRACE("acpi_button_notify");

	if (!button || !button->device)
		return_VOID;

	switch (event) {
	case ACPI_BUTTON_NOTIFY_STATUS:
		acpi_bus_generate_event(button->device, event, ++button->pushed);
		break;
	default:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO,
			"Unsupported event [0x%x]\n", event));
		break;
	}

	return_VOID;
}


acpi_status
acpi_button_notify_fixed (
	void			*data)
{
	struct acpi_button	*button = (struct acpi_button *) data;
	
	ACPI_FUNCTION_TRACE("acpi_button_notify_fixed");

	if (!button)
		return_ACPI_STATUS(AE_BAD_PARAMETER);

	acpi_button_notify(button->handle, ACPI_BUTTON_NOTIFY_STATUS, button);

	return_ACPI_STATUS(AE_OK);
}


static int
acpi_button_add (
	struct acpi_device	*device)
{
	int			result = 0;
	acpi_status		status = AE_OK;
	struct acpi_button	*button = NULL;

	static struct acpi_device *power_button;
	static struct acpi_device *sleep_button;
	static struct acpi_device *lid_button;

	ACPI_FUNCTION_TRACE("acpi_button_add");

	if (!device)
		return_VALUE(-EINVAL);

	button = kmalloc(sizeof(struct acpi_button), GFP_KERNEL);
	if (!button)
		return_VALUE(-ENOMEM);
	memset(button, 0, sizeof(struct acpi_button));

	button->device = device;
	button->handle = device->handle;
	acpi_driver_data(device) = button;

	/*
	 * Determine the button type (via hid), as fixed-feature buttons
	 * need to be handled a bit differently than generic-space.
	 */
	if (!strcmp(acpi_device_hid(device), ACPI_BUTTON_HID_POWER)) {
		button->type = ACPI_BUTTON_TYPE_POWER;
		sprintf(acpi_device_name(device), "%s",
			ACPI_BUTTON_DEVICE_NAME_POWER);
		sprintf(acpi_device_class(device), "%s/%s", 
			ACPI_BUTTON_CLASS, ACPI_BUTTON_SUBCLASS_POWER);
	}
	else if (!strcmp(acpi_device_hid(device), ACPI_BUTTON_HID_POWERF)) {
		button->type = ACPI_BUTTON_TYPE_POWERF;
		sprintf(acpi_device_name(device), "%s",
			ACPI_BUTTON_DEVICE_NAME_POWERF);
		sprintf(acpi_device_class(device), "%s/%s", 
			ACPI_BUTTON_CLASS, ACPI_BUTTON_SUBCLASS_POWER);
	}
	else if (!strcmp(acpi_device_hid(device), ACPI_BUTTON_HID_SLEEP)) {
		button->type = ACPI_BUTTON_TYPE_SLEEP;
		sprintf(acpi_device_name(device), "%s",
			ACPI_BUTTON_DEVICE_NAME_SLEEP);
		sprintf(acpi_device_class(device), "%s/%s", 
			ACPI_BUTTON_CLASS, ACPI_BUTTON_SUBCLASS_SLEEP);
	}
	else if (!strcmp(acpi_device_hid(device), ACPI_BUTTON_HID_SLEEPF)) {
		button->type = ACPI_BUTTON_TYPE_SLEEPF;
		sprintf(acpi_device_name(device), "%s",
			ACPI_BUTTON_DEVICE_NAME_SLEEPF);
		sprintf(acpi_device_class(device), "%s/%s", 
			ACPI_BUTTON_CLASS, ACPI_BUTTON_SUBCLASS_SLEEP);
	}
	else if (!strcmp(acpi_device_hid(device), ACPI_BUTTON_HID_LID)) {
		button->type = ACPI_BUTTON_TYPE_LID;
		sprintf(acpi_device_name(device), "%s",
			ACPI_BUTTON_DEVICE_NAME_LID);
		sprintf(acpi_device_class(device), "%s/%s", 
			ACPI_BUTTON_CLASS, ACPI_BUTTON_SUBCLASS_LID);
	}
	else {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "Unsupported hid [%s]\n",
			acpi_device_hid(device)));
		result = -ENODEV;
		goto end;
	}

	/*
	 * Ensure only one button of each type is used.
	 */
	switch (button->type) {
	case ACPI_BUTTON_TYPE_POWER:
	case ACPI_BUTTON_TYPE_POWERF:
		if (!power_button)
			power_button = device;
		else {
			kfree(button);
			return_VALUE(-ENODEV);
		}
		break;
	case ACPI_BUTTON_TYPE_SLEEP:
	case ACPI_BUTTON_TYPE_SLEEPF:
		if (!sleep_button)
			sleep_button = device;
		else {
			kfree(button);
			return_VALUE(-ENODEV);
		}
		break;
	case ACPI_BUTTON_TYPE_LID:
		if (!lid_button)
			lid_button = device;
		else {
			kfree(button);
			return_VALUE(-ENODEV);
		}
		break;
	}

	result = acpi_button_add_fs(device);
	if (result)
		goto end;

	switch (button->type) {
	case ACPI_BUTTON_TYPE_POWERF:
		status = acpi_install_fixed_event_handler (
			ACPI_EVENT_POWER_BUTTON,
			acpi_button_notify_fixed,
			button);
		break;
	case ACPI_BUTTON_TYPE_SLEEPF:
		status = acpi_install_fixed_event_handler (
			ACPI_EVENT_SLEEP_BUTTON,
			acpi_button_notify_fixed,
			button);
		break;
	default:
		status = acpi_install_notify_handler (
			button->handle,
			ACPI_DEVICE_NOTIFY,
			acpi_button_notify,
			button);
		break;
	}

	if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Error installing notify handler\n"));
		result = -ENODEV;
		goto end;
	}

	printk(KERN_INFO PREFIX "%s [%s]\n", 
		acpi_device_name(device), acpi_device_bid(device));

end:
	if (result) {
		acpi_button_remove_fs(device);
		kfree(button);
	}

	return_VALUE(result);
}


static int
acpi_button_remove (struct acpi_device *device, int type)
{
	acpi_status		status = 0;
	struct acpi_button	*button = NULL;

	ACPI_FUNCTION_TRACE("acpi_button_remove");

	if (!device || !acpi_driver_data(device))
		return_VALUE(-EINVAL);

	button = acpi_driver_data(device);

	/* Unregister for device notifications. */
	switch (button->type) {
	case ACPI_BUTTON_TYPE_POWERF:
		status = acpi_remove_fixed_event_handler(
			ACPI_EVENT_POWER_BUTTON, acpi_button_notify_fixed);
		break;
	case ACPI_BUTTON_TYPE_SLEEPF:
		status = acpi_remove_fixed_event_handler(
			ACPI_EVENT_SLEEP_BUTTON, acpi_button_notify_fixed);
		break;
	default:
		status = acpi_remove_notify_handler(button->handle,
			ACPI_DEVICE_NOTIFY, acpi_button_notify);
		break;
	}

	if (ACPI_FAILURE(status))
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Error removing notify handler\n"));

	acpi_button_remove_fs(device);	

	kfree(button);

	return_VALUE(0);
}


static int __init
acpi_button_init (void)
{
	int			result = 0;

	ACPI_FUNCTION_TRACE("acpi_button_init");

	acpi_button_dir = proc_mkdir(ACPI_BUTTON_CLASS, acpi_root_dir);
	if (!acpi_button_dir)
		return_VALUE(-ENODEV);

	result = acpi_bus_register_driver(&acpi_button_driver);
	if (result < 0) {
		remove_proc_entry(ACPI_BUTTON_CLASS, acpi_root_dir);
		return_VALUE(-ENODEV);
	}

	return_VALUE(0);
}


static void __exit
acpi_button_exit (void)
{
	ACPI_FUNCTION_TRACE("acpi_button_exit");

	acpi_bus_unregister_driver(&acpi_button_driver);

	remove_proc_entry(ACPI_BUTTON_CLASS, acpi_root_dir);

	return_VOID;
}


module_init(acpi_button_init);
module_exit(acpi_button_exit);
