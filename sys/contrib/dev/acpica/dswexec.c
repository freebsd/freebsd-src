/******************************************************************************
 *
 * Module Name: dswexec - Dispatcher method execution callbacks;
 *                        dispatch to interpreter.
 *              $Revision: 48 $
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

#define __DSWEXEC_C__

#include "acpi.h"
#include "acparser.h"
#include "amlcode.h"
#include "acdispat.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "acdebug.h"


#define _COMPONENT          DISPATCHER
        MODULE_NAME         ("dswexec")


/*****************************************************************************
 *
 * FUNCTION:    AcpiDsGetPredicateValue
 *
 * PARAMETERS:  WalkState       - Current state of the parse tree walk
 *
 * RETURN:      Status
 *
 * DESCRIPTION:
 *
 ****************************************************************************/

ACPI_STATUS
AcpiDsGetPredicateValue (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  HasResultObj)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     *ObjDesc;


    FUNCTION_TRACE_PTR ("DsGetPredicateValue", WalkState);


    WalkState->ControlState->Common.State = 0;

    if (HasResultObj)
    {
        Status = AcpiDsResultStackPop (&ObjDesc, WalkState);
        if (ACPI_FAILURE (Status))
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("DsGetPredicateValue: Missing or null operand, %s\n",
                AcpiCmFormatException (Status)));

            return_ACPI_STATUS (Status);
        }
    }

    else
    {
        Status = AcpiDsCreateOperand (WalkState, Op);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        Status = AcpiAmlResolveToValue (&WalkState->Operands [0], WalkState);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        ObjDesc = WalkState->Operands [0];
    }

    if (!ObjDesc)
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("ExecEndOp: No predicate ObjDesc=%X State=%X\n",
            ObjDesc, WalkState));

        return_ACPI_STATUS (AE_AML_NO_OPERAND);
    }


    /*
     * Result of predicate evaluation currently must
     * be a number
     */

    if (ObjDesc->Common.Type != ACPI_TYPE_NUMBER)
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("ExecEndOp: Bad predicate (not a number) ObjDesc=%X State=%X Type=%X\n",
            ObjDesc, WalkState, ObjDesc->Common.Type));

        Status = AE_AML_OPERAND_TYPE;
        goto Cleanup;
    }


    /* TBD: 64/32-bit */

    ObjDesc->Number.Value &= (UINT64) 0x00000000FFFFFFFF;

    /*
     * Save the result of the predicate evaluation on
     * the control stack
     */

    if (ObjDesc->Number.Value)
    {
        WalkState->ControlState->Common.Value = TRUE;
    }

    else
    {
        /*
         * Predicate is FALSE, we will just toss the
         * rest of the package
         */

        WalkState->ControlState->Common.Value = FALSE;
        Status = AE_CTRL_FALSE;
    }


Cleanup:

    DEBUG_PRINT (TRACE_EXEC,
        ("ExecEndOp: Completed a predicate eval=%X Op=%X\n",
        WalkState->ControlState->Common.Value, Op));

     /* Break to debugger to display result */

    DEBUGGER_EXEC (AcpiDbDisplayResultObject (ObjDesc, WalkState));

    /*
     * Delete the predicate result object (we know that
     * we don't need it anymore)
     */

    AcpiCmRemoveReference (ObjDesc);

    WalkState->ControlState->Common.State = CONTROL_NORMAL;

    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiDsExecBeginOp
 *
 * PARAMETERS:  WalkState       - Current state of the parse tree walk
 *              Op              - Op that has been just been reached in the
 *                                walk;  Arguments have not been evaluated yet.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending callback used during the execution of control
 *              methods.  This is where most operators and operands are
 *              dispatched to the interpreter.
 *
 ****************************************************************************/

