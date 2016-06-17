/******************************************************************************
 *
 * Module Name: evxfregn - External Interfaces, ACPI Operation Regions and
 *                         Address Spaces.
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2004, R. Byron Moore
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */


#include <acpi/acpi.h>
#include <acpi/acnamesp.h>
#include <acpi/acevents.h>
#include <acpi/acinterp.h>

#define _COMPONENT          ACPI_EVENTS
	 ACPI_MODULE_NAME    ("evxfregn")


/*******************************************************************************
 *
 * FUNCTION:    acpi_install_address_space_handler
 *
 * PARAMETERS:  Device          - Handle for the device
 *              space_id        - The address space ID
 *              Handler         - Address of the handler
 *              Setup           - Address of the setup function
 *              Context         - Value passed to the handler on each access
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install a handler for all op_regions of a given space_id.
 *
 ******************************************************************************/

acpi_status
acpi_install_address_space_handler (
	acpi_handle                     device,
	acpi_adr_space_type             space_id,
	acpi_adr_space_handler          handler,
	acpi_adr_space_setup            setup,
	void                            *context)
{
	union acpi_operand_object       *obj_desc;
	union acpi_operand_object       *handler_obj;
	struct acpi_namespace_node      *node;
	acpi_status                     status;
	acpi_object_type                type;
	u16                             flags = 0;


	ACPI_FUNCTION_TRACE ("acpi_install_address_space_handler");


	/* Parameter validation */

	if (!device) {
		return_ACPI_STATUS (AE_BAD_PARAMETER);
	}

	status = acpi_ut_acquire_mutex (ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE (status)) {
		return_ACPI_STATUS (status);
	}

	/* Convert and validate the device handle */

	node = acpi_ns_map_handle_to_node (device);
	if (!node) {
		status = AE_BAD_PARAMETER;
		goto unlock_and_exit;
	}

	/*
	 * This registration is valid for only the types below
	 * and the root.  This is where the default handlers
	 * get placed.
	 */
	if ((node->type != ACPI_TYPE_DEVICE)     &&
		(node->type != ACPI_TYPE_PROCESSOR)  &&
		(node->type != ACPI_TYPE_THERMAL)    &&
		(node != acpi_gbl_root_node)) {
		status = AE_BAD_PARAMETER;
		goto unlock_and_exit;
	}

	if (handler == ACPI_DEFAULT_HANDLER) {
		flags = ACPI_ADDR_HANDLER_DEFAULT_INSTALLED;

		switch (space_id) {
		case ACPI_ADR_SPACE_SYSTEM_MEMORY:
			handler = acpi_ex_system_memory_space_handler;
			setup   = acpi_ev_system_memory_region_setup;
			break;

		case ACPI_ADR_SPACE_SYSTEM_IO:
			handler = acpi_ex_system_io_space_handler;
			setup   = acpi_ev_io_space_region_setup;
			break;

		case ACPI_ADR_SPACE_PCI_CONFIG:
			handler = acpi_ex_pci_config_space_handler;
			setup   = acpi_ev_pci_config_region_setup;
			break;

		case ACPI_ADR_SPACE_CMOS:
			handler = acpi_ex_cmos_space_handler;
			setup   = acpi_ev_cmos_region_setup;
			break;

		case ACPI_ADR_SPACE_PCI_BAR_TARGET:
			handler = acpi_ex_pci_bar_space_handler;
			setup   = acpi_ev_pci_bar_region_setup;
			break;

		case ACPI_ADR_SPACE_DATA_TABLE:
			handler = acpi_ex_data_table_space_handler;
			setup   = NULL;
			break;

		default:
			status = AE_BAD_PARAMETER;
			goto unlock_and_exit;
		}
	}

	/* If the caller hasn't specified a setup routine, use the default */

	if (!setup) {
		setup = acpi_ev_default_region_setup;
	}

	/* Check for an existing internal object */

	obj_desc = acpi_ns_get_attached_object (node);
	if (obj_desc) {
		/*
		 * The attached device object already exists.
		 * Make sure the handler is not already installed.
		 */
		handler_obj = obj_desc->device.handler;

		/* Walk the handler list for this device */

		while (handler_obj) {
			/* Same space_id indicates a handler already installed */

			if(handler_obj->address_space.space_id == space_id) {
				if (handler_obj->address_space.handler == handler) {
					/*
					 * It is (relatively) OK to attempt to install the SAME
					 * handler twice. This can easily happen with PCI_Config space.
					 */
					status = AE_SAME_HANDLER;
					goto unlock_and_exit;
				}
				else {
					/* A handler is already installed */

					status = AE_ALREADY_EXISTS;
				}
				goto unlock_and_exit;
			}

			/* Walk the linked list of handlers */

			handler_obj = handler_obj->address_space.next;
		}
	}
	else {
		ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
			"Creating object on Device %p while installing handler\n", node));

		/* obj_desc does not exist, create one */

		if (node->type == ACPI_TYPE_ANY) {
			type = ACPI_TYPE_DEVICE;
		}
		else {
			type = node->type;
		}

		obj_desc = acpi_ut_create_internal_object (type);
		if (!obj_desc) {
			status = AE_NO_MEMORY;
			goto unlock_and_exit;
		}

		/* Init new descriptor */

		obj_desc->common.type = (u8) type;

		/* Attach the new object to the Node */

		status = acpi_ns_attach_object (node, obj_desc, type);

		/* Remove local reference to the object */

		acpi_ut_remove_reference (obj_desc);

		if (ACPI_FAILURE (status)) {
			goto unlock_and_exit;
		}
	}

	ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
		"Installing address handler for region %s(%X) on Device %4.4s %p(%p)\n",
		acpi_ut_get_region_name (space_id), space_id,
		acpi_ut_get_node_name (node), node, obj_desc));

	/*
	 * Install the handler
	 *
	 * At this point there is no existing handler.
	 * Just allocate the object for the handler and link it
	 * into the list.
	 */
	handler_obj = acpi_ut_create_internal_object (ACPI_TYPE_LOCAL_ADDRESS_HANDLER);
	if (!handler_obj) {
		status = AE_NO_MEMORY;
		goto unlock_and_exit;
	}

	/* Init handler obj */

	handler_obj->address_space.space_id  = (u8) space_id;
	handler_obj->address_space.hflags    = flags;
	handler_obj->address_space.region_list = NULL;
	handler_obj->address_space.node      = node;
	handler_obj->address_space.handler   = handler;
	handler_obj->address_space.context   = context;
	handler_obj->address_space.setup     = setup;

	/* Install at head of Device.address_space list */

	handler_obj->address_space.next      = obj_desc->device.handler;

	/*
	 * The Device object is the first reference on the handler_obj.
	 * Each region that uses the handler adds a reference.
	 */
	obj_desc->device.handler = handler_obj;

	/*
	 * Walk the namespace finding all of the regions this
	 * handler will manage.
	 *
	 * Start at the device and search the branch toward
	 * the leaf nodes until either the leaf is encountered or
	 * a device is detected that has an address handler of the
	 * same type.
	 *
	 * In either case, back up and search down the remainder
	 * of the branch
	 */
	status = acpi_ns_walk_namespace (ACPI_TYPE_ANY, device, ACPI_UINT32_MAX,
			  ACPI_NS_WALK_UNLOCK, acpi_ev_install_handler,
			  handler_obj, NULL);

	/*
	 * Now we can run the _REG methods for all Regions for this
	 * space ID.  This is a separate walk in order to handle any
	 * interdependencies between regions and _REG methods.  (i.e. handlers
	 * must be installed for all regions of this Space ID before we
	 * can run any _REG methods.
	 */
	status = acpi_ns_walk_namespace (ACPI_TYPE_ANY, device, ACPI_UINT32_MAX,
			  ACPI_NS_WALK_UNLOCK, acpi_ev_reg_run,
			  handler_obj, NULL);

