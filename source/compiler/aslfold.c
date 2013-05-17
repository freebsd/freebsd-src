/******************************************************************************
 *
 * Module Name: aslfold - Constant folding
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */


#include "aslcompiler.h"
#include "aslcompiler.y.h"
#include "amlcode.h"

#include "acdispat.h"
#include "acparser.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslfold")

/* Local prototypes */

static ACPI_STATUS
OpcAmlEvaluationWalk1 (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
OpcAmlEvaluationWalk2 (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
OpcAmlCheckForConstant (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static void
OpcUpdateIntegerNode (
    ACPI_PARSE_OBJECT       *Op,
    UINT64                  Value);


/*******************************************************************************
 *
 * FUNCTION:    OpcAmlEvaluationWalk1
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending callback for AML execution of constant subtrees
 *
 ******************************************************************************/

static ACPI_STATUS
OpcAmlEvaluationWalk1 (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_WALK_STATE         *WalkState = Context;
    ACPI_STATUS             Status;
    ACPI_PARSE_OBJECT       *OutOp;


    WalkState->Op = Op;
    WalkState->Opcode = Op->Common.AmlOpcode;
    WalkState->OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);

    /* Copy child pointer to Arg for compatibility with Interpreter */

    if (Op->Asl.Child)
    {
        Op->Common.Value.Arg = Op->Asl.Child;
    }

    /* Call AML dispatcher */

    Status = AcpiDsExecBeginOp (WalkState, &OutOp);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Constant interpretation failed - %s\n",
                        AcpiFormatException (Status));
    }

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    OpcAmlEvaluationWalk2
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Ascending callback for AML execution of constant subtrees
 *
 ******************************************************************************/

static ACPI_STATUS
OpcAmlEvaluationWalk2 (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_WALK_STATE         *WalkState = Context;
    ACPI_STATUS             Status;


    WalkState->Op = Op;
    WalkState->Opcode = Op->Common.AmlOpcode;
    WalkState->OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);

    /* Copy child pointer to Arg for compatibility with Interpreter */

    if (Op->Asl.Child)
    {
        Op->Common.Value.Arg = Op->Asl.Child;
    }

    /* Call AML dispatcher */

    Status = AcpiDsExecEndOp (WalkState);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Constant interpretation failed - %s\n",
                        AcpiFormatException (Status));
    }

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    OpcAmlCheckForConstant
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check one Op for a type 3/4/5 AML opcode
 *
 ******************************************************************************/

static ACPI_STATUS
OpcAmlCheckForConstant (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_WALK_STATE         *WalkState = Context;


    WalkState->Op = Op;
    WalkState->Opcode = Op->Common.AmlOpcode;
    WalkState->OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);

    DbgPrint (ASL_PARSE_OUTPUT, "[%.4d] Opcode: %12.12s ",
                Op->Asl.LogicalLineNumber, Op->Asl.ParseOpName);

    /*
     * These opcodes do not appear in the OpcodeInfo table, but
     * they represent constants, so abort the constant walk now.
     */
    if ((WalkState->Opcode == AML_RAW_DATA_BYTE) ||
        (WalkState->Opcode == AML_RAW_DATA_WORD) ||
        (WalkState->Opcode == AML_RAW_DATA_DWORD) ||
        (WalkState->Opcode == AML_RAW_DATA_QWORD))
    {
        WalkState->WalkType = ACPI_WALK_CONST_OPTIONAL;
        return (AE_TYPE);
    }

    if (!(WalkState->OpInfo->Flags & AML_CONSTANT))
    {
        /* The opcode is not a Type 3/4/5 opcode */

        if (Op->Asl.CompileFlags & NODE_IS_TARGET)
        {
            DbgPrint (ASL_PARSE_OUTPUT,
                "**** Valid Target, cannot reduce ****\n");
        }
        else
        {
            DbgPrint (ASL_PARSE_OUTPUT,
                "**** Not a Type 3/4/5 opcode ****\n");
        }

        if (WalkState->WalkType == ACPI_WALK_CONST_OPTIONAL)
        {
            /*
             * We are looking at at normal expression to see if it can be
             * reduced. It can't. No error
             */
            return (AE_TYPE);
        }

        /*
         * This is an expression that MUST reduce to a constant, and it
         * can't be reduced. This is an error
         */
        if (Op->Asl.CompileFlags & NODE_IS_TARGET)
        {
            AslError (ASL_ERROR, ASL_MSG_INVALID_TARGET, Op,
                Op->Asl.ParseOpName);
        }
        else
        {
            AslError (ASL_ERROR, ASL_MSG_INVALID_CONSTANT_OP, Op,
                Op->Asl.ParseOpName);
        }

        return (AE_TYPE);
    }

    /* Debug output */

    DbgPrint (ASL_PARSE_OUTPUT, "TYPE_345");

    if (Op->Asl.CompileFlags & NODE_IS_TARGET)
    {
        DbgPrint (ASL_PARSE_OUTPUT, " TARGET");
    }
    if (Op->Asl.CompileFlags & NODE_IS_TERM_ARG)
    {
        DbgPrint (ASL_PARSE_OUTPUT, " TERMARG");
    }

    DbgPrint (ASL_PARSE_OUTPUT, "\n");
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    OpcAmlConstantWalk
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Reduce an Op and its subtree to a constant if possible
 *
 ******************************************************************************/

