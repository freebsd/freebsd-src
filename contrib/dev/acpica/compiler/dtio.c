/******************************************************************************
 *
 * Module Name: dtio.c - File I/O support for data table compiler
 *
 *****************************************************************************/

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

#define __DTIO_C__

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/compiler/dtcompiler.h>
#include <contrib/dev/acpica/include/acapps.h>

#define _COMPONENT          DT_COMPILER
        ACPI_MODULE_NAME    ("dtio")


/* Local prototypes */

static char *
DtTrim (
    char                    *String);

static void
DtLinkField (
    DT_FIELD                *Field);

static ACPI_STATUS
DtParseLine (
    char                    *LineBuffer,
    UINT32                  Line,
    UINT32                  Offset);

static void
DtWriteBinary (
    DT_SUBTABLE             *Subtable,
    void                    *Context,
    void                    *ReturnValue);

static void
DtDumpBuffer (
    UINT32                  FileId,
    UINT8                   *Buffer,
    UINT32                  Offset,
    UINT32                  Length);

static void
DtDumpSubtableInfo (
    DT_SUBTABLE             *Subtable,
    void                    *Context,
    void                    *ReturnValue);

static void
DtDumpSubtableTree (
    DT_SUBTABLE             *Subtable,
    void                    *Context,
    void                    *ReturnValue);


/* States for DtGetNextLine */

#define DT_NORMAL_TEXT              0
#define DT_START_QUOTED_STRING      1
#define DT_START_COMMENT            2
#define DT_SLASH_ASTERISK_COMMENT   3
#define DT_SLASH_SLASH_COMMENT      4
#define DT_END_COMMENT              5
#define DT_MERGE_LINES              6
#define DT_ESCAPE_SEQUENCE          7

static UINT32  Gbl_NextLineOffset;


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
        ReturnString = UtStringCacheCalloc (1);
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
    ReturnString = UtStringCacheCalloc (Length + 1);
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
 * RETURN:      Status
 *
 * DESCRIPTION: Parse one source line
 *
 *****************************************************************************/

static ACPI_STATUS
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
    BOOLEAN                 IsNullString = FALSE;


    if (!LineBuffer)
    {
        return (AE_OK);
    }

    /* All lines after "Raw Table Data" are ingored */

    if (strstr (LineBuffer, ACPI_RAW_TABLE_DATA_HEADER))
    {
        return (AE_NOT_FOUND);
    }

    Colon = strchr (LineBuffer, ':');
    if (!Colon)
    {
        return (AE_OK);
    }

    Start = LineBuffer;
    End = Colon;

    while (Start < Colon)
    {
        if (*Start == '[')
        {
            /* Found left bracket, go to the right bracket */

            while (Start < Colon && *Start != ']')
            {
                Start++;
            }
        }
        else if (*Start != ' ')
        {
            break;
        }

        Start++;
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

            /* Check for an explicit null string */

            if (*End == '"')
            {
                IsNullString = TRUE;
            }
            while (*End && (*End != '"'))
            {
                End++;
            }

            End++;
            break;
        }

        /*
         * Special "comment" fields at line end, ignore them.
         * Note: normal slash-slash and slash-asterisk comments are
         * stripped already by the DtGetNextLine parser.
         *
         * TBD: Perhaps DtGetNextLine should parse the following type
         * of comments also.
         */
        if (*End == '[')
        {
            End--;
            break;
        }
        End++;
    }

    Length = ACPI_PTR_DIFF (End, Start);
    TmpValue = UtLocalCalloc (Length + 1);

    ACPI_STRNCPY (TmpValue, Start, Length);
    Value = DtTrim (TmpValue);
    ACPI_FREE (TmpValue);

    /* Create a new field object only if we have a valid value field */

    if ((Value && *Value) || IsNullString)
    {
        Field = UtFieldCacheCalloc ();
        Field->Name = Name;
        Field->Value = Value;
        Field->Line = Line;
        Field->ByteOffset = Offset;
        Field->NameColumn = NameColumn;
        Field->Column = Column;

        DtLinkField (Field);
    }
    /* Else -- Ignore this field, it has no valid data */

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtGetNextLine
 *
 * PARAMETERS:  Handle              - Open file handle for the source file
 *
 * RETURN:      Filled line buffer and offset of start-of-line (ASL_EOF on EOF)
 *
 * DESCRIPTION: Get the next valid source line. Removes all comments.
 *              Ignores empty lines.
 *
 * Handles both slash-asterisk and slash-slash comments.
 * Also, quoted strings, but no escapes within.
 *
 * Line is returned in Gbl_CurrentLineBuffer.
 * Line number in original file is returned in Gbl_CurrentLineNumber.
 *
 *****************************************************************************/