unlock_and_exit:
	(void) acpi_ut_release_mutex (ACPI_MTX_NAMESPACE);
	return_ACPI_STATUS (status);
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_remove_address_space_handler
 *
 * PARAMETERS:  Device          - Handle for the device
 *              space_id        - The address space ID
 *              Handler         - Address of the handler
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Remove a previously installed handler.
 *
 ******************************************************************************/

acpi_status
acpi_remove_address_space_handler (
	acpi_handle                     device,
	acpi_adr_space_type             space_id,
	acpi_adr_space_handler          handler)
{
	union acpi_operand_object       *obj_desc;
	union acpi_operand_object       *handler_obj;
	union acpi_operand_object       *region_obj;
	union acpi_operand_object       **last_obj_ptr;
	struct acpi_namespace_node      *node;
	acpi_status                     status;


	ACPI_FUNCTION_TRACE ("acpi_remove_address_space_handler");


	/* Parameter validation */

	if (!device) {
		return_ACPI_STATUS (AE_BAD_PARAMETER);
	}

	status = acpi_ut_acquire_mutex (ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE (status)) {
		return_ACPI_STATUS (status);
	}

	/* Convert and validate the device handle */

	node = acpi_ns_map_handle_to_node (device);
	if (!node) {
		status = AE_BAD_PARAMETER;
		goto unlock_and_exit;
	}

	/* Make sure the internal object exists */

	obj_desc = acpi_ns_get_attached_object (node);
	if (!obj_desc) {
		status = AE_NOT_EXIST;
		goto unlock_and_exit;
	}

	/* Find the address handler the user requested */

	handler_obj = obj_desc->device.handler;
	last_obj_ptr = &obj_desc->device.handler;
	while (handler_obj) {
		/* We have a handler, see if user requested this one */

		if (handler_obj->address_space.space_id == space_id) {
			/* Matched space_id, first dereference this in the Regions */

			ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
				"Removing address handler %p(%p) for region %s on Device %p(%p)\n",
				handler_obj, handler, acpi_ut_get_region_name (space_id),
				node, obj_desc));

			region_obj = handler_obj->address_space.region_list;

			/* Walk the handler's region list */

			while (region_obj) {
				/*
				 * First disassociate the handler from the region.
				 *
				 * NOTE: this doesn't mean that the region goes away
				 * The region is just inaccessible as indicated to
				 * the _REG method
				 */
				acpi_ev_detach_region (region_obj, TRUE);

				/*
				 * Walk the list: Just grab the head because the
				 * detach_region removed the previous head.
				 */
				region_obj = handler_obj->address_space.region_list;

			}

			/* Remove this Handler object from the list */

			*last_obj_ptr = handler_obj->address_space.next;

			/* Now we can delete the handler object */

			acpi_ut_remove_reference (handler_obj);
			goto unlock_and_exit;
		}

		/* Walk the linked list of handlers */

		last_obj_ptr = &handler_obj->address_space.next;
		handler_obj = handler_obj->address_space.next;
	}

	/* The handler does not exist */

	ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
		"Unable to remove address handler %p for %s(%X), dev_node %p, obj %p\n",
		handler, acpi_ut_get_region_name (space_id), space_id, node, obj_desc));

	status = AE_NOT_EXIST;

unlock_and_exit:
	(void) acpi_ut_release_mutex (ACPI_MTX_NAMESPACE);
	return_ACPI_STATUS (status);
}


