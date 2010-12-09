/******************************************************************************
 *
 * Module Name: dtutils.c - Utility routines for the data table compiler
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

#define __DTUTILS_C__

#include "aslcompiler.h"
#include "dtcompiler.h"
#include "actables.h"

#define _COMPONENT          DT_COMPILER
        ACPI_MODULE_NAME    ("dtutils")

/* Local prototypes */

static void
DtSum (
    DT_SUBTABLE             *Subtable,
    void                    *Context,
    void                    *ReturnValue);


/******************************************************************************
 *
 * FUNCTION:    DtError
 *
 * PARAMETERS:  Level               - Seriousness (Warning/error, etc.)
 *              MessageId           - Index into global message buffer
 *              Op                  - Parse node where error happened
 *              ExtraMessage        - additional error message
 *
 * RETURN:      None
 *
 * DESCRIPTION: Common error interface for data table compiler
 *
 *****************************************************************************/

void
DtError (
    UINT8                   Level,
    UINT8                   MessageId,
    DT_FIELD                *FieldObject,
    char                    *ExtraMessage)
{

    switch (Level)
    {
    case ASL_WARNING2:
    case ASL_WARNING3:
        if (Gbl_WarningLevel < Level)
        {
            return;
        }
        break;

    default:
        break;
    }

    if (FieldObject)
    {
        AslCommonError (Level, MessageId,
            FieldObject->Line,
            FieldObject->Line,
            FieldObject->ByteOffset,
            FieldObject->Column,
            Gbl_Files[ASL_FILE_INPUT].Filename, ExtraMessage);
    }
    else
    {
        AslCommonError (Level, MessageId, 0,
            0, 0, 0, 0, ExtraMessage);
    }
}


/******************************************************************************
 *
 * FUNCTION:    DtNameError
 *
 * PARAMETERS:  Level               - Seriousness (Warning/error, etc.)
 *              MessageId           - Index into global message buffer
 *              Op                  - Parse node where error happened
 *              ExtraMessage        - additional error message
 *
 * RETURN:      None
 *
 * DESCRIPTION: Error interface for named objects
 *
 *****************************************************************************/

