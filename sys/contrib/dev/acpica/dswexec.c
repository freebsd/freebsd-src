/******************************************************************************
 *
 * Module Name: dswexec - Dispatcher method execution callbacks;
 *                        dispatch to interpreter.
 *              $Revision: 79 $
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

#define __DSWEXEC_C__

#include "acpi.h"
#include "acparser.h"
#include "amlcode.h"
#include "acdispat.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "acdebug.h"


#define _COMPONENT          ACPI_DISPATCHER
        MODULE_NAME         ("dswexec")

/*
 * Dispatch tables for opcode classes 
 */
ACPI_EXECUTE_OP         AcpiGbl_OpTypeDispatch [] = {
                            AcpiExOpcode_1A_0T_0R,
                            AcpiExOpcode_1A_0T_1R,
                            AcpiExOpcode_1A_1T_0R,
                            AcpiExOpcode_1A_1T_1R,
                            AcpiExOpcode_2A_0T_0R,
                            AcpiExOpcode_2A_0T_1R,
                            AcpiExOpcode_2A_1T_1R,
                            AcpiExOpcode_2A_2T_1R,
                            AcpiExOpcode_3A_0T_0R,
                            AcpiExOpcode_3A_1T_1R,
                            AcpiExOpcode_6A_0T_1R};

/*****************************************************************************
 *
 * FUNCTION:    AcpiDsGetPredicateValue
 *
 * PARAMETERS:  WalkState       - Current state of the parse tree walk
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get the result of a predicate evaluation
 *
 ****************************************************************************/

