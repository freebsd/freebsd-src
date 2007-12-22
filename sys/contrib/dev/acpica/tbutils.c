/******************************************************************************
 *
 * Module Name: tbutils   - table utilities
 *              $Revision: 1.88 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2007, Intel Corp.
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

#define __TBUTILS_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/actables.h>

#define _COMPONENT          ACPI_TABLES
        ACPI_MODULE_NAME    ("tbutils")

/* Local prototypes */

static ACPI_PHYSICAL_ADDRESS
AcpiTbGetRootTableEntry (
    UINT8                   *TableEntry,
    ACPI_NATIVE_UINT        TableEntrySize);


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

    if (AcpiGbl_RootTableList.Count >= 3)
    {
        return (TRUE);
    }

    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbPrintTableHeader
 *
 * PARAMETERS:  Address             - Table physical address
 *              Header              - Table header
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print an ACPI table header. Special cases for FACS and RSDP.
 *
 ******************************************************************************/

void
AcpiTbPrintTableHeader (
    ACPI_PHYSICAL_ADDRESS   Address,
    ACPI_TABLE_HEADER       *Header)
{

    if (ACPI_COMPARE_NAME (Header->Signature, ACPI_SIG_FACS))
    {
        /* FACS only has signature and length fields of common table header */

        ACPI_INFO ((AE_INFO, "%4.4s @ 0x%p/0x%04X",
            Header->Signature, ACPI_CAST_PTR (void, Address), Header->Length));
    }
    else if (ACPI_COMPARE_NAME (Header->Signature, ACPI_SIG_RSDP))
    {
        /* RSDP has no common fields */

        ACPI_INFO ((AE_INFO, "RSDP @ 0x%p/0x%04X (v%3.3d %6.6s)",
            ACPI_CAST_PTR (void, Address),
            (ACPI_CAST_PTR (ACPI_TABLE_RSDP, Header)->Revision > 0) ?
                ACPI_CAST_PTR (ACPI_TABLE_RSDP, Header)->Length : 20,
            ACPI_CAST_PTR (ACPI_TABLE_RSDP, Header)->Revision,
            ACPI_CAST_PTR (ACPI_TABLE_RSDP, Header)->OemId));
    }
    else
    {
        /* Standard ACPI table with full common header */

        ACPI_INFO ((AE_INFO,
            "%4.4s @ 0x%p/0x%04X (v%3.3d %6.6s %8.8s 0x%08X %4.4s 0x%08X)",
            Header->Signature, ACPI_CAST_PTR (void, Address),
            Header->Length, Header->Revision, Header->OemId,
            Header->OemTableId, Header->OemRevision, Header->AslCompilerId,
            Header->AslCompilerRevision));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbValidateChecksum
 *
 * PARAMETERS:  Table               - ACPI table to verify
 *              Length              - Length of entire table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Verifies that the table checksums to zero. Optionally returns
 *              exception on bad checksum.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbVerifyChecksum (
    ACPI_TABLE_HEADER       *Table,
    UINT32                  Length)
{
    UINT8                   Checksum;


    /* Compute the checksum on the table */

    Checksum = AcpiTbChecksum (ACPI_CAST_PTR (UINT8, Table), Length);

    /* Checksum ok? (should be zero) */

    if (Checksum)
    {
        ACPI_WARNING ((AE_INFO,
            "Incorrect checksum in table [%4.4s] -  %2.2X, should be %2.2X",
            Table->Signature, Table->Checksum, (UINT8) (Table->Checksum - Checksum)));

#if (ACPI_CHECKSUM_ABORT)
        return (AE_BAD_CHECKSUM);
#endif
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbChecksum
 *
 * PARAMETERS:  Buffer          - Pointer to memory region to be checked
 *              Length          - Length of this memory region
 *
 * RETURN:      Checksum (UINT8)
 *
 * DESCRIPTION: Calculates circular checksum of memory region.
 *
 ******************************************************************************/

UINT8
AcpiTbChecksum (
    UINT8                   *Buffer,
    ACPI_NATIVE_UINT        Length)
{
    UINT8                   Sum = 0;
    UINT8                   *End = Buffer + Length;


    while (Buffer < End)
    {
        Sum = (UINT8) (Sum + *(Buffer++));
    }

    return Sum;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbInstallTable
 *
 * PARAMETERS:  Address                 - Physical address of DSDT or FACS
 *              Flags                   - Flags
 *              Signature               - Table signature, NULL if no need to
 *                                        match
 *              TableIndex              - Index into root table array
 *
 * RETURN:      None
 *
 * DESCRIPTION: Install an ACPI table into the global data structure.
 *
 ******************************************************************************/

void
AcpiTbInstallTable (
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT8                   Flags,
    char                    *Signature,
    ACPI_NATIVE_UINT        TableIndex)
{
    ACPI_TABLE_HEADER       *Table;


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
        return;
    }

    /* If a particular signature is expected, signature must match */

    if (Signature &&
        !ACPI_COMPARE_NAME (Table->Signature, Signature))
    {
        ACPI_ERROR ((AE_INFO, "Invalid signature 0x%X for ACPI table [%s]",
            *ACPI_CAST_PTR (UINT32, Table->Signature), Signature));
        goto UnmapAndExit;
    }

    /* Initialize the table entry */

    AcpiGbl_RootTableList.Tables[TableIndex].Address = Address;
    AcpiGbl_RootTableList.Tables[TableIndex].Length = Table->Length;
    AcpiGbl_RootTableList.Tables[TableIndex].Flags = Flags;

    ACPI_MOVE_32_TO_32 (
        &(AcpiGbl_RootTableList.Tables[TableIndex].Signature),
        Table->Signature);

    AcpiTbPrintTableHeader (Address, Table);

    if (TableIndex == ACPI_TABLE_INDEX_DSDT)
    {
        /* Global integer width is based upon revision of the DSDT */

        AcpiUtSetIntegerWidth (Table->Revision);
    }

UnmapAndExit:
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
    ACPI_NATIVE_UINT        TableEntrySize)
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
         * 64-bit platform, XSDT: Move (unaligned) 64-bit to local, return 64-bit
         */
        ACPI_MOVE_64_TO_64 (&Address64, TableEntry);

#if ACPI_MACHINE_WIDTH == 32
        if (Address64 > ACPI_UINT32_MAX)
        {
            /* Will truncate 64-bit address to 32 bits, issue warning */

            ACPI_WARNING ((AE_INFO,
                "64-bit Physical Address in XSDT is too large (%8.8X%8.8X), truncating",
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
 *              Flags                   - Flags
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
    ACPI_PHYSICAL_ADDRESS   RsdpAddress,
    UINT8                   Flags)
{
    ACPI_TABLE_RSDP         *Rsdp;
    ACPI_NATIVE_UINT        TableEntrySize;
    ACPI_NATIVE_UINT        i;
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

    AcpiTbPrintTableHeader (RsdpAddress, ACPI_CAST_PTR (ACPI_TABLE_HEADER, Rsdp));

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
        ACPI_ERROR ((AE_INFO, "Invalid length 0x%X in RSDT/XSDT", Length));
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

    TableCount = (UINT32) ((Table->Length - sizeof (ACPI_TABLE_HEADER)) / TableEntrySize);

    /*
     * First two entries in the table array are reserved for the DSDT and FACS,
     * which are not actually present in the RSDT/XSDT - they come from the FADT
     */
    TableEntry = ACPI_CAST_PTR (UINT8, Table) + sizeof (ACPI_TABLE_HEADER);
    AcpiGbl_RootTableList.Count = 2;

    /*
     * Initialize the root table array from the RSDT/XSDT
     */
    for (i = 0; i < TableCount; i++)
    {
        if (AcpiGbl_RootTableList.Count >= AcpiGbl_RootTableList.Size)
        {
            /* There is no more room in the root table array, attempt resize */

            Status = AcpiTbResizeRootTableList ();
            if (ACPI_FAILURE (Status))
            {
                ACPI_WARNING ((AE_INFO, "Truncating %u table entries!",
                    (unsigned) (AcpiGbl_RootTableList.Size - AcpiGbl_RootTableList.Count)));
                break;
            }
        }

        /* Get the table physical address (32-bit for RSDT, 64-bit for XSDT) */

        AcpiGbl_RootTableList.Tables[AcpiGbl_RootTableList.Count].Address =
            AcpiTbGetRootTableEntry (TableEntry, TableEntrySize);

        TableEntry += TableEntrySize;
        AcpiGbl_RootTableList.Count++;
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
    for (i = 2; i < AcpiGbl_RootTableList.Count; i++)
    {
        AcpiTbInstallTable (AcpiGbl_RootTableList.Tables[i].Address,
            Flags, NULL, i);

        /* Special case for FADT - get the DSDT and FACS */

        if (ACPI_COMPARE_NAME (
                &AcpiGbl_RootTableList.Tables[i].Signature, ACPI_SIG_FADT))
        {
            AcpiTbParseFadt (i, Flags);
        }
    }

    return_ACPI_STATUS (AE_OK);
}
