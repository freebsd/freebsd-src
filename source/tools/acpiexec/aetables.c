/******************************************************************************
 *
 * Module Name: aetables - ACPI table setup/install for acpiexec utility
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

    if (ACPI_COMPARE_NAMESEG (ExistingTable->Signature, ACPI_SIG_DSDT))
    {
        *NewTable = DsdtToInstallOverride;
    }

    /* This code tests override of dynamically loaded tables */

    else if (ACPI_COMPARE_NAMESEG (ExistingTable->Signature, "OEM9"))
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

    ACPI_COPY_NAMESEG (Header->Signature, Signature);
    Header->Length = Length;

    Header->OemRevision = 0x1001;
    memcpy (Header->OemId, "Intel ", ACPI_OEM_ID_SIZE);
    memcpy (Header->OemTableId, "AcpiExec", ACPI_OEM_TABLE_ID_SIZE);
    ACPI_COPY_NAMESEG (Header->AslCompilerId, "INTL");
    Header->AslCompilerRevision = ACPI_CA_VERSION;

    /* Set the checksum, must set to zero first */

    Header->Checksum = 0;
    Header->Checksum = (UINT8) -AcpiUtChecksum (
        (void *) Header, Header->Length);
}


/******************************************************************************
 *
 * FUNCTION:    AeBuildLocalTables
 *
 * PARAMETERS:  TableCount      - Number of tables on the command line
 *              ListHead        - List of actual tables from files
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Build a complete ACPI table chain, with a local RSDP, XSDT,
 *              FADT, and several other test tables.
 *
 *****************************************************************************/

