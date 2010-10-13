/******************************************************************************
 *
 * Module Name: aetables - ACPI table setup/install for acpiexec utility
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2010, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
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
 * to or modifications of the Original Intel Code.  No other license or right
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
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
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
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
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
 *****************************************************************************/

#include "aecommon.h"
#include "aetables.h"

#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("aetables")

/* Local prototypes */

void
AeTableOverride (
    ACPI_TABLE_HEADER       *ExistingTable,
    ACPI_TABLE_HEADER       **NewTable);

ACPI_PHYSICAL_ADDRESS
AeLocalGetRootPointer (
    void);

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

#define BASE_XSDT_TABLES        7
#define BASE_XSDT_SIZE          (sizeof (ACPI_TABLE_XSDT) + \
                                    ((BASE_XSDT_TABLES -1) * sizeof (UINT64)))

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

    /* This code exercises the table override mechanism in the core */

    if (ACPI_COMPARE_NAME (ExistingTable->Signature, ACPI_SIG_DSDT))
    {
        *NewTable = DsdtToInstallOverride;
    }

    /* This code tests override of dynamically loaded tables */

    else if (ACPI_COMPARE_NAME (ExistingTable->Signature, "TSDT"))
    {
        *NewTable = ACPI_CAST_PTR (ACPI_TABLE_HEADER, Ssdt3Code);
    }
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
     * Update the table count. For DSDT, it is not put into the XSDT. For
     * FADT, this is already accounted for since we usually install a
     * local FADT.
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

    XsdtSize = BASE_XSDT_SIZE + (TableCount * sizeof (UINT64));

    /* Build an XSDT */

    LocalXSDT = AcpiOsAllocate (XsdtSize);
    if (!LocalXSDT)
    {
        return (AE_NO_MEMORY);
    }

    ACPI_MEMSET (LocalXSDT, 0, XsdtSize);
    ACPI_STRNCPY (LocalXSDT->Header.Signature, ACPI_SIG_XSDT, 4);
    LocalXSDT->Header.Length = XsdtSize;
    LocalXSDT->Header.Revision = 1;

    LocalXSDT->TableOffsetEntry[0] = ACPI_PTR_TO_PHYSADDR (&LocalTEST);
    LocalXSDT->TableOffsetEntry[1] = ACPI_PTR_TO_PHYSADDR (&LocalBADTABLE);
    LocalXSDT->TableOffsetEntry[2] = ACPI_PTR_TO_PHYSADDR (&LocalFADT);

    /* Install two SSDTs to test multiple table support */

    LocalXSDT->TableOffsetEntry[3] = ACPI_PTR_TO_PHYSADDR (&Ssdt1Code);
    LocalXSDT->TableOffsetEntry[4] = ACPI_PTR_TO_PHYSADDR (&Ssdt2Code);

    /* Install the OEM1 table to test LoadTable */

    LocalXSDT->TableOffsetEntry[5] = ACPI_PTR_TO_PHYSADDR (&Oem1Code);

    /* Install the OEMx table to test LoadTable */

    LocalXSDT->TableOffsetEntry[6] = ACPI_PTR_TO_PHYSADDR (&OemxCode);

    /*
     * Install the user tables. The DSDT must be installed in the FADT.
     * All other tables are installed directly into the XSDT.
     */
    NextIndex = BASE_XSDT_TABLES;
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

            DsdtAddress = ACPI_PTR_TO_PHYSADDR (&DsdtCode);
            DsdtToInstallOverride = NextTable->Table;
        }
        else if (ACPI_COMPARE_NAME (NextTable->Table->Signature, ACPI_SIG_FADT))
        {
            ExternalFadt = ACPI_CAST_PTR (ACPI_TABLE_FADT, NextTable->Table);
            LocalXSDT->TableOffsetEntry[2] = ACPI_PTR_TO_PHYSADDR (NextTable->Table);
        }
        else
        {
            /* Install the table in the XSDT */

            LocalXSDT->TableOffsetEntry[NextIndex] = ACPI_PTR_TO_PHYSADDR (NextTable->Table);
            NextIndex++;
        }

        NextTable = NextTable->Next;
    }

    /* Build an RSDP */

    ACPI_MEMSET (&LocalRSDP, 0, sizeof (ACPI_TABLE_RSDP));
    ACPI_MEMCPY (LocalRSDP.Signature, ACPI_SIG_RSDP, 8);
    ACPI_MEMCPY (LocalRSDP.OemId, "I_TEST", 6);
    LocalRSDP.Revision = 2;
    LocalRSDP.XsdtPhysicalAddress = ACPI_PTR_TO_PHYSADDR (LocalXSDT);
    LocalRSDP.Length = sizeof (ACPI_TABLE_XSDT);

    /* Set checksums for both XSDT and RSDP */

    LocalXSDT->Header.Checksum = (UINT8) -AcpiTbChecksum (
        (void *) LocalXSDT, LocalXSDT->Header.Length);
    LocalRSDP.Checksum = (UINT8) -AcpiTbChecksum (
        (void *) &LocalRSDP, ACPI_RSDP_CHECKSUM_LENGTH);

    if (!DsdtAddress)
    {
        /* Use the local DSDT because incoming table(s) are all SSDT(s) */

        DsdtAddress = ACPI_PTR_TO_PHYSADDR (LocalDsdtCode);
        DsdtToInstallOverride = ACPI_CAST_PTR (ACPI_TABLE_HEADER, LocalDsdtCode);
    }

    if (ExternalFadt)
    {
        /*
         * Use the external FADT, but we must update the DSDT/FACS addresses
         * as well as the checksum
         */
        ExternalFadt->Dsdt = DsdtAddress;
        ExternalFadt->Facs = ACPI_PTR_TO_PHYSADDR (&LocalFACS);

        if (ExternalFadt->Header.Length > ACPI_PTR_DIFF (&ExternalFadt->XDsdt, ExternalFadt))
        {
            ExternalFadt->XDsdt = DsdtAddress;
            ExternalFadt->XFacs = ACPI_PTR_TO_PHYSADDR (&LocalFACS);
        }
        /* Complete the FADT with the checksum */

        ExternalFadt->Header.Checksum = 0;
        ExternalFadt->Header.Checksum = (UINT8) -AcpiTbChecksum (
            (void *) ExternalFadt, ExternalFadt->Header.Length);
    }
    else
    {
        /*
         * Build a local FADT so we can test the hardware/event init
         */
        ACPI_MEMSET (&LocalFADT, 0, sizeof (ACPI_TABLE_FADT));
        ACPI_STRNCPY (LocalFADT.Header.Signature, ACPI_SIG_FADT, 4);

        /* Setup FADT header and DSDT/FACS addresses */

        LocalFADT.Dsdt = 0;
        LocalFADT.Facs = 0;

        LocalFADT.XDsdt = DsdtAddress;
        LocalFADT.XFacs = ACPI_PTR_TO_PHYSADDR (&LocalFACS);

        LocalFADT.Header.Revision = 3;
        LocalFADT.Header.Length = sizeof (ACPI_TABLE_FADT);

        /* Miscellaneous FADT fields */

        LocalFADT.Gpe0BlockLength = 16;
        LocalFADT.Gpe0Block = 0x00001234;

        LocalFADT.Gpe1BlockLength = 6;
        LocalFADT.Gpe1Block = 0x00005678;
        LocalFADT.Gpe1Base = 96;

        LocalFADT.Pm1EventLength = 4;
        LocalFADT.Pm1aEventBlock = 0x00001aaa;
        LocalFADT.Pm1bEventBlock = 0x00001bbb;

        LocalFADT.Pm1ControlLength = 2;
        LocalFADT.Pm1aControlBlock = 0xB0;

        LocalFADT.PmTimerLength = 4;
        LocalFADT.PmTimerBlock = 0xA0;

        LocalFADT.Pm2ControlBlock = 0xC0;
        LocalFADT.Pm2ControlLength = 1;

        /* Setup one example X-64 field */

        LocalFADT.XPm1bEventBlock.SpaceId = ACPI_ADR_SPACE_SYSTEM_IO;
        LocalFADT.XPm1bEventBlock.Address = LocalFADT.Pm1bEventBlock;
        LocalFADT.XPm1bEventBlock.BitWidth = (UINT8) ACPI_MUL_8 (LocalFADT.Pm1EventLength);

        /* Complete the FADT with the checksum */

        LocalFADT.Header.Checksum = 0;
        LocalFADT.Header.Checksum = (UINT8) -AcpiTbChecksum (
            (void *) &LocalFADT, LocalFADT.Header.Length);
    }

    /* Build a FACS */

    ACPI_MEMSET (&LocalFACS, 0, sizeof (ACPI_TABLE_FACS));
    ACPI_STRNCPY (LocalFACS.Signature, ACPI_SIG_FACS, 4);

    LocalFACS.Length = sizeof (ACPI_TABLE_FACS);
    LocalFACS.GlobalLock = 0x11AA0011;

    /*
     * Build a fake table [TEST] so that we make sure that the
     * ACPICA core ignores it
     */
    ACPI_MEMSET (&LocalTEST, 0, sizeof (ACPI_TABLE_HEADER));
    ACPI_STRNCPY (LocalTEST.Signature, "TEST", 4);

    LocalTEST.Revision = 1;
    LocalTEST.Length = sizeof (ACPI_TABLE_HEADER);
    LocalTEST.Checksum = (UINT8) -AcpiTbChecksum (
        (void *) &LocalTEST, LocalTEST.Length);

    /*
     * Build a fake table with a bad signature [BAD!] so that we make
     * sure that the ACPICA core ignores it
     */
    ACPI_MEMSET (&LocalBADTABLE, 0, sizeof (ACPI_TABLE_HEADER));
    ACPI_STRNCPY (LocalBADTABLE.Signature, "BAD!", 4);

    LocalBADTABLE.Revision = 1;
    LocalBADTABLE.Length = sizeof (ACPI_TABLE_HEADER);
    LocalBADTABLE.Checksum = (UINT8) -AcpiTbChecksum (
        (void *) &LocalBADTABLE, LocalBADTABLE.Length);

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

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AeLocalGetRootPointer
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
AeLocalGetRootPointer (
    void)
{

    return ((ACPI_PHYSICAL_ADDRESS) &LocalRSDP);
}
