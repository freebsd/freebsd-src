/******************************************************************************
 *
 * Module Name: aetables - ACPI table setup/install for acpiexec utility
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

#include "aecommon.h"
#include "aetables.h"

#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("aetables")

/* Local prototypes */

static void
AeInitializeTableHeader (
    ACPI_TABLE_HEADER       *Header,
    char                    *Signature,
    UINT32                  Length);

void
AeTableOverride (
    ACPI_TABLE_HEADER       *ExistingTable,
    ACPI_TABLE_HEADER       **NewTable);

/* User table (DSDT) */

static ACPI_TABLE_HEADER        *DsdtToInstallOverride;

/* Non-AML tables that are constructed locally and installed */

static ACPI_TABLE_RSDP          LocalRSDP;
static ACPI_TABLE_FACS          LocalFACS;
static ACPI_TABLE_HEADER        LocalTEST;
static ACPI_TABLE_HEADER        LocalBADTABLE;

/*
 * We need a local FADT so that the hardware subcomponent will function,
 * even though the underlying OSD HW access functions don't do anything.
 */
static ACPI_TABLE_FADT          LocalFADT;

/*
 * Use XSDT so that both 32- and 64-bit versions of this utility will
 * function automatically.
 */
static ACPI_TABLE_XSDT          *LocalXSDT;

#define BASE_XSDT_TABLES        9
#define BASE_XSDT_SIZE          ((BASE_XSDT_TABLES) * sizeof (UINT64))

#define ACPI_MAX_INIT_TABLES    (32)
static ACPI_TABLE_DESC          Tables[ACPI_MAX_INIT_TABLES];


/******************************************************************************
 *
 * FUNCTION:    AeTableOverride
 *
 * DESCRIPTION: Local implementation of AcpiOsTableOverride.
 *              Exercise the override mechanism
 *
 *****************************************************************************/

void
AeTableOverride (
    ACPI_TABLE_HEADER       *ExistingTable,
    ACPI_TABLE_HEADER       **NewTable)
{

    if (!AcpiGbl_LoadTestTables)
    {
        *NewTable = NULL;
        return;
    }

    /* This code exercises the table override mechanism in the core */

    if (ACPI_COMPARE_NAME (ExistingTable->Signature, ACPI_SIG_DSDT))
    {
        *NewTable = DsdtToInstallOverride;
    }

    /* This code tests override of dynamically loaded tables */

    else if (ACPI_COMPARE_NAME (ExistingTable->Signature, "OEM9"))
    {
        *NewTable = ACPI_CAST_PTR (ACPI_TABLE_HEADER, Ssdt3Code);
    }
}


/******************************************************************************
 *
 * FUNCTION:    AeInitializeTableHeader
 *
 * PARAMETERS:  Header          - A valid standard ACPI table header
 *              Signature       - Signature to insert
 *              Length          - Length of the table
 *
 * RETURN:      None. Header is modified.
 *
 * DESCRIPTION: Initialize the table header for a local ACPI table.
 *
 *****************************************************************************/

static void
AeInitializeTableHeader (
    ACPI_TABLE_HEADER       *Header,
    char                    *Signature,
    UINT32                  Length)
{

    ACPI_MOVE_NAME (Header->Signature, Signature);
    Header->Length = Length;

    Header->OemRevision = 0x1001;
    strncpy (Header->OemId, "Intel", ACPI_OEM_ID_SIZE);
    strncpy (Header->OemTableId, "AcpiExec", ACPI_OEM_TABLE_ID_SIZE);
    strncpy (Header->AslCompilerId, "INTL", ACPI_NAME_SIZE);
    Header->AslCompilerRevision = 0x20131218;
}


/******************************************************************************
 *
 * FUNCTION:    AeBuildLocalTables
 *
 * PARAMETERS:  TableCount      - Number of tables on the command line
 *              TableList       - List of actual tables from files
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Build a complete ACPI table chain, with a local RSDP, XSDT,
 *              FADT, and several other test tables.
 *
 *****************************************************************************/

