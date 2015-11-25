/******************************************************************************
 *
 * Module Name: acfileio - Get ACPI tables from file
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

#include "acpi.h"
#include "accommon.h"
#include "acapps.h"
#include "actables.h"
#include "acutils.h"
#include <errno.h>

#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("acfileio")


/* Local prototypes */

static ACPI_STATUS
AcpiAcGetOneTableFromFile (
    char                    *Filename,
    FILE                    *File,
    UINT8                   GetOnlyAmlTables,
    ACPI_TABLE_HEADER       **Table);

static ACPI_STATUS
AcpiAcCheckTextModeCorruption (
    ACPI_TABLE_HEADER       *Table);


/*******************************************************************************
 *
 * FUNCTION:    AcpiAcGetAllTablesFromFile
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
AcpiAcGetAllTablesFromFile (
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
        perror ("Could not open input file");
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
        return (AE_ERROR);
    }

    if (FileSize < 4)
    {
        return (AE_BAD_HEADER);
    }

    /* Read all tables within the file */

    while (ACPI_SUCCESS (Status))
    {
        /* Get one entire ACPI table */

        Status = AcpiAcGetOneTableFromFile (
            Filename, File, GetOnlyAmlTables, &Table);
        if (Status == AE_CTRL_TERMINATE)
        {
            Status = AE_OK;
            break;
        }
        else if (Status == AE_TYPE)
        {
            continue;
        }
        else if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        /* Allocate and link a table descriptor */

        TableDesc = AcpiOsAllocate (sizeof (ACPI_NEW_TABLE_DESC));
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

    fclose(File);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiAcGetOneTableFromFile
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
AcpiAcGetOneTableFromFile (
    char                    *Filename,
    FILE                    *File,
    UINT8                   GetOnlyAmlTables,
    ACPI_TABLE_HEADER       **ReturnTable)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_TABLE_HEADER       TableHeader;
    ACPI_TABLE_HEADER       *Table;
    INT32                   Count;
    long                    Position;


    *ReturnTable = NULL;


    /* Get just the table header to get signature and length */

    Position = ftell (File);
    Count = fread (&TableHeader, 1, sizeof (ACPI_TABLE_HEADER), File);
    if (Count != sizeof (ACPI_TABLE_HEADER))
    {
        return (AE_CTRL_TERMINATE);
    }

    if (GetOnlyAmlTables)
    {
        /* Table must be an AML table (DSDT/SSDT) or FADT */

        if (!ACPI_COMPARE_NAME (TableHeader.Signature, ACPI_SIG_FADT) &&
            !AcpiUtIsAmlTable (&TableHeader))
        {
            fprintf (stderr,
                "    %s: [%4.4s] is not an AML table - ignoring\n",
                Filename, TableHeader.Signature);

            return (AE_TYPE);
        }
    }

    /* Allocate a buffer for the entire table */

    Table = AcpiOsAllocate ((size_t) TableHeader.Length);
    if (!Table)
    {
        return (AE_NO_MEMORY);
    }

    /* Now read the entire table */

    fseek (File, Position, SEEK_SET);

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
        Status = AcpiAcCheckTextModeCorruption (Table);
        if (ACPI_FAILURE (Status))
        {
            goto ErrorExit;
        }
    }

    fprintf (stderr,
        "Loading ACPI table [%4.4s] from file %12s - Length 0x%06X (%u)\n",
        TableHeader.Signature, Filename,
        TableHeader.Length, TableHeader.Length);

    *ReturnTable = Table;
    return (AE_OK);


ErrorExit:
    AcpiOsFree (Table);
    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiAcCheckTextModeCorruption
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
AcpiAcCheckTextModeCorruption (
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
