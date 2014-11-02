/******************************************************************************
 *
 * Module Name: acgetline - local line editing
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

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acparser.h>
#include <contrib/dev/acpica/include/acdebug.h>

#include <stdio.h>

/*
 * This is an os-independent implementation of line-editing services needed
 * by the AcpiExec utility. It uses getchar() and putchar() and the existing
 * history support provided by the AML debugger. It assumes that the terminal
 * is in the correct line-editing mode such as raw and noecho. The OSL
 * interface AcpiOsInitialize should do this. AcpiOsTerminate should put the
 * terminal back into the original mode.
 */
#define _COMPONENT          ACPI_OS_SERVICES
        ACPI_MODULE_NAME    ("acgetline")


/* Local prototypes */

static void
AcpiAcClearLine (
    UINT32                  EndOfLine,
    UINT32                  CursorPosition);

/* Various ASCII constants */

#define _ASCII_NUL                  0
#define _ASCII_BACKSPACE            0x08
#define _ASCII_TAB                  0x09
#define _ASCII_ESCAPE               0x1B
#define _ASCII_SPACE                0x20
#define _ASCII_LEFT_BRACKET         0x5B
#define _ASCII_DEL                  0x7F
#define _ASCII_UP_ARROW             'A'
#define _ASCII_DOWN_ARROW           'B'
#define _ASCII_RIGHT_ARROW          'C'
#define _ASCII_LEFT_ARROW           'D'
#define _ASCII_NEWLINE              '\n'

extern UINT32               AcpiGbl_NextCmdNum;

/* Erase a single character on the input command line */

#define ACPI_CLEAR_CHAR() \
    putchar (_ASCII_BACKSPACE); \
    putchar (_ASCII_SPACE); \
    putchar (_ASCII_BACKSPACE);

/* Backup cursor by Count positions */

#define ACPI_BACKUP_CURSOR(i, Count) \
    for (i = 0; i < (Count); i++) \
        {putchar (_ASCII_BACKSPACE);}


/******************************************************************************
 *
 * FUNCTION:    AcpiAcClearLine
 *
 * PARAMETERS:  EndOfLine           - Current end-of-line index
 *              CursorPosition      - Current cursor position within line
 *
 * RETURN:      None
 *
 * DESCRIPTION: Clear the entire command line the hard way, but probably the
 *              most portable.
 *
 *****************************************************************************/

