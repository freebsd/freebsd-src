/******************************************************************************
 *
 * Module Name: asllistsup - Listing file support utilities
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2017, Intel Corp.
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

#include "aslcompiler.h"
#include "aslcompiler.y.h"


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslistsup")


/*******************************************************************************
 *
 * FUNCTION:    LsDumpAscii
 *
 * PARAMETERS:  FileId          - ID of current listing file
 *              Count           - Number of bytes to convert
 *              Buffer          - Buffer of bytes to convert
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert hex bytes to ascii
 *
 ******************************************************************************/

void
LsDumpAscii (
    UINT32                  FileId,
    UINT32                  Count,
    UINT8                   *Buffer)
{
    UINT8                   BufChar;
    UINT32                  i;


    FlPrintFile (FileId, "    \"");
    for (i = 0; i < Count; i++)
    {
        BufChar = Buffer[i];
        if (isprint (BufChar))
        {
            FlPrintFile (FileId, "%c", BufChar);
        }
        else
        {
            /* Not a printable character, just put out a dot */

            FlPrintFile (FileId, ".");
        }
    }

    FlPrintFile (FileId, "\"");
}


/*******************************************************************************
 *
 * FUNCTION:    LsDumpAsciiInComment
 *
 * PARAMETERS:  FileId          - ID of current listing file
 *              Count           - Number of bytes to convert
 *              Buffer          - Buffer of bytes to convert
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert hex bytes to ascii
 *
 ******************************************************************************/

void
LsDumpAsciiInComment (
    UINT32                  FileId,
    UINT32                  Count,
    UINT8                   *Buffer)
{
    UINT8                   BufChar = 0;
    UINT8                   LastChar;
    UINT32                  i;


    FlPrintFile (FileId, "    \"");
    for (i = 0; i < Count; i++)
    {
        LastChar = BufChar;
        BufChar = Buffer[i];

        if (isprint (BufChar))
        {
            /* Handle embedded C comment sequences */

            if (((LastChar == '*') && (BufChar == '/')) ||
                ((LastChar == '/') && (BufChar == '*')))
            {
                /* Insert a space to break the sequence */

                FlPrintFile (FileId, ".", BufChar);
            }

            FlPrintFile (FileId, "%c", BufChar);
        }
        else
        {
            /* Not a printable character, just put out a dot */

            FlPrintFile (FileId, ".");
        }
    }

    FlPrintFile (FileId, "\"");
}


/*******************************************************************************
 *
 * FUNCTION:    LsCheckException
 *
 * PARAMETERS:  LineNumber          - Current logical (cumulative) line #
 *              FileId              - ID of output listing file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Check if there is an exception for this line, and if there is,
 *              put it in the listing immediately. Handles multiple errors
 *              per line. Gbl_NextError points to the next error in the
 *              sorted (by line #) list of compile errors/warnings.
 *
 ******************************************************************************/

void
LsCheckException (
    UINT32                  LineNumber,
    UINT32                  FileId)
{

    if ((!Gbl_NextError) ||
        (LineNumber < Gbl_NextError->LogicalLineNumber ))
    {
        return;
    }

    /* Handle multiple errors per line */

    if (FileId == ASL_FILE_LISTING_OUTPUT)
    {
        while (Gbl_NextError &&
              (LineNumber >= Gbl_NextError->LogicalLineNumber))
        {
            AePrintException (FileId, Gbl_NextError, "\n[****iasl****]\n");
            Gbl_NextError = Gbl_NextError->Next;
        }

        FlPrintFile (FileId, "\n");
    }
}


/*******************************************************************************
 *
 * FUNCTION:    LsWriteListingHexBytes
 *
 * PARAMETERS:  Buffer          - AML code buffer
 *              Length          - Number of AML bytes to write
 *              FileId          - ID of current listing file.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Write the contents of the AML buffer to the listing file via
 *              the listing buffer. The listing buffer is flushed every 16
 *              AML bytes.
 *
 ******************************************************************************/