ACPI_STATUS
OpcAmlConstantWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_WALK_STATE         *WalkState;
    ACPI_STATUS             Status = AE_OK;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_PARSE_OBJECT       *RootOp;
    ACPI_PARSE_OBJECT       *OriginalParentOp;
    UINT8                   WalkType;


    /*
     * Only interested in subtrees that could possibly contain
     * expressions that can be evaluated at this time
     */
    if ((!(Op->Asl.CompileFlags & NODE_COMPILE_TIME_CONST)) ||
          (Op->Asl.CompileFlags & NODE_IS_TARGET))
    {
        return (AE_OK);
    }

    /* Set the walk type based on the reduction used for this op */

    if (Op->Asl.CompileFlags & NODE_IS_TERM_ARG)
    {
        /* Op is a TermArg, constant folding is merely optional */

        if (!Gbl_FoldConstants)
        {
            return (AE_CTRL_DEPTH);
        }

        WalkType = ACPI_WALK_CONST_OPTIONAL;
    }
    else
    {
        /* Op is a DataObject, the expression MUST reduced to a constant */

        WalkType = ACPI_WALK_CONST_REQUIRED;
    }

    /* Create a new walk state */

    WalkState = AcpiDsCreateWalkState (0, NULL, NULL, NULL);
    if (!WalkState)
    {
        return (AE_NO_MEMORY);
    }

    WalkState->NextOp = NULL;
    WalkState->Params = NULL;
    WalkState->WalkType = WalkType;
    WalkState->CallerReturnDesc = &ObjDesc;

    /*
     * Examine the entire subtree -- all nodes must be constants
     * or type 3/4/5 opcodes
     */
    Status = TrWalkParseTree (Op, ASL_WALK_VISIT_DOWNWARD,
        OpcAmlCheckForConstant, NULL, WalkState);

    /*
     * Did we find an entire subtree that contains all constants and type 3/4/5
     * opcodes?  (Only AE_OK or AE_TYPE returned from above)
     */
    if (Status == AE_TYPE)
    {
        /* Subtree cannot be reduced to a constant */

        if (WalkState->WalkType == ACPI_WALK_CONST_OPTIONAL)
        {
            AcpiDsDeleteWalkState (WalkState);
            return (AE_OK);
        }

        /* Don't descend any further, and use a default "constant" value */

        Status = AE_CTRL_DEPTH;
    }
    else
    {
        /* Subtree can be reduced */

        /* Allocate a new temporary root for this subtree */

        RootOp = TrAllocateNode (PARSEOP_INTEGER);
        if (!RootOp)
        {
            return (AE_NO_MEMORY);
        }

        RootOp->Common.AmlOpcode = AML_INT_EVAL_SUBTREE_OP;

        OriginalParentOp = Op->Common.Parent;
        Op->Common.Parent = RootOp;

        /* Hand off the subtree to the AML interpreter */

        Status = TrWalkParseTree (Op, ASL_WALK_VISIT_TWICE,
            OpcAmlEvaluationWalk1, OpcAmlEvaluationWalk2, WalkState);
        Op->Common.Parent = OriginalParentOp;

        /* TBD: we really *should* release the RootOp node */

        if (ACPI_SUCCESS (Status))
        {
            TotalFolds++;

            /* Get the final result */

            Status = AcpiDsResultPop (&ObjDesc, WalkState);
        }

        /* Check for error from the ACPICA core */

        if (ACPI_FAILURE (Status))
        {
            AslCoreSubsystemError (Op, Status,
                "Failure during constant evaluation", FALSE);
        }
    }

    if (ACPI_FAILURE (Status))
    {
        /* We could not resolve the subtree for some reason */

        AslError (ASL_ERROR, ASL_MSG_CONSTANT_EVALUATION, Op,
            Op->Asl.ParseOpName);

        /* Set the subtree value to ZERO anyway. Eliminates further errors */

        OpcUpdateIntegerNode (Op, 0);
    }
    else
    {
        AslError (ASL_OPTIMIZATION, ASL_MSG_CONSTANT_FOLDED, Op,
            Op->Asl.ParseOpName);

        /*
         * Because we know we executed type 3/4/5 opcodes above, we know that
         * the result must be either an Integer, String, or Buffer.
         */
        switch (ObjDesc->Common.Type)
        {
        case ACPI_TYPE_INTEGER:

            OpcUpdateIntegerNode (Op, ObjDesc->Integer.Value);

            DbgPrint (ASL_PARSE_OUTPUT,
                "Constant expression reduced to (%s) %8.8X%8.8X\n",
                Op->Asl.ParseOpName,
                ACPI_FORMAT_UINT64 (Op->Common.Value.Integer));
            break;

        case ACPI_TYPE_STRING:

            Op->Asl.ParseOpcode = PARSEOP_STRING_LITERAL;
            Op->Common.AmlOpcode = AML_STRING_OP;
            Op->Asl.AmlLength = ACPI_STRLEN (ObjDesc->String.Pointer) + 1;
            Op->Common.Value.String = ObjDesc->String.Pointer;

            DbgPrint (ASL_PARSE_OUTPUT,
                "Constant expression reduced to (STRING) %s\n",
                Op->Common.Value.String);

            break;

        case ACPI_TYPE_BUFFER:

            Op->Asl.ParseOpcode = PARSEOP_BUFFER;
            Op->Common.AmlOpcode = AML_BUFFER_OP;
            Op->Asl.CompileFlags = NODE_AML_PACKAGE;
            UtSetParseOpName (Op);

            /* Child node is the buffer length */

            RootOp = TrAllocateNode (PARSEOP_INTEGER);

            RootOp->Asl.AmlOpcode = AML_DWORD_OP;
            RootOp->Asl.Value.Integer = ObjDesc->Buffer.Length;
            RootOp->Asl.Parent = Op;

            (void) OpcSetOptimalIntegerSize (RootOp);

            Op->Asl.Child = RootOp;
            Op = RootOp;
            UtSetParseOpName (Op);

            /* Peer to the child is the raw buffer data */

            RootOp = TrAllocateNode (PARSEOP_RAW_DATA);
            RootOp->Asl.AmlOpcode = AML_RAW_DATA_BUFFER;
            RootOp->Asl.AmlLength = ObjDesc->Buffer.Length;
            RootOp->Asl.Value.String = (char *) ObjDesc->Buffer.Pointer;
            RootOp->Asl.Parent = Op->Asl.Parent;

            Op->Asl.Next = RootOp;
            Op = RootOp;

            DbgPrint (ASL_PARSE_OUTPUT,
                "Constant expression reduced to (BUFFER) length %X\n",
                ObjDesc->Buffer.Length);
            break;

        default:

            printf ("Unsupported return type: %s\n",
                AcpiUtGetObjectTypeName (ObjDesc));
            break;
        }
    }

    UtSetParseOpName (Op);
    Op->Asl.Child = NULL;

    AcpiDsDeleteWalkState (WalkState);
    return (AE_CTRL_DEPTH);
}