static void
AcpiAcClearLine (
    UINT32                  EndOfLine,
    UINT32                  CursorPosition)
{
    UINT32                  i;


    if (CursorPosition < EndOfLine)
    {
        /* Clear line from current position to end of line */

        for (i = 0; i < (EndOfLine - CursorPosition); i++)
        {
            putchar (' ');
        }
    }

    /* Clear the entire line */

    for (; EndOfLine > 0; EndOfLine--)
    {
        ACPI_CLEAR_CHAR ();
    }
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetLine
 *
 * PARAMETERS:  Buffer              - Where to return the command line
 *              BufferLength        - Maximum length of Buffer
 *              BytesRead           - Where the actual byte count is returned
 *
 * RETURN:      Status and actual bytes read
 *
 * DESCRIPTION: Get the next input line from the terminal. NOTE: terminal
 *              is expected to be in a mode that supports line-editing (raw,
 *              noecho). This function is intended to be very portable. Also,
 *              it uses the history support implemented in the AML debugger.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsGetLine (
    char                    *Buffer,
    UINT32                  BufferLength,
    UINT32                  *BytesRead)
{
    char                    *NextCommand;
    UINT32                  MaxCommandIndex = AcpiGbl_NextCmdNum - 1;
    UINT32                  CurrentCommandIndex = MaxCommandIndex;
    UINT32                  PreviousCommandIndex = MaxCommandIndex;
    int                     InputChar;
    UINT32                  CursorPosition = 0;
    UINT32                  EndOfLine = 0;
    UINT32                  i;


    /* Always clear the line buffer before we read a new line */

    memset (Buffer, 0, BufferLength);

    /*
     * This loop gets one character at a time (except for esc sequences)
     * until a newline or error is detected.
     *
     * Note: Don't attempt to write terminal control ESC sequences, even
     * though it makes certain things more difficult.
     */
    while (1)
    {
        if (EndOfLine >= (BufferLength - 1))
        {
            return (AE_BUFFER_OVERFLOW);
        }

        InputChar = getchar ();
        switch (InputChar)
        {
        default: /* This is the normal character case */

            /* Echo the character (at EOL) and copy it to the line buffer */

            if (EndOfLine == CursorPosition)
            {
                putchar (InputChar);
                Buffer[EndOfLine] = (char) InputChar;

                EndOfLine++;
                CursorPosition++;
                Buffer[EndOfLine] = 0;
                continue;
            }

            /* Insert character into the middle of the buffer */

            memmove (&Buffer[CursorPosition + 1], &Buffer[CursorPosition],
                (EndOfLine - CursorPosition + 1));

            Buffer [CursorPosition] = (char) InputChar;
            Buffer [EndOfLine + 1] = 0;

            /* Display the new part of line starting at the new character */

            fprintf (stdout, "%s", &Buffer[CursorPosition]);

            /* Restore cursor */

            ACPI_BACKUP_CURSOR (i, EndOfLine - CursorPosition);
            CursorPosition++;
            EndOfLine++;
            continue;

        case _ASCII_DEL: /* Backspace key */

            if (!EndOfLine) /* Any characters on the command line? */
            {
                continue;
            }

            if (EndOfLine == CursorPosition) /* Erase the final character */
            {
                ACPI_CLEAR_CHAR ();
                EndOfLine--;
                CursorPosition--;
                continue;
            }

            if (!CursorPosition) /* Do not backup beyond start of line */
            {
                continue;
            }

            /* Remove the character from the line */

            memmove (&Buffer[CursorPosition - 1], &Buffer[CursorPosition],
                (EndOfLine - CursorPosition + 1));

            /* Display the new part of line starting at the new character */

            putchar (_ASCII_BACKSPACE);
            fprintf (stdout, "%s ", &Buffer[CursorPosition - 1]);

            /* Restore cursor */

            ACPI_BACKUP_CURSOR (i, EndOfLine - CursorPosition + 1);
            EndOfLine--;
            if (CursorPosition > 0)
            {
                CursorPosition--;
            }
            continue;

        case _ASCII_NEWLINE: /* Normal exit case at end of command line */
        case _ASCII_NUL:

            /* Return the number of bytes in the command line string */

            if (BytesRead)
            {
                *BytesRead = EndOfLine;
            }

            /* Echo, terminate string buffer, and exit */

            putchar (InputChar);
            Buffer[EndOfLine] = 0;
            return (AE_OK);

        case _ASCII_TAB:

            /* Ignore */

            continue;

        case EOF:

            return (AE_ERROR);

        case _ASCII_ESCAPE:

            /* Check for escape sequences of the form "ESC[x" */

            InputChar = getchar ();
            if (InputChar != _ASCII_LEFT_BRACKET)
            {
                continue; /* Ignore this ESC, does not have the '[' */
            }

            /* Get the code following the ESC [ */

            InputChar = getchar (); /* Backup one character */
            switch (InputChar)
            {
            case _ASCII_LEFT_ARROW:

                if (CursorPosition > 0)
                {
                    putchar (_ASCII_BACKSPACE);
                    CursorPosition--;
                }
                continue;

            case _ASCII_RIGHT_ARROW:
                /*
                 * Move one character forward. Do this without sending
                 * ESC sequence to the terminal for max portability.
                 */
                if (CursorPosition < EndOfLine)
                {
                    /* Backup to start of line and print the entire line */

                    ACPI_BACKUP_CURSOR (i, CursorPosition);
                    fprintf (stdout, "%s", Buffer);

                    /* Backup to where the cursor should be */

                    CursorPosition++;
                    ACPI_BACKUP_CURSOR (i, EndOfLine - CursorPosition);
                }
                continue;

            case _ASCII_UP_ARROW:

                /* If no commands available or at start of history list, ignore */

                if (!CurrentCommandIndex)
                {
                    continue;
                }

                /* Manage our up/down progress */

                if (CurrentCommandIndex > PreviousCommandIndex)
                {
                    CurrentCommandIndex = PreviousCommandIndex;
                }

                /* Get the historical command from the debugger */

                NextCommand = AcpiDbGetHistoryByIndex (CurrentCommandIndex);
                if (!NextCommand)
                {
                    return (AE_ERROR);
                }

                /* Make this the active command and echo it */

                AcpiAcClearLine (EndOfLine, CursorPosition);
                strcpy (Buffer, NextCommand);
                fprintf (stdout, "%s", Buffer);
                EndOfLine = CursorPosition = strlen (Buffer);

                PreviousCommandIndex = CurrentCommandIndex;
                CurrentCommandIndex--;
                continue;

            case _ASCII_DOWN_ARROW:

                if (!MaxCommandIndex) /* Any commands available? */
                {
                    continue;
                }

                /* Manage our up/down progress */

                if (CurrentCommandIndex < PreviousCommandIndex)
                {
                    CurrentCommandIndex = PreviousCommandIndex;
                }

                /* If we are the end of the history list, output a clear new line */

                if ((CurrentCommandIndex + 1) > MaxCommandIndex)
                {
                    AcpiAcClearLine (EndOfLine, CursorPosition);
                    EndOfLine = CursorPosition = 0;
                    PreviousCommandIndex = CurrentCommandIndex;
                    continue;
                }

                PreviousCommandIndex = CurrentCommandIndex;
                CurrentCommandIndex++;

                 /* Get the historical command from the debugger */

                NextCommand = AcpiDbGetHistoryByIndex (CurrentCommandIndex);
                if (!NextCommand)
                {
                    return (AE_ERROR);
                }

                /* Make this the active command and echo it */

                AcpiAcClearLine (EndOfLine, CursorPosition);
                strcpy (Buffer, NextCommand);
                fprintf (stdout, "%s", Buffer);
                EndOfLine = CursorPosition = strlen (Buffer);
                continue;

            case 0x31:
            case 0x32:
            case 0x33:
            case 0x34:
            case 0x35:
            case 0x36:
                /*
                 * Ignore the various keys like insert/delete/home/end, etc.
                 * But we must eat the final character of the ESC sequence.
                 */
                InputChar = getchar ();
                continue;

            default:

                /* Ignore random escape sequences that we don't care about */

                continue;
            }
            continue;
        }
    }
}
