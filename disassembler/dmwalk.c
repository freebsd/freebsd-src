/*******************************************************************************
 *
 * Module Name: dmwalk - AML disassembly tree walk
 *
 ******************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
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


#include "acpi.h"
#include "accommon.h"
#include "acparser.h"
#include "amlcode.h"
#include "acdisasm.h"
#include "acdebug.h"


#ifdef ACPI_DISASSEMBLER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dmwalk")


#define DB_FULL_OP_INFO     "[%4.4s] @%5.5X #%4.4X:  "

/* Stub for non-compiler code */

#ifndef ACPI_ASL_COMPILER
void
AcpiDmEmitExternals (
    void)
{
    return;
}
#endif

/* Local prototypes */

static ACPI_STATUS
AcpiDmDescendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
AcpiDmAscendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static UINT32
AcpiDmBlockType (
    ACPI_PARSE_OBJECT       *Op);


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDisassemble
 *
 * PARAMETERS:  WalkState       - Current state
 *              Origin          - Starting object
 *              NumOpcodes      - Max number of opcodes to be displayed
 *
 * RETURN:      None
 *
 * DESCRIPTION: Disassemble parser object and its children.  This is the
 *              main entry point of the disassembler.
 *
 ******************************************************************************/

void
AcpiDmDisassemble (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Origin,
    UINT32                  NumOpcodes)
{
    ACPI_PARSE_OBJECT       *Op = Origin;
    ACPI_OP_WALK_INFO       Info;


    if (!Op)
    {
        return;
    }

    Info.Flags = 0;
    Info.Level = 0;
    Info.Count = 0;
    Info.WalkState = WalkState;
    AcpiDmWalkParseTree (Op, AcpiDmDescendingOp, AcpiDmAscendingOp, &Info);
    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmWalkParseTree
 *
 * PARAMETERS:  Op                      - Root Op object
 *              DescendingCallback      - Called during tree descent
 *              AscendingCallback       - Called during tree ascent
 *              Context                 - To be passed to the callbacks
 *
 * RETURN:      Status from callback(s)
 *
 * DESCRIPTION: Walk the entire parse tree.
 *
 ******************************************************************************/

void
AcpiDmWalkParseTree (
    ACPI_PARSE_OBJECT       *Op,
    ASL_WALK_CALLBACK       DescendingCallback,
    ASL_WALK_CALLBACK       AscendingCallback,
    void                    *Context)
{
    BOOLEAN                 NodePreviouslyVisited;
    ACPI_PARSE_OBJECT       *StartOp = Op;
    ACPI_STATUS             Status;
    ACPI_PARSE_OBJECT       *Next;
    ACPI_OP_WALK_INFO       *Info = Context;


    Info->Level = 0;
    NodePreviouslyVisited = FALSE;

    while (Op)
    {
        if (NodePreviouslyVisited)
        {
            if (AscendingCallback)
            {
                Status = AscendingCallback (Op, Info->Level, Context);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
            }
        }
        else
        {
            /* Let the callback process the node */

            Status = DescendingCallback (Op, Info->Level, Context);
            if (ACPI_SUCCESS (Status))
            {
                /* Visit children first, once */

                Next = AcpiPsGetArg (Op, 0);
                if (Next)
                {
                    Info->Level++;
                    Op = Next;
                    continue;
                }
            }
            else if (Status != AE_CTRL_DEPTH)
            {
                /* Exit immediately on any error */

                return;
            }
        }

        /* Terminate walk at start op */

        if (Op == StartOp)
        {
            break;
        }

        /* No more children, re-visit this node */

        if (!NodePreviouslyVisited)
        {
            NodePreviouslyVisited = TRUE;
            continue;
        }

        /* No more children, visit peers */

        if (Op->Common.Next)
        {
            Op = Op->Common.Next;
            NodePreviouslyVisited = FALSE;
        }
        else
        {
            /* No peers, re-visit parent */

            if (Info->Level != 0 )
            {
                Info->Level--;
            }

            Op = Op->Common.Parent;
            NodePreviouslyVisited = TRUE;
        }
    }

    /* If we get here, the walk completed with no errors */

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmBlockType
 *
 * PARAMETERS:  Op              - Object to be examined
 *
 * RETURN:      BlockType - not a block, parens, braces, or even both.
 *
 * DESCRIPTION: Type of block for this op (parens or braces)
 *
 ******************************************************************************/

static UINT32
AcpiDmBlockType (
    ACPI_PARSE_OBJECT       *Op)
{
    const ACPI_OPCODE_INFO  *OpInfo;


    if (!Op)
    {
        return (BLOCK_NONE);
    }

    switch (Op->Common.AmlOpcode)
    {
    case AML_ELSE_OP:

        return (BLOCK_BRACE);

    case AML_METHOD_OP:
    case AML_DEVICE_OP:
    case AML_SCOPE_OP:
    case AML_PROCESSOR_OP:
    case AML_POWER_RES_OP:
    case AML_THERMAL_ZONE_OP:
    case AML_IF_OP:
    case AML_WHILE_OP:
    case AML_FIELD_OP:
    case AML_INDEX_FIELD_OP:
    case AML_BANK_FIELD_OP:

        return (BLOCK_PAREN | BLOCK_BRACE);

    case AML_BUFFER_OP:

        if (Op->Common.DisasmOpcode == ACPI_DASM_UNICODE)
        {
            return (BLOCK_NONE);
        }

        /*lint -fallthrough */

    case AML_PACKAGE_OP:
    case AML_VAR_PACKAGE_OP:

        return (BLOCK_PAREN | BLOCK_BRACE);

    case AML_EVENT_OP:

        return (BLOCK_PAREN);

    default:

        OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
        if (OpInfo->Flags & AML_HAS_ARGS)
        {
            return (BLOCK_PAREN);
        }

        return (BLOCK_NONE);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmListType
 *
 * PARAMETERS:  Op              - Object to be examined
 *
 * RETURN:      ListType - has commas or not.
 *
 * DESCRIPTION: Type of block for this op (parens or braces)
 *
 ******************************************************************************/

UINT32
AcpiDmListType (
    ACPI_PARSE_OBJECT       *Op)
{
    const ACPI_OPCODE_INFO  *OpInfo;


    if (!Op)
    {
        return (BLOCK_NONE);
    }

    switch (Op->Common.AmlOpcode)
    {

    case AML_ELSE_OP:
    case AML_METHOD_OP:
    case AML_DEVICE_OP:
    case AML_SCOPE_OP:
    case AML_POWER_RES_OP:
    case AML_PROCESSOR_OP:
    case AML_THERMAL_ZONE_OP:
    case AML_IF_OP:
    case AML_WHILE_OP:
    case AML_FIELD_OP:
    case AML_INDEX_FIELD_OP:
    case AML_BANK_FIELD_OP:

        return (BLOCK_NONE);

    case AML_BUFFER_OP:
    case AML_PACKAGE_OP:
    case AML_VAR_PACKAGE_OP:

        return (BLOCK_COMMA_LIST);

    default:

        OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
        if (OpInfo->Flags & AML_HAS_ARGS)
        {
            return (BLOCK_COMMA_LIST);
        }

        return (BLOCK_NONE);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDescendingOp
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: First visitation of a parse object during tree descent.
 *              Decode opcode name and begin parameter list(s), if any.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDmDescendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_OP_WALK_INFO       *Info = Context;
    const ACPI_OPCODE_INFO  *OpInfo;
    UINT32                  Name;
    ACPI_PARSE_OBJECT       *NextOp;


    if (Op->Common.DisasmFlags & ACPI_PARSEOP_IGNORE)
    {
        /* Ignore this op -- it was handled elsewhere */

        return (AE_CTRL_DEPTH);
    }

    /* Level 0 is at the Definition Block level */

    if (Level == 0)
    {
        /* In verbose mode, print the AML offset, opcode and depth count */

        if (Info->WalkState)
        {
            VERBOSE_PRINT ((DB_FULL_OP_INFO,
                (Info->WalkState->MethodNode ?
                    Info->WalkState->MethodNode->Name.Ascii : "   "),
                Op->Common.AmlOffset, (UINT32) Op->Common.AmlOpcode));
        }

        if (Op->Common.AmlOpcode == AML_SCOPE_OP)
        {
            /* This is the beginning of the Definition Block */

            AcpiOsPrintf ("{\n");

            /* Emit all External() declarations here */

            AcpiDmEmitExternals ();
            return (AE_OK);
        }
    }
    else if ((AcpiDmBlockType (Op->Common.Parent) & BLOCK_BRACE) &&
             (!(Op->Common.DisasmFlags & ACPI_PARSEOP_PARAMLIST)) &&
             (Op->Common.AmlOpcode != AML_INT_BYTELIST_OP))
    {
            /*
             * This is a first-level element of a term list,
             * indent a new line
             */
            AcpiDmIndent (Level);
            Info->LastLevel = Level;
            Info->Count = 0;
    }

    /*
     * This is an inexpensive mechanism to try and keep lines from getting
     * too long. When the limit is hit, start a new line at the previous
     * indent plus one. A better but more expensive mechanism would be to
     * keep track of the current column.
     */
    Info->Count++;
    if (Info->Count /*+Info->LastLevel*/ > 10)
    {
        Info->Count = 0;
        AcpiOsPrintf ("\n");
        AcpiDmIndent (Info->LastLevel + 1);
    }

    /* Print the opcode name */

    AcpiDmDisassembleOneOp (NULL, Info, Op);

    if (Op->Common.DisasmOpcode == ACPI_DASM_LNOT_PREFIX)
    {
        return (AE_OK);
    }

    if ((Op->Common.AmlOpcode == AML_NAME_OP) ||
        (Op->Common.AmlOpcode == AML_RETURN_OP))
    {
        Info->Level--;
    }

    /* Start the opcode argument list if necessary */

    OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);

    if ((OpInfo->Flags & AML_HAS_ARGS) ||
        (Op->Common.AmlOpcode == AML_EVENT_OP))
    {
        /* This opcode has an argument list */

        if (AcpiDmBlockType (Op) & BLOCK_PAREN)
        {
            AcpiOsPrintf (" (");
        }

        /* If this is a named opcode, print the associated name value */

        if (OpInfo->Flags & AML_NAMED)
        {
            switch (Op->Common.AmlOpcode)
            {
            case AML_ALIAS_OP:

                NextOp = AcpiPsGetDepthNext (NULL, Op);
                NextOp->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;
                AcpiDmNamestring (NextOp->Common.Value.Name);
                AcpiOsPrintf (", ");

                /*lint -fallthrough */

            default:

                Name = AcpiPsGetName (Op);
                if (Op->Named.Path)
                {
                    AcpiDmNamestring ((char *) Op->Named.Path);
                }
                else
                {
                    AcpiDmDumpName (Name);
                }

                if (Op->Common.AmlOpcode != AML_INT_NAMEDFIELD_OP)
                {
                    if (AcpiGbl_DbOpt_verbose)
                    {
                        (void) AcpiPsDisplayObjectPathname (NULL, Op);
                    }
                }
                break;
            }

            switch (Op->Common.AmlOpcode)
            {
            case AML_METHOD_OP:

                AcpiDmMethodFlags (Op);
                AcpiOsPrintf (")");
                break;


            case AML_NAME_OP:

                /* Check for _HID and related EISAID() */

                AcpiDmIsEisaId (Op);
                AcpiOsPrintf (", ");
                break;


            case AML_REGION_OP:

                AcpiDmRegionFlags (Op);
                break;


            case AML_POWER_RES_OP:

                /* Mark the next two Ops as part of the parameter list */

                AcpiOsPrintf (", ");
                NextOp = AcpiPsGetDepthNext (NULL, Op);
                NextOp->Common.DisasmFlags |= ACPI_PARSEOP_PARAMLIST;

                NextOp = NextOp->Common.Next;
                NextOp->Common.DisasmFlags |= ACPI_PARSEOP_PARAMLIST;
                return (AE_OK);


            case AML_PROCESSOR_OP:

                /* Mark the next three Ops as part of the parameter list */

                AcpiOsPrintf (", ");
                NextOp = AcpiPsGetDepthNext (NULL, Op);
                NextOp->Common.DisasmFlags |= ACPI_PARSEOP_PARAMLIST;

                NextOp = NextOp->Common.Next;
                NextOp->Common.DisasmFlags |= ACPI_PARSEOP_PARAMLIST;

                NextOp = NextOp->Common.Next;
                NextOp->Common.DisasmFlags |= ACPI_PARSEOP_PARAMLIST;
                return (AE_OK);


            case AML_MUTEX_OP:
            case AML_DATA_REGION_OP:

                AcpiOsPrintf (", ");
                return (AE_OK);


            case AML_EVENT_OP:
            case AML_ALIAS_OP:

                return (AE_OK);


            case AML_SCOPE_OP:
            case AML_DEVICE_OP:
            case AML_THERMAL_ZONE_OP:

                AcpiOsPrintf (")");
                break;


            default:

                AcpiOsPrintf ("*** Unhandled named opcode %X\n", Op->Common.AmlOpcode);
                break;
            }
        }

        else switch (Op->Common.AmlOpcode)
        {
        case AML_FIELD_OP:
        case AML_BANK_FIELD_OP:
        case AML_INDEX_FIELD_OP:

            Info->BitOffset = 0;

            /* Name of the parent OperationRegion */

            NextOp = AcpiPsGetDepthNext (NULL, Op);
            AcpiDmNamestring (NextOp->Common.Value.Name);
            AcpiOsPrintf (", ");
            NextOp->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;

            switch (Op->Common.AmlOpcode)
            {
            case AML_BANK_FIELD_OP:

                /* Namestring - Bank Name */

                NextOp = AcpiPsGetDepthNext (NULL, NextOp);
                AcpiDmNamestring (NextOp->Common.Value.Name);
                NextOp->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;
                AcpiOsPrintf (", ");

                /*
                 * Bank Value. This is a TermArg in the middle of the parameter
                 * list, must handle it here.
                 *
                 * Disassemble the TermArg parse tree. ACPI_PARSEOP_PARAMLIST
                 * eliminates newline in the output.
                 */
                NextOp = NextOp->Common.Next;

                Info->Flags = ACPI_PARSEOP_PARAMLIST;
                AcpiDmWalkParseTree (NextOp, AcpiDmDescendingOp, AcpiDmAscendingOp, Info);
                Info->Flags = 0;
                Info->Level = Level;

                NextOp->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;
                AcpiOsPrintf (", ");
                break;

            case AML_INDEX_FIELD_OP:

                /* Namestring - Data Name */

                NextOp = AcpiPsGetDepthNext (NULL, NextOp);
                AcpiDmNamestring (NextOp->Common.Value.Name);
                AcpiOsPrintf (", ");
                NextOp->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;
                break;

            default:

                break;
            }

            AcpiDmFieldFlags (NextOp);
            break;


        case AML_BUFFER_OP:

            /* The next op is the size parameter */

            NextOp = AcpiPsGetDepthNext (NULL, Op);
            if (!NextOp)
            {
                /* Single-step support */

                return (AE_OK);
            }

            if (Op->Common.DisasmOpcode == ACPI_DASM_RESOURCE)
            {
                /*
                 * We have a resource list.  Don't need to output
                 * the buffer size Op.  Open up a new block
                 */
                NextOp->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;
                NextOp = NextOp->Common.Next;
                AcpiOsPrintf (")\n");
                AcpiDmIndent (Info->Level);
                AcpiOsPrintf ("{\n");
                return (AE_OK);
            }

            /* Normal Buffer, mark size as in the parameter list */

            NextOp->Common.DisasmFlags |= ACPI_PARSEOP_PARAMLIST;
            return (AE_OK);


        case AML_VAR_PACKAGE_OP:
        case AML_IF_OP:
        case AML_WHILE_OP:

            /* The next op is the size or predicate parameter */

            NextOp = AcpiPsGetDepthNext (NULL, Op);
            if (NextOp)
            {
                NextOp->Common.DisasmFlags |= ACPI_PARSEOP_PARAMLIST;
            }
            return (AE_OK);


        case AML_PACKAGE_OP:

            /* The next op is the size or predicate parameter */

            NextOp = AcpiPsGetDepthNext (NULL, Op);
            if (NextOp)
            {
                NextOp->Common.DisasmFlags |= ACPI_PARSEOP_PARAMLIST;
            }
            return (AE_OK);


        case AML_MATCH_OP:

            AcpiDmMatchOp (Op);
            break;


        default:

            break;
        }

        if (AcpiDmBlockType (Op) & BLOCK_BRACE)
        {
            AcpiOsPrintf ("\n");
            AcpiDmIndent (Level);
            AcpiOsPrintf ("{\n");
        }
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmAscendingOp
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Second visitation of a parse object, during ascent of parse
 *              tree.  Close out any parameter lists and complete the opcode.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDmAscendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_OP_WALK_INFO       *Info = Context;


    if (Op->Common.DisasmFlags & ACPI_PARSEOP_IGNORE)
    {
        /* Ignore this op -- it was handled elsewhere */

        return (AE_OK);
    }

    if ((Level == 0) && (Op->Common.AmlOpcode == AML_SCOPE_OP))
    {
        /* Indicates the end of the current descriptor block (table) */

        AcpiOsPrintf ("}\n\n");
        return (AE_OK);
    }

    switch (AcpiDmBlockType (Op))
    {
    case BLOCK_PAREN:

        /* Completed an op that has arguments, add closing paren */

        AcpiOsPrintf (")");

        /* Could be a nested operator, check if comma required */

        if (!AcpiDmCommaIfListMember (Op))
        {
            if ((AcpiDmBlockType (Op->Common.Parent) & BLOCK_BRACE) &&
                     (!(Op->Common.DisasmFlags & ACPI_PARSEOP_PARAMLIST)) &&
                     (Op->Common.AmlOpcode != AML_INT_BYTELIST_OP))
            {
                /*
                 * This is a first-level element of a term list
                 * start a new line
                 */
                if (!(Info->Flags & ACPI_PARSEOP_PARAMLIST))
                {
                    AcpiOsPrintf ("\n");
                }
            }
        }
        break;


    case BLOCK_BRACE:
    case (BLOCK_BRACE | BLOCK_PAREN):

        /* Completed an op that has a term list, add closing brace */

        if (Op->Common.DisasmFlags & ACPI_PARSEOP_EMPTY_TERMLIST)
        {
            AcpiOsPrintf ("}");
        }
        else
        {
            AcpiDmIndent (Level);
            AcpiOsPrintf ("}");
        }

        AcpiDmCommaIfListMember (Op);

        if (AcpiDmBlockType (Op->Common.Parent) != BLOCK_PAREN)
        {
            AcpiOsPrintf ("\n");
            if (!(Op->Common.DisasmFlags & ACPI_PARSEOP_EMPTY_TERMLIST))
            {
                if ((Op->Common.AmlOpcode == AML_IF_OP)  &&
                    (Op->Common.Next) &&
                    (Op->Common.Next->Common.AmlOpcode == AML_ELSE_OP))
                {
                    break;
                }

                if ((AcpiDmBlockType (Op->Common.Parent) & BLOCK_BRACE) &&
                    (!Op->Common.Next))
                {
                    break;
                }
                AcpiOsPrintf ("\n");
            }
        }
        break;


    case BLOCK_NONE:
    default:

        /* Could be a nested operator, check if comma required */

        if (!AcpiDmCommaIfListMember (Op))
        {
            if ((AcpiDmBlockType (Op->Common.Parent) & BLOCK_BRACE) &&
                     (!(Op->Common.DisasmFlags & ACPI_PARSEOP_PARAMLIST)) &&
                     (Op->Common.AmlOpcode != AML_INT_BYTELIST_OP))
            {
                /*
                 * This is a first-level element of a term list
                 * start a new line
                 */
                AcpiOsPrintf ("\n");
            }
        }
        else if (Op->Common.Parent)
        {
            switch (Op->Common.Parent->Common.AmlOpcode)
            {
            case AML_PACKAGE_OP:
            case AML_VAR_PACKAGE_OP:

                if (!(Op->Common.DisasmFlags & ACPI_PARSEOP_PARAMLIST))
                {
                    AcpiOsPrintf ("\n");
                }
                break;

            default:

                break;
            }
        }
        break;
    }

    if (Op->Common.DisasmFlags & ACPI_PARSEOP_PARAMLIST)
    {
        if ((Op->Common.Next) &&
            (Op->Common.Next->Common.DisasmFlags & ACPI_PARSEOP_PARAMLIST))
        {
            return (AE_OK);
        }

        /*
         * Just completed a parameter node for something like "Buffer (param)".
         * Close the paren and open up the term list block with a brace
         */
        if (Op->Common.Next)
        {
            AcpiOsPrintf (")\n");
            AcpiDmIndent (Level - 1);
            AcpiOsPrintf ("{\n");
        }
        else
        {
            Op->Common.Parent->Common.DisasmFlags |=
                                    ACPI_PARSEOP_EMPTY_TERMLIST;
            AcpiOsPrintf (") {");
        }
    }

    if ((Op->Common.AmlOpcode == AML_NAME_OP) ||
        (Op->Common.AmlOpcode == AML_RETURN_OP))
    {
        Info->Level++;
    }
    return (AE_OK);
}


#endif  /* ACPI_DISASSEMBLER */
