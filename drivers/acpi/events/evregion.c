/******************************************************************************
 *
 * Module Name: evregion - ACPI address_space (op_region) handler dispatch
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
#include <acpi/acevents.h>
#include <acpi/acnamesp.h>
#include <acpi/acinterp.h>

#define _COMPONENT          ACPI_EVENTS
	 ACPI_MODULE_NAME    ("evregion")

#define ACPI_NUM_DEFAULT_SPACES     4

static u8                   acpi_gbl_default_address_spaces[ACPI_NUM_DEFAULT_SPACES] = {
			 ACPI_ADR_SPACE_SYSTEM_MEMORY,
			 ACPI_ADR_SPACE_SYSTEM_IO,
			 ACPI_ADR_SPACE_PCI_CONFIG,
			 ACPI_ADR_SPACE_DATA_TABLE};


/*******************************************************************************
 *
 * FUNCTION:    acpi_ev_init_address_spaces
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Installs the core subsystem default address space handlers.
 *
 ******************************************************************************/

acpi_status
acpi_ev_init_address_spaces (
	void) {
	acpi_status                     status;
	acpi_native_uint                i;


	ACPI_FUNCTION_TRACE ("ev_init_address_spaces");


	/*
	 * All address spaces (PCI Config, EC, SMBus) are scope dependent
	 * and registration must occur for a specific device.
	 *
	 * In the case of the system memory and IO address spaces there is currently
	 * no device associated with the address space.  For these we use the root.
	 *
	 * We install the default PCI config space handler at the root so
	 * that this space is immediately available even though the we have
	 * not enumerated all the PCI Root Buses yet.  This is to conform
	 * to the ACPI specification which states that the PCI config
	 * space must be always available -- even though we are nowhere
	 * near ready to find the PCI root buses at this point.
	 *
	 * NOTE: We ignore AE_ALREADY_EXISTS because this means that a handler
	 * has already been installed (via acpi_install_address_space_handler).
	 * Similar for AE_SAME_HANDLER.
	 */

	for (i = 0; i < ACPI_NUM_DEFAULT_SPACES; i++) {
		status = acpi_install_address_space_handler ((acpi_handle) acpi_gbl_root_node,
				  acpi_gbl_default_address_spaces[i],
				  ACPI_DEFAULT_HANDLER, NULL, NULL);
		switch (status) {
		case AE_OK:
		case AE_SAME_HANDLER:
		case AE_ALREADY_EXISTS:

			/* These exceptions are all OK */

			break;

		default:

			return_ACPI_STATUS (status);
		}
	}

	return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ev_execute_reg_method
 *
 * PARAMETERS:  region_obj          - Object structure
 *              Function            - On (1) or Off (0)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute _REG method for a region
 *
 ******************************************************************************/

acpi_status
acpi_ev_execute_reg_method (
	union acpi_operand_object      *region_obj,
	u32                             function)
{
	union acpi_operand_object      *params[3];
	union acpi_operand_object      *region_obj2;
	acpi_status                     status;


	ACPI_FUNCTION_TRACE ("ev_execute_reg_method");


	region_obj2 = acpi_ns_get_secondary_object (region_obj);
	if (!region_obj2) {
		return_ACPI_STATUS (AE_NOT_EXIST);
	}

	if (region_obj2->extra.method_REG == NULL) {
		return_ACPI_STATUS (AE_OK);
	}

	/*
	 * _REG method has two arguments
	 * Arg0:   Integer: Operation region space ID
	 *          Same value as region_obj->Region.space_id
	 * Arg1:   Integer: connection status
	 *          1 for connecting the handler,
	 *          0 for disconnecting the handler
	 *          Passed as a parameter
	 */
	params[0] = acpi_ut_create_internal_object (ACPI_TYPE_INTEGER);
	if (!params[0]) {
		return_ACPI_STATUS (AE_NO_MEMORY);
	}

	params[1] = acpi_ut_create_internal_object (ACPI_TYPE_INTEGER);
	if (!params[1]) {
		status = AE_NO_MEMORY;
		goto cleanup;
	}

	/* Set up the parameter objects */

	params[0]->integer.value = region_obj->region.space_id;
	params[1]->integer.value = function;
	params[2] = NULL;

	/* Execute the method, no return value */

	ACPI_DEBUG_EXEC(acpi_ut_display_init_pathname (ACPI_TYPE_METHOD, region_obj2->extra.method_REG, NULL));
	status = acpi_ns_evaluate_by_handle (region_obj2->extra.method_REG, params, NULL);

	acpi_ut_remove_reference (params[1]);

cleanup:
	acpi_ut_remove_reference (params[0]);

	return_ACPI_STATUS (status);
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ev_address_space_dispatch
 *
 * PARAMETERS:  region_obj          - Internal region object
 *              space_id            - ID of the address space (0-255)
 *              Function            - Read or Write operation
 *              Address             - Where in the space to read or write
 *              bit_width           - Field width in bits (8, 16, 32, or 64)
 *              Value               - Pointer to in or out value
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Dispatch an address space or operation region access to
 *              a previously installed handler.
 *
 ******************************************************************************/

acpi_status
acpi_ev_address_space_dispatch (
	union acpi_operand_object       *region_obj,
	u32                             function,
	acpi_physical_address           address,
	u32                             bit_width,
	void                            *value)
{
	acpi_status                     status;
	acpi_status                     status2;
	acpi_adr_space_handler          handler;
	acpi_adr_space_setup            region_setup;
	union acpi_operand_object       *handler_desc;
	union acpi_operand_object       *region_obj2;
	void                            *region_context = NULL;


	ACPI_FUNCTION_TRACE ("ev_address_space_dispatch");


	region_obj2 = acpi_ns_get_secondary_object (region_obj);
	if (!region_obj2) {
		return_ACPI_STATUS (AE_NOT_EXIST);
	}

	/* Ensure that there is a handler associated with this region */

	handler_desc = region_obj->region.handler;
	if (!handler_desc) {
		ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
			"No handler for Region [%4.4s] (%p) [%s]\n",
			acpi_ut_get_node_name (region_obj->region.node),
			region_obj, acpi_ut_get_region_name (region_obj->region.space_id)));

		return_ACPI_STATUS (AE_NOT_EXIST);
	}

	/*
	 * It may be the case that the region has never been initialized
	 * Some types of regions require special init code
	 */
	if (!(region_obj->region.flags & AOPOBJ_SETUP_COMPLETE)) {
		/*
		 * This region has not been initialized yet, do it
		 */
		region_setup = handler_desc->address_space.setup;
		if (!region_setup) {
			/* No initialization routine, exit with error */

			ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "No init routine for region(%p) [%s]\n",
				region_obj, acpi_ut_get_region_name (region_obj->region.space_id)));
			return_ACPI_STATUS (AE_NOT_EXIST);
		}

		/*
		 * We must exit the interpreter because the region setup will potentially
		 * execute control methods (e.g., _REG method for this region)
		 */
		acpi_ex_exit_interpreter ();

		status = region_setup (region_obj, ACPI_REGION_ACTIVATE,
				  handler_desc->address_space.context, &region_context);

		/* Re-enter the interpreter */

		status2 = acpi_ex_enter_interpreter ();
		if (ACPI_FAILURE (status2)) {
			return_ACPI_STATUS (status2);
		}

		/* Check for failure of the Region Setup */

		if (ACPI_FAILURE (status)) {
			ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Region Init: %s [%s]\n",
				acpi_format_exception (status),
				acpi_ut_get_region_name (region_obj->region.space_id)));
			return_ACPI_STATUS (status);
		}

		/*
		 * Region initialization may have been completed by region_setup
		 */
		if (!(region_obj->region.flags & AOPOBJ_SETUP_COMPLETE)) {
			region_obj->region.flags |= AOPOBJ_SETUP_COMPLETE;

			if (region_obj2->extra.region_context) {
				/* The handler for this region was already installed */

				ACPI_MEM_FREE (region_context);
			}
			else {
				/*
				 * Save the returned context for use in all accesses to
				 * this particular region
				 */
				region_obj2->extra.region_context = region_context;
			}
		}
	}

	/* We have everything we need, we can invoke the address space handler */

	handler = handler_desc->address_space.handler;

	ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
		"Handler %p (@%p) Address %8.8X%8.8X [%s]\n",
		&region_obj->region.handler->address_space, handler,
		ACPI_FORMAT_UINT64 (address),
		acpi_ut_get_region_name (region_obj->region.space_id)));

	if (!(handler_desc->address_space.flags & ACPI_ADDR_HANDLER_DEFAULT_INSTALLED)) {
		/*
		 * For handlers other than the default (supplied) handlers, we must
		 * exit the interpreter because the handler *might* block -- we don't
		 * know what it will do, so we can't hold the lock on the intepreter.
		 */
		acpi_ex_exit_interpreter();
	}

	/* Call the handler */

	status = handler (function, address, bit_width, value,
			 handler_desc->address_space.context,
			 region_obj2->extra.region_context);

	if (ACPI_FAILURE (status)) {
		ACPI_REPORT_ERROR (("Handler for [%s] returned %s\n",
			acpi_ut_get_region_name (region_obj->region.space_id),
			acpi_format_exception (status)));
	}

	if (!(handler_desc->address_space.flags & ACPI_ADDR_HANDLER_DEFAULT_INSTALLED)) {
		/*
		 * We just returned from a non-default handler, we must re-enter the
		 * interpreter
		 */
		status2 = acpi_ex_enter_interpreter ();
		if (ACPI_FAILURE (status2)) {
			return_ACPI_STATUS (status2);
		}
	}

	return_ACPI_STATUS (status);
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ev_detach_region
 *
 * PARAMETERS:  region_obj      - Region Object
 *              acpi_ns_is_locked - Namespace Region Already Locked?
 *
 * RETURN:      None
 *
 * DESCRIPTION: Break the association between the handler and the region
 *              this is a two way association.
 *
 ******************************************************************************/

