/******************************************************************************
 *
 * Module Name: dtio.c - File I/O support for data table compiler
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

#define __DTIO_C__

#include "aslcompiler.h"
#include "dtcompiler.h"

#define _COMPONENT          DT_COMPILER
        ACPI_MODULE_NAME    ("dtio")


/******************************************************************************
 *
 * FUNCTION:    DtIsComment
 *
 * PARAMETERS:  Line                - Current source code line
 *
 * RETURN:      TRUE if comment, FALSE otherwise
 *
 * DESCRIPTION: Detect a comment in the source module
 *
 *****************************************************************************/

/* TBD: Temporary: very simple code to detect comments */

static int
DtIsComment(
    char                    *Line)
{

    if (!ACPI_STRNCMP (Line, "/*", 2) ||
        !ACPI_STRNCMP (Line, " *", 2))
    {
        return 1;
    }

    return 0;
}


/******************************************************************************
 *
 * FUNCTION:    DtTrim
 *
 * PARAMETERS:  String              - Current source code line to trim
 *
 * RETURN:      Trimmed line. Must be freed by caller.
 *
 * DESCRIPTION: Trim left and right spaces
 *
 *****************************************************************************/

static char *
DtTrim (
    char                    *String)
{
    char                    *Start;
    char                    *End;
    char                    *ReturnString;
    ACPI_SIZE               Length;


    /* Skip lines that start with a space */

    if (!ACPI_STRCMP (String, " "))
    {
        ReturnString = UtLocalCalloc (1);
        return (ReturnString);
    }

    /* Setup pointers to start and end of input string */

    Start = String;
    End = String + ACPI_STRLEN (String) - 1;

    /* Find first non-whitespace character */

    while ((Start <= End) && ((*Start == ' ') || (*Start == '\t')))
    {
        Start++;
    }

    /* Find last non-space character */

    while (End >= Start)
    {
        if (*End == '\r' || *End == '\n')
        {
            End--;
            continue;
        }

        if (*End != ' ')
        {
            break;
        }

        End--;
    }

    /* Remove any quotes around the string */

    if (*Start == '\"')
    {
        Start++;
    }
    if (*End == '\"')
    {
        End--;
    }

    /* Create the trimmed return string */

    Length = ACPI_PTR_DIFF (End, Start) + 1;
    ReturnString = UtLocalCalloc (Length + 1);
    if (ACPI_STRLEN (Start))
    {
        ACPI_STRNCPY (ReturnString, Start, Length);
    }

    ReturnString[Length] = 0;
    return (ReturnString);
}


/******************************************************************************
 *
 * FUNCTION:    DtLinkField
 *
 * PARAMETERS:  Field               - New field object to link
 *
 * RETURN:      None
 *
 * DESCRIPTION: Link one field name and value to the list
 *
 *****************************************************************************/

static void
DtLinkField (
    DT_FIELD                *Field)
{
    DT_FIELD                *Prev;
    DT_FIELD                *Next;


    Prev = Next = Gbl_FieldList;

    while (Next)
    {
        Prev = Next;
        Next = Next->Next;
    }

    if (Prev)
    {
        Prev->Next = Field;
    }
    else
    {
        Gbl_FieldList = Field;
    }
}


/******************************************************************************
 *
 * FUNCTION:    DtParseLine
 *
 * PARAMETERS:  LineBuffer          - Current source code line
 *              Line                - Current line number in the source
 *              Offset              - Current byte offset of the line
 *
 * RETURN:      None
 *
 * DESCRIPTION: Parse one source line
 *
 *****************************************************************************/

