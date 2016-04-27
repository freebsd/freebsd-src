/******************************************************************************
 *
 * Module Name: dtsubtable.c - handling of subtables within ACPI tables
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
#include <contrib/dev/acpica/compiler/dtcompiler.h>

#define _COMPONENT          DT_COMPILER
        ACPI_MODULE_NAME    ("dtsubtable")


/******************************************************************************
 *
 * FUNCTION:    DtCreateSubtable
 *
 * PARAMETERS:  Buffer              - Input buffer
 *              Length              - Buffer length
 *              RetSubtable         - Returned newly created subtable
 *
 * RETURN:      None
 *
 * DESCRIPTION: Create a subtable that is not listed with ACPI_DMTABLE_INFO
 *              For example, FACS has 24 bytes reserved at the end
 *              and it's not listed at AcpiDmTableInfoFacs
 *
 *****************************************************************************/

void
DtCreateSubtable (
    UINT8                   *Buffer,
    UINT32                  Length,
    DT_SUBTABLE             **RetSubtable)
{
    DT_SUBTABLE             *Subtable;
    char                    *String;


    Subtable = UtSubtableCacheCalloc ();

    /* Create a new buffer for the subtable data */

    String = UtStringCacheCalloc (Length);
    Subtable->Buffer = ACPI_CAST_PTR (UINT8, String);
    memcpy (Subtable->Buffer, Buffer, Length);

    Subtable->Length = Length;
    Subtable->TotalLength = Length;

    *RetSubtable = Subtable;
}


/******************************************************************************
 *
 * FUNCTION:    DtInsertSubtable
 *
 * PARAMETERS:  ParentTable         - The Parent of the new subtable
 *              Subtable            - The new subtable to insert
 *
 * RETURN:      None
 *
 * DESCRIPTION: Insert the new subtable to the parent table
 *
 *****************************************************************************/

void
DtInsertSubtable (
    DT_SUBTABLE             *ParentTable,
    DT_SUBTABLE             *Subtable)
{
    DT_SUBTABLE             *ChildTable;


    Subtable->Peer = NULL;
    Subtable->Parent = ParentTable;
    Subtable->Depth = ParentTable->Depth + 1;

    /* Link the new entry into the child list */

    if (!ParentTable->Child)
    {
        ParentTable->Child = Subtable;
    }
    else
    {
        /* Walk to the end of the child list */

        ChildTable = ParentTable->Child;
        while (ChildTable->Peer)
        {
            ChildTable = ChildTable->Peer;
        }

        /* Add new subtable at the end of the child list */

        ChildTable->Peer = Subtable;
    }
}


/******************************************************************************
 *
 * FUNCTION:    DtPushSubtable
 *
 * PARAMETERS:  Subtable            - Subtable to push
 *
 * RETURN:      None
 *
 * DESCRIPTION: Push a subtable onto a subtable stack
 *
 *****************************************************************************/

void
DtPushSubtable (
    DT_SUBTABLE             *Subtable)
{

    Subtable->StackTop = Gbl_SubtableStack;
    Gbl_SubtableStack = Subtable;
}


/******************************************************************************
 *
 * FUNCTION:    DtPopSubtable
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Pop a subtable from a subtable stack. Uses global SubtableStack
 *
 *****************************************************************************/

void
DtPopSubtable (
    void)
{
    DT_SUBTABLE             *Subtable;


    Subtable = Gbl_SubtableStack;

    if (Subtable)
    {
        Gbl_SubtableStack = Subtable->StackTop;
    }
}


/******************************************************************************
 *
 * FUNCTION:    DtPeekSubtable
 *
 * PARAMETERS:  None
 *
 * RETURN:      The subtable on top of stack
 *
 * DESCRIPTION: Get the subtable on top of stack
 *
 *****************************************************************************/

DT_SUBTABLE *
DtPeekSubtable (
    void)
{

    return (Gbl_SubtableStack);
}


/******************************************************************************
 *
 * FUNCTION:    DtGetNextSubtable
 *
 * PARAMETERS:  ParentTable         - Parent table whose children we are
 *                                    getting
 *              ChildTable          - Previous child that was found.
 *                                    The NEXT child will be returned
 *
 * RETURN:      Pointer to the NEXT child or NULL if none is found.
 *
 * DESCRIPTION: Return the next peer subtable within the tree.
 *
 *****************************************************************************/

DT_SUBTABLE *
DtGetNextSubtable (
    DT_SUBTABLE             *ParentTable,
    DT_SUBTABLE             *ChildTable)
{
    ACPI_FUNCTION_ENTRY ();


    if (!ChildTable)
    {
        /* It's really the parent's _scope_ that we want */

        return (ParentTable->Child);
    }

    /* Otherwise just return the next peer (NULL if at end-of-list) */

    return (ChildTable->Peer);
}


/******************************************************************************
 *
 * FUNCTION:    DtGetParentSubtable
 *
 * PARAMETERS:  Subtable            - Current subtable
 *
 * RETURN:      Parent of the given subtable
 *
 * DESCRIPTION: Get the parent of the given subtable in the tree
 *
 *****************************************************************************/

DT_SUBTABLE *
DtGetParentSubtable (
    DT_SUBTABLE             *Subtable)
{

    if (!Subtable)
    {
        return (NULL);
    }

    return (Subtable->Parent);
}


/******************************************************************************
 *
 * FUNCTION:    DtGetSubtableLength
 *
 * PARAMETERS:  Field               - Current field list pointer
 *              Info                - Data table info
 *
 * RETURN:      Subtable length
 *
 * DESCRIPTION: Get length of bytes needed to compile the subtable
 *
 *****************************************************************************/

UINT32
DtGetSubtableLength (
    DT_FIELD                *Field,
    ACPI_DMTABLE_INFO       *Info)
{
    UINT32                  ByteLength = 0;
    UINT8                   Step;
    UINT8                   i;


    /* Walk entire Info table; Null name terminates */

    for (; Info->Name; Info++)
    {
        if (Info->Opcode == ACPI_DMT_EXTRA_TEXT)
        {
            continue;
        }

        if (!Field)
        {
            goto Error;
        }

        ByteLength += DtGetFieldLength (Field, Info);

        switch (Info->Opcode)
        {
        case ACPI_DMT_GAS:

            Step = 5;
            break;

        case ACPI_DMT_HESTNTFY:

            Step = 9;
            break;

        case ACPI_DMT_IORTMEM:

            Step = 10;
            break;

        default:

            Step = 1;
            break;
        }

        for (i = 0; i < Step; i++)
        {
            if (!Field)
            {
                goto Error;
            }

            Field = Field->Next;
        }
    }

    return (ByteLength);

Error:
    if (!Field)
    {
        sprintf (MsgBuffer, "Found NULL field - Field name \"%s\" needed",
            Info->Name);
        DtFatal (ASL_MSG_COMPILER_INTERNAL, NULL, MsgBuffer);
    }

    return (ASL_EOF);
}


/******************************************************************************
 *
 * FUNCTION:    DtSetSubtableLength
 *
 * PARAMETERS:  Subtable            - Subtable
 *
 * RETURN:      None
 *
 * DESCRIPTION: Set length of the subtable into its length field
 *
 *****************************************************************************/

void
DtSetSubtableLength (
    DT_SUBTABLE             *Subtable)
{

    if (!Subtable->LengthField)
    {
        return;
    }

    memcpy (Subtable->LengthField, &Subtable->TotalLength,
        Subtable->SizeOfLengthField);
}
