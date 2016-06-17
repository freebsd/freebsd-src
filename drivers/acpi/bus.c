/*
 *  acpi_bus.c - ACPI Bus Driver ($Revision: 77 $)
 *
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
#include <linux/version.h>
#include <linux/ioport.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/proc_fs.h>
#ifdef CONFIG_X86
#include <asm/mpspec.h>
#endif
#include <acpi/acpi_bus.h>
#include <acpi/acpi_drivers.h>
#include <acpi/acinterp.h>	/* for acpi_ex_eisa_id_to_string() */


#define _COMPONENT		ACPI_BUS_COMPONENT
ACPI_MODULE_NAME		("acpi_bus")

MODULE_AUTHOR("Paul Diefenbaugh");
MODULE_DESCRIPTION(ACPI_BUS_DRIVER_NAME);
MODULE_LICENSE("GPL");

#define	PREFIX			"ACPI: "

extern void __init acpi_pic_sci_set_trigger(unsigned int irq, u16 trigger);

FADT_DESCRIPTOR			acpi_fadt;
struct acpi_device		*acpi_root;
struct proc_dir_entry		*acpi_root_dir;

#define STRUCT_TO_INT(s)	(*((int*)&s))


/* --------------------------------------------------------------------------
                          Linux Driver Model (LDM) Support
   -------------------------------------------------------------------------- */

#ifdef CONFIG_LDM

static int acpi_device_probe(struct device *dev);
static int acpi_device_remove(struct device *dev);
static int acpi_device_suspend(struct device *dev, u32 state, u32 stage);
static int acpi_device_resume(struct device *dev, u32 stage);

static struct device_driver acpi_bus_driver = {
	.probe = acpi_device_probe,
	.remove = acpi_device_remove,	
	.suspend = acpi_device_suspend,
	.resume = acpi_device_resume,
};


static int
acpi_device_probe (
	struct device		*dev)
{
	ACPI_FUNCTION_TRACE("acpi_device_probe");

	if (!dev)
		return_VALUE(-EINVAL);

	/* TBD */

	return_VALUE(0);
}


static int
acpi_device_remove (
	struct device		*dev)
{
	ACPI_FUNCTION_TRACE("acpi_device_remove");

	if (!dev)
		return_VALUE(-EINVAL);

	/* TBD */

	return_VALUE(0);
}


static int
acpi_device_suspend (
	struct device		*dev,
	u32			state,
	u32			stage)
{
	ACPI_FUNCTION_TRACE("acpi_device_suspend");

	if (!dev)
		return_VALUE(-EINVAL);

	/* TBD */

	return_VALUE(0);
}


static int
acpi_device_resume (
	struct device		*dev,
	u32			stage)
{
	ACPI_FUNCTION_TRACE("acpi_device_resume");

	if (!dev)
		return_VALUE(-EINVAL);

	/* TBD */

	return_VALUE(0);
}

#if 0 /* not used ATM */
static int
acpi_platform_add (
	struct device		*dev)
{
	ACPI_FUNCTION_TRACE("acpi_platform_add");

	if (!dev)
		return -EINVAL;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Device %s (%s) added\n",
		dev->name, dev->bus_id));

	/* TBD */

	return_VALUE(0);
}


static int
acpi_platform_remove (
	struct device		*dev)
{
	ACPI_FUNCTION_TRACE("acpi_platform_add");

	if (!dev)
		return -EINVAL;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Device %s (%s) removed\n",
		dev->name, dev->bus_id));

	/* TBD */

	return_VALUE(0);
}
#endif /* unused */


#endif /*CONFIG_LDM*/


static int
acpi_device_register (
	struct acpi_device	*device,
	struct acpi_device	*parent)
{
	int			result = 0;

	ACPI_FUNCTION_TRACE("acpi_device_register");

	if (!device)
		return_VALUE(-EINVAL);

#ifdef CONFIG_LDM
	sprintf(device->dev.name, "ACPI device %s:%s", 
		device->pnp.hardware_id, device->pnp.unique_id);
	strncpy(device->dev.bus_id, device->pnp.bus_id, sizeof(acpi_bus_id));
	if (parent)
		device->dev.parent = &parent->dev;
	device->dev.driver = &acpi_bus_driver;

	result = device_register(&device->dev);
#endif /*CONFIG_LDM*/

	return_VALUE(result);
}


static int
acpi_device_unregister (
	struct acpi_device	*device)
{
	ACPI_FUNCTION_TRACE("acpi_device_unregister");

	if (!device)
		return_VALUE(-EINVAL);

#ifdef CONFIG_LDM
	put_device(&device->dev);
#endif /*CONFIG_LDM*/

	return_VALUE(0);
}


/* --------------------------------------------------------------------------
                                Device Management
   -------------------------------------------------------------------------- */

static void
acpi_bus_data_handler (
	acpi_handle		handle,
	u32			function,
	void			*context)
{
	ACPI_FUNCTION_TRACE("acpi_bus_data_handler");

	/* TBD */

	return_VOID;
}


int
acpi_bus_get_device (
	acpi_handle		handle,
	struct acpi_device	**device)
{
	acpi_status		status = AE_OK;

	ACPI_FUNCTION_TRACE("acpi_bus_get_device");

	if (!device)
		return_VALUE(-EINVAL);

	/* TBD: Support fixed-feature devices */

	status = acpi_get_data(handle, acpi_bus_data_handler, (void**) device);
	if (ACPI_FAILURE(status) || !*device) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN, "Error getting context for object [%p]\n",
			handle));
		return_VALUE(-ENODEV);
	}

	return_VALUE(0);
}

int
acpi_bus_get_status (
	struct acpi_device	*device)
{
	acpi_status		status = AE_OK;
	unsigned long		sta = 0;
	
	ACPI_FUNCTION_TRACE("acpi_bus_get_status");

	if (!device)
		return_VALUE(-EINVAL);

	/*
	 * Evaluate _STA if present.
	 */
	if (device->flags.dynamic_status) {
		status = acpi_evaluate_integer(device->handle, "_STA", NULL, &sta);
		if (ACPI_FAILURE(status))
			return_VALUE(-ENODEV);
		STRUCT_TO_INT(device->status) = (int) sta;
	}

	/*
	 * Otherwise we assume the status of our parent (unless we don't
	 * have one, in which case status is implied).
	 */
	else if (device->parent)
		device->status = device->parent->status;
	else
		STRUCT_TO_INT(device->status) = 0x0F;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Device [%s] status [%08x]\n", 
		device->pnp.bus_id, (u32) STRUCT_TO_INT(device->status)));

	return_VALUE(0);
}