void
acpi_ev_detach_region(
	union acpi_operand_object       *region_obj,
	u8                              acpi_ns_is_locked)
{
	union acpi_operand_object       *handler_obj;
	union acpi_operand_object       *obj_desc;
	union acpi_operand_object       **last_obj_ptr;
	acpi_adr_space_setup            region_setup;
	void                            **region_context;
	union acpi_operand_object       *region_obj2;
	acpi_status                     status;


	ACPI_FUNCTION_TRACE ("ev_detach_region");


	region_obj2 = acpi_ns_get_secondary_object (region_obj);
	if (!region_obj2) {
		return_VOID;
	}
	region_context = &region_obj2->extra.region_context;

	/* Get the address handler from the region object */

	handler_obj = region_obj->region.handler;
	if (!handler_obj) {
		/* This region has no handler, all done */

		return_VOID;
	}

	/* Find this region in the handler's list */

	obj_desc = handler_obj->address_space.region_list;
	last_obj_ptr = &handler_obj->address_space.region_list;

	while (obj_desc) {
		/* Is this the correct Region? */

		if (obj_desc == region_obj) {
			ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
				"Removing Region %p from address handler %p\n",
				region_obj, handler_obj));

			/* This is it, remove it from the handler's list */

			*last_obj_ptr = obj_desc->region.next;
			obj_desc->region.next = NULL;           /* Must clear field */

			if (acpi_ns_is_locked) {
				status = acpi_ut_release_mutex (ACPI_MTX_NAMESPACE);
				if (ACPI_FAILURE (status)) {
					return_VOID;
				}
			}

			/* Now stop region accesses by executing the _REG method */

			status = acpi_ev_execute_reg_method (region_obj, 0);
			if (ACPI_FAILURE (status)) {
				ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "%s from region _REG, [%s]\n",
					acpi_format_exception (status),
					acpi_ut_get_region_name (region_obj->region.space_id)));
			}

			if (acpi_ns_is_locked) {
				status = acpi_ut_acquire_mutex (ACPI_MTX_NAMESPACE);
				if (ACPI_FAILURE (status)) {
					return_VOID;
				}
			}

			/* Call the setup handler with the deactivate notification */

			region_setup = handler_obj->address_space.setup;
			status = region_setup (region_obj, ACPI_REGION_DEACTIVATE,
					  handler_obj->address_space.context, region_context);

			/* Init routine may fail, Just ignore errors */

			if (ACPI_FAILURE (status)) {
				ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "%s from region init, [%s]\n",
					acpi_format_exception (status),
					acpi_ut_get_region_name (region_obj->region.space_id)));
			}

			region_obj->region.flags &= ~(AOPOBJ_SETUP_COMPLETE);

			/*
			 * Remove handler reference in the region
			 *
			 * NOTE: this doesn't mean that the region goes away
			 * The region is just inaccessible as indicated to
			 * the _REG method
			 *
			 * If the region is on the handler's list
			 * this better be the region's handler
			 */
			region_obj->region.handler = NULL;
			acpi_ut_remove_reference (handler_obj);

			return_VOID;
		}

		/* Walk the linked list of handlers */

		last_obj_ptr = &obj_desc->region.next;
		obj_desc = obj_desc->region.next;
	}

	/* If we get here, the region was not in the handler's region list */

	ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
		"Cannot remove region %p from address handler %p\n",
		region_obj, handler_obj));

	return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ev_attach_region
 *
 * PARAMETERS:  handler_obj     - Handler Object
 *              region_obj      - Region Object
 *              acpi_ns_is_locked - Namespace Region Already Locked?
 *
 * RETURN:      None
 *
 * DESCRIPTION: Create the association between the handler and the region
 *              this is a two way association.
 *
 ******************************************************************************/

