/******************************************************************************
 *
 * Module Name: psparse - Parser top level AML parse routines
 *              $Revision: 85 $
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
#include "acdebug.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_PARSER
        MODULE_NAME         ("psparse")


UINT32                      AcpiGbl_Depth = 0;
extern UINT32               AcpiGbl_ScopeDepth;


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsPeekOpcode
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get next AML opcode (without incrementing AML pointer)
 *
 ******************************************************************************/

static UINT32
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
    Opcode = (UINT16) GET8 (Aml);

    Aml++;


    /*
     * Original code special cased LNOTEQUAL, LLESSEQUAL, LGREATEREQUAL.
     * These opcodes are no longer recognized. Instead, they are broken into
     * two opcodes.
     *
     *
     *    if (Opcode == AML_EXTOP
     *       || (Opcode == AML_LNOT
     *          && (GET8 (Aml) == AML_LEQUAL
     *               || GET8 (Aml) == AML_LGREATER
     *               || GET8 (Aml) == AML_LLESS)))
     *
     *     extended Opcode, !=, <=, or >=
     */

    if (Opcode == AML_EXTOP)
    {
        /* Extended opcode */

        Opcode = (UINT16) ((Opcode << 8) | GET8 (Aml));
        Aml++;
    }

    /* don't convert bare name to a namepath */

    return (Opcode);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsCreateState
 *
 * PARAMETERS:  Aml             - Aml code pointer
 *              AmlSize         - Length of AML code
 *
 * RETURN:      A new parser state object
 *
 * DESCRIPTION: Create and initialize a new parser state object
 *
 ******************************************************************************/