/*
static int
acpi_bus_create_device_fs (struct device *device)
{
	ACPI_FUNCTION_TRACE("acpi_bus_create_device_fs");

	if (!device)
		return_VALUE(-EINVAL);

	if (device->dir.entry)
		return_VALUE(-EEXIST);

	if (!device->parent)
		device->dir.entry = proc_mkdir(device->pnp.bus_id, NULL);
	else
		device->dir.entry = proc_mkdir(device->pnp.bus_id,
			device->parent->fs.entry);

	if (!device->dir.entry) {
		printk(KERN_ERR PREFIX "Unable to create fs entry '%s'\n",
			device->pnp.bus_id);
		return_VALUE(-ENODEV);
	}

	return_VALUE(0);
}


static int
acpi_bus_remove_device_fs (struct device *device)
{
	ACPI_FUNCTION_TRACE("acpi_bus_create_device_fs");

	if (!device)
		return_VALUE(-EINVAL);

	if (!device->dir.entry)
		return_VALUE(-ENODEV);

	if (!device->parent)
		remove_proc_entry(device->pnp_bus_id, NULL);
	else
		remove_proc_entry(device->pnp.bus_id, device->parent->fs.entry);

	device->dir.entry = NULL;

	return_VALUE(0);
}
*/


/* --------------------------------------------------------------------------
                                 Power Management
   -------------------------------------------------------------------------- */

int
acpi_bus_get_power (
	acpi_handle		handle,
	int			*state)
{
	int			result = 0;
	acpi_status             status = 0;
	struct acpi_device	*device = NULL;
	unsigned long		psc = 0;

	ACPI_FUNCTION_TRACE("acpi_bus_get_power");

	result = acpi_bus_get_device(handle, &device);
	if (result)
		return_VALUE(result);

	*state = ACPI_STATE_UNKNOWN;

	if (!device->flags.power_manageable) {
		/* TBD: Non-recursive algorithm for walking up hierarchy */
		if (device->parent)
			*state = device->parent->power.state;
		else
			*state = ACPI_STATE_D0;
	}
	else {
		/*
		 * Get the device's power state either directly (via _PSC) or 
		 * indirectly (via power resources).
		 */
		if (device->power.flags.explicit_get) {
			status = acpi_evaluate_integer(device->handle, "_PSC", 
				NULL, &psc);
			if (ACPI_FAILURE(status))
				return_VALUE(-ENODEV);
			device->power.state = (int) psc;
		}
		else if (device->power.flags.power_resources) {
			result = acpi_power_get_inferred_state(device);
			if (result)
				return_VALUE(result);
		}

		*state = device->power.state;
	}

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Device [%s] power state is D%d\n",
		device->pnp.bus_id, device->power.state));

	return_VALUE(0);
}


int
acpi_bus_set_power (
	acpi_handle		handle,
	int			state)
{
	int			result = 0;
	acpi_status		status = AE_OK;
	struct acpi_device	*device = NULL;
	char			object_name[5] = {'_','P','S','0'+state,'\0'};

	ACPI_FUNCTION_TRACE("acpi_bus_set_power");

	result = acpi_bus_get_device(handle, &device);
	if (result)
		return_VALUE(result);

	if ((state < ACPI_STATE_D0) || (state > ACPI_STATE_D3))
		return_VALUE(-EINVAL);

	/* Make sure this is a valid target state */

	if (!device->flags.power_manageable) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN, "Device is not power manageable\n"));
		return_VALUE(-ENODEV);
	}
	if (state == device->power.state) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Device is already at D%d\n", state));
		return_VALUE(0);
	}
	if (!device->power.states[state].flags.valid) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN, "Device does not support D%d\n", state));
		return_VALUE(-ENODEV);
	}
	if (device->parent && (state < device->parent->power.state)) {
		ACPI_DEBUG_PRINT((ACPI_DB_WARN, "Cannot set device to a higher-powered state than parent\n"));
		return_VALUE(-ENODEV);
	}

	/*
	 * Transition Power
	 * ----------------
	 * On transitions to a high-powered state we first apply power (via
	 * power resources) then evalute _PSx.  Conversly for transitions to
	 * a lower-powered state.
	 */ 
	if (state < device->power.state) {
		if (device->power.flags.power_resources) {
			result = acpi_power_transition(device, state);
			if (result)
				goto end;
		}
		if (device->power.states[state].flags.explicit_set) {
			status = acpi_evaluate_object(device->handle, 
				object_name, NULL, NULL);
			if (ACPI_FAILURE(status)) {
				result = -ENODEV;
				goto end;
			}
		}
	}
	else {
		if (device->power.states[state].flags.explicit_set) {
			status = acpi_evaluate_object(device->handle, 
				object_name, NULL, NULL);
			if (ACPI_FAILURE(status)) {
				result = -ENODEV;
				goto end;
			}
		}
		if (device->power.flags.power_resources) {
			result = acpi_power_transition(device, state);
			if (result)
				goto end;
		}
	}

end:
	if (result)
		ACPI_DEBUG_PRINT((ACPI_DB_WARN, "Error transitioning device [%s] to D%d\n",
			device->pnp.bus_id, state));
	else
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Device [%s] transitioned to D%d\n",
			device->pnp.bus_id, state));

	return_VALUE(result);
}


static int
acpi_bus_get_power_flags (
	struct acpi_device	*device)
{
	acpi_status             status = 0;
	acpi_handle		handle = 0;
	u32                     i = 0;

	ACPI_FUNCTION_TRACE("acpi_bus_get_power_flags");

	if (!device)
		return -ENODEV;

	/*
	 * Power Management Flags
	 */
	status = acpi_get_handle(device->handle, "_PSC", &handle);
	if (ACPI_SUCCESS(status))
		device->power.flags.explicit_get = 1;
	status = acpi_get_handle(device->handle, "_IRC", &handle);
	if (ACPI_SUCCESS(status))
		device->power.flags.inrush_current = 1;
	status = acpi_get_handle(device->handle, "_PRW", &handle);
	if (ACPI_SUCCESS(status))
		device->power.flags.wake_capable = 1;

	/*
	 * Enumerate supported power management states
	 */
	for (i = ACPI_STATE_D0; i <= ACPI_STATE_D3; i++) {
		struct acpi_device_power_state *ps = &device->power.states[i];
		char		object_name[5] = {'_','P','R','0'+i,'\0'};

		/* Evaluate "_PRx" to se if power resources are referenced */
		acpi_evaluate_reference(device->handle, object_name, NULL,
			&ps->resources);
		if (ps->resources.count) {
			device->power.flags.power_resources = 1;
			ps->flags.valid = 1;
		}

		/* Evaluate "_PSx" to see if we can do explicit sets */
		object_name[2] = 'S';
		status = acpi_get_handle(device->handle, object_name, &handle);
		if (ACPI_SUCCESS(status)) {
			ps->flags.explicit_set = 1;
			ps->flags.valid = 1;
		}

		/* State is valid if we have some power control */
		if (ps->resources.count || ps->flags.explicit_set)
			ps->flags.valid = 1;

		ps->power = -1;		/* Unknown - driver assigned */
		ps->latency = -1;	/* Unknown - driver assigned */
	}

	/* Set defaults for D0 and D3 states (always valid) */
	device->power.states[ACPI_STATE_D0].flags.valid = 1;
	device->power.states[ACPI_STATE_D0].power = 100;
	device->power.states[ACPI_STATE_D3].flags.valid = 1;
	device->power.states[ACPI_STATE_D3].power = 0;

