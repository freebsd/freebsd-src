/******************************************************************************
 *
 * Module Name: aslfold - Constant folding
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2015, Intel Corp.
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

static ACPI_STATUS
TrTransformToStoreOp (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState);

static ACPI_STATUS
TrSimpleConstantReduction (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState);

static void
TrInstallReducedConstant (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_OPERAND_OBJECT     *ObjDesc);


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


    if (Op->Asl.CompileFlags == 0)
    {
        return (AE_OK);
    }

    /*
     * Only interested in subtrees that could possibly contain
     * expressions that can be evaluated at this time
     */
    if ((!(Op->Asl.CompileFlags & NODE_COMPILE_TIME_CONST)) ||
          (Op->Asl.CompileFlags & NODE_IS_TARGET))
    {
        return (AE_OK);
    }

    /* Create a new walk state */

    WalkState = AcpiDsCreateWalkState (0, NULL, NULL, NULL);
    if (!WalkState)
    {
        return (AE_NO_MEMORY);
    }

    WalkState->NextOp = NULL;
    WalkState->Params = NULL;

    /*
     * Examine the entire subtree -- all nodes must be constants
     * or type 3/4/5 opcodes
     */
    Status = TrWalkParseTree (Op, ASL_WALK_VISIT_DOWNWARD,
        OpcAmlCheckForConstant, NULL, WalkState);

    /*
     * Did we find an entire subtree that contains all constants
     * and type 3/4/5 opcodes?
     */
    switch (Status)
    {
    case AE_OK:

        /* Simple case, like Add(3,4) -> 7 */

        Status = TrSimpleConstantReduction (Op, WalkState);
        break;

    case AE_CTRL_RETURN_VALUE:

        /* More complex case, like Add(3,4,Local0) -> Store(7,Local0) */

        Status = TrTransformToStoreOp (Op, WalkState);
        break;

    case AE_TYPE:

        AcpiDsDeleteWalkState (WalkState);
        return (AE_OK);

    default:
        AcpiDsDeleteWalkState (WalkState);
        break;
    }

    if (ACPI_FAILURE (Status))
    {
        DbgPrint (ASL_PARSE_OUTPUT, "Cannot resolve, %s\n",
            AcpiFormatException (Status));

        /* We could not resolve the subtree for some reason */

        AslError (ASL_ERROR, ASL_MSG_CONSTANT_EVALUATION, Op,
            (char *) AcpiFormatException (Status));

        /* Set the subtree value to ZERO anyway. Eliminates further errors */

        OpcUpdateIntegerNode (Op, 0);
    }

    /* Abort the walk of this subtree, we are done with it */

    return (AE_CTRL_DEPTH);
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
    ACPI_STATUS             Status = AE_OK;


    WalkState->Op = Op;
    WalkState->Opcode = Op->Common.AmlOpcode;
    WalkState->OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);

    DbgPrint (ASL_PARSE_OUTPUT, "[%.4d] Opcode: %12.12s ",
        Op->Asl.LogicalLineNumber, Op->Asl.ParseOpName);

    /*
     * TBD: Ignore buffer constants for now. The problem is that these
     * constants have been transformed into RAW_DATA at this point, from
     * the parse tree transform process which currently happens before
     * the constant folding process. We may need to defer this transform
     * for buffer until after the constant folding.
     */
    if (WalkState->Opcode == AML_BUFFER_OP)
    {
        DbgPrint (ASL_PARSE_OUTPUT,
            "\nBuffer+Buffer->Buffer constant reduction is not supported yet");
        Status = AE_TYPE;
        goto CleanupAndExit;
    }

    /*
     * These opcodes do not appear in the OpcodeInfo table, but
     * they represent constants, so abort the constant walk now.
     */
    if ((WalkState->Opcode == AML_RAW_DATA_BYTE) ||
        (WalkState->Opcode == AML_RAW_DATA_WORD) ||
        (WalkState->Opcode == AML_RAW_DATA_DWORD) ||
        (WalkState->Opcode == AML_RAW_DATA_QWORD))
    {
        DbgPrint (ASL_PARSE_OUTPUT, "RAW DATA");
        Status = AE_TYPE;
        goto CleanupAndExit;
    }

    /* Type 3/4/5 opcodes have the AML_CONSTANT flag set */

    if (!(WalkState->OpInfo->Flags & AML_CONSTANT))
    {
        /* Not 3/4/5 opcode, but maybe can convert to STORE */

        if (Op->Asl.CompileFlags & NODE_IS_TARGET)
        {
            DbgPrint (ASL_PARSE_OUTPUT,
                "**** Valid Target, transform to Store ****\n");
            return (AE_CTRL_RETURN_VALUE);
        }

        /* Expression cannot be reduced */

        DbgPrint (ASL_PARSE_OUTPUT,
            "**** Not a Type 3/4/5 opcode (%s) ****",
             Op->Asl.ParseOpName);

        Status = AE_TYPE;
        goto CleanupAndExit;
    }

    /* Debug output */

    DbgPrint (ASL_PARSE_OUTPUT, "TYPE_345");

    if (Op->Asl.CompileFlags & NODE_IS_TARGET)
    {
        if (Op->Asl.ParseOpcode == PARSEOP_ZERO)
        {
            DbgPrint (ASL_PARSE_OUTPUT, "%-16s", " NULL TARGET");
        }
        else
        {
            DbgPrint (ASL_PARSE_OUTPUT, "%-16s", " VALID TARGET");
        }
    }
    if (Op->Asl.CompileFlags & NODE_IS_TERM_ARG)
    {
        DbgPrint (ASL_PARSE_OUTPUT, "%-16s", " TERMARG");
    }

