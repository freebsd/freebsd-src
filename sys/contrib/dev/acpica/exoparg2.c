/******************************************************************************
 *
 * Module Name: exoparg2 - AML execution - opcodes with 2 arguments
 *              $Revision: 97 $
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


#define __EXOPARG2_C__

#include "acpi.h"
#include "acparser.h"
#include "acnamesp.h"
#include "acinterp.h"
#include "acevents.h"
#include "amlcode.h"
#include "acdispat.h"


#define _COMPONENT          ACPI_EXECUTER
        MODULE_NAME         ("exoparg2")


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
 * FUNCTION:    AcpiExOpcode_2A_0T_0R
 *
 * PARAMETERS:  WalkState           - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute opcode with two arguments, no target, and no return
 *              value.
 *
 * ALLOCATION:  Deletes both operands
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExOpcode_2A_0T_0R (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     **Operand = &WalkState->Operands[0];
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE_STR ("ExOpcode_2A_0T_0R", AcpiPsGetOpcodeName (WalkState->Opcode));


    /* Examine the opcode */

    switch (WalkState->Opcode)
    {

    case AML_NOTIFY_OP:         /* Notify (NotifyObject, NotifyValue) */

        /* The first operand is a namespace node */

        Node = (ACPI_NAMESPACE_NODE *) Operand[0];

        /* The node must refer to a device or thermal zone */

        if (Node && Operand[1])     /* TBD: is this check necessary? */
        {
            switch (Node->Type)
            {
            case ACPI_TYPE_DEVICE:
            case ACPI_TYPE_THERMAL:

                /*
                 * Dispatch the notify to the appropriate handler
                 * NOTE: the request is queued for execution after this method
                 * completes.  The notify handlers are NOT invoked synchronously
                 * from this thread -- because handlers may in turn run other
                 * control methods.
                 */
                Status = AcpiEvQueueNotifyRequest (Node,
                                        (UINT32) Operand[1]->Integer.Value);
                break;

            default:
                ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Unexpected notify object type %X\n",
                    Node->Type));

                Status = AE_AML_OPERAND_TYPE;
                break;
            }
        }
        break;

    default:

        REPORT_ERROR (("AcpiExOpcode_2A_0T_0R: Unknown opcode %X\n", WalkState->Opcode));
        Status = AE_AML_BAD_OPCODE;
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExOpcode_2A_2T_1R
 *
 * PARAMETERS:  WalkState           - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute a dyadic operator (2 operands) with 2 output targets
 *              and one implicit return value.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExOpcode_2A_2T_1R (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     **Operand = &WalkState->Operands[0];
    ACPI_OPERAND_OBJECT     *ReturnDesc1 = NULL;
    ACPI_OPERAND_OBJECT     *ReturnDesc2 = NULL;
    ACPI_STATUS             Status;


    FUNCTION_TRACE_STR ("ExOpcode_2A_2T_1R", AcpiPsGetOpcodeName (WalkState->Opcode));


    /*
     * Execute the opcode
     */
    switch (WalkState->Opcode)
    {
    case AML_DIVIDE_OP:             /* Divide (Dividend, Divisor, RemainderResult QuotientResult) */

        ReturnDesc1 = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
        if (!ReturnDesc1)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        ReturnDesc2 = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
        if (!ReturnDesc2)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        /* Quotient to ReturnDesc1, remainder to ReturnDesc2 */

        Status = AcpiUtDivide (&Operand[0]->Integer.Value, &Operand[1]->Integer.Value,
                               &ReturnDesc1->Integer.Value, &ReturnDesc2->Integer.Value);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }
        break;


    default:

        REPORT_ERROR (("AcpiExOpcode_2A_2T_1R: Unknown opcode %X\n",
                WalkState->Opcode));
        Status = AE_AML_BAD_OPCODE;
        goto Cleanup;
        break;
    }


    /* Store the results to the target reference operands */

    Status = AcpiExStore (ReturnDesc2, Operand[2], WalkState);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    Status = AcpiExStore (ReturnDesc1, Operand[3], WalkState);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    /* Return the remainder */

    WalkState->ResultObj = ReturnDesc1;


