/******************************************************************************
 *
 * Module Name: exconvrt - Object conversion routines
 *              $Revision: 13 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, 2000, 2001, Intel Corp.
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

#include "acpi.h"
#include "acparser.h"
#include "acnamesp.h"
#include "acinterp.h"
#include "acevents.h"
#include "amlcode.h"
#include "acdispat.h"


#define _COMPONENT          ACPI_EXECUTER
        MODULE_NAME         ("exconvrt")


/*******************************************************************************
 *
 * FUNCTION:    AcpiExConvertToInteger
 *
 * PARAMETERS:  *ObjDesc        - Object to be converted.  Must be an
 *                                Integer, Buffer, or String
 *              WalkState       - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert an ACPI Object to an integer.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExConvertToInteger (
    ACPI_OPERAND_OBJECT     **ObjDesc,
    ACPI_WALK_STATE         *WalkState)
{
    UINT32                  i;
    ACPI_OPERAND_OBJECT     *RetDesc;
    UINT32                  Count;
    char                    *Pointer;
    ACPI_INTEGER            Result;
    UINT32                  IntegerSize = sizeof (ACPI_INTEGER);


    switch ((*ObjDesc)->Common.Type)
    {
    case ACPI_TYPE_INTEGER:
        return (AE_OK);

    case ACPI_TYPE_STRING:
        Pointer = (*ObjDesc)->String.Pointer;
        Count = (*ObjDesc)->String.Length;
        break;

    case ACPI_TYPE_BUFFER:
        Pointer = (char *) (*ObjDesc)->Buffer.Pointer;
        Count = (*ObjDesc)->Buffer.Length;
        break;

    default:
        return (AE_TYPE);
    }

    /*
     * Create a new integer
     */
    RetDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
    if (!RetDesc)
    {
        return (AE_NO_MEMORY);
    }


    /* Handle both ACPI 1.0 and ACPI 2.0 Integer widths */

    if (WalkState->MethodNode->Flags & ANOBJ_DATA_WIDTH_32)
    {
        /*
         * We are running a method that exists in a 32-bit ACPI table.
         * Truncate the value to 32 bits by zeroing out the upper 32-bit field
         */
        IntegerSize = sizeof (UINT32);
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

    /* Transfer no more than an integer's worth of data */

    if (Count > IntegerSize)
    {
        Count = IntegerSize;
    }

    /*
     * String conversion is different than Buffer conversion
     */
    switch ((*ObjDesc)->Common.Type)
    {
    case ACPI_TYPE_STRING:

        /* TBD: Need to use 64-bit STRTOUL */

        /*
         * Convert string to an integer
         * String must be hexadecimal as per the ACPI specification
         */

        Result = STRTOUL (Pointer, NULL, 16);
        break;


    case ACPI_TYPE_BUFFER:

        /*
         * Buffer conversion - we simply grab enough raw data from the
         * buffer to fill an integer
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
    }

    /* Save the Result, delete original descriptor, store new descriptor */

    RetDesc->Integer.Value = Result;

    if (WalkState->Opcode != AML_STORE_OP)
    {
        AcpiUtRemoveReference (*ObjDesc);
    }

    *ObjDesc = RetDesc;

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExConvertToBuffer
 *
 * PARAMETERS:  *ObjDesc        - Object to be converted.  Must be an
 *                                Integer, Buffer, or String
 *              WalkState       - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert an ACPI Object to an Buffer
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExConvertToBuffer (
    ACPI_OPERAND_OBJECT     **ObjDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     *RetDesc;
    UINT32                  i;
    UINT32                  IntegerSize = sizeof (ACPI_INTEGER);
    UINT8                   *NewBuf;


    switch ((*ObjDesc)->Common.Type)
    {
    case ACPI_TYPE_INTEGER:

        /*
         * Create a new Buffer
         */
        RetDesc = AcpiUtCreateInternalObject (ACPI_TYPE_BUFFER);
        if (!RetDesc)
        {
            return (AE_NO_MEMORY);
        }

        /* Handle both ACPI 1.0 and ACPI 2.0 Integer widths */

        if (WalkState->MethodNode->Flags & ANOBJ_DATA_WIDTH_32)
        {
            /*
             * We are running a method that exists in a 32-bit ACPI table.
             * Truncate the value to 32 bits by zeroing out the upper
             * 32-bit field
             */
            IntegerSize = sizeof (UINT32);
        }

        /* Need enough space for one integers */

        RetDesc->Buffer.Length = IntegerSize;
        NewBuf = AcpiUtCallocate (IntegerSize);
        if (!NewBuf)
        {
            REPORT_ERROR
                (("ExDyadic2R/ConcatOp: Buffer allocation failure\n"));
            AcpiUtRemoveReference (RetDesc);
            return (AE_NO_MEMORY);
        }

        /* Copy the integer to the buffer */

        for (i = 0; i < IntegerSize; i++)
        {
            NewBuf[i] = (UINT8) ((*ObjDesc)->Integer.Value >> (i * 8));
        }
        RetDesc->Buffer.Pointer = NewBuf;

        /* Return the new buffer descriptor */

        if (WalkState->Opcode != AML_STORE_OP)
        {
            AcpiUtRemoveReference (*ObjDesc);
        }
        *ObjDesc = RetDesc;
        break;


    case ACPI_TYPE_STRING:
        break;


    case ACPI_TYPE_BUFFER:
        break;


    default:
        return (AE_TYPE);
        break;
   }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExConvertToString
 *
 * PARAMETERS:  *ObjDesc        - Object to be converted.  Must be an
 *                                Integer, Buffer, or String
 *              WalkState       - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert an ACPI Object to a string
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExConvertToString (
    ACPI_OPERAND_OBJECT     **ObjDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     *RetDesc;
    UINT32                  i;
    UINT32                  Index;
    UINT32                  IntegerSize = sizeof (ACPI_INTEGER);
    UINT8                   *NewBuf;
    UINT8                   *Pointer;


    switch ((*ObjDesc)->Common.Type)
    {
    case ACPI_TYPE_INTEGER:

        /*
         * Create a new String
         */
        RetDesc = AcpiUtCreateInternalObject (ACPI_TYPE_STRING);
        if (!RetDesc)
        {
            return (AE_NO_MEMORY);
        }

        /* Handle both ACPI 1.0 and ACPI 2.0 Integer widths */

        if (WalkState->MethodNode->Flags & ANOBJ_DATA_WIDTH_32)
        {
            /*
             * We are running a method that exists in a 32-bit ACPI table.
             * Truncate the value to 32 bits by zeroing out the upper
             * 32-bit field
             */
            IntegerSize = sizeof (UINT32);
        }

        /* Need enough space for one ASCII integer plus null terminator */

        RetDesc->String.Length = (IntegerSize * 2) + 1;
        NewBuf = AcpiUtCallocate (RetDesc->String.Length);
        if (!NewBuf)
        {
            REPORT_ERROR
                (("ExConvertToString: Buffer allocation failure\n"));
            AcpiUtRemoveReference (RetDesc);
            return (AE_NO_MEMORY);
        }

        /* Copy the integer to the buffer */

        for (i = 0; i < (IntegerSize * 2); i++)
        {
            NewBuf[i] = AcpiGbl_HexToAscii [((*ObjDesc)->Integer.Value >> (i * 4)) & 0xF];
        }

        /* Null terminate */

        NewBuf [i] = 0;
        RetDesc->Buffer.Pointer = NewBuf;

        /* Return the new buffer descriptor */

        if (WalkState->Opcode != AML_STORE_OP)
        {
            AcpiUtRemoveReference (*ObjDesc);
        }
        *ObjDesc = RetDesc;

        return (AE_OK);


    case ACPI_TYPE_BUFFER:

        if (((*ObjDesc)->Buffer.Length * 3) > ACPI_MAX_STRING_CONVERSION)
        {
            return (AE_AML_STRING_LIMIT);
        }

        /*
         * Create a new String
         */
        RetDesc = AcpiUtCreateInternalObject (ACPI_TYPE_STRING);
        if (!RetDesc)
        {
            return (AE_NO_MEMORY);
        }

        /* Need enough space for one ASCII integer plus null terminator */

        RetDesc->String.Length = (*ObjDesc)->Buffer.Length * 3;
        NewBuf = AcpiUtCallocate (RetDesc->String.Length + 1);
        if (!NewBuf)
        {
            REPORT_ERROR
                (("ExConvertToString: Buffer allocation failure\n"));
            AcpiUtRemoveReference (RetDesc);
            return (AE_NO_MEMORY);
        }

        /*
         * Convert each byte of the buffer to two ASCII characters plus a space.
         */
        Pointer = (*ObjDesc)->Buffer.Pointer;
        Index = 0;
        for (i = 0; i < (*ObjDesc)->Buffer.Length; i++)
        {
            NewBuf[Index + 0] = AcpiGbl_HexToAscii [Pointer[i] & 0x0F];
            NewBuf[Index + 1] = AcpiGbl_HexToAscii [(Pointer[i] >> 4) & 0x0F];
            NewBuf[Index + 2] = ' ';
            Index += 3;
        }

        /* Null terminate */

        NewBuf [Index] = 0;
        RetDesc->Buffer.Pointer = NewBuf;

        /* Return the new buffer descriptor */

        if (WalkState->Opcode != AML_STORE_OP)
        {
            AcpiUtRemoveReference (*ObjDesc);
        }
        *ObjDesc = RetDesc;
        break;


    case ACPI_TYPE_STRING:
        break;


    default:
        return (AE_TYPE);
        break;
   }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExConvertToTargetType
 *
 * PARAMETERS:  *ObjDesc        - Object to be converted.
 *              WalkState       - Current method state
 *
 * RETURN:      Status
 *
 * DESCRIPTION:
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExConvertToTargetType (
    ACPI_OBJECT_TYPE8       DestinationType,
    ACPI_OPERAND_OBJECT     **ObjDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("ExConvertToTargetType");


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
        case INTERNAL_TYPE_REGION_FIELD:
            /*
             * Named field can always handle conversions
             */
            break;

        default:
            /* No conversion allowed for these types */

            if (DestinationType != (*ObjDesc)->Common.Type)
            {
                DEBUG_PRINTP (ACPI_ERROR,
                    ("Target does not allow conversion of type %s to %s\n",
                    AcpiUtGetTypeName ((*ObjDesc)->Common.Type),
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
        case INTERNAL_TYPE_BANK_FIELD:
        case INTERNAL_TYPE_INDEX_FIELD:
            /*
             * These types require an Integer operand.  We can convert
             * a Buffer or a String to an Integer if necessary.
             */
            Status = AcpiExConvertToInteger (ObjDesc, WalkState);
            break;


        case ACPI_TYPE_STRING:

            /*
             * The operand must be a String.  We can convert an
             * Integer or Buffer if necessary
             */
            Status = AcpiExConvertToString (ObjDesc, WalkState);
            break;


        case ACPI_TYPE_BUFFER:

            /*
             * The operand must be a String.  We can convert an
             * Integer or Buffer if necessary
             */
            Status = AcpiExConvertToBuffer (ObjDesc, WalkState);
            break;
        }
        break;


    case ARGI_REFERENCE:
        /*
         * CreateXxxxField cases - we are storing the field object into the name
         */
        break;


    default:
        DEBUG_PRINTP (ACPI_ERROR,
            ("Unknown Target type ID 0x%X Op %s DestType %s\n",
            GET_CURRENT_ARG_TYPE (WalkState->OpInfo->RuntimeArgs),
            WalkState->OpInfo->Name, AcpiUtGetTypeName (DestinationType)));

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


