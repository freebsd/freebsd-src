
/******************************************************************************
 *
 * Module Name: aslfold - Constant folding
 *              $Revision: 1.20 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2007, Intel Corp.
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


#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"
#include <contrib/dev/acpica/amlcode.h>

#include <contrib/dev/acpica/acdispat.h>
#include <contrib/dev/acpica/acparser.h>

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
             * reduced.  It can't.  No error
             */
            return (AE_TYPE);
        }

        /*
         * This is an expression that MUST reduce to a constant, and it
         * can't be reduced.  This is an error
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
        return AE_NO_MEMORY;
    }

    WalkState->NextOp               = NULL;
    WalkState->Params               = NULL;
    WalkState->CallerReturnDesc     = &ObjDesc;
    WalkState->WalkType             = WalkType;

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
    }

    if (ACPI_FAILURE (Status))
    {
        /* We could not resolve the subtree for some reason */

        AslCoreSubsystemError (Op, Status,
            "Failure during constant evaluation", FALSE);
        AslError (ASL_ERROR, ASL_MSG_CONSTANT_EVALUATION, Op,
            Op->Asl.ParseOpName);

        /* Set the subtree value to ZERO anyway.  Eliminates further errors */

        Op->Asl.ParseOpcode      = PARSEOP_INTEGER;
        Op->Common.Value.Integer = 0;
        OpcSetOptimalIntegerSize (Op);
    }
    else
    {
        AslError (ASL_OPTIMIZATION, ASL_MSG_CONSTANT_FOLDED, Op,
            Op->Asl.ParseOpName);

        /*
         * Because we know we executed type 3/4/5 opcodes above, we know that
         * the result must be either an Integer, String, or Buffer.
         */
        switch (ACPI_GET_OBJECT_TYPE (ObjDesc))
        {
        case ACPI_TYPE_INTEGER:

            Op->Asl.ParseOpcode      = PARSEOP_INTEGER;
            Op->Common.Value.Integer = ObjDesc->Integer.Value;
            OpcSetOptimalIntegerSize (Op);

            DbgPrint (ASL_PARSE_OUTPUT,
                "Constant expression reduced to (INTEGER) %8.8X%8.8X\n",
                ACPI_FORMAT_UINT64 (ObjDesc->Integer.Value));
            break;


        case ACPI_TYPE_STRING:

            Op->Asl.ParseOpcode     = PARSEOP_STRING_LITERAL;
            Op->Common.AmlOpcode    = AML_STRING_OP;
            Op->Asl.AmlLength       = ACPI_STRLEN (ObjDesc->String.Pointer) + 1;
            Op->Common.Value.String = ObjDesc->String.Pointer;

            DbgPrint (ASL_PARSE_OUTPUT,
                "Constant expression reduced to (STRING) %s\n",
                Op->Common.Value.String);

            break;


        case ACPI_TYPE_BUFFER:

            Op->Asl.ParseOpcode     = PARSEOP_BUFFER;
            Op->Common.AmlOpcode    = AML_BUFFER_OP;
            Op->Asl.CompileFlags    = NODE_AML_PACKAGE;
            UtSetParseOpName (Op);

            /* Child node is the buffer length */

            RootOp = TrAllocateNode (PARSEOP_INTEGER);

            RootOp->Asl.AmlOpcode     = AML_DWORD_OP;
            RootOp->Asl.Value.Integer = ObjDesc->Buffer.Length;
            RootOp->Asl.Parent        = Op;

            (void) OpcSetOptimalIntegerSize (RootOp);

            Op->Asl.Child = RootOp;
            Op = RootOp;
            UtSetParseOpName (Op);

            /* Peer to the child is the raw buffer data */

            RootOp = TrAllocateNode (PARSEOP_RAW_DATA);
            RootOp->Asl.AmlOpcode     = AML_RAW_DATA_BUFFER;
            RootOp->Asl.AmlLength     = ObjDesc->Buffer.Length;
            RootOp->Asl.Value.String  = (char *) ObjDesc->Buffer.Pointer;
            RootOp->Asl.Parent        = Op->Asl.Parent;

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

