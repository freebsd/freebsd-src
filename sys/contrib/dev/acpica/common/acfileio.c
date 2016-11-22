/******************************************************************************
 *
 * Module Name: acfileio - Get ACPI tables from file
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

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/actables.h>
#include <contrib/dev/acpica/include/acutils.h>
#include <contrib/dev/acpica/include/acapps.h>

#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("acfileio")


/* Local prototypes */

static ACPI_STATUS
AcGetOneTableFromFile (
    char                    *Filename,
    FILE                    *File,
    UINT8                   GetOnlyAmlTables,
    ACPI_TABLE_HEADER       **Table);

static ACPI_STATUS
AcCheckTextModeCorruption (
    ACPI_TABLE_HEADER       *Table);


/*******************************************************************************
 *
 * FUNCTION:    AcGetAllTablesFromFile
 *
 * PARAMETERS:  Filename            - Table filename
 *              GetOnlyAmlTables    - TRUE if the tables must be AML tables
 *              ReturnListHead      - Where table list is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get all ACPI tables from within a single file.
 *
 ******************************************************************************/

ACPI_STATUS
AcGetAllTablesFromFile (
    char                    *Filename,
    UINT8                   GetOnlyAmlTables,
    ACPI_NEW_TABLE_DESC     **ReturnListHead)
{
    ACPI_NEW_TABLE_DESC     *ListHead = NULL;
    ACPI_NEW_TABLE_DESC     *ListTail = NULL;
    ACPI_NEW_TABLE_DESC     *TableDesc;
    FILE                    *File;
    ACPI_TABLE_HEADER       *Table = NULL;
    UINT32                  FileSize;
    ACPI_STATUS             Status = AE_OK;


    File = fopen (Filename, "rb");
    if (!File)
    {
        fprintf (stderr, "Could not open input file: %s\n", Filename);
        if (errno == ENOENT)
        {
            return (AE_NOT_EXIST);
        }

        return (AE_ERROR);
    }

    /* Get the file size */

    FileSize = CmGetFileSize (File);
    if (FileSize == ACPI_UINT32_MAX)
    {
        Status = AE_ERROR;
        goto Exit;
    }

    fprintf (stderr,
        "Input file %s, Length 0x%X (%u) bytes\n",
        Filename, FileSize, FileSize);

    /* We must have at least one ACPI table header */

    if (FileSize < sizeof (ACPI_TABLE_HEADER))
    {
        Status = AE_BAD_HEADER;
        goto Exit;
    }

    /* Check for an non-binary file */

    if (!AcIsFileBinary (File))
    {
        fprintf (stderr,
            "    %s: File does not appear to contain a valid AML table\n",
            Filename);
        Status = AE_TYPE;
        goto Exit;
    }

    /* Read all tables within the file */

    while (ACPI_SUCCESS (Status))
    {
        /* Get one entire ACPI table */

        Status = AcGetOneTableFromFile (
            Filename, File, GetOnlyAmlTables, &Table);

        if (Status == AE_CTRL_TERMINATE)
        {
            Status = AE_OK;
            break;
        }
        else if (Status == AE_TYPE)
        {
            Status = AE_OK;
            goto Exit;
        }
        else if (ACPI_FAILURE (Status))
        {
            goto Exit;
        }

        /* Print table header for iASL/disassembler only */

#ifdef ACPI_ASL_COMPILER

        AcpiTbPrintTableHeader (0, Table);
#endif

        /* Allocate and link a table descriptor */

        TableDesc = AcpiOsAllocate (sizeof (ACPI_NEW_TABLE_DESC));
        if (!TableDesc)
        {
            AcpiOsFree (Table);
            Status = AE_NO_MEMORY;
            goto Exit;
        }

        TableDesc->Table = Table;
        TableDesc->Next = NULL;

        /* Link at the end of the local table list */

        if (!ListHead)
        {
            ListHead = TableDesc;
            ListTail = TableDesc;
        }
        else
        {
            ListTail->Next = TableDesc;
            ListTail = TableDesc;
        }
    }

    /* Add the local table list to the end of the global list */

    if (*ReturnListHead)
    {
        ListTail = *ReturnListHead;
        while (ListTail->Next)
        {
            ListTail = ListTail->Next;
        }

        ListTail->Next = ListHead;
    }
    else
    {
        *ReturnListHead = ListHead;
    }

Exit:
    fclose(File);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcGetOneTableFromFile
 *
 * PARAMETERS:  Filename            - File where table is located
 *              File                - Open FILE pointer to Filename
 *              GetOnlyAmlTables    - TRUE if the tables must be AML tables.
 *              ReturnTable         - Where a pointer to the table is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Read the next ACPI table from a file. Implements support
 *              for multiple tables within a single file. File must already
 *              be open.
 *
 * Note: Loading an RSDP is not supported.
 *
 ******************************************************************************/

static ACPI_STATUS
AcGetOneTableFromFile (
    char                    *Filename,
    FILE                    *File,
    UINT8                   GetOnlyAmlTables,
    ACPI_TABLE_HEADER       **ReturnTable)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_TABLE_HEADER       TableHeader;
    ACPI_TABLE_HEADER       *Table;
    INT32                   Count;
    long                    TableOffset;


    *ReturnTable = NULL;

    /* Get the table header to examine signature and length */

    TableOffset = ftell (File);
    Count = fread (&TableHeader, 1, sizeof (ACPI_TABLE_HEADER), File);
    if (Count != sizeof (ACPI_TABLE_HEADER))
    {
        return (AE_CTRL_TERMINATE);
    }

    /* Validate the table signature/header (limited ASCII chars) */

    Status = AcValidateTableHeader (File, TableOffset);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    if (GetOnlyAmlTables)
    {
        /*
         * Table must be an AML table (DSDT/SSDT).
         * Used for iASL -e option only.
         */
        if (!AcpiUtIsAmlTable (&TableHeader))
        {
            fprintf (stderr,
                "    %s: Table [%4.4s] is not an AML table - ignoring\n",
                Filename, TableHeader.Signature);

            return (AE_TYPE);
        }
    }

    /* Allocate a buffer for the entire table */

    Table = AcpiOsAllocate ((ACPI_SIZE) TableHeader.Length);
    if (!Table)
    {
        return (AE_NO_MEMORY);
    }

    /* Read the entire ACPI table, including header */

    fseek (File, TableOffset, SEEK_SET);

    Count = fread (Table, 1, TableHeader.Length, File);
    if (Count != (INT32) TableHeader.Length)
    {
        Status = AE_ERROR;
        goto ErrorExit;
    }

    /* Validate the checksum (just issue a warning) */

    Status = AcpiTbVerifyChecksum (Table, TableHeader.Length);
    if (ACPI_FAILURE (Status))
    {
        Status = AcCheckTextModeCorruption (Table);
        if (ACPI_FAILURE (Status))
        {
            goto ErrorExit;
        }
    }

    *ReturnTable = Table;
    return (AE_OK);


ErrorExit:
    AcpiOsFree (Table);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcIsFileBinary
 *
 * PARAMETERS:  File                - Open input file
 *
 * RETURN:      TRUE if file appears to be binary
 *
 * DESCRIPTION: Scan a file for any non-ASCII bytes.
 *
 * Note: Maintains current file position.
 *
 ******************************************************************************/

BOOLEAN
AcIsFileBinary (
    FILE                    *File)
{
    UINT8                   Byte;
    BOOLEAN                 IsBinary = FALSE;
    long                    FileOffset;


    /* Scan entire file for any non-ASCII bytes */

    FileOffset = ftell (File);
    while (fread (&Byte, 1, 1, File) == 1)
    {
        if (!isprint (Byte) && !isspace (Byte))
        {
            IsBinary = TRUE;
            goto Exit;
        }
    }

Exit:
    fseek (File, FileOffset, SEEK_SET);
    return (IsBinary);
}


/*******************************************************************************
 *
 * FUNCTION:    AcValidateTableHeader
 *
 * PARAMETERS:  File                - Open input file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Determine if a file seems to contain one or more binary ACPI
 *              tables, via the
 *              following checks on what would be the table header:
 *              1) File must be at least as long as an ACPI_TABLE_HEADER
 *              2) There must be enough room in the file to hold entire table
 *              3) Signature, OemId, OemTableId, AslCompilerId must be ASCII
 *
 * Note: There can be multiple definition blocks per file, so we cannot
 * expect/compare the file size to be equal to the table length. 12/2015.
 *
 * Note: Maintains current file position.
 *
 ******************************************************************************/

ACPI_STATUS
AcValidateTableHeader (
    FILE                    *File,
    long                    TableOffset)
{
    ACPI_TABLE_HEADER       TableHeader;
    ACPI_SIZE               Actual;
    long                    OriginalOffset;
    UINT32                  FileSize;
    UINT32                  i;


    ACPI_FUNCTION_TRACE ("AcValidateTableHeader");


    /* Read a potential table header */

    OriginalOffset = ftell (File);
    fseek (File, TableOffset, SEEK_SET);

    Actual = fread (&TableHeader, 1, sizeof (ACPI_TABLE_HEADER), File);
    fseek (File, OriginalOffset, SEEK_SET);

    if (Actual < sizeof (ACPI_TABLE_HEADER))
    {
        return (AE_ERROR);
    }

    /* Validate the signature (limited ASCII chars) */

    if (!AcpiUtValidNameseg (TableHeader.Signature))
    {
        fprintf (stderr, "Invalid table signature: 0x%8.8X\n",
            *ACPI_CAST_PTR (UINT32, TableHeader.Signature));
        return (AE_BAD_SIGNATURE);
    }

    /* Validate table length against bytes remaining in the file */

    FileSize = CmGetFileSize (File);
    if (TableHeader.Length > (UINT32) (FileSize - TableOffset))
    {
        fprintf (stderr, "Table [%4.4s] is too long for file - "
            "needs: 0x%.2X, remaining in file: 0x%.2X\n",
            TableHeader.Signature, TableHeader.Length,
            (UINT32) (FileSize - TableOffset));
        return (AE_BAD_HEADER);
    }

    /*
     * These fields must be ASCII: OemId, OemTableId, AslCompilerId.
     * We allow a NULL terminator in OemId and OemTableId.
     */
    for (i = 0; i < ACPI_NAME_SIZE; i++)
    {
        if (!ACPI_IS_ASCII ((UINT8) TableHeader.AslCompilerId[i]))
        {
            goto BadCharacters;
        }
    }

    for (i = 0; (i < ACPI_OEM_ID_SIZE) && (TableHeader.OemId[i]); i++)
    {
        if (!ACPI_IS_ASCII ((UINT8) TableHeader.OemId[i]))
        {
            goto BadCharacters;
        }
    }

    for (i = 0; (i < ACPI_OEM_TABLE_ID_SIZE) && (TableHeader.OemTableId[i]); i++)
    {
        if (!ACPI_IS_ASCII ((UINT8) TableHeader.OemTableId[i]))
        {
            goto BadCharacters;
        }
    }

    return (AE_OK);


BadCharacters:

    ACPI_WARNING ((AE_INFO,
        "Table header for [%4.4s] has invalid ASCII character(s)",
        TableHeader.Signature));
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcCheckTextModeCorruption
 *
 * PARAMETERS:  Table           - Table buffer starting with table header
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check table for text mode file corruption where all linefeed
 *              characters (LF) have been replaced by carriage return linefeed
 *              pairs (CR/LF).
 *
 ******************************************************************************/

static ACPI_STATUS
AcCheckTextModeCorruption (
    ACPI_TABLE_HEADER       *Table)
{
    UINT32                  i;
    UINT32                  Pairs = 0;
    UINT8                   *Buffer = ACPI_CAST_PTR (UINT8, Table);


    /* Scan entire table to determine if each LF has been prefixed with a CR */

    for (i = 1; i < Table->Length; i++)
    {
        if (Buffer[i] == 0x0A)
        {
            if (Buffer[i - 1] != 0x0D)
            {
                /* The LF does not have a preceding CR, table not corrupted */

                return (AE_OK);
            }
            else
            {
                /* Found a CR/LF pair */

                Pairs++;
            }

            i++;
        }
    }

    if (!Pairs)
    {
        return (AE_OK);
    }

    /*
     * Entire table scanned, each CR is part of a CR/LF pair --
     * meaning that the table was treated as a text file somewhere.
     *
     * NOTE: We can't "fix" the table, because any existing CR/LF pairs in the
     * original table are left untouched by the text conversion process --
     * meaning that we cannot simply replace CR/LF pairs with LFs.
     */
    AcpiOsPrintf ("Table has been corrupted by text mode conversion\n");
    AcpiOsPrintf ("All LFs (%u) were changed to CR/LF pairs\n", Pairs);
    AcpiOsPrintf ("Table cannot be repaired!\n");

    return (AE_BAD_VALUE);
}
