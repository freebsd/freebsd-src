
/******************************************************************************
 *
 * Module Name: exmisc - ACPI AML (p-code) execution - specific opcodes
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
#include <acpi/amlcode.h>


#define _COMPONENT          ACPI_EXECUTER
	 ACPI_MODULE_NAME    ("exmisc")


/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_get_object_reference
 *
 * PARAMETERS:  obj_desc            - Create a reference to this object
 *              return_desc         - Where to store the reference
 *              walk_state          - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Obtain and return a "reference" to the target object
 *              Common code for the ref_of_op and the cond_ref_of_op.
 *
 ******************************************************************************/

acpi_status
acpi_ex_get_object_reference (
	union acpi_operand_object       *obj_desc,
	union acpi_operand_object       **return_desc,
	struct acpi_walk_state          *walk_state)
{
	union acpi_operand_object       *reference_obj;
	union acpi_operand_object       *referenced_obj;


	ACPI_FUNCTION_TRACE_PTR ("ex_get_object_reference", obj_desc);


	*return_desc = NULL;

	switch (ACPI_GET_DESCRIPTOR_TYPE (obj_desc)) {
	case ACPI_DESC_TYPE_OPERAND:

		if (ACPI_GET_OBJECT_TYPE (obj_desc) != ACPI_TYPE_LOCAL_REFERENCE) {
			return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
		}

		/*
		 * Must be a reference to a Local or Arg
		 */
		switch (obj_desc->reference.opcode) {
		case AML_LOCAL_OP:
		case AML_ARG_OP:

			/* The referenced object is the pseudo-node for the local/arg */

			referenced_obj = obj_desc->reference.object;
			break;

		default:

			ACPI_REPORT_ERROR (("Unknown Reference subtype in get ref %X\n",
				obj_desc->reference.opcode));
			return_ACPI_STATUS (AE_AML_INTERNAL);
		}
		break;


	case ACPI_DESC_TYPE_NAMED:

		/*
		 * A named reference that has already been resolved to a Node
		 */
		referenced_obj = obj_desc;
		break;


	default:

		ACPI_REPORT_ERROR (("Invalid descriptor type in get ref: %X\n",
				ACPI_GET_DESCRIPTOR_TYPE (obj_desc)));
		return_ACPI_STATUS (AE_TYPE);
	}


	/* Create a new reference object */

	reference_obj = acpi_ut_create_internal_object (ACPI_TYPE_LOCAL_REFERENCE);
	if (!reference_obj) {
		return_ACPI_STATUS (AE_NO_MEMORY);
	}

	reference_obj->reference.opcode = AML_REF_OF_OP;
	reference_obj->reference.object = referenced_obj;
	*return_desc = reference_obj;

	ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Object %p Type [%s], returning Reference %p\n",
			obj_desc, acpi_ut_get_object_type_name (obj_desc), *return_desc));

	return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_concat_template
 *
 * PARAMETERS:  *obj_desc           - Object to be converted.  Must be an
 *                                    Integer, Buffer, or String
 *              walk_state          - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Concatenate two resource templates
 *
 ******************************************************************************/

acpi_status
acpi_ex_concat_template (
	union acpi_operand_object       *obj_desc1,
	union acpi_operand_object       *obj_desc2,
	union acpi_operand_object       **actual_return_desc,
	struct acpi_walk_state          *walk_state)
{
	union acpi_operand_object       *return_desc;
	u8                              *new_buf;
	u8                              *end_tag1;
	u8                              *end_tag2;
	acpi_size                       length1;
	acpi_size                       length2;


	ACPI_FUNCTION_TRACE ("ex_concat_template");


	/* Find the end_tags in each resource template */

	end_tag1 = acpi_ut_get_resource_end_tag (obj_desc1);
	end_tag2 = acpi_ut_get_resource_end_tag (obj_desc2);
	if (!end_tag1 || !end_tag2) {
		return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
	}

	/* Compute the length of each part */

	length1 = ACPI_PTR_DIFF (end_tag1, obj_desc1->buffer.pointer);
	length2 = ACPI_PTR_DIFF (end_tag2, obj_desc2->buffer.pointer) +
			  2; /* Size of END_TAG */

	/* Create a new buffer object for the result */

