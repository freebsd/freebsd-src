/******************************************************************************
 *
 * Module Name: amdyadic - ACPI AML (p-code) execution for dyadic operators
 *              $Revision: 65 $
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


#define __AMDYADIC_C__

#include "acpi.h"
#include "acparser.h"
#include "acnamesp.h"
#include "acinterp.h"
#include "acevents.h"
#include "amlcode.h"
#include "acdispat.h"


#define _COMPONENT          INTERPRETER
        MODULE_NAME         ("amdyadic")


/*****************************************************************************
 *
 * FUNCTION:    AcpiAmlExecDyadic1
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
 ****************************************************************************/

ACPI_STATUS
AcpiAmlExecDyadic1 (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_OPERAND_OBJECT     *ObjDesc = NULL;
    ACPI_OPERAND_OBJECT     *ValDesc = NULL;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE_PTR ("AmlExecDyadic1", WALK_OPERANDS);


    /* Resolve all operands */

    Status = AcpiAmlResolveOperands (Opcode, WALK_OPERANDS, WalkState);
    DUMP_OPERANDS (WALK_OPERANDS, IMODE_EXECUTE, AcpiPsGetOpcodeName (Opcode),
                    2, "after AcpiAmlResolveOperands");

    /* Get the operands */

    Status |= AcpiDsObjStackPopObject (&ValDesc, WalkState);
    Status |= AcpiDsObjStackPopObject (&ObjDesc, WalkState);
    if (ACPI_FAILURE (Status))
    {
        /* Invalid parameters on object stack  */

        DEBUG_PRINT (ACPI_ERROR,
            ("ExecDyadic1/%s: bad operand(s) (0x%X)\n",
            AcpiPsGetOpcodeName (Opcode), Status));

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
                 * Requires that Device and ThermalZone be compatible
                 * mappings
                 */

                /* Dispatch the notify to the appropriate handler */

                AcpiEvNotifyDispatch (Node, (UINT32) ValDesc->Number.Value);
                break;

            default:
                DEBUG_PRINT (ACPI_ERROR,
                    ("AmlExecDyadic1/NotifyOp: unexpected notify object type %d\n",
                    ObjDesc->Common.Type));

                Status = AE_AML_OPERAND_TYPE;
            }
        }
        break;

    default:

        REPORT_ERROR (("AcpiAmlExecDyadic1: Unknown dyadic opcode %X\n",
            Opcode));
        Status = AE_AML_BAD_OPCODE;
    }


Cleanup:

    /* Always delete both operands */

    AcpiCmRemoveReference (ValDesc);
    AcpiCmRemoveReference (ObjDesc);


    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiAmlExecDyadic2R
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
 ****************************************************************************/

