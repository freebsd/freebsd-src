
/******************************************************************************
 *
 * Module Name: amstorob - AML Interpreter object store support, store to object
 *              $Revision: 22 $
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
 * FUNCTION:    AcpiAmlCopyBufferToBuffer
 *
 * PARAMETERS:  SourceDesc          - Source object to copy
 *              TargetDesc          - Destination object of the copy
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Copy a buffer object to another buffer object.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlCopyBufferToBuffer (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     *TargetDesc)
{
    UINT32                  Length;
    UINT8                   *Buffer;
   
    
    /*
     * We know that SourceDesc is a buffer by now
     */
    Buffer = (UINT8 *) SourceDesc->Buffer.Pointer;
    Length = SourceDesc->Buffer.Length;

    /*
     * Buffer is a static allocation,
     * only place what will fit in the buffer.
     */
    if (Length <= TargetDesc->Buffer.Length)
    {
        /* Clear existing buffer and copy in the new one */

        MEMSET(TargetDesc->Buffer.Pointer, 0, TargetDesc->Buffer.Length);
        MEMCPY(TargetDesc->Buffer.Pointer, Buffer, Length);
    }

    else
    {
        /*
         * Truncate the source, copy only what will fit
         */
        MEMCPY(TargetDesc->Buffer.Pointer, Buffer, TargetDesc->Buffer.Length);

        DEBUG_PRINT (ACPI_INFO,
            ("AmlStoreObjectToNode: Truncating src buffer from %X to %X\n",
            Length, TargetDesc->Buffer.Length));
    }

    return (AE_OK);
}




/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlCopyStringToString
 *
 * PARAMETERS:  SourceDesc          - Source object to copy
 *              TargetDesc          - Destination object of the copy
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Copy a String object to another String object
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlCopyStringToString (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     *TargetDesc)
{
    UINT32                  Length;
    UINT8                   *Buffer;


    /*
     * We know that SourceDesc is a string by now.
     */
    Buffer = (UINT8 *) SourceDesc->String.Pointer;
    Length = SourceDesc->String.Length;

    /*
     * Setting a string value replaces the old string
     */
    if (Length < TargetDesc->String.Length)
    {
        /* Clear old string and copy in the new one */

        MEMSET(TargetDesc->String.Pointer, 0, TargetDesc->String.Length);
        MEMCPY(TargetDesc->String.Pointer, Buffer, Length);
    }

    else
    {
        /*
         * Free the current buffer, then allocate a buffer
         * large enough to hold the value
         */
        if (TargetDesc->String.Pointer &&
            !AcpiTbSystemTablePointer (TargetDesc->String.Pointer))
        {
            /*
             * Only free if not a pointer into the DSDT
             */
            AcpiCmFree(TargetDesc->String.Pointer);
        }

        TargetDesc->String.Pointer = AcpiCmAllocate (Length + 1);
        TargetDesc->String.Length = Length;

        if (!TargetDesc->String.Pointer)
        {
            return (AE_NO_MEMORY);
        }

        MEMCPY(TargetDesc->String.Pointer, Buffer, Length);
    }

    return (AE_OK);
}





/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlCopyIntegerToIndexField
 *
 * PARAMETERS:  SourceDesc          - Source object to copy
 *              TargetDesc          - Destination object of the copy
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write an Integer to an Index Field
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlCopyIntegerToIndexField (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     *TargetDesc)
{
    ACPI_STATUS             Status;
    BOOLEAN                 Locked;


    /*
     * Get the global lock if needed
     */
    Locked = AcpiAmlAcquireGlobalLock (TargetDesc->IndexField.LockRule);

    /*
     * Set Index value to select proper Data register
     * perform the update (Set index)
     */
    Status = AcpiAmlAccessNamedField (ACPI_WRITE,
                            TargetDesc->IndexField.Index,
                            &TargetDesc->IndexField.Value,
                            sizeof (TargetDesc->IndexField.Value));
    if (ACPI_SUCCESS (Status))
    {
        /* SetIndex was successful, next set Data value */

        Status = AcpiAmlAccessNamedField (ACPI_WRITE,
                            TargetDesc->IndexField.Data,
                            &SourceDesc->Integer.Value,
                            sizeof (SourceDesc->Integer.Value));

        DEBUG_PRINT (ACPI_INFO,
            ("AmlStoreObjectToNode: IndexField: set data returned %s\n",
            AcpiCmFormatException (Status)));
    }

    else
    {
        DEBUG_PRINT (ACPI_INFO,
            ("AmlStoreObjectToNode: IndexField: set index returned %s\n",
            AcpiCmFormatException (Status)));
    }


    /*
     * Release global lock if we acquired it earlier
     */
    AcpiAmlReleaseGlobalLock (Locked);

    return (Status);
}



/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlCopyIntegerToBankField
 *
 * PARAMETERS:  SourceDesc          - Source object to copy
 *              TargetDesc          - Destination object of the copy
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write an Integer to a Bank Field
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlCopyIntegerToBankField (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     *TargetDesc)
{
    ACPI_STATUS             Status;
    BOOLEAN                 Locked;


    /*
     * Get the global lock if needed
     */
    Locked = AcpiAmlAcquireGlobalLock (TargetDesc->IndexField.LockRule);



    /*
     * Set Bank value to select proper Bank
     * Perform the update (Set Bank Select)
     */

    Status = AcpiAmlAccessNamedField (ACPI_WRITE,
                            TargetDesc->BankField.BankSelect,
                            &TargetDesc->BankField.Value,
                            sizeof (TargetDesc->BankField.Value));
    if (ACPI_SUCCESS (Status))
    {
        /* Set bank select successful, set data value  */

        Status = AcpiAmlAccessNamedField (ACPI_WRITE,
                            TargetDesc->BankField.BankSelect,
                            &SourceDesc->BankField.Value,
                            sizeof (SourceDesc->BankField.Value));
    }

    else
    {
        DEBUG_PRINT (ACPI_INFO,
            ("AmlStoreObjectToNode: BankField: set bakn returned %s\n",
            AcpiCmFormatException (Status)));
    }


    /*
     * Release global lock if we acquired it earlier
     */
    AcpiAmlReleaseGlobalLock (Locked);

    return (Status);
}




