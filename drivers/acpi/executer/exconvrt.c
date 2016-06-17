/******************************************************************************
 *
 * Module Name: exconvrt - Object conversion routines
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
	 ACPI_MODULE_NAME    ("exconvrt")


/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_convert_to_integer
 *
 * PARAMETERS:  obj_desc        - Object to be converted.  Must be an
 *                                Integer, Buffer, or String
 *              result_desc     - Where the new Integer object is returned
 *              walk_state      - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert an ACPI Object to an integer.
 *
 ******************************************************************************/

acpi_status
acpi_ex_convert_to_integer (
	union acpi_operand_object       *obj_desc,
	union acpi_operand_object       **result_desc,
	struct acpi_walk_state          *walk_state)
{
	u32                             i;
	union acpi_operand_object       *ret_desc;
	u32                             count;
	u8                              *pointer;
	acpi_integer                    result;
	acpi_status                     status;


	ACPI_FUNCTION_TRACE_PTR ("ex_convert_to_integer", obj_desc);


	switch (ACPI_GET_OBJECT_TYPE (obj_desc)) {
	case ACPI_TYPE_INTEGER:
		*result_desc = obj_desc;
		return_ACPI_STATUS (AE_OK);

	case ACPI_TYPE_STRING:
		pointer = (u8 *) obj_desc->string.pointer;
		count   = obj_desc->string.length;
		break;

	case ACPI_TYPE_BUFFER:
		pointer = obj_desc->buffer.pointer;
		count   = obj_desc->buffer.length;
		break;

	default:
		return_ACPI_STATUS (AE_TYPE);
	}

	/*
	 * Convert the buffer/string to an integer.  Note that both buffers and
	 * strings are treated as raw data - we don't convert ascii to hex for
	 * strings.
	 *
	 * There are two terminating conditions for the loop:
	 * 1) The size of an integer has been reached, or
	 * 2) The end of the buffer or string has been reached
	 */
	result = 0;

	/* Transfer no more than an integer's worth of data */

	if (count > acpi_gbl_integer_byte_width) {
		count = acpi_gbl_integer_byte_width;
	}

	/*
	 * String conversion is different than Buffer conversion
	 */
	switch (ACPI_GET_OBJECT_TYPE (obj_desc)) {
	case ACPI_TYPE_STRING:

		/*
		 * Convert string to an integer
		 * String must be hexadecimal as per the ACPI specification
		 */
		status = acpi_ut_strtoul64 ((char *) pointer, 16, &result);
		if (ACPI_FAILURE (status)) {
			return_ACPI_STATUS (status);
		}
		break;


	case ACPI_TYPE_BUFFER:

		/*
		 * Buffer conversion - we simply grab enough raw data from the
		 * buffer to fill an integer
		 */
		for (i = 0; i < count; i++) {
			/*
			 * Get next byte and shift it into the Result.
			 * Little endian is used, meaning that the first byte of the buffer
			 * is the LSB of the integer
			 */
			result |= (((acpi_integer) pointer[i]) << (i * 8));
		}
		break;


	default:
		/* No other types can get here */
		break;
	}

	/*
	 * Create a new integer
	 */
	ret_desc = acpi_ut_create_internal_object (ACPI_TYPE_INTEGER);
	if (!ret_desc) {
		return_ACPI_STATUS (AE_NO_MEMORY);
	}

	/* Save the Result */

	ret_desc->integer.value = result;

	/*
	 * If we are about to overwrite the original object on the operand stack,
	 * we must remove a reference on the original object because we are
	 * essentially removing it from the stack.
	 */
	if (*result_desc == obj_desc) {
		if (walk_state->opcode != AML_STORE_OP) {
			acpi_ut_remove_reference (obj_desc);
		}
	}

	*result_desc = ret_desc;
	return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_convert_to_buffer
 *
 * PARAMETERS:  obj_desc        - Object to be converted.  Must be an
 *                                Integer, Buffer, or String
 *              result_desc     - Where the new buffer object is returned
 *              walk_state      - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert an ACPI Object to a Buffer
 *
 ******************************************************************************/

acpi_status
acpi_ex_convert_to_buffer (
	union acpi_operand_object       *obj_desc,
	union acpi_operand_object       **result_desc,
	struct acpi_walk_state          *walk_state)
{
	union acpi_operand_object       *ret_desc;
	u32                             i;
	u8                              *new_buf;


