/*******************************************************************************
 *
 * Module Name: dbdisasm - parser op tree display routines
 *              $Revision: 61 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2002, Intel Corp.
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
#include "acnamesp.h"
#include "acdebug.h"


#ifdef ENABLE_DEBUGGER

#define _COMPONENT          ACPI_DEBUGGER
        ACPI_MODULE_NAME    ("dbdisasm")


#define BLOCK_PAREN         1
#define BLOCK_BRACE         2
#define DB_NO_OP_INFO       "            [%2.2d]  "
#define DB_FULL_OP_INFO     "%5.5X #%4.4X [%2.2d]  "


NATIVE_CHAR                 *AcpiGbl_DbDisasmIndent = "....";


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbBlockType
 *
 * PARAMETERS:  Op              - Object to be examined
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Type of block for this op (parens or braces)
 *
 ******************************************************************************/

UINT32
AcpiDbBlockType (
    ACPI_PARSE_OBJECT       *Op)
{

    switch (Op->Opcode)
    {
    case AML_METHOD_OP:
        return (BLOCK_BRACE);

    default:
        break;
    }

    return (BLOCK_PAREN);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsDisplayObjectPathname
 *
 * PARAMETERS:  Op              - Object whose pathname is to be obtained
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Diplay the pathname associated with a named object.  Two
 *              versions. One searches the parse tree (for parser-only
 *              applications suchas AcpiDump), and the other searches the
 *              ACPI namespace (the parse tree is probably deleted)
 *
 ******************************************************************************/

#ifdef PARSER_ONLY

ACPI_STATUS
AcpiPsDisplayObjectPathname (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *TargetOp;
    char                    *Name;


    if (Op->Flags & ACPI_PARSEOP_GENERIC)
    {
        Name = Op->Value.Name;
        if (Name[0] == '\\')
        {
            AcpiOsPrintf ("  (Fully Qualified Pathname)");
            return (AE_OK);
        }
    }
    else
    {
        Name = (char *) &((ACPI_PARSE2_OBJECT *) Op)->Name;
    }

    /* Search parent tree up to the root if necessary */

    TargetOp = AcpiPsFind (Op, Name, 0, 0);
    if (!TargetOp)
    {
        /*
         * Didn't find the name in the parse tree.  This may be
         * a problem, or it may simply be one of the predefined names
         * (such as _OS_).  Rather than worry about looking up all
         * the predefined names, just display the name as given
         */
        AcpiOsPrintf ("  **** Path not found in parse tree");
    }
    else
    {
        /* The target was found, print the name and complete path */

        AcpiOsPrintf ("  (Path ");
        AcpiDbDisplayPath (TargetOp);
        AcpiOsPrintf (")");
    }

    return (AE_OK);
}

#else

ACPI_STATUS
AcpiPsDisplayObjectPathname (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_BUFFER             Buffer;
    UINT32                  DebugLevel;


    /* Save current debug level so we don't get extraneous debug output */

    DebugLevel = AcpiDbgLevel;
    AcpiDbgLevel = 0;

    /* Just get the Node out of the Op object */

    Node = Op->Node;
    if (!Node)
    {
        /* Node not defined in this scope, look it up */

        Status = AcpiNsLookup (WalkState->ScopeInfo, Op->Value.String, ACPI_TYPE_ANY,
                        ACPI_IMODE_EXECUTE, ACPI_NS_SEARCH_PARENT, WalkState, &(Node));

        if (ACPI_FAILURE (Status))
        {
            /*
             * We can't get the pathname since the object
             * is not in the namespace.  This can happen during single
             * stepping where a dynamic named object is *about* to be created.
             */
            AcpiOsPrintf ("  [Path not found]");
            goto Exit;
        }

        /* Save it for next time. */

        Op->Node = Node;
    }

    /* Convert NamedDesc/handle to a full pathname */

    Buffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
    Status = AcpiNsHandleToPathname (Node, &Buffer);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("****Could not get pathname****)");
        goto Exit;
    }

    AcpiOsPrintf ("  (Path %s)", Buffer.Pointer);
    ACPI_MEM_FREE (Buffer.Pointer);


Exit:
    /* Restore the debug level */

    AcpiDbgLevel = DebugLevel;
    return (Status);
}

#endif


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayOp
 *
 * PARAMETERS:  Origin          - Starting object
 *              NumOpcodes      - Max number of opcodes to be displayed
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display parser object and its children
 *
 ******************************************************************************/

