/*******************************************************************************
 *
 * Module Name: nseval - Object evaluation interfaces -- includes control
 *                       method lookup and execution.
 *
 ******************************************************************************/

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
#include <acpi/acparser.h>
#include <acpi/acinterp.h>
#include <acpi/acnamesp.h>


#define _COMPONENT          ACPI_NAMESPACE
	 ACPI_MODULE_NAME    ("nseval")


/*******************************************************************************
 *
 * FUNCTION:    acpi_ns_evaluate_relative
 *
 * PARAMETERS:  Handle              - The relative containing object
 *              Pathname            - Name of method to execute, If NULL, the
 *                                    handle is the object to execute
 *              Params              - List of parameters to pass to the method,
 *                                    terminated by NULL.  Params itself may be
 *                                    NULL if no parameters are being passed.
 *              return_object       - Where to put method's return value (if
 *                                    any).  If NULL, no value is returned.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Find and execute the requested method using the handle as a
 *              scope
 *
 * MUTEX:       Locks Namespace
 *
 ******************************************************************************/

acpi_status
acpi_ns_evaluate_relative (
	struct acpi_namespace_node      *handle,
	char                            *pathname,
	union acpi_operand_object       **params,
	union acpi_operand_object       **return_object)
{
	acpi_status                     status;
	struct acpi_namespace_node      *prefix_node;
	struct acpi_namespace_node      *node = NULL;
	union acpi_generic_state        *scope_info;
	char                            *internal_path = NULL;


	ACPI_FUNCTION_TRACE ("ns_evaluate_relative");


	/*
	 * Must have a valid object handle
	 */
	if (!handle) {
		return_ACPI_STATUS (AE_BAD_PARAMETER);
	}

	/* Build an internal name string for the method */

	status = acpi_ns_internalize_name (pathname, &internal_path);
	if (ACPI_FAILURE (status)) {
		return_ACPI_STATUS (status);
	}

	scope_info = acpi_ut_create_generic_state ();
	if (!scope_info) {
		goto cleanup1;
	}

	/* Get the prefix handle and Node */

	status = acpi_ut_acquire_mutex (ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE (status)) {
		goto cleanup;
	}

	prefix_node = acpi_ns_map_handle_to_node (handle);
	if (!prefix_node) {
		(void) acpi_ut_release_mutex (ACPI_MTX_NAMESPACE);
		status = AE_BAD_PARAMETER;
		goto cleanup;
	}

	/* Lookup the name in the namespace */

	scope_info->scope.node = prefix_node;
	status = acpi_ns_lookup (scope_info, internal_path, ACPI_TYPE_ANY,
			 ACPI_IMODE_EXECUTE, ACPI_NS_NO_UPSEARCH, NULL,
			 &node);

	(void) acpi_ut_release_mutex (ACPI_MTX_NAMESPACE);

	if (ACPI_FAILURE (status)) {
		ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "Object [%s] not found [%s]\n",
			pathname, acpi_format_exception (status)));
		goto cleanup;
	}

	/*
	 * Now that we have a handle to the object, we can attempt
	 * to evaluate it.
	 */
	ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "%s [%p] Value %p\n",
		pathname, node, acpi_ns_get_attached_object (node)));

	status = acpi_ns_evaluate_by_handle (node, params, return_object);

	ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "*** Completed eval of object %s ***\n",
		pathname));

cleanup:
	acpi_ut_delete_generic_state (scope_info);

cleanup1:
	ACPI_MEM_FREE (internal_path);
	return_ACPI_STATUS (status);
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ns_evaluate_by_name
 *
 * PARAMETERS:  Pathname            - Fully qualified pathname to the object
 *              return_object       - Where to put method's return value (if
 *                                    any).  If NULL, no value is returned.
 *              Params              - List of parameters to pass to the method,
 *                                    terminated by NULL.  Params itself may be
 *                                    NULL if no parameters are being passed.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Find and execute the requested method passing the given
 *              parameters
 *
 * MUTEX:       Locks Namespace
 *
 ******************************************************************************/

acpi_status
acpi_ns_evaluate_by_name (
	char                            *pathname,
	union acpi_operand_object       **params,
	union acpi_operand_object       **return_object)
{
	acpi_status                     status;
	struct acpi_namespace_node      *node = NULL;
	char                            *internal_path = NULL;


	ACPI_FUNCTION_TRACE ("ns_evaluate_by_name");


	/* Build an internal name string for the method */

	status = acpi_ns_internalize_name (pathname, &internal_path);
	if (ACPI_FAILURE (status)) {
		return_ACPI_STATUS (status);
	}

