/******************************************************************************
 *
 * Module Name: psparse - Parser top level AML parse routines
 *              $Revision: 146 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2004, Intel Corp.
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


/*
 * Parse the AML and build an operation tree as most interpreters,
 * like Perl, do.  Parsing is done by hand rather than with a YACC
 * generated parser to tightly constrain stack and dynamic memory
 * usage.  At the same time, parsing is kept flexible and the code
 * fairly compact by parsing based on a list of AML opcode
 * templates in AmlOpInfo[]
 */

#include "acpi.h"
#include "acparser.h"
#include "acdispat.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_PARSER
        ACPI_MODULE_NAME    ("psparse")


static UINT32               AcpiGbl_Depth = 0;


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetOpcodeSize
 *
 * PARAMETERS:  Opcode          - An AML opcode
 *
 * RETURN:      Size of the opcode, in bytes (1 or 2)
 *
 * DESCRIPTION: Get the size of the current opcode.
 *
 ******************************************************************************/

UINT32
AcpiPsGetOpcodeSize (
    UINT32                  Opcode)
{

    /* Extended (2-byte) opcode if > 255 */

    if (Opcode > 0x00FF)
    {
        return (2);
    }

    /* Otherwise, just a single byte opcode */

    return (1);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsPeekOpcode
 *
 * PARAMETERS:  ParserState         - A parser state object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get next AML opcode (without incrementing AML pointer)
 *
 ******************************************************************************/

UINT16
AcpiPsPeekOpcode (
    ACPI_PARSE_STATE        *ParserState)
{
    UINT8                   *Aml;
    UINT16                  Opcode;


    Aml = ParserState->Aml;
    Opcode = (UINT16) ACPI_GET8 (Aml);


    if (Opcode == AML_EXTOP)
    {
        /* Extended opcode */

        Aml++;
        Opcode = (UINT16) ((Opcode << 8) | ACPI_GET8 (Aml));
    }

    return (Opcode);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsCompleteThisOp
 *
 * PARAMETERS:  WalkState       - Current State
 *              Op              - Op to complete
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Perform any cleanup at the completion of an Op.
 *
 ******************************************************************************/

void
AcpiPsCompleteThisOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *Prev;
    ACPI_PARSE_OBJECT       *Next;
    const ACPI_OPCODE_INFO  *ParentInfo;
    ACPI_PARSE_OBJECT       *ReplacementOp = NULL;


    ACPI_FUNCTION_TRACE_PTR ("PsCompleteThisOp", Op);


    /* Check for null Op, can happen if AML code is corrupt */

    if (!Op)
    {
        return_VOID;
    }

    /* Delete this op and the subtree below it if asked to */

    if (((WalkState->ParseFlags & ACPI_PARSE_TREE_MASK) != ACPI_PARSE_DELETE_TREE) ||
         (WalkState->OpInfo->Class == AML_CLASS_ARGUMENT))
    {
        return_VOID;
    }

    /* Make sure that we only delete this subtree */

    if (Op->Common.Parent)
    {
        /*
         * Check if we need to replace the operator and its subtree
         * with a return value op (placeholder op)
         */
        ParentInfo  = AcpiPsGetOpcodeInfo (Op->Common.Parent->Common.AmlOpcode);

        switch (ParentInfo->Class)
        {
        case AML_CLASS_CONTROL:
            break;

        case AML_CLASS_CREATE:

            /*
             * These opcodes contain TermArg operands.  The current
             * op must be replaced by a placeholder return op
             */
            ReplacementOp = AcpiPsAllocOp (AML_INT_RETURN_VALUE_OP);
            if (!ReplacementOp)
            {
                goto Cleanup;
            }
            break;

        case AML_CLASS_NAMED_OBJECT:

            /*
             * These opcodes contain TermArg operands.  The current
             * op must be replaced by a placeholder return op
             */
            if ((Op->Common.Parent->Common.AmlOpcode == AML_REGION_OP)       ||
                (Op->Common.Parent->Common.AmlOpcode == AML_DATA_REGION_OP)  ||
                (Op->Common.Parent->Common.AmlOpcode == AML_BUFFER_OP)       ||
                (Op->Common.Parent->Common.AmlOpcode == AML_PACKAGE_OP)      ||
                (Op->Common.Parent->Common.AmlOpcode == AML_VAR_PACKAGE_OP))
            {
                ReplacementOp = AcpiPsAllocOp (AML_INT_RETURN_VALUE_OP);
                if (!ReplacementOp)
                {
                    goto Cleanup;
                }
            }

            if ((Op->Common.Parent->Common.AmlOpcode == AML_NAME_OP) &&
                (WalkState->DescendingCallback != AcpiDsExecBeginOp))

            {
                if ((Op->Common.AmlOpcode == AML_BUFFER_OP) ||
                    (Op->Common.AmlOpcode == AML_PACKAGE_OP) ||
                    (Op->Common.AmlOpcode == AML_VAR_PACKAGE_OP))
                {
                    ReplacementOp = AcpiPsAllocOp (Op->Common.AmlOpcode);
                    if (!ReplacementOp)
                    {
                        goto Cleanup;
                    }

                    ReplacementOp->Named.Data = Op->Named.Data;
                    ReplacementOp->Named.Length = Op->Named.Length;
                }
            }
            break;

        default:
            ReplacementOp = AcpiPsAllocOp (AML_INT_RETURN_VALUE_OP);
            if (!ReplacementOp)
            {
                goto Cleanup;
            }
        }

        /* We must unlink this op from the parent tree */

        Prev = Op->Common.Parent->Common.Value.Arg;
        if (Prev == Op)
        {
            /* This op is the first in the list */

            if (ReplacementOp)
            {
                ReplacementOp->Common.Parent        = Op->Common.Parent;
                ReplacementOp->Common.Value.Arg     = NULL;
                ReplacementOp->Common.Node          = Op->Common.Node;
                Op->Common.Parent->Common.Value.Arg = ReplacementOp;
                ReplacementOp->Common.Next          = Op->Common.Next;
            }
            else
            {
                Op->Common.Parent->Common.Value.Arg = Op->Common.Next;
            }
        }

        /* Search the parent list */

        else while (Prev)
        {
            /* Traverse all siblings in the parent's argument list */

            Next = Prev->Common.Next;
            if (Next == Op)
            {
                if (ReplacementOp)
                {
                    ReplacementOp->Common.Parent    = Op->Common.Parent;
                    ReplacementOp->Common.Value.Arg = NULL;
                    ReplacementOp->Common.Node      = Op->Common.Node;
                    Prev->Common.Next               = ReplacementOp;
                    ReplacementOp->Common.Next      = Op->Common.Next;
                    Next = NULL;
                }
                else
                {
                    Prev->Common.Next = Op->Common.Next;
                    Next = NULL;
                }
            }

            Prev = Next;
        }
    }


Cleanup:

    /* Now we can actually delete the subtree rooted at op */

    AcpiPsDeleteParseTree (Op);
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsNextParseState
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Update the parser state based upon the return exception from
 *              the parser callback.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiPsNextParseState (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_STATUS             CallbackStatus)
{
    ACPI_PARSE_STATE        *ParserState = &WalkState->ParserState;
    ACPI_STATUS             Status = AE_CTRL_PENDING;


    ACPI_FUNCTION_TRACE_PTR ("PsNextParseState", Op);


    switch (CallbackStatus)
    {
    case AE_CTRL_TERMINATE:

        /*
         * A control method was terminated via a RETURN statement.
         * The walk of this method is complete.
         */
        ParserState->Aml = ParserState->AmlEnd;
        Status = AE_CTRL_TERMINATE;
        break;


    case AE_CTRL_BREAK:

        ParserState->Aml = WalkState->AmlLastWhile;
        WalkState->ControlState->Common.Value = FALSE;
        Status = AE_CTRL_BREAK;
        break;

    case AE_CTRL_CONTINUE:


        ParserState->Aml = WalkState->AmlLastWhile;
        Status = AE_CTRL_CONTINUE;
        break;

    case AE_CTRL_PENDING:

        ParserState->Aml = WalkState->AmlLastWhile;
        break;

#if 0
    case AE_CTRL_SKIP:

        ParserState->Aml = ParserState->Scope->ParseScope.PkgEnd;
        Status = AE_OK;
        break;
#endif

    case AE_CTRL_TRUE:

        /*
         * Predicate of an IF was true, and we are at the matching ELSE.
         * Just close out this package
         */
        ParserState->Aml = AcpiPsGetNextPackageEnd (ParserState);
        break;


    case AE_CTRL_FALSE:

        /*
         * Either an IF/WHILE Predicate was false or we encountered a BREAK
         * opcode.  In both cases, we do not execute the rest of the
         * package;  We simply close out the parent (finishing the walk of
         * this branch of the tree) and continue execution at the parent
         * level.
         */
        ParserState->Aml = ParserState->Scope->ParseScope.PkgEnd;

        /* In the case of a BREAK, just force a predicate (if any) to FALSE */

        WalkState->ControlState->Common.Value = FALSE;
        Status = AE_CTRL_END;
        break;


    case AE_CTRL_TRANSFER:

        /*
         * A method call (invocation) -- transfer control
         */
        Status = AE_CTRL_TRANSFER;
        WalkState->PrevOp = Op;
        WalkState->MethodCallOp = Op;
        WalkState->MethodCallNode = (Op->Common.Value.Arg)->Common.Node;

        /* Will return value (if any) be used by the caller? */

        WalkState->ReturnUsed = AcpiDsIsResultUsed (Op, WalkState);
        break;


    default:
        Status = CallbackStatus;
        if ((CallbackStatus & AE_CODE_MASK) == AE_CODE_CONTROL)
        {
            Status = AE_OK;
        }
        break;
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsParseLoop
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Parse AML (pointed to by the current parser state) and return
 *              a tree of ops.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiPsParseLoop (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_PARSE_OBJECT       *Op = NULL;     /* current op */
    ACPI_PARSE_OBJECT       *Arg = NULL;
    ACPI_PARSE_OBJECT       *PreOp = NULL;
    ACPI_PARSE_STATE        *ParserState;
    UINT8                   *AmlOpStart = NULL;


    ACPI_FUNCTION_TRACE_PTR ("PsParseLoop", WalkState);

    if (WalkState->DescendingCallback == NULL)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    ParserState = &WalkState->ParserState;
    WalkState->ArgTypes = 0;

#if (!defined (ACPI_NO_METHOD_EXECUTION) && !defined (ACPI_CONSTANT_EVAL_ONLY))
    if (WalkState->WalkType & ACPI_WALK_METHOD_RESTART)
    {
        /* We are restarting a preempted control method */

        if (AcpiPsHasCompletedScope (ParserState))
        {
            /*
             * We must check if a predicate to an IF or WHILE statement
             * was just completed
             */
            if ((ParserState->Scope->ParseScope.Op) &&
               ((ParserState->Scope->ParseScope.Op->Common.AmlOpcode == AML_IF_OP) ||
                (ParserState->Scope->ParseScope.Op->Common.AmlOpcode == AML_WHILE_OP)) &&
                (WalkState->ControlState) &&
                (WalkState->ControlState->Common.State ==
                    ACPI_CONTROL_PREDICATE_EXECUTING))
            {
                /*
                 * A predicate was just completed, get the value of the
                 * predicate and branch based on that value
                 */
                WalkState->Op = NULL;
                Status = AcpiDsGetPredicateValue (WalkState, ACPI_TO_POINTER (TRUE));
                if (ACPI_FAILURE (Status) &&
                    ((Status & AE_CODE_MASK) != AE_CODE_CONTROL))
                {
                    if (Status == AE_AML_NO_RETURN_VALUE)
                    {
                        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                            "Invoked method did not return a value, %s\n",
                            AcpiFormatException (Status)));

                    }
                    ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "GetPredicate Failed, %s\n",
                        AcpiFormatException (Status)));
                    return_ACPI_STATUS (Status);
                }

                Status = AcpiPsNextParseState (WalkState, Op, Status);
            }

            AcpiPsPopScope (ParserState, &Op,
                &WalkState->ArgTypes, &WalkState->ArgCount);
            ACPI_DEBUG_PRINT ((ACPI_DB_PARSE, "Popped scope, Op=%p\n", Op));
        }
        else if (WalkState->PrevOp)
        {
            /* We were in the middle of an op */

            Op = WalkState->PrevOp;
            WalkState->ArgTypes = WalkState->PrevArgTypes;
        }
    }
#endif

    /*
     * Iterative parsing loop, while there is more aml to process:
     */
    while ((ParserState->Aml < ParserState->AmlEnd) || (Op))
    {
        AmlOpStart = ParserState->Aml;
        if (!Op)
        {
            /* Get the next opcode from the AML stream */

            WalkState->AmlOffset = (UINT32) ACPI_PTR_DIFF (ParserState->Aml,
                                                           ParserState->AmlStart);
            WalkState->Opcode    = AcpiPsPeekOpcode (ParserState);

            /*
             * First cut to determine what we have found:
             * 1) A valid AML opcode
             * 2) A name string
             * 3) An unknown/invalid opcode
             */
            WalkState->OpInfo = AcpiPsGetOpcodeInfo (WalkState->Opcode);
            switch (WalkState->OpInfo->Class)
            {
            case AML_CLASS_ASCII:
            case AML_CLASS_PREFIX:
                /*
                 * Starts with a valid prefix or ASCII char, this is a name
                 * string.  Convert the bare name string to a namepath.
                 */
                WalkState->Opcode = AML_INT_NAMEPATH_OP;
                WalkState->ArgTypes = ARGP_NAMESTRING;
                break;

            case AML_CLASS_UNKNOWN:

                /* The opcode is unrecognized.  Just skip unknown opcodes */

                ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                    "Found unknown opcode %X at AML address %p offset %X, ignoring\n",
                    WalkState->Opcode, ParserState->Aml, WalkState->AmlOffset));

                ACPI_DUMP_BUFFER (ParserState->Aml, 128);

                /* Assume one-byte bad opcode */

                ParserState->Aml++;
                continue;

            default:

                /* Found opcode info, this is a normal opcode */

                ParserState->Aml += AcpiPsGetOpcodeSize (WalkState->Opcode);
                WalkState->ArgTypes = WalkState->OpInfo->ParseArgs;
                break;
            }

            /* Create Op structure and append to parent's argument list */

            if (WalkState->OpInfo->Flags & AML_NAMED)
            {
                /* Allocate a new PreOp if necessary */

                if (!PreOp)
                {
                    PreOp = AcpiPsAllocOp (WalkState->Opcode);
                    if (!PreOp)
                    {
                        Status = AE_NO_MEMORY;
                        goto CloseThisOp;
                    }
                }

                PreOp->Common.Value.Arg = NULL;
                PreOp->Common.AmlOpcode = WalkState->Opcode;

                /*
                 * Get and append arguments until we find the node that contains
                 * the name (the type ARGP_NAME).
                 */
                while (GET_CURRENT_ARG_TYPE (WalkState->ArgTypes) &&
                      (GET_CURRENT_ARG_TYPE (WalkState->ArgTypes) != ARGP_NAME))
                {
                    Status = AcpiPsGetNextArg (WalkState, ParserState,
                                GET_CURRENT_ARG_TYPE (WalkState->ArgTypes), &Arg);
                    if (ACPI_FAILURE (Status))
                    {
                        goto CloseThisOp;
                    }

                    AcpiPsAppendArg (PreOp, Arg);
                    INCREMENT_ARG_LIST (WalkState->ArgTypes);
                }

                /* Make sure that we found a NAME and didn't run out of arguments */

                if (!GET_CURRENT_ARG_TYPE (WalkState->ArgTypes))
                {
                    Status = AE_AML_NO_OPERAND;
                    goto CloseThisOp;
                }

                /* We know that this arg is a name, move to next arg */

                INCREMENT_ARG_LIST (WalkState->ArgTypes);

                /*
                 * Find the object.  This will either insert the object into
                 * the namespace or simply look it up
                 */
                WalkState->Op = NULL;

                Status = WalkState->DescendingCallback (WalkState, &Op);
                if (ACPI_FAILURE (Status))
                {
                    ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "During name lookup/catalog, %s\n",
                            AcpiFormatException (Status)));
                    goto CloseThisOp;
                }

                if (Op == NULL)
                {
                    continue;
                }

                Status = AcpiPsNextParseState (WalkState, Op, Status);
                if (Status == AE_CTRL_PENDING)
                {
                    Status = AE_OK;
                    goto CloseThisOp;
                }

                if (ACPI_FAILURE (Status))
                {
                    goto CloseThisOp;
                }

                AcpiPsAppendArg (Op, PreOp->Common.Value.Arg);
                AcpiGbl_Depth++;

                if (Op->Common.AmlOpcode == AML_REGION_OP)
                {
                    /*
                     * Defer final parsing of an OperationRegion body,
                     * because we don't have enough info in the first pass
                     * to parse it correctly (i.e., there may be method
                     * calls within the TermArg elements of the body.)
                     *
                     * However, we must continue parsing because
                     * the opregion is not a standalone package --
                     * we don't know where the end is at this point.
                     *
                     * (Length is unknown until parse of the body complete)
                     */
                    Op->Named.Data    = AmlOpStart;
                    Op->Named.Length  = 0;
                }
            }
            else
            {
                /* Not a named opcode, just allocate Op and append to parent */

                WalkState->OpInfo = AcpiPsGetOpcodeInfo (WalkState->Opcode);
                Op = AcpiPsAllocOp (WalkState->Opcode);
                if (!Op)
                {
                    Status = AE_NO_MEMORY;
                    goto CloseThisOp;
                }

                if (WalkState->OpInfo->Flags & AML_CREATE)
                {
                    /*
                     * Backup to beginning of CreateXXXfield declaration
                     * BodyLength is unknown until we parse the body
                     */
                    Op->Named.Data    = AmlOpStart;
                    Op->Named.Length  = 0;
                }

                AcpiPsAppendArg (AcpiPsGetParentScope (ParserState), Op);

                if ((WalkState->DescendingCallback != NULL))
                {
                    /*
                     * Find the object.  This will either insert the object into
                     * the namespace or simply look it up
                     */
                    WalkState->Op = Op;

                    Status = WalkState->DescendingCallback (WalkState, &Op);
                    Status = AcpiPsNextParseState (WalkState, Op, Status);
                    if (Status == AE_CTRL_PENDING)
                    {
                        Status = AE_OK;
                        goto CloseThisOp;
                    }

                    if (ACPI_FAILURE (Status))
                    {
                        goto CloseThisOp;
                    }
                }
            }

            Op->Common.AmlOffset = WalkState->AmlOffset;

            if (WalkState->OpInfo)
            {
                ACPI_DEBUG_PRINT ((ACPI_DB_PARSE,
                    "Opcode %4.4X [%s] Op %p Aml %p AmlOffset %5.5X\n",
                     (UINT32) Op->Common.AmlOpcode, WalkState->OpInfo->Name,
                     Op, ParserState->Aml, Op->Common.AmlOffset));
            }
        }


        /* Start ArgCount at zero because we don't know if there are any args yet */

        WalkState->ArgCount  = 0;

        if (WalkState->ArgTypes)   /* Are there any arguments that must be processed? */
        {
            /* Get arguments */

            switch (Op->Common.AmlOpcode)
            {
            case AML_BYTE_OP:       /* AML_BYTEDATA_ARG */
            case AML_WORD_OP:       /* AML_WORDDATA_ARG */
            case AML_DWORD_OP:      /* AML_DWORDATA_ARG */
            case AML_QWORD_OP:      /* AML_QWORDATA_ARG */
            case AML_STRING_OP:     /* AML_ASCIICHARLIST_ARG */

                /* Fill in constant or string argument directly */

                AcpiPsGetNextSimpleArg (ParserState,
                    GET_CURRENT_ARG_TYPE (WalkState->ArgTypes), Op);
                break;

            case AML_INT_NAMEPATH_OP:   /* AML_NAMESTRING_ARG */

                Status = AcpiPsGetNextNamepath (WalkState, ParserState, Op, 1);
                if (ACPI_FAILURE (Status))
                {
                    goto CloseThisOp;
                }

                WalkState->ArgTypes = 0;
                break;

            default:

                /* Op is not a constant or string, append each argument to the Op */

                while (GET_CURRENT_ARG_TYPE (WalkState->ArgTypes) &&
                        !WalkState->ArgCount)
                {
                    WalkState->AmlOffset = (UINT32) ACPI_PTR_DIFF (ParserState->Aml,
                                                                   ParserState->AmlStart);
                    Status = AcpiPsGetNextArg (WalkState, ParserState,
                                GET_CURRENT_ARG_TYPE (WalkState->ArgTypes), &Arg);
                    if (ACPI_FAILURE (Status))
                    {
                        goto CloseThisOp;
                    }

                    if (Arg)
                    {
                        Arg->Common.AmlOffset = WalkState->AmlOffset;
                        AcpiPsAppendArg (Op, Arg);
                    }
                    INCREMENT_ARG_LIST (WalkState->ArgTypes);
                }

                /* Special processing for certain opcodes */

                switch (Op->Common.AmlOpcode)
                {
                case AML_METHOD_OP:

                    /*
                     * Skip parsing of control method
                     * because we don't have enough info in the first pass
                     * to parse it correctly.
                     *
                     * Save the length and address of the body
                     */
                    Op->Named.Data   = ParserState->Aml;
                    Op->Named.Length = (UINT32) (ParserState->PkgEnd - ParserState->Aml);

                    /* Skip body of method */

                    ParserState->Aml    = ParserState->PkgEnd;
                    WalkState->ArgCount = 0;
                    break;

                case AML_BUFFER_OP:
                case AML_PACKAGE_OP:
                case AML_VAR_PACKAGE_OP:

                    if ((Op->Common.Parent) &&
                        (Op->Common.Parent->Common.AmlOpcode == AML_NAME_OP) &&
                        (WalkState->DescendingCallback != AcpiDsExecBeginOp))
                    {
                        /*
                         * Skip parsing of Buffers and Packages
                         * because we don't have enough info in the first pass
                         * to parse them correctly.
                         */
                        Op->Named.Data   = AmlOpStart;
                        Op->Named.Length = (UINT32) (ParserState->PkgEnd - AmlOpStart);

                        /* Skip body */

                        ParserState->Aml    = ParserState->PkgEnd;
                        WalkState->ArgCount = 0;
                    }
                    break;

                case AML_WHILE_OP:

                    if (WalkState->ControlState)
                    {
                        WalkState->ControlState->Control.PackageEnd = ParserState->PkgEnd;
                    }
                    break;

                default:

                    /* No action for all other opcodes */
                    break;
                }
                break;
            }
        }

        /* Check for arguments that need to be processed */

        if (WalkState->ArgCount)
        {
            /* There are arguments (complex ones), push Op and prepare for argument */

            Status = AcpiPsPushScope (ParserState, Op,
                        WalkState->ArgTypes, WalkState->ArgCount);
            if (ACPI_FAILURE (Status))
            {
                goto CloseThisOp;
            }
            Op = NULL;
            continue;
        }

        /* All arguments have been processed -- Op is complete, prepare for next */

        WalkState->OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
        if (WalkState->OpInfo->Flags & AML_NAMED)
        {
            if (AcpiGbl_Depth)
            {
                AcpiGbl_Depth--;
            }

            if (Op->Common.AmlOpcode == AML_REGION_OP)
            {
                /*
                 * Skip parsing of control method or opregion body,
                 * because we don't have enough info in the first pass
                 * to parse them correctly.
                 *
                 * Completed parsing an OpRegion declaration, we now
                 * know the length.
                 */
                Op->Named.Length = (UINT32) (ParserState->Aml - Op->Named.Data);
            }
        }

        if (WalkState->OpInfo->Flags & AML_CREATE)
        {
            /*
             * Backup to beginning of CreateXXXfield declaration (1 for
             * Opcode)
             *
             * BodyLength is unknown until we parse the body
             */
            Op->Named.Length = (UINT32) (ParserState->Aml - Op->Named.Data);
        }

        /* This op complete, notify the dispatcher */

        if (WalkState->AscendingCallback != NULL)
        {
            WalkState->Op     = Op;
            WalkState->Opcode = Op->Common.AmlOpcode;

            Status = WalkState->AscendingCallback (WalkState);
            Status = AcpiPsNextParseState (WalkState, Op, Status);
            if (Status == AE_CTRL_PENDING)
            {
                Status = AE_OK;
                goto CloseThisOp;
            }
        }