	/*
	 * System Power States
	 * -------------------
	 */
	/* TBD: S1-S4 power state support and resource requirements. */
	/*
	for (i=ACPI_STATE_S1; i<ACPI_STATE_S5; i++) {
		char name[5] = {'_','S',('0'+i),'D','\0'};
		status = acpi_evaluate_integer(device->handle, name, NULL,
			&state);
		if (ACPI_FAILURE(status))
			continue;
	}
	*/

	/* TBD: System wake support and resource requirements. */

	device->power.state = ACPI_STATE_UNKNOWN;

	return 0;
}


/* --------------------------------------------------------------------------
                              Performance Management
   -------------------------------------------------------------------------- */

static int
acpi_bus_get_perf_flags (
	struct acpi_device	*device)
{
	ACPI_FUNCTION_TRACE("acpi_bus_get_perf_flags");

	if (!device)
		return -ENODEV;

	device->performance.state = ACPI_STATE_UNKNOWN;

	return 0;
}


/* --------------------------------------------------------------------------
                                Event Management
   -------------------------------------------------------------------------- */

static spinlock_t		acpi_bus_event_lock = SPIN_LOCK_UNLOCKED;

LIST_HEAD(acpi_bus_event_list);
DECLARE_WAIT_QUEUE_HEAD(acpi_bus_event_queue);

extern int			event_is_open;

int
acpi_bus_generate_event (
	struct acpi_device	*device,
	u8			type,
	int			data)
{
	struct acpi_bus_event	*event = NULL;
	u32			flags = 0;

	ACPI_FUNCTION_TRACE("acpi_bus_generate_event");

	if (!device)
		return_VALUE(-EINVAL);

	/* drop event on the floor if no one's listening */
	if (!event_is_open)
		return_VALUE(0);

	event = kmalloc(sizeof(struct acpi_bus_event), GFP_ATOMIC);
	if (!event)
		return_VALUE(-ENOMEM);

	sprintf(event->device_class, "%s", device->pnp.device_class);
	sprintf(event->bus_id, "%s", device->pnp.bus_id);
	event->type = type;
	event->data = data;

	spin_lock_irqsave(&acpi_bus_event_lock, flags);
	list_add_tail(&event->node, &acpi_bus_event_list);
	spin_unlock_irqrestore(&acpi_bus_event_lock, flags);

	wake_up_interruptible(&acpi_bus_event_queue);

	return_VALUE(0);
}

int
acpi_bus_receive_event (
	struct acpi_bus_event	*event)
{
	u32			flags = 0;
	struct acpi_bus_event	*entry = NULL;

	DECLARE_WAITQUEUE(wait, current);

	ACPI_FUNCTION_TRACE("acpi_bus_receive_event");

	if (!event)
		return -EINVAL;

	if (list_empty(&acpi_bus_event_list)) {

		set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&acpi_bus_event_queue, &wait);

		if (list_empty(&acpi_bus_event_list))
			schedule();

		remove_wait_queue(&acpi_bus_event_queue, &wait);
		set_current_state(TASK_RUNNING);

		if (signal_pending(current))
			return_VALUE(-ERESTARTSYS);
	}

	spin_lock_irqsave(&acpi_bus_event_lock, flags);
	entry = list_entry(acpi_bus_event_list.next, struct acpi_bus_event, node);
	if (entry)
		list_del(&entry->node);
	spin_unlock_irqrestore(&acpi_bus_event_lock, flags);

	if (!entry)
		return_VALUE(-ENODEV);

	memcpy(event, entry, sizeof(struct acpi_bus_event));

	kfree(entry);

	return_VALUE(0);
}


/* --------------------------------------------------------------------------
                               Namespace Management
   -------------------------------------------------------------------------- */

#define WALK_UP			0
#define WALK_DOWN		1

typedef int (*acpi_bus_walk_callback)(struct acpi_device*, int, void*);

#define HAS_CHILDREN(d)		((d)->children.next != &((d)->children))
#define HAS_SIBLINGS(d)		(((d)->parent) && ((d)->node.next != &(d)->parent->children))
#define NODE_TO_DEVICE(n)	(list_entry(n, struct acpi_device, node))


/**
 * acpi_bus_walk
 * -------------
 * Used to walk the ACPI Bus's device namespace.  Can walk down (depth-first)
 * or up.  Able to parse starting at any node in the namespace.  Note that a
 * callback return value of -ELOOP will terminate the walk.
 *
 * @start:	starting point
 * callback:	function to call for every device encountered while parsing
 * direction:	direction to parse (up or down)
 * @data:	context for this search operation
 */
static int
acpi_bus_walk (
	struct acpi_device	*start, 
	acpi_bus_walk_callback	callback, 
	int			direction, 
	void			*data)
{
	int			result = 0;
	int			level = 0;
	struct acpi_device	*device = NULL;

	if (!start || !callback)
		return -EINVAL;

	device = start;

	/*
	 * Parse Namespace
	 * ---------------
	 * Parse a given subtree (specified by start) in the given direction.
	 * Walking 'up' simply means that we execute the callback on leaf
	 * devices prior to their parents (useful for things like removing
	 * or powering down a subtree).
	 */

	while (device) {

		if (direction == WALK_DOWN)
			if (-ELOOP == callback(device, level, data))
				break;

		/* Depth First */

		if (HAS_CHILDREN(device)) {
			device = NODE_TO_DEVICE(device->children.next);
			++level;
			continue;
		}

		if (direction == WALK_UP)
			if (-ELOOP == callback(device, level, data))
				break;

		/* Now Breadth */

		if (HAS_SIBLINGS(device)) {
			device = NODE_TO_DEVICE(device->node.next);
			continue;
		}

		/* Scope Exhausted - Find Next */

		while ((device = device->parent)) {
			--level;
			if (HAS_SIBLINGS(device)) {
				device = NODE_TO_DEVICE(device->node.next);
				break;
			}
		}
	}

	if ((direction == WALK_UP) && (result == 0))
		callback(start, level, data);

	return result;
}


/* --------------------------------------------------------------------------
                             Notification Handling
   -------------------------------------------------------------------------- */

static int
acpi_bus_check_device (
	struct acpi_device	*device,
	int			*status_changed)
{
	acpi_status             status = 0;
	struct acpi_device_status old_status;

	ACPI_FUNCTION_TRACE("acpi_bus_check_device");

	if (!device)
		return_VALUE(-EINVAL);

	if (status_changed)
		*status_changed = 0;

	old_status = device->status;

	/*
	 * Make sure this device's parent is present before we go about
	 * messing with the device.
	 */
	if (device->parent && !device->parent->status.present) {
		device->status = device->parent->status;
		if (STRUCT_TO_INT(old_status) != STRUCT_TO_INT(device->status)) {
			if (status_changed)
				*status_changed = 1;
		}
		return_VALUE(0);
	}

	status = acpi_bus_get_status(device);
	if (ACPI_FAILURE(status))
		return_VALUE(-ENODEV);

	if (STRUCT_TO_INT(old_status) == STRUCT_TO_INT(device->status))
		return_VALUE(0);

	if (status_changed)
		*status_changed = 1;
	
	/*
	 * Device Insertion/Removal
	 */
	if ((device->status.present) && !(old_status.present)) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Device insertion detected\n"));
		/* TBD: Handle device insertion */
	}
	else if (!(device->status.present) && (old_status.present)) {
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Device removal detected\n"));
		/* TBD: Handle device removal */
	}

	return_VALUE(0);
}