	status = acpi_ut_acquire_mutex (ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE (status)) {
		goto cleanup;
	}

	/* Lookup the name in the namespace */

	status = acpi_ns_lookup (NULL, internal_path, ACPI_TYPE_ANY,
			 ACPI_IMODE_EXECUTE, ACPI_NS_NO_UPSEARCH, NULL,
			 &node);

	(void) acpi_ut_release_mutex (ACPI_MTX_NAMESPACE);

	if (ACPI_FAILURE (status)) {
		ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "Object at [%s] was not found, status=%.4X\n",
			pathname, status));
		goto cleanup;
	}

	/*
	 * Now that we have a handle to the object, we can attempt
	 * to evaluate it.
	 */
	ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "%s [%p] Value %p\n",
		pathname, node, acpi_ns_get_attached_object (node)));

	status = acpi_ns_evaluate_by_handle (node, params, return_object);

	ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "*** Completed eval of object %s ***\n",
		pathname));


cleanup:

	/* Cleanup */

	if (internal_path) {
		ACPI_MEM_FREE (internal_path);
	}

	return_ACPI_STATUS (status);
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ns_evaluate_by_handle
 *
 * PARAMETERS:  Handle              - Method Node to execute
 *              Params              - List of parameters to pass to the method,
 *                                    terminated by NULL.  Params itself may be
 *                                    NULL if no parameters are being passed.
 *              return_object       - Where to put method's return value (if
 *                                    any).  If NULL, no value is returned.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute the requested method passing the given parameters
 *
 * MUTEX:       Locks Namespace
 *
 ******************************************************************************/

acpi_status
acpi_ns_evaluate_by_handle (
	struct acpi_namespace_node      *handle,
	union acpi_operand_object       **params,
	union acpi_operand_object       **return_object)
{
	struct acpi_namespace_node      *node;
	acpi_status                     status;
	union acpi_operand_object       *local_return_object;


	ACPI_FUNCTION_TRACE ("ns_evaluate_by_handle");


	/* Check if namespace has been initialized */

	if (!acpi_gbl_root_node) {
		return_ACPI_STATUS (AE_NO_NAMESPACE);
	}

	/* Parameter Validation */

	if (!handle) {
		return_ACPI_STATUS (AE_BAD_PARAMETER);
	}

	if (return_object) {
		/* Initialize the return value to an invalid object */

		*return_object = NULL;
	}

	/* Get the prefix handle and Node */

	status = acpi_ut_acquire_mutex (ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE (status)) {
		return_ACPI_STATUS (status);
	}

	node = acpi_ns_map_handle_to_node (handle);
	if (!node) {
		(void) acpi_ut_release_mutex (ACPI_MTX_NAMESPACE);
		return_ACPI_STATUS (AE_BAD_PARAMETER);
	}

	/*
	 * For a method alias, we must grab the actual method node
	 * so that proper scoping context will be established
	 * before execution.
	 */
	if (acpi_ns_get_type (node) == ACPI_TYPE_LOCAL_METHOD_ALIAS) {
		node = ACPI_CAST_PTR (struct acpi_namespace_node, node->object);
	}

	/*
	 * Two major cases here:
	 * 1) The object is an actual control method -- execute it.
	 * 2) The object is not a method -- just return it's current
	 *      value
	 *
	 * In both cases, the namespace is unlocked by the
	 *  acpi_ns* procedure
	 */
	if (acpi_ns_get_type (node) == ACPI_TYPE_METHOD) {
		/*
		 * Case 1) We have an actual control method to execute
		 */
		status = acpi_ns_execute_control_method (node, params,
				 &local_return_object);
	}
	else {
		/*
		 * Case 2) Object is NOT a method, just return its
		 * current value
		 */
		status = acpi_ns_get_object_value (node, &local_return_object);
	}

	/*
	 * Check if there is a return value on the stack that must
	 * be dealt with
	 */
	if (status == AE_CTRL_RETURN_VALUE) {
		/*
		 * If the Method returned a value and the caller
		 * provided a place to store a returned value, Copy
		 * the returned value to the object descriptor provided
		 * by the caller.
		 */
		if (return_object) {
			/*
			 * Valid return object, copy the pointer to
			 * the returned object
			 */
			*return_object = local_return_object;
		}

		/* Map AE_CTRL_RETURN_VALUE to AE_OK, we are done with it */

		status = AE_OK;
	}

