/*******************************************************************************
 *
 * Module Name: dmnames - AML disassembler, names, namestrings, pathnames
 *
 ******************************************************************************/

/*
 * Copyright (C) 2000 - 2014, Intel Corp.
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

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acdisasm.h>


#ifdef ACPI_DISASSEMBLER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dmnames")

/* Local prototypes */

#ifdef ACPI_OBSOLETE_FUNCTIONS
void
AcpiDmDisplayPath (
    ACPI_PARSE_OBJECT       *Op);
#endif


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpName
 *
 * PARAMETERS:  Name            - 4 character ACPI name
 *
 * RETURN:      Final length of name
 *
 * DESCRIPTION: Dump an ACPI name, minus any trailing underscores.
 *
 ******************************************************************************/

UINT32
AcpiDmDumpName (
    UINT32                  Name)
{
    UINT32                  i;
    UINT32                  Length;
    char                    NewName[4];


    /* Copy name locally in case the original name is not writeable */

    *ACPI_CAST_PTR (UINT32, &NewName[0]) = Name;

    /* Ensure that the name is printable, even if we have to fix it */

    AcpiUtRepairName (NewName);

    /* Remove all trailing underscores from the name */

    Length = ACPI_NAME_SIZE;
    for (i = (ACPI_NAME_SIZE - 1); i != 0; i--)
    {
        if (NewName[i] == '_')
        {
            Length--;
        }
        else
        {
            break;
        }
    }

    /* Dump the name, up to the start of the trailing underscores */

    for (i = 0; i < Length; i++)
    {
        AcpiOsPrintf ("%c", NewName[i]);
    }

    return (Length);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsDisplayObjectPathname
 *
 * PARAMETERS:  WalkState       - Current walk state
 *              Op              - Object whose pathname is to be obtained
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Diplay the pathname associated with a named object. Two
 *              versions. One searches the parse tree (for parser-only
 *              applications suchas AcpiDump), and the other searches the
 *              ACPI namespace (the parse tree is probably deleted)
 *
 ******************************************************************************/

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

    Node = Op->Common.Node;
    if (!Node)
    {
        /* Node not defined in this scope, look it up */

        Status = AcpiNsLookup (WalkState->ScopeInfo, Op->Common.Value.String,
                    ACPI_TYPE_ANY, ACPI_IMODE_EXECUTE, ACPI_NS_SEARCH_PARENT,
                    WalkState, &(Node));

        if (ACPI_FAILURE (Status))
        {
            /*
             * We can't get the pathname since the object
             * is not in the namespace. This can happen during single
             * stepping where a dynamic named object is *about* to be created.
             */
            AcpiOsPrintf ("  [Path not found]");
            goto Exit;
        }

        /* Save it for next time. */

        Op->Common.Node = Node;
    }

    /* Convert NamedDesc/handle to a full pathname */

    Buffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
    Status = AcpiNsHandleToPathname (Node, &Buffer);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("****Could not get pathname****)");
        goto Exit;
    }

    AcpiOsPrintf ("  (Path %s)", (char *) Buffer.Pointer);
    ACPI_FREE (Buffer.Pointer);


Exit:
    /* Restore the debug level */

    AcpiDbgLevel = DebugLevel;
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmNamestring
 *
 * PARAMETERS:  Name                - ACPI Name string to store
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode and dump an ACPI namestring. Handles prefix characters
 *
 ******************************************************************************/

void
AcpiDmNamestring (
    char                    *Name)
{
    UINT32                  SegCount;


    if (!Name)
    {
        return;
    }

    /* Handle all Scope Prefix operators */

    while (ACPI_IS_ROOT_PREFIX (ACPI_GET8 (Name)) ||
           ACPI_IS_PARENT_PREFIX (ACPI_GET8 (Name)))
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

        AcpiDmDumpName (*ACPI_CAST_PTR (UINT32, Name));

        SegCount--;
        if (SegCount)
        {
            /* Not last name, append dot separator */

            AcpiOsPrintf (".");
        }
        Name += ACPI_NAME_SIZE;
    }
}


#ifdef ACPI_OBSOLETE_FUNCTIONS
/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDisplayPath
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
AcpiDmDisplayPath (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *Prev;
    ACPI_PARSE_OBJECT       *Search;
    UINT32                  Name;
    BOOLEAN                 DoDot = FALSE;
    ACPI_PARSE_OBJECT       *NamePath;
    const ACPI_OPCODE_INFO  *OpInfo;


    /* We are only interested in named objects */

    OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
    if (!(OpInfo->Flags & AML_NSNODE))
    {
        return;
    }

    if (OpInfo->Flags & AML_CREATE)
    {
        /* Field creation - check for a fully qualified namepath */

        if (Op->Common.AmlOpcode == AML_CREATE_FIELD_OP)
        {
            NamePath = AcpiPsGetArg (Op, 3);
        }
        else
        {
            NamePath = AcpiPsGetArg (Op, 2);
        }

        if ((NamePath) &&
            (NamePath->Common.Value.String) &&
            (ACPI_IS_ROOT_PREFIX (NamePath->Common.Value.String[0])))
        {
            AcpiDmNamestring (NamePath->Common.Value.String);
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
            if (Search->Common.Parent == Prev)
            {
                break;
            }

            /* Go up one level */

            Search = Search->Common.Parent;
        }

        if (Prev)
        {
            OpInfo = AcpiPsGetOpcodeInfo (Search->Common.AmlOpcode);
            if (!(OpInfo->Flags & AML_FIELD))
            {
                /* Below root scope, append scope name */

                if (DoDot)
                {
                    /* Append dot */

                    AcpiOsPrintf (".");
                }

                if (OpInfo->Flags & AML_CREATE)
                {
                    if (Op->Common.AmlOpcode == AML_CREATE_FIELD_OP)
                    {
                        NamePath = AcpiPsGetArg (Op, 3);
                    }
                    else
                    {
                        NamePath = AcpiPsGetArg (Op, 2);
                    }

                    if ((NamePath) &&
                        (NamePath->Common.Value.String))
                    {
                        AcpiDmDumpName (NamePath->Common.Value.String);
                    }
                }
                else
                {
                    Name = AcpiPsGetName (Search);
                    AcpiDmDumpName ((char *) &Name);
                }

                DoDot = TRUE;
            }
        }
        Prev = Search;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmValidateName
 *
 * PARAMETERS:  Name            - 4 character ACPI name
 *
 * RETURN:      None
 *
 * DESCRIPTION: Lookup the name
 *
 ******************************************************************************/

void
AcpiDmValidateName (
    char                    *Name,
    ACPI_PARSE_OBJECT       *Op)
{

    if ((!Name) ||
        (!Op->Common.Parent))
    {
        return;
    }

    if (!Op->Common.Node)
    {
        AcpiOsPrintf (
            " /**** Name not found or not accessible from this scope ****/ ");
    }

    ACPI_PARSE_OBJECT       *TargetOp;


    if ((!Name) ||
        (!Op->Common.Parent))
    {
        return;
    }

    TargetOp = AcpiPsFind (Op, Name, 0, 0);
    if (!TargetOp)
    {
        /*
         * Didn't find the name in the parse tree. This may be
         * a problem, or it may simply be one of the predefined names
         * (such as _OS_). Rather than worry about looking up all
         * the predefined names, just display the name as given
         */
        AcpiOsPrintf (
            " /**** Name not found or not accessible from this scope ****/ ");
    }
}
#endif

#endif