void
AcpiDbDisplayOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Origin,
    UINT32                  NumOpcodes)
{
    ACPI_PARSE_OBJECT       *Op = Origin;
    ACPI_PARSE_OBJECT       *arg;
    ACPI_PARSE_OBJECT       *depth;
    UINT32                  DepthCount = 0;
    UINT32                  LastDepth = 0;
    UINT32                  i;
    UINT32                  j;


    if (!Op)
    {
        AcpiDbDisplayOpcode (WalkState, Op);
        return;
    }


    while (Op)
    {
        /* Indentation */

        DepthCount = 0;
        if (!AcpiGbl_DbOpt_verbose)
        {
            DepthCount++;
        }

        /* Determine the nesting depth of this argument */

        for (depth = Op->Parent; depth; depth = depth->Parent)
        {
            arg = AcpiPsGetArg (depth, 0);
            while (arg && arg != Origin)
            {
                arg = arg->Next;
            }

            if (arg)
            {
                break;
            }

            DepthCount++;
        }

        /* Open a new block if we are nested further than last time */

        if (DepthCount > LastDepth)
        {
            VERBOSE_PRINT ((DB_NO_OP_INFO, LastDepth));
            for (i = 0; i < LastDepth; i++)
            {
                AcpiOsPrintf ("%s", AcpiGbl_DbDisasmIndent);
            }

            if (AcpiDbBlockType (Op) == BLOCK_PAREN)
            {
                AcpiOsPrintf ("(\n");
            }
            else
            {
                AcpiOsPrintf ("{\n");
            }
        }

        /* Close a block if we are nested less than last time */

        else if (DepthCount < LastDepth)
        {
            for (j = 0; j < (LastDepth - DepthCount); j++)
            {
                VERBOSE_PRINT ((DB_NO_OP_INFO, LastDepth - j));
                for (i = 0; i < (LastDepth - j - 1); i++)
                {
                    AcpiOsPrintf ("%s", AcpiGbl_DbDisasmIndent);
                }

                if (AcpiDbBlockType (Op) == BLOCK_PAREN)
                {
                    AcpiOsPrintf (")\n");
                }
                else
                {
                    AcpiOsPrintf ("}\n");
                }
            }
        }

        /* In verbose mode, print the AML offset, opcode and depth count */

        VERBOSE_PRINT ((DB_FULL_OP_INFO, (unsigned) Op->AmlOffset, Op->Opcode, DepthCount));


        /* Indent the output according to the depth count */

        for (i = 0; i < DepthCount; i++)
        {
            AcpiOsPrintf ("%s", AcpiGbl_DbDisasmIndent);
        }

        /* Now print the opcode */

        AcpiDbDisplayOpcode (WalkState, Op);

        /* Resolve a name reference */

        if ((Op->Opcode == AML_INT_NAMEPATH_OP && Op->Value.Name)  &&
            (Op->Parent) &&
            (AcpiGbl_DbOpt_verbose))
        {
            AcpiPsDisplayObjectPathname (WalkState, Op);
        }

        AcpiOsPrintf ("\n");

        /* Get the next node in the tree */

        Op = AcpiPsGetDepthNext (Origin, Op);
        LastDepth = DepthCount;

        NumOpcodes--;
        if (!NumOpcodes)
        {
            Op = NULL;
        }
    }

    /* Close the last block(s) */

    DepthCount = LastDepth -1;
    for (i = 0; i < LastDepth; i++)
    {
        VERBOSE_PRINT ((DB_NO_OP_INFO, LastDepth - i));
        for (j = 0; j < DepthCount; j++)
        {
            AcpiOsPrintf ("%s", AcpiGbl_DbDisasmIndent);
        }
        AcpiOsPrintf ("}\n");
        DepthCount--;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayNamestring
 *
 * PARAMETERS:  Name                - ACPI Name string to store
 *
 * RETURN:      None
 *
 * DESCRIPTION: Display namestring. Handles prefix characters
 *
 ******************************************************************************/

void
AcpiDbDisplayNamestring (
    NATIVE_CHAR             *Name)
{
    UINT32                  SegCount;


    if (!Name)
    {
        AcpiOsPrintf ("<NULL NAME PTR>");
        return;
    }

    /* Handle all Scope Prefix operators */

    while (AcpiPsIsPrefixChar (ACPI_GET8 (Name)))
    {
        /* Append prefix character */

        AcpiOsPrintf ("%1c", ACPI_GET8 (Name));
        Name++;
    }

    switch (ACPI_GET8 (Name))
    {
    case 0:
        SegCount = 0;
        break;

    case AML_DUAL_NAME_PREFIX:
        SegCount = 2;
        Name++;
        break;

    case AML_MULTI_NAME_PREFIX_OP:
        SegCount = (UINT32) ACPI_GET8 (Name + 1);
        Name += 2;
        break;

    default:
        SegCount = 1;
        break;
    }

    while (SegCount)
    {
        /* Append Name segment */

        AcpiOsPrintf ("%4.4s", Name);

        SegCount--;
        if (SegCount)
        {
            /* Not last name, append dot separator */

            AcpiOsPrintf (".");
        }
        Name += ACPI_NAME_SIZE;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayPath
 *
 * PARAMETERS:  Op                  - Named Op whose path is to be constructed
 *
 * RETURN:      None
 *
 * DESCRIPTION: Walk backwards from current scope and display the name
 *              of each previous level of scope up to the root scope
 *              (like "pwd" does with file systems)
 *
 ******************************************************************************/

void
AcpiDbDisplayPath (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *Prev;
    ACPI_PARSE_OBJECT       *Search;
    UINT32                  Name;
    BOOLEAN                 DoDot = FALSE;
    ACPI_PARSE_OBJECT       *NamePath;
    const ACPI_OPCODE_INFO  *OpInfo;


    /* We are only interested in named objects */

    OpInfo = AcpiPsGetOpcodeInfo (Op->Opcode);
    if (!(OpInfo->Flags & AML_NSNODE))
    {
        return;
    }

    if (OpInfo->Flags & AML_CREATE)
    {
        /* Field creation - check for a fully qualified namepath */

        if (Op->Opcode == AML_CREATE_FIELD_OP)
        {
            NamePath = AcpiPsGetArg (Op, 3);
        }
        else
        {
            NamePath = AcpiPsGetArg (Op, 2);
        }

        if ((NamePath) &&
            (NamePath->Value.String) &&
            (NamePath->Value.String[0] == '\\'))
        {
            AcpiDbDisplayNamestring (NamePath->Value.String);
            return;
        }
    }

    Prev = NULL;            /* Start with Root Node */

    while (Prev != Op)
    {
        /* Search upwards in the tree to find scope with "prev" as its parent */

        Search = Op;
        for (; ;)
        {
            if (Search->Parent == Prev)
            {
                break;
            }

            /* Go up one level */

            Search = Search->Parent;
        }

        if (Prev)
        {
            OpInfo = AcpiPsGetOpcodeInfo (Search->Opcode);
            if (!(OpInfo->Flags & AML_FIELD))
            {
                /* below root scope, append scope name */

                if (DoDot)
                {
                    /* append dot */

                    AcpiOsPrintf (".");
                }

                if (OpInfo->Flags & AML_CREATE)
                {
                    if (Op->Opcode == AML_CREATE_FIELD_OP)
                    {
                        NamePath = AcpiPsGetArg (Op, 3);
                    }
                    else
                    {
                        NamePath = AcpiPsGetArg (Op, 2);
                    }

                    if ((NamePath) &&
                        (NamePath->Value.String))
                    {
                        AcpiOsPrintf ("%4.4s", NamePath->Value.String);
                    }
                }
                else
                {
                    Name = AcpiPsGetName (Search);
                    AcpiOsPrintf ("%4.4s", &Name);
                }

                DoDot = TRUE;
            }
        }

        Prev = Search;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbDisplayOpcode
 *
 * PARAMETERS:  Op                  - Op that is to be printed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Store printed op in a Buffer and return its length
 *              (or -1 if out of space)
 *
 * NOTE: Terse mode prints out ASL-like code.  Verbose mode adds more info.
 *
 ******************************************************************************/

void
AcpiDbDisplayOpcode (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op)
{
    UINT8                   *ByteData;
    UINT32                  ByteCount;
    UINT32                  i;
    const ACPI_OPCODE_INFO  *OpInfo = NULL;
    UINT32                  Name;


    if (!Op)
    {
        AcpiOsPrintf ("<NULL OP PTR>");
    }

    /* op and arguments */

    switch (Op->Opcode)
    {
    case AML_BYTE_OP:

        if (AcpiGbl_DbOpt_verbose)
        {
            AcpiOsPrintf ("(UINT8)  0x%2.2X", Op->Value.Integer8);
        }
        else
        {
            AcpiOsPrintf ("0x%2.2X", Op->Value.Integer8);
        }
        break;


    case AML_WORD_OP:

        if (AcpiGbl_DbOpt_verbose)
        {
            AcpiOsPrintf ("(UINT16) 0x%4.4X", Op->Value.Integer16);
        }
        else
        {
            AcpiOsPrintf ("0x%4.4X", Op->Value.Integer16);
        }
        break;


    case AML_DWORD_OP:

        if (AcpiGbl_DbOpt_verbose)
        {
            AcpiOsPrintf ("(UINT32) 0x%8.8X", Op->Value.Integer32);
        }
        else
        {
            AcpiOsPrintf ("0x%8.8X", Op->Value.Integer32);
        }
        break;


    case AML_QWORD_OP:

        if (AcpiGbl_DbOpt_verbose)
        {
            AcpiOsPrintf ("(UINT64) 0x%8.8X%8.8X", Op->Value.Integer64.Hi,
                                                   Op->Value.Integer64.Lo);
        }
        else
        {
            AcpiOsPrintf ("0x%8.8X%8.8X", Op->Value.Integer64.Hi,
                                          Op->Value.Integer64.Lo);
        }
        break;


    case AML_STRING_OP:

        if (Op->Value.String)
        {
            AcpiOsPrintf ("\"%s\"", Op->Value.String);
        }
        else
        {
            AcpiOsPrintf ("<\"NULL STRING PTR\">");
        }
        break;


    case AML_INT_STATICSTRING_OP:

        if (Op->Value.String)
        {
            AcpiOsPrintf ("\"%s\"", Op->Value.String);
        }
        else
        {
            AcpiOsPrintf ("\"<NULL STATIC STRING PTR>\"");
        }
        break;


    case AML_INT_NAMEPATH_OP:

        AcpiDbDisplayNamestring (Op->Value.Name);
        break;


    case AML_INT_NAMEDFIELD_OP:

        AcpiOsPrintf ("NamedField    (Length 0x%8.8X)  ", Op->Value.Integer32);
        break;


    case AML_INT_RESERVEDFIELD_OP:

        AcpiOsPrintf ("ReservedField (Length 0x%8.8X)  ", Op->Value.Integer32);
        break;


    case AML_INT_ACCESSFIELD_OP:

        AcpiOsPrintf ("AccessField   (Length 0x%8.8X)  ", Op->Value.Integer32);
        break;


    case AML_INT_BYTELIST_OP:

        if (AcpiGbl_DbOpt_verbose)
        {
            AcpiOsPrintf ("ByteList      (Length 0x%8.8X)  ", Op->Value.Integer32);
        }
        else
        {
            AcpiOsPrintf ("0x%2.2X", Op->Value.Integer32);

            ByteCount = Op->Value.Integer32;
            ByteData = ((ACPI_PARSE2_OBJECT *) Op)->Data;

            for (i = 0; i < ByteCount; i++)
            {
                AcpiOsPrintf (", 0x%2.2X", ByteData[i]);
            }
        }
        break;


    default:

        /* Just get the opcode name and print it */

        OpInfo = AcpiPsGetOpcodeInfo (Op->Opcode);
        AcpiOsPrintf ("%s", OpInfo->Name);


#ifndef PARSER_ONLY
        if ((Op->Opcode == AML_INT_RETURN_VALUE_OP) &&
            (WalkState->Results) &&
            (WalkState->Results->Results.NumResults))
        {
            AcpiDbDecodeInternalObject (WalkState->Results->Results.ObjDesc [WalkState->Results->Results.NumResults-1]);
        }
#endif
        break;
    }

    if (!OpInfo)
    {
        /* If there is another element in the list, add a comma */

        if (Op->Next)
        {
            AcpiOsPrintf (",");
        }
    }

    /*
     * If this is a named opcode, print the associated name value
     */
    OpInfo = AcpiPsGetOpcodeInfo (Op->Opcode);
    if (Op && (OpInfo->Flags & AML_NAMED))
    {
        Name = AcpiPsGetName (Op);
        AcpiOsPrintf (" %4.4s", &Name);

        if ((AcpiGbl_DbOpt_verbose) && (Op->Opcode != AML_INT_NAMEDFIELD_OP))
        {
            AcpiPsDisplayObjectPathname (WalkState, Op);
        }
    }
}

#endif  /* ENABLE_DEBUGGER */