ACPI_STATUS
AcpiAmlExecDyadic2R (
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
    ACPI_INTEGER            Remainder;
    UINT32                  NumOperands = 3;
    NATIVE_CHAR             *NewBuf;


    FUNCTION_TRACE_U32 ("AmlExecDyadic2R", Opcode);


    /* Resolve all operands */

    Status = AcpiAmlResolveOperands (Opcode, WALK_OPERANDS, WalkState);
    DUMP_OPERANDS (WALK_OPERANDS, IMODE_EXECUTE, AcpiPsGetOpcodeName (Opcode),
                    NumOperands, "after AcpiAmlResolveOperands");

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
        DEBUG_PRINT (ACPI_ERROR,
            ("ExecDyadic2R/%s: bad operand(s) (0x%X)\n",
            AcpiPsGetOpcodeName (Opcode), Status));

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

        RetDesc = AcpiCmCreateInternalObject (ACPI_TYPE_NUMBER);
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

        RetDesc->Number.Value = ObjDesc->Number.Value +
                                ObjDesc2->Number.Value;
        break;


    /* DefAnd  :=  AndOp   Operand1    Operand2    Result  */

    case AML_BIT_AND_OP:

        RetDesc->Number.Value = ObjDesc->Number.Value &
                                ObjDesc2->Number.Value;
        break;


    /* DefNAnd :=  NAndOp  Operand1    Operand2    Result  */

    case AML_BIT_NAND_OP:

        RetDesc->Number.Value = ~(ObjDesc->Number.Value &
                                  ObjDesc2->Number.Value);
        break;


    /* DefOr   :=  OrOp    Operand1    Operand2    Result  */

    case AML_BIT_OR_OP:

        RetDesc->Number.Value = ObjDesc->Number.Value |
                                ObjDesc2->Number.Value;
        break;


    /* DefNOr  :=  NOrOp   Operand1    Operand2    Result  */

    case AML_BIT_NOR_OP:

        RetDesc->Number.Value = ~(ObjDesc->Number.Value |
                                  ObjDesc2->Number.Value);
        break;


    /* DefXOr  :=  XOrOp   Operand1    Operand2    Result  */

    case AML_BIT_XOR_OP:

        RetDesc->Number.Value = ObjDesc->Number.Value ^
                                ObjDesc2->Number.Value;
        break;


    /* DefDivide   :=  DivideOp Dividend Divisor Remainder Quotient    */

    case AML_DIVIDE_OP:

        if ((UINT32) 0 == ObjDesc2->Number.Value)
        {
            REPORT_ERROR
                (("AmlExecDyadic2R/DivideOp: Divide by zero\n"));

            Status = AE_AML_DIVIDE_BY_ZERO;
            goto Cleanup;
        }

        RetDesc2 = AcpiCmCreateInternalObject (ACPI_TYPE_NUMBER);
        if (!RetDesc2)
        {
            Status = AE_NO_MEMORY;
            goto Cleanup;
        }

        Remainder               = ObjDesc->Number.Value %
                                  ObjDesc2->Number.Value;
        RetDesc->Number.Value   = Remainder;

        /* Result (what we used to call the quotient) */

        RetDesc2->Number.Value  = ObjDesc->Number.Value /
                                    ObjDesc2->Number.Value;
        break;


    /* DefMultiply :=  MultiplyOp  Operand1    Operand2    Result  */

    case AML_MULTIPLY_OP:

        RetDesc->Number.Value = ObjDesc->Number.Value *
                                ObjDesc2->Number.Value;
        break;


    /* DefShiftLeft    :=  ShiftLeftOp Operand ShiftCount  Result  */

    case AML_SHIFT_LEFT_OP:

        RetDesc->Number.Value = ObjDesc->Number.Value <<
                                ObjDesc2->Number.Value;
        break;


    /* DefShiftRight   :=  ShiftRightOp    Operand ShiftCount  Result  */

    case AML_SHIFT_RIGHT_OP:

        RetDesc->Number.Value = ObjDesc->Number.Value >>
                                ObjDesc2->Number.Value;
        break;


    /* DefSubtract :=  SubtractOp  Operand1    Operand2    Result  */

    case AML_SUBTRACT_OP:

        RetDesc->Number.Value = ObjDesc->Number.Value -
                                ObjDesc2->Number.Value;
        break;


    /* DefConcat   :=  ConcatOp    Data1   Data2   Result  */

    case AML_CONCAT_OP:

        if (ObjDesc2->Common.Type != ObjDesc->Common.Type)
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("AmlExecDyadic2R/ConcatOp: operand type mismatch %d %d\n",
                ObjDesc->Common.Type, ObjDesc2->Common.Type));
            Status = AE_AML_OPERAND_TYPE;
            goto Cleanup;
        }

        /* Both operands are now known to be the same */

        if (ACPI_TYPE_STRING == ObjDesc->Common.Type)
        {
            RetDesc = AcpiCmCreateInternalObject (ACPI_TYPE_STRING);
            if (!RetDesc)
            {
                Status = AE_NO_MEMORY;
                goto Cleanup;
            }

            /* Operand1 is string  */

            NewBuf = AcpiCmAllocate (ObjDesc->String.Length +
                                     ObjDesc2->String.Length + 1);
            if (!NewBuf)
            {
                REPORT_ERROR
                    (("AmlExecDyadic2R/ConcatOp: String allocation failure\n"));
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
        }

        else
        {
            /* Operand1 is not a string ==> must be a buffer */

            RetDesc = AcpiCmCreateInternalObject (ACPI_TYPE_BUFFER);
            if (!RetDesc)
            {
                Status = AE_NO_MEMORY;
                goto Cleanup;
            }

            NewBuf = AcpiCmAllocate (ObjDesc->Buffer.Length +
                                     ObjDesc2->Buffer.Length);
            if (!NewBuf)
            {
                REPORT_ERROR
                    (("AmlExecDyadic2R/ConcatOp: Buffer allocation failure\n"));
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
        }
        break;


    default:

        REPORT_ERROR (("AcpiAmlExecDyadic2R: Unknown dyadic opcode %X\n", Opcode));
        Status = AE_AML_BAD_OPCODE;
        goto Cleanup;
    }


    /*
     * Store the result of the operation (which is now in ObjDesc) into
     * the result descriptor, or the location pointed to by the result
     * descriptor (ResDesc).
     */

    Status = AcpiAmlExecStore (RetDesc, ResDesc, WalkState);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    if (AML_DIVIDE_OP == Opcode)
    {
        Status = AcpiAmlExecStore (RetDesc2, ResDesc2, WalkState);

        /*
         * Since the remainder is not returned, remove a reference to
         * the object we created earlier
         */

        AcpiCmRemoveReference (RetDesc2);
    }


Cleanup:

    /* Always delete the operands */

    AcpiCmRemoveReference (ObjDesc);
    AcpiCmRemoveReference (ObjDesc2);


    /* Delete return object on error */

    if (ACPI_FAILURE (Status))
    {
        /* On failure, delete the result ops */

        AcpiCmRemoveReference (ResDesc);
        AcpiCmRemoveReference (ResDesc2);

        if (RetDesc)
        {
            /* And delete the internal return object */

            AcpiCmRemoveReference (RetDesc);
            RetDesc = NULL;
        }
    }

    /* Set the return object and exit */

    *ReturnDesc = RetDesc;
    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiAmlExecDyadic2S
 *
 * PARAMETERS:  Opcode              - The opcode to be executed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute Type 2 dyadic synchronization operator
 *
 * ALLOCATION:  Deletes one operand descriptor -- other remains on stack
 *
 ****************************************************************************/

ACPI_STATUS
AcpiAmlExecDyadic2S (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **ReturnDesc)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_OPERAND_OBJECT     *TimeDesc;
    ACPI_OPERAND_OBJECT     *RetDesc = NULL;
    ACPI_STATUS             Status;


    FUNCTION_TRACE_PTR ("AmlExecDyadic2S", WALK_OPERANDS);


    /* Resolve all operands */

    Status = AcpiAmlResolveOperands (Opcode, WALK_OPERANDS, WalkState);
    DUMP_OPERANDS (WALK_OPERANDS, IMODE_EXECUTE, AcpiPsGetOpcodeName (Opcode),
                    2, "after AcpiAmlResolveOperands");

    /* Get all operands */

    Status |= AcpiDsObjStackPopObject (&TimeDesc, WalkState);
    Status |= AcpiDsObjStackPopObject (&ObjDesc, WalkState);
    if (ACPI_FAILURE (Status))
    {
        /* Invalid parameters on object stack  */

        DEBUG_PRINT (ACPI_ERROR,
            ("ExecDyadic2S/%s: bad operand(s) (0x%X)\n",
            AcpiPsGetOpcodeName (Opcode), Status));

        goto Cleanup;
    }


    /* Create the internal return object */

    RetDesc = AcpiCmCreateInternalObject (ACPI_TYPE_NUMBER);
    if (!RetDesc)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }

    /* Default return value is FALSE, operation did not time out */

    RetDesc->Number.Value = 0;


    /* Examine the opcode */

    switch (Opcode)
    {

    /* DefAcquire  :=  AcquireOp   MutexObject Timeout */

    case AML_ACQUIRE_OP:

        Status = AcpiAmlSystemAcquireMutex (TimeDesc, ObjDesc);
        break;


    /* DefWait :=  WaitOp  AcpiEventObject Timeout */

    case AML_WAIT_OP:

        Status = AcpiAmlSystemWaitEvent (TimeDesc, ObjDesc);
        break;


    default:

        REPORT_ERROR (("AcpiAmlExecDyadic2S: Unknown dyadic synchronization opcode %X\n", Opcode));
        Status = AE_AML_BAD_OPCODE;
        goto Cleanup;
    }


    /*
     * Return a boolean indicating if operation timed out
     * (TRUE) or not (FALSE)
     */

    if (Status == AE_TIME)
    {
        RetDesc->Number.Value = ACPI_INTEGER_MAX;   /* TRUE, op timed out */
        Status = AE_OK;
    }


