
/******************************************************************************
 *
 * Module Name: exoparg6 - AML execution - opcodes with 6 arguments
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
#include <acpi/acinterp.h>
#include <acpi/acparser.h>
#include <acpi/amlcode.h>


#define _COMPONENT          ACPI_EXECUTER
	 ACPI_MODULE_NAME    ("exoparg6")


/*!
 * Naming convention for AML interpreter execution routines.
 *
 * The routines that begin execution of AML opcodes are named with a common
 * convention based upon the number of arguments, the number of target operands,
 * and whether or not a value is returned:
 *
 *      AcpiExOpcode_xA_yT_zR
 *
 * Where:
 *
 * xA - ARGUMENTS:    The number of arguments (input operands) that are
 *                    required for this opcode type (1 through 6 args).
 * yT - TARGETS:      The number of targets (output operands) that are required
 *                    for this opcode type (0, 1, or 2 targets).
 * zR - RETURN VALUE: Indicates whether this opcode type returns a value
 *                    as the function return (0 or 1).
 *
 * The AcpiExOpcode* functions are called via the Dispatcher component with
 * fully resolved operands.
!*/


/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_do_match
 *
 * PARAMETERS:  match_op        - The AML match operand
 *              package_value   - Value from the target package
 *              match_value     - Value to be matched
 *
 * RETURN:      TRUE if the match is successful, FALSE otherwise
 *
 * DESCRIPTION: Implements the low-level match for the ASL Match operator
 *
 ******************************************************************************/

u8
acpi_ex_do_match (
	u32                             match_op,
	acpi_integer                    package_value,
	acpi_integer                    match_value)
{

	switch (match_op) {
	case MATCH_MTR:   /* always true */

		break;


	case MATCH_MEQ:   /* true if equal   */

		if (package_value != match_value) {
			return (FALSE);
		}
		break;


	case MATCH_MLE:   /* true if less than or equal  */

		if (package_value > match_value) {
			return (FALSE);
		}
		break;


	case MATCH_MLT:   /* true if less than   */

		if (package_value >= match_value) {
			return (FALSE);
		}
		break;


	case MATCH_MGE:   /* true if greater than or equal   */

		if (package_value < match_value) {
			return (FALSE);
		}
		break;


	case MATCH_MGT:   /* true if greater than    */

		if (package_value <= match_value) {
			return (FALSE);
		}
		break;


	default:    /* undefined   */

		return (FALSE);
	}


	return TRUE;
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_opcode_6A_0T_1R
 *
 * PARAMETERS:  walk_state          - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute opcode with 6 arguments, no target, and a return value
 *
 ******************************************************************************/

acpi_status
acpi_ex_opcode_6A_0T_1R (
	struct acpi_walk_state          *walk_state)
{
	union acpi_operand_object       **operand = &walk_state->operands[0];
	union acpi_operand_object       *return_desc = NULL;
	acpi_status                     status = AE_OK;
	u32                             index;
	union acpi_operand_object       *this_element;


	ACPI_FUNCTION_TRACE_STR ("ex_opcode_6A_0T_1R", acpi_ps_get_opcode_name (walk_state->opcode));


	switch (walk_state->opcode) {
	case AML_MATCH_OP:
		/*
		 * Match (search_package[0], match_op1[1], match_object1[2],
		 *                          match_op2[3], match_object2[4], start_index[5])
		 */

		/* Validate match comparison sub-opcodes */

		if ((operand[1]->integer.value > MAX_MATCH_OPERATOR) ||
			(operand[3]->integer.value > MAX_MATCH_OPERATOR)) {
			ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "operation encoding out of range\n"));
			status = AE_AML_OPERAND_VALUE;
			goto cleanup;
		}

		index = (u32) operand[5]->integer.value;
		if (index >= (u32) operand[0]->package.count) {
			ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Index beyond package end\n"));
			status = AE_AML_PACKAGE_LIMIT;
			goto cleanup;
		}

		return_desc = acpi_ut_create_internal_object (ACPI_TYPE_INTEGER);
		if (!return_desc) {
			status = AE_NO_MEMORY;
			goto cleanup;

		}

		/* Default return value if no match found */

		return_desc->integer.value = ACPI_INTEGER_MAX;

		/*
		 * Examine each element until a match is found.  Within the loop,
		 * "continue" signifies that the current element does not match
		 * and the next should be examined.
		 *
		 * Upon finding a match, the loop will terminate via "break" at
		 * the bottom.  If it terminates "normally", match_value will be -1
		 * (its initial value) indicating that no match was found.  When
		 * returned as a Number, this will produce the Ones value as specified.
		 */
		for ( ; index < operand[0]->package.count; index++) {
			this_element = operand[0]->package.elements[index];

			/*
			 * Treat any NULL or non-numeric elements as non-matching.
			 */
			if (!this_element ||
				ACPI_GET_OBJECT_TYPE (this_element) != ACPI_TYPE_INTEGER) {
				continue;
			}

			/*
			 * "continue" (proceed to next iteration of enclosing
			 * "for" loop) signifies a non-match.
			 */
			if (!acpi_ex_do_match ((u32) operand[1]->integer.value,
					   this_element->integer.value, operand[2]->integer.value)) {
				continue;
			}

			if (!acpi_ex_do_match ((u32) operand[3]->integer.value,
					   this_element->integer.value, operand[4]->integer.value)) {
				continue;
			}

			/* Match found: Index is the return value */

			return_desc->integer.value = index;
			break;
		}

		break;


	case AML_LOAD_TABLE_OP:

		status = acpi_ex_load_table_op (walk_state, &return_desc);
		break;


	default:

		ACPI_REPORT_ERROR (("acpi_ex_opcode_3A_0T_0R: Unknown opcode %X\n",
				walk_state->opcode));
		status = AE_AML_BAD_OPCODE;
		goto cleanup;
	}


	walk_state->result_obj = return_desc;


cleanup:

	/* Delete return object on error */

	if (ACPI_FAILURE (status)) {
		acpi_ut_remove_reference (return_desc);
	}

	return_ACPI_STATUS (status);
}
