/******************************************************************************
 *
 * Module Name: exdyadic - ACPI AML (p-code) execution for dyadic operators
 *              $Revision: 77 $
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


#define __EXDYADIC_C__

#include "acpi.h"
#include "acparser.h"
#include "acnamesp.h"
#include "acinterp.h"
#include "acevents.h"
#include "amlcode.h"
#include "acdispat.h"


#define _COMPONENT          ACPI_EXECUTER
        MODULE_NAME         ("exdyadic")


/*******************************************************************************
 *
 * FUNCTION:    AcpiExDoConcatenate
 *
 * PARAMETERS:  *ObjDesc        - Object to be converted.  Must be an
 *                                Integer, Buffer, or String
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Concatenate two objects OF THE SAME TYPE.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExDoConcatenate (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     *ObjDesc2,
    ACPI_OPERAND_OBJECT     **ActualRetDesc,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status;
    UINT32                  i;
    ACPI_INTEGER            ThisInteger;
    ACPI_OPERAND_OBJECT     *RetDesc;
    NATIVE_CHAR             *NewBuf;
    UINT32                  IntegerSize = sizeof (ACPI_INTEGER);


    /*
     * There are three cases to handle:
     * 1) Two Integers concatenated to produce a buffer
     * 2) Two Strings concatenated to produce a string
     * 3) Two Buffers concatenated to produce a buffer
     */
    switch (ObjDesc->Common.Type)
    {
    case ACPI_TYPE_INTEGER:

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

        /* Result of two integers is a buffer */

        RetDesc = AcpiUtCreateInternalObject (ACPI_TYPE_BUFFER);
        if (!RetDesc)
        {
            return (AE_NO_MEMORY);
        }

        /* Need enough space for two integers */

        RetDesc->Buffer.Length = IntegerSize * 2;
        NewBuf = AcpiUtCallocate (RetDesc->Buffer.Length);
        if (!NewBuf)
        {
            REPORT_ERROR
                (("ExDoConcatenate: Buffer allocation failure\n"));
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        RetDesc->Buffer.Pointer = (UINT8 *) NewBuf;

        /* Convert the first integer */

        ThisInteger = ObjDesc->Integer.Value;
        for (i = 0; i < IntegerSize; i++)
        {
            NewBuf[i] = (UINT8) ThisInteger;
            ThisInteger >>= 8;
        }

        /* Convert the second integer */

        ThisInteger = ObjDesc2->Integer.Value;
        for (; i < (IntegerSize * 2); i++)
        {
            NewBuf[i] = (UINT8) ThisInteger;
            ThisInteger >>= 8;
        }

        break;


    case ACPI_TYPE_STRING:

        RetDesc = AcpiUtCreateInternalObject (ACPI_TYPE_STRING);
        if (!RetDesc)
        {
            return (AE_NO_MEMORY);
        }

        /* Operand1 is string  */

        NewBuf = AcpiUtAllocate (ObjDesc->String.Length +
                                 ObjDesc2->String.Length + 1);
        if (!NewBuf)
        {
            REPORT_ERROR
                (("ExDoConcatenate: String allocation failure\n"));
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        STRCPY (NewBuf, ObjDesc->String.Pointer);
        STRCPY (NewBuf + ObjDesc->String.Length,
                        ObjDesc2->String.Pointer);

        /* Point the return object to the new string */

        RetDesc->String.Pointer = NewBuf;
        RetDesc->String.Length = ObjDesc->String.Length +=
                                 ObjDesc2->String.Length;
        break;


    case ACPI_TYPE_BUFFER:

        /* Operand1 is a buffer */

        RetDesc = AcpiUtCreateInternalObject (ACPI_TYPE_BUFFER);
        if (!RetDesc)
        {
            return (AE_NO_MEMORY);
        }

        NewBuf = AcpiUtAllocate (ObjDesc->Buffer.Length +
                                 ObjDesc2->Buffer.Length);
        if (!NewBuf)
        {
            REPORT_ERROR
                (("ExDoConcatenate: Buffer allocation failure\n"));
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        MEMCPY (NewBuf, ObjDesc->Buffer.Pointer,
                        ObjDesc->Buffer.Length);
        MEMCPY (NewBuf + ObjDesc->Buffer.Length, ObjDesc2->Buffer.Pointer,
                        ObjDesc2->Buffer.Length);

        /*
         * Point the return object to the new buffer
         */

        RetDesc->Buffer.Pointer     = (UINT8 *) NewBuf;
        RetDesc->Buffer.Length      = ObjDesc->Buffer.Length +
                                      ObjDesc2->Buffer.Length;
        break;

    default:
        Status = AE_AML_INTERNAL;
        RetDesc = NULL;
    }


    *ActualRetDesc = RetDesc;
    return (AE_OK);


Cleanup:

    AcpiUtRemoveReference (RetDesc);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExDyadic1
 *
 * PARAMETERS:  Opcode              - The opcode to be executed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute Type 1 dyadic operator with numeric operands:
 *              NotifyOp
 *
 * ALLOCATION:  Deletes both operands
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExDyadic1 (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     *ObjDesc = NULL;
    ACPI_OPERAND_OBJECT     *ValDesc = NULL;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE_PTR ("ExDyadic1", WALK_OPERANDS);


    /* Resolve all operands */

    Status = AcpiExResolveOperands (Opcode, WALK_OPERANDS, WalkState);
    DUMP_OPERANDS (WALK_OPERANDS, IMODE_EXECUTE, AcpiPsGetOpcodeName (Opcode),
                    2, "after AcpiExResolveOperands");

    /* Get the operands */

    Status |= AcpiDsObjStackPopObject (&ValDesc, WalkState);
    Status |= AcpiDsObjStackPopObject (&ObjDesc, WalkState);
    if (ACPI_FAILURE (Status))
    {
        /* Invalid parameters on object stack  */

        DEBUG_PRINTP (ACPI_ERROR, ("(%s) bad operand(s) %s\n",
            AcpiPsGetOpcodeName (Opcode), AcpiUtFormatException (Status)));

        goto Cleanup;
    }


    /* Examine the opcode */

    switch (Opcode)
    {

    /* DefNotify   :=  NotifyOp    NotifyObject    NotifyValue */

    case AML_NOTIFY_OP:

        /* The ObjDesc is actually an Node */

        Node = (ACPI_NAMESPACE_NODE *) ObjDesc;
        ObjDesc = NULL;

        /* Object must be a device or thermal zone */

        if (Node && ValDesc)
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
                                        (UINT32) ValDesc->Integer.Value);
                break;

            default:
                DEBUG_PRINTP (ACPI_ERROR, ("Unexpected notify object type %X\n",
                    ObjDesc->Common.Type));

                Status = AE_AML_OPERAND_TYPE;
                break;
            }
        }
        break;

    default:

        REPORT_ERROR (("AcpiExDyadic1: Unknown dyadic opcode %X\n", Opcode));
        Status = AE_AML_BAD_OPCODE;
    }


Cleanup:

    /* Always delete both operands */

    AcpiUtRemoveReference (ValDesc);
    AcpiUtRemoveReference (ObjDesc);


    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExDyadic2R
 *
 * PARAMETERS:  Opcode              - The opcode to be executed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute Type 2 dyadic operator with numeric operands and
 *              one or two result operands.
 *
 * ALLOCATION:  Deletes one operand descriptor -- other remains on stack
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExDyadic2R (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **ReturnDesc)
{
    ACPI_OPERAND_OBJECT     *ObjDesc    = NULL;
    ACPI_OPERAND_OBJECT     *ObjDesc2   = NULL;
    ACPI_OPERAND_OBJECT     *ResDesc    = NULL;
    ACPI_OPERAND_OBJECT     *ResDesc2   = NULL;
    ACPI_OPERAND_OBJECT     *RetDesc    = NULL;
    ACPI_OPERAND_OBJECT     *RetDesc2   = NULL;
    ACPI_STATUS             Status      = AE_OK;
    UINT32                  NumOperands = 3;


    FUNCTION_TRACE_U32 ("ExDyadic2R", Opcode);


    /* Resolve all operands */

    Status = AcpiExResolveOperands (Opcode, WALK_OPERANDS, WalkState);
    DUMP_OPERANDS (WALK_OPERANDS, IMODE_EXECUTE, AcpiPsGetOpcodeName (Opcode),
                    NumOperands, "after AcpiExResolveOperands");

    /* Get all operands */

    if (AML_DIVIDE_OP == Opcode)
    {
        NumOperands = 4;
        Status |= AcpiDsObjStackPopObject (&ResDesc2, WalkState);
    }

    Status |= AcpiDsObjStackPopObject (&ResDesc, WalkState);
    Status |= AcpiDsObjStackPopObject (&ObjDesc2, WalkState);
    Status |= AcpiDsObjStackPopObject (&ObjDesc, WalkState);
    if (ACPI_FAILURE (Status))
    {
        DEBUG_PRINTP (ACPI_ERROR, ("(%s) bad operand(s) (%s)\n",
            AcpiPsGetOpcodeName (Opcode), AcpiUtFormatException (Status)));

        goto Cleanup;
    }


    /* Create an internal return object if necessary */

    switch (Opcode)
    {
    case AML_ADD_OP:
    case AML_BIT_AND_OP:
    case AML_BIT_NAND_OP:
    case AML_BIT_OR_OP:
    case AML_BIT_NOR_OP:
    case AML_BIT_XOR_OP:
    case AML_DIVIDE_OP:
    case AML_MULTIPLY_OP:
    case AML_SHIFT_LEFT_OP:
    case AML_SHIFT_RIGHT_OP:
    case AML_SUBTRACT_OP:

        RetDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
        if (!RetDesc)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        break;
    }


    /*
     * Execute the opcode
     */

    switch (Opcode)
    {

    /* DefAdd  :=  AddOp   Operand1    Operand2    Result  */

    case AML_ADD_OP:

        RetDesc->Integer.Value = ObjDesc->Integer.Value +
                                ObjDesc2->Integer.Value;
        break;


    /* DefAnd  :=  AndOp   Operand1    Operand2    Result  */

    case AML_BIT_AND_OP:

        RetDesc->Integer.Value = ObjDesc->Integer.Value &
                                ObjDesc2->Integer.Value;
        break;


    /* DefNAnd :=  NAndOp  Operand1    Operand2    Result  */

    case AML_BIT_NAND_OP:

        RetDesc->Integer.Value = ~(ObjDesc->Integer.Value &
                                  ObjDesc2->Integer.Value);
        break;


    /* DefOr   :=  OrOp    Operand1    Operand2    Result  */

    case AML_BIT_OR_OP:

        RetDesc->Integer.Value = ObjDesc->Integer.Value |
                                ObjDesc2->Integer.Value;
        break;


    /* DefNOr  :=  NOrOp   Operand1    Operand2    Result  */

    case AML_BIT_NOR_OP:

        RetDesc->Integer.Value = ~(ObjDesc->Integer.Value |
                                  ObjDesc2->Integer.Value);
        break;


    /* DefXOr  :=  XOrOp   Operand1    Operand2    Result  */

    case AML_BIT_XOR_OP:

        RetDesc->Integer.Value = ObjDesc->Integer.Value ^
                                ObjDesc2->Integer.Value;
        break;


    /* DefDivide   :=  DivideOp Dividend Divisor Remainder Quotient  */

    case AML_DIVIDE_OP:

        if (!ObjDesc2->Integer.Value)
        {
            REPORT_ERROR
                (("ExDyadic2R/DivideOp: Divide by zero\n"));

            Status = AE_AML_DIVIDE_BY_ZERO;
            goto Cleanup;
        }

        RetDesc2 = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
        if (!RetDesc2)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        /* Remainder (modulo) */

        RetDesc->Integer.Value   = ACPI_MODULO (ObjDesc->Integer.Value,
                                                ObjDesc2->Integer.Value);

        /* Result (what we used to call the quotient) */

        RetDesc2->Integer.Value  = ACPI_DIVIDE (ObjDesc->Integer.Value,
                                                ObjDesc2->Integer.Value);
        break;


    /* DefMultiply :=  MultiplyOp  Operand1    Operand2    Result  */

    case AML_MULTIPLY_OP:

        RetDesc->Integer.Value = ObjDesc->Integer.Value *
                                ObjDesc2->Integer.Value;
        break;


    /* DefShiftLeft    :=  ShiftLeftOp Operand ShiftCount  Result  */

    case AML_SHIFT_LEFT_OP:

        RetDesc->Integer.Value = ObjDesc->Integer.Value <<
                                ObjDesc2->Integer.Value;
        break;


    /* DefShiftRight   :=  ShiftRightOp    Operand ShiftCount  Result  */

    case AML_SHIFT_RIGHT_OP:

        RetDesc->Integer.Value = ObjDesc->Integer.Value >>
                                ObjDesc2->Integer.Value;
        break;


    /* DefSubtract :=  SubtractOp  Operand1    Operand2    Result  */

    case AML_SUBTRACT_OP:

        RetDesc->Integer.Value = ObjDesc->Integer.Value -
                                ObjDesc2->Integer.Value;
        break;


    /* DefConcat   :=  ConcatOp    Data1   Data2   Result  */

    case AML_CONCAT_OP:


        /*
         * Convert the second operand if necessary.  The first operand
         * determines the type of the second operand, (See the Data Types
         * section of the ACPI specification.)  Both object types are
         * guaranteed to be either Integer/String/Buffer by the operand
         * resolution mechanism above.
         */

        switch (ObjDesc->Common.Type)
        {
        case ACPI_TYPE_INTEGER:
            Status = AcpiExConvertToInteger (&ObjDesc2, WalkState);
            break;

        case ACPI_TYPE_STRING:
            Status = AcpiExConvertToString (&ObjDesc2, WalkState);
            break;

        case ACPI_TYPE_BUFFER:
            Status = AcpiExConvertToBuffer (&ObjDesc2, WalkState);
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
        Status = AcpiExDoConcatenate (ObjDesc, ObjDesc2, &RetDesc, WalkState);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }
        break;


    default:

        REPORT_ERROR (("AcpiExDyadic2R: Unknown dyadic opcode %X\n",
                Opcode));
        Status = AE_AML_BAD_OPCODE;
        goto Cleanup;
    }


    /*
     * Store the result of the operation (which is now in ObjDesc) into
     * the result descriptor, or the location pointed to by the result
     * descriptor (ResDesc).
     */

    Status = AcpiExStore (RetDesc, ResDesc, WalkState);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    if (AML_DIVIDE_OP == Opcode)
    {
        Status = AcpiExStore (RetDesc2, ResDesc2, WalkState);

        /*
         * Since the remainder is not returned, remove a reference to
         * the object we created earlier
         */

        AcpiUtRemoveReference (RetDesc2);
    }


Cleanup:

    /* Always delete the operands */

    AcpiUtRemoveReference (ObjDesc);
    AcpiUtRemoveReference (ObjDesc2);


    /* Delete return object on error */

    if (ACPI_FAILURE (Status))
    {
        /* On failure, delete the result ops */

        AcpiUtRemoveReference (ResDesc);
        AcpiUtRemoveReference (ResDesc2);

        if (RetDesc)
        {
            /* And delete the internal return object */

            AcpiUtRemoveReference (RetDesc);
            RetDesc = NULL;
        }
    }

    /* Set the return object and exit */

    *ReturnDesc = RetDesc;
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExDyadic2S
 *
 * PARAMETERS:  Opcode              - The opcode to be executed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute Type 2 dyadic synchronization operator
 *
 * ALLOCATION:  Deletes one operand descriptor -- other remains on stack
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExDyadic2S (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **ReturnDesc)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_OPERAND_OBJECT     *TimeDesc;
    ACPI_OPERAND_OBJECT     *RetDesc = NULL;
    ACPI_STATUS             Status;


    FUNCTION_TRACE_PTR ("ExDyadic2S", WALK_OPERANDS);


    /* Resolve all operands */

    Status = AcpiExResolveOperands (Opcode, WALK_OPERANDS, WalkState);
    DUMP_OPERANDS (WALK_OPERANDS, IMODE_EXECUTE, AcpiPsGetOpcodeName (Opcode),
                    2, "after AcpiExResolveOperands");

    /* Get all operands */

    Status |= AcpiDsObjStackPopObject (&TimeDesc, WalkState);
    Status |= AcpiDsObjStackPopObject (&ObjDesc, WalkState);
    if (ACPI_FAILURE (Status))
    {
        /* Invalid parameters on object stack  */

        DEBUG_PRINTP (ACPI_ERROR, ("(%s) bad operand(s) %s\n",
            AcpiPsGetOpcodeName (Opcode), AcpiUtFormatException (Status)));

        goto Cleanup;
    }


    /* Create the internal return object */

    RetDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
    if (!RetDesc)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }

    /* Default return value is FALSE, operation did not time out */

    RetDesc->Integer.Value = 0;


    /* Examine the opcode */

    switch (Opcode)
    {

    /* DefAcquire  :=  AcquireOp   MutexObject Timeout */

    case AML_ACQUIRE_OP:

        Status = AcpiExAcquireMutex (TimeDesc, ObjDesc, WalkState);
        break;


    /* DefWait :=  WaitOp  AcpiEventObject Timeout */

    case AML_WAIT_OP:

        Status = AcpiExSystemWaitEvent (TimeDesc, ObjDesc);
        break;


    default:

        REPORT_ERROR (("AcpiExDyadic2S: Unknown dyadic synchronization opcode %X\n", Opcode));
        Status = AE_AML_BAD_OPCODE;
        goto Cleanup;
    }


    /*
     * Return a boolean indicating if operation timed out
     * (TRUE) or not (FALSE)
     */

    if (Status == AE_TIME)
    {
        RetDesc->Integer.Value = ACPI_INTEGER_MAX;   /* TRUE, op timed out */
        Status = AE_OK;
    }


Cleanup:

    /* Delete params */

    AcpiUtRemoveReference (TimeDesc);
    AcpiUtRemoveReference (ObjDesc);

    /* Delete return object on error */

    if (ACPI_FAILURE (Status) &&
        (RetDesc))
    {
        AcpiUtRemoveReference (RetDesc);
        RetDesc = NULL;
    }


    /* Set the return object and exit */

    *ReturnDesc = RetDesc;
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExDyadic2
 *
 * PARAMETERS:  Opcode              - The opcode to be executed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute Type 2 dyadic operator with numeric operands and
 *              no result operands
 *
 * ALLOCATION:  Deletes one operand descriptor -- other remains on stack
 *              containing result value
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExDyadic2 (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **ReturnDesc)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_OPERAND_OBJECT     *ObjDesc2;
    ACPI_OPERAND_OBJECT     *RetDesc = NULL;
    ACPI_STATUS             Status;
    BOOLEAN                 Lboolean;


    FUNCTION_TRACE_PTR ("ExDyadic2", WALK_OPERANDS);


    /* Resolve all operands */

    Status = AcpiExResolveOperands (Opcode, WALK_OPERANDS, WalkState);
    DUMP_OPERANDS (WALK_OPERANDS, IMODE_EXECUTE, AcpiPsGetOpcodeName (Opcode),
                    2, "after AcpiExResolveOperands");

    /* Get all operands */

    Status |= AcpiDsObjStackPopObject (&ObjDesc2, WalkState);
    Status |= AcpiDsObjStackPopObject (&ObjDesc, WalkState);
    if (ACPI_FAILURE (Status))
    {
        /* Invalid parameters on object stack  */

        DEBUG_PRINTP (ACPI_ERROR, ("(%s) bad operand(s) %s\n",
            AcpiPsGetOpcodeName (Opcode), AcpiUtFormatException (Status)));

        goto Cleanup;
    }


    /* Create the internal return object */

    RetDesc = AcpiUtCreateInternalObject (ACPI_TYPE_INTEGER);
    if (!RetDesc)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }

    /*
     * Execute the Opcode
     */

    Lboolean = FALSE;
    switch (Opcode)
    {

    /* DefLAnd :=  LAndOp  Operand1    Operand2    */

    case AML_LAND_OP:

        Lboolean = (BOOLEAN) (ObjDesc->Integer.Value &&
                              ObjDesc2->Integer.Value);
        break;


    /* DefLEqual   :=  LEqualOp    Operand1    Operand2    */

    case AML_LEQUAL_OP:

        Lboolean = (BOOLEAN) (ObjDesc->Integer.Value ==
                              ObjDesc2->Integer.Value);
        break;


    /* DefLGreater :=  LGreaterOp  Operand1    Operand2    */

    case AML_LGREATER_OP:

        Lboolean = (BOOLEAN) (ObjDesc->Integer.Value >
                              ObjDesc2->Integer.Value);
        break;


    /* DefLLess    :=  LLessOp Operand1    Operand2    */

    case AML_LLESS_OP:

        Lboolean = (BOOLEAN) (ObjDesc->Integer.Value <
                              ObjDesc2->Integer.Value);
        break;


    /* DefLOr  :=  LOrOp   Operand1    Operand2    */

    case AML_LOR_OP:

        Lboolean = (BOOLEAN) (ObjDesc->Integer.Value ||
                              ObjDesc2->Integer.Value);
        break;


    default:

        REPORT_ERROR (("AcpiExDyadic2: Unknown dyadic opcode %X\n", Opcode));
        Status = AE_AML_BAD_OPCODE;
        goto Cleanup;
        break;
    }


    /* Set return value to logical TRUE (all ones) or FALSE (zero) */

    if (Lboolean)
    {
        RetDesc->Integer.Value = ACPI_INTEGER_MAX;
    }
    else
    {
        RetDesc->Integer.Value = 0;
    }


Cleanup:

    /* Always delete operands */

    AcpiUtRemoveReference (ObjDesc);
    AcpiUtRemoveReference (ObjDesc2);


    /* Delete return object on error */

    if (ACPI_FAILURE (Status) &&
        (RetDesc))
    {
        AcpiUtRemoveReference (RetDesc);
        RetDesc = NULL;
    }


    /* Set the return object and exit */

    *ReturnDesc = RetDesc;
    return_ACPI_STATUS (Status);
}