UINT32
DtGetNextLine (
    FILE                    *Handle)
{
    BOOLEAN                 LineNotAllBlanks = FALSE;
    UINT32                  State = DT_NORMAL_TEXT;
    UINT32                  CurrentLineOffset;
    UINT32                  i;
    int                     c;


    for (i = 0; ;)
    {
        /*
         * If line is too long, expand the line buffers. Also increases
         * Gbl_LineBufferSize.
         */
        if (i >= Gbl_LineBufferSize)
        {
            UtExpandLineBuffers ();
        }

        c = getc (Handle);
        if (c == EOF)
        {
            switch (State)
            {
            case DT_START_QUOTED_STRING:
            case DT_SLASH_ASTERISK_COMMENT:

                AcpiOsPrintf ("**** EOF within comment/string %u\n", State);
                break;

            default:

                break;
            }

            /* Standalone EOF is OK */

            if (i == 0)
            {
                return (ASL_EOF);
            }

            /*
             * Received an EOF in the middle of a line. Terminate the
             * line with a newline. The next call to this function will
             * return a standalone EOF. Thus, the upper parsing software
             * never has to deal with an EOF within a valid line (or
             * the last line does not get tossed on the floor.)
             */
            c = '\n';
            State = DT_NORMAL_TEXT;
        }

        switch (State)
        {
        case DT_NORMAL_TEXT:

            /* Normal text, insert char into line buffer */

            Gbl_CurrentLineBuffer[i] = (char) c;
            switch (c)
            {
            case '/':

                State = DT_START_COMMENT;
                break;

            case '"':

                State = DT_START_QUOTED_STRING;
                LineNotAllBlanks = TRUE;
                i++;
                break;

            case '\\':
                /*
                 * The continuation char MUST be last char on this line.
                 * Otherwise, it will be assumed to be a valid ASL char.
                 */
                State = DT_MERGE_LINES;
                break;

            case '\n':

                CurrentLineOffset = Gbl_NextLineOffset;
                Gbl_NextLineOffset = (UINT32) ftell (Handle);
                Gbl_CurrentLineNumber++;

                /*
                 * Exit if line is complete. Ignore empty lines (only \n)
                 * or lines that contain nothing but blanks.
                 */
                if ((i != 0) && LineNotAllBlanks)
                {
                    if ((i + 1) >= Gbl_LineBufferSize)
                    {
                        UtExpandLineBuffers ();
                    }

                    Gbl_CurrentLineBuffer[i+1] = 0; /* Terminate string */
                    return (CurrentLineOffset);
                }

                /* Toss this line and start a new one */

                i = 0;
                LineNotAllBlanks = FALSE;
                break;

            default:

                if (c != ' ')
                {
                    LineNotAllBlanks = TRUE;
                }

                i++;
                break;
            }
            break;

        case DT_START_QUOTED_STRING:

            /* Insert raw chars until end of quoted string */

            Gbl_CurrentLineBuffer[i] = (char) c;
            i++;

            switch (c)
            {
            case '"':

                State = DT_NORMAL_TEXT;
                break;

            case '\\':

                State = DT_ESCAPE_SEQUENCE;
                break;

            case '\n':

                AcpiOsPrintf ("ERROR at line %u: Unterminated quoted string\n",
                    Gbl_CurrentLineNumber++);
                State = DT_NORMAL_TEXT;
                break;

            default:    /* Get next character */

                break;
            }
            break;

        case DT_ESCAPE_SEQUENCE:

            /* Just copy the escaped character. TBD: sufficient for table compiler? */

            Gbl_CurrentLineBuffer[i] = (char) c;
            i++;
            State = DT_START_QUOTED_STRING;
            break;

        case DT_START_COMMENT:

            /* Open comment if this character is an asterisk or slash */

            switch (c)
            {
            case '*':

                State = DT_SLASH_ASTERISK_COMMENT;
                break;

            case '/':

                State = DT_SLASH_SLASH_COMMENT;
                break;

            default:    /* Not a comment */

                i++;    /* Save the preceding slash */
                if (i >= Gbl_LineBufferSize)
                {
                    UtExpandLineBuffers ();
                }

                Gbl_CurrentLineBuffer[i] = (char) c;
                i++;
                State = DT_NORMAL_TEXT;
                break;
            }
            break;

        case DT_SLASH_ASTERISK_COMMENT:

            /* Ignore chars until an asterisk-slash is found */

            switch (c)
            {
            case '\n':

                Gbl_NextLineOffset = (UINT32) ftell (Handle);
                Gbl_CurrentLineNumber++;
                break;

            case '*':

                State = DT_END_COMMENT;
                break;

            default:

                break;
            }
            break;

        case DT_SLASH_SLASH_COMMENT:

            /* Ignore chars until end-of-line */

            if (c == '\n')
            {
                /* We will exit via the NORMAL_TEXT path */

                ungetc (c, Handle);
                State = DT_NORMAL_TEXT;
            }
            break;

        case DT_END_COMMENT:

            /* End comment if this char is a slash */

            switch (c)
            {
            case '/':

                State = DT_NORMAL_TEXT;
                break;

            case '\n':

                CurrentLineOffset = Gbl_NextLineOffset;
                Gbl_NextLineOffset = (UINT32) ftell (Handle);
                Gbl_CurrentLineNumber++;
                break;

            case '*':

                /* Consume all adjacent asterisks */
                break;

            default:

                State = DT_SLASH_ASTERISK_COMMENT;
                break;
            }
            break;

        case DT_MERGE_LINES:

            if (c != '\n')
            {
                /*
                 * This is not a continuation backslash, it is a normal
                 * normal ASL backslash - for example: Scope(\_SB_)
                 */
                i++; /* Keep the backslash that is already in the buffer */

                ungetc (c, Handle);
                State = DT_NORMAL_TEXT;
            }
            else
            {
                /*
                 * This is a continuation line -- a backlash followed
                 * immediately by a newline. Insert a space between the
                 * lines (overwrite the backslash)
                 */
                Gbl_CurrentLineBuffer[i] = ' ';
                i++;

                /* Ignore newline, this will merge the lines */

                CurrentLineOffset = Gbl_NextLineOffset;
                Gbl_NextLineOffset = (UINT32) ftell (Handle);
                Gbl_CurrentLineNumber++;
                State = DT_NORMAL_TEXT;
            }
            break;

        default:

            DtFatal (ASL_MSG_COMPILER_INTERNAL, NULL, "Unknown input state");
            return (ASL_EOF);
        }
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
 * DESCRIPTION: Scan source file, link all field names and values
 *              to the global parse tree: Gbl_FieldList
 *
 *****************************************************************************/

DT_FIELD *
DtScanFile (
    FILE                    *Handle)
{
    ACPI_STATUS             Status;
    UINT32                  Offset;


    ACPI_FUNCTION_NAME (DtScanFile);


    /* Get the file size */

    Gbl_InputByteCount = CmGetFileSize (Handle);
    if (Gbl_InputByteCount == ACPI_UINT32_MAX)
    {
        AslAbort ();
    }

    Gbl_CurrentLineNumber = 0;
    Gbl_CurrentLineOffset = 0;
    Gbl_NextLineOffset = 0;

    /* Scan line-by-line */

    while ((Offset = DtGetNextLine (Handle)) != ASL_EOF)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_PARSE, "Line %2.2u/%4.4X - %s",
            Gbl_CurrentLineNumber, Offset, Gbl_CurrentLineBuffer));

        Status = DtParseLine (Gbl_CurrentLineBuffer, Gbl_CurrentLineNumber, Offset);
        if (Status == AE_NOT_FOUND)
        {
            break;
        }
    }

    /* Dump the parse tree if debug enabled */

    DtDumpFieldList (Gbl_FieldList);
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

    Gbl_TableLength = CmGetFileSize (Gbl_Files[ASL_FILE_AML_OUTPUT].Handle);
    if (Gbl_TableLength == ACPI_UINT32_MAX)
    {
        AslAbort ();
    }
}