CleanupAndExit:

    /* Dump the node compile flags also */

    TrPrintNodeCompileFlags (Op->Asl.CompileFlags);
    DbgPrint (ASL_PARSE_OUTPUT, "\n");
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    TrSimpleConstantReduction
 *
 * PARAMETERS:  Op                  - Parent operator to be transformed
 *              WalkState           - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Reduce an entire AML operation to a single constant. The
 *              operation must not have a target operand.
 *
 *              Add (32,64) --> 96
 *
 ******************************************************************************/

static ACPI_STATUS
TrSimpleConstantReduction (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_PARSE_OBJECT       *RootOp;
    ACPI_PARSE_OBJECT       *OriginalParentOp;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_STATUS             Status;


    DbgPrint (ASL_PARSE_OUTPUT,
        "Simple subtree constant reduction, operator to constant\n");

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

    WalkState->CallerReturnDesc = &ObjDesc;

    Status = TrWalkParseTree (Op, ASL_WALK_VISIT_TWICE,
        OpcAmlEvaluationWalk1, OpcAmlEvaluationWalk2, WalkState);

    /* Restore original parse tree */

    Op->Common.Parent = OriginalParentOp;

    if (ACPI_FAILURE (Status))
    {
        DbgPrint (ASL_PARSE_OUTPUT,
            "Constant Subtree evaluation(1), %s\n",
            AcpiFormatException (Status));
        return (Status);
    }

    /* Get the final result */

    Status = AcpiDsResultPop (&ObjDesc, WalkState);
    if (ACPI_FAILURE (Status))
    {
        DbgPrint (ASL_PARSE_OUTPUT,
            "Constant Subtree evaluation(2), %s\n",
            AcpiFormatException (Status));
        return (Status);
    }

    /* Disconnect any existing children, install new constant */

    Op->Asl.Child = NULL;
    TrInstallReducedConstant (Op, ObjDesc);

    UtSetParseOpName (Op);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    TrTransformToStoreOp
 *
 * PARAMETERS:  Op                  - Parent operator to be transformed
 *              WalkState           - Current walk state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Transforms a single AML operation with a constant and target
 *              to a simple store operation:
 *
 *              Add (32,64,DATA) --> Store (96,DATA)
 *
 ******************************************************************************/

static ACPI_STATUS
TrTransformToStoreOp (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_PARSE_OBJECT       *OriginalTarget;
    ACPI_PARSE_OBJECT       *NewTarget;
    ACPI_PARSE_OBJECT       *Child1;
    ACPI_PARSE_OBJECT       *Child2;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_PARSE_OBJECT       *NewParent;
    ACPI_PARSE_OBJECT       *OriginalParent;
    ACPI_STATUS             Status;


    DbgPrint (ASL_PARSE_OUTPUT,
        "Reduction/Transform to StoreOp: Store(Constant, Target)\n");

    /* Extract the operands */

    Child1 = Op->Asl.Child;
    Child2 = Child1->Asl.Next;

    /*
     * Special case for DIVIDE -- it has two targets. The first
     * is for the remainder and if present, we will not attempt
     * to reduce the expression.
     */
    if (Op->Asl.ParseOpcode == PARSEOP_DIVIDE)
    {
        Child2 = Child2->Asl.Next;
        if (Child2->Asl.ParseOpcode != PARSEOP_ZERO)
        {
            DbgPrint (ASL_PARSE_OUTPUT,
                "Cannot reduce DIVIDE - has two targets\n\n");
            return (AE_OK);
        }
    }

    /*
     * Create a NULL (zero) target so that we can use the
     * interpreter to evaluate the expression.
     */
    NewTarget = TrCreateNullTarget ();
    NewTarget->Common.AmlOpcode = AML_INT_NAMEPATH_OP;

    /* Handle one-operand cases (NOT, TOBCD, etc.) */

    if (!Child2->Asl.Next)
    {
        Child2 = Child1;
    }

    /* Link in new NULL target as the last operand */

    OriginalTarget = Child2->Asl.Next;
    Child2->Asl.Next = NewTarget;
    NewTarget->Asl.Parent = OriginalTarget->Asl.Parent;

    NewParent = TrAllocateNode (PARSEOP_INTEGER);
    NewParent->Common.AmlOpcode = AML_INT_EVAL_SUBTREE_OP;

    OriginalParent = Op->Common.Parent;
    Op->Common.Parent = NewParent;

    /* Hand off the subtree to the AML interpreter */

    WalkState->CallerReturnDesc = &ObjDesc;

    Status = TrWalkParseTree (Op, ASL_WALK_VISIT_TWICE,
        OpcAmlEvaluationWalk1, OpcAmlEvaluationWalk2, WalkState);
    if (ACPI_FAILURE (Status))
    {
        DbgPrint (ASL_PARSE_OUTPUT,
            "Constant Subtree evaluation(3), %s\n",
            AcpiFormatException (Status));
        goto EvalError;
    }

    /* Get the final result */

    Status = AcpiDsResultPop (&ObjDesc, WalkState);
    if (ACPI_FAILURE (Status))
    {
        DbgPrint (ASL_PARSE_OUTPUT,
            "Constant Subtree evaluation(4), %s\n",
            AcpiFormatException (Status));
        goto EvalError;
    }

    /* Truncate any subtree expressions, they have been evaluated */

    Child1->Asl.Child = NULL;

    /* Folded constant is in ObjDesc, store into Child1 */

    TrInstallReducedConstant (Child1, ObjDesc);

    /* Convert operator to STORE */

    Op->Asl.ParseOpcode = PARSEOP_STORE;
    Op->Asl.AmlOpcode = AML_STORE_OP;
    UtSetParseOpName (Op);
    Op->Common.Parent = OriginalParent;

    /* First child is the folded constant */

    /* Second child will be the target */

    Child1->Asl.Next = OriginalTarget;
    return (AE_OK);


EvalError:

    /* Restore original links */

    Op->Common.Parent = OriginalParent;
    Child2->Asl.Next = OriginalTarget;
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    TrInstallReducedConstant
 *
 * PARAMETERS:  Op                  - Parent operator to be transformed
 *              ObjDesc             - Reduced constant to be installed
 *
 * RETURN:      None
 *
 * DESCRIPTION: Transform the original operator to a simple constant.
 *              Handles Integers, Strings, and Buffers.
 *
 ******************************************************************************/

static void
TrInstallReducedConstant (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_OPERAND_OBJECT     *ObjDesc)
{
    ACPI_PARSE_OBJECT       *LengthOp;
    ACPI_PARSE_OBJECT       *DataOp;


    TotalFolds++;
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
            "Constant expression reduced to (%s) %8.8X%8.8X\n\n",
            Op->Asl.ParseOpName,
            ACPI_FORMAT_UINT64 (Op->Common.Value.Integer));
        break;

    case ACPI_TYPE_STRING:

        Op->Asl.ParseOpcode = PARSEOP_STRING_LITERAL;
        Op->Common.AmlOpcode = AML_STRING_OP;
        Op->Asl.AmlLength = strlen (ObjDesc->String.Pointer) + 1;
        Op->Common.Value.String = ObjDesc->String.Pointer;

        DbgPrint (ASL_PARSE_OUTPUT,
            "Constant expression reduced to (STRING) %s\n\n",
            Op->Common.Value.String);
        break;

    case ACPI_TYPE_BUFFER:
        /*
         * Create a new parse subtree of the form:
         *
         * BUFFER (Buffer AML opcode)
         *    INTEGER (Buffer length in bytes)
         *    RAW_DATA (Buffer byte data)
         */
        Op->Asl.ParseOpcode = PARSEOP_BUFFER;
        Op->Common.AmlOpcode = AML_BUFFER_OP;
        Op->Asl.CompileFlags = NODE_AML_PACKAGE;
        UtSetParseOpName (Op);

        /* Child node is the buffer length */

        LengthOp = TrAllocateNode (PARSEOP_INTEGER);

        LengthOp->Asl.AmlOpcode = AML_DWORD_OP;
        LengthOp->Asl.Value.Integer = ObjDesc->Buffer.Length;
        LengthOp->Asl.Parent = Op;
        (void) OpcSetOptimalIntegerSize (LengthOp);

        Op->Asl.Child = LengthOp;

        /* Next child is the raw buffer data */

        DataOp = TrAllocateNode (PARSEOP_RAW_DATA);
        DataOp->Asl.AmlOpcode = AML_RAW_DATA_BUFFER;
        DataOp->Asl.AmlLength = ObjDesc->Buffer.Length;
        DataOp->Asl.Value.String = (char *) ObjDesc->Buffer.Pointer;
        DataOp->Asl.Parent = Op;

        LengthOp->Asl.Next = DataOp;

        DbgPrint (ASL_PARSE_OUTPUT,
            "Constant expression reduced to (BUFFER) length %X\n\n",
            ObjDesc->Buffer.Length);
        break;

    default:
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    OpcUpdateIntegerNode
 *
 * PARAMETERS:  Op                  - Current parse object
 *              Value               - Value for the integer op
 *
 * RETURN:      None
 *
 * DESCRIPTION: Update node to the correct Integer type and value
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
        DbgPrint (ASL_PARSE_OUTPUT,
            "%s Constant interpretation failed (1) - %s\n",
            Op->Asl.ParseOpName, AcpiFormatException (Status));
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
        DbgPrint (ASL_PARSE_OUTPUT,
            "%s: Constant interpretation failed (2) - %s\n",
            Op->Asl.ParseOpName, AcpiFormatException (Status));
    }

    return (Status);
}