static int
acpi_bus_check_scope (
	struct acpi_device	*device)
{
	int			result = 0;
	int			status_changed = 0;

	ACPI_FUNCTION_TRACE("acpi_bus_check_scope");

	if (!device)
		return_VALUE(-EINVAL);

	/* Status Change? */
	result = acpi_bus_check_device(device, &status_changed);
	if (result)
		return_VALUE(result);

	if (!status_changed)
		return_VALUE(0);

	/*
	 * TBD: Enumerate child devices within this device's scope and
	 *       run acpi_bus_check_device()'s on them.
	 */

	return_VALUE(0);
}


/**
 * acpi_bus_notify
 * ---------------
 * Callback for all 'system-level' device notifications (values 0x00-0x7F).
 */
static void 
acpi_bus_notify (
	acpi_handle             handle,
	u32                     type,
	void                    *data)
{
	int			result = 0;
	struct acpi_device	*device = NULL;

	ACPI_FUNCTION_TRACE("acpi_bus_notify");

	if (acpi_bus_get_device(handle, &device))
		return_VOID;

	switch (type) {

	case ACPI_NOTIFY_BUS_CHECK:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Received BUS CHECK notification for device [%s]\n", 
			device->pnp.bus_id));
		result = acpi_bus_check_scope(device);
		/* 
		 * TBD: We'll need to outsource certain events to non-ACPI
		 *	drivers via the device manager (device.c).
		 */
		break;

	case ACPI_NOTIFY_DEVICE_CHECK:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Received DEVICE CHECK notification for device [%s]\n", 
			device->pnp.bus_id));
		result = acpi_bus_check_device(device, NULL);
		/* 
		 * TBD: We'll need to outsource certain events to non-ACPI
		 *	drivers via the device manager (device.c).
		 */
		break;

	case ACPI_NOTIFY_DEVICE_WAKE:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Received DEVICE WAKE notification for device [%s]\n", 
			device->pnp.bus_id));
		/* TBD */
		break;

	case ACPI_NOTIFY_EJECT_REQUEST:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Received EJECT REQUEST notification for device [%s]\n", 
			device->pnp.bus_id));
		/* TBD */
		break;

	case ACPI_NOTIFY_DEVICE_CHECK_LIGHT:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Received DEVICE CHECK LIGHT notification for device [%s]\n", 
			device->pnp.bus_id));
		/* TBD: Exactly what does 'light' mean? */
		break;

	case ACPI_NOTIFY_FREQUENCY_MISMATCH:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Received FREQUENCY MISMATCH notification for device [%s]\n", 
			device->pnp.bus_id));
		/* TBD */
		break;

	case ACPI_NOTIFY_BUS_MODE_MISMATCH:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Received BUS MODE MISMATCH notification for device [%s]\n", 
			device->pnp.bus_id));
		/* TBD */
		break;

	case ACPI_NOTIFY_POWER_FAULT:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Received POWER FAULT notification for device [%s]\n", 
			device->pnp.bus_id));
		/* TBD */
		break;

	default:
		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Received unknown/unsupported notification [%08x]\n", 
			type));
		break;
	}

	return_VOID;
}


/* --------------------------------------------------------------------------
                                 Driver Management
   -------------------------------------------------------------------------- */

static LIST_HEAD(acpi_bus_drivers);
static DECLARE_MUTEX(acpi_bus_drivers_lock);


/**
 * acpi_bus_match 
 * --------------
 * Checks the device's hardware (_HID) or compatible (_CID) ids to see if it
 * matches the specified driver's criteria.
 */
static int
acpi_bus_match (
	struct acpi_device	*device,
	struct acpi_driver	*driver)
{
	int error = 0;

	if (device->flags.hardware_id)
		if (strstr(driver->ids, device->pnp.hardware_id))
			goto Done;

	if (device->flags.compatible_ids) {
		struct acpi_compatible_id_list *cid_list = device->pnp.cid_list;
		int i;

		/* compare multiple _CID entries against driver ids */
		for (i = 0; i < cid_list->count; i++)
		{
			if (strstr(driver->ids, cid_list->id[i].value))
				goto Done;
		}
	}
	error = -ENOENT;

 Done:
	
	return error;
}


/**
 * acpi_bus_driver_init 
 * --------------------
 * Used to initialize a device via its device driver.  Called whenever a 
 * driver is bound to a device.  Invokes the driver's add() and start() ops.
 */
static int
acpi_bus_driver_init (
	struct acpi_device	*device, 
	struct acpi_driver	*driver)
{
	int			result = 0;

	ACPI_FUNCTION_TRACE("acpi_bus_driver_init");

	if (!device || !driver)
		return_VALUE(-EINVAL);

	if (!driver->ops.add)
		return_VALUE(-ENOSYS);

	result = driver->ops.add(device);
	if (result) {
		device->driver = NULL;
		acpi_driver_data(device) = NULL;
		return_VALUE(result);
	}

	device->driver = driver;

	/*
	 * TBD - Configuration Management: Assign resources to device based
	 * upon possible configuration and currently allocated resources.
	 */

	if (driver->ops.start) {
		result = driver->ops.start(device);
		if (result && driver->ops.remove)
			driver->ops.remove(device, ACPI_BUS_REMOVAL_NORMAL);
		return_VALUE(result);
	}

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Driver successfully bound to device\n"));

#ifdef CONFIG_LDM
	/* 
	 * Update the device information (in the global device hierarchy) now
	 * that there's a driver bound to it.
	 */
	strncpy(device->dev.name, device->pnp.device_name, 
		sizeof(device->dev.name));
#endif

	if (driver->ops.scan) {
		driver->ops.scan(device);
	}

	return_VALUE(0);
}


/**
 * acpi_bus_attach 
 * -------------
 * Callback for acpi_bus_walk() used to find devices that match a specific 
 * driver's criteria and then attach the driver.
 */
static int
acpi_bus_attach (
	struct acpi_device	*device, 
	int			level, 
	void			*data)
{
	int			result = 0;
	struct acpi_driver	*driver = NULL;

	ACPI_FUNCTION_TRACE("acpi_bus_attach");

	if (!device || !data)
		return_VALUE(-EINVAL);

	driver = (struct acpi_driver *) data;

	if (device->driver)
		return_VALUE(-EEXIST);

	if (!device->status.present)
		return_VALUE(-ENODEV);

	result = acpi_bus_match(device, driver);
	if (result)
		return_VALUE(result);

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Found driver [%s] for device [%s]\n",
		driver->name, device->pnp.bus_id));
	
	result = acpi_bus_driver_init(device, driver);
	if (result)
		return_VALUE(result);

	down(&acpi_bus_drivers_lock);
	++driver->references;
	up(&acpi_bus_drivers_lock);

	return_VALUE(0);
}