ACPI_STATUS
AcpiDsExecBeginOp (
    UINT16                  Opcode,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       **OutOp)
{
    ACPI_OPCODE_INFO        *OpInfo;
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE_PTR ("DsExecBeginOp", Op);


    if (!Op)
    {
        Status = AcpiDsLoad2BeginOp (Opcode, NULL, WalkState, OutOp);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        Op = *OutOp;
    }

    if (Op == WalkState->Origin)
    {
        if (OutOp)
        {
            *OutOp = Op;
        }

        return_ACPI_STATUS (AE_OK);
    }

    /*
     * If the previous opcode was a conditional, this opcode
     * must be the beginning of the associated predicate.
     * Save this knowledge in the current scope descriptor
     */

    if ((WalkState->ControlState) &&
        (WalkState->ControlState->Common.State ==
            CONTROL_CONDITIONAL_EXECUTING))
    {
        DEBUG_PRINT (TRACE_EXEC, ("BeginOp: Exec predicate Op=%X State=%X\n",
                        Op, WalkState));

        WalkState->ControlState->Common.State = CONTROL_PREDICATE_EXECUTING;

        /* Save start of predicate */

        WalkState->ControlState->Control.PredicateOp = Op;
    }


    OpInfo = AcpiPsGetOpcodeInfo (Op->Opcode);

    /* We want to send namepaths to the load code */

    if (Op->Opcode == AML_NAMEPATH_OP)
    {
        OpInfo->Flags = OPTYPE_NAMED_OBJECT;
    }


    /*
     * Handle the opcode based upon the opcode type
     */

    switch (ACPI_GET_OP_CLASS (OpInfo))
    {
    case OPTYPE_CONTROL:

        Status = AcpiDsExecBeginControlOp (WalkState, Op);
        break;


    case OPTYPE_NAMED_OBJECT:

        if (WalkState->WalkType == WALK_METHOD)
        {
            /*
             * Found a named object declaration during method
             * execution;  we must enter this object into the
             * namespace.  The created object is temporary and
             * will be deleted upon completion of the execution
             * of this method.
             */

            Status = AcpiDsLoad2BeginOp (Op->Opcode, Op, WalkState, NULL);
        }
        break;


    default:
        break;
    }

    /* Nothing to do here during method execution */

    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiDsExecEndOp
 *
 * PARAMETERS:  WalkState       - Current state of the parse tree walk
 *              Op              - Op that has been just been completed in the
 *                                walk;  Arguments have now been evaluated.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Ascending callback used during the execution of control
 *              methods.  The only thing we really need to do here is to
 *              notice the beginning of IF, ELSE, and WHILE blocks.
 *
 ****************************************************************************/

ACPI_STATUS
AcpiDsExecEndOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_STATUS             Status = AE_OK;
    UINT16                  Opcode;
    UINT8                   Optype;
    ACPI_PARSE_OBJECT       *NextOp;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_PARSE_OBJECT       *FirstArg;
    ACPI_OPERAND_OBJECT     *ResultObj = NULL;
    ACPI_OPCODE_INFO        *OpInfo;
    UINT32                  OperandIndex;


    FUNCTION_TRACE_PTR ("DsExecEndOp", Op);


    Opcode = (UINT16) Op->Opcode;


    OpInfo = AcpiPsGetOpcodeInfo (Op->Opcode);
    if (ACPI_GET_OP_TYPE (OpInfo) != ACPI_OP_TYPE_OPCODE)
    {
        DEBUG_PRINT (ACPI_ERROR, ("ExecEndOp: Unknown opcode. Op=%X\n",
                        Op));

        return_ACPI_STATUS (AE_NOT_IMPLEMENTED);
    }

    Optype = (UINT8) ACPI_GET_OP_CLASS (OpInfo);
    FirstArg = Op->Value.Arg;

    /* Init the walk state */

    WalkState->NumOperands = 0;
    WalkState->ReturnDesc = NULL;


    /* Call debugger for single step support (DEBUG build only) */

    DEBUGGER_EXEC (Status = AcpiDbSingleStep (WalkState, Op, Optype));
    DEBUGGER_EXEC (if (ACPI_FAILURE (Status)) {return_ACPI_STATUS (Status);});


    /* Decode the opcode */

    switch (Optype)
    {
    case OPTYPE_UNDEFINED:

        DEBUG_PRINT (ACPI_ERROR, ("ExecEndOp: Undefined opcode type Op=%X\n",
            Op));
        return_ACPI_STATUS (AE_NOT_IMPLEMENTED);
        break;


    case OPTYPE_BOGUS:
        DEBUG_PRINT (TRACE_DISPATCH,
            ("ExecEndOp: Internal opcode=%X type Op=%X\n",
            Opcode, Op));
        break;

    case OPTYPE_CONSTANT:           /* argument type only */
    case OPTYPE_LITERAL:            /* argument type only */
    case OPTYPE_DATA_TERM:          /* argument type only */
    case OPTYPE_LOCAL_VARIABLE:     /* argument type only */
    case OPTYPE_METHOD_ARGUMENT:    /* argument type only */
        break;


    /* most operators with arguments */

    case OPTYPE_MONADIC1:
    case OPTYPE_DYADIC1:
    case OPTYPE_MONADIC2:
    case OPTYPE_MONADIC2R:
    case OPTYPE_DYADIC2:
    case OPTYPE_DYADIC2R:
    case OPTYPE_DYADIC2S:
    case OPTYPE_RECONFIGURATION:
    case OPTYPE_INDEX:
    case OPTYPE_MATCH:
    case OPTYPE_FATAL:


        Status = AcpiDsCreateOperands (WalkState, FirstArg);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        OperandIndex = WalkState->NumOperands - 1;

        switch (Optype)
        {

        case OPTYPE_MONADIC1:

            /* 1 Operand, 0 ExternalResult, 0 InternalResult */

            Status = AcpiAmlExecMonadic1 (Opcode, WalkState);
            break;


        case OPTYPE_MONADIC2:

            /* 1 Operand, 0 ExternalResult, 1 InternalResult */

            Status = AcpiAmlExecMonadic2 (Opcode, WalkState, &ResultObj);
            if (ACPI_SUCCESS (Status))
            {
                Status = AcpiDsResultStackPush (ResultObj, WalkState);
            }

            break;


        case OPTYPE_MONADIC2R:

            /* 1 Operand, 1 ExternalResult, 1 InternalResult */

            Status = AcpiAmlExecMonadic2R (Opcode, WalkState, &ResultObj);
            if (ACPI_SUCCESS (Status))
            {
                Status = AcpiDsResultStackPush (ResultObj, WalkState);
            }

            break;


        case OPTYPE_DYADIC1:

            /* 2 Operands, 0 ExternalResult, 0 InternalResult */

            Status = AcpiAmlExecDyadic1 (Opcode, WalkState);

            break;


        case OPTYPE_DYADIC2:

            /* 2 Operands, 0 ExternalResult, 1 InternalResult */

            Status = AcpiAmlExecDyadic2 (Opcode, WalkState, &ResultObj);
            if (ACPI_SUCCESS (Status))
            {
                Status = AcpiDsResultStackPush (ResultObj, WalkState);
            }

            break;


        case OPTYPE_DYADIC2R:

            /* 2 Operands, 1 or 2 ExternalResults, 1 InternalResult */


            /* NEW INTERFACE:
             * Pass in WalkState, keep result obj  but let interpreter
             * push the result
             */

            Status = AcpiAmlExecDyadic2R (Opcode, WalkState, &ResultObj);
            if (ACPI_SUCCESS (Status))
            {
                Status = AcpiDsResultStackPush (ResultObj, WalkState);
            }

            break;


        case OPTYPE_DYADIC2S:   /* Synchronization Operator */

            /* 2 Operands, 0 ExternalResult, 1 InternalResult */

            Status = AcpiAmlExecDyadic2S (Opcode, WalkState, &ResultObj);
            if (ACPI_SUCCESS (Status))
            {
                Status = AcpiDsResultStackPush (ResultObj, WalkState);
            }

            break;


        case OPTYPE_RECONFIGURATION:

            /* 1 or 2 operands, 0 Internal Result */

            Status = AcpiAmlExecReconfiguration (Opcode, WalkState);
            break;


        case OPTYPE_FATAL:

            /* 3 Operands, 0 ExternalResult, 0 InternalResult */

            Status = AcpiAmlExecFatal (WalkState);
            break;


        case OPTYPE_INDEX:  /* Type 2 opcode with 3 operands */

            /* 3 Operands, 1 ExternalResult, 1 InternalResult */

            Status = AcpiAmlExecIndex (WalkState, &ResultObj);
            if (ACPI_SUCCESS (Status))
            {
                Status = AcpiDsResultStackPush (ResultObj, WalkState);
            }

            break;


        case OPTYPE_MATCH:  /* Type 2 opcode with 6 operands */

            /* 6 Operands, 0 ExternalResult, 1 InternalResult */

            Status = AcpiAmlExecMatch (WalkState, &ResultObj);
            if (ACPI_SUCCESS (Status))
            {
                Status = AcpiDsResultStackPush (ResultObj, WalkState);
            }

            break;
        }

        break;


    case OPTYPE_CONTROL:    /* Type 1 opcode, IF/ELSE/WHILE/NOOP */

        /* 1 Operand, 0 ExternalResult, 0 InternalResult */

        Status = AcpiDsExecEndControlOp (WalkState, Op);

        break;


    case OPTYPE_METHOD_CALL:

        DEBUG_PRINT (TRACE_DISPATCH,
            ("ExecEndOp: Method invocation, Op=%X\n", Op));

        /*
         * (AML_METHODCALL) Op->Value->Arg->Node contains
         * the method Node pointer
         */
        /* NextOp points to the op that holds the method name */

        NextOp = FirstArg;
        Node = NextOp->Node;

        /* NextOp points to first argument op */

        NextOp = NextOp->Next;

        /*
         * Get the method's arguments and put them on the operand stack
         */
        Status = AcpiDsCreateOperands (WalkState, NextOp);
        if (ACPI_FAILURE (Status))
        {
            break;
        }

        /*
         * Since the operands will be passed to another
         * control method, we must resolve all local
         * references here (Local variables, arguments
         * to *this* method, etc.)
         */

        Status = AcpiDsResolveOperands (WalkState);
        if (ACPI_FAILURE (Status))
        {
            break;
        }

        /*
         * Tell the walk loop to preempt this running method and
         * execute the new method
         */
        Status = AE_CTRL_TRANSFER;

        /*
         * Return now; we don't want to disturb anything,
         * especially the operand count!
         */
        return_ACPI_STATUS (Status);
        break;


    case OPTYPE_CREATE_FIELD:

        DEBUG_PRINT (TRACE_EXEC,
            ("ExecEndOp: Executing CreateField Buffer/Index Op=%X\n",
            Op));

        Status = AcpiDsLoad2EndOp (WalkState, Op);
        if (ACPI_FAILURE (Status))
        {
            break;
        }

        Status = AcpiDsEvalFieldUnitOperands (WalkState, Op);
        break;


    case OPTYPE_NAMED_OBJECT:

        Status = AcpiDsLoad2EndOp (WalkState, Op);
        if (ACPI_FAILURE (Status))
        {
            break;
        }

        switch (Op->Opcode)
        {
        case AML_REGION_OP:

            DEBUG_PRINT (TRACE_EXEC,
                ("ExecEndOp: Executing OpRegion Address/Length Op=%X\n",
                Op));

            Status = AcpiDsEvalRegionOperands (WalkState, Op);

            break;


        case AML_METHOD_OP:

            break;


        case AML_ALIAS_OP:

            /* Alias creation was already handled by call
            to psxload above */
            break;


        default:
            /* Nothing needs to be done */

            Status = AE_OK;
            break;
        }

        break;

    default:

        DEBUG_PRINT (ACPI_ERROR,
            ("ExecEndOp: Unimplemented opcode, type=%X Opcode=%X Op=%X\n",
            Optype, Op->Opcode, Op));

        Status = AE_NOT_IMPLEMENTED;
        break;
    }


    /*
     * ACPI 2.0 support for 64-bit integers:
     * Truncate numeric result value if we are executing from a 32-bit ACPI table
     */
    AcpiAmlTruncateFor32bitTable (ResultObj, WalkState);

    /*
     * Check if we just completed the evaluation of a
     * conditional predicate
     */

    if ((WalkState->ControlState) &&
        (WalkState->ControlState->Common.State ==
            CONTROL_PREDICATE_EXECUTING) &&
        (WalkState->ControlState->Control.PredicateOp == Op))
    {
        Status = AcpiDsGetPredicateValue (WalkState, Op, (UINT32) ResultObj);
        ResultObj = NULL;
    }


Cleanup:
    if (ResultObj)
    {
        /* Break to debugger to display result */

        DEBUGGER_EXEC (AcpiDbDisplayResultObject (ResultObj, WalkState));

        /*
         * Delete the result op if and only if:
         * Parent will not use the result -- such as any
         * non-nested type2 op in a method (parent will be method)
         */
        AcpiDsDeleteResultIfNotUsed (Op, ResultObj, WalkState);
    }

    /* Always clear the object stack */

    /* TBD: [Investigate] Clear stack of return value,
    but don't delete it */
    WalkState->NumOperands = 0;

    return_ACPI_STATUS (Status);
}