static void
DtParseLine (
    char                    *LineBuffer,
    UINT32                  Line,
    UINT32                  Offset)
{
    char                    *Start;
    char                    *End;
    char                    *TmpName;
    char                    *TmpValue;
    char                    *Name;
    char                    *Value;
    char                    *Colon;
    UINT32                  Length;
    DT_FIELD                *Field;
    UINT32                  Column;
    UINT32                  NameColumn;


    if (!LineBuffer || DtIsComment (LineBuffer))
    {
        return;
    }

    Colon = strchr (LineBuffer, ':');
    if (!Colon || *(Colon - 1) != ' ')
    {
        return;
    }

    Start = LineBuffer;
    End = Colon;

    while (Start < Colon)
    {
        if (*Start == ' ')
        {
            Start++;
            continue;
        }

        /* Found left bracket, go to the right bracket */

        if (*Start == '[')
        {
            while (Start < Colon && *Start != ']')
            {
                Start++;
            }

            if (Start == Colon)
            {
                MYDEBUG ("ERROR: right bracket reaches colon position\n");
                break;
            }

            Start++;
            continue;
        }

        break;
    }

    /*
     * There are two column values. One for the field name,
     * and one for the field value.
     */
    Column = ACPI_PTR_DIFF (Colon, LineBuffer) + 3;
    NameColumn = ACPI_PTR_DIFF (Start, LineBuffer) + 1;

    Length = ACPI_PTR_DIFF (End, Start);

    TmpName = UtLocalCalloc (Length + 1);
    ACPI_STRNCPY (TmpName, Start, Length);
    Name = DtTrim (TmpName);
    ACPI_FREE (TmpName);

    Start = End = (Colon + 1);

    while (*End)
    {
        /* Found left quotation, go to the right quotation and break */

        if (*End == '"')
        {
            End++;
            while (*End && *End != '"')
            {
                End++;
            }

            End++;
            break;
        }

        if (*End == '(' ||
            *End == '<' ||
            *End == '/')
        {
            break;
        }

        End++;
    }

    Length = ACPI_PTR_DIFF (End, Start);
    TmpValue = UtLocalCalloc (Length + 1);
    ACPI_STRNCPY (TmpValue, Start, Length);
    Value = DtTrim (TmpValue);
    ACPI_FREE (TmpValue);

    if (Name && Value)
    {
        Field = UtLocalCalloc (sizeof (DT_FIELD));
        Field->Name = Name;
        Field->Value = Value;
        Field->Line = Line;
        Field->ByteOffset = Offset;
        Field->NameColumn = NameColumn;
        Field->Column = Column;

        DtLinkField (Field);
    }
}


/******************************************************************************
 *
 * FUNCTION:    DtScanFile
 *
 * PARAMETERS:  Handle              - Open file handle for the source file
 *
 * RETURN:      Pointer to start of the constructed parse tree.
 *
 * DESCRIPTION: Scan source file, link all field name and value
 *              to the global parse tree: Gbl_FieldList
 *
 *****************************************************************************/

DT_FIELD *
DtScanFile (
    FILE                    *Handle)
{
    UINT32                  Line = 0;
    UINT32                  Offset = 0;


    /* Get the file size */

    Gbl_InputByteCount = DtGetFileSize (Handle);

    /* Scan line-by-line */

    while (fgets (Gbl_CurrentLineBuffer, ASL_LINE_BUFFER_SIZE, Handle))
    {
        Line++;
        Gbl_CurrentLineNumber++;
        DtParseLine (Gbl_CurrentLineBuffer, Line, Offset);

        Offset = (UINT32) ftell (Handle);
    }

    return (Gbl_FieldList);
}


/*
 * Output functions
 */


/******************************************************************************
 *
 * FUNCTION:    DtWriteBinary
 *
 * PARAMETERS:  DT_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write one subtable of a binary ACPI table
 *
 *****************************************************************************/

static void
DtWriteBinary (
    DT_SUBTABLE             *Subtable,
    void                    *Context,
    void                    *ReturnValue)
{

    FlWriteFile (ASL_FILE_AML_OUTPUT, Subtable->Buffer, Subtable->Length);
}


/******************************************************************************
 *
 * FUNCTION:    DtOutputBinary
 *
 * PARAMETERS:
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Write entire binary ACPI table (result of compilation)
 *
 *****************************************************************************/

void
DtOutputBinary (
    DT_SUBTABLE             *RootTable)
{

    if (!RootTable)
    {
        return;
    }

    /* Walk the entire parse tree, emitting the binary data */

    DtWalkTableTree (RootTable, DtWriteBinary, NULL, NULL);
    Gbl_TableLength = DtGetFileSize (Gbl_Files[ASL_FILE_AML_OUTPUT].Handle);
}