/**
 * acpi_bus_unattach 
 * -----------------
 * Callback for acpi_bus_walk() used to find devices that match a specific 
 * driver's criteria and unattach the driver.
 */
static int
acpi_bus_unattach (
	struct acpi_device	*device, 
	int			level, 
	void			*data)
{
	int			result = 0;
	struct acpi_driver	*driver = (struct acpi_driver *) data;

	ACPI_FUNCTION_TRACE("acpi_bus_unattach");

	if (!device || !driver)
		return_VALUE(-EINVAL);

	if (device->driver != driver)
		return_VALUE(-ENOENT);

	if (!driver->ops.remove)
		return_VALUE(-ENOSYS);

	result = driver->ops.remove(device, ACPI_BUS_REMOVAL_NORMAL);
	if (result)
		return_VALUE(result);

	device->driver = NULL;
	acpi_driver_data(device) = NULL;

	down(&acpi_bus_drivers_lock);
	driver->references--;
	up(&acpi_bus_drivers_lock);

	return_VALUE(0);
}


/**
 * acpi_bus_find_driver 
 * --------------------
 * Parses the list of registered drivers looking for a driver applicable for
 * the specified device.
 */
static int
acpi_bus_find_driver (
	struct acpi_device	*device)
{
	int			result = -ENODEV;
	struct list_head	*entry = NULL;
	struct acpi_driver	*driver = NULL;

	ACPI_FUNCTION_TRACE("acpi_bus_find_driver");

	if (!device || device->driver)
		return_VALUE(-EINVAL);

	down(&acpi_bus_drivers_lock);

	list_for_each(entry, &acpi_bus_drivers) {

		driver = list_entry(entry, struct acpi_driver, node);

		if (acpi_bus_match(device, driver))
			continue;

		result = acpi_bus_driver_init(device, driver);
		if (!result)
			++driver->references;

		break;
	}

	up(&acpi_bus_drivers_lock);

	return_VALUE(result);
}


/**
 * acpi_bus_register_driver 
 * ------------------------ 
 * Registers a driver with the ACPI bus.  Searches the namespace for all
 * devices that match the driver's criteria and binds.
 */
int
acpi_bus_register_driver (
	struct acpi_driver	*driver)
{
	ACPI_FUNCTION_TRACE("acpi_bus_register_driver");

	if (!driver)
		return_VALUE(-EINVAL);

	if (acpi_disabled)
		return_VALUE(-ENODEV);

	down(&acpi_bus_drivers_lock);
	list_add_tail(&driver->node, &acpi_bus_drivers);
	up(&acpi_bus_drivers_lock);

	acpi_bus_walk(acpi_root, acpi_bus_attach, 
		WALK_DOWN, driver);

	return_VALUE(driver->references);
}


/**
 * acpi_bus_unregister_driver 
 * --------------------------
 * Unregisters a driver with the ACPI bus.  Searches the namespace for all
 * devices that match the driver's criteria and unbinds.
 */
int
acpi_bus_unregister_driver (
	struct acpi_driver	*driver)
{
	ACPI_FUNCTION_TRACE("acpi_bus_unregister_driver");

	if (!driver)
		return_VALUE(-EINVAL);

	acpi_bus_walk(acpi_root, acpi_bus_unattach, WALK_UP, driver);

	if (driver->references)
		return_VALUE(driver->references);

	down(&acpi_bus_drivers_lock);
	list_del(&driver->node);
	up(&acpi_bus_drivers_lock);

	return_VALUE(0);
}


/* --------------------------------------------------------------------------
                                 Device Enumeration
   -------------------------------------------------------------------------- */

static int 
acpi_bus_get_flags (
	struct acpi_device	*device)
{
	acpi_status		status = AE_OK;
	acpi_handle		temp = NULL;

	ACPI_FUNCTION_TRACE("acpi_bus_get_flags");

	/* Presence of _STA indicates 'dynamic_status' */
	status = acpi_get_handle(device->handle, "_STA", &temp);
	if (ACPI_SUCCESS(status))
		device->flags.dynamic_status = 1;

	/* Presence of _CID indicates 'compatible_ids' */
	status = acpi_get_handle(device->handle, "_CID", &temp);
	if (ACPI_SUCCESS(status))
		device->flags.compatible_ids = 1;

	/* Presence of _RMV indicates 'removable' */
	status = acpi_get_handle(device->handle, "_RMV", &temp);
	if (ACPI_SUCCESS(status))
		device->flags.removable = 1;

	/* Presence of _EJD|_EJ0 indicates 'ejectable' */
	status = acpi_get_handle(device->handle, "_EJD", &temp);
	if (ACPI_SUCCESS(status))
		device->flags.ejectable = 1;
	else {
		status = acpi_get_handle(device->handle, "_EJ0", &temp);
		if (ACPI_SUCCESS(status))
			device->flags.ejectable = 1;
	}

	/* Presence of _LCK indicates 'lockable' */
	status = acpi_get_handle(device->handle, "_LCK", &temp);
	if (ACPI_SUCCESS(status))
		device->flags.lockable = 1;

	/* Presence of _PS0|_PR0 indicates 'power manageable' */
	status = acpi_get_handle(device->handle, "_PS0", &temp);
	if (ACPI_FAILURE(status))
		status = acpi_get_handle(device->handle, "_PR0", &temp);
	if (ACPI_SUCCESS(status))
		device->flags.power_manageable = 1;

	/* TBD: Peformance management */

	return_VALUE(0);
}