ACPI_STATUS
AeBuildLocalTables (
    ACPI_NEW_TABLE_DESC     *ListHead)
{
    UINT32                  TableCount = 1;
    ACPI_PHYSICAL_ADDRESS   DsdtAddress = 0;
    UINT32                  XsdtSize;
    ACPI_NEW_TABLE_DESC     *NextTable;
    UINT32                  NextIndex;
    ACPI_TABLE_FADT         *ExternalFadt = NULL;


    /*
     * Update the table count. For the DSDT, it is not put into the XSDT.
     * For the FADT, this table is already accounted for since we usually
     * install a local FADT.
     */
    NextTable = ListHead;
    while (NextTable)
    {
        if (!ACPI_COMPARE_NAMESEG (NextTable->Table->Signature, ACPI_SIG_DSDT) &&
            !ACPI_COMPARE_NAMESEG (NextTable->Table->Signature, ACPI_SIG_FADT))
        {
            TableCount++;
        }

        NextTable = NextTable->Next;
    }

    XsdtSize = (((TableCount + 1) * sizeof (UINT64)) +
        sizeof (ACPI_TABLE_HEADER));
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
    LocalXSDT->TableOffsetEntry[0] = ACPI_PTR_TO_PHYSADDR (&LocalFADT);
    NextIndex = 1;

    /*
     * Install the user tables. The DSDT must be installed in the FADT.
     * All other tables are installed directly into the XSDT.
     */
    NextTable = ListHead;
    while (NextTable)
    {
        /*
         * Incoming DSDT or FADT are special cases. All other tables are
         * just immediately installed into the XSDT.
         */
        if (ACPI_COMPARE_NAMESEG (NextTable->Table->Signature, ACPI_SIG_DSDT))
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
        else if (ACPI_COMPARE_NAMESEG (NextTable->Table->Signature, ACPI_SIG_FADT))
        {
            ExternalFadt = ACPI_CAST_PTR (ACPI_TABLE_FADT, NextTable->Table);
            LocalXSDT->TableOffsetEntry[0] = ACPI_PTR_TO_PHYSADDR (NextTable->Table);
        }
        else
        {
            /* Install the table in the XSDT */

            LocalXSDT->TableOffsetEntry[NextIndex] =
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

    AeInitializeTableHeader ((void *) LocalXSDT, ACPI_SIG_XSDT, XsdtSize);

    LocalRSDP.Checksum = 0;
    LocalRSDP.Checksum = (UINT8) -AcpiUtChecksum (
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
         * Use the external FADT, but we must update the DSDT/FACS
         * addresses as well as the checksum
         */
        ExternalFadt->Dsdt = (UINT32) DsdtAddress;
        if (!AcpiGbl_ReducedHardware)
        {
            ExternalFadt->Facs = ACPI_PTR_TO_PHYSADDR (&LocalFACS);
        }

        /*
         * If there room in the FADT for the XDsdt and XFacs 64-bit
         * pointers, use them.
         */
        if (ExternalFadt->Header.Length > ACPI_PTR_DIFF (
            &ExternalFadt->XDsdt, ExternalFadt))
        {
            ExternalFadt->Dsdt = 0;
            ExternalFadt->Facs = 0;

            ExternalFadt->XDsdt = DsdtAddress;
            if (!AcpiGbl_ReducedHardware)
            {
                ExternalFadt->XFacs = ACPI_PTR_TO_PHYSADDR (&LocalFACS);
            }
        }

        /* Complete the external FADT with the checksum */

        ExternalFadt->Header.Checksum = 0;
        ExternalFadt->Header.Checksum = (UINT8) -AcpiUtChecksum (
            (void *) ExternalFadt, ExternalFadt->Header.Length);
    }
    else if (AcpiGbl_UseHwReducedFadt)
    {
        memcpy (&LocalFADT, HwReducedFadtCode, ACPI_FADT_V5_SIZE);
        LocalFADT.Dsdt = 0;
        LocalFADT.XDsdt = DsdtAddress;
    }
    else
    {
        /*
         * Build a local FADT so we can test the hardware/event init
         */
        LocalFADT.Header.Revision = 5;

        /* Setup FADT header and DSDT/FACS addresses */

        LocalFADT.Dsdt = 0;
        LocalFADT.Facs = 0;

        LocalFADT.XDsdt = DsdtAddress;
        LocalFADT.XFacs = ACPI_PTR_TO_PHYSADDR (&LocalFACS);

        /* Miscellaneous FADT fields */

        LocalFADT.Gpe0BlockLength = 0x20;
        LocalFADT.Gpe0Block = 0x00003210;

        LocalFADT.Gpe1BlockLength = 0x20;
        LocalFADT.Gpe1Block = 0x0000BA98;
        LocalFADT.Gpe1Base = 0x80;

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
        LocalFADT.XPm1bEventBlock.BitWidth = (UINT8)
            ACPI_MUL_8 (LocalFADT.Pm1EventLength);
    }

    AeInitializeTableHeader ((void *) &LocalFADT,
        ACPI_SIG_FADT, sizeof (ACPI_TABLE_FADT));

    /* Build a FACS */

    memset (&LocalFACS, 0, sizeof (ACPI_TABLE_FACS));
    ACPI_COPY_NAMESEG (LocalFACS.Signature, ACPI_SIG_FACS);

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
        ACPI_COPY_NAMESEG (LocalTEST.Signature, "TEST");

        LocalTEST.Revision = 1;
        LocalTEST.Length = sizeof (ACPI_TABLE_HEADER);

        LocalTEST.Checksum = 0;
        LocalTEST.Checksum = (UINT8) -AcpiUtChecksum (
            (void *) &LocalTEST, LocalTEST.Length);

        /*
         * Build a fake table with a bad signature [BAD!] so that we make
         * sure that the ACPICA core ignores it
         */
        memset (&LocalBADTABLE, 0, sizeof (ACPI_TABLE_HEADER));
        ACPI_COPY_NAMESEG (LocalBADTABLE.Signature, "BAD!");

        LocalBADTABLE.Revision = 1;
        LocalBADTABLE.Length = sizeof (ACPI_TABLE_HEADER);

        LocalBADTABLE.Checksum = 0;
        LocalBADTABLE.Checksum = (UINT8) -AcpiUtChecksum (
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
    UINT32                  i;


    Status = AcpiInitializeTables (NULL, ACPI_MAX_INIT_TABLES, TRUE);
    ACPI_CHECK_OK (AcpiInitializeTables, Status);

    /*
     * The following code is prepared to test the deferred table
     * verification mechanism. When AcpiGbl_EnableTableValidation is set
     * to FALSE by default, AcpiReallocateRootTable() sets it back to TRUE
     * and triggers the deferred table verification mechanism accordingly.
     */
    (void) AcpiReallocateRootTable ();

    if (AcpiGbl_LoadTestTables)
    {
        /* Test multiple table/UEFI support. First, get the headers */

        Status = AcpiGetTableHeader (ACPI_SIG_UEFI, 1, &Header);
        ACPI_CHECK_OK (AcpiGetTableHeader, Status);

        Status = AcpiGetTableHeader (ACPI_SIG_UEFI, 2, &Header);
        ACPI_CHECK_OK (AcpiGetTableHeader, Status);

        Status = AcpiGetTableHeader (ACPI_SIG_UEFI, 3, &Header);
        ACPI_CHECK_STATUS (AcpiGetTableHeader, Status, AE_NOT_FOUND);

        /* Now get the actual tables */

        Status = AcpiGetTable (ACPI_SIG_UEFI, 1, &Table);
        ACPI_CHECK_OK (AcpiGetTable, Status);

        Status = AcpiGetTable (ACPI_SIG_UEFI, 2, &Table);
        ACPI_CHECK_OK (AcpiGetTable, Status);

        Status = AcpiGetTable (ACPI_SIG_UEFI, 3, &Table);
        ACPI_CHECK_STATUS (AcpiGetTable, Status, AE_NOT_FOUND);
    }

    /* Check that we can get all of the ACPI tables */

    for (i = 0; ; i++)
    {
        Status = AcpiGetTableByIndex (i, &Table);
        if ((Status == AE_BAD_PARAMETER) || !Table)
        {
            break;
        }

        ACPI_CHECK_OK (AcpiGetTableByIndex, Status);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AeLoadTables
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load the definition block ACPI tables
 *
 *****************************************************************************/

ACPI_STATUS
AeLoadTables (
    void)
{
    ACPI_STATUS             Status;


    Status = AcpiLoadTables ();
    ACPI_CHECK_OK (AcpiLoadTables, Status);

    /*
     * Test run-time control method installation. Do it twice to test code
     * for an existing name.
     */
    Status = AcpiInstallMethod (MethodCode);
    ACPI_CHECK_OK (AcpiInstallMethod, Status);

    Status = AcpiInstallMethod (MethodCode);
    ACPI_CHECK_OK (AcpiInstallMethod, Status);

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