CloseThisOp:
        /*
         * Finished one argument of the containing scope
         */
        ParserState->Scope->ParseScope.ArgCount--;

        /* Close this Op (will result in parse subtree deletion) */

        AcpiPsCompleteThisOp (WalkState, Op);
        Op = NULL;
        if (PreOp)
        {
            AcpiPsFreeOp (PreOp);
            PreOp = NULL;
        }

        switch (Status)
        {
        case AE_OK:
            break;


        case AE_CTRL_TRANSFER:

            /*
             * We are about to transfer to a called method.
             */
            WalkState->PrevOp = Op;
            WalkState->PrevArgTypes = WalkState->ArgTypes;
            return_ACPI_STATUS (Status);


        case AE_CTRL_END:

            AcpiPsPopScope (ParserState, &Op,
                &WalkState->ArgTypes, &WalkState->ArgCount);

            if (Op)
            {
                WalkState->Op     = Op;
                WalkState->OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
                WalkState->Opcode = Op->Common.AmlOpcode;

                Status = WalkState->AscendingCallback (WalkState);
                Status = AcpiPsNextParseState (WalkState, Op, Status);

                AcpiPsCompleteThisOp (WalkState, Op);
                Op = NULL;
            }
            Status = AE_OK;
            break;


        case AE_CTRL_BREAK:
        case AE_CTRL_CONTINUE:

            /* Pop off scopes until we find the While */

            while (!Op || (Op->Common.AmlOpcode != AML_WHILE_OP))
            {
                AcpiPsPopScope (ParserState, &Op,
                    &WalkState->ArgTypes, &WalkState->ArgCount);
            }

            /* Close this iteration of the While loop */

            WalkState->Op     = Op;
            WalkState->OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
            WalkState->Opcode = Op->Common.AmlOpcode;

            Status = WalkState->AscendingCallback (WalkState);
            Status = AcpiPsNextParseState (WalkState, Op, Status);

            AcpiPsCompleteThisOp (WalkState, Op);
            Op = NULL;

            Status = AE_OK;
            break;


        case AE_CTRL_TERMINATE:

            Status = AE_OK;

            /* Clean up */
            do
            {
                if (Op)
                {
                    AcpiPsCompleteThisOp (WalkState, Op);
                }
                AcpiPsPopScope (ParserState, &Op,
                    &WalkState->ArgTypes, &WalkState->ArgCount);

            } while (Op);

            return_ACPI_STATUS (Status);


        default:  /* All other non-AE_OK status */

            do
            {
                if (Op)
                {
                    AcpiPsCompleteThisOp (WalkState, Op);
                }
                AcpiPsPopScope (ParserState, &Op,
                    &WalkState->ArgTypes, &WalkState->ArgCount);

            } while (Op);


            /*
             * TBD: Cleanup parse ops on error
             */
#if 0
            if (Op == NULL)
            {
                AcpiPsPopScope (ParserState, &Op,
                    &WalkState->ArgTypes, &WalkState->ArgCount);
            }
#endif
            WalkState->PrevOp = Op;
            WalkState->PrevArgTypes = WalkState->ArgTypes;
            return_ACPI_STATUS (Status);
        }

        /* This scope complete? */

        if (AcpiPsHasCompletedScope (ParserState))
        {
            AcpiPsPopScope (ParserState, &Op,
                &WalkState->ArgTypes, &WalkState->ArgCount);
            ACPI_DEBUG_PRINT ((ACPI_DB_PARSE, "Popped scope, Op=%p\n", Op));
        }
        else
        {
            Op = NULL;
        }

    } /* while ParserState->Aml */


    /*
     * Complete the last Op (if not completed), and clear the scope stack.
     * It is easily possible to end an AML "package" with an unbounded number
     * of open scopes (such as when several ASL blocks are closed with
     * sequential closing braces).  We want to terminate each one cleanly.
     */
    ACPI_DEBUG_PRINT ((ACPI_DB_PARSE, "AML package complete at Op %p\n", Op));
    do
    {
        if (Op)
        {
            if (WalkState->AscendingCallback != NULL)
            {
                WalkState->Op     = Op;
                WalkState->OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
                WalkState->Opcode = Op->Common.AmlOpcode;

                Status = WalkState->AscendingCallback (WalkState);
                Status = AcpiPsNextParseState (WalkState, Op, Status);
                if (Status == AE_CTRL_PENDING)
                {
                    Status = AE_OK;
                    goto CloseThisOp;
                }

                if (Status == AE_CTRL_TERMINATE)
                {
                    Status = AE_OK;

                    /* Clean up */
                    do
                    {
                        if (Op)
                        {
                            AcpiPsCompleteThisOp (WalkState, Op);
                        }

                        AcpiPsPopScope (ParserState, &Op,
                            &WalkState->ArgTypes, &WalkState->ArgCount);

                    } while (Op);

                    return_ACPI_STATUS (Status);
                }

                else if (ACPI_FAILURE (Status))
                {
                    AcpiPsCompleteThisOp (WalkState, Op);
                    return_ACPI_STATUS (Status);
                }
            }

            AcpiPsCompleteThisOp (WalkState, Op);
        }

        AcpiPsPopScope (ParserState, &Op, &WalkState->ArgTypes,
            &WalkState->ArgCount);

    } while (Op);

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsParseAml
 *
 * PARAMETERS:  StartScope      - The starting point of the parse.  Becomes the
 *                                root of the parsed op tree.
 *              Aml             - Pointer to the raw AML code to parse
 *              AmlSize         - Length of the AML to parse
 *
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Parse raw AML and return a tree of ops
 *
 ******************************************************************************/