	return_desc = acpi_ut_create_buffer_object (length1 + length2);
	if (!return_desc) {
		return_ACPI_STATUS (AE_NO_MEMORY);
	}

	/* Copy the templates to the new descriptor */

	new_buf = return_desc->buffer.pointer;
	ACPI_MEMCPY (new_buf, obj_desc1->buffer.pointer, length1);
	ACPI_MEMCPY (new_buf + length1, obj_desc2->buffer.pointer, length2);

	/* Compute the new checksum */

	new_buf[return_desc->buffer.length - 1] =
			acpi_ut_generate_checksum (return_desc->buffer.pointer,
					   (return_desc->buffer.length - 1));

	/* Return the completed template descriptor */

	*actual_return_desc = return_desc;
	return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_do_concatenate
 *
 * PARAMETERS:  obj_desc1           - First source object
 *              obj_desc2           - Second source object
 *              actual_return_desc  - Where to place the return object
 *              walk_state          - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Concatenate two objects OF THE SAME TYPE.
 *
 ******************************************************************************/

acpi_status
acpi_ex_do_concatenate (
	union acpi_operand_object       *obj_desc1,
	union acpi_operand_object       *obj_desc2,
	union acpi_operand_object       **actual_return_desc,
	struct acpi_walk_state          *walk_state)
{
	acpi_status                     status;
	u32                             i;
	acpi_integer                    this_integer;
	union acpi_operand_object       *return_desc;
	char                            *new_buf;


	ACPI_FUNCTION_ENTRY ();


	/*
	 * There are three cases to handle:
	 *
	 * 1) Two Integers concatenated to produce a new Buffer
	 * 2) Two Strings concatenated to produce a new String
	 * 3) Two Buffers concatenated to produce a new Buffer
	 */
	switch (ACPI_GET_OBJECT_TYPE (obj_desc1)) {
	case ACPI_TYPE_INTEGER:

		/* Result of two Integers is a Buffer */
		/* Need enough buffer space for two integers */

		return_desc = acpi_ut_create_buffer_object (acpi_gbl_integer_byte_width * 2);
		if (!return_desc) {
			return (AE_NO_MEMORY);
		}

		new_buf = (char *) return_desc->buffer.pointer;

		/* Convert the first integer */

		this_integer = obj_desc1->integer.value;
		for (i = 0; i < acpi_gbl_integer_byte_width; i++) {
			new_buf[i] = (char) this_integer;
			this_integer >>= 8;
		}

		/* Convert the second integer */

		this_integer = obj_desc2->integer.value;
		for (; i < (ACPI_MUL_2 (acpi_gbl_integer_byte_width)); i++) {
			new_buf[i] = (char) this_integer;
			this_integer >>= 8;
		}

		break;


	case ACPI_TYPE_STRING:

		/* Result of two Strings is a String */

		return_desc = acpi_ut_create_internal_object (ACPI_TYPE_STRING);
		if (!return_desc) {
			return (AE_NO_MEMORY);
		}

		/* Operand0 is string  */

		new_buf = ACPI_MEM_CALLOCATE ((acpi_size) obj_desc1->string.length +
				   (acpi_size) obj_desc2->string.length + 1);
		if (!new_buf) {
			ACPI_REPORT_ERROR
				(("ex_do_concatenate: String allocation failure\n"));
			status = AE_NO_MEMORY;
			goto cleanup;
		}

		/* Concatenate the strings */

		ACPI_STRCPY (new_buf, obj_desc1->string.pointer);
		ACPI_STRCPY (new_buf + obj_desc1->string.length,
				  obj_desc2->string.pointer);

		/* Complete the String object initialization */

		return_desc->string.pointer = new_buf;
		return_desc->string.length = obj_desc1->string.length +
				   obj_desc2->string.length;
		break;


	case ACPI_TYPE_BUFFER:

		/* Result of two Buffers is a Buffer */

		return_desc = acpi_ut_create_buffer_object (
				   (acpi_size) obj_desc1->buffer.length +
				   (acpi_size) obj_desc2->buffer.length);
		if (!return_desc) {
			return (AE_NO_MEMORY);
		}

		new_buf = (char *) return_desc->buffer.pointer;

		/* Concatenate the buffers */

		ACPI_MEMCPY (new_buf, obj_desc1->buffer.pointer,
				  obj_desc1->buffer.length);
		ACPI_MEMCPY (new_buf + obj_desc1->buffer.length, obj_desc2->buffer.pointer,
				   obj_desc2->buffer.length);

		break;


	default:

		/* Invalid object type, should not happen here */

		ACPI_REPORT_ERROR (("Concat - invalid obj type: %X\n",
				ACPI_GET_OBJECT_TYPE (obj_desc1)));
		status = AE_AML_INTERNAL;
		return_desc = NULL;
	}

