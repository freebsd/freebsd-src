/******************************************************************************
 *
 * Module Name: apdump - Dump routines for ACPI tables (acpidump)
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
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

#include "acpidump.h"


/* Local prototypes */

static int
ApDumpTableBuffer (
    ACPI_TABLE_HEADER       *Table,
    UINT32                  Instance,
    ACPI_PHYSICAL_ADDRESS   Address);


/******************************************************************************
 *
 * FUNCTION:    ApIsValidHeader
 *
 * PARAMETERS:  Table               - Pointer to table to be validated
 *
 * RETURN:      TRUE if the header appears to be valid. FALSE otherwise
 *
 * DESCRIPTION: Check for a valid ACPI table header
 *
 ******************************************************************************/

BOOLEAN
ApIsValidHeader (
    ACPI_TABLE_HEADER       *Table)
{
    if (!ACPI_VALIDATE_RSDP_SIG (Table->Signature))
    {
        /* Make sure signature is all ASCII and a valid ACPI name */

        if (!AcpiUtValidAcpiName (Table->Signature))
        {
            fprintf (stderr, "Table signature (0x%8.8X) is invalid\n",
                *(UINT32 *) Table->Signature);
            return (FALSE);
        }

        /* Check for minimum table length */

        if (Table->Length < sizeof (ACPI_TABLE_HEADER))
        {
            fprintf (stderr, "Table length (0x%8.8X) is invalid\n",
                Table->Length);
            return (FALSE);
        }
    }

    return (TRUE);
}


/******************************************************************************
 *
 * FUNCTION:    ApIsValidChecksum
 *
 * PARAMETERS:  Table               - Pointer to table to be validated
 *
 * RETURN:      TRUE if the checksum appears to be valid. FALSE otherwise
 *
 * DESCRIPTION: Check for a valid ACPI table checksum
 *
 ******************************************************************************/