static int 
acpi_bus_add (
	struct acpi_device	**child,
	struct acpi_device	*parent,
	acpi_handle		handle,
	int			type)
{
	int			result = 0;
	acpi_status		status = AE_OK;
	struct acpi_device	*device = NULL;
	char			bus_id[5] = {'?',0};
	struct acpi_buffer	buffer = {0, NULL};
	struct acpi_device_info	*info;
	char			*hid = NULL;
	char			*uid = NULL;
	struct acpi_compatible_id_list *cid_list = NULL;
	int			i = 0;

	ACPI_FUNCTION_TRACE("acpi_bus_add");

	if (!child)
		return_VALUE(-EINVAL);

	device = kmalloc(sizeof(struct acpi_device), GFP_KERNEL);
	if (!device) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "Memory allocation error\n"));
		return_VALUE(-ENOMEM);
	}
	memset(device, 0, sizeof(struct acpi_device));

	device->handle = handle;
	device->parent = parent;

	/*
	 * Bus ID
	 * ------
	 * The device's Bus ID is simply the object name.
	 * TBD: Shouldn't this value be unique (within the ACPI namespace)?
	 */
	switch (type) {
	case ACPI_BUS_TYPE_SYSTEM:
		sprintf(device->pnp.bus_id, "%s", "ACPI");
		break;
	case ACPI_BUS_TYPE_POWER_BUTTON:
		sprintf(device->pnp.bus_id, "%s", "PWRF");
		break;
	case ACPI_BUS_TYPE_SLEEP_BUTTON:
		sprintf(device->pnp.bus_id, "%s", "SLPF");
		break;
	default:
		buffer.length = sizeof(bus_id);
		buffer.pointer = bus_id;
		acpi_get_name(handle, ACPI_SINGLE_NAME, &buffer);
		/* Clean up trailing underscores (if any) */
		for (i = 3; i > 1; i--) {
			if (bus_id[i] == '_')
				bus_id[i] = '\0';
			else
				break;
		}
		sprintf(device->pnp.bus_id, "%s", bus_id);
		buffer.pointer = NULL;
		break;
	}

	/*
	 * Flags
	 * -----
	 * Get prior to calling acpi_bus_get_status() so we know whether
	 * or not _STA is present.  Note that we only look for object
	 * handles -- cannot evaluate objects until we know the device is
	 * present and properly initialized.
	 */
	result = acpi_bus_get_flags(device);
	if (result)
		goto end;

	/*
	 * Status
	 * ------
	 * See if the device is present.  We always assume that non-Device()
	 * objects (e.g. thermal zones, power resources, processors, etc.) are
	 * present, functioning, etc. (at least when parent object is present).
	 * Note that _STA has a different meaning for some objects (e.g.
	 * power resources) so we need to be careful how we use it.
	 */
	switch (type) {
	case ACPI_BUS_TYPE_DEVICE:
		result = acpi_bus_get_status(device);
		if (result)
			goto end;
		break;
	default:
		STRUCT_TO_INT(device->status) = 0x0F;
		break;
	}
	if (!device->status.present) {
		result = -ENOENT;
		goto end;
	}

	/*
	 * Initialize Device
	 * -----------------
	 * TBD: Synch with Core's enumeration/initialization process.
	 */

	/*
	 * Hardware ID, Unique ID, & Bus Address
	 * -------------------------------------
	 */
	switch (type) {
	case ACPI_BUS_TYPE_DEVICE:
		buffer.length = ACPI_ALLOCATE_BUFFER;
		buffer.pointer = NULL;
		status = acpi_get_object_info(handle, &buffer);
		if (ACPI_FAILURE(status)) {
			ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
				"Error reading device info\n"));
			result = -ENODEV;
			goto end;
		}
		info = buffer.pointer;
		if (info->valid & ACPI_VALID_HID)
			hid = info->hardware_id.value;
		if (info->valid & ACPI_VALID_UID)
			uid = info->unique_id.value;
		if (info->valid & ACPI_VALID_CID) {
			cid_list = &info->compatibility_id;
			device->pnp.cid_list = kmalloc(cid_list->size, GFP_KERNEL);
			if (device->pnp.cid_list)
				memcpy(device->pnp.cid_list, cid_list, cid_list->size);
			else
				ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "Memory allocation error\n"));
		}
		if (info->valid & ACPI_VALID_ADR) {
			device->pnp.bus_address = info->address;
			device->flags.bus_address = 1;
		}
		break;
	case ACPI_BUS_TYPE_POWER:
		hid = ACPI_POWER_HID;
		break;
	case ACPI_BUS_TYPE_PROCESSOR:
		hid = ACPI_PROCESSOR_HID;
		break;
	case ACPI_BUS_TYPE_SYSTEM:
		hid = ACPI_SYSTEM_HID;
		break;
	case ACPI_BUS_TYPE_THERMAL:
		hid = ACPI_THERMAL_HID;
		break;
	case ACPI_BUS_TYPE_POWER_BUTTON:
		hid = ACPI_BUTTON_HID_POWERF;
		break;
	case ACPI_BUS_TYPE_SLEEP_BUTTON:
		hid = ACPI_BUTTON_HID_SLEEPF;
		break;
	}

	/* 
	 * \_SB
	 * ----
	 * Fix for the system root bus device -- the only root-level device.
	 */
	if ((parent == ACPI_ROOT_OBJECT) && (type == ACPI_BUS_TYPE_DEVICE)) {
		hid = ACPI_BUS_HID;
		sprintf(device->pnp.device_name, "%s", ACPI_BUS_DEVICE_NAME);
		sprintf(device->pnp.device_class, "%s", ACPI_BUS_CLASS);
	}

	if (hid) {
		sprintf(device->pnp.hardware_id, "%s", hid);
		device->flags.hardware_id = 1;
	}
	if (uid) {
		sprintf(device->pnp.unique_id, "%s", uid);
		device->flags.unique_id = 1;
	}

	/*
	 * If we called get_object_info, we now are finished with the buffer,
	 * so we can free it.
	 */
	if (buffer.pointer)
		acpi_os_free(buffer.pointer);

	/*
	 * Power Management
	 * ----------------
	 */
	if (device->flags.power_manageable) {
		result = acpi_bus_get_power_flags(device);
		if (result)
			goto end;
	}

	/*
	 * Performance Management
	 * ----------------------
	 */
	if (device->flags.performance_manageable) {
		result = acpi_bus_get_perf_flags(device);
		if (result)
			goto end;
	}

	/*
	 * Context
	 * -------
	 * Attach this 'struct acpi_device' to the ACPI object.  This makes
	 * resolutions from handle->device very efficient.  Note that we need
	 * to be careful with fixed-feature devices as they all attach to the
	 * root object.
	 */
	switch (type) {
	case ACPI_BUS_TYPE_POWER_BUTTON:
	case ACPI_BUS_TYPE_SLEEP_BUTTON:
		break;
	default:
		status = acpi_attach_data(device->handle,
			acpi_bus_data_handler, device);
		break;
	}
	if (ACPI_FAILURE(status)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Error attaching device data\n"));
		result = -ENODEV;
		goto end;
	}

	/*
	 * Linkage
	 * -------
	 * Link this device to its parent and siblings.
	 */
	INIT_LIST_HEAD(&device->children);
	if (!device->parent)
		INIT_LIST_HEAD(&device->node);
	else
		list_add_tail(&device->node, &device->parent->children);

#ifdef CONFIG_ACPI_DEBUG
	{
		char			*type_string = NULL;
		char			name[80] = {'?','\0'};
		struct acpi_buffer	buffer = {sizeof(name), name};

		acpi_get_name(handle, ACPI_FULL_PATHNAME, &buffer);

		switch (type) {
		case ACPI_BUS_TYPE_DEVICE:
			type_string = "Device";
			break;
		case ACPI_BUS_TYPE_POWER:
			type_string = "Power Resource";
			break;
		case ACPI_BUS_TYPE_PROCESSOR:
			type_string = "Processor";
			break;
		case ACPI_BUS_TYPE_SYSTEM:
			type_string = "System";
			break;
		case ACPI_BUS_TYPE_THERMAL:
			type_string = "Thermal Zone";
			break;
		case ACPI_BUS_TYPE_POWER_BUTTON:
			type_string = "Power Button";
			sprintf(name, "PWRB");
			break;
		case ACPI_BUS_TYPE_SLEEP_BUTTON:
			type_string = "Sleep Button";
			sprintf(name, "SLPB");
			break;
		}

		ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Found %s %s [%p]\n", 
			type_string, name, handle));
	}
