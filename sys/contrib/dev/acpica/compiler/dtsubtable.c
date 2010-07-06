/******************************************************************************
 *
 * Module Name: dtsubtable.c - handling of subtables within ACPI tables
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2010, Intel Corp.
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

#define __DTSUBTABLE_C__

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


    Subtable = UtLocalCalloc (sizeof (DT_SUBTABLE));

    /* Create a new buffer for the subtable data */

    Subtable->Buffer = UtLocalCalloc (Length);
    ACPI_MEMCPY (Subtable->Buffer, Buffer, Length);

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


    /* Walk entire Info table; Null name terminates */

    for (; Info->Name; Info++)
    {
        ByteLength += DtGetFieldLength (Field, Info);
    }

    return (ByteLength);
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

    ACPI_MEMCPY (Subtable->LengthField, &Subtable->TotalLength,
        Subtable->SizeOfLengthField);
}