ACPI_PARSE_STATE *
AcpiPsCreateState (
    UINT8                   *Aml,
    UINT32                  AmlSize)
{
    ACPI_PARSE_STATE        *ParserState;


    FUNCTION_TRACE ("PsCreateState");


    ParserState = AcpiUtCallocate (sizeof (ACPI_PARSE_STATE));
    if (!ParserState)
    {
        return_PTR (NULL);
    }

    ParserState->Aml       = Aml;
    ParserState->AmlEnd    = Aml + AmlSize;
    ParserState->PkgEnd    = ParserState->AmlEnd;
    ParserState->AmlStart  = Aml;


    return_PTR (ParserState);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsFindObject
 *
 * PARAMETERS:  Opcode          - Current opcode
 *              ParserState     - Current state
 *              WalkState       - Current state
 *              *Op             - Where found/new op is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Find a named object.  Two versions - one to search the parse
 *              tree (for parser-only applications such as acpidump), another
 *              to search the ACPI internal namespace (the parse tree may no
 *              longer exist)
 *
 ******************************************************************************/

#ifdef PARSER_ONLY

ACPI_STATUS
AcpiPsFindObject (
    UINT16                  Opcode,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       **OutOp)
{
    NATIVE_CHAR             *Path;


    /* We are only interested in opcodes that have an associated name */

    if (!AcpiPsIsNamedOp (Opcode))
    {
        *OutOp = Op;
        return (AE_OK);
    }

    /* Find the name in the parse tree */

    Path = AcpiPsGetNextNamestring (WalkState->ParserState);

    *OutOp = AcpiPsFind (AcpiPsGetParentScope (WalkState->ParserState),
                 Path, Opcode, 1);

    if (!(*OutOp))
    {
        return (AE_NOT_FOUND);
    }

    return (AE_OK);
}

#endif


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsCompleteThisOp
 *
 * PARAMETERS:  WalkState       - Current State
 *              Op              - Op to complete
 *
 * RETURN:      TRUE if Op and subtree was deleted
 *
 * DESCRIPTION: Perform any cleanup at the completion of an Op.
 *
 ******************************************************************************/

static BOOLEAN
AcpiPsCompleteThisOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op)
{
#ifndef PARSER_ONLY
    ACPI_PARSE_OBJECT       *Prev;
    ACPI_PARSE_OBJECT       *Next;
    ACPI_OPCODE_INFO        *OpInfo;
    ACPI_OPCODE_INFO        *ParentInfo;
    UINT32                  OpcodeClass;
    ACPI_PARSE_OBJECT       *ReplacementOp = NULL;


    FUNCTION_TRACE_PTR ("PsCompleteThisOp", Op);


    OpInfo      = AcpiPsGetOpcodeInfo (Op->Opcode);
    OpcodeClass = ACPI_GET_OP_CLASS (OpInfo);


    /* Delete this op and the subtree below it if asked to */

    if (((WalkState->ParseFlags & ACPI_PARSE_TREE_MASK) == ACPI_PARSE_DELETE_TREE) &&
        (OpcodeClass != OPTYPE_CONSTANT)        &&
        (OpcodeClass != OPTYPE_LITERAL)         &&
        (OpcodeClass != OPTYPE_LOCAL_VARIABLE)  &&
        (OpcodeClass != OPTYPE_METHOD_ARGUMENT) &&
        (OpcodeClass != OPTYPE_DATA_TERM)       &&
        (Op->Opcode  != AML_INT_NAMEPATH_OP))
    {
        /* Make sure that we only delete this subtree */

        if (Op->Parent)
        {
            /*
             * Check if we need to replace the operator and its subtree
             * with a return value op (placeholder op)
             */

            ParentInfo  = AcpiPsGetOpcodeInfo (Op->Parent->Opcode);

            switch (ACPI_GET_OP_CLASS (ParentInfo))
            {
            case OPTYPE_CONTROL:        /* IF, ELSE, WHILE only */
                break;

            case OPTYPE_NAMED_OBJECT:   /* Scope, method, etc. */

                /*
                 * These opcodes contain TermArg operands.  The current
                 * op must be replace by a placeholder return op
                 */

                if ((Op->Parent->Opcode == AML_REGION_OP)               ||
                    (Op->Parent->Opcode == AML_CREATE_FIELD_OP)         ||
                    (Op->Parent->Opcode == AML_CREATE_BIT_FIELD_OP)     ||
                    (Op->Parent->Opcode == AML_CREATE_BYTE_FIELD_OP)    ||
                    (Op->Parent->Opcode == AML_CREATE_WORD_FIELD_OP)    ||
                    (Op->Parent->Opcode == AML_CREATE_DWORD_FIELD_OP)   ||
                    (Op->Parent->Opcode == AML_CREATE_QWORD_FIELD_OP))
                {
                    ReplacementOp = AcpiPsAllocOp (AML_INT_RETURN_VALUE_OP);
                    if (!ReplacementOp)
                    {
                        return_VALUE (FALSE);
                    }
                }

                break;

            default:
                ReplacementOp = AcpiPsAllocOp (AML_INT_RETURN_VALUE_OP);
                if (!ReplacementOp)
                {
                    return_VALUE (FALSE);
                }
            }

            /* We must unlink this op from the parent tree */

            Prev = Op->Parent->Value.Arg;
            if (Prev == Op)
            {
                /* This op is the first in the list */

                if (ReplacementOp)
                {
                    ReplacementOp->Parent    = Op->Parent;
                    ReplacementOp->Value.Arg = NULL;
                    Op->Parent->Value.Arg    = ReplacementOp;
                    ReplacementOp->Next      = Op->Next;
                }
                else
                {
                    Op->Parent->Value.Arg    = Op->Next;
                }
            }

            /* Search the parent list */

            else while (Prev)
            {
                /* Traverse all siblings in the parent's argument list */

                Next = Prev->Next;
                if (Next == Op)
                {
                    if (ReplacementOp)
                    {
                        ReplacementOp->Parent = Op->Parent;
                        ReplacementOp->Value.Arg = NULL;
                        Prev->Next = ReplacementOp;
                        ReplacementOp->Next = Op->Next;
                        Next = NULL;
                    }
                    else
                    {
                        Prev->Next = Op->Next;
                        Next = NULL;
                    }
                }

                Prev = Next;
            }

        }

        /* Now we can actually delete the subtree rooted at op */

        AcpiPsDeleteParseTree (Op);

        return_VALUE (TRUE);
    }

    return_VALUE (FALSE);

#else
    return (FALSE);
#endif
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsNextParseState
 *
 * PARAMETERS:  ParserState         - Current parser state object
 *
 * RETURN:
 *
 * DESCRIPTION:
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiPsNextParseState (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_STATUS             CallbackStatus)
{
    ACPI_PARSE_STATE        *ParserState = WalkState->ParserState;
    ACPI_STATUS             Status = AE_CTRL_PENDING;
    UINT8                   *Start;
    UINT32                  PackageLength;


    FUNCTION_TRACE_PTR ("PsNextParseState", Op);


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


    case AE_CTRL_PENDING:

            /*
             * Predicate of a WHILE was true and the loop just completed an
             * execution.  Go back to the start of the loop and reevaluate the
             * predicate.
             */
/*            WalkState->ControlState->Common.State =
                    CONTROL_PREDICATE_EXECUTING;*/

        /* TBD: How to handle a break within a while. */
        /* This code attempts it */

        ParserState->Aml = WalkState->AmlLastWhile;
        break;


    case AE_CTRL_TRUE:
            /*
             * Predicate of an IF was true, and we are at the matching ELSE.
             * Just close out this package
             *
             * Note: ParserState->Aml is modified by the package length procedure
             * TBD: [Investigate] perhaps it shouldn't, too much trouble
             */
        Start = ParserState->Aml;
        PackageLength = AcpiPsGetNextPackageLength (ParserState);
        ParserState->Aml = Start + PackageLength;
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
        WalkState->MethodCallNode = (Op->Value.Arg)->Node;

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
    ACPI_OPCODE_INFO        *OpInfo;
    ACPI_PARSE_OBJECT       *Arg = NULL;
    ACPI_PARSE2_OBJECT      *DeferredOp;
    UINT32                  ArgCount;       /* push for fixed or var args */
    UINT32                  ArgTypes = 0;
    UINT32                  AmlOffset;
    UINT16                  Opcode;
    ACPI_PARSE_OBJECT       PreOp;
    ACPI_PARSE_STATE        *ParserState;
    UINT8                   *AmlOpStart;


    FUNCTION_TRACE_PTR ("PsParseLoop", WalkState);


    ParserState = WalkState->ParserState;

#ifndef PARSER_ONLY
    if (WalkState->WalkType & WALK_METHOD_RESTART)
    {
        /* We are restarting a preempted control method */

        if (AcpiPsHasCompletedScope (ParserState))
        {
            /*
             * We must check if a predicate to an IF or WHILE statement
             * was just completed
             */
            if ((ParserState->Scope->ParseScope.Op) &&
                ((ParserState->Scope->ParseScope.Op->Opcode == AML_IF_OP) ||
                (ParserState->Scope->ParseScope.Op->Opcode == AML_WHILE_OP)) &&
                (WalkState->ControlState) &&
                (WalkState->ControlState->Common.State ==
                    CONTROL_PREDICATE_EXECUTING))
            {

                /*
                 * A predicate was just completed, get the value of the
                 * predicate and branch based on that value
                 */

                Status = AcpiDsGetPredicateValue (WalkState, NULL, TRUE);
                if (ACPI_FAILURE (Status) &&
                    ((Status & AE_CODE_MASK) != AE_CODE_CONTROL))
                {
                    if (Status == AE_AML_NO_RETURN_VALUE)
                    {
                        DEBUG_PRINTP (ACPI_ERROR,
                            ("Invoked method did not return a value, %s\n",
                            AcpiUtFormatException (Status)));

                    }
                    DEBUG_PRINTP (ACPI_ERROR, ("GetPredicate Failed, %s\n",
                        AcpiUtFormatException (Status)));
                    return_ACPI_STATUS (Status);
                }

                Status = AcpiPsNextParseState (WalkState, Op, Status);
            }

            AcpiPsPopScope (ParserState, &Op, &ArgTypes, &ArgCount);
            DEBUG_PRINTP (TRACE_PARSE, ("Popped scope, Op=%p\n", Op));
        }

        else if (WalkState->PrevOp)
        {
            /* We were in the middle of an op */

            Op = WalkState->PrevOp;
            ArgTypes = WalkState->PrevArgTypes;
        }
    }
#endif

    /*
     * Iterative parsing loop, while there is more aml to process:
     */
    while ((ParserState->Aml < ParserState->AmlEnd) || (Op))
    {
        if (!Op)
        {
            /* Get the next opcode from the AML stream */

            AmlOpStart = ParserState->Aml;
            AmlOffset  = ParserState->Aml - ParserState->AmlStart;
            Opcode     = AcpiPsPeekOpcode (ParserState);

            /*
             * First cut to determine what we have found:
             * 1) A valid AML opcode
             * 2) A name string
             * 3) An unknown/invalid opcode
             */

            OpInfo = AcpiPsGetOpcodeInfo (Opcode);
            switch (ACPI_GET_OP_TYPE (OpInfo))
            {
            case ACPI_OP_TYPE_OPCODE:

                /* Found opcode info, this is a normal opcode */

                ParserState->Aml += AcpiPsGetOpcodeSize (Opcode);
                ArgTypes = OpInfo->ParseArgs;
                break;

            case ACPI_OP_TYPE_ASCII:
            case ACPI_OP_TYPE_PREFIX:
                /*
                 * Starts with a valid prefix or ASCII char, this is a name
                 * string.  Convert the bare name string to a namepath.
                 */

                Opcode = AML_INT_NAMEPATH_OP;
                ArgTypes = ARGP_NAMESTRING;
                break;

            case ACPI_OP_TYPE_UNKNOWN:

                /* The opcode is unrecognized.  Just skip unknown opcodes */

                DEBUG_PRINTP (ACPI_ERROR,
                    ("Found unknown opcode %lX at AML offset %X, ignoring\n",
                    Opcode, AmlOffset));

                DUMP_BUFFER (ParserState->Aml, 128);

                /* Assume one-byte bad opcode */

                ParserState->Aml++;
                continue;
            }


            /* Create Op structure and append to parent's argument list */

            if (AcpiPsIsNamedOp (Opcode))
            {
                PreOp.Value.Arg = NULL;
                PreOp.Opcode = Opcode;

                while (GET_CURRENT_ARG_TYPE (ArgTypes) != ARGP_NAME)
                {
                    Arg = AcpiPsGetNextArg (ParserState,
                                            GET_CURRENT_ARG_TYPE (ArgTypes),
                                            &ArgCount);
                    AcpiPsAppendArg (&PreOp, Arg);
                    INCREMENT_ARG_LIST (ArgTypes);
                }


                /* We know that this arg is a name, move to next arg */

                INCREMENT_ARG_LIST (ArgTypes);

                if (WalkState->DescendingCallback != NULL)
                {
                    /*
                     * Find the object.  This will either insert the object into
                     * the namespace or simply look it up
                     */
                    Status = WalkState->DescendingCallback (Opcode, NULL, WalkState, &Op);
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
                }

                AcpiPsAppendArg (Op, PreOp.Value.Arg);
                AcpiGbl_Depth++;


                if (Op->Opcode == AML_REGION_OP)
                {
                    DeferredOp = AcpiPsToExtendedOp (Op);
                    if (DeferredOp)
                    {
                        /*
                         * Defer final parsing of an OperationRegion body,
                         * because we don't have enough info in the first pass
                         * to parse it correctly (i.e., there may be method
                         * calls within the TermArg elements of the body.
                         *
                         * However, we must continue parsing because
                         * the opregion is not a standalone package --
                         * we don't know where the end is at this point.
                         *
                         * (Length is unknown until parse of the body complete)
                         */

                        DeferredOp->Data    = AmlOpStart;
                        DeferredOp->Length  = 0;
                    }
                }
            }


            else
            {
                /* Not a named opcode, just allocate Op and append to parent */

                Op = AcpiPsAllocOp (Opcode);
                if (!Op)
                {
                    return_ACPI_STATUS (AE_NO_MEMORY);
                }


                if ((Op->Opcode == AML_CREATE_FIELD_OP)        ||
                    (Op->Opcode == AML_CREATE_BIT_FIELD_OP)    ||
                    (Op->Opcode == AML_CREATE_BYTE_FIELD_OP)   ||
                    (Op->Opcode == AML_CREATE_WORD_FIELD_OP)   ||
                    (Op->Opcode == AML_CREATE_DWORD_FIELD_OP)  ||
                    (Op->Opcode == AML_CREATE_QWORD_FIELD_OP))
                 {
                    /*
                     * Backup to beginning of CreateXXXfield declaration
                     * BodyLength is unknown until we parse the body
                     */
                    DeferredOp = (ACPI_PARSE2_OBJECT *) Op;

                    DeferredOp->Data    = AmlOpStart;
                    DeferredOp->Length  = 0;
                }

                AcpiPsAppendArg (AcpiPsGetParentScope (ParserState), Op);

                if ((WalkState->DescendingCallback != NULL))
                {
                    /*
                     * Find the object.  This will either insert the object into
                     * the namespace or simply look it up
                     */
                    Status = WalkState->DescendingCallback (Opcode, Op, WalkState, &Op);
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

            Op->AmlOffset = AmlOffset;

            if (OpInfo)
            {
                DEBUG_PRINTP (TRACE_PARSE,
                    ("Op=%p Opcode=%4.4lX Aml %p Oft=%5.5lX\n",
                     Op, Op->Opcode, ParserState->Aml, Op->AmlOffset));
            }
        }


        /* Start ArgCount at zero because we don't know if there are any args yet */

        ArgCount  = 0;


        if (ArgTypes)   /* Are there any arguments that must be processed? */
        {
            /* get arguments */

            switch (Op->Opcode)
            {
            case AML_BYTE_OP:       /* AML_BYTEDATA_ARG */
            case AML_WORD_OP:       /* AML_WORDDATA_ARG */
            case AML_DWORD_OP:      /* AML_DWORDATA_ARG */
            case AML_STRING_OP:     /* AML_ASCIICHARLIST_ARG */

                /* fill in constant or string argument directly */

                AcpiPsGetNextSimpleArg (ParserState,
                                        GET_CURRENT_ARG_TYPE (ArgTypes), Op);
                break;

            case AML_INT_NAMEPATH_OP:   /* AML_NAMESTRING_ARG */

                AcpiPsGetNextNamepath (ParserState, Op, &ArgCount, 1);
                ArgTypes = 0;
                break;


            default:

                /* Op is not a constant or string, append each argument */

                while (GET_CURRENT_ARG_TYPE (ArgTypes) && !ArgCount)
                {
                    AmlOffset = ParserState->Aml - ParserState->AmlStart;
                    Arg = AcpiPsGetNextArg (ParserState,
                                            GET_CURRENT_ARG_TYPE (ArgTypes),
                                            &ArgCount);
                    if (Arg)
                    {
                        Arg->AmlOffset = AmlOffset;
                        AcpiPsAppendArg (Op, Arg);
                    }

                    INCREMENT_ARG_LIST (ArgTypes);
                }


                /* For a method, save the length and address of the body */

                if (Op->Opcode == AML_METHOD_OP)
                {
                    DeferredOp = AcpiPsToExtendedOp (Op);
                    if (DeferredOp)
                    {
                        /*
                         * Skip parsing of control method or opregion body,
                         * because we don't have enough info in the first pass
                         * to parse them correctly.
                         */

                        DeferredOp->Data    = ParserState->Aml;
                        DeferredOp->Length  = (UINT32) (ParserState->PkgEnd -
                                                        ParserState->Aml);

                        /*
                         * Skip body of method.  For OpRegions, we must continue
                         * parsing because the opregion is not a standalone
                         * package (We don't know where the end is).
                         */
                        ParserState->Aml    = ParserState->PkgEnd;
                        ArgCount            = 0;
                    }
                }

                break;
            }
        }


        /*
         * Zero ArgCount means that all arguments for this op have been processed
         */
        if (!ArgCount)
        {
            /* completed Op, prepare for next */

            if (AcpiPsIsNamedOp (Op->Opcode))
            {
                if (AcpiGbl_Depth)
                {
                    AcpiGbl_Depth--;
                }

                if (Op->Opcode == AML_REGION_OP)
                {
                    DeferredOp = AcpiPsToExtendedOp (Op);
                    if (DeferredOp)
                    {
                        /*
                         * Skip parsing of control method or opregion body,
                         * because we don't have enough info in the first pass
                         * to parse them correctly.
                         *
                         * Completed parsing an OpRegion declaration, we now
                         * know the length.
                         */

                        DeferredOp->Length = (UINT32) (ParserState->Aml -
                                                        DeferredOp->Data);
                    }
                }
            }

            if ((Op->Opcode == AML_CREATE_FIELD_OP)         ||
                (Op->Opcode == AML_CREATE_BIT_FIELD_OP)     ||
                (Op->Opcode == AML_CREATE_BYTE_FIELD_OP)    ||
                (Op->Opcode == AML_CREATE_WORD_FIELD_OP)    ||
                (Op->Opcode == AML_CREATE_DWORD_FIELD_OP)   ||
                (Op->Opcode == AML_CREATE_QWORD_FIELD_OP))
            {
                /*
                 * Backup to beginning of CreateXXXfield declaration (1 for
                 * Opcode)
                 *
                 * BodyLength is unknown until we parse the body
                 */
                DeferredOp = (ACPI_PARSE2_OBJECT *) Op;
                DeferredOp->Length = (UINT32) (ParserState->Aml -
                                               DeferredOp->Data);
            }

            /* This op complete, notify the dispatcher */

            if (WalkState->AscendingCallback != NULL)
            {
                Status = WalkState->AscendingCallback (WalkState, Op);
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

            /* Close this Op (may result in parse subtree deletion) */

            if (AcpiPsCompleteThisOp (WalkState, Op))
            {
                Op = NULL;
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
                WalkState->PrevArgTypes = ArgTypes;
                return_ACPI_STATUS (Status);
                break;


            case AE_CTRL_END:

                AcpiPsPopScope (ParserState, &Op, &ArgTypes, &ArgCount);

                Status = WalkState->AscendingCallback (WalkState, Op);
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

                    AcpiPsPopScope (ParserState, &Op, &ArgTypes, &ArgCount);
                } while (Op);

                return_ACPI_STATUS (Status);
                break;


            default:  /* All other non-AE_OK status */

                if (Op == NULL)
                {
                    AcpiPsPopScope (ParserState, &Op, &ArgTypes, &ArgCount);
                }
                WalkState->PrevOp = Op;
                WalkState->PrevArgTypes = ArgTypes;

                /*
                 * TEMP:
                 */

                return_ACPI_STATUS (Status);
                break;
            }


            /* This scope complete? */

            if (AcpiPsHasCompletedScope (ParserState))
            {
                AcpiPsPopScope (ParserState, &Op, &ArgTypes, &ArgCount);
                DEBUG_PRINTP (TRACE_PARSE, ("Popped scope, Op=%p\n", Op));
            }

            else
            {
                Op = NULL;
            }

        }


        /* ArgCount is non-zero */

        else
        {
            /* complex argument, push Op and prepare for argument */

            AcpiPsPushScope (ParserState, Op, ArgTypes, ArgCount);
            Op = NULL;
        }

    } /* while ParserState->Aml */


    /*
     * Complete the last Op (if not completed), and clear the scope stack.
     * It is easily possible to end an AML "package" with an unbounded number
     * of open scopes (such as when several AML blocks are closed with
     * sequential closing braces).  We want to terminate each one cleanly.
     */

    DEBUG_PRINTP (TRACE_PARSE, ("Package complete at Op %p\n", Op));
    do
    {
        if (Op)
        {
            if (WalkState->AscendingCallback != NULL)
            {
                Status = WalkState->AscendingCallback (WalkState, Op);
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

                        AcpiPsPopScope (ParserState, &Op, &ArgTypes, &ArgCount);

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

        AcpiPsPopScope (ParserState, &Op, &ArgTypes, &ArgCount);

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
 * RETURN:      Status
 *
 * DESCRIPTION: Parse raw AML and return a tree of ops
 *
 ******************************************************************************/

ACPI_STATUS
AcpiPsParseAml (
    ACPI_PARSE_OBJECT       *StartScope,
    UINT8                   *Aml,
    UINT32                  AmlSize,
    UINT32                  ParseFlags,
    ACPI_NAMESPACE_NODE     *MethodNode,
    ACPI_OPERAND_OBJECT     **Params,
    ACPI_OPERAND_OBJECT     **CallerReturnDesc,
    ACPI_PARSE_DOWNWARDS    DescendingCallback,
    ACPI_PARSE_UPWARDS      AscendingCallback)
{
    ACPI_STATUS             Status;
    ACPI_PARSE_STATE        *ParserState;
    ACPI_WALK_STATE         *WalkState;
    ACPI_WALK_LIST          WalkList;
    ACPI_NAMESPACE_NODE     *Node = NULL;
    ACPI_WALK_LIST          *PrevWalkList = AcpiGbl_CurrentWalkList;
    ACPI_OPERAND_OBJECT     *ReturnDesc;
    ACPI_OPERAND_OBJECT     *EffectiveReturnDesc = NULL;
    ACPI_OPERAND_OBJECT     *MthDesc = NULL;


    FUNCTION_TRACE ("PsParseAml");

    DEBUG_PRINTP (TRACE_PARSE, ("Entered with Scope=%p Aml=%p size=%lX\n",
        StartScope, Aml, AmlSize));


    /* Create and initialize a new parser state */

    ParserState = AcpiPsCreateState (Aml, AmlSize);
    if (!ParserState)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    AcpiPsInitScope (ParserState, StartScope);

    if (MethodNode)
    {
        MthDesc = AcpiNsGetAttachedObject (MethodNode);
    }

    /* Create and initialize a new walk list */

    WalkList.WalkState = NULL;
    WalkList.AcquiredMutexList.Prev = NULL;
    WalkList.AcquiredMutexList.Next = NULL;

    WalkState = AcpiDsCreateWalkState (TABLE_ID_DSDT, ParserState->StartOp, 
                        MthDesc, &WalkList);
    if (!WalkState)
    {
        Status = AE_NO_MEMORY;
        goto Cleanup;
    }

    WalkState->MethodNode           = MethodNode;
    WalkState->ParserState          = ParserState;
    WalkState->ParseFlags           = ParseFlags;
    WalkState->DescendingCallback   = DescendingCallback;
    WalkState->AscendingCallback    = AscendingCallback;

    /* TBD: [Restructure] TEMP until we pass WalkState to the interpreter
     */
    AcpiGbl_CurrentWalkList = &WalkList;


    if (MethodNode)
    {
        ParserState->StartNode  = MethodNode;
        WalkState->WalkType     = WALK_METHOD;

        /* Push start scope on scope stack and make it current  */

        Status = AcpiDsScopeStackPush (MethodNode, ACPI_TYPE_METHOD, WalkState);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /* Init arguments if this is a control method */
        /* TBD: [Restructure] add walkstate as a param */

        AcpiDsMethodDataInitArgs (Params, MTH_NUM_ARGS, WalkState);
    }

    else
    {
        /* Setup the current scope */

        Node = ParserState->StartOp->Node;
        ParserState->StartNode = Node;

        if (Node)
        {
            /* Push start scope on scope stack and make it current  */

            Status = AcpiDsScopeStackPush (Node, Node->Type,
                                            WalkState);
            if (ACPI_FAILURE (Status))
            {
                goto Cleanup;
            }

        }
    }


    Status = AE_OK;

    /*
     * Execute the walk loop as long as there is a valid Walk State.  This
     * handles nested control method invocations without recursion.
     */

    DEBUG_PRINTP (TRACE_PARSE, ("State=%p\n", WalkState));

    while (WalkState)
    {
        if (ACPI_SUCCESS (Status))
        {
            Status = AcpiPsParseLoop (WalkState);
        }

        DEBUG_PRINTP (TRACE_PARSE, 
            ("Completed one call to walk loop, State=%p\n", WalkState));

        if (Status == AE_CTRL_TRANSFER)
        {
            /*
             * A method call was detected.
             * Transfer control to the called control method
             */

            Status = AcpiDsCallControlMethod (&WalkList, WalkState, NULL);

            /*
             * If the transfer to the new method method call worked, a new walk
             * state was created -- get it
             */

            WalkState = AcpiDsGetCurrentWalkState (&WalkList);
            continue;
        }

        else if (Status == AE_CTRL_TERMINATE)
        {
            Status = AE_OK;
        }

        /* We are done with this walk, move on to the parent if any */


        WalkState = AcpiDsPopWalkState (&WalkList);

        /* Extract return value before we delete WalkState */

        ReturnDesc = WalkState->ReturnDesc;

        /* Save the last effective return value */

        if (CallerReturnDesc && ReturnDesc)
        {
	    EffectiveReturnDesc = ReturnDesc;
            AcpiUtAddReference (EffectiveReturnDesc);
        }

	DEBUG_PRINTP (TRACE_PARSE, ("ReturnValue=%p, State=%p\n",
            WalkState->ReturnDesc, WalkState));

        /* Reset the current scope to the beginning of scope stack */

        AcpiDsScopeStackClear (WalkState);

        /*
         * If we just returned from the execution of a control method,
         * there's lots of cleanup to do
         */

        if ((WalkState->ParseFlags & ACPI_PARSE_MODE_MASK) == ACPI_PARSE_EXECUTE)
        {
            AcpiDsTerminateControlMethod (WalkState);
        }

         /* Delete this walk state and all linked control states */

        AcpiPsCleanupScope (WalkState->ParserState);
        AcpiUtFree (WalkState->ParserState);
        AcpiDsDeleteWalkState (WalkState);

       /* Check if we have restarted a preempted walk */

        WalkState = AcpiDsGetCurrentWalkState (&WalkList);
        if (WalkState &&
            ACPI_SUCCESS (Status))
        {
            /* There is another walk state, restart it */

            /*
             * If the method returned value is not used by the parent,
             * The object is deleted
             */

            AcpiDsRestartControlMethod (WalkState, ReturnDesc);
            WalkState->WalkType |= WALK_METHOD_RESTART;
        }

        /*
         * Just completed a 1st-level method, save the final internal return
         * value (if any)
         */

        else if (CallerReturnDesc)
        {
            /*
             * Some AML code expects return value w/o ReturnOp.
             * Return the saved effective return value instead.
             */

            if (ReturnDesc == NULL && EffectiveReturnDesc != NULL)
            {
                AcpiUtRemoveReference (ReturnDesc);
                ReturnDesc = EffectiveReturnDesc;
            }
            *CallerReturnDesc = ReturnDesc; /* NULL if no return value */
        }

        else if (ReturnDesc)
        {
            /* Caller doesn't want it, must delete it */

            AcpiUtRemoveReference (ReturnDesc);
        }
    }


    /* Normal exit */

    AcpiExReleaseAllMutexes ((ACPI_OPERAND_OBJECT *) &WalkList.AcquiredMutexList);
    AcpiGbl_CurrentWalkList = PrevWalkList;
    return_ACPI_STATUS (Status);


Cleanup:

    /* Cleanup */

    AcpiDsDeleteWalkState (WalkState);
    AcpiPsCleanupScope (ParserState);
    AcpiUtFree (ParserState);

    AcpiExReleaseAllMutexes ((ACPI_OPERAND_OBJECT *)&WalkList.AcquiredMutexList);
    AcpiGbl_CurrentWalkList = PrevWalkList;

    return_ACPI_STATUS (Status);
}