#endif /*CONFIG_ACPI_DEBUG*/

	/*
	 * Global Device Hierarchy:
	 * ------------------------
	 * Register this device with the global device hierarchy.
	 */
	acpi_device_register(device, parent);

	/*
	 * Bind _ADR-Based Devices
	 * -----------------------
	 * If there's a a bus address (_ADR) then we utilize the parent's 
	 * 'bind' function (if exists) to bind the ACPI- and natively-
	 * enumerated device representations.
	 */
	if (device->flags.bus_address) {
		if (device->parent && device->parent->ops.bind)
			device->parent->ops.bind(device);
	}

	/*
	 * Locate & Attach Driver
	 * ----------------------
	 * If there's a hardware id (_HID) or compatible ids (_CID) we check
	 * to see if there's a driver installed for this kind of device.  Note
	 * that drivers can install before or after a device is enumerated.
	 *
	 * TBD: Assumes LDM provides driver hot-plug capability.
	 */
	if (device->flags.hardware_id || device->flags.compatible_ids)
		acpi_bus_find_driver(device);

end:
	if (result) {
		if (device->pnp.cid_list) {
			kfree(device->pnp.cid_list);
		}
		kfree(device);
		return_VALUE(result);
	}
	*child = device;

	return_VALUE(0);
}


static int
acpi_bus_remove (
	struct acpi_device	*device, 
	int			type)
{
	ACPI_FUNCTION_TRACE("acpi_bus_remove");

	if (!device)
		return_VALUE(-ENODEV);

	acpi_device_unregister(device);

#if 0
	if (device->pnp.cid_list)
		kfree(device->pnp.cid_list);
#endif
	kfree(device);

	return_VALUE(0);
}


int
acpi_bus_scan (
	struct acpi_device	*start)
{
	acpi_status		status = AE_OK;
	struct acpi_device	*parent = NULL;
	struct acpi_device	*child = NULL;
	acpi_handle		phandle = 0;
	acpi_handle		chandle = 0;
	acpi_object_type	type = 0;
	u32			level = 1;

	ACPI_FUNCTION_TRACE("acpi_bus_scan");

	if (!start)
		return_VALUE(-EINVAL);

	parent = start;
	phandle = start->handle;
	
	/*
	 * Parse through the ACPI namespace, identify all 'devices', and
	 * create a new 'struct acpi_device' for each.
	 */
	while ((level > 0) && parent) {

		status = acpi_get_next_object(ACPI_TYPE_ANY, phandle,
			chandle, &chandle);

		/*
		 * If this scope is exhausted then move our way back up.
		 */
		if (ACPI_FAILURE(status)) {
			level--;
			chandle = phandle;
			acpi_get_parent(phandle, &phandle);
			if (parent->parent)
				parent = parent->parent;
			continue;
		}

		status = acpi_get_type(chandle, &type);
		if (ACPI_FAILURE(status))
			continue;

		/*
		 * If this is a scope object then parse it (depth-first).
		 */
		if (type == ACPI_TYPE_LOCAL_SCOPE) {
			level++;
			phandle = chandle;
			chandle = 0;
			continue;
		}

		/*
		 * We're only interested in objects that we consider 'devices'.
		 */
		switch (type) {
		case ACPI_TYPE_DEVICE:
			type = ACPI_BUS_TYPE_DEVICE;
			break;
		case ACPI_TYPE_PROCESSOR:
			type = ACPI_BUS_TYPE_PROCESSOR;
			break;
		case ACPI_TYPE_THERMAL:
			type = ACPI_BUS_TYPE_THERMAL;
			break;
		case ACPI_TYPE_POWER:
			type = ACPI_BUS_TYPE_POWER;
			break;
		default:
			continue;
		}

		status = acpi_bus_add(&child, parent, chandle, type);
		if (ACPI_FAILURE(status))
			continue;

		/*
		 * If the device is present, enabled, and functioning then
		 * parse its scope (depth-first).  Note that we need to
		 * represent absent devices to facilitate PnP notifications
		 * -- but only the subtree head (not all of its children,
		 * which will be enumerated when the parent is inserted).
		 *
		 * TBD: Need notifications and other detection mechanisms
		 *	in place before we can fully implement this.
		 */
		if (child->status.present) {
			status = acpi_get_next_object(ACPI_TYPE_ANY, chandle,
				0, NULL);
			if (ACPI_SUCCESS(status)) {
				level++;
				phandle = chandle;
				chandle = 0;
				parent = child;
			}
		}
	}

	return_VALUE(0);
}


static int
acpi_bus_scan_fixed (
	struct acpi_device	*root)
{
	int			result = 0;
	struct acpi_device	*device = NULL;

	ACPI_FUNCTION_TRACE("acpi_bus_scan_fixed");

	if (!root)
		return_VALUE(-ENODEV);

	/*
	 * Enumerate all fixed-feature devices.
	 */
	if (acpi_fadt.pwr_button == 0)
		result = acpi_bus_add(&device, acpi_root, 
			NULL, ACPI_BUS_TYPE_POWER_BUTTON);

	if (acpi_fadt.sleep_button == 0)
		result = acpi_bus_add(&device, acpi_root, 
			NULL, ACPI_BUS_TYPE_SLEEP_BUTTON);

	return_VALUE(result);
}


/* --------------------------------------------------------------------------
                             Initialization/Cleanup
   -------------------------------------------------------------------------- */

static int __init
acpi_bus_init_irq (void)
{
	acpi_status		status = AE_OK;
	union acpi_object	arg = {ACPI_TYPE_INTEGER};
	struct acpi_object_list	arg_list = {1, &arg};
	char			*message = NULL;

	ACPI_FUNCTION_TRACE("acpi_bus_init_irq");

	/* 
	 * Let the system know what interrupt model we are using by
	 * evaluating the \_PIC object, if exists.
	 */

	switch (acpi_irq_model) {
	case ACPI_IRQ_MODEL_PIC:
		message = "PIC";
		break;
	case ACPI_IRQ_MODEL_IOAPIC:
		message = "IOAPIC";
		break;
	case ACPI_IRQ_MODEL_IOSAPIC:
		message = "IOSAPIC";
		break;
	default:
		printk(KERN_WARNING PREFIX "Unknown interrupt routing model\n");
		return_VALUE(-ENODEV);
	}

	printk(KERN_INFO PREFIX "Using %s for interrupt routing\n", message);

	arg.integer.value = acpi_irq_model;

	status = acpi_evaluate_object(NULL, "\\_PIC", &arg_list, NULL);
	if (ACPI_FAILURE(status) && (status != AE_NOT_FOUND)) {
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR, "Error evaluating _PIC\n"));
		return_VALUE(-ENODEV);
	}

	return_VALUE(0);
}