acpi_status
acpi_ev_attach_region (
	union acpi_operand_object       *handler_obj,
	union acpi_operand_object       *region_obj,
	u8                              acpi_ns_is_locked)
{

	ACPI_FUNCTION_TRACE ("ev_attach_region");


	ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
		"Adding Region [%4.4s] %p to address handler %p [%s]\n",
		acpi_ut_get_node_name (region_obj->region.node),
		region_obj, handler_obj,
		acpi_ut_get_region_name (region_obj->region.space_id)));

	/* Link this region to the front of the handler's list */

	region_obj->region.next = handler_obj->address_space.region_list;
	handler_obj->address_space.region_list = region_obj;

	/* Install the region's handler */

	if (region_obj->region.handler) {
		return_ACPI_STATUS (AE_ALREADY_EXISTS);
	}

	region_obj->region.handler = handler_obj;
	acpi_ut_add_reference (handler_obj);

	return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ev_install_handler
 *
 * PARAMETERS:  walk_namespace callback
 *
 * DESCRIPTION: This routine installs an address handler into objects that are
 *              of type Region or Device.
 *
 *              If the Object is a Device, and the device has a handler of
 *              the same type then the search is terminated in that branch.
 *
 *              This is because the existing handler is closer in proximity
 *              to any more regions than the one we are trying to install.
 *
 ******************************************************************************/