Cleanup:

    /* Delete params */

    AcpiCmRemoveReference (TimeDesc);
    AcpiCmRemoveReference (ObjDesc);

    /* Delete return object on error */

    if (ACPI_FAILURE (Status) &&
        (RetDesc))
    {
        AcpiCmRemoveReference (RetDesc);
        RetDesc = NULL;
    }


    /* Set the return object and exit */

    *ReturnDesc = RetDesc;
    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiAmlExecDyadic2
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
 ****************************************************************************/

ACPI_STATUS
AcpiAmlExecDyadic2 (
    UINT16                  Opcode,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **ReturnDesc)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_OPERAND_OBJECT     *ObjDesc2;
    ACPI_OPERAND_OBJECT     *RetDesc = NULL;
    ACPI_STATUS             Status;
    BOOLEAN                 Lboolean;


    FUNCTION_TRACE_PTR ("AmlExecDyadic2", WALK_OPERANDS);


    /* Resolve all operands */

    Status = AcpiAmlResolveOperands (Opcode, WALK_OPERANDS, WalkState);
    DUMP_OPERANDS (WALK_OPERANDS, IMODE_EXECUTE, AcpiPsGetOpcodeName (Opcode),
                    2, "after AcpiAmlResolveOperands");

    /* Get all operands */

    Status |= AcpiDsObjStackPopObject (&ObjDesc2, WalkState);
    Status |= AcpiDsObjStackPopObject (&ObjDesc, WalkState);
    if (ACPI_FAILURE (Status))
    {
        /* Invalid parameters on object stack  */

        DEBUG_PRINT (ACPI_ERROR,
            ("ExecDyadic2/%s: bad operand(s) (0x%X)\n",
            AcpiPsGetOpcodeName (Opcode), Status));

        goto Cleanup;
    }


    /* Create the internal return object */

    RetDesc = AcpiCmCreateInternalObject (ACPI_TYPE_NUMBER);
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

        Lboolean = (BOOLEAN) (ObjDesc->Number.Value &&
                              ObjDesc2->Number.Value);
        break;


    /* DefLEqual   :=  LEqualOp    Operand1    Operand2    */

    case AML_LEQUAL_OP:

        Lboolean = (BOOLEAN) (ObjDesc->Number.Value ==
                              ObjDesc2->Number.Value);
        break;


    /* DefLGreater :=  LGreaterOp  Operand1    Operand2    */

    case AML_LGREATER_OP:

        Lboolean = (BOOLEAN) (ObjDesc->Number.Value >
                              ObjDesc2->Number.Value);
        break;


    /* DefLLess    :=  LLessOp Operand1    Operand2    */

    case AML_LLESS_OP:

        Lboolean = (BOOLEAN) (ObjDesc->Number.Value <
                              ObjDesc2->Number.Value);
        break;


    /* DefLOr  :=  LOrOp   Operand1    Operand2    */

    case AML_LOR_OP:

        Lboolean = (BOOLEAN) (ObjDesc->Number.Value ||
                              ObjDesc2->Number.Value);
        break;


    default:

        REPORT_ERROR (("AcpiAmlExecDyadic2: Unknown dyadic opcode %X\n", Opcode));
        Status = AE_AML_BAD_OPCODE;
        goto Cleanup;
        break;
    }


    /* Set return value to logical TRUE (all ones) or FALSE (zero) */

    if (Lboolean)
    {
        RetDesc->Number.Value = ACPI_INTEGER_MAX;
    }
    else
    {
        RetDesc->Number.Value = 0;
    }


Cleanup:

    /* Always delete operands */

    AcpiCmRemoveReference (ObjDesc);
    AcpiCmRemoveReference (ObjDesc2);


    /* Delete return object on error */

    if (ACPI_FAILURE (Status) &&
        (RetDesc))
    {
        AcpiCmRemoveReference (RetDesc);
        RetDesc = NULL;
    }


    /* Set the return object and exit */

    *ReturnDesc = RetDesc;
    return_ACPI_STATUS (Status);
}