/*
 * Listing support
 */

/******************************************************************************
 *
 * FUNCTION:    DtDumpBuffer
 *
 * PARAMETERS:  FileID              - Where to write buffer data
 *              Buffer              - Buffer to dump
 *              Offset              - Offset in current table
 *              Length              - Buffer Length
 *
 * RETURN:      None
 *
 * DESCRIPTION: Another copy of DumpBuffer routine (unfortunately).
 *
 * TBD: merge dump buffer routines
 *
 *****************************************************************************/

static void
DtDumpBuffer (
    UINT32                  FileId,
    UINT8                   *Buffer,
    UINT32                  Offset,
    UINT32                  Length)
{
    UINT32                  i;
    UINT32                  j;
    UINT8                   BufChar;


    FlPrintFile (FileId, "Output: [%3.3Xh %4.4d %3d] ",
        Offset, Offset, Length);

    i = 0;
    while (i < Length)
    {
        if (i >= 16)
        {
            FlPrintFile (FileId, "%24s", "");
        }

        /* Print 16 hex chars */

        for (j = 0; j < 16;)
        {
            if (i + j >= Length)
            {
                /* Dump fill spaces */

                FlPrintFile (FileId, "   ");
                j++;
                continue;
            }

            FlPrintFile (FileId, "%02X ", Buffer[i+j]);
            j++;
        }

        FlPrintFile (FileId, " ");
        for (j = 0; j < 16; j++)
        {
            if (i + j >= Length)
            {
                FlPrintFile (FileId, "\n\n");
                return;
            }

            BufChar = Buffer[(ACPI_SIZE) i + j];
            if (ACPI_IS_PRINT (BufChar))
            {
                FlPrintFile (FileId, "%c", BufChar);
            }
            else
            {
                FlPrintFile (FileId, ".");
            }
        }

        /* Done with that line. */

        FlPrintFile (FileId, "\n");
        i += 16;
    }

    FlPrintFile (FileId, "\n\n");
}