acpi_status
acpi_ev_install_handler (
	acpi_handle                     obj_handle,
	u32                             level,
	void                            *context,
	void                            **return_value)
{
	union acpi_operand_object       *handler_obj;
	union acpi_operand_object       *next_handler_obj;
	union acpi_operand_object       *obj_desc;
	struct acpi_namespace_node      *node;
	acpi_status                     status;


	ACPI_FUNCTION_NAME ("ev_install_handler");


	handler_obj = (union acpi_operand_object   *) context;

	/* Parameter validation */

	if (!handler_obj) {
		return (AE_OK);
	}

	/* Convert and validate the device handle */

	node = acpi_ns_map_handle_to_node (obj_handle);
	if (!node) {
		return (AE_BAD_PARAMETER);
	}

	/*
	 * We only care about regions.and objects
	 * that are allowed to have address space handlers
	 */
	if ((node->type != ACPI_TYPE_DEVICE) &&
		(node->type != ACPI_TYPE_REGION) &&
		(node != acpi_gbl_root_node)) {
		return (AE_OK);
	}

	/* Check for an existing internal object */

	obj_desc = acpi_ns_get_attached_object (node);
	if (!obj_desc) {
		/* No object, just exit */

		return (AE_OK);
	}

	/* Devices are handled different than regions */

	if (ACPI_GET_OBJECT_TYPE (obj_desc) == ACPI_TYPE_DEVICE) {
		/* Check if this Device already has a handler for this address space */

		next_handler_obj = obj_desc->device.handler;
		while (next_handler_obj) {
			/* Found a handler, is it for the same address space? */

			if (next_handler_obj->address_space.space_id == handler_obj->address_space.space_id) {
				ACPI_DEBUG_PRINT ((ACPI_DB_OPREGION,
					"Found handler for region [%s] in device %p(%p) handler %p\n",
					acpi_ut_get_region_name (handler_obj->address_space.space_id),
					obj_desc, next_handler_obj, handler_obj));

				/*
				 * Since the object we found it on was a device, then it
				 * means that someone has already installed a handler for
				 * the branch of the namespace from this device on.  Just
				 * bail out telling the walk routine to not traverse this
				 * branch.  This preserves the scoping rule for handlers.
				 */
				return (AE_CTRL_DEPTH);
			}

			/* Walk the linked list of handlers attached to this device */

			next_handler_obj = next_handler_obj->address_space.next;
		}

		/*
		 * As long as the device didn't have a handler for this
		 * space we don't care about it.  We just ignore it and
		 * proceed.
		 */
		return (AE_OK);
	}

	/* Object is a Region */

	if (obj_desc->region.space_id != handler_obj->address_space.space_id) {
		/*
		 * This region is for a different address space
		 * -- just ignore it
		 */
		return (AE_OK);
	}

	/*
	 * Now we have a region and it is for the handler's address
	 * space type.
	 *
	 * First disconnect region for any previous handler (if any)
	 */
	acpi_ev_detach_region (obj_desc, FALSE);

	/* Connect the region to the new handler */

	status = acpi_ev_attach_region (handler_obj, obj_desc, FALSE);
	return (status);
}