/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlCopyDataToNamedField
 *
 * PARAMETERS:  SourceDesc          - Source object to copy
 *              Node                - Destination Namespace node
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Copy raw data to a Named Field.  No implicit conversion
 *              is performed on the source object
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlCopyDataToNamedField (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_NAMESPACE_NODE     *Node)
{
    ACPI_STATUS             Status;
    BOOLEAN                 Locked;
    UINT32                  Length;
    UINT8                   *Buffer;


    /*
     * Named fields (CreateXxxField) - We don't perform any conversions on the
     * source operand, just use the raw data
     */
    switch (SourceDesc->Common.Type)
    {
    case ACPI_TYPE_INTEGER:
        Buffer = (UINT8 *) &SourceDesc->Integer.Value;
        Length = sizeof (SourceDesc->Integer.Value);
        break;

    case ACPI_TYPE_BUFFER:
        Buffer = (UINT8 *) SourceDesc->Buffer.Pointer;
        Length = SourceDesc->Buffer.Length;
        break;

    case ACPI_TYPE_STRING:
        Buffer = (UINT8 *) SourceDesc->String.Pointer;
        Length = SourceDesc->String.Length;
        break;

    default:
        return (AE_TYPE);
    }

    /*
     * Get the global lock if needed before the update
     * TBD: not needed!
     */
    Locked = AcpiAmlAcquireGlobalLock (SourceDesc->Field.LockRule);

    Status = AcpiAmlAccessNamedField (ACPI_WRITE,
                                Node, Buffer, Length);

    AcpiAmlReleaseGlobalLock (Locked);

    return (Status);
}




/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlCopyIntegerToFieldUnit
 *
 * PARAMETERS:  SourceDesc          - Source object to copy
 *              TargetDesc          - Destination object of the copy
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write an Integer to a Field Unit.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlCopyIntegerToFieldUnit (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     *TargetDesc)
{
    ACPI_STATUS             Status = AE_OK;
    UINT8                   *Location = NULL;
    UINT32                  Mask;
    UINT32                  NewValue;
    BOOLEAN                 Locked = FALSE;



    FUNCTION_TRACE ("AmlCopyIntegerToFieldUnit");

    /*
     * If the Field Buffer and Index have not been previously evaluated,
     * evaluate them and save the results.
     */
    if (!(TargetDesc->Common.Flags & AOPOBJ_DATA_VALID))
    {
        Status = AcpiDsGetFieldUnitArguments (TargetDesc);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    if ((!TargetDesc->FieldUnit.Container ||
        ACPI_TYPE_BUFFER != TargetDesc->FieldUnit.Container->Common.Type))
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("Null Container or wrong type: %p", TargetDesc->FieldUnit.Container));

        if (TargetDesc->FieldUnit.Container)
        {
            DEBUG_PRINT_RAW (ACPI_ERROR, (" Type %X",
                TargetDesc->FieldUnit.Container->Common.Type));
        }
        DEBUG_PRINT_RAW (ACPI_ERROR, ("\n"));

        return_ACPI_STATUS (AE_AML_INTERNAL);
    }

    /*
     * Get the global lock if needed
     */
    Locked = AcpiAmlAcquireGlobalLock (TargetDesc->FieldUnit.LockRule);

    /*
     * TBD: [Unhandled] REMOVE this limitation
     * Make sure the operation is within the limits of our implementation
     * this is not a Spec limitation!!
     */
    if (TargetDesc->FieldUnit.Length + TargetDesc->FieldUnit.BitOffset > 32)
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("AmlCopyIntegerToFieldUnit: FieldUnit: Implementation limitation - Field exceeds UINT32\n"));
        return_ACPI_STATUS (AE_NOT_IMPLEMENTED);
    }

    /* Field location is (base of buffer) + (byte offset) */

    Location = TargetDesc->FieldUnit.Container->Buffer.Pointer
                    + TargetDesc->FieldUnit.Offset;

    /*
     * Construct Mask with 1 bits where the field is,
     * 0 bits elsewhere
     */
    Mask = ((UINT32) 1 << TargetDesc->FieldUnit.Length) - ((UINT32)1
                        << TargetDesc->FieldUnit.BitOffset);

    DEBUG_PRINT (TRACE_EXEC,
        ("** Store %lx in buffer %p byte %ld bit %X width %d addr %p mask %08lx\n",
        SourceDesc->Integer.Value,
        TargetDesc->FieldUnit.Container->Buffer.Pointer,
        TargetDesc->FieldUnit.Offset, TargetDesc->FieldUnit.BitOffset,
        TargetDesc->FieldUnit.Length,Location, Mask));

    /* Zero out the field in the buffer */

    MOVE_UNALIGNED32_TO_32 (&NewValue, Location);
    NewValue &= ~Mask;

    /*
     * Shift and mask the new value into position,
     * and or it into the buffer.
     */
    NewValue |= (SourceDesc->Integer.Value << TargetDesc->FieldUnit.BitOffset) &
                Mask;

    /* Store back the value */

    MOVE_UNALIGNED32_TO_32 (Location, &NewValue);

    DEBUG_PRINT (TRACE_EXEC, ("New Field value %08lx\n", NewValue));
    return_ACPI_STATUS (AE_OK);
}





