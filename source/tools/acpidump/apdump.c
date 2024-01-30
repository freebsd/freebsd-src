/******************************************************************************
 *
 * Module Name: apdump - Dump routines for ACPI tables (acpidump)
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2023, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code. No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

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

        if (!AcpiUtValidNameseg (Table->Signature))
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
 * RETURN:      TRUE if the checksum appears to be valid. FALSE otherwise.
 *
 * DESCRIPTION: Check for a valid ACPI table checksum.
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
        /* We don't have to check for a CDAT here, since CDAT is not in the RSDT/XSDT */

        Status = AcpiUtVerifyChecksum (Table, Table->Length);
    }

    if (ACPI_FAILURE (Status))
    {
        fprintf (stderr, "%4.4s: Warning: wrong checksum in table\n",
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
 * DESCRIPTION: Obtain table length according to table signature.
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
        return (AcpiTbGetRsdpLength (Rsdp));
    }

    /* Normal ACPI table */

    return (Table->Length);
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
     * Dump the table with header for use with acpixtract utility.
     * Note: simplest to just always emit a 64-bit address. AcpiXtract
     * utility can handle this.
     */
    fprintf (Gbl_OutputFile, "%4.4s @ 0x%8.8X%8.8X\n",
        Table->Signature, ACPI_FORMAT_UINT64 (Address));

    AcpiUtDumpBufferToFile (Gbl_OutputFile,
        ACPI_CAST_PTR (UINT8, Table), TableLength,
        DB_BYTE_DISPLAY, 0);
    fprintf (Gbl_OutputFile, "\n");
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
    int                     TableStatus;
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

        TableStatus = ApDumpTableBuffer (Table, Instance, Address);
        ACPI_FREE (Table);

        if (TableStatus)
        {
            break;
        }
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

    Status = AcpiUtStrtoul64 (AsciiAddress, &LongAddress);
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
    ACPI_FREE (Table);
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
    char                    LocalSignature [ACPI_NAMESEG_SIZE + 1];
    UINT32                  Instance;
    ACPI_TABLE_HEADER       *Table;
    ACPI_PHYSICAL_ADDRESS   Address;
    ACPI_STATUS             Status;
    int                     TableStatus;


    if (strlen (Signature) != ACPI_NAMESEG_SIZE)
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

    if (ACPI_COMPARE_NAMESEG (LocalSignature, "FADT"))
    {
        strcpy (LocalSignature, ACPI_SIG_FADT);
    }
    else if (ACPI_COMPARE_NAMESEG (LocalSignature, "MADT"))
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

        TableStatus = ApDumpTableBuffer (Table, Instance, Address);
        ACPI_FREE (Table);

        if (TableStatus)
        {
            break;
        }
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
    int                     TableStatus = -1;


    /* Get the entire ACPI table from the file */

    Table = ApGetTableFromFile (Pathname, &FileSize);
    if (!Table)
    {
        return (-1);
    }

    if (!AcpiUtValidNameseg (Table->Signature))
    {
        fprintf (stderr,
            "No valid ACPI signature was found in input file %s\n",
            Pathname);
    }

    /* File must be at least as long as the table length */

    if (Table->Length > FileSize)
    {
        fprintf (stderr,
            "Table length (0x%X) is too large for input file (0x%X) %s\n",
            Table->Length, FileSize, Pathname);
        goto Exit;
    }

    if (Gbl_VerboseMode)
    {
        fprintf (stderr,
            "Input file:  %s contains table [%4.4s], 0x%X (%u) bytes\n",
            Pathname, Table->Signature, FileSize, FileSize);
    }

    TableStatus = ApDumpTableBuffer (Table, 0, 0);

Exit:
    ACPI_FREE (Table);
    return (TableStatus);
}