	ACPI_FUNCTION_TRACE_PTR ("ex_convert_to_buffer", obj_desc);


	switch (ACPI_GET_OBJECT_TYPE (obj_desc)) {
	case ACPI_TYPE_BUFFER:

		/* No conversion necessary */

		*result_desc = obj_desc;
		return_ACPI_STATUS (AE_OK);


	case ACPI_TYPE_INTEGER:

		/*
		 * Create a new Buffer object.
		 * Need enough space for one integer
		 */
		ret_desc = acpi_ut_create_buffer_object (acpi_gbl_integer_byte_width);
		if (!ret_desc) {
			return_ACPI_STATUS (AE_NO_MEMORY);
		}

		/* Copy the integer to the buffer */

		new_buf = ret_desc->buffer.pointer;
		for (i = 0; i < acpi_gbl_integer_byte_width; i++) {
			new_buf[i] = (u8) (obj_desc->integer.value >> (i * 8));
		}
		break;


	case ACPI_TYPE_STRING:

		/*
		 * Create a new Buffer object
		 * Size will be the string length
		 */
		ret_desc = acpi_ut_create_buffer_object ((acpi_size) obj_desc->string.length);
		if (!ret_desc) {
			return_ACPI_STATUS (AE_NO_MEMORY);
		}

		/* Copy the string to the buffer */

		new_buf = ret_desc->buffer.pointer;
		ACPI_STRNCPY ((char *) new_buf, (char *) obj_desc->string.pointer,
			obj_desc->string.length);
		break;


	default:
		return_ACPI_STATUS (AE_TYPE);
	}

	/* Mark buffer initialized */

	ret_desc->common.flags |= AOPOBJ_DATA_VALID;

	/*
	 * If we are about to overwrite the original object on the operand stack,
	 * we must remove a reference on the original object because we are
	 * essentially removing it from the stack.
	 */
	if (*result_desc == obj_desc) {
		if (walk_state->opcode != AML_STORE_OP) {
			acpi_ut_remove_reference (obj_desc);
		}
	}

