/******************************************************************************
 *
 * Module Name: pswalk - Parser routines to walk parsed op tree(s)
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
#include <acpi/acparser.h>
#include <acpi/acdispat.h>

#define _COMPONENT          ACPI_PARSER
	 ACPI_MODULE_NAME    ("pswalk")


/*******************************************************************************
 *
 * FUNCTION:    acpi_ps_get_next_walk_op
 *
 * PARAMETERS:  walk_state          - Current state of the walk
 *              Op                  - Current Op to be walked
 *              ascending_callback  - Procedure called when Op is complete
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get the next Op in a walk of the parse tree.
 *
 ******************************************************************************/

acpi_status
acpi_ps_get_next_walk_op (
	struct acpi_walk_state          *walk_state,
	union acpi_parse_object         *op,
	acpi_parse_upwards              ascending_callback)
{
	union acpi_parse_object         *next;
	union acpi_parse_object         *parent;
	union acpi_parse_object         *grand_parent;
	acpi_status                     status;


	ACPI_FUNCTION_TRACE_PTR ("ps_get_next_walk_op", op);


	/* Check for a argument only if we are descending in the tree */

	if (walk_state->next_op_info != ACPI_NEXT_OP_UPWARD) {
		/* Look for an argument or child of the current op */

		next = acpi_ps_get_arg (op, 0);
		if (next) {
			/* Still going downward in tree (Op is not completed yet) */

			walk_state->prev_op     = op;
			walk_state->next_op     = next;
			walk_state->next_op_info = ACPI_NEXT_OP_DOWNWARD;

			return_ACPI_STATUS (AE_OK);
		}

		/*
		 * No more children, this Op is complete.  Save Next and Parent
		 * in case the Op object gets deleted by the callback routine
		 */
		next    = op->common.next;
		parent  = op->common.parent;

		walk_state->op    = op;
		walk_state->op_info = acpi_ps_get_opcode_info (op->common.aml_opcode);
		walk_state->opcode = op->common.aml_opcode;

		status = ascending_callback (walk_state);

		/*
		 * If we are back to the starting point, the walk is complete.
		 */
		if (op == walk_state->origin) {
			/* Reached the point of origin, the walk is complete */

			walk_state->prev_op     = op;
			walk_state->next_op     = NULL;

			return_ACPI_STATUS (status);
		}

		/*
		 * Check for a sibling to the current op.  A sibling means
		 * we are still going "downward" in the tree.
		 */
		if (next) {
			/* There is a sibling, it will be next */

			walk_state->prev_op     = op;
			walk_state->next_op     = next;
			walk_state->next_op_info = ACPI_NEXT_OP_DOWNWARD;

			/* Continue downward */

			return_ACPI_STATUS (status);
		}

		/*
		 * Drop into the loop below because we are moving upwards in
		 * the tree
		 */
	}
	else {
		/*
		 * We are resuming a walk, and we were (are) going upward in the tree.
		 * So, we want to drop into the parent loop below.
		 */
		parent = op;
	}

	/*
	 * Look for a sibling of the current Op's parent
	 * Continue moving up the tree until we find a node that has not been
	 * visited, or we get back to where we started.
	 */
	while (parent) {
		/* We are moving up the tree, therefore this parent Op is complete */

		grand_parent = parent->common.parent;
		next        = parent->common.next;

		walk_state->op    = parent;
		walk_state->op_info = acpi_ps_get_opcode_info (parent->common.aml_opcode);
		walk_state->opcode = parent->common.aml_opcode;

		status = ascending_callback (walk_state);

		/*
		 * If we are back to the starting point, the walk is complete.
		 */
		if (parent == walk_state->origin) {
			/* Reached the point of origin, the walk is complete */

			walk_state->prev_op     = parent;
			walk_state->next_op     = NULL;

			return_ACPI_STATUS (status);
		}

		/*
		 * If there is a sibling to this parent (it is not the starting point
		 * Op), then we will visit it.
		 */
		if (next) {
			/* found sibling of parent */

			walk_state->prev_op     = parent;
			walk_state->next_op     = next;
			walk_state->next_op_info = ACPI_NEXT_OP_DOWNWARD;

			return_ACPI_STATUS (status);
		}

		/* No siblings, no errors, just move up one more level in the tree */

		op                  = parent;
		parent              = grand_parent;
		walk_state->prev_op = op;
	}


	/*
	 * Got all the way to the top of the tree, we must be done!
	 * However, the code should have terminated in the loop above
	 */
	walk_state->next_op     = NULL;

	return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ps_delete_completed_op
 *
 * PARAMETERS:  State           - Walk state
 *              Op              - Completed op
 *
 * RETURN:      AE_OK
 *
 * DESCRIPTION: Callback function for acpi_ps_get_next_walk_op(). Used during
 *              acpi_ps_delete_parse tree to delete Op objects when all sub-objects
 *              have been visited (and deleted.)
 *
 ******************************************************************************/

acpi_status
acpi_ps_delete_completed_op (
	struct acpi_walk_state          *walk_state)
{

	acpi_ps_free_op (walk_state->op);
	return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ps_delete_parse_tree
 *
 * PARAMETERS:  subtree_root        - Root of tree (or subtree) to delete
 *
 * RETURN:      None
 *
 * DESCRIPTION: Delete a portion of or an entire parse tree.
 *
 ******************************************************************************/

void
acpi_ps_delete_parse_tree (
	union acpi_parse_object         *subtree_root)
{
	struct acpi_walk_state          *walk_state;
	struct acpi_thread_state        *thread;
	acpi_status                     status;


	ACPI_FUNCTION_TRACE_PTR ("ps_delete_parse_tree", subtree_root);


	if (!subtree_root) {
		return_VOID;
	}

	/* Create and initialize a new walk list */

	thread = acpi_ut_create_thread_state ();
	if (!thread) {
		return_VOID;
	}

	walk_state = acpi_ds_create_walk_state (0, NULL, NULL, thread);
	if (!walk_state) {
		return_VOID;
	}

	walk_state->parse_flags         = 0;
	walk_state->descending_callback = NULL;
	walk_state->ascending_callback  = NULL;

	walk_state->origin = subtree_root;
	walk_state->next_op = subtree_root;

	/* Head downward in the tree */

	walk_state->next_op_info = ACPI_NEXT_OP_DOWNWARD;

	/* Visit all nodes in the subtree */

	while (walk_state->next_op) {
		status = acpi_ps_get_next_walk_op (walk_state, walk_state->next_op,
				 acpi_ps_delete_completed_op);
		if (ACPI_FAILURE (status)) {
			break;
		}
	}

	/* We are done with this walk */

	acpi_ut_delete_generic_state (ACPI_CAST_PTR (union acpi_generic_state, thread));
	acpi_ds_delete_walk_state (walk_state);

	return_VOID;
}


