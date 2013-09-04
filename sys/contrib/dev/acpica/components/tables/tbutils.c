/******************************************************************************
 *
 * Module Name: tbutils - ACPI Table utilities
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

#define __TBUTILS_C__

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/actables.h>

#define _COMPONENT          ACPI_TABLES
        ACPI_MODULE_NAME    ("tbutils")


/* Local prototypes */

static ACPI_PHYSICAL_ADDRESS
AcpiTbGetRootTableEntry (
    UINT8                   *TableEntry,
    UINT32                  TableEntrySize);


#if (!ACPI_REDUCED_HARDWARE)
/*******************************************************************************
 *
 * FUNCTION:    AcpiTbInitializeFacs
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a permanent mapping for the FADT and save it in a global
 *              for accessing the Global Lock and Firmware Waking Vector
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbInitializeFacs (
    void)
{
    ACPI_STATUS             Status;


    /* If Hardware Reduced flag is set, there is no FACS */

    if (AcpiGbl_ReducedHardware)
    {
        AcpiGbl_FACS = NULL;
        return (AE_OK);
    }

    Status = AcpiGetTableByIndex (ACPI_TABLE_INDEX_FACS,
                ACPI_CAST_INDIRECT_PTR (ACPI_TABLE_HEADER, &AcpiGbl_FACS));
    return (Status);
}
#endif /* !ACPI_REDUCED_HARDWARE */


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbTablesLoaded
 *
 * PARAMETERS:  None
 *
 * RETURN:      TRUE if required ACPI tables are loaded
 *
 * DESCRIPTION: Determine if the minimum required ACPI tables are present
 *              (FADT, FACS, DSDT)
 *
 ******************************************************************************/

