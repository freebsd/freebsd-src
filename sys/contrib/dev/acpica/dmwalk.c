/*******************************************************************************
 *
 * Module Name: dmwalk - AML disassembly tree walk
 *              $Revision: 9 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2003, Intel Corp.
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


#include "acpi.h"
#include "acparser.h"
#include "amlcode.h"
#include "acdisasm.h"
#include "acdebug.h"


#ifdef ACPI_DISASSEMBLER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dmwalk")


#define DB_FULL_OP_INFO     "%5.5X #%4.4hX "


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDisassemble
 *
 * PARAMETERS:  Origin          - Starting object
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

    Info.Level = 0;
    AcpiDmWalkParseTree (Op, AcpiDmDescendingOp, AcpiDmAscendingOp, &Info);

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmWalkParseTree
 *
 * PARAMETERS:  DescendingCallback      - Called during tree descent
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
            Status = AscendingCallback (Op, Info->Level, Context);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
        }
        else
        {
            /*
             * Let the callback process the node.
             */
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
 * RETURN:      Status
 *
 * DESCRIPTION: Type of block for this op (parens or braces)
 *
 ******************************************************************************/

UINT32
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
 * RETURN:      Status
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

        return (0);

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

ACPI_STATUS
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


    if (Level == 0)
    {
        /* In verbose mode, print the AML offset, opcode and depth count */

        VERBOSE_PRINT ((DB_FULL_OP_INFO, (UINT32) Op->Common.AmlOffset,
                    Op->Common.AmlOpcode));

        if (Op->Common.AmlOpcode == AML_SCOPE_OP)
        {
            AcpiOsPrintf ("{\n");
            return (AE_OK);
        }
    }
    else if ((AcpiDmBlockType (Op->Common.Parent) & BLOCK_BRACE) &&
             (!(Op->Common.DisasmFlags & ACPI_PARSEOP_PARAMLIST)) &&
             (Op->Common.AmlOpcode != AML_INT_BYTELIST_OP))
    {
            /* This is a first-level element of a term list, indent a new line */

            AcpiDmIndent (Level);
    }

    /* Print the opcode name */

    AcpiDmDisassembleOneOp (NULL, Info, Op);

    if ((Op->Common.AmlOpcode == AML_NAME_OP) ||
        (Op->Common.AmlOpcode == AML_RETURN_OP))
    {
        Info->Level--;
    }

    /*
     * Start the opcode argument list if necessary
     */
    OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);

    if ((OpInfo->Flags & AML_HAS_ARGS) ||
        (Op->Common.AmlOpcode == AML_EVENT_OP))
    {
        /* This opcode has an argument list */

        if (AcpiDmBlockType (Op) & BLOCK_PAREN)
        {
            AcpiOsPrintf (" (");
        }

        /*
         * If this is a named opcode, print the associated name value
         */
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
                    AcpiDmDumpName ((char *) &Name);
                }


                if (Op->Common.AmlOpcode != AML_INT_NAMEDFIELD_OP)
                {
                    AcpiDmValidateName ((char *) &Name, Op);
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

                AcpiIsEisaId (Op);
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

                AcpiOsPrintf ("*** Unhandled named opcode\n");
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

                /* Namestring */

                NextOp = AcpiPsGetDepthNext (NULL, NextOp);
                AcpiDmNamestring (NextOp->Common.Value.Name);
                NextOp->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;
                AcpiOsPrintf (", ");


                NextOp = NextOp->Common.Next;
                AcpiDmDisassembleOneOp (NULL, Info, NextOp);
                NextOp->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;
                AcpiOsPrintf (", ");
                break;

            case AML_INDEX_FIELD_OP:

                /* Namestring */

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

ACPI_STATUS
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
                /* This is a first-level element of a term list, start a new line */

                AcpiOsPrintf ("\n");
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
                /* This is a first-level element of a term list, start a new line */

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
            Op->Common.Parent->Common.DisasmFlags |= ACPI_PARSEOP_EMPTY_TERMLIST;
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
