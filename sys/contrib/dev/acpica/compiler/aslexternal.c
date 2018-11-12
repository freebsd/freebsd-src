/******************************************************************************
 *
 * Module Name: aslexternal - ASL External opcode compiler support
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2016, Intel Corp.
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

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"
#include <contrib/dev/acpica/include/acparser.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acnamesp.h>


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslexternal")


/* Local prototypes */

static void
ExInsertArgCount (
    ACPI_PARSE_OBJECT       *Op);

static void
ExMoveExternals (
    ACPI_PARSE_OBJECT       *DefinitionBlockOp);


/*******************************************************************************
 *
 * FUNCTION:    ExDoExternal
 *
 * PARAMETERS:  Op                  - Current Parse node
 *
 * RETURN:      None
 *
 * DESCRIPTION: Add an External() definition to the global list. This list
 *              is used to generate External opcodes.
 *
 ******************************************************************************/

void
ExDoExternal (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *ListOp;
    ACPI_PARSE_OBJECT       *Prev;
    ACPI_PARSE_OBJECT       *Next;
    ACPI_PARSE_OBJECT       *ArgCountOp;


    ArgCountOp = Op->Asl.Child->Asl.Next->Asl.Next;
    ArgCountOp->Asl.AmlOpcode = AML_RAW_DATA_BYTE;
    ArgCountOp->Asl.ParseOpcode = PARSEOP_BYTECONST;
    ArgCountOp->Asl.Value.Integer = 0;
    UtSetParseOpName (ArgCountOp);

    /* Create new list node of arbitrary type */

    ListOp = TrAllocateNode (PARSEOP_DEFAULT_ARG);

    /* Store External node as child */

    ListOp->Asl.Child = Op;
    ListOp->Asl.Next = NULL;

    if (Gbl_ExternalsListHead)
    {
        /* Link new External to end of list */

        Prev = Gbl_ExternalsListHead;
        Next = Prev;
        while (Next)
        {
            Prev = Next;
            Next = Next->Asl.Next;
        }

        Prev->Asl.Next = ListOp;
    }
    else
    {
        Gbl_ExternalsListHead = ListOp;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    ExInsertArgCount
 *
 * PARAMETERS:  Op              - Op for a method invocation
 *
 * RETURN:      None
 *
 * DESCRIPTION: Obtain the number of arguments for a control method -- from
 *              the actual invocation.
 *
 ******************************************************************************/

static void
ExInsertArgCount (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *Next;
    ACPI_PARSE_OBJECT       *NameOp;
    ACPI_PARSE_OBJECT       *Child;
    ACPI_PARSE_OBJECT       *ArgCountOp;
    char *                  ExternalName;
    char *                  CallName;
    UINT16                  ArgCount = 0;
    ACPI_STATUS             Status;


    CallName = AcpiNsGetNormalizedPathname (Op->Asl.Node, TRUE);

    Next = Gbl_ExternalsListHead;
    while (Next)
    {
        ArgCount = 0;

        /* Skip if External node already handled */

        if (Next->Asl.Child->Asl.CompileFlags & NODE_VISITED)
        {
            Next = Next->Asl.Next;
            continue;
        }

        NameOp = Next->Asl.Child->Asl.Child;
        ExternalName = AcpiNsGetNormalizedPathname (NameOp->Asl.Node, TRUE);

        if (strcmp (CallName, ExternalName))
        {
            ACPI_FREE (ExternalName);
            Next = Next->Asl.Next;
            continue;
        }

        Next->Asl.Child->Asl.CompileFlags |= NODE_VISITED;

        /*
         * Since we will reposition Externals to the Root, set Namepath
         * to the fully qualified name and recalculate the aml length
         */
        Status = UtInternalizeName (ExternalName,
            &NameOp->Asl.Value.String);

        ACPI_FREE (ExternalName);
        if (ACPI_FAILURE (Status))
        {
            AslError (ASL_ERROR, ASL_MSG_COMPILER_INTERNAL,
                NULL, "- Could not Internalize External");
            break;
        }

        NameOp->Asl.AmlLength = strlen (NameOp->Asl.Value.String);

        /* Get argument count */

        Child = Op->Asl.Child;
        while (Child)
        {
            ArgCount++;
            Child = Child->Asl.Next;
        }

        /* Setup ArgCount operand */

        ArgCountOp = Next->Asl.Child->Asl.Child->Asl.Next->Asl.Next;
        ArgCountOp->Asl.Value.Integer = ArgCount;
        break;
    }

    ACPI_FREE (CallName);
}


/*******************************************************************************
 *
 * FUNCTION:    ExAmlExternalWalkBegin
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      None
 *
 * DESCRIPTION: Parse tree walk to create external opcode list for methods.
 *
 ******************************************************************************/

ACPI_STATUS
ExAmlExternalWalkBegin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{

    /* External list head saved in the definition block op */

    if (Op->Asl.ParseOpcode == PARSEOP_DEFINITION_BLOCK)
    {
        Gbl_ExternalsListHead = Op->Asl.Value.Arg;
    }

    if (!Gbl_ExternalsListHead)
    {
        return (AE_OK);
    }

    if (Op->Asl.ParseOpcode != PARSEOP_METHODCALL)
    {
        return (AE_OK);
    }

    /*
     * The NameOp child under an ExternalOp gets turned into PARSE_METHODCALL
     * by XfNamespaceLocateBegin(). Ignore these.
     */
    if (Op->Asl.Parent &&
        Op->Asl.Parent->Asl.ParseOpcode == PARSEOP_EXTERNAL)
    {
        return (AE_OK);
    }

    ExInsertArgCount (Op);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    ExAmlExternalWalkEnd
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      None
 *
 * DESCRIPTION: Parse tree walk to create external opcode list for methods.
 *              Here, we just want to catch the case where a definition block
 *              has been completed. Then we move all of the externals into
 *              a single block in the parse tree and thus the AML code.
 *
 ******************************************************************************/

ACPI_STATUS
ExAmlExternalWalkEnd (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{

    if (Op->Asl.ParseOpcode == PARSEOP_DEFINITION_BLOCK)
    {
        /*
         * Process any existing external list. (Support for
         * multiple definition blocks in a single file/compile)
         */
        ExMoveExternals (Op);
        Gbl_ExternalsListHead = NULL;
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    ExMoveExternals
 *
 * PARAMETERS:  DefinitionBlockOp       - Op for current definition block
 *
 * RETURN:      None
 *
 * DESCRIPTION: Move all externals present in the source file into a single
 *              block of AML code, surrounded by an "If (0)" to prevent
 *              AML interpreters from attempting to execute the External
 *              opcodes.
 *
 ******************************************************************************/

static void
ExMoveExternals (
    ACPI_PARSE_OBJECT       *DefinitionBlockOp)
{
    ACPI_PARSE_OBJECT       *ParentOp;
    ACPI_PARSE_OBJECT       *ExternalOp;
    ACPI_PARSE_OBJECT       *PredicateOp;
    ACPI_PARSE_OBJECT       *NextOp;
    ACPI_PARSE_OBJECT       *Prev;
    ACPI_PARSE_OBJECT       *Next;
    ACPI_OBJECT_TYPE        ObjType;
    UINT32                  i;


    if (!Gbl_ExternalsListHead)
    {
        return;
    }

    /* Remove the External nodes from the tree */

    NextOp = Gbl_ExternalsListHead;
    while (NextOp)
    {
        /*
         * The External is stored in child pointer of each node in the
         * list
         */
        ExternalOp = NextOp->Asl.Child;

        /* Set line numbers (for listings, etc.) */

        ExternalOp->Asl.LineNumber = 0;
        ExternalOp->Asl.LogicalLineNumber = 0;

        Next = ExternalOp->Asl.Child;
        Next->Asl.LineNumber = 0;
        Next->Asl.LogicalLineNumber = 0;

        Next = Next->Asl.Next;
        Next->Asl.LineNumber = 0;
        Next->Asl.LogicalLineNumber = 0;

        Next = Next->Asl.Next;
        Next->Asl.LineNumber = 0;
        Next->Asl.LogicalLineNumber = 0;

        Next = Next->Asl.Next;
        Next->Asl.LineNumber = 0;
        Next->Asl.LogicalLineNumber = 0;

        ParentOp = ExternalOp->Asl.Parent;
        Prev = Next = ParentOp->Asl.Child;

        /* Now find the External node's position in parse tree */

        while (Next != ExternalOp)
        {
            Prev = Next;
            Next = Next->Asl.Next;
        }

        /* Remove the External from the parse tree */

        if (Prev == ExternalOp)
        {
            /* External was the first child node */

            ParentOp->Asl.Child = ExternalOp->Asl.Next;
        }

        Prev->Asl.Next = ExternalOp->Asl.Next;
        ExternalOp->Asl.Next = NULL;
        ExternalOp->Asl.Parent = Gbl_ExternalsListHead;

        /* Point the External to the next in the list */

        if (NextOp->Asl.Next)
        {
            ExternalOp->Asl.Next = NextOp->Asl.Next->Asl.Child;
        }

        NextOp = NextOp->Asl.Next;
    }

    /*
     * Loop again to remove MethodObj Externals for which
     * a MethodCall was not found (dead external reference)
     */
    Prev = Gbl_ExternalsListHead->Asl.Child;
    Next = Prev;
    while (Next)
    {
        ObjType = (ACPI_OBJECT_TYPE)
            Next->Asl.Child->Asl.Next->Asl.Value.Integer;

        if (ObjType == ACPI_TYPE_METHOD &&
            !(Next->Asl.CompileFlags & NODE_VISITED))
        {
            if (Next == Prev)
            {
                Gbl_ExternalsListHead->Asl.Child = Next->Asl.Next;
                Next->Asl.Next = NULL;
                Prev = Gbl_ExternalsListHead->Asl.Child;
                Next = Prev;
                continue;
            }
            else
            {
                Prev->Asl.Next = Next->Asl.Next;
                Next->Asl.Next = NULL;
                Next = Prev->Asl.Next;
                continue;
            }
        }

        Prev = Next;
        Next = Next->Asl.Next;
    }

    /* If list is now empty, don't bother to make If (0) block */

    if (!Gbl_ExternalsListHead->Asl.Child)
    {
        return;
    }

    /* Convert Gbl_ExternalsListHead parent to If(). */

    Gbl_ExternalsListHead->Asl.ParseOpcode = PARSEOP_IF;
    Gbl_ExternalsListHead->Asl.AmlOpcode = AML_IF_OP;
    Gbl_ExternalsListHead->Asl.CompileFlags = NODE_AML_PACKAGE;
    UtSetParseOpName (Gbl_ExternalsListHead);

    /* Create a Zero op for the If predicate */

    PredicateOp = TrAllocateNode (PARSEOP_ZERO);
    PredicateOp->Asl.AmlOpcode = AML_ZERO_OP;

    PredicateOp->Asl.Parent = Gbl_ExternalsListHead;
    PredicateOp->Asl.Child = NULL;
    PredicateOp->Asl.Next = Gbl_ExternalsListHead->Asl.Child;
    Gbl_ExternalsListHead->Asl.Child = PredicateOp;

    /* Set line numbers (for listings, etc.) */

    Gbl_ExternalsListHead->Asl.LineNumber = 0;
    Gbl_ExternalsListHead->Asl.LogicalLineNumber = 0;

    PredicateOp->Asl.LineNumber = 0;
    PredicateOp->Asl.LogicalLineNumber = 0;

    /* Insert block back in the list */

    Prev = DefinitionBlockOp->Asl.Child;
    Next = Prev;

    /* Find last default arg */

    for (i = 0; i < 6; i++)
    {
        Prev = Next;
        Next = Prev->Asl.Next;
    }

    if (Next)
    {
        /* Definition Block is not empty */

        Gbl_ExternalsListHead->Asl.Next = Next;
    }
    else
    {
        /* Definition Block is empty. */

        Gbl_ExternalsListHead->Asl.Next = NULL;
    }

    Prev->Asl.Next = Gbl_ExternalsListHead;
    Gbl_ExternalsListHead->Asl.Parent = Prev->Asl.Parent;
}
