
/******************************************************************************
 *
 * Module Name: asllisting - Listing file generation
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
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
#include "amlcode.h"
#include "acparser.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslisting")

/* Local prototypes */

static void
LsDumpAscii (
    UINT32                  FileId,
    UINT32                  Count,
    UINT8                   *Buffer);

static void
LsDumpAsciiInComment (
    UINT32                  FileId,
    UINT32                  Count,
    UINT8                   *Buffer);

static ACPI_STATUS
LsAmlListingWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static void
LsGenerateListing (
    UINT32                  FileId);

static void
LsPushNode (
    char                    *Filename);

static ASL_LISTING_NODE *
LsPopNode (
    void);

static void
LsCheckException (
    UINT32                  LineNumber,
    UINT32                  FileId);

static void
LsFlushListingBuffer (
    UINT32                  FileId);

static void
LsWriteListingHexBytes (
    UINT8                   *Buffer,
    UINT32                  Length,
    UINT32                  FileId);

static UINT32
LsWriteOneSourceLine (
    UINT32                  FileId);

static void
LsFinishSourceListing (
    UINT32                  FileId);

static void
LsWriteSourceLines (
    UINT32                  ToLineNumber,
    UINT32                  ToLogicalLineNumber,
    UINT32                  FileId);

static void
LsWriteNodeToListing (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  FileId);

static void
LsDoHexOutputC (
    void);

static void
LsDoHexOutputAsm (
    void);

static void
LsDoHexOutputAsl (
    void);

static ACPI_STATUS
LsTreeWriteWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);


/*******************************************************************************
 *
 * FUNCTION:    LsTreeWriteWalk
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Dump entire parse tree, for compiler debug only
 *
 ******************************************************************************/

static ACPI_STATUS
LsTreeWriteWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{

    /* Debug output */

    DbgPrint (ASL_TREE_OUTPUT,
        "%5.5d [%2d]", Op->Asl.LogicalLineNumber, Level);
    UtPrintFormattedName (Op->Asl.ParseOpcode, Level);


    DbgPrint (ASL_TREE_OUTPUT, "\n");
    return (AE_OK);
}


