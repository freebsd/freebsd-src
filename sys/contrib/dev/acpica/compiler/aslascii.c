/******************************************************************************
 *
 * Module Name: aslascii - ASCII detection and support routines
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2015, Intel Corp.
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
#include <contrib/dev/acpica/include/acapps.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslascii")


/* Local prototypes */

static void
FlConsumeAnsiComment (
    FILE                    *Handle,
    ASL_FILE_STATUS         *Status);

static void
FlConsumeNewComment (
    FILE                    *Handle,
    ASL_FILE_STATUS         *Status);


/*******************************************************************************
 *
 * FUNCTION:    FlCheckForAcpiTable
 *
 * PARAMETERS:  Handle              - Open input file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Determine if a file seems to be a binary ACPI table, via the
 *              following checks on what would be the table header:
 *              0) File must be at least as long as an ACPI_TABLE_HEADER
 *              1) The header length field must match the file size
 *              2) Signature, OemId, OemTableId, AslCompilerId must be ASCII
 *
 ******************************************************************************/

ACPI_STATUS
FlCheckForAcpiTable (
    FILE                    *Handle)
{
    ACPI_TABLE_HEADER       Table;
    UINT32                  FileSize;
    size_t                  Actual;
    UINT32                  i;


    /* Read a potential table header */

    Actual = fread (&Table, 1, sizeof (ACPI_TABLE_HEADER), Handle);
    fseek (Handle, 0, SEEK_SET);

    if (Actual < sizeof (ACPI_TABLE_HEADER))
    {
        return (AE_ERROR);
    }

    /* Header length field must match the file size */

    FileSize = CmGetFileSize (Handle);
    if (Table.Length != FileSize)
    {
        return (AE_ERROR);
    }

    /*
     * These fields must be ASCII:
     * Signature, OemId, OemTableId, AslCompilerId.
     * We allow a NULL terminator in OemId and OemTableId.
     */
    for (i = 0; i < ACPI_NAME_SIZE; i++)
    {
        if (!ACPI_IS_ASCII ((UINT8) Table.Signature[i]))
        {
            return (AE_ERROR);
        }

        if (!ACPI_IS_ASCII ((UINT8) Table.AslCompilerId[i]))
        {
            return (AE_ERROR);
        }
    }

    for (i = 0; (i < ACPI_OEM_ID_SIZE) && (Table.OemId[i]); i++)
    {
        if (!ACPI_IS_ASCII ((UINT8) Table.OemId[i]))
        {
            return (AE_ERROR);
        }
    }

    for (i = 0; (i < ACPI_OEM_TABLE_ID_SIZE) && (Table.OemTableId[i]); i++)
    {
        if (!ACPI_IS_ASCII ((UINT8) Table.OemTableId[i]))
        {
            return (AE_ERROR);
        }
    }

    printf ("Binary file appears to be a valid ACPI table, disassembling\n");
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    FlCheckForAscii
 *
 * PARAMETERS:  Filename            - Full input filename
 *              DisplayErrors       - TRUE if error messages desired
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Verify that the input file is entirely ASCII. Ignores characters
 *              within comments. Note: does not handle nested comments and does
 *              not handle comment delimiters within string literals. However,
 *              on the rare chance this happens and an invalid character is
 *              missed, the parser will catch the error by failing in some
 *              spectactular manner.
 *
 ******************************************************************************/

ACPI_STATUS
FlCheckForAscii (
    char                    *Filename,
    BOOLEAN                 DisplayErrors)
{
    UINT8                   Byte;
    ACPI_SIZE               BadBytes = 0;
    BOOLEAN                 OpeningComment = FALSE;
    ASL_FILE_STATUS         Status;
    FILE                    *Handle;


    /* Open file in text mode so file offset is always accurate */

    Handle = fopen (Filename, "rb");
    if (!Handle)
    {
        perror ("Could not open input file");
        return (AE_ERROR);
    }

    Status.Line = 1;
    Status.Offset = 0;

    /* Read the entire file */

    while (fread (&Byte, 1, 1, Handle) == 1)
    {
        /* Ignore comment fields (allow non-ascii within) */

        if (OpeningComment)
        {
            /* Check for second comment open delimiter */

            if (Byte == '*')
            {
                FlConsumeAnsiComment (Handle, &Status);
            }

            if (Byte == '/')
            {
                FlConsumeNewComment (Handle, &Status);
            }

            /* Reset */

            OpeningComment = FALSE;
        }
        else if (Byte == '/')
        {
            OpeningComment = TRUE;
        }

        /* Check for an ASCII character */

        if (!ACPI_IS_ASCII (Byte))
        {
            if ((BadBytes < 10) && (DisplayErrors))
            {
                AcpiOsPrintf (
                    "Found non-ASCII character in source text: "
                    "0x%2.2X in line %u, file offset 0x%2.2X\n",
                    Byte, Status.Line, Status.Offset);
            }
            BadBytes++;
        }

        /* Ensure character is either printable or a "space" char */

        else if (!isprint (Byte) && !isspace (Byte))
        {
            if ((BadBytes < 10) && (DisplayErrors))
            {
                AcpiOsPrintf (
                    "Found invalid character in source text: "
                    "0x%2.2X in line %u, file offset 0x%2.2X\n",
                    Byte, Status.Line, Status.Offset);
            }
            BadBytes++;
        }

        /* Update line counter as necessary */

        if (Byte == 0x0A)
        {
            Status.Line++;
        }

        Status.Offset++;
    }

    fclose (Handle);

    /* Were there any non-ASCII characters in the file? */

    if (BadBytes)
    {
        if (DisplayErrors)
        {
            AcpiOsPrintf (
                "Total %u invalid characters found in input source text, "
                "could be a binary file\n", BadBytes);
            AslError (ASL_ERROR, ASL_MSG_NON_ASCII, NULL, Filename);
        }

        return (AE_BAD_CHARACTER);
    }

    /* File is OK (100% ASCII) */

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    FlConsumeAnsiComment
 *
 * PARAMETERS:  Handle              - Open input file
 *              Status              - File current status struct
 *
 * RETURN:      Number of lines consumed
 *
 * DESCRIPTION: Step over a normal slash-star type comment
 *
 ******************************************************************************/

static void
FlConsumeAnsiComment (
    FILE                    *Handle,
    ASL_FILE_STATUS         *Status)
{
    UINT8                   Byte;
    BOOLEAN                 ClosingComment = FALSE;


    while (fread (&Byte, 1, 1, Handle) == 1)
    {
        /* Scan until comment close is found */

        if (ClosingComment)
        {
            if (Byte == '/')
            {
                Status->Offset++;
                return;
            }

            if (Byte != '*')
            {
                /* Reset */

                ClosingComment = FALSE;
            }
        }
        else if (Byte == '*')
        {
            ClosingComment = TRUE;
        }

        /* Maintain line count */

        if (Byte == 0x0A)
        {
            Status->Line++;
        }

        Status->Offset++;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    FlConsumeNewComment
 *
 * PARAMETERS:  Handle              - Open input file
 *              Status              - File current status struct
 *
 * RETURN:      Number of lines consumed
 *
 * DESCRIPTION: Step over a slash-slash type of comment
 *
 ******************************************************************************/

static void
FlConsumeNewComment (
    FILE                    *Handle,
    ASL_FILE_STATUS         *Status)
{
    UINT8                   Byte;


    while (fread (&Byte, 1, 1, Handle) == 1)
    {
        Status->Offset++;

        /* Comment ends at newline */

        if (Byte == 0x0A)
        {
            Status->Line++;
            return;
        }
    }
}
