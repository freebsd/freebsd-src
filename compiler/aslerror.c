
/******************************************************************************
 *
 * Module Name: aslerror - Error handling and statistics
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

#define ASL_EXCEPTIONS
#include "aslcompiler.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslerror")

/* Local prototypes */

static void
AeAddToErrorLog (
    ASL_ERROR_MSG           *Enode);


void
AeClearErrorLog (
    void)
{
    ASL_ERROR_MSG           *Enode = Gbl_ErrorLog;
    ASL_ERROR_MSG           *Next;

    /* Walk the error node list */

    while (Enode)
    {
        Next = Enode->Next;
        ACPI_FREE (Enode);
        Enode = Next;
    }

    Gbl_ErrorLog = NULL;
}


/*******************************************************************************
 *
 * FUNCTION:    AeAddToErrorLog
 *
 * PARAMETERS:  Enode       - An error node to add to the log
 *
 * RETURN:      None
 *
 * DESCRIPTION: Add a new error node to the error log.  The error log is
 *              ordered by the "logical" line number (cumulative line number
 *              including all include files.)
 *
 ******************************************************************************/

static void
AeAddToErrorLog (
    ASL_ERROR_MSG           *Enode)
{
    ASL_ERROR_MSG           *Next;
    ASL_ERROR_MSG           *Prev;


    /* If Gbl_ErrorLog is null, this is the first error node */

    if (!Gbl_ErrorLog)
    {
        Gbl_ErrorLog = Enode;
        return;
    }

    /*
     * Walk error list until we find a line number greater than ours.
     * List is sorted according to line number.
     */
    Prev = NULL;
    Next = Gbl_ErrorLog;

    while ((Next) &&
           (Next->LogicalLineNumber <= Enode->LogicalLineNumber))
    {
        Prev = Next;
        Next = Next->Next;
    }

    /* Found our place in the list */

    Enode->Next = Next;

    if (Prev)
    {
        Prev->Next = Enode;
    }
    else
    {
        Gbl_ErrorLog = Enode;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AePrintException
 *
 * PARAMETERS:  FileId          - ID of output file
 *              Enode           - Error node to print
 *              Header          - Additional text before each message
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print the contents of an error node.
 *
 * NOTE:        We don't use the FlxxxFile I/O functions here because on error
 *              they abort the compiler and call this function!  Since we
 *              are reporting errors here, we ignore most output errors and
 *              just try to get out as much as we can.
 *
 ******************************************************************************/

void
AePrintException (
    UINT32                  FileId,
    ASL_ERROR_MSG           *Enode,
    char                    *Header)
{
    UINT8                   SourceByte;
    int                     Actual;
    size_t                  RActual;
    UINT32                  MsgLength;
    char                    *MainMessage;
    char                    *ExtraMessage;
    UINT32                  SourceColumn;
    UINT32                  ErrorColumn;
    FILE                    *OutputFile;
    FILE                    *SourceFile;
    long                    FileSize;
    BOOLEAN                 PrematureEOF = FALSE;


    if (Gbl_NoErrors)
    {
        return;
    }

    /*
     * Only listing files have a header, and remarks/optimizations
     * are always output
     */
    if (!Header)
    {
        /* Ignore remarks if requested */

        switch (Enode->Level)
        {
        case ASL_REMARK:
            if (!Gbl_DisplayRemarks)
            {
                return;
            }
            break;

        case ASL_OPTIMIZATION:
            if (!Gbl_DisplayOptimizations)
            {
                return;
            }
            break;

        default:
            break;
        }
    }

    /* Get the file handles */

    OutputFile = Gbl_Files[FileId].Handle;

    /* Use the merged header/source file if present, otherwise use input file */

    SourceFile = Gbl_Files[ASL_FILE_SOURCE_OUTPUT].Handle;
    if (!SourceFile)
    {
        SourceFile = Gbl_Files[ASL_FILE_INPUT].Handle;
    }

    if (SourceFile)
    {
        /* Determine if the error occurred at source file EOF */

        fseek (SourceFile, 0, SEEK_END);
        FileSize = ftell (SourceFile);

        if ((long) Enode->LogicalByteOffset >= FileSize)
        {
            PrematureEOF = TRUE;
        }
    }

    if (Header)
    {
        fprintf (OutputFile, "%s", Header);
    }

    /* Print filename and line number if present and valid */

    if (Enode->Filename)
    {
        if (Gbl_VerboseErrors)
        {
            fprintf (OutputFile, "%6s", Enode->Filename);

            if (Enode->LineNumber)
            {
                fprintf (OutputFile, " %6u: ", Enode->LineNumber);

                /*
                 * If not at EOF, get the corresponding source code line and
                 * display it. Don't attempt this if we have a premature EOF
                 * condition.
                 */
                if (!PrematureEOF)
                {
                    /*
                     * Seek to the offset in the combined source file, read
                     * the source line, and write it to the output.
                     */
                    Actual = fseek (SourceFile, (long) Enode->LogicalByteOffset,
                                (int) SEEK_SET);
                    if (Actual)
                    {
                        fprintf (OutputFile,
                            "[*** iASL: Seek error on source code temp file %s ***]",
                            Gbl_Files[ASL_FILE_SOURCE_OUTPUT].Filename);
                    }
                    else
                    {
                        RActual = fread (&SourceByte, 1, 1, SourceFile);
                        if (!RActual)
                        {
                            fprintf (OutputFile,
                                "[*** iASL: Read error on source code temp file %s ***]",
                                Gbl_Files[ASL_FILE_SOURCE_OUTPUT].Filename);
                        }

                        else while (RActual && SourceByte && (SourceByte != '\n'))
                        {
                            fwrite (&SourceByte, 1, 1, OutputFile);
                            RActual = fread (&SourceByte, 1, 1, SourceFile);
                        }
                    }
                }

                fprintf (OutputFile, "\n");
            }
        }
        else
        {
            fprintf (OutputFile, "%s", Enode->Filename);

            if (Enode->LineNumber)
            {
                fprintf (OutputFile, "(%u) : ", Enode->LineNumber);
            }
        }
    }

    /* NULL message ID, just print the raw message */

    if (Enode->MessageId == 0)
    {
        fprintf (OutputFile, "%s\n", Enode->Message);
    }
    else
    {
        /* Decode the message ID */

        fprintf (OutputFile, "%s %4.4d - ",
                    AslErrorLevel[Enode->Level],
                    Enode->MessageId + ((Enode->Level+1) * 1000));

        MainMessage = AslMessages[Enode->MessageId];
        ExtraMessage = Enode->Message;

        if (Enode->LineNumber)
        {
            MsgLength = strlen (MainMessage);
            if (MsgLength == 0)
            {
                MainMessage = Enode->Message;

                MsgLength = strlen (MainMessage);
                ExtraMessage = NULL;
            }

            if (Gbl_VerboseErrors && !PrematureEOF)
            {
                SourceColumn = Enode->Column + Enode->FilenameLength + 6 + 2;
                ErrorColumn = ASL_ERROR_LEVEL_LENGTH + 5 + 2 + 1;

                if ((MsgLength + ErrorColumn) < (SourceColumn - 1))
                {
                    fprintf (OutputFile, "%*s%s",
                        (int) ((SourceColumn - 1) - ErrorColumn),
                        MainMessage, " ^ ");
                }
                else
                {
                    fprintf (OutputFile, "%*s %s",
                        (int) ((SourceColumn - ErrorColumn) + 1), "^",
                        MainMessage);
                }
            }
            else
            {
                fprintf (OutputFile, " %s", MainMessage);
            }

            /* Print the extra info message if present */

            if (ExtraMessage)
            {
                fprintf (OutputFile, " (%s)", ExtraMessage);
            }

            if (PrematureEOF)
            {
                fprintf (OutputFile, " and premature End-Of-File");
            }

            fprintf (OutputFile, "\n");
            if (Gbl_VerboseErrors)
            {
                fprintf (OutputFile, "\n");
            }
        }
        else
        {
            fprintf (OutputFile, " %s %s\n\n", MainMessage, ExtraMessage);
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AePrintErrorLog
 *
 * PARAMETERS:  FileId           - Where to output the error log
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print the entire contents of the error log
 *
 ******************************************************************************/

void
AePrintErrorLog (
    UINT32                  FileId)
{
    ASL_ERROR_MSG           *Enode = Gbl_ErrorLog;


    /* Walk the error node list */

    while (Enode)
    {
        AePrintException (FileId, Enode, NULL);
        Enode = Enode->Next;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AslCommonError
 *
 * PARAMETERS:  Level               - Seriousness (Warning/error, etc.)
 *              MessageId           - Index into global message buffer
 *              CurrentLineNumber   - Actual file line number
 *              LogicalLineNumber   - Cumulative line number
 *              LogicalByteOffset   - Byte offset in source file
 *              Column              - Column in current line
 *              Filename            - source filename
 *              ExtraMessage        - additional error message
 *
 * RETURN:      None
 *
 * DESCRIPTION: Create a new error node and add it to the error log
 *
 ******************************************************************************/

void
AslCommonError (
    UINT8                   Level,
    UINT8                   MessageId,
    UINT32                  CurrentLineNumber,
    UINT32                  LogicalLineNumber,
    UINT32                  LogicalByteOffset,
    UINT32                  Column,
    char                    *Filename,
    char                    *ExtraMessage)
{
    UINT32                  MessageSize;
    char                    *MessageBuffer = NULL;
    ASL_ERROR_MSG           *Enode;


    Enode = UtLocalCalloc (sizeof (ASL_ERROR_MSG));

    if (ExtraMessage)
    {
        /* Allocate a buffer for the message and a new error node */

        MessageSize   = strlen (ExtraMessage) + 1;
        MessageBuffer = UtLocalCalloc (MessageSize);

        /* Keep a copy of the extra message */

        ACPI_STRCPY (MessageBuffer, ExtraMessage);
    }

    /* Initialize the error node */

    if (Filename)
    {
        Enode->Filename       = Filename;
        Enode->FilenameLength = strlen (Filename);
        if (Enode->FilenameLength < 6)
        {
            Enode->FilenameLength = 6;
        }
    }

    Enode->MessageId            = MessageId;
    Enode->Level                = Level;
    Enode->LineNumber           = CurrentLineNumber;
    Enode->LogicalLineNumber    = LogicalLineNumber;
    Enode->LogicalByteOffset    = LogicalByteOffset;
    Enode->Column               = Column;
    Enode->Message              = MessageBuffer;

    /* Add the new node to the error node list */

    AeAddToErrorLog (Enode);

    if (Gbl_DebugFlag)
    {
        /* stderr is a file, send error to it immediately */

        AePrintException (ASL_FILE_STDERR, Enode, NULL);
    }

    Gbl_ExceptionCount[Level]++;
    if (Gbl_ExceptionCount[ASL_ERROR] > ASL_MAX_ERROR_COUNT)
    {
        printf ("\nMaximum error count (%u) exceeded\n", ASL_MAX_ERROR_COUNT);

        Gbl_SourceLine = 0;
        Gbl_NextError = Gbl_ErrorLog;
        CmDoOutputFiles ();
        CmCleanupAndExit ();
        exit(1);
    }

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AslError
 *
 * PARAMETERS:  Level               - Seriousness (Warning/error, etc.)
 *              MessageId           - Index into global message buffer
 *              Op                  - Parse node where error happened
 *              ExtraMessage        - additional error message
 *
 * RETURN:      None
 *
 * DESCRIPTION: Main error reporting routine for the ASL compiler (all code
 *              except the parser.)
 *
 ******************************************************************************/

void
AslError (
    UINT8                   Level,
    UINT8                   MessageId,
    ACPI_PARSE_OBJECT       *Op,
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


    if (Op)
    {
        AslCommonError (Level, MessageId, Op->Asl.LineNumber,
                        Op->Asl.LogicalLineNumber,
                        Op->Asl.LogicalByteOffset,
                        Op->Asl.Column,
                        Op->Asl.Filename, ExtraMessage);
    }
    else
    {
        AslCommonError (Level, MessageId, 0,
                        0, 0, 0, NULL, ExtraMessage);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AslCoreSubsystemError
 *
 * PARAMETERS:  Op                  - Parse node where error happened
 *              Status              - The ACPI CA Exception
 *              ExtraMessage        - additional error message
 *              Abort               - TRUE -> Abort compilation
 *
 * RETURN:      None
 *
 * DESCRIPTION: Error reporting routine for exceptions returned by the ACPI
 *              CA core subsystem.
 *
 ******************************************************************************/

void
AslCoreSubsystemError (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_STATUS             Status,
    char                    *ExtraMessage,
    BOOLEAN                 Abort)
{

    sprintf (MsgBuffer, "%s %s", AcpiFormatException (Status), ExtraMessage);

    if (Op)
    {
        AslCommonError (ASL_ERROR, ASL_MSG_CORE_EXCEPTION, Op->Asl.LineNumber,
                        Op->Asl.LogicalLineNumber,
                        Op->Asl.LogicalByteOffset,
                        Op->Asl.Column,
                        Op->Asl.Filename, MsgBuffer);
    }
    else
    {
        AslCommonError (ASL_ERROR, ASL_MSG_CORE_EXCEPTION, 0,
                        0, 0, 0, NULL, MsgBuffer);
    }

    if (Abort)
    {
        AslAbort ();
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AslCompilererror
 *
 * PARAMETERS:  CompilerMessage         - Error message from the parser
 *
 * RETURN:      Status (0 for now)
 *
 * DESCRIPTION: Report an error situation discovered in a production
 *              NOTE: don't change the name of this function, it is called
 *              from the auto-generated parser.
 *
 ******************************************************************************/

int
AslCompilererror (
    char                    *CompilerMessage)
{

    AslCommonError (ASL_ERROR, ASL_MSG_SYNTAX, Gbl_CurrentLineNumber,
                    Gbl_LogicalLineNumber, Gbl_CurrentLineOffset,
                    Gbl_CurrentColumn, Gbl_Files[ASL_FILE_INPUT].Filename,
                    CompilerMessage);

    return 0;
}


