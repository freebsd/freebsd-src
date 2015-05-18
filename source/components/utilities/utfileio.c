/*******************************************************************************
 *
 * Module Name: utfileio - simple file I/O routines
 *
 ******************************************************************************/

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
#include "actables.h"
#include "acapps.h"

#ifdef ACPI_ASL_COMPILER
#include "aslcompiler.h"
#endif


#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("utfileio")


#ifdef ACPI_APPLICATION

/* Local prototypes */

static ACPI_STATUS
AcpiUtCheckTextModeCorruption (
    UINT8                   *Table,
    UINT32                  TableLength,
    UINT32                  FileLength);

static ACPI_STATUS
AcpiUtReadTable (
    FILE                    *fp,
    ACPI_TABLE_HEADER       **Table,
    UINT32                  *TableLength);


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCheckTextModeCorruption
 *
 * PARAMETERS:  Table           - Table buffer
 *              TableLength     - Length of table from the table header
 *              FileLength      - Length of the file that contains the table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check table for text mode file corruption where all linefeed
 *              characters (LF) have been replaced by carriage return linefeed
 *              pairs (CR/LF).
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiUtCheckTextModeCorruption (
    UINT8                   *Table,
    UINT32                  TableLength,
    UINT32                  FileLength)
{
    UINT32                  i;
    UINT32                  Pairs = 0;


    if (TableLength != FileLength)
    {
        ACPI_WARNING ((AE_INFO,
            "File length (0x%X) is not the same as the table length (0x%X)",
            FileLength, TableLength));
    }

    /* Scan entire table to determine if each LF has been prefixed with a CR */

    for (i = 1; i < FileLength; i++)
    {
        if (Table[i] == 0x0A)
        {
            if (Table[i - 1] != 0x0D)
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


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtReadTable
 *
 * PARAMETERS:  fp              - File that contains table
 *              Table           - Return value, buffer with table
 *              TableLength     - Return value, length of table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load the DSDT from the file pointer
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiUtReadTable (
    FILE                    *fp,
    ACPI_TABLE_HEADER       **Table,
    UINT32                  *TableLength)
{
    ACPI_TABLE_HEADER       TableHeader;
    UINT32                  Actual;
    ACPI_STATUS             Status;
    UINT32                  FileSize;
    BOOLEAN                 StandardHeader = TRUE;
    INT32                   Count;

    /* Get the file size */

    FileSize = CmGetFileSize (fp);
    if (FileSize == ACPI_UINT32_MAX)
    {
        return (AE_ERROR);
    }

    if (FileSize < 4)
    {
        return (AE_BAD_HEADER);
    }

    /* Read the signature */

    fseek (fp, 0, SEEK_SET);

    Count = fread (&TableHeader, 1, sizeof (ACPI_TABLE_HEADER), fp);
    if (Count != sizeof (ACPI_TABLE_HEADER))
    {
        AcpiOsPrintf ("Could not read the table header\n");
        return (AE_BAD_HEADER);
    }

    /* The RSDP table does not have standard ACPI header */

    if (ACPI_VALIDATE_RSDP_SIG (TableHeader.Signature))
    {
        *TableLength = FileSize;
        StandardHeader = FALSE;
    }
    else
    {

#if 0
        /* Validate the table header/length */

        Status = AcpiTbValidateTableHeader (&TableHeader);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Table header is invalid!\n");
            return (Status);
        }
#endif

        /* File size must be at least as long as the Header-specified length */

        if (TableHeader.Length > FileSize)
        {
            AcpiOsPrintf (
                "TableHeader length [0x%X] greater than the input file size [0x%X]\n",
                TableHeader.Length, FileSize);

#ifdef ACPI_ASL_COMPILER
            AcpiOsPrintf ("File is corrupt or is ASCII text -- "
                "it must be a binary file\n");
#endif
            return (AE_BAD_HEADER);
        }

#ifdef ACPI_OBSOLETE_CODE
        /* We only support a limited number of table types */

        if (!ACPI_COMPARE_NAME ((char *) TableHeader.Signature, ACPI_SIG_DSDT) &&
            !ACPI_COMPARE_NAME ((char *) TableHeader.Signature, ACPI_SIG_PSDT) &&
            !ACPI_COMPARE_NAME ((char *) TableHeader.Signature, ACPI_SIG_SSDT))
        {
            AcpiOsPrintf ("Table signature [%4.4s] is invalid or not supported\n",
                (char *) TableHeader.Signature);
            ACPI_DUMP_BUFFER (&TableHeader, sizeof (ACPI_TABLE_HEADER));
            return (AE_ERROR);
        }
#endif

        *TableLength = TableHeader.Length;
    }

    /* Allocate a buffer for the table */

    *Table = AcpiOsAllocate ((size_t) FileSize);
    if (!*Table)
    {
        AcpiOsPrintf (
            "Could not allocate memory for ACPI table %4.4s (size=0x%X)\n",
            TableHeader.Signature, *TableLength);
        return (AE_NO_MEMORY);
    }

    /* Get the rest of the table */

    fseek (fp, 0, SEEK_SET);
    Actual = fread (*Table, 1, (size_t) FileSize, fp);
    if (Actual == FileSize)
    {
        if (StandardHeader)
        {
            /* Now validate the checksum */

            Status = AcpiTbVerifyChecksum ((void *) *Table,
                        ACPI_CAST_PTR (ACPI_TABLE_HEADER, *Table)->Length);

            if (Status == AE_BAD_CHECKSUM)
            {
                Status = AcpiUtCheckTextModeCorruption ((UINT8 *) *Table,
                            FileSize, (*Table)->Length);
                return (Status);
            }
        }
        return (AE_OK);
    }

    if (Actual > 0)
    {
        AcpiOsPrintf ("Warning - reading table, asked for %X got %X\n",
            FileSize, Actual);
        return (AE_OK);
    }

    AcpiOsPrintf ("Error - could not read the table file\n");
    AcpiOsFree (*Table);
    *Table = NULL;
    *TableLength = 0;
    return (AE_ERROR);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtReadTableFromFile
 *
 * PARAMETERS:  Filename         - File where table is located
 *              Table            - Where a pointer to the table is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get an ACPI table from a file
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtReadTableFromFile (
    char                    *Filename,
    ACPI_TABLE_HEADER       **Table)
{
    FILE                    *File;
    UINT32                  FileSize;
    UINT32                  TableLength;
    ACPI_STATUS             Status = AE_ERROR;


    /* Open the file, get current size */

    File = fopen (Filename, "rb");
    if (!File)
    {
        perror ("Could not open input file");
        return (Status);
    }

    FileSize = CmGetFileSize (File);
    if (FileSize == ACPI_UINT32_MAX)
    {
        goto Exit;
    }

    /* Get the entire file */

    fprintf (stderr, "Reading ACPI table from file %10s - Length %.8u (0x%06X)\n",
        Filename, FileSize, FileSize);

    Status = AcpiUtReadTable (File, Table, &TableLength);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not get table from the file\n");
    }

Exit:
    fclose(File);
    return (Status);
}

#endif