void
LsWriteListingHexBytes (
    UINT8                   *Buffer,
    UINT32                  Length,
    UINT32                  FileId)
{
    UINT32                  i;


    /* Transfer all requested bytes */

    for (i = 0; i < Length; i++)
    {
        /* Print line header when buffer is empty */

        if (Gbl_CurrentHexColumn == 0)
        {
            if (Gbl_HasIncludeFiles)
            {
                FlPrintFile (FileId, "%*s", 10, " ");
            }

            switch (FileId)
            {
            case ASL_FILE_LISTING_OUTPUT:

                FlPrintFile (FileId, "%8.8X%s", Gbl_CurrentAmlOffset,
                    ASL_LISTING_LINE_PREFIX);
                break;

            case ASL_FILE_ASM_SOURCE_OUTPUT:

                FlPrintFile (FileId, "    db ");
                break;

            case ASL_FILE_C_SOURCE_OUTPUT:

                FlPrintFile (FileId, "        ");
                break;

            default:

                /* No other types supported */

                return;
            }
        }

        /* Transfer AML byte and update counts */

        Gbl_AmlBuffer[Gbl_CurrentHexColumn] = Buffer[i];

        Gbl_CurrentHexColumn++;
        Gbl_CurrentAmlOffset++;

        /* Flush buffer when it is full */

        if (Gbl_CurrentHexColumn >= HEX_LISTING_LINE_SIZE)
        {
            LsFlushListingBuffer (FileId);
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    LsWriteSourceLines
 *
 * PARAMETERS:  ToLineNumber            -
 *              ToLogicalLineNumber     - Write up to this source line number
 *              FileId                  - ID of current listing file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Read then write source lines to the listing file until we have
 *              reached the specified logical (cumulative) line number. This
 *              automatically echos out comment blocks and other non-AML
 *              generating text until we get to the actual AML-generating line
 *              of ASL code specified by the logical line number.
 *
 ******************************************************************************/

void
LsWriteSourceLines (
    UINT32                  ToLineNumber,
    UINT32                  ToLogicalLineNumber,
    UINT32                  FileId)
{

    /* Nothing to do for these file types */

    if ((FileId == ASL_FILE_ASM_INCLUDE_OUTPUT) ||
        (FileId == ASL_FILE_C_INCLUDE_OUTPUT))
    {
        return;
    }

    Gbl_CurrentLine = ToLogicalLineNumber;

    /* Flush any hex bytes remaining from the last opcode */

    LsFlushListingBuffer (FileId);

    /* Read lines and write them as long as we are not caught up */

    if (Gbl_SourceLine < Gbl_CurrentLine)
    {
        /*
         * If we just completed writing some AML hex bytes, output a linefeed
         * to add some whitespace for readability.
         */
        if (Gbl_HexBytesWereWritten)
        {
            FlPrintFile (FileId, "\n");
            Gbl_HexBytesWereWritten = FALSE;
        }

        if (FileId == ASL_FILE_C_SOURCE_OUTPUT)
        {
            FlPrintFile (FileId, "    /*\n");
        }

        /* Write one line at a time until we have reached the target line # */

        while ((Gbl_SourceLine < Gbl_CurrentLine) &&
                LsWriteOneSourceLine (FileId))
        { ; }

        if (FileId == ASL_FILE_C_SOURCE_OUTPUT)
        {
            FlPrintFile (FileId, "     */");
        }

        FlPrintFile (FileId, "\n");
    }
}


/*******************************************************************************
 *
 * FUNCTION:    LsWriteOneSourceLine
 *
 * PARAMETERS:  FileId          - ID of current listing file
 *
 * RETURN:      FALSE on EOF (input source file), TRUE otherwise
 *
 * DESCRIPTION: Read one line from the input source file and echo it to the
 *              listing file, prefixed with the line number, and if the source
 *              file contains include files, prefixed with the current filename
 *
 ******************************************************************************/

UINT32
LsWriteOneSourceLine (
    UINT32                  FileId)
{
    UINT8                   FileByte;
    UINT32                  Column = 0;
    UINT32                  Index = 16;
    BOOLEAN                 StartOfLine = FALSE;
    BOOLEAN                 ProcessLongLine = FALSE;


    Gbl_SourceLine++;
    Gbl_ListingNode->LineNumber++;

    /* Ignore lines that are completely blank (but count the line above) */

    if (FlReadFile (ASL_FILE_SOURCE_OUTPUT, &FileByte, 1) != AE_OK)
    {
        return (0);
    }
    if (FileByte == '\n')
    {
        return (1);
    }

    /*
     * This is a non-empty line, we will print the entire line with
     * the line number and possibly other prefixes and transforms.
     */

    /* Line prefixes for special files, C and ASM output */

    if (FileId == ASL_FILE_C_SOURCE_OUTPUT)
    {
        FlPrintFile (FileId, "     *");
    }
    if (FileId == ASL_FILE_ASM_SOURCE_OUTPUT)
    {
        FlPrintFile (FileId, "; ");
    }

    if (Gbl_HasIncludeFiles)
    {
        /*
         * This file contains "include" statements, print the current
         * filename and line number within the current file
         */
        FlPrintFile (FileId, "%12s %5d%s",
            Gbl_ListingNode->Filename, Gbl_ListingNode->LineNumber,
            ASL_LISTING_LINE_PREFIX);
    }
    else
    {
        /* No include files, just print the line number */

        FlPrintFile (FileId, "%8u%s", Gbl_SourceLine,
            ASL_LISTING_LINE_PREFIX);
    }

    /* Read the rest of this line (up to a newline or EOF) */

    do
    {
        if (FileId == ASL_FILE_C_SOURCE_OUTPUT)
        {
            if (FileByte == '/')
            {
                FileByte = '*';
            }
        }

        /* Split long input lines for readability in the listing */

        Column++;
        if (Column >= 128)
        {
            if (!ProcessLongLine)
            {
                if ((FileByte != '}') &&
                    (FileByte != '{'))
                {
                    goto WriteByte;
                }

                ProcessLongLine = TRUE;
            }

            if (FileByte == '{')
            {
                FlPrintFile (FileId, "\n%*s{\n", Index, " ");
                StartOfLine = TRUE;
                Index += 4;
                continue;
            }

            else if (FileByte == '}')
            {
                if (!StartOfLine)
                {
                    FlPrintFile (FileId, "\n");
                }

                StartOfLine = TRUE;
                Index -= 4;
                FlPrintFile (FileId, "%*s}\n", Index, " ");
                continue;
            }

            /* Ignore spaces/tabs at the start of line */

            else if ((FileByte == ' ') && StartOfLine)
            {
                continue;
            }

            else if (StartOfLine)
            {
                StartOfLine = FALSE;
                FlPrintFile (FileId, "%*s", Index, " ");
            }

WriteByte:
            FlWriteFile (FileId, &FileByte, 1);
            if (FileByte == '\n')
            {
                /*
                 * This line has been completed.
                 * Check if an error occurred on this source line during the compile.
                 * If so, we print the error message after the source line.
                 */
                LsCheckException (Gbl_SourceLine, FileId);
                return (1);
            }
        }
        else
        {
            FlWriteFile (FileId, &FileByte, 1);
            if (FileByte == '\n')
            {
                /*
                 * This line has been completed.
                 * Check if an error occurred on this source line during the compile.
                 * If so, we print the error message after the source line.
                 */
                LsCheckException (Gbl_SourceLine, FileId);
                return (1);
            }
        }

    } while (FlReadFile (ASL_FILE_SOURCE_OUTPUT, &FileByte, 1) == AE_OK);

    /* EOF on the input file was reached */

    return (0);
}


/*******************************************************************************
 *
 * FUNCTION:    LsFlushListingBuffer
 *
 * PARAMETERS:  FileId          - ID of the listing file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Flush out the current contents of the 16-byte hex AML code
 *              buffer. Usually called at the termination of a single line
 *              of source code or when the buffer is full.
 *
 ******************************************************************************/

void
LsFlushListingBuffer (
    UINT32                  FileId)
{
    UINT32                  i;


    if (Gbl_CurrentHexColumn == 0)
    {
        return;
    }

    /* Write the hex bytes */

    switch (FileId)
    {
    case ASL_FILE_LISTING_OUTPUT:

        for (i = 0; i < Gbl_CurrentHexColumn; i++)
        {
            FlPrintFile (FileId, "%2.2X ", Gbl_AmlBuffer[i]);
        }

        for (i = 0; i < ((HEX_LISTING_LINE_SIZE - Gbl_CurrentHexColumn) * 3); i++)
        {
            FlWriteFile (FileId, ".", 1);
        }

        /* Write the ASCII character associated with each of the bytes */

        LsDumpAscii (FileId, Gbl_CurrentHexColumn, Gbl_AmlBuffer);
        break;


    case ASL_FILE_ASM_SOURCE_OUTPUT:

        for (i = 0; i < Gbl_CurrentHexColumn; i++)
        {
            if (i > 0)
            {
                FlPrintFile (FileId, ",");
            }

            FlPrintFile (FileId, "0%2.2Xh", Gbl_AmlBuffer[i]);
        }

        for (i = 0; i < ((HEX_LISTING_LINE_SIZE - Gbl_CurrentHexColumn) * 5); i++)
        {
            FlWriteFile (FileId, " ", 1);
        }

        FlPrintFile (FileId, "  ;%8.8X",
            Gbl_CurrentAmlOffset - HEX_LISTING_LINE_SIZE);

        /* Write the ASCII character associated with each of the bytes */

        LsDumpAscii (FileId, Gbl_CurrentHexColumn, Gbl_AmlBuffer);
        break;


    case ASL_FILE_C_SOURCE_OUTPUT:

        for (i = 0; i < Gbl_CurrentHexColumn; i++)
        {
            FlPrintFile (FileId, "0x%2.2X,", Gbl_AmlBuffer[i]);
        }

        /* Pad hex output with spaces if line is shorter than max line size */

        for (i = 0; i < ((HEX_LISTING_LINE_SIZE - Gbl_CurrentHexColumn) * 5); i++)
        {
            FlWriteFile (FileId, " ", 1);
        }

        /* AML offset for the start of the line */

        FlPrintFile (FileId, "    /* %8.8X",
            Gbl_CurrentAmlOffset - Gbl_CurrentHexColumn);

        /* Write the ASCII character associated with each of the bytes */

        LsDumpAsciiInComment (FileId, Gbl_CurrentHexColumn, Gbl_AmlBuffer);
        FlPrintFile (FileId, " */");
        break;

    default:

        /* No other types supported */

        return;
    }

    FlPrintFile (FileId, "\n");

    Gbl_CurrentHexColumn = 0;
    Gbl_HexBytesWereWritten = TRUE;
}


/*******************************************************************************
 *
 * FUNCTION:    LsPushNode
 *
 * PARAMETERS:  Filename        - Pointer to the include filename
 *
 * RETURN:      None
 *
 * DESCRIPTION: Push a listing node on the listing/include file stack. This
 *              stack enables tracking of include files (infinitely nested)
 *              and resumption of the listing of the parent file when the
 *              include file is finished.
 *
 ******************************************************************************/

void
LsPushNode (
    char                    *Filename)
{
    ASL_LISTING_NODE        *Lnode;


    /* Create a new node */

    Lnode = UtLocalCalloc (sizeof (ASL_LISTING_NODE));

    /* Initialize */

    Lnode->Filename = Filename;
    Lnode->LineNumber = 0;

    /* Link (push) */

    Lnode->Next = Gbl_ListingNode;
    Gbl_ListingNode = Lnode;
}


/*******************************************************************************
 *
 * FUNCTION:    LsPopNode
 *
 * PARAMETERS:  None
 *
 * RETURN:      List head after current head is popped off
 *
 * DESCRIPTION: Pop the current head of the list, free it, and return the
 *              next node on the stack (the new current node).
 *
 ******************************************************************************/

ASL_LISTING_NODE *
LsPopNode (
    void)
{
    ASL_LISTING_NODE        *Lnode;


    /* Just grab the node at the head of the list */

    Lnode = Gbl_ListingNode;
    if ((!Lnode) ||
        (!Lnode->Next))
    {
        AslError (ASL_ERROR, ASL_MSG_COMPILER_INTERNAL, NULL,
            "Could not pop empty listing stack");
        return (Gbl_ListingNode);
    }

    Gbl_ListingNode = Lnode->Next;
    ACPI_FREE (Lnode);

    /* New "Current" node is the new head */

    return (Gbl_ListingNode);
}
