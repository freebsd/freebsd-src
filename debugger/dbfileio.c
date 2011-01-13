/*******************************************************************************
 *
 * Module Name: dbfileio - Debugger file I/O commands. These can't usually
 *              be used when running the debugger in Ring 0 (Kernel mode)
 *
 ******************************************************************************/

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


#include "acpi.h"
#include "accommon.h"
#include "acdebug.h"

#ifdef ACPI_APPLICATION
#include "actables.h"
#endif

#if (defined ACPI_DEBUGGER || defined ACPI_DISASSEMBLER)

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbfileio")

/*
 * NOTE: this is here for lack of a better place. It is used in all
 * flavors of the debugger, need LCD file
 */
#ifdef ACPI_APPLICATION
#include <stdio.h>
FILE                        *AcpiGbl_DebugFile = NULL;
#endif


#ifdef ACPI_DEBUGGER

/* Local prototypes */

#ifdef ACPI_APPLICATION

static ACPI_STATUS
AcpiDbCheckTextModeCorruption (
    UINT8                   *Table,
    UINT32                  TableLength,
    UINT32                  FileLength);

#endif

/*******************************************************************************
 *
 * FUNCTION:    AcpiDbCloseDebugFile
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: If open, close the current debug output file
 *
 ******************************************************************************/

void
AcpiDbCloseDebugFile (
    void)
{

#ifdef ACPI_APPLICATION

    if (AcpiGbl_DebugFile)
    {
       fclose (AcpiGbl_DebugFile);
       AcpiGbl_DebugFile = NULL;
       AcpiGbl_DbOutputToFile = FALSE;
       AcpiOsPrintf ("Debug output file %s closed\n", AcpiGbl_DbDebugFilename);
    }
#endif
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbOpenDebugFile
 *
 * PARAMETERS:  Name                - Filename to open
 *
 * RETURN:      None
 *
 * DESCRIPTION: Open a file where debug output will be directed.
 *
 ******************************************************************************/

void
AcpiDbOpenDebugFile (
    char                    *Name)
{

#ifdef ACPI_APPLICATION

    AcpiDbCloseDebugFile ();
    AcpiGbl_DebugFile = fopen (Name, "w+");
    if (AcpiGbl_DebugFile)
    {
        AcpiOsPrintf ("Debug output file %s opened\n", Name);
        ACPI_STRCPY (AcpiGbl_DbDebugFilename, Name);
        AcpiGbl_DbOutputToFile = TRUE;
    }
    else
    {
        AcpiOsPrintf ("Could not open debug file %s\n", Name);
    }

#endif
}
#endif


#ifdef ACPI_APPLICATION
/*******************************************************************************
 *
 * FUNCTION:    AcpiDbCheckTextModeCorruption
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
AcpiDbCheckTextModeCorruption (
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
 * FUNCTION:    AcpiDbReadTable
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
AcpiDbReadTable (
    FILE                    *fp,
    ACPI_TABLE_HEADER       **Table,
    UINT32                  *TableLength)
{
    ACPI_TABLE_HEADER       TableHeader;
    UINT32                  Actual;
    ACPI_STATUS             Status;
    UINT32                  FileSize;
    BOOLEAN                 StandardHeader = TRUE;


    /* Get the file size */

    fseek (fp, 0, SEEK_END);
    FileSize = (UINT32) ftell (fp);
    fseek (fp, 0, SEEK_SET);

    if (FileSize < 4)
    {
        return (AE_BAD_HEADER);
    }

    /* Read the signature */

    if (fread (&TableHeader, 1, 4, fp) != 4)
    {
        AcpiOsPrintf ("Could not read the table signature\n");
        return (AE_BAD_HEADER);
    }

    fseek (fp, 0, SEEK_SET);

    /* The RSDT and FACS tables do not have standard ACPI headers */

    if (ACPI_COMPARE_NAME (TableHeader.Signature, "RSD ") ||
        ACPI_COMPARE_NAME (TableHeader.Signature, "FACS"))
    {
        *TableLength = FileSize;
        StandardHeader = FALSE;
    }
    else
    {
        /* Read the table header */

        if (fread (&TableHeader, 1, sizeof (TableHeader), fp) !=
                sizeof (ACPI_TABLE_HEADER))
        {
            AcpiOsPrintf ("Could not read the table header\n");
            return (AE_BAD_HEADER);
        }

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
            return (AE_BAD_HEADER);
        }

#ifdef ACPI_OBSOLETE_CODE
        /* We only support a limited number of table types */

        if (ACPI_STRNCMP ((char *) TableHeader.Signature, DSDT_SIG, 4) &&
            ACPI_STRNCMP ((char *) TableHeader.Signature, PSDT_SIG, 4) &&
            ACPI_STRNCMP ((char *) TableHeader.Signature, SSDT_SIG, 4))
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
                Status = AcpiDbCheckTextModeCorruption ((UINT8 *) *Table,
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
 * FUNCTION:    AeLocalLoadTable
 *
 * PARAMETERS:  Table           - pointer to a buffer containing the entire
 *                                table to be loaded
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to load a table from the caller's
 *              buffer. The buffer must contain an entire ACPI Table including
 *              a valid header. The header fields will be verified, and if it
 *              is determined that the table is invalid, the call will fail.
 *
 ******************************************************************************/

static ACPI_STATUS
AeLocalLoadTable (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status = AE_OK;
/*    ACPI_TABLE_DESC         TableInfo; */


    ACPI_FUNCTION_TRACE (AeLocalLoadTable);
#if 0


    if (!Table)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    TableInfo.Pointer = Table;
    Status = AcpiTbRecognizeTable (&TableInfo, ACPI_TABLE_ALL);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Install the new table into the local data structures */

    Status = AcpiTbInstallTable (&TableInfo);
    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_ALREADY_EXISTS)
        {
            /* Table already exists, no error */

            Status = AE_OK;
        }

        /* Free table allocated by AcpiTbGetTable */

        AcpiTbDeleteSingleTable (&TableInfo);
        return_ACPI_STATUS (Status);
    }

#if (!defined (ACPI_NO_METHOD_EXECUTION) && !defined (ACPI_CONSTANT_EVAL_ONLY))

    Status = AcpiNsLoadTable (TableInfo.InstalledDesc, AcpiGbl_RootNode);
    if (ACPI_FAILURE (Status))
    {
        /* Uninstall table and free the buffer */

        AcpiTbDeleteTablesByType (ACPI_TABLE_ID_DSDT);
        return_ACPI_STATUS (Status);
    }
#endif
#endif

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbReadTableFromFile
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
AcpiDbReadTableFromFile (
    char                    *Filename,
    ACPI_TABLE_HEADER       **Table)
{
    FILE                    *fp;
    UINT32                  TableLength;
    ACPI_STATUS             Status;


    /* Open the file */

    fp = fopen (Filename, "rb");
    if (!fp)
    {
        AcpiOsPrintf ("Could not open input file %s\n", Filename);
        return (AE_ERROR);
    }

    /* Get the entire file */

    fprintf (stderr, "Loading Acpi table from file %s\n", Filename);
    Status = AcpiDbReadTable (fp, Table, &TableLength);
    fclose(fp);

    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not get table from the file\n");
        return (Status);
    }

    return (AE_OK);
 }