static int __init
acpi_bus_init (void)
{
	int			result = 0;
	acpi_status		status = AE_OK;
	struct acpi_buffer	buffer = {sizeof(acpi_fadt), &acpi_fadt};

	ACPI_FUNCTION_TRACE("acpi_bus_init");

	status = acpi_initialize_subsystem();
	if (ACPI_FAILURE(status)) {
		printk(KERN_ERR PREFIX "Unable to initialize the ACPI Interpreter\n");
		goto error0;
	}

	status = acpi_load_tables();
	if (ACPI_FAILURE(status)) {
		printk(KERN_ERR PREFIX "Unable to load the System Description Tables\n");
		goto error0;
	}

	/*
	 * Get a separate copy of the FADT for use by other drivers.
	 */
	status = acpi_get_table(ACPI_TABLE_FADT, 1, &buffer);
	if (ACPI_FAILURE(status)) {
		printk(KERN_ERR PREFIX "Unable to get the FADT\n");
		goto error1;
	}

#ifdef CONFIG_X86
	if (!acpi_ioapic) {
		extern acpi_interrupt_flags acpi_sci_flags;

		/* compatible (0) means level (3) */
		if (acpi_sci_flags.trigger == 0)
			acpi_sci_flags.trigger = 3;

		/* Set PIC-mode SCI trigger type */
		acpi_pic_sci_set_trigger(acpi_fadt.sci_int, acpi_sci_flags.trigger);
	} else {
		extern int acpi_sci_override_gsi;
		/*
		 * now that acpi_fadt is initialized,
		 * update it with result from INT_SRC_OVR parsing
		 */
		acpi_fadt.sci_int = acpi_sci_override_gsi;
	}
#endif

	status = acpi_enable_subsystem(ACPI_FULL_INITIALIZATION);
	if (ACPI_FAILURE(status)) {
		printk(KERN_ERR PREFIX "Unable to start the ACPI Interpreter\n");
		goto error1;
	}

#ifdef CONFIG_ACPI_EC
	/*
	 * ACPI 2.0 requires the EC driver to be loaded and work before
	 * the EC device is found in the namespace. This is accomplished
	 * by looking for the ECDT table, and getting the EC parameters out
	 * of that.
	 */
	result = acpi_ec_ecdt_probe();
	/* Ignore result. Not having an ECDT is not fatal. */
#endif

	status = acpi_initialize_objects(ACPI_FULL_INITIALIZATION);
	if (ACPI_FAILURE(status)) {
		printk(KERN_ERR PREFIX "Unable to initialize ACPI objects\n");
		goto error1;
	}

	printk(KERN_INFO PREFIX "Interpreter enabled\n");

	/*
	 * Get the system interrupt model and evaluate \_PIC.
	 */
	result = acpi_bus_init_irq();
	if (result)
		goto error1;

	/*
	 * Register the for all standard device notifications.
	 */
	status = acpi_install_notify_handler(ACPI_ROOT_OBJECT, ACPI_SYSTEM_NOTIFY, &acpi_bus_notify, NULL);
	if (ACPI_FAILURE(status)) {
		printk(KERN_ERR PREFIX "Unable to register for device notifications\n");
		result = -ENODEV;
		goto error1;
	}

	/*
	 * Create the root device in the bus's device tree
	 */
	result = acpi_bus_add(&acpi_root, NULL, ACPI_ROOT_OBJECT, 
		ACPI_BUS_TYPE_SYSTEM);
	if (result)
		goto error2;

	/*
	 * Create the top ACPI proc directory
	 */
	acpi_device_dir(acpi_root) = proc_mkdir(ACPI_BUS_FILE_ROOT, NULL);
	if (!acpi_root) {
		result = -ENODEV;
		goto error3;
	}
	acpi_root_dir = acpi_device_dir(acpi_root);

	/*
	 * Install drivers required for proper enumeration of the
	 * ACPI namespace.
	 */
	acpi_system_init();	/* ACPI System */
	acpi_power_init();	/* ACPI Bus Power Management */
#ifdef CONFIG_ACPI_EC
	acpi_ec_init();		/* ACPI Embedded Controller */
#endif
#ifdef CONFIG_ACPI_PCI
	acpi_pci_link_init();	/* ACPI PCI Interrupt Link */
	acpi_pci_root_init();	/* ACPI PCI Root Bridge */
#endif
	/*
	 * Enumerate devices in the ACPI namespace.
	 */
	result = acpi_bus_scan_fixed(acpi_root);
	if (result)
		goto error4;
	result = acpi_bus_scan(acpi_root);
	if (result)
		goto error4;

	return_VALUE(0);

	/* Mimic structured exception handling */
error4:
	remove_proc_entry(ACPI_BUS_FILE_ROOT, NULL);
error3:
	acpi_bus_remove(acpi_root, ACPI_BUS_REMOVAL_NORMAL);
error2:
	acpi_remove_notify_handler(ACPI_ROOT_OBJECT,
		ACPI_SYSTEM_NOTIFY, &acpi_bus_notify);
error1:
	acpi_terminate();
error0:
	return_VALUE(-ENODEV);
}


static void __exit
acpi_bus_exit (void)
{
	acpi_status		status = AE_OK;

	ACPI_FUNCTION_TRACE("acpi_bus_exit");

	status = acpi_remove_notify_handler(ACPI_ROOT_OBJECT,
		ACPI_SYSTEM_NOTIFY, acpi_bus_notify);
	if (ACPI_FAILURE(status))
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Error removing notify handler\n"));

#ifdef CONFIG_ACPI_PCI
	acpi_pci_root_exit();
	acpi_pci_link_exit();
#endif
#ifdef CONFIG_ACPI_EC
	acpi_ec_exit();
#endif
	acpi_power_exit();
	acpi_system_exit();

	acpi_bus_remove(acpi_root, ACPI_BUS_REMOVAL_NORMAL);

	remove_proc_entry(ACPI_BUS_FILE_ROOT, NULL);

	status = acpi_terminate();
	if (ACPI_FAILURE(status))
		printk(KERN_ERR PREFIX "Unable to terminate the ACPI Interpreter\n");
	else
		printk(KERN_ERR PREFIX "Interpreter disabled\n");

	return_VOID;
}


int __init
acpi_init (void)
{
	int			result = 0;

	ACPI_FUNCTION_TRACE("acpi_init");

	printk(KERN_INFO PREFIX "Subsystem revision %08x\n",
		ACPI_CA_VERSION);

	/* Initial core debug level excludes drivers, so include them now */
	acpi_set_debug(ACPI_DEBUG_LOW);

	if (acpi_disabled) {
		printk(KERN_INFO PREFIX "Interpreter disabled.\n");
		return -ENODEV;
	}

#ifdef CONFIG_PM
	if (PM_IS_ACTIVE()) {
		printk(KERN_INFO PREFIX "APM is already active, exiting\n");
		return -ENODEV;
	}
#endif

	result = acpi_bus_init();
	if (result) {
		disable_acpi();
		return_VALUE(result);
	}

#ifdef CONFIG_PM
	pm_active = 1;
#endif

	return_VALUE(0);
}


void __exit
acpi_exit (void)
{
	ACPI_FUNCTION_TRACE("acpi_exit");

#ifdef CONFIG_PM
	pm_active = 0;
#endif

	acpi_bus_exit();

	return_VOID;
}