void
LsDumpParseTree (
    void)
{

    if (!Gbl_DebugFlag)
    {
        return;
    }

    DbgPrint (ASL_TREE_OUTPUT, "\nOriginal parse tree from parser:\n\n");
    TrWalkParseTree (RootNode, ASL_WALK_VISIT_DOWNWARD,
        LsTreeWriteWalk, NULL, NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    LsDumpAscii
 *
 * PARAMETERS:  FileId          - ID of current listing file
 *              Count           - Number of bytes to convert
 *              Buffer          - Buffer of bytes to convert
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Convert hex bytes to ascii
 *
 ******************************************************************************/

static void
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
 * RETURN:      None.
 *
 * DESCRIPTION: Convert hex bytes to ascii
 *
 ******************************************************************************/

static void
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
 * FUNCTION:    LsAmlListingWalk
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Process one node during a listing file generation.
 *
 ******************************************************************************/

static ACPI_STATUS
LsAmlListingWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    UINT8                   FileByte;
    UINT32                  i;
    UINT32                  FileId = (UINT32) ACPI_TO_INTEGER (Context);


    LsWriteNodeToListing (Op, FileId);

    if (Op->Asl.CompileFlags & NODE_IS_RESOURCE_DATA)
    {
        /* Buffer is a resource template, don't dump the data all at once */

        return (AE_OK);
    }

    /* Write the hex bytes to the listing file(s) (if requested) */

    for (i = 0; i < Op->Asl.FinalAmlLength; i++)
    {
        if (ACPI_FAILURE (FlReadFile (ASL_FILE_AML_OUTPUT, &FileByte, 1)))
        {
            FlFileError (ASL_FILE_AML_OUTPUT, ASL_MSG_READ);
            AslAbort ();
        }
        LsWriteListingHexBytes (&FileByte, 1, FileId);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    LsGenerateListing
 *
 * PARAMETERS:  FileId      - ID of listing file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Generate a listing file.  This can be one of the several types
 *              of "listings" supported.
 *
 ******************************************************************************/

static void
LsGenerateListing (
    UINT32                  FileId)
{

    /* Start at the beginning of both the source and AML files */

    FlSeekFile (ASL_FILE_SOURCE_OUTPUT, 0);
    FlSeekFile (ASL_FILE_AML_OUTPUT, 0);
    Gbl_SourceLine = 0;
    Gbl_CurrentHexColumn = 0;
    LsPushNode (Gbl_Files[ASL_FILE_INPUT].Filename);

    /* Process all parse nodes */

    TrWalkParseTree (RootNode, ASL_WALK_VISIT_DOWNWARD, LsAmlListingWalk,
                        NULL, (void *) ACPI_TO_POINTER (FileId));

    /* Final processing */

    LsFinishSourceListing (FileId);
}


/*******************************************************************************
 *
 * FUNCTION:    LsDoListings
 *
 * PARAMETERS:  None.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Generate all requested listing files.
 *
 ******************************************************************************/

void
LsDoListings (
    void)
{

    if (Gbl_C_OutputFlag)
    {
        LsGenerateListing (ASL_FILE_C_SOURCE_OUTPUT);
    }

    if (Gbl_ListingFlag)
    {
        LsGenerateListing (ASL_FILE_LISTING_OUTPUT);
    }

    if (Gbl_AsmOutputFlag)
    {
        LsGenerateListing (ASL_FILE_ASM_SOURCE_OUTPUT);
    }

    if (Gbl_C_IncludeOutputFlag)
    {
        LsGenerateListing (ASL_FILE_C_INCLUDE_OUTPUT);
    }

    if (Gbl_AsmIncludeOutputFlag)
    {
        LsGenerateListing (ASL_FILE_ASM_INCLUDE_OUTPUT);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    LsPushNode
 *
 * PARAMETERS:  Filename        - Pointer to the include filename
 *
 * RETURN:      None
 *
 * DESCRIPTION: Push a listing node on the listing/include file stack.  This
 *              stack enables tracking of include files (infinitely nested)
 *              and resumption of the listing of the parent file when the
 *              include file is finished.
 *
 ******************************************************************************/

static void
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

static ASL_LISTING_NODE *
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
        return Gbl_ListingNode;
    }

    Gbl_ListingNode = Lnode->Next;
    ACPI_FREE (Lnode);

    /* New "Current" node is the new head */

    return (Gbl_ListingNode);
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
 *              put it in the listing immediately.  Handles multiple errors
 *              per line.  Gbl_NextError points to the next error in the
 *              sorted (by line #) list of compile errors/warnings.
 *
 ******************************************************************************/

static void
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
 * FUNCTION:    LsFlushListingBuffer
 *
 * PARAMETERS:  FileId          - ID of the listing file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Flush out the current contents of the 16-byte hex AML code
 *              buffer.  Usually called at the termination of a single line
 *              of source code or when the buffer is full.
 *
 ******************************************************************************/

static void
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

        for (i = 0; i < ((HEX_LISTING_LINE_SIZE - Gbl_CurrentHexColumn) * 5); i++)
        {
            FlWriteFile (FileId, " ", 1);
        }

        FlPrintFile (FileId, "    /* %8.8X",
            Gbl_CurrentAmlOffset - HEX_LISTING_LINE_SIZE);

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
 * FUNCTION:    LsWriteListingHexBytes
 *
 * PARAMETERS:  Buffer          - AML code buffer
 *              Length          - Number of AML bytes to write
 *              FileId          - ID of current listing file.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Write the contents of the AML buffer to the listing file via
 *              the listing buffer.  The listing buffer is flushed every 16
 *              AML bytes.
 *
 ******************************************************************************/

static void
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

                FlPrintFile (FileId, "%8.8X....", Gbl_CurrentAmlOffset);
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
 * FUNCTION:    LsWriteOneSourceLine
 *
 * PARAMETERS:  FileID          - ID of current listing file
 *
 * RETURN:      FALSE on EOF (input source file), TRUE otherwise
 *
 * DESCRIPTION: Read one line from the input source file and echo it to the
 *              listing file, prefixed with the line number, and if the source
 *              file contains include files, prefixed with the current filename
 *
 ******************************************************************************/

static UINT32
LsWriteOneSourceLine (
    UINT32                  FileId)
{
    UINT8                   FileByte;


    Gbl_SourceLine++;
    Gbl_ListingNode->LineNumber++;

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
        FlPrintFile (FileId, "%12s %5d....",
                    Gbl_ListingNode->Filename, Gbl_ListingNode->LineNumber);
    }
    else
    {
        /* No include files, just print the line number */

        FlPrintFile (FileId, "%8d....", Gbl_SourceLine);
    }

    /* Read one line (up to a newline or EOF) */

    while (FlReadFile (ASL_FILE_SOURCE_OUTPUT, &FileByte, 1) == AE_OK)
    {
        if (FileId == ASL_FILE_C_SOURCE_OUTPUT)
        {
            if (FileByte == '/')
            {
                FileByte = '*';
            }
        }

        FlWriteFile (FileId, &FileByte, 1);
        if (FileByte == '\n')
        {
            /*
             * Check if an error occurred on this source line during the compile.
             * If so, we print the error message after the source line.
             */
            LsCheckException (Gbl_SourceLine, FileId);
            return (1);
        }
    }

    /* EOF on the input file was reached */

    return (0);
}


/*******************************************************************************
 *
 * FUNCTION:    LsFinishSourceListing
 *
 * PARAMETERS:  FileId          - ID of current listing file.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Cleanup routine for the listing file.  Flush the hex AML
 *              listing buffer, and flush out any remaining lines in the
 *              source input file.
 *
 ******************************************************************************/

static void
LsFinishSourceListing (
    UINT32                  FileId)
{

    if ((FileId == ASL_FILE_ASM_INCLUDE_OUTPUT) ||
        (FileId == ASL_FILE_C_INCLUDE_OUTPUT))
    {
        return;
    }

    LsFlushListingBuffer (FileId);
    Gbl_CurrentAmlOffset = 0;

    /* Flush any remaining text in the source file */

    if (FileId == ASL_FILE_C_SOURCE_OUTPUT)
    {
        FlPrintFile (FileId, "    /*\n");
    }

    while (LsWriteOneSourceLine (FileId))
    { ; }

    if (FileId == ASL_FILE_C_SOURCE_OUTPUT)
    {
        FlPrintFile (FileId, "\n     */\n    };\n");
    }

    FlPrintFile (FileId, "\n");

    if (FileId == ASL_FILE_LISTING_OUTPUT)
    {
        /* Print a summary of the compile exceptions */

        FlPrintFile (FileId, "\n\nSummary of errors and warnings\n\n");
        AePrintErrorLog (FileId);
        FlPrintFile (FileId, "\n");
        UtDisplaySummary (FileId);
        FlPrintFile (FileId, "\n");
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
 *              reached the specified logical (cumulative) line number.  This
 *              automatically echos out comment blocks and other non-AML
 *              generating text until we get to the actual AML-generating line
 *              of ASL code specified by the logical line number.
 *
 ******************************************************************************/

static void
LsWriteSourceLines (
    UINT32                  ToLineNumber,
    UINT32                  ToLogicalLineNumber,
    UINT32                  FileId)
{

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
 * FUNCTION:    LsWriteNodeToListing
 *
 * PARAMETERS:  Op            - Parse node to write to the listing file.
 *              FileId          - ID of current listing file
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Write "a node" to the listing file.  This means to
 *              1) Write out all of the source text associated with the node
 *              2) Write out all of the AML bytes associated with the node
 *              3) Write any compiler exceptions associated with the node
 *
 ******************************************************************************/

static void
LsWriteNodeToListing (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  FileId)
{
    const ACPI_OPCODE_INFO  *OpInfo;
    UINT32                  OpClass;
    char                    *Pathname;
    UINT32                  Length;
    UINT32                  i;


    OpInfo  = AcpiPsGetOpcodeInfo (Op->Asl.AmlOpcode);
    OpClass = OpInfo->Class;

    /* TBD: clean this up with a single flag that says:
     * I start a named output block
     */
    if (FileId == ASL_FILE_C_SOURCE_OUTPUT)
    {
        switch (Op->Asl.ParseOpcode)
        {
        case PARSEOP_DEFINITIONBLOCK:
        case PARSEOP_METHODCALL:
        case PARSEOP_INCLUDE:
        case PARSEOP_INCLUDE_END:
        case PARSEOP_DEFAULT_ARG:

            break;

        default:
            switch (OpClass)
            {
            case AML_CLASS_NAMED_OBJECT:
                switch (Op->Asl.AmlOpcode)
                {
                case AML_SCOPE_OP:
                case AML_ALIAS_OP:
                    break;

                default:
                    if (Op->Asl.ExternalName)
                    {
                        LsFlushListingBuffer (FileId);
                        FlPrintFile (FileId, "    };\n");
                    }
                    break;
                }
                break;

            default:
                /* Don't care about other objects */
                break;
            }
            break;
        }
    }

    /* These cases do not have a corresponding AML opcode */

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_DEFINITIONBLOCK:

        LsWriteSourceLines (Op->Asl.EndLine, Op->Asl.EndLogicalLine, FileId);

        /* Use the table Signature and TableId to build a unique name */

        if (FileId == ASL_FILE_ASM_SOURCE_OUTPUT)
        {
            FlPrintFile (FileId,
                "%s_%s_Header \\\n",
                Gbl_TableSignature, Gbl_TableId);
        }
        if (FileId == ASL_FILE_C_SOURCE_OUTPUT)
        {
            FlPrintFile (FileId,
                "    unsigned char    %s_%s_Header [] =\n    {\n",
                Gbl_TableSignature, Gbl_TableId);
        }
        if (FileId == ASL_FILE_ASM_INCLUDE_OUTPUT)
        {
            FlPrintFile (FileId,
                "extrn %s_%s_Header : byte\n",
                Gbl_TableSignature, Gbl_TableId);
        }
        if (FileId == ASL_FILE_C_INCLUDE_OUTPUT)
        {
            FlPrintFile (FileId,
                "extern unsigned char    %s_%s_Header [];\n",
                Gbl_TableSignature, Gbl_TableId);
        }
        return;


    case PARSEOP_METHODCALL:

        LsWriteSourceLines (Op->Asl.LineNumber, Op->Asl.LogicalLineNumber,
            FileId);
        return;


    case PARSEOP_INCLUDE:

        /* Flush everything up to and including the include source line */

        LsWriteSourceLines (Op->Asl.LineNumber, Op->Asl.LogicalLineNumber,
            FileId);

        /* Create a new listing node and push it */

        LsPushNode (Op->Asl.Child->Asl.Value.String);
        return;


    case PARSEOP_INCLUDE_END:

        /* Flush out the rest of the include file */

        LsWriteSourceLines (Op->Asl.LineNumber, Op->Asl.LogicalLineNumber,
            FileId);

        /* Pop off this listing node and go back to the parent file */

        (void) LsPopNode ();
        return;


    case PARSEOP_DEFAULT_ARG:

        if (Op->Asl.CompileFlags & NODE_IS_RESOURCE_DESC)
        {
            LsWriteSourceLines (Op->Asl.LineNumber, Op->Asl.EndLogicalLine,
                FileId);
        }
        return;


    default:
        /* All other opcodes have an AML opcode */
        break;
    }

    /*
     * Otherwise, we look at the AML opcode because we can
     * switch on the opcode type, getting an entire class
     * at once
     */
    switch (OpClass)
    {
    case AML_CLASS_ARGUMENT:       /* argument type only */
    case AML_CLASS_INTERNAL:

        break;


    case AML_CLASS_NAMED_OBJECT:

        switch (Op->Asl.AmlOpcode)
        {
        case AML_FIELD_OP:
        case AML_INDEX_FIELD_OP:
        case AML_BANK_FIELD_OP:

            /*
             * For fields, we want to dump all the AML after the
             * entire definition
             */
            LsWriteSourceLines (Op->Asl.EndLine, Op->Asl.EndLogicalLine,
                FileId);
            break;

        case AML_NAME_OP:

            if (Op->Asl.CompileFlags & NODE_IS_RESOURCE_DESC)
            {
                LsWriteSourceLines (Op->Asl.LineNumber, Op->Asl.LogicalLineNumber,
                    FileId);
            }
            else
            {
                /*
                 * For fields, we want to dump all the AML after the
                 * entire definition
                 */
                LsWriteSourceLines (Op->Asl.EndLine, Op->Asl.EndLogicalLine,
                    FileId);
            }
            break;

        default:
            LsWriteSourceLines (Op->Asl.LineNumber, Op->Asl.LogicalLineNumber,
                FileId);
            break;
        }

        switch (Op->Asl.AmlOpcode)
        {
        case AML_SCOPE_OP:
        case AML_ALIAS_OP:

            /* These opcodes do not declare a new object, ignore them */

            break;

        default:

            /* All other named object opcodes come here */

            switch (FileId)
            {
            case ASL_FILE_ASM_SOURCE_OUTPUT:
            case ASL_FILE_C_SOURCE_OUTPUT:
            case ASL_FILE_ASM_INCLUDE_OUTPUT:
            case ASL_FILE_C_INCLUDE_OUTPUT:

                /*
                 * For named objects, we will create a valid symbol so that the
                 * AML code can be referenced from C or ASM
                 */
                if (Op->Asl.ExternalName)
                {
                    /* Get the full pathname associated with this node */

                    Pathname = AcpiNsGetExternalPathname (Op->Asl.Node);
                    Length = strlen (Pathname);
                    if (Length >= 4)
                    {
                        /* Convert all dots in the path to underscores */

                        for (i = 0; i < Length; i++)
                        {
                            if (Pathname[i] == '.')
                            {
                                Pathname[i] = '_';
                            }
                        }

                        /* Create the appropriate symbol in the output file */

                        if (FileId == ASL_FILE_ASM_SOURCE_OUTPUT)
                        {
                            FlPrintFile (FileId,
                                "%s_%s_%s  \\\n",
                                Gbl_TableSignature, Gbl_TableId, &Pathname[1]);
                        }
                        if (FileId == ASL_FILE_C_SOURCE_OUTPUT)
                        {
                            FlPrintFile (FileId,
                                "    unsigned char    %s_%s_%s [] =\n    {\n",
                                Gbl_TableSignature, Gbl_TableId, &Pathname[1]);
                        }
                        if (FileId == ASL_FILE_ASM_INCLUDE_OUTPUT)
                        {
                            FlPrintFile (FileId,
                                "extrn %s_%s_%s : byte\n",
                                Gbl_TableSignature, Gbl_TableId, &Pathname[1]);
                        }
                        if (FileId == ASL_FILE_C_INCLUDE_OUTPUT)
                        {
                            FlPrintFile (FileId,
                                "extern unsigned char    %s_%s_%s [];\n",
                                Gbl_TableSignature, Gbl_TableId, &Pathname[1]);
                        }
                    }
                    ACPI_FREE (Pathname);
                }
                break;

            default:
                /* Nothing to do for listing file */
                break;
            }
        }
        break;

    case AML_CLASS_EXECUTE:
    case AML_CLASS_CREATE:
    default:

        if ((Op->Asl.ParseOpcode == PARSEOP_BUFFER) &&
            (Op->Asl.CompileFlags & NODE_IS_RESOURCE_DESC))
        {
            return;
        }

        LsWriteSourceLines (Op->Asl.LineNumber, Op->Asl.LogicalLineNumber,
            FileId);
        break;

    case AML_CLASS_UNKNOWN:
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    LsDoHexOutput
 *
 * PARAMETERS:  None
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Create the hex output file.
 *
 ******************************************************************************/

void
LsDoHexOutput (
    void)
{

    switch (Gbl_HexOutputFlag)
    {
    case HEX_OUTPUT_C:

        LsDoHexOutputC ();
        break;

    case HEX_OUTPUT_ASM:

        LsDoHexOutputAsm ();
        break;

    case HEX_OUTPUT_ASL:

        LsDoHexOutputAsl ();
        break;

    default:
        /* No other output types supported */
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    LsDoHexOutputC
 *
 * PARAMETERS:  None
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Create the hex output file.  This is the same data as the AML
 *              output file, but formatted into hex/ascii bytes suitable for
 *              inclusion into a C source file.
 *
 ******************************************************************************/

static void
LsDoHexOutputC (
    void)
{
    UINT8                   FileData[HEX_TABLE_LINE_SIZE];
    UINT32                  LineLength;
    UINT32                  Offset = 0;
    UINT32                  AmlFileSize;
    UINT32                  i;


    /* Get AML size, seek back to start */

    AmlFileSize = FlGetFileSize (ASL_FILE_AML_OUTPUT);
    FlSeekFile (ASL_FILE_AML_OUTPUT, 0);

    FlPrintFile (ASL_FILE_HEX_OUTPUT, " * C source code output\n");
    FlPrintFile (ASL_FILE_HEX_OUTPUT, " * AML code block contains 0x%X bytes\n *\n */\n",
        AmlFileSize);
    FlPrintFile (ASL_FILE_HEX_OUTPUT, "unsigned char AmlCode[] =\n{\n");

    while (Offset < AmlFileSize)
    {
        /* Read enough bytes needed for one output line */

        LineLength = fread (FileData, 1, HEX_TABLE_LINE_SIZE,
                        Gbl_Files[ASL_FILE_AML_OUTPUT].Handle);
        if (!LineLength)
        {
            break;
        }

        FlPrintFile (ASL_FILE_HEX_OUTPUT, "    ");

        for (i = 0; i < LineLength; i++)
        {
            /*
             * Print each hex byte.
             * Add a comma until the very last byte of the AML file
             * (Some C compilers complain about a trailing comma)
             */
            FlPrintFile (ASL_FILE_HEX_OUTPUT, "0x%2.2X", FileData[i]);
            if ((Offset + i + 1) < AmlFileSize)
            {
                FlPrintFile (ASL_FILE_HEX_OUTPUT, ",");
            }
            else
            {
                FlPrintFile (ASL_FILE_HEX_OUTPUT, " ");
            }
        }

        /* Add fill spaces if needed for last line */

        if (LineLength < HEX_TABLE_LINE_SIZE)
        {
            FlPrintFile (ASL_FILE_HEX_OUTPUT, "%*s",
                5 * (HEX_TABLE_LINE_SIZE - LineLength), " ");
        }

        /* Emit the offset and ascii dump for the entire line */

        FlPrintFile (ASL_FILE_HEX_OUTPUT, "  /* %8.8X", Offset);
        LsDumpAsciiInComment (ASL_FILE_HEX_OUTPUT, LineLength, FileData);
        FlPrintFile (ASL_FILE_HEX_OUTPUT, "%*s*/\n",
            HEX_TABLE_LINE_SIZE - LineLength + 1, " ");

        Offset += LineLength;
    }

    FlPrintFile (ASL_FILE_HEX_OUTPUT, "};\n");
}


/*******************************************************************************
 *
 * FUNCTION:    LsDoHexOutputAsl
 *
 * PARAMETERS:  None
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Create the hex output file.  This is the same data as the AML
 *              output file, but formatted into hex/ascii bytes suitable for
 *              inclusion into a C source file.
 *
 ******************************************************************************/

static void
LsDoHexOutputAsl (
    void)
{
    UINT8                   FileData[HEX_TABLE_LINE_SIZE];
    UINT32                  LineLength;
    UINT32                  Offset = 0;
    UINT32                  AmlFileSize;
    UINT32                  i;


    /* Get AML size, seek back to start */

    AmlFileSize = FlGetFileSize (ASL_FILE_AML_OUTPUT);
    FlSeekFile (ASL_FILE_AML_OUTPUT, 0);

    FlPrintFile (ASL_FILE_HEX_OUTPUT, " * ASL source code output\n");
    FlPrintFile (ASL_FILE_HEX_OUTPUT, " * AML code block contains 0x%X bytes\n *\n */\n",
        AmlFileSize);
    FlPrintFile (ASL_FILE_HEX_OUTPUT, "    Name (BUF1, Buffer()\n    {\n");

    while (Offset < AmlFileSize)
    {
        /* Read enough bytes needed for one output line */

        LineLength = fread (FileData, 1, HEX_TABLE_LINE_SIZE,
                        Gbl_Files[ASL_FILE_AML_OUTPUT].Handle);
        if (!LineLength)
        {
            break;
        }

        FlPrintFile (ASL_FILE_HEX_OUTPUT, "        ");

        for (i = 0; i < LineLength; i++)
        {
            /*
             * Print each hex byte.
             * Add a comma until the very last byte of the AML file
             * (Some C compilers complain about a trailing comma)
             */
            FlPrintFile (ASL_FILE_HEX_OUTPUT, "0x%2.2X", FileData[i]);
            if ((Offset + i + 1) < AmlFileSize)
            {
                FlPrintFile (ASL_FILE_HEX_OUTPUT, ",");
            }
            else
            {
                FlPrintFile (ASL_FILE_HEX_OUTPUT, " ");
            }
        }

        /* Add fill spaces if needed for last line */

        if (LineLength < HEX_TABLE_LINE_SIZE)
        {
            FlPrintFile (ASL_FILE_HEX_OUTPUT, "%*s",
                5 * (HEX_TABLE_LINE_SIZE - LineLength), " ");
        }

        /* Emit the offset and ascii dump for the entire line */

        FlPrintFile (ASL_FILE_HEX_OUTPUT, "  /* %8.8X", Offset);
        LsDumpAsciiInComment (ASL_FILE_HEX_OUTPUT, LineLength, FileData);
        FlPrintFile (ASL_FILE_HEX_OUTPUT, "%*s*/\n",
            HEX_TABLE_LINE_SIZE - LineLength + 1, " ");

        Offset += LineLength;
    }

    FlPrintFile (ASL_FILE_HEX_OUTPUT, "    })\n");
}


/*******************************************************************************
 *
 * FUNCTION:    LsDoHexOutputAsm
 *
 * PARAMETERS:  None
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Create the hex output file.  This is the same data as the AML
 *              output file, but formatted into hex/ascii bytes suitable for
 *              inclusion into a ASM source file.
 *
 ******************************************************************************/

static void
LsDoHexOutputAsm (
    void)
{
    UINT8                   FileData[HEX_TABLE_LINE_SIZE];
    UINT32                  LineLength;
    UINT32                  Offset = 0;
    UINT32                  AmlFileSize;
    UINT32                  i;


    /* Get AML size, seek back to start */

    AmlFileSize = FlGetFileSize (ASL_FILE_AML_OUTPUT);
    FlSeekFile (ASL_FILE_AML_OUTPUT, 0);

    FlPrintFile (ASL_FILE_HEX_OUTPUT, "; Assembly code source output\n");
    FlPrintFile (ASL_FILE_HEX_OUTPUT, "; AML code block contains 0x%X bytes\n;\n",
        AmlFileSize);

    while (Offset < AmlFileSize)
    {
        /* Read enough bytes needed for one output line */

        LineLength = fread (FileData, 1, HEX_TABLE_LINE_SIZE,
                        Gbl_Files[ASL_FILE_AML_OUTPUT].Handle);
        if (!LineLength)
        {
            break;
        }

        FlPrintFile (ASL_FILE_HEX_OUTPUT, "  db  ");

        for (i = 0; i < LineLength; i++)
        {
            /*
             * Print each hex byte.
             * Add a comma until the last byte of the line
             */
            FlPrintFile (ASL_FILE_HEX_OUTPUT, "0%2.2Xh", FileData[i]);
            if ((i + 1) < LineLength)
            {
                FlPrintFile (ASL_FILE_HEX_OUTPUT, ",");
            }
        }

        FlPrintFile (ASL_FILE_HEX_OUTPUT, " ");

        /* Add fill spaces if needed for last line */

        if (LineLength < HEX_TABLE_LINE_SIZE)
        {
            FlPrintFile (ASL_FILE_HEX_OUTPUT, "%*s",
                5 * (HEX_TABLE_LINE_SIZE - LineLength), " ");
        }

        /* Emit the offset and ascii dump for the entire line */

        FlPrintFile (ASL_FILE_HEX_OUTPUT, "  ; %8.8X", Offset);
        LsDumpAsciiInComment (ASL_FILE_HEX_OUTPUT, LineLength, FileData);
        FlPrintFile (ASL_FILE_HEX_OUTPUT, "\n");

        Offset += LineLength;
    }

    FlPrintFile (ASL_FILE_HEX_OUTPUT, "\n");
}