	/*
	 * Namespace was unlocked by the handling acpi_ns* function,
	 * so we just return
	 */
	return_ACPI_STATUS (status);
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ns_execute_control_method
 *
 * PARAMETERS:  method_node         - The method to execute
 *              Params              - List of parameters to pass to the method,
 *                                    terminated by NULL.  Params itself may be
 *                                    NULL if no parameters are being passed.
 *              return_obj_desc     - List of result objects to be returned
 *                                    from the method.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute the requested method passing the given parameters
 *
 * MUTEX:       Assumes namespace is locked
 *
 ******************************************************************************/

acpi_status
acpi_ns_execute_control_method (
	struct acpi_namespace_node      *method_node,
	union acpi_operand_object       **params,
	union acpi_operand_object       **return_obj_desc)
{
	acpi_status                     status;
	union acpi_operand_object       *obj_desc;


	ACPI_FUNCTION_TRACE ("ns_execute_control_method");


	/* Verify that there is a method associated with this object */

	obj_desc = acpi_ns_get_attached_object (method_node);
	if (!obj_desc) {
		ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "No attached method object\n"));

		(void) acpi_ut_release_mutex (ACPI_MTX_NAMESPACE);
		return_ACPI_STATUS (AE_NULL_OBJECT);
	}

	ACPI_DUMP_PATHNAME (method_node, "Execute Method:",
		ACPI_LV_INFO, _COMPONENT);

	ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Method at AML address %p Length %X\n",
		obj_desc->method.aml_start + 1, obj_desc->method.aml_length - 1));

	/*
	 * Unlock the namespace before execution.  This allows namespace access
	 * via the external Acpi* interfaces while a method is being executed.
	 * However, any namespace deletion must acquire both the namespace and
	 * interpreter locks to ensure that no thread is using the portion of the
	 * namespace that is being deleted.
	 */
	status = acpi_ut_release_mutex (ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE (status)) {
		return_ACPI_STATUS (status);
	}

	/*
	 * Execute the method via the interpreter.  The interpreter is locked
	 * here before calling into the AML parser
	 */
	status = acpi_ex_enter_interpreter ();
	if (ACPI_FAILURE (status)) {
		return_ACPI_STATUS (status);
	}

	status = acpi_psx_execute (method_node, params, return_obj_desc);
	acpi_ex_exit_interpreter ();

	return_ACPI_STATUS (status);
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ns_get_object_value
 *
 * PARAMETERS:  Node                - The object
 *              return_obj_desc     - Where the objects value is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Return the current value of the object
 *
 * MUTEX:       Assumes namespace is locked, leaves namespace unlocked
 *
 ******************************************************************************/

acpi_status
acpi_ns_get_object_value (
	struct acpi_namespace_node      *node,
	union acpi_operand_object       **return_obj_desc)
{
	acpi_status                     status = AE_OK;
	struct acpi_namespace_node      *resolved_node = node;


	ACPI_FUNCTION_TRACE ("ns_get_object_value");


	/*
	 * Objects require additional resolution steps (e.g., the
	 * Node may be a field that must be read, etc.) -- we can't just grab
	 * the object out of the node.
	 */

	/*
	 * Use resolve_node_to_value() to get the associated value. This call
	 * always deletes obj_desc (allocated above).
	 *
	 * NOTE: we can get away with passing in NULL for a walk state
	 * because obj_desc is guaranteed to not be a reference to either
	 * a method local or a method argument (because this interface can only be
	 * called from the acpi_evaluate external interface, never called from
	 * a running control method.)
	 *
	 * Even though we do not directly invoke the interpreter
	 * for this, we must enter it because we could access an opregion.
	 * The opregion access code assumes that the interpreter
	 * is locked.
	 *
	 * We must release the namespace lock before entering the
	 * intepreter.
	 */
	status = acpi_ut_release_mutex (ACPI_MTX_NAMESPACE);
	if (ACPI_FAILURE (status)) {
		return_ACPI_STATUS (status);
	}

	status = acpi_ex_enter_interpreter ();
	if (ACPI_SUCCESS (status)) {
		status = acpi_ex_resolve_node_to_value (&resolved_node, NULL);
		/*
		 * If acpi_ex_resolve_node_to_value() succeeded, the return value was
		 * placed in resolved_node.
		 */
		acpi_ex_exit_interpreter ();

		if (ACPI_SUCCESS (status)) {
			status = AE_CTRL_RETURN_VALUE;
			*return_obj_desc = ACPI_CAST_PTR (union acpi_operand_object, resolved_node);
			ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "Returning object %p [%s]\n",
				*return_obj_desc, acpi_ut_get_object_type_name (*return_obj_desc)));
		}
	}

	/* Namespace is unlocked */

	return_ACPI_STATUS (status);
}