	*result_desc = ret_desc;
	return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_convert_ascii
 *
 * PARAMETERS:  Integer         - Value to be converted
 *              Base            - 10 or 16
 *              String          - Where the string is returned
 *              data_width      - Size of data item to be converted
 *
 * RETURN:      Actual string length
 *
 * DESCRIPTION: Convert an ACPI Integer to a hex or decimal string
 *
 ******************************************************************************/

u32
acpi_ex_convert_to_ascii (
	acpi_integer                    integer,
	u32                             base,
	u8                              *string,
	u8                              data_width)
{
	u32                             i;
	u32                             j;
	u32                             k = 0;
	char                            hex_digit;
	acpi_integer                    digit;
	u32                             remainder;
	u32                             length;
	u8                              leading_zero;


	ACPI_FUNCTION_ENTRY ();


	if (data_width < sizeof (acpi_integer)) {
		leading_zero = FALSE;
		length = data_width;
	}
	else {
		leading_zero = TRUE;
		length = sizeof (acpi_integer);
	}

	switch (base) {
	case 10:

		remainder = 0;
		for (i = ACPI_MAX_DECIMAL_DIGITS; i > 0; i--) {
			/* Divide by nth factor of 10 */

			digit = integer;
			for (j = 0; j < i; j++) {
				(void) acpi_ut_short_divide (&digit, 10, &digit, &remainder);
			}

			/* Create the decimal digit */

			if (remainder != 0) {
				leading_zero = FALSE;
			}

			if (!leading_zero) {
				string[k] = (u8) (ACPI_ASCII_ZERO + remainder);
				k++;
			}
		}
		break;


	case 16:

		/* Copy the integer to the buffer */

		for (i = 0, j = ((length * 2) -1); i < (length * 2); i++, j--) {

			hex_digit = acpi_ut_hex_to_ascii_char (integer, (j * 4));
			if (hex_digit != ACPI_ASCII_ZERO) {
				leading_zero = FALSE;
			}

			if (!leading_zero) {
				string[k] = (u8) hex_digit;
				k++;
			}
		}
		break;


	default:
		break;
	}

	/*
	 * Since leading zeros are supressed, we must check for the case where
	 * the integer equals 0
	 *
	 * Finally, null terminate the string and return the length
	 */
	if (!k) {
		string [0] = ACPI_ASCII_ZERO;
		k = 1;
	}

	string [k] = 0;
	return (k);
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_convert_to_string
 *
 * PARAMETERS:  obj_desc        - Object to be converted.  Must be an
 *                                  Integer, Buffer, or String
 *              result_desc     - Where the string object is returned
 *              Base            - 10 or 16
 *              max_length      - Max length of the returned string
 *              walk_state      - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert an ACPI Object to a string
 *
 ******************************************************************************/

acpi_status
acpi_ex_convert_to_string (
	union acpi_operand_object       *obj_desc,
	union acpi_operand_object       **result_desc,
	u32                             base,
	u32                             max_length,
	struct acpi_walk_state          *walk_state)
{
	union acpi_operand_object       *ret_desc;
	u8                              *new_buf;
	u8                              *pointer;
	u32                             string_length;
	u32                             i;


	ACPI_FUNCTION_TRACE_PTR ("ex_convert_to_string", obj_desc);


	switch (ACPI_GET_OBJECT_TYPE (obj_desc)) {
	case ACPI_TYPE_STRING:

		if (max_length >= obj_desc->string.length) {
			*result_desc = obj_desc;
			return_ACPI_STATUS (AE_OK);
		}
		else {
			/* Must copy the string first and then truncate it */

			return_ACPI_STATUS (AE_NOT_IMPLEMENTED);
		}


	case ACPI_TYPE_INTEGER:

		string_length = acpi_gbl_integer_byte_width * 2;
		if (base == 10) {
			string_length = ACPI_MAX_DECIMAL_DIGITS;
		}

		/*
		 * Create a new String
		 */
		ret_desc = acpi_ut_create_internal_object (ACPI_TYPE_STRING);
		if (!ret_desc) {
			return_ACPI_STATUS (AE_NO_MEMORY);
		}

		/* Need enough space for one ASCII integer plus null terminator */

		new_buf = ACPI_MEM_CALLOCATE ((acpi_size) string_length + 1);
		if (!new_buf) {
			ACPI_REPORT_ERROR
				(("ex_convert_to_string: Buffer allocation failure\n"));
			acpi_ut_remove_reference (ret_desc);
			return_ACPI_STATUS (AE_NO_MEMORY);
		}

		/* Convert */

		i = acpi_ex_convert_to_ascii (obj_desc->integer.value, base, new_buf, sizeof (acpi_integer));

		/* Null terminate at the correct place */

		if (max_length < i) {
			new_buf[max_length] = 0;
			ret_desc->string.length = max_length;
		}
		else {
			new_buf [i] = 0;
			ret_desc->string.length = i;
		}

		ret_desc->buffer.pointer = new_buf;
		break;


	case ACPI_TYPE_BUFFER:

		/* Find the string length */

		pointer = obj_desc->buffer.pointer;
		for (string_length = 0; string_length < obj_desc->buffer.length; string_length++) {
			/* Exit on null terminator */

			if (!pointer[string_length]) {
				break;
			}
		}

		if (max_length > ACPI_MAX_STRING_CONVERSION) {
			if (string_length > ACPI_MAX_STRING_CONVERSION) {
				return_ACPI_STATUS (AE_AML_STRING_LIMIT);
			}
		}

		/*
		 * Create a new string object
		 */
		ret_desc = acpi_ut_create_internal_object (ACPI_TYPE_STRING);
		if (!ret_desc) {
			return_ACPI_STATUS (AE_NO_MEMORY);
		}

		/* String length is the lesser of the Max or the actual length */

		if (max_length < string_length) {
			string_length = max_length;
		}

		new_buf = ACPI_MEM_CALLOCATE ((acpi_size) string_length + 1);
		if (!new_buf) {
			ACPI_REPORT_ERROR
				(("ex_convert_to_string: Buffer allocation failure\n"));
			acpi_ut_remove_reference (ret_desc);
			return_ACPI_STATUS (AE_NO_MEMORY);
		}

		/* Copy the appropriate number of buffer characters */

		ACPI_MEMCPY (new_buf, pointer, string_length);

		/* Null terminate */

		new_buf [string_length] = 0;
		ret_desc->buffer.pointer = new_buf;
		ret_desc->string.length = string_length;
		break;


	default:
		return_ACPI_STATUS (AE_TYPE);
	}

	/*
	 * If we are about to overwrite the original object on the operand stack,
	 * we must remove a reference on the original object because we are
	 * essentially removing it from the stack.
	 */
	if (*result_desc == obj_desc) {
		if (walk_state->opcode != AML_STORE_OP) {
			acpi_ut_remove_reference (obj_desc);
		}
	}

	*result_desc = ret_desc;
	return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    acpi_ex_convert_to_target_type
 *
 * PARAMETERS:  destination_type    - Current type of the destination
 *              source_desc         - Source object to be converted.
 *              result_desc         - Where the converted object is returned
 *              walk_state          - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Implements "implicit conversion" rules for storing an object.
 *
 ******************************************************************************/

acpi_status
acpi_ex_convert_to_target_type (
	acpi_object_type                destination_type,
	union acpi_operand_object       *source_desc,
	union acpi_operand_object       **result_desc,
	struct acpi_walk_state          *walk_state)
{
	acpi_status                     status = AE_OK;


	ACPI_FUNCTION_TRACE ("ex_convert_to_target_type");


	/* Default behavior */

	*result_desc = source_desc;

	/*
	 * If required by the target,
	 * perform implicit conversion on the source before we store it.
	 */
	switch (GET_CURRENT_ARG_TYPE (walk_state->op_info->runtime_args)) {
	case ARGI_SIMPLE_TARGET:
	case ARGI_FIXED_TARGET:
	case ARGI_INTEGER_REF:      /* Handles Increment, Decrement cases */

		switch (destination_type) {
		case ACPI_TYPE_LOCAL_REGION_FIELD:
			/*
			 * Named field can always handle conversions
			 */
			break;

		default:
			/* No conversion allowed for these types */

			if (destination_type != ACPI_GET_OBJECT_TYPE (source_desc)) {
				ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
					"Explicit operator, will store (%s) over existing type (%s)\n",
					acpi_ut_get_object_type_name (source_desc),
					acpi_ut_get_type_name (destination_type)));
				status = AE_TYPE;
			}
		}
		break;


	case ARGI_TARGETREF:

		switch (destination_type) {
		case ACPI_TYPE_INTEGER:
		case ACPI_TYPE_BUFFER_FIELD:
		case ACPI_TYPE_LOCAL_BANK_FIELD:
		case ACPI_TYPE_LOCAL_INDEX_FIELD:
			/*
			 * These types require an Integer operand.  We can convert
			 * a Buffer or a String to an Integer if necessary.
			 */
			status = acpi_ex_convert_to_integer (source_desc, result_desc, walk_state);
			break;


		case ACPI_TYPE_STRING:

			/*
			 * The operand must be a String.  We can convert an
			 * Integer or Buffer if necessary
			 */
			status = acpi_ex_convert_to_string (source_desc, result_desc, 16, ACPI_UINT32_MAX, walk_state);
			break;


		case ACPI_TYPE_BUFFER:

			/*
			 * The operand must be a Buffer.  We can convert an
			 * Integer or String if necessary
			 */
			status = acpi_ex_convert_to_buffer (source_desc, result_desc, walk_state);
			break;


		default:
			ACPI_REPORT_ERROR (("Bad destination type during conversion: %X\n",
				destination_type));
			status = AE_AML_INTERNAL;
			break;
		}
		break;


	case ARGI_REFERENCE:
		/*
		 * create_xxxx_field cases - we are storing the field object into the name
		 */
		break;


	default:
		ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
			"Unknown Target type ID 0x%X Op %s dest_type %s\n",
			GET_CURRENT_ARG_TYPE (walk_state->op_info->runtime_args),
			walk_state->op_info->name, acpi_ut_get_type_name (destination_type)));

		ACPI_REPORT_ERROR (("Bad Target Type (ARGI): %X\n",
			GET_CURRENT_ARG_TYPE (walk_state->op_info->runtime_args)))
		status = AE_AML_INTERNAL;
	}

	/*
	 * Source-to-Target conversion semantics:
	 *
	 * If conversion to the target type cannot be performed, then simply
	 * overwrite the target with the new object and type.
	 */
	if (status == AE_TYPE) {
		status = AE_OK;
	}

	return_ACPI_STATUS (status);
}


