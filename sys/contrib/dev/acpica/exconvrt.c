/******************************************************************************
 *
 * Module Name: exconvrt - Object conversion routines
 *              $Revision: 59 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2004, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/


#define __EXCONVRT_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acinterp.h>
#include <contrib/dev/acpica/amlcode.h>


#define _COMPONENT          ACPI_EXECUTER
        ACPI_MODULE_NAME    ("exconvrt")


/*******************************************************************************
 *
 * FUNCTION:    AcpiExConvertToInteger
 *
 * PARAMETERS:  ObjDesc         - Object to be converted.  Must be an
 *                                Integer, Buffer, or String
 *              ResultDesc      - Where the new Integer object is returned
 *              Flags           - Used for string conversion
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert an ACPI Object to an integer.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExConvertToInteger (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     **ResultDesc,
    UINT32                  Flags)
{
    ACPI_OPERAND_OBJECT     *ReturnDesc;
    UINT8                   *Pointer;
    ACPI_INTEGER            Result;
    UINT32                  i;
    UINT32                  Count;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE_PTR ("ExConvertToInteger", ObjDesc);


    switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
    {
    case ACPI_TYPE_INTEGER:

        /* No conversion necessary */

        *ResultDesc = ObjDesc;
        return_ACPI_STATUS (AE_OK);

    case ACPI_TYPE_BUFFER:
    case ACPI_TYPE_STRING:

        /* Note: Takes advantage of common buffer/string fields */

        Pointer = ObjDesc->Buffer.Pointer;
        Count   = ObjDesc->Buffer.Length;
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
    Result = 0;

    /*
     * String conversion is different than Buffer conversion
     */
    switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
    {
    case ACPI_TYPE_STRING:

        /*
         * Convert string to an integer - for most cases, the string must be
         * hexadecimal as per the ACPI specification.  The only exception (as
         * of ACPI 3.0) is that the ToInteger() operator allows both decimal
         * and hexadecimal strings (hex prefixed with "0x").
         */
        Status = AcpiUtStrtoul64 ((char *) Pointer, Flags, &Result);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
        break;


    case ACPI_TYPE_BUFFER:

        /* Transfer no more than an integer's worth of data */

        if (Count > AcpiGbl_IntegerByteWidth)
        {
            Count = AcpiGbl_IntegerByteWidth;
        }

        /*
         * Convert buffer to an integer - we simply grab enough raw data
         * from the buffer to fill an integer
         */
        for (i = 0; i < Count; i++)
        {
            /*
             * Get next byte and shift it into the Result.
             * Little endian is used, meaning that the first byte of the buffer
             * is the LSB of the integer
             */
            Result |= (((ACPI_INTEGER) Pointer[i]) << (i * 8));
        }
        break;


    default:
        /* No other types can get here */
        break;
    }

    /*
     * Create a new integer
     */
    ReturnDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
    if (!ReturnDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Save the Result */

    ReturnDesc->Integer.Value = Result;
    AcpiExTruncateFor32bitTable (ReturnDesc);
    *ResultDesc = ReturnDesc;
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExConvertToBuffer
 *
 * PARAMETERS:  ObjDesc         - Object to be converted.  Must be an
 *                                Integer, Buffer, or String
 *              ResultDesc      - Where the new buffer object is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert an ACPI Object to a Buffer
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExConvertToBuffer (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     **ResultDesc)
{
    ACPI_OPERAND_OBJECT     *ReturnDesc;
    UINT8                   *NewBuf;


    ACPI_FUNCTION_TRACE_PTR ("ExConvertToBuffer", ObjDesc);


    switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
    {
    case ACPI_TYPE_BUFFER:

        /* No conversion necessary */

        *ResultDesc = ObjDesc;
        return_ACPI_STATUS (AE_OK);


    case ACPI_TYPE_INTEGER:

        /*
         * Create a new Buffer object.
         * Need enough space for one integer
         */
        ReturnDesc = AcpiUtCreateBufferObject (AcpiGbl_IntegerByteWidth);
        if (!ReturnDesc)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        /* Copy the integer to the buffer, LSB first */

        NewBuf = ReturnDesc->Buffer.Pointer;
        ACPI_MEMCPY (NewBuf,
                        &ObjDesc->Integer.Value,
                        AcpiGbl_IntegerByteWidth);
        break;


    case ACPI_TYPE_STRING:

        /*
         * Create a new Buffer object
         * Size will be the string length
         */
        ReturnDesc = AcpiUtCreateBufferObject ((ACPI_SIZE) ObjDesc->String.Length);
        if (!ReturnDesc)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        /* Copy the string to the buffer */

        NewBuf = ReturnDesc->Buffer.Pointer;
        ACPI_STRNCPY ((char *) NewBuf, (char *) ObjDesc->String.Pointer,
            ObjDesc->String.Length);
        break;


    default:
        return_ACPI_STATUS (AE_TYPE);
    }

    /* Mark buffer initialized */

    ReturnDesc->Common.Flags |= AOPOBJ_DATA_VALID;
    *ResultDesc = ReturnDesc;
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExConvertToAscii
 *
 * PARAMETERS:  Integer         - Value to be converted
 *              Base            - ACPI_STRING_DECIMAL or ACPI_STRING_HEX
 *              String          - Where the string is returned
 *              DataWidth       - Size of data item to be converted, in bytes
 *
 * RETURN:      Actual string length
 *
 * DESCRIPTION: Convert an ACPI Integer to a hex or decimal string
 *
 ******************************************************************************/

UINT32
AcpiExConvertToAscii (
    ACPI_INTEGER            Integer,
    UINT16                  Base,
    UINT8                   *String,
    UINT8                   DataWidth)
{
    ACPI_INTEGER            Digit;
    ACPI_NATIVE_UINT        i;
    ACPI_NATIVE_UINT        j;
    ACPI_NATIVE_UINT        k = 0;
    ACPI_NATIVE_UINT        HexLength;
    ACPI_NATIVE_UINT        DecimalLength;
    UINT32                  Remainder;
    BOOLEAN                 SupressZeros;


    ACPI_FUNCTION_ENTRY ();


    switch (Base)
    {
    case 10:

        /* Setup max length for the decimal number */

        switch (DataWidth)
        {
        case 1:
            DecimalLength = ACPI_MAX8_DECIMAL_DIGITS;
            break;

        case 4:
            DecimalLength = ACPI_MAX32_DECIMAL_DIGITS;
            break;

        case 8:
        default:
            DecimalLength = ACPI_MAX64_DECIMAL_DIGITS;
            break;
        }

        SupressZeros = TRUE;     /* No leading zeros */
        Remainder = 0;

        for (i = DecimalLength; i > 0; i--)
        {
            /* Divide by nth factor of 10 */

            Digit = Integer;
            for (j = 0; j < i; j++)
            {
                (void) AcpiUtShortDivide (Digit, 10, &Digit, &Remainder);
            }

            /* Handle leading zeros */

            if (Remainder != 0)
            {
                SupressZeros = FALSE;
            }

            if (!SupressZeros)
            {
                String[k] = (UINT8) (ACPI_ASCII_ZERO + Remainder);
                k++;
            }
        }
        break;

    case 16:

        HexLength = ACPI_MUL_2 (DataWidth); /* 2 ascii hex chars per data byte */

        for (i = 0, j = (HexLength-1); i < HexLength; i++, j--)
        {
            /* Get one hex digit, most significant digits first */

            String[k] = (UINT8) AcpiUtHexToAsciiChar (Integer, ACPI_MUL_4 (j));
            k++;
        }
        break;

    default:
        return (0);
    }

    /*
     * Since leading zeros are supressed, we must check for the case where
     * the integer equals 0
     *
     * Finally, null terminate the string and return the length
     */
    if (!k)
    {
        String [0] = ACPI_ASCII_ZERO;
        k = 1;
    }

    String [k] = 0;
    return ((UINT32) k);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExConvertToString
 *
 * PARAMETERS:  ObjDesc         - Object to be converted.  Must be an
 *                                Integer, Buffer, or String
 *              ResultDesc      - Where the string object is returned
 *              Type            - String flags (base and conversion type)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert an ACPI Object to a string
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExConvertToString (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     **ResultDesc,
    UINT32                  Type)
{
    ACPI_OPERAND_OBJECT     *ReturnDesc;
    UINT8                   *NewBuf;
    UINT32                  StringLength = 0;
    UINT16                  Base = 16;
    UINT32                  i;
    UINT8                   Separator = ',';


    ACPI_FUNCTION_TRACE_PTR ("ExConvertToString", ObjDesc);


    switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
    {
    case ACPI_TYPE_STRING:

        /* No conversion necessary */

        *ResultDesc = ObjDesc;
        return_ACPI_STATUS (AE_OK);


    case ACPI_TYPE_INTEGER:

        switch (Type)
        {
        case ACPI_EXPLICIT_CONVERT_DECIMAL:

            /* Make room for maximum decimal number */

            StringLength = ACPI_MAX_DECIMAL_DIGITS;
            Base = 10;
            break;

        default:

            /* Two hex string characters for each integer byte */

            StringLength = ACPI_MUL_2 (AcpiGbl_IntegerByteWidth);
            break;
        }

        /*
         * Create a new String
         * Need enough space for one ASCII integer (plus null terminator)
         */
        ReturnDesc = AcpiUtCreateStringObject ((ACPI_SIZE) StringLength);
        if (!ReturnDesc)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        NewBuf = ReturnDesc->Buffer.Pointer;

        /* Convert integer to string */

        StringLength = AcpiExConvertToAscii (ObjDesc->Integer.Value, Base,
                            NewBuf, AcpiGbl_IntegerByteWidth);

        /* Null terminate at the correct place */

        ReturnDesc->String.Length = StringLength;
        NewBuf [StringLength] = 0;
        break;


    case ACPI_TYPE_BUFFER:

        switch (Type)
        {
        case ACPI_EXPLICIT_CONVERT_DECIMAL: /* Used by ToDecimalString operator */
            /*
             * From ACPI: "If Data is a buffer, it is converted to a string of
             * decimal values separated by commas."
             */
            Base = 10;
            StringLength = ObjDesc->Buffer.Length; /* 4 chars for each decimal */

            /*lint -fallthrough */

        case ACPI_IMPLICIT_CONVERT_HEX:
            /*
             * From the ACPI spec:
             *"The entire contents of the buffer are converted to a string of
             * two-character hexadecimal numbers, each separated by a space."
             */
            if (Type == ACPI_IMPLICIT_CONVERT_HEX)
            {
                Separator = ' ';
            }

            /*lint -fallthrough */

        case ACPI_EXPLICIT_CONVERT_HEX:     /* Used by ToHexString operator */
            /*
             * From ACPI: "If Data is a buffer, it is converted to a string of
             * hexadecimal values separated by commas."
             */
            StringLength += (ObjDesc->Buffer.Length * 3);
            if (StringLength > ACPI_MAX_STRING_CONVERSION)  /* ACPI limit */
            {
                return_ACPI_STATUS (AE_AML_STRING_LIMIT);
            }

            /* Create a new string object and string buffer */

            ReturnDesc = AcpiUtCreateStringObject ((ACPI_SIZE) StringLength -1);
            if (!ReturnDesc)
            {
                return_ACPI_STATUS (AE_NO_MEMORY);
            }

            NewBuf = ReturnDesc->Buffer.Pointer;

            /*
             * Convert buffer bytes to hex or decimal values
             * (separated by commas)
             */
            for (i = 0; i < ObjDesc->Buffer.Length; i++)
            {
                NewBuf += AcpiExConvertToAscii (
                            (ACPI_INTEGER) ObjDesc->Buffer.Pointer[i], Base,
                            NewBuf, 1);
                *NewBuf++ = Separator; /* each separated by a comma or space */
            }

            /* Null terminate the string (overwrites final comma from above) */

            NewBuf--;
            *NewBuf = 0;

            /* Recalculate length */

            ReturnDesc->String.Length = (UINT32) 
                ACPI_STRLEN (ReturnDesc->String.Pointer);
            break;

        default:
            return_ACPI_STATUS (AE_BAD_PARAMETER);
        }
        break;

    default:
        return_ACPI_STATUS (AE_TYPE);
    }

    *ResultDesc = ReturnDesc;
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExConvertToTargetType
 *
 * PARAMETERS:  DestinationType     - Current type of the destination
 *              SourceDesc          - Source object to be converted.
 *              ResultDesc          - Where the converted object is returned
 *              WalkState           - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Implements "implicit conversion" rules for storing an object.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExConvertToTargetType (
    ACPI_OBJECT_TYPE        DestinationType,
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     **ResultDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE ("ExConvertToTargetType");


    /* Default behavior */

    *ResultDesc = SourceDesc;

    /*
     * If required by the target,
     * perform implicit conversion on the source before we store it.
     */
    switch (GET_CURRENT_ARG_TYPE (WalkState->OpInfo->RuntimeArgs))
    {
    case ARGI_SIMPLE_TARGET:
    case ARGI_FIXED_TARGET:
    case ARGI_INTEGER_REF:      /* Handles Increment, Decrement cases */

        switch (DestinationType)
        {
        case ACPI_TYPE_LOCAL_REGION_FIELD:
            /*
             * Named field can always handle conversions
             */
            break;

        default:
            /* No conversion allowed for these types */

            if (DestinationType != ACPI_GET_OBJECT_TYPE (SourceDesc))
            {
                ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
                    "Explicit operator, will store (%s) over existing type (%s)\n",
                    AcpiUtGetObjectTypeName (SourceDesc),
                    AcpiUtGetTypeName (DestinationType)));
                Status = AE_TYPE;
            }
        }
        break;


    case ARGI_TARGETREF:

        switch (DestinationType)
        {
        case ACPI_TYPE_INTEGER:
        case ACPI_TYPE_BUFFER_FIELD:
        case ACPI_TYPE_LOCAL_BANK_FIELD:
        case ACPI_TYPE_LOCAL_INDEX_FIELD:
            /*
             * These types require an Integer operand.  We can convert
             * a Buffer or a String to an Integer if necessary.
             */
            Status = AcpiExConvertToInteger (SourceDesc, ResultDesc,
                        16);
            break;


        case ACPI_TYPE_STRING:

            /*
             * The operand must be a String.  We can convert an
             * Integer or Buffer if necessary
             */
            Status = AcpiExConvertToString (SourceDesc, ResultDesc,
                        ACPI_IMPLICIT_CONVERT_HEX);
            break;


        case ACPI_TYPE_BUFFER:

            /*
             * The operand must be a Buffer.  We can convert an
             * Integer or String if necessary
             */
            Status = AcpiExConvertToBuffer (SourceDesc, ResultDesc);
            break;


        default:
            ACPI_REPORT_ERROR (("Bad destination type during conversion: %X\n",
                DestinationType));
            Status = AE_AML_INTERNAL;
            break;
        }
        break;


    case ARGI_REFERENCE:
        /*
         * CreateXxxxField cases - we are storing the field object into the name
         */
        break;


    default:
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Unknown Target type ID 0x%X Op %s DestType %s\n",
            GET_CURRENT_ARG_TYPE (WalkState->OpInfo->RuntimeArgs),
            WalkState->OpInfo->Name, AcpiUtGetTypeName (DestinationType)));

        ACPI_REPORT_ERROR (("Bad Target Type (ARGI): %X\n",
            GET_CURRENT_ARG_TYPE (WalkState->OpInfo->RuntimeArgs)))
        Status = AE_AML_INTERNAL;
    }

    /*
     * Source-to-Target conversion semantics:
     *
     * If conversion to the target type cannot be performed, then simply
     * overwrite the target with the new object and type.
     */
    if (Status == AE_TYPE)
    {
        Status = AE_OK;
    }

    return_ACPI_STATUS (Status);
}