BOOLEAN
AcpiTbTablesLoaded (
    void)
{

    if (AcpiGbl_RootTableList.CurrentTableCount >= 3)
    {
        return (TRUE);
    }

    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbCheckDsdtHeader
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Quick compare to check validity of the DSDT. This will detect
 *              if the DSDT has been replaced from outside the OS and/or if
 *              the DSDT header has been corrupted.
 *
 ******************************************************************************/

void
AcpiTbCheckDsdtHeader (
    void)
{

    /* Compare original length and checksum to current values */

    if (AcpiGbl_OriginalDsdtHeader.Length != AcpiGbl_DSDT->Length ||
        AcpiGbl_OriginalDsdtHeader.Checksum != AcpiGbl_DSDT->Checksum)
    {
        ACPI_BIOS_ERROR ((AE_INFO,
            "The DSDT has been corrupted or replaced - "
            "old, new headers below"));
        AcpiTbPrintTableHeader (0, &AcpiGbl_OriginalDsdtHeader);
        AcpiTbPrintTableHeader (0, AcpiGbl_DSDT);

        /* Disable further error messages */

        AcpiGbl_OriginalDsdtHeader.Length = AcpiGbl_DSDT->Length;
        AcpiGbl_OriginalDsdtHeader.Checksum = AcpiGbl_DSDT->Checksum;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbCopyDsdt
 *
 * PARAMETERS:  TableDesc           - Installed table to copy
 *
 * RETURN:      None
 *
 * DESCRIPTION: Implements a subsystem option to copy the DSDT to local memory.
 *              Some very bad BIOSs are known to either corrupt the DSDT or
 *              install a new, bad DSDT. This copy works around the problem.
 *
 ******************************************************************************/

ACPI_TABLE_HEADER *
AcpiTbCopyDsdt (
    UINT32                  TableIndex)
{
    ACPI_TABLE_HEADER       *NewTable;
    ACPI_TABLE_DESC         *TableDesc;


    TableDesc = &AcpiGbl_RootTableList.Tables[TableIndex];

    NewTable = ACPI_ALLOCATE (TableDesc->Length);
    if (!NewTable)
    {
        ACPI_ERROR ((AE_INFO, "Could not copy DSDT of length 0x%X",
            TableDesc->Length));
        return (NULL);
    }

    ACPI_MEMCPY (NewTable, TableDesc->Pointer, TableDesc->Length);
    AcpiTbDeleteTable (TableDesc);
    TableDesc->Pointer = NewTable;
    TableDesc->Flags = ACPI_TABLE_ORIGIN_ALLOCATED;

    ACPI_INFO ((AE_INFO,
        "Forced DSDT copy: length 0x%05X copied locally, original unmapped",
        NewTable->Length));

    return (NewTable);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbInstallTable
 *
 * PARAMETERS:  Address                 - Physical address of DSDT or FACS
 *              Signature               - Table signature, NULL if no need to
 *                                        match
 *              TableIndex              - Index into root table array
 *
 * RETURN:      None
 *
 * DESCRIPTION: Install an ACPI table into the global data structure. The
 *              table override mechanism is called to allow the host
 *              OS to replace any table before it is installed in the root
 *              table array.
 *
 ******************************************************************************/

void
AcpiTbInstallTable (
    ACPI_PHYSICAL_ADDRESS   Address,
    char                    *Signature,
    UINT32                  TableIndex)
{
    ACPI_TABLE_HEADER       *Table;
    ACPI_TABLE_HEADER       *FinalTable;
    ACPI_TABLE_DESC         *TableDesc;


    if (!Address)
    {
        ACPI_ERROR ((AE_INFO, "Null physical address for ACPI table [%s]",
            Signature));
        return;
    }

    /* Map just the table header */

    Table = AcpiOsMapMemory (Address, sizeof (ACPI_TABLE_HEADER));
    if (!Table)
    {
        ACPI_ERROR ((AE_INFO, "Could not map memory for table [%s] at %p",
            Signature, ACPI_CAST_PTR (void, Address)));
        return;
    }

    /* If a particular signature is expected (DSDT/FACS), it must match */

    if (Signature &&
        !ACPI_COMPARE_NAME (Table->Signature, Signature))
    {
        ACPI_BIOS_ERROR ((AE_INFO,
            "Invalid signature 0x%X for ACPI table, expected [%s]",
            *ACPI_CAST_PTR (UINT32, Table->Signature), Signature));
        goto UnmapAndExit;
    }

    /*
     * Initialize the table entry. Set the pointer to NULL, since the
     * table is not fully mapped at this time.
     */
    TableDesc = &AcpiGbl_RootTableList.Tables[TableIndex];

    TableDesc->Address = Address;
    TableDesc->Pointer = NULL;
    TableDesc->Length = Table->Length;
    TableDesc->Flags = ACPI_TABLE_ORIGIN_MAPPED;
    ACPI_MOVE_32_TO_32 (TableDesc->Signature.Ascii, Table->Signature);

    /*
     * ACPI Table Override:
     *
     * Before we install the table, let the host OS override it with a new
     * one if desired. Any table within the RSDT/XSDT can be replaced,
     * including the DSDT which is pointed to by the FADT.
     *
     * NOTE: If the table is overridden, then FinalTable will contain a
     * mapped pointer to the full new table. If the table is not overridden,
     * or if there has been a physical override, then the table will be
     * fully mapped later (in verify table). In any case, we must
     * unmap the header that was mapped above.
     */
    FinalTable = AcpiTbTableOverride (Table, TableDesc);
    if (!FinalTable)
    {
        FinalTable = Table; /* There was no override */
    }

    AcpiTbPrintTableHeader (TableDesc->Address, FinalTable);

    /* Set the global integer width (based upon revision of the DSDT) */

    if (TableIndex == ACPI_TABLE_INDEX_DSDT)
    {
        AcpiUtSetIntegerWidth (FinalTable->Revision);
    }

    /*
     * If we have a physical override during this early loading of the ACPI
     * tables, unmap the table for now. It will be mapped again later when
     * it is actually used. This supports very early loading of ACPI tables,
     * before virtual memory is fully initialized and running within the
     * host OS. Note: A logical override has the ACPI_TABLE_ORIGIN_OVERRIDE
     * flag set and will not be deleted below.
     */
    if (FinalTable != Table)
    {
        AcpiTbDeleteTable (TableDesc);
    }


UnmapAndExit:

    /* Always unmap the table header that we mapped above */

    AcpiOsUnmapMemory (Table, sizeof (ACPI_TABLE_HEADER));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetRootTableEntry
 *
 * PARAMETERS:  TableEntry          - Pointer to the RSDT/XSDT table entry
 *              TableEntrySize      - sizeof 32 or 64 (RSDT or XSDT)
 *
 * RETURN:      Physical address extracted from the root table
 *
 * DESCRIPTION: Get one root table entry. Handles 32-bit and 64-bit cases on
 *              both 32-bit and 64-bit platforms
 *
 * NOTE:        ACPI_PHYSICAL_ADDRESS is 32-bit on 32-bit platforms, 64-bit on
 *              64-bit platforms.
 *
 ******************************************************************************/

static ACPI_PHYSICAL_ADDRESS
AcpiTbGetRootTableEntry (
    UINT8                   *TableEntry,
    UINT32                  TableEntrySize)
{
    UINT64                  Address64;


    /*
     * Get the table physical address (32-bit for RSDT, 64-bit for XSDT):
     * Note: Addresses are 32-bit aligned (not 64) in both RSDT and XSDT
     */
    if (TableEntrySize == sizeof (UINT32))
    {
        /*
         * 32-bit platform, RSDT: Return 32-bit table entry
         * 64-bit platform, RSDT: Expand 32-bit to 64-bit and return
         */
        return ((ACPI_PHYSICAL_ADDRESS) (*ACPI_CAST_PTR (UINT32, TableEntry)));
    }
    else
    {
        /*
         * 32-bit platform, XSDT: Truncate 64-bit to 32-bit and return
         * 64-bit platform, XSDT: Move (unaligned) 64-bit to local,
         *  return 64-bit
         */
        ACPI_MOVE_64_TO_64 (&Address64, TableEntry);

#if ACPI_MACHINE_WIDTH == 32
        if (Address64 > ACPI_UINT32_MAX)
        {
            /* Will truncate 64-bit address to 32 bits, issue warning */

            ACPI_BIOS_WARNING ((AE_INFO,
                "64-bit Physical Address in XSDT is too large (0x%8.8X%8.8X),"
                " truncating",
                ACPI_FORMAT_UINT64 (Address64)));
        }
#endif
        return ((ACPI_PHYSICAL_ADDRESS) (Address64));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbParseRootTable
 *
 * PARAMETERS:  Rsdp                    - Pointer to the RSDP
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to parse the Root System Description
 *              Table (RSDT or XSDT)
 *
 * NOTE:        Tables are mapped (not copied) for efficiency. The FACS must
 *              be mapped and cannot be copied because it contains the actual
 *              memory location of the ACPI Global Lock.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbParseRootTable (
    ACPI_PHYSICAL_ADDRESS   RsdpAddress)
{
    ACPI_TABLE_RSDP         *Rsdp;
    UINT32                  TableEntrySize;
    UINT32                  i;
    UINT32                  TableCount;
    ACPI_TABLE_HEADER       *Table;
    ACPI_PHYSICAL_ADDRESS   Address;
    UINT32                  Length;
    UINT8                   *TableEntry;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (TbParseRootTable);


    /*
     * Map the entire RSDP and extract the address of the RSDT or XSDT
     */
    Rsdp = AcpiOsMapMemory (RsdpAddress, sizeof (ACPI_TABLE_RSDP));
    if (!Rsdp)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    AcpiTbPrintTableHeader (RsdpAddress,
        ACPI_CAST_PTR (ACPI_TABLE_HEADER, Rsdp));

    /* Differentiate between RSDT and XSDT root tables */

    if (Rsdp->Revision > 1 && Rsdp->XsdtPhysicalAddress)
    {
        /*
         * Root table is an XSDT (64-bit physical addresses). We must use the
         * XSDT if the revision is > 1 and the XSDT pointer is present, as per
         * the ACPI specification.
         */
        Address = (ACPI_PHYSICAL_ADDRESS) Rsdp->XsdtPhysicalAddress;
        TableEntrySize = sizeof (UINT64);
    }
    else
    {
        /* Root table is an RSDT (32-bit physical addresses) */

        Address = (ACPI_PHYSICAL_ADDRESS) Rsdp->RsdtPhysicalAddress;
        TableEntrySize = sizeof (UINT32);
    }

    /*
     * It is not possible to map more than one entry in some environments,
     * so unmap the RSDP here before mapping other tables
     */
    AcpiOsUnmapMemory (Rsdp, sizeof (ACPI_TABLE_RSDP));


    /* Map the RSDT/XSDT table header to get the full table length */

    Table = AcpiOsMapMemory (Address, sizeof (ACPI_TABLE_HEADER));
    if (!Table)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    AcpiTbPrintTableHeader (Address, Table);

    /* Get the length of the full table, verify length and map entire table */

    Length = Table->Length;
    AcpiOsUnmapMemory (Table, sizeof (ACPI_TABLE_HEADER));

    if (Length < sizeof (ACPI_TABLE_HEADER))
    {
        ACPI_BIOS_ERROR ((AE_INFO,
            "Invalid table length 0x%X in RSDT/XSDT", Length));
        return_ACPI_STATUS (AE_INVALID_TABLE_LENGTH);
    }

    Table = AcpiOsMapMemory (Address, Length);
    if (!Table)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Validate the root table checksum */

    Status = AcpiTbVerifyChecksum (Table, Length);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsUnmapMemory (Table, Length);
        return_ACPI_STATUS (Status);
    }

    /* Calculate the number of tables described in the root table */

    TableCount = (UINT32) ((Table->Length - sizeof (ACPI_TABLE_HEADER)) /
        TableEntrySize);

    /*
     * First two entries in the table array are reserved for the DSDT
     * and FACS, which are not actually present in the RSDT/XSDT - they
     * come from the FADT
     */
    TableEntry = ACPI_CAST_PTR (UINT8, Table) + sizeof (ACPI_TABLE_HEADER);
    AcpiGbl_RootTableList.CurrentTableCount = 2;

    /*
     * Initialize the root table array from the RSDT/XSDT
     */
    for (i = 0; i < TableCount; i++)
    {
        if (AcpiGbl_RootTableList.CurrentTableCount >=
            AcpiGbl_RootTableList.MaxTableCount)
        {
            /* There is no more room in the root table array, attempt resize */

            Status = AcpiTbResizeRootTableList ();
            if (ACPI_FAILURE (Status))
            {
                ACPI_WARNING ((AE_INFO, "Truncating %u table entries!",
                    (unsigned) (TableCount -
                    (AcpiGbl_RootTableList.CurrentTableCount - 2))));
                break;
            }
        }

        /* Get the table physical address (32-bit for RSDT, 64-bit for XSDT) */

        AcpiGbl_RootTableList.Tables[AcpiGbl_RootTableList.CurrentTableCount].Address =
            AcpiTbGetRootTableEntry (TableEntry, TableEntrySize);

        TableEntry += TableEntrySize;
        AcpiGbl_RootTableList.CurrentTableCount++;
    }

    /*
     * It is not possible to map more than one entry in some environments,
     * so unmap the root table here before mapping other tables
     */
    AcpiOsUnmapMemory (Table, Length);

    /*
     * Complete the initialization of the root table array by examining
     * the header of each table
     */
    for (i = 2; i < AcpiGbl_RootTableList.CurrentTableCount; i++)
    {
        AcpiTbInstallTable (AcpiGbl_RootTableList.Tables[i].Address,
            NULL, i);

        /* Special case for FADT - get the DSDT and FACS */

        if (ACPI_COMPARE_NAME (
                &AcpiGbl_RootTableList.Tables[i].Signature, ACPI_SIG_FADT))
        {
            AcpiTbParseFadt (i);
        }
    }

    return_ACPI_STATUS (AE_OK);
}