/*******************************************************************************
 *
 * FUNCTION:    OpcUpdateIntegerNode
 *
 * PARAMETERS:  Op                  - Current parse object
 *
 * RETURN:      None
 *
 * DESCRIPTION: Update node to the correct integer type.
 *
 ******************************************************************************/

static void
OpcUpdateIntegerNode (
    ACPI_PARSE_OBJECT       *Op,
    UINT64                  Value)
{

    Op->Common.Value.Integer = Value;

    /*
     * The AmlLength is used by the parser to indicate a constant,
     * (if non-zero). Length is either (1/2/4/8)
     */
    switch (Op->Asl.AmlLength)
    {
    case 1:

        TrUpdateNode (PARSEOP_BYTECONST, Op);
        Op->Asl.AmlOpcode = AML_RAW_DATA_BYTE;
        break;

    case 2:

        TrUpdateNode (PARSEOP_WORDCONST, Op);
        Op->Asl.AmlOpcode = AML_RAW_DATA_WORD;
        break;

    case 4:

        TrUpdateNode (PARSEOP_DWORDCONST, Op);
        Op->Asl.AmlOpcode = AML_RAW_DATA_DWORD;
        break;

    case 8:

        TrUpdateNode (PARSEOP_QWORDCONST, Op);
        Op->Asl.AmlOpcode = AML_RAW_DATA_QWORD;
        break;

    case 0:
    default:

        OpcSetOptimalIntegerSize (Op);
        TrUpdateNode (PARSEOP_INTEGER, Op);
        break;
    }

    Op->Asl.AmlLength = 0;
}