Cleanup:
    /*
     * Since the remainder is not returned indirectly, remove a reference to
     * it. Only the quotient is returned indirectly.
     */
    AcpiUtRemoveReference (ReturnDesc2);

    if (ACPI_FAILURE (Status))
    {
        /* Delete the return object */

        AcpiUtRemoveReference (ReturnDesc1);
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExOpcode_2A_1T_1R
 *
 * PARAMETERS:  WalkState           - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute opcode with two arguments, one target, and a return
 *              value.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExOpcode_2A_1T_1R (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     **Operand   = &WalkState->Operands[0];
    ACPI_OPERAND_OBJECT     *ReturnDesc = NULL;
    ACPI_OPERAND_OBJECT     *TempDesc;
    UINT32                  Index;
    ACPI_STATUS             Status      = AE_OK;


    FUNCTION_TRACE_STR ("ExOpcode_2A_1T_1R", AcpiPsGetOpcodeName (WalkState->Opcode));


    /*
     * Execute the opcode
     */
    if (WalkState->OpInfo->Flags & AML_MATH)
    {
        /* All simple math opcodes (add, etc.) */

        ReturnDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
        if (!ReturnDesc)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        ReturnDesc->Integer.Value = AcpiExDoMathOp (WalkState->Opcode, 
                                                Operand[0]->Integer.Value, 
                                                Operand[1]->Integer.Value);
        goto StoreResultToTarget;
    }


    switch (WalkState->Opcode)
    {
    case AML_MOD_OP:                /* Mod (Dividend, Divisor, RemainderResult (ACPI 2.0) */

        ReturnDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
        if (!ReturnDesc)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        /* ReturnDesc will contain the remainder */

        Status = AcpiUtDivide (&Operand[0]->Integer.Value, &Operand[1]->Integer.Value,
                        NULL, &ReturnDesc->Integer.Value);

        break;


    case AML_CONCAT_OP:             /* Concatenate (Data1, Data2, Result) */

        /*
         * Convert the second operand if necessary.  The first operand
         * determines the type of the second operand, (See the Data Types
         * section of the ACPI specification.)  Both object types are
         * guaranteed to be either Integer/String/Buffer by the operand
         * resolution mechanism above.
         */
        switch (Operand[0]->Common.Type)
        {
        case ACPI_TYPE_INTEGER:
            Status = AcpiExConvertToInteger (Operand[1], &Operand[1], WalkState);
            break;

        case ACPI_TYPE_STRING:
            Status = AcpiExConvertToString (Operand[1], &Operand[1], 16, ACPI_UINT32_MAX, WalkState);
            break;

        case ACPI_TYPE_BUFFER:
            Status = AcpiExConvertToBuffer (Operand[1], &Operand[1], WalkState);
            break;

        default:
            Status = AE_AML_INTERNAL;
        }

        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        /*
         * Both operands are now known to be the same object type
         * (Both are Integer, String, or Buffer), and we can now perform the
         * concatenation.
         */
        Status = AcpiExDoConcatenate (Operand[0], Operand[1], &ReturnDesc, WalkState);
        break;


    case AML_TO_STRING_OP:          /* ToString (Buffer, Length, Result) (ACPI 2.0) */

        Status = AcpiExConvertToString (Operand[0], &ReturnDesc, 16,
                        (UINT32) Operand[1]->Integer.Value, WalkState);
        break;


    case AML_CONCAT_RES_OP:         /* ConcatenateResTemplate (Buffer, Buffer, Result) (ACPI 2.0) */

        Status = AE_NOT_IMPLEMENTED;
        break;


    case AML_INDEX_OP:              /* Index (Source Index Result) */

        /* Create the internal return object */

        ReturnDesc = AcpiUtCreateInternalObject (INTERNAL_TYPE_REFERENCE);
        if (!ReturnDesc)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        Index = (UINT32) Operand[1]->Integer.Value;

        /*
         * At this point, the Source operand is either a Package or a Buffer
         */
        if (Operand[0]->Common.Type == ACPI_TYPE_PACKAGE)
        {
            /* Object to be indexed is a Package */

            if (Index >= Operand[0]->Package.Count)
            {
                ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Index value beyond package end\n"));
                Status = AE_AML_PACKAGE_LIMIT;
                goto Cleanup;
            }

            if ((Operand[2]->Common.Type == INTERNAL_TYPE_REFERENCE) &&
                (Operand[2]->Reference.Opcode == AML_ZERO_OP))
            {
                /*
                 * There is no actual result descriptor (the ZeroOp Result
                 * descriptor is a placeholder), so just delete the placeholder and
                 * return a reference to the package element
                 */
                AcpiUtRemoveReference (Operand[2]);
            }

            else
            {
                /*
                 * Each element of the package is an internal object.  Get the one
                 * we are after.
                 */
                TempDesc                         = Operand[0]->Package.Elements [Index];
                ReturnDesc->Reference.Opcode     = AML_INDEX_OP;
                ReturnDesc->Reference.TargetType = TempDesc->Common.Type;
                ReturnDesc->Reference.Object     = TempDesc;

                Status = AcpiExStore (ReturnDesc, Operand[2], WalkState);
                ReturnDesc->Reference.Object     = NULL;
            }

            /*
             * The local return object must always be a reference to the package element,
             * not the element itself.
             */
            ReturnDesc->Reference.Opcode     = AML_INDEX_OP;
            ReturnDesc->Reference.TargetType = ACPI_TYPE_PACKAGE;
            ReturnDesc->Reference.Where      = &Operand[0]->Package.Elements [Index];
        }

        else
        {
            /* Object to be indexed is a Buffer */

            if (Index >= Operand[0]->Buffer.Length)
            {
                ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Index value beyond end of buffer\n"));
                Status = AE_AML_BUFFER_LIMIT;
                goto Cleanup;
            }

            ReturnDesc->Reference.Opcode       = AML_INDEX_OP;
            ReturnDesc->Reference.TargetType   = ACPI_TYPE_BUFFER_FIELD;
            ReturnDesc->Reference.Object       = Operand[0];
            ReturnDesc->Reference.Offset       = Index;

            Status = AcpiExStore (ReturnDesc, Operand[2], WalkState);
        }

        WalkState->ResultObj = ReturnDesc;
        goto Cleanup;
        break;


    default:

        REPORT_ERROR (("AcpiExOpcode_2A_1T_1R: Unknown opcode %X\n",
                WalkState->Opcode));
        Status = AE_AML_BAD_OPCODE;
        break;
    }


StoreResultToTarget:

    if (ACPI_SUCCESS (Status))
    {
        /*
         * Store the result of the operation (which is now in ReturnDesc) into
         * the Target descriptor.
         */
        Status = AcpiExStore (ReturnDesc, Operand[2], WalkState);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        WalkState->ResultObj = ReturnDesc;
    }


Cleanup:

    /* Delete return object on error */

    if (ACPI_FAILURE (Status))
    {
        AcpiUtRemoveReference (ReturnDesc);
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExOpcode_2A_0T_1R
 *
 * PARAMETERS:  WalkState           - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute opcode with 2 arguments, no target, and a return value
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExOpcode_2A_0T_1R (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     **Operand = &WalkState->Operands[0];
    ACPI_OPERAND_OBJECT     *ReturnDesc = NULL;
    ACPI_STATUS             Status = AE_OK;
    BOOLEAN                 LogicalResult = FALSE;


    FUNCTION_TRACE_STR ("ExOpcode_2A_0T_1R", AcpiPsGetOpcodeName (WalkState->Opcode));


    /* Create the internal return object */

    ReturnDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
    if (!ReturnDesc)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }

    /*
     * Execute the Opcode
     */
    if (WalkState->OpInfo->Flags & AML_LOGICAL) /* LogicalOp  (Operand0, Operand1) */
    {
        LogicalResult = AcpiExDoLogicalOp (WalkState->Opcode, 
                                            Operand[0]->Integer.Value,
                                            Operand[1]->Integer.Value);
        goto StoreLogicalResult;
    }


    switch (WalkState->Opcode)
    {
    case AML_ACQUIRE_OP:            /* Acquire (MutexObject, Timeout) */

        Status = AcpiExAcquireMutex (Operand[1], Operand[0], WalkState);
        if (Status == AE_TIME)
        {
            LogicalResult = TRUE;       /* TRUE = Acquire timed out */
            Status = AE_OK;
        }
        break;


    case AML_WAIT_OP:               /* Wait (EventObject, Timeout) */

        Status = AcpiExSystemWaitEvent (Operand[1], Operand[0]);
        if (Status == AE_TIME)
        {
            LogicalResult = TRUE;       /* TRUE, Wait timed out */
            Status = AE_OK;
        }
        break;


    default:

        REPORT_ERROR (("AcpiExOpcode_2A_0T_1R: Unknown opcode %X\n", WalkState->Opcode));
        Status = AE_AML_BAD_OPCODE;
        goto Cleanup;
        break;
    }


StoreLogicalResult:
    /* 
     * Set return value to according to LogicalResult. logical TRUE (all ones)
     * Default is FALSE (zero) 
     */
    if (LogicalResult)
    {
        ReturnDesc->Integer.Value = ACPI_INTEGER_MAX;
    }

    WalkState->ResultObj = ReturnDesc;


Cleanup:

    /* Delete return object on error */

    if (ACPI_FAILURE (Status))
    {
        AcpiUtRemoveReference (ReturnDesc);
    }

    return_ACPI_STATUS (Status);
}