ACPI_STATUS
AcpiPsParseAml (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status;
    ACPI_STATUS             TerminateStatus;
    ACPI_THREAD_STATE       *Thread;
    ACPI_THREAD_STATE       *PrevWalkList = AcpiGbl_CurrentWalkList;
    ACPI_WALK_STATE         *PreviousWalkState;


    ACPI_FUNCTION_TRACE ("PsParseAml");

    ACPI_DEBUG_PRINT ((ACPI_DB_PARSE, "Entered with WalkState=%p Aml=%p size=%X\n",
        WalkState, WalkState->ParserState.Aml, WalkState->ParserState.AmlSize));


    /* Create and initialize a new thread state */

    Thread = AcpiUtCreateThreadState ();
    if (!Thread)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    WalkState->Thread = Thread;
    AcpiDsPushWalkState (WalkState, Thread);

    /*
     * This global allows the AML debugger to get a handle to the currently
     * executing control method.
     */
    AcpiGbl_CurrentWalkList = Thread;

    /*
     * Execute the walk loop as long as there is a valid Walk State.  This
     * handles nested control method invocations without recursion.
     */
    ACPI_DEBUG_PRINT ((ACPI_DB_PARSE, "State=%p\n", WalkState));

    Status = AE_OK;
    while (WalkState)
    {
        if (ACPI_SUCCESS (Status))
        {
            /*
             * The ParseLoop executes AML until the method terminates
             * or calls another method.
             */
            Status = AcpiPsParseLoop (WalkState);
        }

        ACPI_DEBUG_PRINT ((ACPI_DB_PARSE,
            "Completed one call to walk loop, %s State=%p\n",
            AcpiFormatException (Status), WalkState));

        if (Status == AE_CTRL_TRANSFER)
        {
            /*
             * A method call was detected.
             * Transfer control to the called control method
             */
            Status = AcpiDsCallControlMethod (Thread, WalkState, NULL);

            /*
             * If the transfer to the new method method call worked, a new walk
             * state was created -- get it
             */
            WalkState = AcpiDsGetCurrentWalkState (Thread);
            continue;
        }
        else if (Status == AE_CTRL_TERMINATE)
        {
            Status = AE_OK;
        }
        else if ((Status != AE_OK) && (WalkState->MethodDesc))
        {
            ACPI_REPORT_METHOD_ERROR ("Method execution failed",
                WalkState->MethodNode, NULL, Status);

            /* Check for possible multi-thread reentrancy problem */

            if ((Status == AE_ALREADY_EXISTS) &&
                (!WalkState->MethodDesc->Method.Semaphore))
            {
                /*
                 * This method is marked NotSerialized, but it tried to create a named
                 * object, causing the second thread entrance to fail.  We will workaround
                 * this by marking the method permanently as Serialized.
                 */
                WalkState->MethodDesc->Method.MethodFlags |= AML_METHOD_SERIALIZED;
                WalkState->MethodDesc->Method.Concurrency = 1;
            }
        }

        if (WalkState->MethodDesc)
        {
            /* Decrement the thread count on the method parse tree */

            if (WalkState->MethodDesc->Method.ThreadCount)
            {
                WalkState->MethodDesc->Method.ThreadCount--;
            }
        }

        /* We are done with this walk, move on to the parent if any */

        WalkState = AcpiDsPopWalkState (Thread);

        /* Reset the current scope to the beginning of scope stack */

        AcpiDsScopeStackClear (WalkState);

        /*
         * If we just returned from the execution of a control method,
         * there's lots of cleanup to do
         */
        if ((WalkState->ParseFlags & ACPI_PARSE_MODE_MASK) == ACPI_PARSE_EXECUTE)
        {
            TerminateStatus = AcpiDsTerminateControlMethod (WalkState);
            if (ACPI_FAILURE (TerminateStatus))
            {
                ACPI_REPORT_ERROR ((
                    "Could not terminate control method properly\n"));

                /* Ignore error and continue */
            }
        }

        /* Delete this walk state and all linked control states */

        AcpiPsCleanupScope (&WalkState->ParserState);

        PreviousWalkState = WalkState;

        ACPI_DEBUG_PRINT ((ACPI_DB_PARSE, "ReturnValue=%p, State=%p\n",
            WalkState->ReturnDesc, WalkState));

        /* Check if we have restarted a preempted walk */

        WalkState = AcpiDsGetCurrentWalkState (Thread);
        if (WalkState)
        {
            if (ACPI_SUCCESS (Status))
            {
                /*
                 * There is another walk state, restart it.
                 * If the method return value is not used by the parent,
                 * The object is deleted
                 */
                Status = AcpiDsRestartControlMethod (WalkState,
                            PreviousWalkState->ReturnDesc);
                if (ACPI_SUCCESS (Status))
                {
                    WalkState->WalkType |= ACPI_WALK_METHOD_RESTART;
                }
            }
            else
            {
                /* On error, delete any return object */

                AcpiUtRemoveReference (PreviousWalkState->ReturnDesc);
            }
        }

        /*
         * Just completed a 1st-level method, save the final internal return
         * value (if any)
         */
        else if (PreviousWalkState->CallerReturnDesc)
        {
            *(PreviousWalkState->CallerReturnDesc) = PreviousWalkState->ReturnDesc; /* NULL if no return value */
        }
        else if (PreviousWalkState->ReturnDesc)
        {
            /* Caller doesn't want it, must delete it */

            AcpiUtRemoveReference (PreviousWalkState->ReturnDesc);
        }

        AcpiDsDeleteWalkState (PreviousWalkState);
    }

    /* Normal exit */

    AcpiExReleaseAllMutexes (Thread);
    AcpiUtDeleteGenericState (ACPI_CAST_PTR (ACPI_GENERIC_STATE, Thread));
    AcpiGbl_CurrentWalkList = PrevWalkList;
    return_ACPI_STATUS (Status);
}


