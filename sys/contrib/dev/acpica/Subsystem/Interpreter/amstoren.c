
/******************************************************************************
 *
 * Module Name: amstoren - AML Interpreter object store support,
 *                         Store to Node (namespace object)
 *              $Revision: 23 $
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

#define __AMSTOREN_C__

#include "acpi.h"
#include "acparser.h"
#include "acdispat.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "actables.h"


#define _COMPONENT          INTERPRETER
        MODULE_NAME         ("amstoren")


/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlStoreObjectToNode
 *
 * PARAMETERS:  *ValDesc            - Value to be stored
 *              *Node           - Named object to recieve the value
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Store the object to the named object.
 *
 *              The Assignment of an object to a named object is handled here
 *              The val passed in will replace the current value (if any)
 *              with the input value.
 *
 *              When storing into an object the data is converted to the
 *              target object type then stored in the object.  This means
 *              that the target object type (for an initialized target) will
 *              not be changed by a store operation.
 *
 *              NOTE: the global lock is acquired early.  This will result
 *              in the global lock being held a bit longer.  Also, if the
 *              function fails during set up we may get the lock when we
 *              don't really need it.  I don't think we care.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlStoreObjectToNode (
    ACPI_OPERAND_OBJECT     *ValDesc,
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;
    UINT8                   *Buffer = NULL;
    UINT32                  Length = 0;
    UINT32                  Mask;
    UINT32                  NewValue;
    BOOLEAN                 Locked = FALSE;
    UINT8                   *Location=NULL;
    ACPI_OPERAND_OBJECT     *DestDesc;
    OBJECT_TYPE_INTERNAL    DestinationType = ACPI_TYPE_ANY;


    FUNCTION_TRACE ("AmlStoreObjectToNte");

    DEBUG_PRINT (ACPI_INFO,
        ("entered AcpiAmlStoreObjectToNode: NamedObj=%p, Obj=%p\n",
        Node, ValDesc));

    /*
     *  Assuming the parameters are valid!!!
     */
    ACPI_ASSERT((Node) && (ValDesc));

    DestinationType = AcpiNsGetType (Node);

    DEBUG_PRINT (ACPI_INFO, ("AmlStoreObjectToNte: Storing %s into %s\n",
        AcpiCmGetTypeName (ValDesc->Common.Type),
        AcpiCmGetTypeName (DestinationType)));

    /*
     *  First ensure we have a value that can be stored in the target
     */
    switch (DestinationType)
    {
        /* Type of Name's existing value */

    case INTERNAL_TYPE_ALIAS:

        /*
         *  Aliases are resolved by AcpiAmlPrepOperands
         */

        DEBUG_PRINT (ACPI_WARN,
            ("AmlStoreObjectToNte: Store into Alias - should never happen\n"));
        Status = AE_AML_INTERNAL;
        break;


    case INTERNAL_TYPE_BANK_FIELD:
    case INTERNAL_TYPE_INDEX_FIELD:
    case ACPI_TYPE_FIELD_UNIT:
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
                    ("AmlStoreObjectToNte: Value assigned to %s must be Number, not %s\n",
                    AcpiCmGetTypeName (DestinationType),
                    AcpiCmGetTypeName (ValDesc->Common.Type)));
                Status = AE_AML_OPERAND_TYPE;
            }
        }

        break;

    case ACPI_TYPE_STRING:
    case ACPI_TYPE_BUFFER:
    case INTERNAL_TYPE_DEF_FIELD:

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
                    ("AmlStoreObjectToNte: Assign wrong type %s to %s (must be type Num/Str/Buf)\n",
                    AcpiCmGetTypeName (ValDesc->Common.Type),
                    AcpiCmGetTypeName (DestinationType)));
                Status = AE_AML_OPERAND_TYPE;
            }
        }
        break;


    case ACPI_TYPE_PACKAGE:

        /*
         *  TBD: [Unhandled] Not real sure what to do here
         */
        Status = AE_NOT_IMPLEMENTED;
        break;


    default:

        /*
         * All other types than Alias and the various Fields come here.
         * Store ValDesc as the new value of the Name, and set
         * the Name's type to that of the value being stored in it.
         * ValDesc reference count is incremented by AttachObject.
         */

        Status = AcpiNsAttachObject (Node, ValDesc, ValDesc->Common.Type);

        DEBUG_PRINT (ACPI_INFO,
            ("AmlStoreObjectToNte: Store %s into %s via Attach\n",
            AcpiCmGetTypeName (ValDesc->Common.Type),
            AcpiCmGetTypeName (DestinationType)));

        goto CleanUpAndBailOut;
        break;
    }

    /* Exit now if failure above */

    if (ACPI_FAILURE (Status))
    {
        goto CleanUpAndBailOut;
    }

    /*
     *  Get descriptor for object attached to Node
     */
    DestDesc = AcpiNsGetAttachedObject (Node);
    if (!DestDesc)
    {
        /*
         *  There is no existing object attached to this Node
         */
        DEBUG_PRINT (ACPI_ERROR,
            ("AmlStoreObjectToNte: Internal error - no destination object for %4.4s type %d\n",
            &Node->Name, DestinationType));
        Status = AE_AML_INTERNAL;
        goto CleanUpAndBailOut;
    }

    /*
     *  Make sure the destination Object is the same as the Node
     */
    if (DestDesc->Common.Type != (UINT8) DestinationType)
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("AmlStoreObjectToNte: Internal error - Name %4.4s type %d does not match value-type %d at %p\n",
            &Node->Name, AcpiNsGetType (Node),
            DestDesc->Common.Type, DestDesc));
        Status = AE_AML_INTERNAL;
        goto CleanUpAndBailOut;
    }

    /*
     * AcpiEverything is ready to execute now,  We have
     * a value we can handle, just perform the update
     */

    switch (DestinationType)
    {
        /* Type of Name's existing value */

    case INTERNAL_TYPE_BANK_FIELD:

        /*
         * Get the global lock if needed
         */
        Locked = AcpiAmlAcquireGlobalLock (DestDesc->BankField.LockRule);

        /*
         *  Set Bank value to select proper Bank
         *  Perform the update (Set Bank Select)
         */

        Status = AcpiAmlAccessNamedField (ACPI_WRITE,
                                DestDesc->BankField.BankSelect,
                                &DestDesc->BankField.Value,
                                sizeof (DestDesc->BankField.Value));
        if (ACPI_SUCCESS (Status))
        {
            /* Set bank select successful, set data value  */

            Status = AcpiAmlAccessNamedField (ACPI_WRITE,
                                DestDesc->BankField.BankSelect,
                                &ValDesc->BankField.Value,
                                sizeof (ValDesc->BankField.Value));
        }

        break;


    case INTERNAL_TYPE_DEF_FIELD:

        /*
         * Get the global lock if needed
         */
        Locked = AcpiAmlAcquireGlobalLock (ValDesc->Field.LockRule);

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

        Status = AcpiAmlAccessNamedField (ACPI_WRITE,
                                    Node, Buffer, Length);

        break;      /* Global Lock released below   */


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
             *  archetecture independance.  Just clear the whole thing
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
         *  Buffer is a static allocation,
         *  only place what will fit in the buffer.
         */
        if (Length <= DestDesc->Buffer.Length)
        {
            /*
             *  Zero fill first, not willing to do pointer arithmetic for
             *  archetecture independence.  Just clear the whole thing
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
                ("AmlStoreObjectToNte: Truncating src buffer from %d to %d\n",
                Length, DestDesc->Buffer.Length));
        }
        break;


    case INTERNAL_TYPE_INDEX_FIELD:

        /*
         * Get the global lock if needed
         */
        Locked = AcpiAmlAcquireGlobalLock (DestDesc->IndexField.LockRule);

        /*
         *  Set Index value to select proper Data register
         *  perform the update (Set index)
         */

        Status = AcpiAmlAccessNamedField (ACPI_WRITE,
                                DestDesc->IndexField.Index,
                                &DestDesc->IndexField.Value,
                                sizeof (DestDesc->IndexField.Value));

        DEBUG_PRINT (ACPI_INFO,
            ("AmlStoreObjectToNte: IndexField: set index returned %s\n",
            AcpiCmFormatException (Status)));

        if (ACPI_SUCCESS (Status))
        {
            /* set index successful, next set Data value */

            Status = AcpiAmlAccessNamedField (ACPI_WRITE,
                                DestDesc->IndexField.Data,
                                &ValDesc->Number.Value,
                                sizeof (ValDesc->Number.Value));
            DEBUG_PRINT (ACPI_INFO,
                ("AmlStoreObjectToNte: IndexField: set data returned %s\n",
                AcpiCmFormatException (Status)));
        }
        break;


    case ACPI_TYPE_FIELD_UNIT:


        /*
         * If the Field Buffer and Index have not been previously evaluated,
         * evaluate them and save the results.
         */
        if (!(DestDesc->Common.Flags & AOPOBJ_DATA_VALID))
        {
            Status = AcpiDsGetFieldUnitArguments (DestDesc);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
        }

        if ((!DestDesc->FieldUnit.Container ||
            ACPI_TYPE_BUFFER != DestDesc->FieldUnit.Container->Common.Type))
        {
            DUMP_PATHNAME (Node,
                "AmlStoreObjectToNte: FieldUnit: Bad container in ",
                ACPI_ERROR, _COMPONENT);
            DUMP_ENTRY (Node, ACPI_ERROR);
            DEBUG_PRINT (ACPI_ERROR,
                ("Container: %p", DestDesc->FieldUnit.Container));

            if (DestDesc->FieldUnit.Container)
            {
                DEBUG_PRINT_RAW (ACPI_ERROR, (" Type %d",
                    DestDesc->FieldUnit.Container->Common.Type));
            }
            DEBUG_PRINT_RAW (ACPI_ERROR, ("\n"));

            Status = AE_AML_INTERNAL;
            goto CleanUpAndBailOut;
        }

        /*
         *  Get the global lock if needed
         */
        Locked = AcpiAmlAcquireGlobalLock (DestDesc->FieldUnit.LockRule);

        /*
         * TBD: [Unhandled] REMOVE this limitation
         * Make sure the operation is within the limits of our implementation
         * this is not a Spec limitation!!
         */
        if (DestDesc->FieldUnit.Length + DestDesc->FieldUnit.BitOffset > 32)
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("AmlStoreObjectToNte: FieldUnit: Implementation limitation - Field exceeds UINT32\n"));
            Status = AE_NOT_IMPLEMENTED;
            goto CleanUpAndBailOut;
        }

        /* Field location is (base of buffer) + (byte offset) */

        Location = DestDesc->FieldUnit.Container->Buffer.Pointer
                        + DestDesc->FieldUnit.Offset;

        /*
         * Construct Mask with 1 bits where the field is,
         * 0 bits elsewhere
         */
        Mask = ((UINT32) 1 << DestDesc->FieldUnit.Length) - ((UINT32)1
                            << DestDesc->FieldUnit.BitOffset);

        DEBUG_PRINT (TRACE_EXEC,
            ("** Store %lx in buffer %p byte %ld bit %d width %d addr %p mask %08lx\n",
            ValDesc->Number.Value,
            DestDesc->FieldUnit.Container->Buffer.Pointer,
            DestDesc->FieldUnit.Offset, DestDesc->FieldUnit.BitOffset,
            DestDesc->FieldUnit.Length,Location, Mask));

        /* Zero out the field in the buffer */

        MOVE_UNALIGNED32_TO_32 (&NewValue, Location);
        NewValue &= ~Mask;

        /*
         * Shift and mask the new value into position,
         * and or it into the buffer.
         */
        NewValue |= (ValDesc->Number.Value << DestDesc->FieldUnit.BitOffset) &
                    Mask;

        /* Store back the value */

        MOVE_UNALIGNED32_TO_32 (Location, &NewValue);

        DEBUG_PRINT (TRACE_EXEC, ("New Field value %08lx\n", NewValue));
        break;


    case ACPI_TYPE_NUMBER:


        DestDesc->Number.Value = ValDesc->Number.Value;

        /* Truncate value if we are executing from a 32-bit ACPI table */

        AcpiAmlTruncateFor32bitTable (DestDesc, WalkState);
        break;


    case ACPI_TYPE_PACKAGE:

        /*
         *  TBD: [Unhandled] Not real sure what to do here
         */
        Status = AE_NOT_IMPLEMENTED;
        break;


    default:

        /*
         * All other types than Alias and the various Fields come here.
         * Store ValDesc as the new value of the Name, and set
         * the Name's type to that of the value being stored in it.
         * ValDesc reference count is incremented by AttachObject.
         */

        DEBUG_PRINT (ACPI_WARN,
            ("AmlStoreObjectToNte: Store into %s not implemented\n",
            AcpiCmGetTypeName (AcpiNsGetType (Node))));

        Status = AE_NOT_IMPLEMENTED;
        break;
    }


CleanUpAndBailOut:

    /*
     * Release global lock if we acquired it earlier
     */
    AcpiAmlReleaseGlobalLock (Locked);

    return_ACPI_STATUS (Status);
}