/******************************************************************************
 *
 * FUNCTION:    DtDumpFieldList
 *
 * PARAMETERS:  Field               - Root field
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump the entire field list
 *
 *****************************************************************************/

void
DtDumpFieldList (
    DT_FIELD                *Field)
{

    if (!Gbl_DebugFlag || !Field)
    {
        return;
    }

    DbgPrint (ASL_DEBUG_OUTPUT,  "\nField List:\n"
        "LineNo   ByteOff  NameCol  Column   TableOff "
        "Flags    %32s : %s\n\n", "Name", "Value");
    while (Field)
    {
        DbgPrint (ASL_DEBUG_OUTPUT,
            "%.08X %.08X %.08X %.08X %.08X %.08X %32s : %s\n",
            Field->Line, Field->ByteOffset, Field->NameColumn,
            Field->Column, Field->TableOffset, Field->Flags,
            Field->Name, Field->Value);

        Field = Field->Next;
    }

    DbgPrint (ASL_DEBUG_OUTPUT,  "\n\n");
}


/******************************************************************************
 *
 * FUNCTION:    DtDumpSubtableInfo, DtDumpSubtableTree
 *
 * PARAMETERS:  DT_WALK_CALLBACK
 *
 * RETURN:      None
 *
 * DESCRIPTION: Info - dump a subtable tree entry with extra information.
 *              Tree - dump a subtable tree formatted by depth indentation.
 *
 *****************************************************************************/

static void
DtDumpSubtableInfo (
    DT_SUBTABLE             *Subtable,
    void                    *Context,
    void                    *ReturnValue)
{

    DbgPrint (ASL_DEBUG_OUTPUT,
        "[%.04X] %.08X %.08X %.08X %.08X %.08X %p %p %p\n",
        Subtable->Depth, Subtable->Length, Subtable->TotalLength,
        Subtable->SizeOfLengthField, Subtable->Flags, Subtable,
        Subtable->Parent, Subtable->Child, Subtable->Peer);
}

static void
DtDumpSubtableTree (
    DT_SUBTABLE             *Subtable,
    void                    *Context,
    void                    *ReturnValue)
{

    DbgPrint (ASL_DEBUG_OUTPUT,
        "[%.04X] %*s%08X (%.02X) - (%.02X)\n",
        Subtable->Depth, (4 * Subtable->Depth), " ",
        Subtable, Subtable->Length, Subtable->TotalLength);
}


