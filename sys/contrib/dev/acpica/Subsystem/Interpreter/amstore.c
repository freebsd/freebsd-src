
/******************************************************************************
 *
 * Module Name: amstore - AML Interpreter object store support
 *              $Revision: 116 $
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

#define __AMSTORE_C__

#include "acpi.h"
#include "acparser.h"
#include "acdispat.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "actables.h"


#define _COMPONENT          INTERPRETER
        MODULE_NAME         ("amstore")


/*******************************************************************************
 *
 * FUNCTION:    AcpiAmlExecStore
 *
 * PARAMETERS:  *ValDesc            - Value to be stored
 *              *DestDesc           - Where to store it 0 Must be (ACPI_HANDLE)
 *                                    or an ACPI_OPERAND_OBJECT  of type
 *                                    Reference; if the latter the descriptor
 *                                    will be either reused or deleted.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Store the value described by ValDesc into the location
 *              described by DestDesc.  Called by various interpreter
 *              functions to store the result of an operation into
 *              the destination operand.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiAmlExecStore (
    ACPI_OPERAND_OBJECT     *ValDesc,
    ACPI_OPERAND_OBJECT     *DestDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     *DeleteDestDesc = NULL;
    ACPI_OPERAND_OBJECT     *TmpDesc;
    ACPI_NAMESPACE_NODE     *Node = NULL;
    UINT8                   Value = 0;
    UINT32                  Length;
    UINT32                  i;


    FUNCTION_TRACE ("AmlExecStore");

    DEBUG_PRINT (ACPI_INFO, ("entered AcpiAmlExecStore: Val=%p, Dest=%p\n",
                    ValDesc, DestDesc));


    /* Validate parameters */

    if (!ValDesc || !DestDesc)
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("AmlExecStore: Internal error - null pointer\n"));
        return_ACPI_STATUS (AE_AML_NO_OPERAND);
    }

    /* Examine the datatype of the DestDesc */

    if (VALID_DESCRIPTOR_TYPE (DestDesc, ACPI_DESC_TYPE_NAMED))
    {
        /* Dest is an ACPI_HANDLE, create a new object */

        Node = (ACPI_NAMESPACE_NODE *) DestDesc;
        DestDesc = AcpiCmCreateInternalObject (INTERNAL_TYPE_REFERENCE);
        if (!DestDesc)
        {
            /* Allocation failure  */

            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        /* Build a new Reference wrapper around the handle */

        DestDesc->Reference.OpCode = AML_NAME_OP;
        DestDesc->Reference.Object = Node;
    }

    else
    {
        DEBUG_PRINT (ACPI_INFO,
            ("AmlExecStore: Dest is object (not handle) - may be deleted!\n"));
    }

    /* Destination object must be of type Reference */

    if (DestDesc->Common.Type != INTERNAL_TYPE_REFERENCE)
    {
        /* Destination is not an Reference */

        DEBUG_PRINT (ACPI_ERROR,
            ("AmlExecStore: Destination is not an Reference [%p]\n", DestDesc));

        DUMP_STACK_ENTRY (ValDesc);
        DUMP_STACK_ENTRY (DestDesc);
        DUMP_OPERANDS (&DestDesc, IMODE_EXECUTE, "AmlExecStore",
                        2, "target not Reference");

        return_ACPI_STATUS (AE_AML_OPERAND_TYPE);
    }

    /* Examine the Reference opcode */

    switch (DestDesc->Reference.OpCode)
    {

    case AML_NAME_OP:

        /*
         *  Storing into a Name
         */
        DeleteDestDesc = DestDesc;
        Status = AcpiAmlStoreObjectToNode (ValDesc, DestDesc->Reference.Object,
                        WalkState);

        break;  /* Case NameOp */


    case AML_INDEX_OP:

        DeleteDestDesc = DestDesc;

        /*
         * Valid source value and destination reference pointer.
         *
         * ACPI Specification 1.0B section 15.2.3.4.2.13:
         * Destination should point to either a buffer or a package
         */

        /*
         * Actually, storing to a package is not so simple.  The source must be
         * evaluated and converted to the type of the destination and then the
         * source is copied into the destination - we can't just point to the
         * source object.
         */
        if (DestDesc->Reference.TargetType == ACPI_TYPE_PACKAGE)
        {
            /*
             * The object at *(DestDesc->Reference.Where) is the
             *  element within the package that is to be modified.
             */
            TmpDesc = *(DestDesc->Reference.Where);
            if (TmpDesc)
            {
                /*
                 * If the Destination element is a package, we will delete
                 *  that object and construct a new one.
                 *
                 * TBD: [Investigate] Should both the src and dest be required
                 *      to be packages?
                 *       && (ValDesc->Common.Type == ACPI_TYPE_PACKAGE)
                 */
                if (TmpDesc->Common.Type == ACPI_TYPE_PACKAGE)
                {
                    /*
                     * Take away the reference for being part of a package and
                     * delete
                     */
                    AcpiCmRemoveReference (TmpDesc);
                    AcpiCmRemoveReference (TmpDesc);

                    TmpDesc = NULL;
                }
            }

            if (!TmpDesc)
            {
                /*
                 * If the TmpDesc is NULL, that means an uninitialized package
                 * has been used as a destination, therefore, we must create
                 * the destination element to match the type of the source
                 * element NOTE: ValDesc can be of any type.
                 */
                TmpDesc = AcpiCmCreateInternalObject (ValDesc->Common.Type);
                if (!TmpDesc)
                {
                    Status = AE_NO_MEMORY;
                    goto Cleanup;
                }

                /*
                 * If the source is a package, copy the source to the new dest
                 */
                if (ACPI_TYPE_PACKAGE == TmpDesc->Common.Type)
                {
                    Status = AcpiAmlBuildCopyInternalPackageObject (
                                ValDesc, TmpDesc, WalkState);
                    if (ACPI_FAILURE (Status))
                    {
                        AcpiCmRemoveReference (TmpDesc);
                        TmpDesc = NULL;
                        goto Cleanup;
                    }
                }

                /*
                 * Install the new descriptor into the package and add a
                 * reference to the newly created descriptor for now being
                 * part of the parent package
                 */

                *(DestDesc->Reference.Where) = TmpDesc;
                AcpiCmAddReference (TmpDesc);
            }

            if (ACPI_TYPE_PACKAGE != TmpDesc->Common.Type)
            {
                /*
                 * The destination element is not a package, so we need to
                 * convert the contents of the source (ValDesc) and copy into
                 * the destination (TmpDesc)
                 */
                Status = AcpiAmlStoreObjectToObject (ValDesc, TmpDesc,
                                                        WalkState);
                if (ACPI_FAILURE (Status))
                {
                    /*
                     * An error occurrered when copying the internal object
                     * so delete the reference.
                     */
                    DEBUG_PRINT (ACPI_ERROR,
                        ("AmlExecStore/Index: Unable to copy the internal object\n"));
                    Status = AE_AML_OPERAND_TYPE;
                }
            }

            break;
        }

        /*
         * Check that the destination is a Buffer Field type
         */
        if (DestDesc->Reference.TargetType != ACPI_TYPE_BUFFER_FIELD)
        {
            Status = AE_AML_OPERAND_TYPE;
            break;
        }

        /*
         * Storing into a buffer at a location defined by an Index.
         *
         * Each 8-bit element of the source object is written to the
         * 8-bit Buffer Field of the Index destination object.
         */

        /*
         * Set the TmpDesc to the destination object and type check.
         */
        TmpDesc = DestDesc->Reference.Object;

        if (TmpDesc->Common.Type != ACPI_TYPE_BUFFER)
        {
            Status = AE_AML_OPERAND_TYPE;
            break;
        }

        /*
         * The assignment of the individual elements will be slightly
         * different for each source type.
         */

        switch (ValDesc->Common.Type)
        {
        /*
         * If the type is Integer, the Length is 4.
         * This loop to assign each of the elements is somewhat
         *  backward because of the Big Endian-ness of IA-64
         */
        case ACPI_TYPE_NUMBER:
            Length = 4;
            for (i = Length; i != 0; i--)
            {
                Value = (UINT8)(ValDesc->Number.Value >> (MUL_8 (i - 1)));
                TmpDesc->Buffer.Pointer[DestDesc->Reference.Offset] = Value;
            }
            break;

        /*
         * If the type is Buffer, the Length is in the structure.
         * Just loop through the elements and assign each one in turn.
         */
        case ACPI_TYPE_BUFFER:
            Length = ValDesc->Buffer.Length;
            for (i = 0; i < Length; i++)
            {
                Value = *(ValDesc->Buffer.Pointer + i);
                TmpDesc->Buffer.Pointer[DestDesc->Reference.Offset] = Value;
            }
            break;

        /*
         * If the type is String, the Length is in the structure.
         * Just loop through the elements and assign each one in turn.
         */
        case ACPI_TYPE_STRING:
            Length = ValDesc->String.Length;
            for (i = 0; i < Length; i++)
            {
                Value = *(ValDesc->String.Pointer + i);
                TmpDesc->Buffer.Pointer[DestDesc->Reference.Offset] = Value;
            }
            break;

        /*
         * If source is not a valid type so return an error.
         */
        default:
            DEBUG_PRINT (ACPI_ERROR,
                ("AmlExecStore/Index: Source must be Number/Buffer/String type, not 0x%x\n",
                ValDesc->Common.Type));
            Status = AE_AML_OPERAND_TYPE;
            break;
        }

        /*
         * If we had an error, break out of this case statement.
         */
        if (ACPI_FAILURE (Status))
        {
            break;
        }

        /*
         * Set the return pointer
         */
        DestDesc = TmpDesc;

        break;

    case AML_ZERO_OP:
    case AML_ONE_OP:
    case AML_ONES_OP:

        /*
         * Storing to a constant is a no-op -- see ACPI Specification
         * Delete the result descriptor.
         */

        DeleteDestDesc = DestDesc;
        break;


    case AML_LOCAL_OP:

        Status = AcpiDsMethodDataSetValue (MTH_TYPE_LOCAL,
                        (DestDesc->Reference.Offset), ValDesc, WalkState);
        DeleteDestDesc = DestDesc;
        break;


    case AML_ARG_OP:

        Status = AcpiDsMethodDataSetValue (MTH_TYPE_ARG,
                        (DestDesc->Reference.Offset), ValDesc, WalkState);
        DeleteDestDesc = DestDesc;
        break;


    case AML_DEBUG_OP:

        /*
         * Storing to the Debug object causes the value stored to be
         * displayed and otherwise has no effect -- see ACPI Specification
         */
        DEBUG_PRINT (ACPI_INFO, ("**** Write to Debug Object: ****: \n"));
        if (ValDesc->Common.Type == ACPI_TYPE_STRING)
        {
            DEBUG_PRINT (ACPI_INFO, ("%s\n", ValDesc->String.Pointer));
        }
        else
        {
            DUMP_STACK_ENTRY (ValDesc);
        }

        DeleteDestDesc = DestDesc;
        break;


    default:

        DEBUG_PRINT (ACPI_ERROR,
            ("AmlExecStore: Internal error - Unknown Reference subtype %02x\n",
            DestDesc->Reference.OpCode));

        /* TBD: [Restructure] use object dump routine !! */

        DUMP_BUFFER (DestDesc, sizeof (ACPI_OPERAND_OBJECT));

        DeleteDestDesc = DestDesc;
        Status = AE_AML_INTERNAL;

    }   /* switch(DestDesc->Reference.OpCode) */


Cleanup:

    /* Cleanup and exit*/

    if (DeleteDestDesc)
    {
        AcpiCmRemoveReference (DeleteDestDesc);
    }

    return_ACPI_STATUS (Status);
}