	*actual_return_desc = return_desc;
	return (AE_OK);


cleanup:

	acpi_ut_remove_reference (return_desc);
	return (status);
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_do_math_op
 *
 * PARAMETERS:  Opcode              - AML opcode
 *              Operand0            - Integer operand #0
 *              Operand1            - Integer operand #1
 *
 * RETURN:      Integer result of the operation
 *
 * DESCRIPTION: Execute a math AML opcode. The purpose of having all of the
 *              math functions here is to prevent a lot of pointer dereferencing
 *              to obtain the operands.
 *
 ******************************************************************************/

acpi_integer
acpi_ex_do_math_op (
	u16                             opcode,
	acpi_integer                    operand0,
	acpi_integer                    operand1)
{


	switch (opcode) {
	case AML_ADD_OP:                /* Add (Operand0, Operand1, Result) */

		return (operand0 + operand1);


	case AML_BIT_AND_OP:            /* And (Operand0, Operand1, Result) */

		return (operand0 & operand1);


	case AML_BIT_NAND_OP:           /* NAnd (Operand0, Operand1, Result) */

		return (~(operand0 & operand1));


	case AML_BIT_OR_OP:             /* Or (Operand0, Operand1, Result) */

		return (operand0 | operand1);


	case AML_BIT_NOR_OP:            /* NOr (Operand0, Operand1, Result) */

		return (~(operand0 | operand1));


	case AML_BIT_XOR_OP:            /* XOr (Operand0, Operand1, Result) */

		return (operand0 ^ operand1);


	case AML_MULTIPLY_OP:           /* Multiply (Operand0, Operand1, Result) */

		return (operand0 * operand1);


	case AML_SHIFT_LEFT_OP:         /* shift_left (Operand, shift_count, Result) */

		return (operand0 << operand1);


	case AML_SHIFT_RIGHT_OP:        /* shift_right (Operand, shift_count, Result) */

		return (operand0 >> operand1);


	case AML_SUBTRACT_OP:           /* Subtract (Operand0, Operand1, Result) */

		return (operand0 - operand1);

	default:

		return (0);
	}
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_do_logical_op
 *
 * PARAMETERS:  Opcode              - AML opcode
 *              Operand0            - Integer operand #0
 *              Operand1            - Integer operand #1
 *
 * RETURN:      TRUE/FALSE result of the operation
 *
 * DESCRIPTION: Execute a logical AML opcode. The purpose of having all of the
 *              functions here is to prevent a lot of pointer dereferencing
 *              to obtain the operands and to simplify the generation of the
 *              logical value.
 *
 *              Note: cleanest machine code seems to be produced by the code
 *              below, rather than using statements of the form:
 *                  Result = (Operand0 == Operand1);
 *
 ******************************************************************************/

u8
acpi_ex_do_logical_op (
	u16                             opcode,
	acpi_integer                    operand0,
	acpi_integer                    operand1)
{


	switch (opcode) {

	case AML_LAND_OP:               /* LAnd (Operand0, Operand1) */

		if (operand0 && operand1) {
			return (TRUE);
		}
		break;


	case AML_LEQUAL_OP:             /* LEqual (Operand0, Operand1) */

		if (operand0 == operand1) {
			return (TRUE);
		}
		break;


	case AML_LGREATER_OP:           /* LGreater (Operand0, Operand1) */

		if (operand0 > operand1) {
			return (TRUE);
		}
		break;


	case AML_LLESS_OP:              /* LLess (Operand0, Operand1) */

		if (operand0 < operand1) {
			return (TRUE);
		}
		break;


	case AML_LOR_OP:                 /* LOr (Operand0, Operand1) */

		if (operand0 || operand1) {
			return (TRUE);
		}
		break;

	default:
		break;
	}

	return (FALSE);
}