ACPI_STATUS
AcpiDsGetPredicateValue (
    ACPI_WALK_STATE         *WalkState,
    UINT32                  HasResultObj)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     *ObjDesc;


    FUNCTION_TRACE_PTR ("DsGetPredicateValue", WalkState);


    WalkState->ControlState->Common.State = 0;

    if (HasResultObj)
    {
        Status = AcpiDsResultPop (&ObjDesc, WalkState);
        if (ACPI_FAILURE (Status))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Could not get result from predicate evaluation, %s\n",
                AcpiFormatException (Status)));

            return_ACPI_STATUS (Status);
        }
    }

    else
    {
        Status = AcpiDsCreateOperand (WalkState, WalkState->Op, 0);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        Status = AcpiExResolveToValue (&WalkState->Operands [0], WalkState);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        ObjDesc = WalkState->Operands [0];
    }

    if (!ObjDesc)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "No predicate ObjDesc=%p State=%p\n",
            ObjDesc, WalkState));

        return_ACPI_STATUS (AE_AML_NO_OPERAND);
    }


    /*
     * Result of predicate evaluation currently must
     * be a number
     */
    if (ObjDesc->Common.Type != ACPI_TYPE_INTEGER)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Bad predicate (not a number) ObjDesc=%p State=%p Type=%X\n",
            ObjDesc, WalkState, ObjDesc->Common.Type));

        Status = AE_AML_OPERAND_TYPE;
        goto Cleanup;
    }


    /* Truncate the predicate to 32-bits if necessary */

    AcpiExTruncateFor32bitTable (ObjDesc, WalkState);

    /*
     * Save the result of the predicate evaluation on
     * the control stack
     */
    if (ObjDesc->Integer.Value)
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

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Completed a predicate eval=%X Op=%pn",
        WalkState->ControlState->Common.Value, WalkState->Op));

     /* Break to debugger to display result */

    DEBUGGER_EXEC (AcpiDbDisplayResultObject (ObjDesc, WalkState));

    /*
     * Delete the predicate result object (we know that
     * we don't need it anymore)
     */
    AcpiUtRemoveReference (ObjDesc);

    WalkState->ControlState->Common.State = CONTROL_NORMAL;
    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiDsExecBeginOp
 *
 * PARAMETERS:  WalkState       - Current state of the parse tree walk
 *              OutOp           - Return op if a new one is created
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
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       **OutOp)
{
    ACPI_PARSE_OBJECT       *Op;
    ACPI_STATUS             Status = AE_OK;
    UINT32                  OpcodeClass;


    FUNCTION_TRACE_PTR ("DsExecBeginOp", WalkState);


    Op = WalkState->Op;
    if (!Op)
    {
        Status = AcpiDsLoad2BeginOp (WalkState, OutOp);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        Op = *OutOp;
        WalkState->Op = Op;
        WalkState->OpInfo = AcpiPsGetOpcodeInfo (Op->Opcode);
        WalkState->Opcode = Op->Opcode;
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
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Exec predicate Op=%p State=%p\n",
                        Op, WalkState));

        WalkState->ControlState->Common.State = CONTROL_PREDICATE_EXECUTING;

        /* Save start of predicate */

        WalkState->ControlState->Control.PredicateOp = Op;
    }


    OpcodeClass = WalkState->OpInfo->Class;

    /* We want to send namepaths to the load code */

    if (Op->Opcode == AML_INT_NAMEPATH_OP)
    {
        OpcodeClass = AML_CLASS_NAMED_OBJECT;
    }

    /*
     * Handle the opcode based upon the opcode type
     */
    switch (OpcodeClass)
    {
    case AML_CLASS_CONTROL:

        Status = AcpiDsResultStackPush (WalkState);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        Status = AcpiDsExecBeginControlOp (WalkState, Op);
        break;


    case AML_CLASS_NAMED_OBJECT:

        if (WalkState->WalkType == WALK_METHOD)
        {
            /*
             * Found a named object declaration during method
             * execution;  we must enter this object into the
             * namespace.  The created object is temporary and
             * will be deleted upon completion of the execution
             * of this method.
             */
            Status = AcpiDsLoad2BeginOp (WalkState, NULL);
        }


        if (Op->Opcode == AML_REGION_OP)
        {
            Status = AcpiDsResultStackPush (WalkState);
        }

        break;


    /* most operators with arguments */

    case AML_CLASS_EXECUTE:
    case AML_CLASS_CREATE:

        /* Start a new result/operand state */

        Status = AcpiDsResultStackPush (WalkState);
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
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_PARSE_OBJECT       *Op;
    ACPI_STATUS             Status = AE_OK;
    UINT32                  OpType;
    UINT32                  OpClass;
    ACPI_PARSE_OBJECT       *NextOp;
    ACPI_PARSE_OBJECT       *FirstArg;
    UINT32                  i;


    FUNCTION_TRACE_PTR ("DsExecEndOp", WalkState);


    Op      = WalkState->Op;
    OpType  = WalkState->OpInfo->Type;
    OpClass = WalkState->OpInfo->Class;

    if (OpClass == AML_CLASS_UNKNOWN)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Unknown opcode %X\n", Op->Opcode));
        return_ACPI_STATUS (AE_NOT_IMPLEMENTED);
    }

    FirstArg = Op->Value.Arg;

    /* Init the walk state */

    WalkState->NumOperands = 0;
    WalkState->ReturnDesc = NULL;
    WalkState->ResultObj = NULL;


    /* Call debugger for single step support (DEBUG build only) */

    DEBUGGER_EXEC (Status = AcpiDbSingleStep (WalkState, Op, OpClass));
    DEBUGGER_EXEC (if (ACPI_FAILURE (Status)) {return_ACPI_STATUS (Status);});


    switch (OpClass)
    {
    /* Decode the Opcode Class */

    case AML_CLASS_ARGUMENT: /* constants, literals, etc.  do nothing */
        break;

    /* most operators with arguments */

    case AML_CLASS_EXECUTE:

        /* Build resolved operand stack */

        Status = AcpiDsCreateOperands (WalkState, FirstArg);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        /* Done with this result state (Now that operand stack is built) */

        Status = AcpiDsResultStackPop (WalkState);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        /* Resolve all operands */

        Status = AcpiExResolveOperands (WalkState->Opcode, 
                        &(WalkState->Operands [WalkState->NumOperands -1]), 
                        WalkState);
        if (ACPI_FAILURE (Status))
        {
            /* TBD: must pop and delete operands */

            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "[%s]: Could not resolve operands, %s\n",
                AcpiPsGetOpcodeName (WalkState->Opcode), AcpiFormatException (Status)));

            /*
             * On error, we must delete all the operands and clear the 
             * operand stack
             */
            for (i = 0; i < WalkState->NumOperands; i++)
            {
                AcpiUtRemoveReference (WalkState->Operands[i]);
                WalkState->Operands[i] = NULL;
            }

            WalkState->NumOperands = 0;
            goto Cleanup;
        }

        DUMP_OPERANDS (WALK_OPERANDS, IMODE_EXECUTE, AcpiPsGetOpcodeName (WalkState->Opcode),
                        WalkState->NumOperands, "after ExResolveOperands");

        /*
         * Dispatch the request to the appropriate interpreter handler
         * routine.  There is one routine per opcode "type" based upon the
         * number of opcode arguments and return type.
         */
        Status = AcpiGbl_OpTypeDispatch [OpType] (WalkState);


        /* Delete argument objects and clear the operand stack */

        for (i = 0; i < WalkState->NumOperands; i++)
        {
            /*
             * Remove a reference to all operands, including both 
             * "Arguments" and "Targets".
             */
            AcpiUtRemoveReference (WalkState->Operands[i]);
            WalkState->Operands[i] = NULL;
        }

        WalkState->NumOperands = 0;

        /*
         * If a result object was returned from above, push it on the
         * current result stack
         */
        if (ACPI_SUCCESS (Status) &&
            WalkState->ResultObj)
        {
            Status = AcpiDsResultPush (WalkState->ResultObj, WalkState);
        }

        break;


    default:

        switch (OpType)
        {
        case AML_TYPE_CONTROL:    /* Type 1 opcode, IF/ELSE/WHILE/NOOP */

            /* 1 Operand, 0 ExternalResult, 0 InternalResult */

            Status = AcpiDsExecEndControlOp (WalkState, Op);

            AcpiDsResultStackPop (WalkState);
            break;


        case AML_TYPE_METHOD_CALL:

            ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Method invocation, Op=%p\n", Op));

            /*
             * (AML_METHODCALL) Op->Value->Arg->Node contains
             * the method Node pointer
             */
            /* NextOp points to the op that holds the method name */

            NextOp = FirstArg;

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


        case AML_TYPE_CREATE_FIELD:

            ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
                "Executing CreateField Buffer/Index Op=%p\n", Op));

            Status = AcpiDsLoad2EndOp (WalkState);
            if (ACPI_FAILURE (Status))
            {
                break;
            }

            Status = AcpiDsEvalBufferFieldOperands (WalkState, Op);
            break;


        case AML_TYPE_NAMED_FIELD:
        case AML_TYPE_NAMED_COMPLEX:
        case AML_TYPE_NAMED_SIMPLE:

            Status = AcpiDsLoad2EndOp (WalkState);
            if (ACPI_FAILURE (Status))
            {
                break;
            }

            if (Op->Opcode == AML_REGION_OP)
            {
                ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
                    "Executing OpRegion Address/Length Op=%p\n", Op));

                Status = AcpiDsEvalRegionOperands (WalkState, Op);
                if (ACPI_FAILURE (Status))
                {
                    break;
                }

                Status = AcpiDsResultStackPop (WalkState);
            }

            break;

        case AML_TYPE_UNDEFINED:

            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Undefined opcode type Op=%p\n", Op));
            return_ACPI_STATUS (AE_NOT_IMPLEMENTED);
            break;


        case AML_TYPE_BOGUS:
            ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH, "Internal opcode=%X type Op=%p\n",
                WalkState->Opcode, Op));
            break;

        default:

            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Unimplemented opcode, class=%X type=%X Opcode=%X Op=%p\n",
                OpClass, OpType, Op->Opcode, Op));

            Status = AE_NOT_IMPLEMENTED;
            break;
        }
    }

    /*
     * ACPI 2.0 support for 64-bit integers:
     * Truncate numeric result value if we are executing from a 32-bit ACPI table
     */
    AcpiExTruncateFor32bitTable (WalkState->ResultObj, WalkState);

    /*
     * Check if we just completed the evaluation of a
     * conditional predicate
     */

    if ((WalkState->ControlState) &&
        (WalkState->ControlState->Common.State ==
            CONTROL_PREDICATE_EXECUTING) &&
        (WalkState->ControlState->Control.PredicateOp == Op))
    {
        Status = AcpiDsGetPredicateValue (WalkState, (UINT32) WalkState->ResultObj);
        WalkState->ResultObj = NULL;
    }


Cleanup:
    if (WalkState->ResultObj)
    {
        /* Break to debugger to display result */

        DEBUGGER_EXEC (AcpiDbDisplayResultObject (WalkState->ResultObj, WalkState));

        /*
         * Delete the result op if and only if:
         * Parent will not use the result -- such as any
         * non-nested type2 op in a method (parent will be method)
         */
        AcpiDsDeleteResultIfNotUsed (Op, WalkState->ResultObj, WalkState);
    }

    /* Always clear the object stack */

    /* TBD: [Investigate] Clear stack of return value,
    but don't delete it */
    WalkState->NumOperands = 0;

    return_ACPI_STATUS (Status);
}