BOOLEAN
ApIsValidChecksum (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_TABLE_RSDP         *Rsdp;


    if (ACPI_VALIDATE_RSDP_SIG (Table->Signature))
    {
        /*
         * Checksum for RSDP.
         * Note: Other checksums are computed during the table dump.
         */

        Rsdp = ACPI_CAST_PTR (ACPI_TABLE_RSDP, Table);
        Status = AcpiTbValidateRsdp (Rsdp);
    }
    else
    {
        Status = AcpiTbVerifyChecksum (Table, Table->Length);
    }

    if (ACPI_FAILURE (Status))
    {
        fprintf (stderr, "%4.4s: Warning: wrong checksum\n",
            Table->Signature);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    ApGetTableLength
 *
 * PARAMETERS:  Table               - Pointer to the table
 *
 * RETURN:      Table length
 *
 * DESCRIPTION: Obtain table length according to table signature
 *
 ******************************************************************************/

UINT32
ApGetTableLength (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_TABLE_RSDP         *Rsdp;


    /* Check if table is valid */

    if (!ApIsValidHeader (Table))
    {
        return (0);
    }

    if (ACPI_VALIDATE_RSDP_SIG (Table->Signature))
    {
        Rsdp = ACPI_CAST_PTR (ACPI_TABLE_RSDP, Table);
        return (Rsdp->Length);
    }
    else
    {
        return (Table->Length);
    }
}


/******************************************************************************
 *
 * FUNCTION:    ApDumpTableBuffer
 *
 * PARAMETERS:  Table               - ACPI table to be dumped
 *              Instance            - ACPI table instance no. to be dumped
 *              Address             - Physical address of the table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump an ACPI table in standard ASCII hex format, with a
 *              header that is compatible with the AcpiXtract utility.
 *
 ******************************************************************************/

static int
ApDumpTableBuffer (
    ACPI_TABLE_HEADER       *Table,
    UINT32                  Instance,
    ACPI_PHYSICAL_ADDRESS   Address)
{
    UINT32                  TableLength;


    TableLength = ApGetTableLength (Table);

    /* Print only the header if requested */

    if (Gbl_SummaryMode)
    {
        AcpiTbPrintTableHeader (Address, Table);
        return (0);
    }

    /* Dump to binary file if requested */

    if (Gbl_BinaryMode)
    {
        return (ApWriteToBinaryFile (Table, Instance));
    }

    /*
     * Dump the table with header for use with acpixtract utility
     * Note: simplest to just always emit a 64-bit address. AcpiXtract
     * utility can handle this.
     */
    printf ("%4.4s @ 0x%8.8X%8.8X\n", Table->Signature,
        ACPI_FORMAT_UINT64 (Address));

    AcpiUtDumpBuffer (ACPI_CAST_PTR (UINT8, Table), TableLength,
        DB_BYTE_DISPLAY, 0);
    printf ("\n");
    return (0);
}


/******************************************************************************
 *
 * FUNCTION:    ApDumpAllTables
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get all tables from the RSDT/XSDT (or at least all of the
 *              tables that we can possibly get).
 *
 ******************************************************************************/

int
ApDumpAllTables (
    void)
{
    ACPI_TABLE_HEADER       *Table;
    UINT32                  Instance = 0;
    ACPI_PHYSICAL_ADDRESS   Address;
    ACPI_STATUS             Status;
    UINT32                  i;


    /* Get and dump all available ACPI tables */

    for (i = 0; i < AP_MAX_ACPI_FILES; i++)
    {
        Status = AcpiOsGetTableByIndex (i, &Table, &Instance, &Address);
        if (ACPI_FAILURE (Status))
        {
            /* AE_LIMIT means that no more tables are available */

            if (Status == AE_LIMIT)
            {
                return (0);
            }
            else if (i == 0)
            {
                fprintf (stderr, "Could not get ACPI tables, %s\n",
                    AcpiFormatException (Status));
                return (-1);
            }
            else
            {
                fprintf (stderr, "Could not get ACPI table at index %u, %s\n",
                    i, AcpiFormatException (Status));
                continue;
            }
        }

        if (ApDumpTableBuffer (Table, Instance, Address))
        {
            return (-1);
        }
        free (Table);
    }

    /* Something seriously bad happened if the loop terminates here */

    return (-1);
}


/******************************************************************************
 *
 * FUNCTION:    ApDumpTableByAddress
 *
 * PARAMETERS:  AsciiAddress        - Address for requested ACPI table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get an ACPI table via a physical address and dump it.
 *
 ******************************************************************************/

int
ApDumpTableByAddress (
    char                    *AsciiAddress)
{
    ACPI_PHYSICAL_ADDRESS   Address;
    ACPI_TABLE_HEADER       *Table;
    ACPI_STATUS             Status;
    int                     TableStatus;
    UINT64                  LongAddress;


    /* Convert argument to an integer physical address */

    Status = AcpiUtStrtoul64 (AsciiAddress, 0, &LongAddress);
    if (ACPI_FAILURE (Status))
    {
        fprintf (stderr, "%s: Could not convert to a physical address\n",
            AsciiAddress);
        return (-1);
    }

    Address = (ACPI_PHYSICAL_ADDRESS) LongAddress;
    Status = AcpiOsGetTableByAddress (Address, &Table);
    if (ACPI_FAILURE (Status))
    {
        fprintf (stderr, "Could not get table at 0x%8.8X%8.8X, %s\n",
            ACPI_FORMAT_UINT64 (Address),
            AcpiFormatException (Status));
        return (-1);
    }

    TableStatus = ApDumpTableBuffer (Table, 0, Address);
    free (Table);
    return (TableStatus);
}


/******************************************************************************
 *
 * FUNCTION:    ApDumpTableByName
 *
 * PARAMETERS:  Signature           - Requested ACPI table signature
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get an ACPI table via a signature and dump it. Handles
 *              multiple tables with the same signature (SSDTs).
 *
 ******************************************************************************/

int
ApDumpTableByName (
    char                    *Signature)
{
    char                    LocalSignature [ACPI_NAME_SIZE + 1];
    UINT32                  Instance;
    ACPI_TABLE_HEADER       *Table;
    ACPI_PHYSICAL_ADDRESS   Address;
    ACPI_STATUS             Status;


    if (strlen (Signature) != ACPI_NAME_SIZE)
    {
        fprintf (stderr,
            "Invalid table signature [%s]: must be exactly 4 characters\n",
            Signature);
        return (-1);
    }

    /* Table signatures are expected to be uppercase */

    strcpy (LocalSignature, Signature);
    AcpiUtStrupr (LocalSignature);

    /* To be friendly, handle tables whose signatures do not match the name */

    if (ACPI_COMPARE_NAME (LocalSignature, AP_DUMP_SIG_RSDP))
    {
        strcpy (LocalSignature, AP_DUMP_SIG_RSDP);
    }
    else if (ACPI_COMPARE_NAME (LocalSignature, "FADT"))
    {
        strcpy (LocalSignature, ACPI_SIG_FADT);
    }
    else if (ACPI_COMPARE_NAME (LocalSignature, "MADT"))
    {
        strcpy (LocalSignature, ACPI_SIG_MADT);
    }

    /* Dump all instances of this signature (to handle multiple SSDTs) */

    for (Instance = 0; Instance < AP_MAX_ACPI_FILES; Instance++)
    {
        Status = AcpiOsGetTableByName (LocalSignature, Instance,
            &Table, &Address);
        if (ACPI_FAILURE (Status))
        {
            /* AE_LIMIT means that no more tables are available */

            if (Status == AE_LIMIT)
            {
                return (0);
            }

            fprintf (stderr,
                "Could not get ACPI table with signature [%s], %s\n",
                LocalSignature, AcpiFormatException (Status));
            return (-1);
        }

        if (ApDumpTableBuffer (Table, Instance, Address))
        {
            return (-1);
        }
        free (Table);
    }

    /* Something seriously bad happened if the loop terminates here */

    return (-1);
}


/******************************************************************************
 *
 * FUNCTION:    ApDumpTableFromFile
 *
 * PARAMETERS:  Pathname            - File containing the binary ACPI table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Dump an ACPI table from a binary file
 *
 ******************************************************************************/

int
ApDumpTableFromFile (
    char                    *Pathname)
{
    ACPI_TABLE_HEADER       *Table;
    UINT32                  FileSize = 0;
    int                     TableStatus;


    /* Get the entire ACPI table from the file */

    Table = ApGetTableFromFile (Pathname, &FileSize);
    if (!Table)
    {
        return (-1);
    }

    /* File must be at least as long as the table length */

    if (Table->Length > FileSize)
    {
        fprintf (stderr,
            "Table length (0x%X) is too large for input file (0x%X) %s\n",
            Table->Length, FileSize, Pathname);
        return (-1);
    }

    if (Gbl_VerboseMode)
    {
        fprintf (stderr,
            "Input file:  %s contains table [%4.4s], 0x%X (%u) bytes\n",
            Pathname, Table->Signature, FileSize, FileSize);
    }

    TableStatus = ApDumpTableBuffer (Table, 0, 0);
    free (Table);
    return (TableStatus);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOs* print functions
 *
 * DESCRIPTION: Used for linkage with ACPICA modules
 *
 ******************************************************************************/

void ACPI_INTERNAL_VAR_XFACE
AcpiOsPrintf (
    const char              *Fmt,
    ...)
{
    va_list                 Args;

    va_start (Args, Fmt);
    vfprintf (stdout, Fmt, Args);
    va_end (Args);
}

void
AcpiOsVprintf (
    const char              *Fmt,
    va_list                 Args)
{
    vfprintf (stdout, Fmt, Args);
}
