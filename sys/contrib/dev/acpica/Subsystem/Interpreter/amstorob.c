
/******************************************************************************
 *
 * Module Name: amstorob - AML Interpreter object store support, store to object
 *              $Revision: 17 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, Intel Corp.  All rights
 * reserved.
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

#define __AMSTOROB_C__

#include "acpi.h"
#include "acparser.h"
#include "acdispat.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "actables.h"


#define _COMPONENT          INTERPRETER
        MODULE_NAME         ("amstorob")


/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlStoreObjectToObject
 *
 * PARAMETERS:  *ValDesc            - Value to be stored
 *              *DestDesc           - Object to receive the value
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Store an object to another object.
 *
 *              The Assignment of an object to another (not named) object
 *              is handled here.
 *              The val passed in will replace the current value (if any)
 *              with the input value.
 *
 *              When storing into an object the data is converted to the
 *              target object type then stored in the object.  This means
 *              that the target object type (for an initialized target) will
 *              not be changed by a store operation.
 *
 *              This module allows destination types of Number, String,
 *              and Buffer.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlStoreObjectToObject (
    ACPI_OPERAND_OBJECT     *ValDesc,
    ACPI_OPERAND_OBJECT     *DestDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;
    UINT8                   *Buffer = NULL;
    UINT32                  Length = 0;
    OBJECT_TYPE_INTERNAL    DestinationType = DestDesc->Common.Type;


    FUNCTION_TRACE ("AmlStoreObjectToObject");

    DEBUG_PRINT (ACPI_INFO,
        ("entered AcpiAmlStoreObjectToObject: Dest=%p, Val=%p\n",
        DestDesc, ValDesc));

    /*
     *  Assuming the parameters are valid!!!
     */
    ACPI_ASSERT((DestDesc) && (ValDesc));

    DEBUG_PRINT (ACPI_INFO, ("AmlStoreObjectToObject: Storing %s into %s\n",
                    AcpiCmGetTypeName (ValDesc->Common.Type),
                    AcpiCmGetTypeName (DestDesc->Common.Type)));

    /*
     *  First ensure we have a value that can be stored in the target
     */
    switch (DestinationType)
    {
        /* Type of Name's existing value */

    case ACPI_TYPE_NUMBER:

        /*
         *  These cases all require only number values or values that
         *  can be converted to numbers.
         *
         *  If value is not a Number, try to resolve it to one.
         */

        if (ValDesc->Common.Type != ACPI_TYPE_NUMBER)
        {
            /*
             *  Initially not a number, convert
             */
            Status = AcpiAmlResolveToValue (&ValDesc, WalkState);
            if (ACPI_SUCCESS (Status) &&
                (ValDesc->Common.Type != ACPI_TYPE_NUMBER))
            {
                /*
                 *  Conversion successful but still not a number
                 */
                DEBUG_PRINT (ACPI_ERROR,
                    ("AmlStoreObjectToObject: Value assigned to %s must be Number, not %s\n",
                    AcpiCmGetTypeName (DestinationType),
                    AcpiCmGetTypeName (ValDesc->Common.Type)));
                Status = AE_AML_OPERAND_TYPE;
            }
        }

        break;

    case ACPI_TYPE_STRING:
    case ACPI_TYPE_BUFFER:

        /*
         *  Storing into a Field in a region or into a buffer or into
         *  a string all is essentially the same.
         *
         *  If value is not a valid type, try to resolve it to one.
         */

        if ((ValDesc->Common.Type != ACPI_TYPE_NUMBER) &&
            (ValDesc->Common.Type != ACPI_TYPE_BUFFER) &&
            (ValDesc->Common.Type != ACPI_TYPE_STRING))
        {
            /*
             *  Initially not a valid type, convert
             */
            Status = AcpiAmlResolveToValue (&ValDesc, WalkState);
            if (ACPI_SUCCESS (Status) &&
                (ValDesc->Common.Type != ACPI_TYPE_NUMBER) &&
                (ValDesc->Common.Type != ACPI_TYPE_BUFFER) &&
                (ValDesc->Common.Type != ACPI_TYPE_STRING))
            {
                /*
                 *  Conversion successful but still not a valid type
                 */
                DEBUG_PRINT (ACPI_ERROR,
                    ("AmlStoreObjectToObject: Assign wrong type %s to %s (must be type Num/Str/Buf)\n",
                    AcpiCmGetTypeName (ValDesc->Common.Type),
                    AcpiCmGetTypeName (DestinationType)));
                Status = AE_AML_OPERAND_TYPE;
            }
        }
        break;


    default:

        /*
         * TBD: [Unhandled] What other combinations must be implemented?
         */
        Status = AE_NOT_IMPLEMENTED;
        break;
    }

    /* Exit now if failure above */

    if (ACPI_FAILURE (Status))
    {
        goto CleanUpAndBailOut;
    }

    /*
     * AcpiEverything is ready to execute now,  We have
     * a value we can handle, just perform the update
     */

    switch (DestinationType)
    {

    case ACPI_TYPE_STRING:

        /*
         *  Perform the update
         */

        switch (ValDesc->Common.Type)
        {
        case ACPI_TYPE_NUMBER:
            Buffer = (UINT8 *) &ValDesc->Number.Value;
            Length = sizeof (ValDesc->Number.Value);
            break;

        case ACPI_TYPE_BUFFER:
            Buffer = (UINT8 *) ValDesc->Buffer.Pointer;
            Length = ValDesc->Buffer.Length;
            break;

        case ACPI_TYPE_STRING:
            Buffer = (UINT8 *) ValDesc->String.Pointer;
            Length = ValDesc->String.Length;
            break;
        }

        /*
         *  Setting a string value replaces the old string
         */

        if (Length < DestDesc->String.Length)
        {
            /*
             *  Zero fill, not willing to do pointer arithmetic for
             *  architecture independence.  Just clear the whole thing
             */
            MEMSET(DestDesc->String.Pointer, 0, DestDesc->String.Length);
            MEMCPY(DestDesc->String.Pointer, Buffer, Length);
        }
        else
        {
            /*
             *  Free the current buffer, then allocate a buffer
             *  large enough to hold the value
             */
            if ( DestDesc->String.Pointer &&
                !AcpiTbSystemTablePointer (DestDesc->String.Pointer))
            {
                /*
                 *  Only free if not a pointer into the DSDT
                 */

                AcpiCmFree(DestDesc->String.Pointer);
            }

            DestDesc->String.Pointer = AcpiCmAllocate (Length + 1);
            DestDesc->String.Length = Length;

            if (!DestDesc->String.Pointer)
            {
                Status = AE_NO_MEMORY;
                goto CleanUpAndBailOut;
            }

            MEMCPY(DestDesc->String.Pointer, Buffer, Length);
        }
        break;


    case ACPI_TYPE_BUFFER:

        /*
         *  Perform the update to the buffer
         */

        switch (ValDesc->Common.Type)
        {
        case ACPI_TYPE_NUMBER:
            Buffer = (UINT8 *) &ValDesc->Number.Value;
            Length = sizeof (ValDesc->Number.Value);
            break;

        case ACPI_TYPE_BUFFER:
            Buffer = (UINT8 *) ValDesc->Buffer.Pointer;
            Length = ValDesc->Buffer.Length;
            break;

        case ACPI_TYPE_STRING:
            Buffer = (UINT8 *) ValDesc->String.Pointer;
            Length = ValDesc->String.Length;
            break;
        }

        /*
         * If the buffer is uninitialized,
         *  memory needs to be allocated for the copy.
         */
        if(0 == DestDesc->Buffer.Length)
        {
            DestDesc->Buffer.Pointer = AcpiCmCallocate(Length);
            DestDesc->Buffer.Length = Length;

            if (!DestDesc->Buffer.Pointer)
            {
                Status = AE_NO_MEMORY;
                goto CleanUpAndBailOut;
            }
        }

        /*
         *  Buffer is a static allocation,
         *  only place what will fit in the buffer.
         */
        if (Length <= DestDesc->Buffer.Length)
        {
            /*
             *  Zero fill first, not willing to do pointer arithmetic for
             *  architecture independence.  Just clear the whole thing
             */
            MEMSET(DestDesc->Buffer.Pointer, 0, DestDesc->Buffer.Length);
            MEMCPY(DestDesc->Buffer.Pointer, Buffer, Length);
        }
        else
        {
            /*
             *  truncate, copy only what will fit
             */
            MEMCPY(DestDesc->Buffer.Pointer, Buffer, DestDesc->Buffer.Length);
            DEBUG_PRINT (ACPI_INFO,
                ("AmlStoreObjectToObject: Truncating src buffer from %d to %d\n",
                Length, DestDesc->Buffer.Length));
        }
        break;

    case ACPI_TYPE_NUMBER:

        DestDesc->Number.Value = ValDesc->Number.Value;

        /* Truncate value if we are executing from a 32-bit ACPI table */

        AcpiAmlTruncateFor32bitTable (DestDesc, WalkState);
        break;

    default:

        /*
         * All other types than Alias and the various Fields come here.
         * Store ValDesc as the new value of the Name, and set
         * the Name's type to that of the value being stored in it.
         * ValDesc reference count is incremented by AttachObject.
         */

        DEBUG_PRINT (ACPI_WARN,
            ("AmlStoreObjectToObject: Store into %s not implemented\n",
            AcpiCmGetTypeName (DestDesc->Common.Type)));

        Status = AE_NOT_IMPLEMENTED;
        break;
    }

CleanUpAndBailOut:

    return_ACPI_STATUS (Status);
}