#endif


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbGetTableFromFile
 *
 * PARAMETERS:  Filename        - File where table is located
 *              ReturnTable     - Where a pointer to the table is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load an ACPI table from a file
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbGetTableFromFile (
    char                    *Filename,
    ACPI_TABLE_HEADER       **ReturnTable)
{
#ifdef ACPI_APPLICATION
    ACPI_STATUS             Status;
    ACPI_TABLE_HEADER       *Table;
    BOOLEAN                 IsAmlTable = TRUE;


    Status = AcpiDbReadTableFromFile (Filename, &Table);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

#ifdef ACPI_DATA_TABLE_DISASSEMBLY
    IsAmlTable = AcpiUtIsAmlTable (Table);
#endif

    if (IsAmlTable)
    {
        /* Attempt to recognize and install the table */

        Status = AeLocalLoadTable (Table);
        if (ACPI_FAILURE (Status))
        {
            if (Status == AE_ALREADY_EXISTS)
            {
                AcpiOsPrintf ("Table %4.4s is already installed\n",
                    Table->Signature);
            }
            else
            {
                AcpiOsPrintf ("Could not install table, %s\n",
                    AcpiFormatException (Status));
            }

            return (Status);
        }

        fprintf (stderr,
            "Acpi table [%4.4s] successfully installed and loaded\n",
            Table->Signature);
    }

    AcpiGbl_AcpiHardwarePresent = FALSE;
    if (ReturnTable)
    {
        *ReturnTable = Table;
    }


#endif  /* ACPI_APPLICATION */
    return (AE_OK);
}

#endif  /* ACPI_DEBUGGER */