void
DtNameError (
    UINT8                   Level,
    UINT8                   MessageId,
    DT_FIELD                *FieldObject,
    char                    *ExtraMessage)
{

    switch (Level)
    {
    case ASL_WARNING2:
    case ASL_WARNING3:
        if (Gbl_WarningLevel < Level)
        {
            return;
        }
        break;

    default:
        break;
    }

    if (FieldObject)
    {
        AslCommonError (Level, MessageId,
            FieldObject->Line,
            FieldObject->Line,
            FieldObject->ByteOffset,
            FieldObject->NameColumn,
            Gbl_Files[ASL_FILE_INPUT].Filename, ExtraMessage);
    }
    else
    {
        AslCommonError (Level, MessageId, 0,
            0, 0, 0, 0, ExtraMessage);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    DtFatal
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump the error log and abort the compiler. Used for serious
 *              compile or I/O errors
 *
 ******************************************************************************/

void
DtFatal (
    UINT8                   MessageId,
    DT_FIELD                *FieldObject,
    char                    *ExtraMessage)
{

    DtError (ASL_ERROR, MessageId, FieldObject, ExtraMessage);

    CmCleanupAndExit ();
    exit (1);
}


/******************************************************************************
 *
 * FUNCTION:    DtStrtoul64
 *
 * PARAMETERS:  String              - Null terminated string
 *              ReturnInteger       - Where the converted integer is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Simple conversion of a string hex integer constant to unsigned
 *              value. Assumes no leading "0x" for the constant.
 *
 * Portability note: The reason this function exists is because a 64-bit
 * sscanf is not available in all environments.
 *
 *****************************************************************************/

ACPI_STATUS
DtStrtoul64 (
    char                    *String,
    UINT64                  *ReturnInteger)
{
    char                    *ThisChar = String;
    UINT32                  ThisDigit;
    UINT64                  ReturnValue = 0;
    int                     DigitCount = 0;


    /* Skip over any white space in the buffer */

    while ((*ThisChar == ' ') || (*ThisChar == '\t'))
    {
        ThisChar++;
    }

    /* Skip leading zeros */

    while ((*ThisChar) == '0')
    {
        ThisChar++;
    }

    /* Convert character-by-character */

    while (*ThisChar)
    {
        if (ACPI_IS_DIGIT (*ThisChar))
        {
            /* Convert ASCII 0-9 to Decimal value */

            ThisDigit = ((UINT8) *ThisChar) - '0';
        }
        else /* Letter */
        {
            ThisDigit = (UINT32) ACPI_TOUPPER (*ThisChar);
            if (!ACPI_IS_XDIGIT ((char) ThisDigit))
            {
                /* Not A-F */

                return (AE_BAD_CHARACTER);
            }

            /* Convert ASCII Hex char (A-F) to value */

            ThisDigit = (ThisDigit - 'A') + 10;
        }

        /* Insert the 4-bit hex digit */

        ReturnValue <<= 4;
        ReturnValue += ThisDigit;

        ThisChar++;
        DigitCount++;
        if (DigitCount > 16)
        {
            /* Value is too large (> 64 bits/8 bytes/16 hex digits) */

            return (AE_LIMIT);
        }
    }

    *ReturnInteger = ReturnValue;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtGetFileSize
 *
 * PARAMETERS:  Handle              - Open file handler
 *
 * RETURN:      Current file size
 *
 * DESCRIPTION: Get the current size of a file. Seek to the EOF and get the
 *              offset. Seek back to the original location.
 *
 *****************************************************************************/

UINT32
DtGetFileSize (
    FILE                    *Handle)
{
    int                     CurrentOffset;
    int                     LastOffset;


    CurrentOffset = ftell (Handle);
    fseek (Handle, 0, SEEK_END);
    LastOffset = ftell (Handle);
    fseek (Handle, CurrentOffset, SEEK_SET);

    return ((UINT32) LastOffset);
}


/******************************************************************************
 *
 * FUNCTION:    DtGetFieldValue
 *
 * PARAMETERS:  Field               - Current field list pointer
 *              Name                - Field name
 *
 * RETURN:      Field value
 *
 * DESCRIPTION: Get field value
 *
 *****************************************************************************/

char *
DtGetFieldValue (
    DT_FIELD                *Field,
    char                    *Name)
{

    /* Search the field list for the name */

    while (Field)
    {
        if (!ACPI_STRCMP (Name, Field->Name))
        {
            return (Field->Value);
        }

        Field = Field->Next;
    }

    return (NULL);
}


/******************************************************************************
 *
 * FUNCTION:    DtGetFieldType
 *
 * PARAMETERS:  Info                - Data table info
 *
 * RETURN:      Field type
 *
 * DESCRIPTION: Get field type
 *
 *****************************************************************************/

UINT8
DtGetFieldType (
    ACPI_DMTABLE_INFO       *Info)
{
    UINT8                   Type;


    /* DT_FLAG means that this is the start of a block of flag bits */
    /* TBD - we can make these a separate opcode later */

    if (Info->Flags & DT_FLAG)
    {
        return (DT_FIELD_TYPE_FLAGS_INTEGER);
    }

    /* Type is based upon the opcode for this field in the info table */

    switch (Info->Opcode)
    {
    case ACPI_DMT_FLAG0:
    case ACPI_DMT_FLAG1:
    case ACPI_DMT_FLAG2:
    case ACPI_DMT_FLAG3:
    case ACPI_DMT_FLAG4:
    case ACPI_DMT_FLAG5:
    case ACPI_DMT_FLAG6:
    case ACPI_DMT_FLAG7:
    case ACPI_DMT_FLAGS0:
    case ACPI_DMT_FLAGS2:
        Type = DT_FIELD_TYPE_FLAG;
        break;

    case ACPI_DMT_NAME4:
    case ACPI_DMT_SIG:
    case ACPI_DMT_NAME6:
    case ACPI_DMT_NAME8:
    case ACPI_DMT_STRING:
        Type = DT_FIELD_TYPE_STRING;
        break;

    case ACPI_DMT_BUFFER:
    case ACPI_DMT_BUF16:
    case ACPI_DMT_PCI_PATH:
        Type = DT_FIELD_TYPE_BUFFER;
        break;

    case ACPI_DMT_GAS:
    case ACPI_DMT_HESTNTFY:
        Type = DT_FIELD_TYPE_INLINE_SUBTABLE;
        break;

    default:
        Type = DT_FIELD_TYPE_INTEGER;
        break;
    }

    return (Type);
}


/******************************************************************************
 *
 * FUNCTION:    DtGetBufferLength
 *
 * PARAMETERS:  Buffer              - List of integers,
 *                                    for example "10 3A 4F 2E"
 *
 * RETURN:      Count of integer
 *
 * DESCRIPTION: Get length of bytes needed to store the integers
 *
 *****************************************************************************/

UINT32
DtGetBufferLength (
    char                    *Buffer)
{
    UINT32                  ByteLength = 0;


    while (*Buffer)
    {
        if (*Buffer == ' ')
        {
            ByteLength++;

            while (*Buffer == ' ')
            {
                Buffer++;
            }
        }

        Buffer++;
    }

    return (++ByteLength);
}


/******************************************************************************
 *
 * FUNCTION:    DtGetFieldLength
 *
 * PARAMETERS:  Field               - Current field list pointer
 *              Info                - Data table info
 *
 * RETURN:      Field length
 *
 * DESCRIPTION: Get length of bytes needed to compile the field
 *
 * Note: This function must remain in sync with AcpiDmDumpTable.
 *
 *****************************************************************************/

UINT32
DtGetFieldLength (
    DT_FIELD                *Field,
    ACPI_DMTABLE_INFO       *Info)
{
    UINT32                  ByteLength = 0;
    char                    *Value;


    /* Length is based upon the opcode for this field in the info table */

    switch (Info->Opcode)
    {
    case ACPI_DMT_FLAG0:
    case ACPI_DMT_FLAG1:
    case ACPI_DMT_FLAG2:
    case ACPI_DMT_FLAG3:
    case ACPI_DMT_FLAG4:
    case ACPI_DMT_FLAG5:
    case ACPI_DMT_FLAG6:
    case ACPI_DMT_FLAG7:
    case ACPI_DMT_FLAGS0:
    case ACPI_DMT_FLAGS2:
        ByteLength = 0;
        break;

    case ACPI_DMT_UINT8:
    case ACPI_DMT_CHKSUM:
    case ACPI_DMT_SPACEID:
    case ACPI_DMT_ACCWIDTH:
    case ACPI_DMT_IVRS:
    case ACPI_DMT_MADT:
    case ACPI_DMT_SRAT:
    case ACPI_DMT_ASF:
    case ACPI_DMT_HESTNTYP:
    case ACPI_DMT_FADTPM:
    case ACPI_DMT_EINJACT:
    case ACPI_DMT_EINJINST:
    case ACPI_DMT_ERSTACT:
    case ACPI_DMT_ERSTINST:
        ByteLength = 1;
        break;

    case ACPI_DMT_UINT16:
    case ACPI_DMT_DMAR:
    case ACPI_DMT_HEST:
    case ACPI_DMT_PCI_PATH:
        ByteLength = 2;
        break;

    case ACPI_DMT_UINT24:
        ByteLength = 3;
        break;

    case ACPI_DMT_UINT32:
    case ACPI_DMT_NAME4:
    case ACPI_DMT_SIG:
        ByteLength = 4;
        break;

    case ACPI_DMT_NAME6:
        ByteLength = 6;
        break;

    case ACPI_DMT_UINT56:
        ByteLength = 7;
        break;

    case ACPI_DMT_UINT64:
    case ACPI_DMT_NAME8:
        ByteLength = 8;
        break;

    case ACPI_DMT_STRING:
        Value = DtGetFieldValue (Field, Info->Name);

        /* TBD: error if Value is NULL? (as below?) */

        ByteLength = ACPI_STRLEN (Value) + 1;
        break;

    case ACPI_DMT_GAS:
        ByteLength = sizeof (ACPI_GENERIC_ADDRESS);
        break;

    case ACPI_DMT_HESTNTFY:
        ByteLength = sizeof (ACPI_HEST_NOTIFY);
        break;

    case ACPI_DMT_BUFFER:
        Value = DtGetFieldValue (Field, Info->Name);
        if (Value)
        {
            ByteLength = DtGetBufferLength (Value);
        }
        else
        {   /* At this point, this is a fatal error */

            sprintf (MsgBuffer, "Expected \"%s\"", Info->Name);
            DtFatal (ASL_MSG_COMPILER_INTERNAL, NULL, MsgBuffer);
        }
        break;

    case ACPI_DMT_BUF16:
        ByteLength = 16;
        break;

    default:
        DtFatal (ASL_MSG_COMPILER_INTERNAL, Field, "Invalid table opcode");
        break;
    }

    return (ByteLength);
}


/******************************************************************************
 *
 * FUNCTION:    DtSum
 *
 * PARAMETERS:  DT_WALK_CALLBACK:
 *              Subtable            - Subtable
 *              Context             - Unused
 *              ReturnValue         - Store the checksum of subtable
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get the checksum of subtable
 *
 *****************************************************************************/

static void
DtSum (
    DT_SUBTABLE             *Subtable,
    void                    *Context,
    void                    *ReturnValue)
{
    UINT8                   Checksum;
    UINT8                   *Sum = ReturnValue;


    Checksum = AcpiTbChecksum (Subtable->Buffer, Subtable->Length);
    *Sum = (UINT8) (*Sum + Checksum);
}


/******************************************************************************
 *
 * FUNCTION:    DtSetTableChecksum
 *
 * PARAMETERS:  ChecksumPointer     - Where to return the checksum
 *
 * RETURN:      None
 *
 * DESCRIPTION: Set checksum of the whole data table into the checksum field
 *
 *****************************************************************************/

void
DtSetTableChecksum (
    UINT8                   *ChecksumPointer)
{
    UINT8                   Checksum = 0;
    UINT8                   OldSum;


    DtWalkTableTree (Gbl_RootTable, DtSum, NULL, &Checksum);

    OldSum = *ChecksumPointer;
    Checksum = (UINT8) (Checksum - OldSum);

    /* Compute the final checksum */

    Checksum = (UINT8) (0 - Checksum);
    *ChecksumPointer = Checksum;
}


/******************************************************************************
 *
 * FUNCTION:    DtSetTableLength
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Walk the subtables and set all the length fields
 *
 *****************************************************************************/

void
DtSetTableLength (
    void)
{
    DT_SUBTABLE             *ParentTable;
    DT_SUBTABLE             *ChildTable;


    ParentTable = Gbl_RootTable;
    ChildTable = NULL;

    if (!ParentTable)
    {
        return;
    }

    DtSetSubtableLength (ParentTable);

    while (1)
    {
        ChildTable = DtGetNextSubtable (ParentTable, ChildTable);
        if (ChildTable)
        {
            if (ChildTable->LengthField)
            {
                DtSetSubtableLength (ChildTable);
            }

            if (ChildTable->Child)
            {
                ParentTable = ChildTable;
                ChildTable = NULL;
            }
            else
            {
                ParentTable->TotalLength += ChildTable->TotalLength;
                if (ParentTable->LengthField)
                {
                    DtSetSubtableLength (ParentTable);
                }
            }
        }
        else
        {
            ChildTable = ParentTable;

            if (ChildTable == Gbl_RootTable)
            {
                break;
            }

            ParentTable = DtGetParentSubtable (ParentTable);

            ParentTable->TotalLength += ChildTable->TotalLength;
            if (ParentTable->LengthField)
            {
                DtSetSubtableLength (ParentTable);
            }
        }
    }
}


/******************************************************************************
 *
 * FUNCTION:    DtWalkTableTree
 *
 * PARAMETERS:  StartTable          - Subtable in the tree where walking begins
 *              UserFunction        - Called during the walk
 *              Context             - Passed to user function
 *              ReturnValue         - The return value of UserFunction
 *
 * RETURN:      None
 *
 * DESCRIPTION: Performs a depth-first walk of the subtable tree
 *
 *****************************************************************************/

void
DtWalkTableTree (
    DT_SUBTABLE             *StartTable,
    DT_WALK_CALLBACK        UserFunction,
    void                    *Context,
    void                    *ReturnValue)
{
    DT_SUBTABLE             *ParentTable;
    DT_SUBTABLE             *ChildTable;


    ParentTable = StartTable;
    ChildTable = NULL;

    if (!ParentTable)
    {
        return;
    }

    UserFunction (ParentTable, Context, ReturnValue);

    while (1)
    {
        ChildTable = DtGetNextSubtable (ParentTable, ChildTable);
        if (ChildTable)
        {
            UserFunction (ChildTable, Context, ReturnValue);

            if (ChildTable->Child)
            {
                ParentTable = ChildTable;
                ChildTable = NULL;
            }
        }
        else
        {
            ChildTable = ParentTable;
            if (ChildTable == Gbl_RootTable)
            {
                break;
            }

            ParentTable = DtGetParentSubtable (ParentTable);

            if (ChildTable->Peer == StartTable)
            {
                break;
            }
        }
    }
}


/******************************************************************************
 *
 * FUNCTION:    DtFreeFieldList
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Free the field list
 *
 *****************************************************************************/

void
DtFreeFieldList (
    void)
{
    DT_FIELD                *Field = Gbl_FieldList;
    DT_FIELD                *NextField;


    /* Walk and free entire field list */

    while (Field)
    {
        NextField = Field->Next; /* Save link */

        if (!(Field->Flags & DT_FIELD_NOT_ALLOCATED))
        {
            ACPI_FREE (Field->Name);
            ACPI_FREE (Field->Value);
        }

        ACPI_FREE (Field);
        Field = NextField;
    }
}