ACPI_STATUS
AeBuildLocalTables (
    UINT32                  TableCount,
    AE_TABLE_DESC           *TableList)
{
    ACPI_PHYSICAL_ADDRESS   DsdtAddress = 0;
    UINT32                  XsdtSize;
    AE_TABLE_DESC           *NextTable;
    UINT32                  NextIndex;
    ACPI_TABLE_FADT         *ExternalFadt = NULL;


    /*
     * Update the table count. For the DSDT, it is not put into the XSDT.
     * For the FADT, this table is already accounted for since we usually
     * install a local FADT.
     */
    NextTable = TableList;
    while (NextTable)
    {
        if (ACPI_COMPARE_NAME (NextTable->Table->Signature, ACPI_SIG_DSDT) ||
            ACPI_COMPARE_NAME (NextTable->Table->Signature, ACPI_SIG_FADT))
        {
            TableCount--;
        }
        NextTable = NextTable->Next;
    }

    XsdtSize = (((TableCount + 1) * sizeof (UINT64)) + sizeof (ACPI_TABLE_HEADER));
    if (AcpiGbl_LoadTestTables)
    {
        XsdtSize += BASE_XSDT_SIZE;
    }

    /* Build an XSDT */

    LocalXSDT = AcpiOsAllocate (XsdtSize);
    if (!LocalXSDT)
    {
        return (AE_NO_MEMORY);
    }

    memset (LocalXSDT, 0, XsdtSize);
    AeInitializeTableHeader ((void *) LocalXSDT, ACPI_SIG_XSDT, XsdtSize);

    LocalXSDT->TableOffsetEntry[0] = ACPI_PTR_TO_PHYSADDR (&LocalFADT);
    NextIndex = 1;

    /*
     * Install the user tables. The DSDT must be installed in the FADT.
     * All other tables are installed directly into the XSDT.
     *
     * Note: The tables are loaded in reverse order from the incoming
     * input, which makes it match the command line order.
     */
    NextTable = TableList;
    while (NextTable)
    {
        /*
         * Incoming DSDT or FADT are special cases. All other tables are
         * just immediately installed into the XSDT.
         */
        if (ACPI_COMPARE_NAME (NextTable->Table->Signature, ACPI_SIG_DSDT))
        {
            if (DsdtAddress)
            {
                printf ("Already found a DSDT, only one allowed\n");
                return (AE_ALREADY_EXISTS);
            }

            /* The incoming user table is a DSDT */

            DsdtAddress = ACPI_PTR_TO_PHYSADDR (NextTable->Table);
            DsdtToInstallOverride = NextTable->Table;
        }
        else if (ACPI_COMPARE_NAME (NextTable->Table->Signature, ACPI_SIG_FADT))
        {
            ExternalFadt = ACPI_CAST_PTR (ACPI_TABLE_FADT, NextTable->Table);
            LocalXSDT->TableOffsetEntry[0] = ACPI_PTR_TO_PHYSADDR (NextTable->Table);
        }
        else
        {
            /* Install the table in the XSDT */

            LocalXSDT->TableOffsetEntry[TableCount - NextIndex + 1] =
                ACPI_PTR_TO_PHYSADDR (NextTable->Table);
            NextIndex++;
        }

        NextTable = NextTable->Next;
    }

    /* Install the optional extra local tables */

    if (AcpiGbl_LoadTestTables)
    {
        LocalXSDT->TableOffsetEntry[NextIndex++] = ACPI_PTR_TO_PHYSADDR (&LocalTEST);
        LocalXSDT->TableOffsetEntry[NextIndex++] = ACPI_PTR_TO_PHYSADDR (&LocalBADTABLE);

        /* Install two SSDTs to test multiple table support */

        LocalXSDT->TableOffsetEntry[NextIndex++] = ACPI_PTR_TO_PHYSADDR (&Ssdt1Code);
        LocalXSDT->TableOffsetEntry[NextIndex++] = ACPI_PTR_TO_PHYSADDR (&Ssdt2Code);

        /* Install the OEM1 table to test LoadTable */

        LocalXSDT->TableOffsetEntry[NextIndex++] = ACPI_PTR_TO_PHYSADDR (&Oem1Code);

        /* Install the OEMx table to test LoadTable */

        LocalXSDT->TableOffsetEntry[NextIndex++] = ACPI_PTR_TO_PHYSADDR (&OemxCode);

         /* Install the ECDT table to test _REG */

        LocalXSDT->TableOffsetEntry[NextIndex++] = ACPI_PTR_TO_PHYSADDR (&EcdtCode);

        /* Install two UEFIs to test multiple table support */

        LocalXSDT->TableOffsetEntry[NextIndex++] = ACPI_PTR_TO_PHYSADDR (&Uefi1Code);
        LocalXSDT->TableOffsetEntry[NextIndex++] = ACPI_PTR_TO_PHYSADDR (&Uefi2Code);
    }

    /* Build an RSDP. Contains a valid XSDT only, no RSDT */

    memset (&LocalRSDP, 0, sizeof (ACPI_TABLE_RSDP));
    ACPI_MAKE_RSDP_SIG (LocalRSDP.Signature);
    memcpy (LocalRSDP.OemId, "Intel", 6);

    LocalRSDP.Revision = 2;
    LocalRSDP.XsdtPhysicalAddress = ACPI_PTR_TO_PHYSADDR (LocalXSDT);
    LocalRSDP.Length = sizeof (ACPI_TABLE_RSDP);

    /* Set checksums for both XSDT and RSDP */

    LocalXSDT->Header.Checksum = 0;
    LocalXSDT->Header.Checksum = (UINT8) -AcpiTbChecksum (
        (void *) LocalXSDT, LocalXSDT->Header.Length);

    LocalRSDP.Checksum = 0;
    LocalRSDP.Checksum = (UINT8) -AcpiTbChecksum (
        (void *) &LocalRSDP, ACPI_RSDP_CHECKSUM_LENGTH);

    if (!DsdtAddress)
    {
        /* Use the local DSDT because incoming table(s) are all SSDT(s) */

        DsdtAddress = ACPI_PTR_TO_PHYSADDR (LocalDsdtCode);
        DsdtToInstallOverride = ACPI_CAST_PTR (ACPI_TABLE_HEADER, LocalDsdtCode);
    }

    /*
     * Build an FADT. There are three options for the FADT:
     * 1) Incoming external FADT specified on the command line
     * 2) A "hardware reduced" local FADT
     * 3) A fully featured local FADT
     */
    memset (&LocalFADT, 0, sizeof (ACPI_TABLE_FADT));

    if (ExternalFadt)
    {
        /*
         * Use the external FADT, but we must update the DSDT/FACS addresses
         * as well as the checksum
         */
        ExternalFadt->Dsdt = (UINT32) DsdtAddress;
        if (!AcpiGbl_ReducedHardware)
        {
            ExternalFadt->Facs = ACPI_PTR_TO_PHYSADDR (&LocalFACS);
        }

        /* Is there room in the FADT for the XDsdst and XFacs 64-bit pointers? */

        if (ExternalFadt->Header.Length > ACPI_PTR_DIFF (&ExternalFadt->XDsdt, ExternalFadt))
        {
            ExternalFadt->XDsdt = DsdtAddress;

            if (!AcpiGbl_ReducedHardware)
            {
                ExternalFadt->XFacs = ACPI_PTR_TO_PHYSADDR (&LocalFACS);
            }
        }

        /* Complete the FADT with the checksum */

        ExternalFadt->Header.Checksum = 0;
        ExternalFadt->Header.Checksum = (UINT8) -AcpiTbChecksum (
            (void *) ExternalFadt, ExternalFadt->Header.Length);
    }
    else if (AcpiGbl_UseHwReducedFadt)
    {
        memcpy (&LocalFADT, HwReducedFadtCode, ACPI_FADT_V5_SIZE);
        LocalFADT.Dsdt = (UINT32) DsdtAddress;
        LocalFADT.XDsdt = DsdtAddress;

        LocalFADT.Header.Checksum = 0;
        LocalFADT.Header.Checksum = (UINT8) -AcpiTbChecksum (
            (void *) &LocalFADT, LocalFADT.Header.Length);
    }
    else
    {
        /*
         * Build a local FADT so we can test the hardware/event init
         */
        LocalFADT.Header.Revision = 5;
        AeInitializeTableHeader ((void *) &LocalFADT, ACPI_SIG_FADT, sizeof (ACPI_TABLE_FADT));

        /* Setup FADT header and DSDT/FACS addresses */

        LocalFADT.Dsdt = 0;
        LocalFADT.Facs = 0;

        LocalFADT.XDsdt = DsdtAddress;
        LocalFADT.XFacs = ACPI_PTR_TO_PHYSADDR (&LocalFACS);

        /* Miscellaneous FADT fields */

        LocalFADT.Gpe0BlockLength = 0x08;
        LocalFADT.Gpe0Block = 0x00001234;

        LocalFADT.Gpe1BlockLength = 0x80;
        LocalFADT.Gpe1Block = 0x00005678;
        LocalFADT.Gpe1Base = 100;

        LocalFADT.Pm1EventLength = 4;
        LocalFADT.Pm1aEventBlock = 0x00001aaa;
        LocalFADT.Pm1bEventBlock = 0x00001bbb;

        LocalFADT.Pm1ControlLength = 2;
        LocalFADT.Pm1aControlBlock = 0xB0;

        LocalFADT.PmTimerLength = 4;
        LocalFADT.PmTimerBlock = 0xA0;

        LocalFADT.Pm2ControlBlock = 0xC0;
        LocalFADT.Pm2ControlLength = 1;

        /* Setup one example X-64 GAS field */

        LocalFADT.XPm1bEventBlock.SpaceId = ACPI_ADR_SPACE_SYSTEM_IO;
        LocalFADT.XPm1bEventBlock.Address = LocalFADT.Pm1bEventBlock;
        LocalFADT.XPm1bEventBlock.BitWidth = (UINT8) ACPI_MUL_8 (LocalFADT.Pm1EventLength);

        /* Complete the FADT with the checksum */

        LocalFADT.Header.Checksum = 0;
        LocalFADT.Header.Checksum = (UINT8) -AcpiTbChecksum (
            (void *) &LocalFADT, LocalFADT.Header.Length);
    }

    /* Build a FACS */

    memset (&LocalFACS, 0, sizeof (ACPI_TABLE_FACS));
    ACPI_MOVE_NAME (LocalFACS.Signature, ACPI_SIG_FACS);

    LocalFACS.Length = sizeof (ACPI_TABLE_FACS);
    LocalFACS.GlobalLock = 0x11AA0011;

    /* Build the optional local tables */

    if (AcpiGbl_LoadTestTables)
    {
        /*
         * Build a fake table [TEST] so that we make sure that the
         * ACPICA core ignores it
         */
        memset (&LocalTEST, 0, sizeof (ACPI_TABLE_HEADER));
        ACPI_MOVE_NAME (LocalTEST.Signature, "TEST");

        LocalTEST.Revision = 1;
        LocalTEST.Length = sizeof (ACPI_TABLE_HEADER);
        LocalTEST.Checksum = (UINT8) -AcpiTbChecksum (
            (void *) &LocalTEST, LocalTEST.Length);

        /*
         * Build a fake table with a bad signature [BAD!] so that we make
         * sure that the ACPICA core ignores it
         */
        memset (&LocalBADTABLE, 0, sizeof (ACPI_TABLE_HEADER));
        ACPI_MOVE_NAME (LocalBADTABLE.Signature, "BAD!");

        LocalBADTABLE.Revision = 1;
        LocalBADTABLE.Length = sizeof (ACPI_TABLE_HEADER);
        LocalBADTABLE.Checksum = (UINT8) -AcpiTbChecksum (
            (void *) &LocalBADTABLE, LocalBADTABLE.Length);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AeInstallTables
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install the various ACPI tables
 *
 *****************************************************************************/

ACPI_STATUS
AeInstallTables (
    void)
{
    ACPI_STATUS             Status;
    ACPI_TABLE_HEADER       Header;
    ACPI_TABLE_HEADER       *Table;


    Status = AcpiInitializeTables (Tables, ACPI_MAX_INIT_TABLES, TRUE);
    AE_CHECK_OK (AcpiInitializeTables, Status);

    Status = AcpiReallocateRootTable ();
    AE_CHECK_OK (AcpiReallocateRootTable, Status);

    Status = AcpiLoadTables ();
    AE_CHECK_OK (AcpiLoadTables, Status);

    /*
     * Test run-time control method installation. Do it twice to test code
     * for an existing name.
     */
    Status = AcpiInstallMethod (MethodCode);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("%s, Could not install method\n",
            AcpiFormatException (Status));
    }

    Status = AcpiInstallMethod (MethodCode);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("%s, Could not install method\n",
            AcpiFormatException (Status));
    }

    if (AcpiGbl_LoadTestTables)
    {
        /* Test multiple table/UEFI support. First, get the headers */

        Status = AcpiGetTableHeader (ACPI_SIG_UEFI, 1, &Header);
        AE_CHECK_OK (AcpiGetTableHeader, Status);

        Status = AcpiGetTableHeader (ACPI_SIG_UEFI, 2, &Header);
        AE_CHECK_OK (AcpiGetTableHeader, Status);

        Status = AcpiGetTableHeader (ACPI_SIG_UEFI, 3, &Header);
        AE_CHECK_STATUS (AcpiGetTableHeader, Status, AE_NOT_FOUND);

        /* Now get the actual tables */

        Status = AcpiGetTable (ACPI_SIG_UEFI, 1, &Table);
        AE_CHECK_OK (AcpiGetTable, Status);

        Status = AcpiGetTable (ACPI_SIG_UEFI, 2, &Table);
        AE_CHECK_OK (AcpiGetTable, Status);

        Status = AcpiGetTable (ACPI_SIG_UEFI, 3, &Table);
        AE_CHECK_STATUS (AcpiGetTable, Status, AE_NOT_FOUND);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetRootPointer
 *
 * PARAMETERS:  Flags       - not used
 *              Address     - Where the root pointer is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Return a local RSDP, used to dynamically load tables via the
 *              standard ACPI mechanism.
 *
 *****************************************************************************/

ACPI_PHYSICAL_ADDRESS
AcpiOsGetRootPointer (
    void)
{

    return (ACPI_PTR_TO_PHYSADDR (&LocalRSDP));
}