/*******************************************************************************
 *
 * FUNCTION:    acpi_ev_reg_run
 *
 * PARAMETERS:  walk_namespace callback
 *
 * DESCRIPTION: Run _REg method for region objects of the requested space_iD
 *
 ******************************************************************************/

acpi_status
acpi_ev_reg_run (
	acpi_handle                     obj_handle,
	u32                             level,
	void                            *context,
	void                            **return_value)
{
	union acpi_operand_object       *handler_obj;
	union acpi_operand_object       *obj_desc;
	struct acpi_namespace_node      *node;
	acpi_status                     status;


	handler_obj = (union acpi_operand_object   *) context;

	/* Parameter validation */

	if (!handler_obj) {
		return (AE_OK);
	}

	/* Convert and validate the device handle */

	node = acpi_ns_map_handle_to_node (obj_handle);
	if (!node) {
		return (AE_BAD_PARAMETER);
	}

	/*
	 * We only care about regions.and objects
	 * that are allowed to have address space handlers
	 */
	if ((node->type != ACPI_TYPE_REGION) &&
		(node != acpi_gbl_root_node)) {
		return (AE_OK);
	}

	/* Check for an existing internal object */

	obj_desc = acpi_ns_get_attached_object (node);
	if (!obj_desc) {
		/* No object, just exit */

		return (AE_OK);
	}


	/* Object is a Region */

	if (obj_desc->region.space_id != handler_obj->address_space.space_id) {
		/*
		 * This region is for a different address space
		 * -- just ignore it
		 */
		return (AE_OK);
	}

	status = acpi_ev_execute_reg_method (obj_desc, 1);
	return (status);
}