/******************************************************************************
 *
 * FUNCTION:    DtDumpSubtableList
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump the raw list of subtables with information, and also
 *              dump the subtable list in formatted tree format. Assists with
 *              the development of new table code.
 *
 *****************************************************************************/

void
DtDumpSubtableList (
    void)
{

    if (!Gbl_DebugFlag || !Gbl_RootTable)
    {
        return;
    }

    DbgPrint (ASL_DEBUG_OUTPUT,
        "Subtable Info:\n"
        "Depth  Length   TotalLen LenSize  Flags    "
        "This     Parent   Child    Peer\n\n");
    DtWalkTableTree (Gbl_RootTable, DtDumpSubtableInfo, NULL, NULL);

    DbgPrint (ASL_DEBUG_OUTPUT,
        "\nSubtable Tree: (Depth, Subtable, Length, TotalLength)\n\n");
    DtWalkTableTree (Gbl_RootTable, DtDumpSubtableTree, NULL, NULL);

    DbgPrint (ASL_DEBUG_OUTPUT, "\n");
}


/******************************************************************************
 *
 * FUNCTION:    DtWriteFieldToListing
 *
 * PARAMETERS:  Buffer              - Contains the compiled data
 *              Field               - Field node for the input line
 *              Length              - Length of the output data
 *
 * RETURN:      None
 *
 * DESCRIPTION: Write one field to the listing file (if listing is enabled).
 *
 *****************************************************************************/

void
DtWriteFieldToListing (
    UINT8                   *Buffer,
    DT_FIELD                *Field,
    UINT32                  Length)
{
    UINT8                   FileByte;


    if (!Gbl_ListingFlag || !Field)
    {
        return;
    }

    /* Dump the original source line */

    FlPrintFile (ASL_FILE_LISTING_OUTPUT, "Input:  ");
    FlSeekFile (ASL_FILE_INPUT, Field->ByteOffset);

    while (FlReadFile (ASL_FILE_INPUT, &FileByte, 1) == AE_OK)
    {
        FlWriteFile (ASL_FILE_LISTING_OUTPUT, &FileByte, 1);
        if (FileByte == '\n')
        {
            break;
        }
    }

    /* Dump the line as parsed and represented internally */

    FlPrintFile (ASL_FILE_LISTING_OUTPUT, "Parsed: %*s : %.64s",
        Field->Column-4, Field->Name, Field->Value);

    if (strlen (Field->Value) > 64)
    {
        FlPrintFile (ASL_FILE_LISTING_OUTPUT, "...Additional data, length 0x%X\n",
            strlen (Field->Value));
    }
    FlPrintFile (ASL_FILE_LISTING_OUTPUT, "\n");

    /* Dump the hex data that will be output for this field */

    DtDumpBuffer (ASL_FILE_LISTING_OUTPUT, Buffer, Field->TableOffset, Length);
}


/******************************************************************************
 *
 * FUNCTION:    DtWriteTableToListing
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Write the entire compiled table to the listing file
 *              in hex format
 *
 *****************************************************************************/

void
DtWriteTableToListing (
    void)
{
    UINT8                   *Buffer;


    if (!Gbl_ListingFlag)
    {
        return;
    }

    /* Read the entire table from the output file */

    Buffer = UtLocalCalloc (Gbl_TableLength);
    FlSeekFile (ASL_FILE_AML_OUTPUT, 0);
    FlReadFile (ASL_FILE_AML_OUTPUT, Buffer, Gbl_TableLength);

    /* Dump the raw table data */

    AcpiOsRedirectOutput (Gbl_Files[ASL_FILE_LISTING_OUTPUT].Handle);

    AcpiOsPrintf ("\n%s: Length %d (0x%X)\n\n",
        ACPI_RAW_TABLE_DATA_HEADER, Gbl_TableLength, Gbl_TableLength);
    AcpiUtDumpBuffer (Buffer, Gbl_TableLength, DB_BYTE_DISPLAY, 0);

    AcpiOsRedirectOutput (stdout);
    ACPI_FREE (Buffer);
}
