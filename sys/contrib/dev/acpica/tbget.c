/******************************************************************************
 *
 * Module Name: tbget - ACPI Table get* routines
 *              $Revision: 56 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, 2000, 2001, Intel Corp.
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

#define __TBGET_C__

#include "acpi.h"
#include "achware.h"
#include "actables.h"


#define _COMPONENT          ACPI_TABLES
        MODULE_NAME         ("tbget")

#define RSDP_CHECKSUM_LENGTH 20


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetTablePtr
 *
 * PARAMETERS:  TableType       - one of the defined table types
 *              Instance        - Which table of this type
 *              TablePtrLoc     - pointer to location to place the pointer for
 *                                return
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to get the pointer to an ACPI table.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetTablePtr (
    ACPI_TABLE_TYPE         TableType,
    UINT32                  Instance,
    ACPI_TABLE_HEADER       **TablePtrLoc)
{
    ACPI_TABLE_DESC         *TableDesc;
    UINT32                  i;


    FUNCTION_TRACE ("TbGetTablePtr");


    if (!AcpiGbl_DSDT)
    {
        return_ACPI_STATUS (AE_NO_ACPI_TABLES);
    }

    if (TableType > ACPI_TABLE_MAX)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }


    /*
     * For all table types (Single/Multiple), the first
     * instance is always in the list head.
     */
    if (Instance == 1)
    {
        /*
         * Just pluck the pointer out of the global table!
         * Will be null if no table is present
         */
        *TablePtrLoc = AcpiGbl_AcpiTables[TableType].Pointer;
        return_ACPI_STATUS (AE_OK);
    }


    /*
     * Check for instance out of range
     */
    if (Instance > AcpiGbl_AcpiTables[TableType].Count)
    {
        return_ACPI_STATUS (AE_NOT_EXIST);
    }

    /* Walk the list to get the desired table
     * Since the if (Instance == 1) check above checked for the
     * first table, setting TableDesc equal to the .Next member
     * is actually pointing to the second table.  Therefore, we
     * need to walk from the 2nd table until we reach the Instance
     * that the user is looking for and return its table pointer.
     */
    TableDesc = AcpiGbl_AcpiTables[TableType].Next;
    for (i = 2; i < Instance; i++)
    {
        TableDesc = TableDesc->Next;
    }

    /* We are now pointing to the requested table's descriptor */

    *TablePtrLoc = TableDesc->Pointer;

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetTable
 *
 * PARAMETERS:  PhysicalAddress         - Physical address of table to retrieve
 *              *BufferPtr              - If BufferPtr is valid, read data from
 *                                         buffer rather than searching memory
 *              *TableInfo              - Where the table info is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Maps the physical address of table into a logical address
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetTable (
    ACPI_PHYSICAL_ADDRESS   PhysicalAddress,
    ACPI_TABLE_HEADER       *BufferPtr,
    ACPI_TABLE_DESC         *TableInfo)
{
    ACPI_TABLE_HEADER       *TableHeader = NULL;
    ACPI_TABLE_HEADER       *FullTable = NULL;
    UINT32                  Size;
    UINT8                   Allocation;
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("TbGetTable");


    if (!TableInfo)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }


    if (BufferPtr)
    {
        /*
         * Getting data from a buffer, not BIOS tables
         */
        TableHeader = BufferPtr;
        Status = AcpiTbValidateTableHeader (TableHeader);
        if (ACPI_FAILURE (Status))
        {
            /* Table failed verification, map all errors to BAD_DATA */

            return_ACPI_STATUS (AE_BAD_DATA);
        }

        /* Allocate buffer for the entire table */

        FullTable = ACPI_MEM_ALLOCATE (TableHeader->Length);
        if (!FullTable)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        /* Copy the entire table (including header) to the local buffer */

        Size = TableHeader->Length;
        MEMCPY (FullTable, BufferPtr, Size);

        /* Save allocation type */

        Allocation = ACPI_MEM_ALLOCATED;
    }


    /*
     * Not reading from a buffer, just map the table's physical memory
     * into our address space.
     */
    else
    {
        Size = SIZE_IN_HEADER;

        Status = AcpiTbMapAcpiTable (PhysicalAddress, &Size, &FullTable);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /* Save allocation type */

        Allocation = ACPI_MEM_MAPPED;
    }


    /* Return values */

    TableInfo->Pointer      = FullTable;
    TableInfo->Length       = Size;
    TableInfo->Allocation   = Allocation;
    TableInfo->BasePointer  = FullTable;

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetAllTables
 *
 * PARAMETERS:  NumberOfTables      - Number of tables to get
 *              TablePtr            - Input buffer pointer, optional
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load and validate all tables other than the RSDT.  The RSDT must
 *              already be loaded and validated.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetAllTables (
    UINT32                  NumberOfTables,
    ACPI_TABLE_HEADER       *TablePtr)
{
    ACPI_STATUS             Status = AE_OK;
    UINT32                  Index;
    ACPI_TABLE_DESC         TableInfo;


    FUNCTION_TRACE ("TbGetAllTables");

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Number of tables: %d\n", NumberOfTables));


    /*
     * Loop through all table pointers found in RSDT.
     * This will NOT include the FACS and DSDT - we must get
     * them after the loop
     */
    for (Index = 0; Index < NumberOfTables; Index++)
    {
        /* Clear the TableInfo each time */

        MEMSET (&TableInfo, 0, sizeof (ACPI_TABLE_DESC));

        /* Get the table via the XSDT */

        Status = AcpiTbGetTable ((ACPI_PHYSICAL_ADDRESS)
                                ACPI_GET_ADDRESS (AcpiGbl_XSDT->TableOffsetEntry[Index]),
                                TablePtr, &TableInfo);

        /* Ignore a table that failed verification */

        if (Status == AE_BAD_DATA)
        {
            continue;
        }

        /* However, abort on serious errors */

        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /* Recognize and install the table */

        Status = AcpiTbInstallTable (TablePtr, &TableInfo);
        if (ACPI_FAILURE (Status))
        {
            /*
             * Unrecognized or unsupported table, delete it and ignore the
             * error.  Just get as many tables as we can, later we will
             * determine if there are enough tables to continue.
             */
            AcpiTbUninstallTable (&TableInfo);
        }
    }


    /*
     * Convert the FADT to a common format.  This allows earlier revisions of the
     * table to coexist with newer versions, using common access code.
     */
    Status = AcpiTbConvertTableFadt ();
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }


    /*
     * Get the minimum set of ACPI tables, namely:
     *
     * 1) FADT (via RSDT in loop above)
     * 2) FACS
     * 3) DSDT
     *
     */

    /*
     * Get the FACS (must have the FADT first, from loop above)
     * AcpiTbGetTableFacs will fail if FADT pointer is not valid
     */
    Status = AcpiTbGetTableFacs (TablePtr, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Install the FACS */

    Status = AcpiTbInstallTable (TablePtr, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Create the common FACS pointer table
     * (Contains pointers to the original table)
     */
    Status = AcpiTbBuildCommonFacs (&TableInfo);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }


    /*
     * Get the DSDT (We know that the FADT is valid now)
     */
    Status = AcpiTbGetTable ((ACPI_PHYSICAL_ADDRESS) ACPI_GET_ADDRESS (AcpiGbl_FADT->XDsdt),
                                TablePtr, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Install the DSDT */

    Status = AcpiTbInstallTable (TablePtr, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Dump the DSDT Header */

    ACPI_DEBUG_PRINT ((ACPI_DB_TABLES, "Hex dump of DSDT Header:\n"));
    DUMP_BUFFER ((UINT8 *) AcpiGbl_DSDT, sizeof (ACPI_TABLE_HEADER));

    /* Dump the entire DSDT */

    ACPI_DEBUG_PRINT ((ACPI_DB_TABLES,
        "Hex dump of DSDT (After header), size %d (%x)\n",
        AcpiGbl_DSDT->Length, AcpiGbl_DSDT->Length));
    DUMP_BUFFER ((UINT8 *) (AcpiGbl_DSDT + 1), AcpiGbl_DSDT->Length);

    /*
     * Initialize the capabilities flags.
     * Assumes that platform supports ACPI_MODE since we have tables!
     */
    AcpiGbl_SystemFlags |= AcpiHwGetModeCapabilities ();


    /* Always delete the RSDP mapping, we are done with it */

    AcpiTbDeleteAcpiTable (ACPI_TABLE_RSDP);

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbVerifyRsdp
 *
 * PARAMETERS:  NumberOfTables      - Where the table count is placed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load and validate the RSDP (ptr) and RSDT (table)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbVerifyRsdp (
    ACPI_PHYSICAL_ADDRESS   RsdpPhysicalAddress)
{
    ACPI_TABLE_DESC         TableInfo;
    ACPI_STATUS             Status;
    UINT8                   *TablePtr;


    FUNCTION_TRACE ("TbVerifyRsdp");


    /*
     * Obtain access to the RSDP structure
     */
    Status = AcpiOsMapMemory (RsdpPhysicalAddress, sizeof (RSDP_DESCRIPTOR),
                                (void **) &TablePtr);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     *  The signature and checksum must both be correct
     */
    if (STRNCMP ((NATIVE_CHAR *) TablePtr, RSDP_SIG, sizeof (RSDP_SIG)-1) != 0)
    {
        /* Nope, BAD Signature */

        Status = AE_BAD_SIGNATURE;
        goto Cleanup;
    }

    if (AcpiTbChecksum (TablePtr, RSDP_CHECKSUM_LENGTH) != 0)
    {
        /* Nope, BAD Checksum */

        Status = AE_BAD_CHECKSUM;
        goto Cleanup;
    }

    /* TBD: Check extended checksum if table version >= 2 */

    /* The RSDP supplied is OK */

    TableInfo.Pointer      = (ACPI_TABLE_HEADER *) TablePtr;
    TableInfo.Length       = sizeof (RSDP_DESCRIPTOR);
    TableInfo.Allocation   = ACPI_MEM_MAPPED;
    TableInfo.BasePointer  = TablePtr;

    /* Save the table pointers and allocation info */

    Status = AcpiTbInitTableDescriptor (ACPI_TABLE_RSDP, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }


    /* Save the RSDP in a global for easy access */

    AcpiGbl_RSDP = (RSDP_DESCRIPTOR *) TableInfo.Pointer;
    return_ACPI_STATUS (Status);


    /* Error exit */
Cleanup:

    AcpiOsUnmapMemory (TablePtr, sizeof (RSDP_DESCRIPTOR));
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetRsdtAddress
 *
 * PARAMETERS:  None
 *
 * RETURN:      RSDT physical address
 *
 * DESCRIPTION: Extract the address of the RSDT or XSDT, depending on the
 *              version of the RSDP
 *
 ******************************************************************************/

ACPI_PHYSICAL_ADDRESS
AcpiTbGetRsdtAddress (void)
{
    ACPI_PHYSICAL_ADDRESS   PhysicalAddress;


    FUNCTION_ENTRY ();


    /*
     * For RSDP revision 0 or 1, we use the RSDT.
     * For RSDP revision 2 (and above), we use the XSDT
     */
    if (AcpiGbl_RSDP->Revision < 2)
    {
#ifdef _IA64
        /* 0.71 RSDP has 64bit Rsdt address field */
        PhysicalAddress = ((RSDP_DESCRIPTOR_REV071 *)AcpiGbl_RSDP)->RsdtPhysicalAddress;
#else
        PhysicalAddress = (ACPI_PHYSICAL_ADDRESS) AcpiGbl_RSDP->RsdtPhysicalAddress;
#endif
    }

    else
    {
        PhysicalAddress = (ACPI_PHYSICAL_ADDRESS)
                            ACPI_GET_ADDRESS (AcpiGbl_RSDP->XsdtPhysicalAddress);
    }

    return (PhysicalAddress);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbValidateRsdt
 *
 * PARAMETERS:  TablePtr        - Addressable pointer to the RSDT.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Validate signature for the RSDT or XSDT
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbValidateRsdt (
    ACPI_TABLE_HEADER       *TablePtr)
{
    UINT32                  NoMatch;


    PROC_NAME ("TbValidateRsdt");


    /*
     * For RSDP revision 0 or 1, we use the RSDT.
     * For RSDP revision 2 (and above), we use the XSDT
     */
    if (AcpiGbl_RSDP->Revision < 2)
    {
        NoMatch = STRNCMP ((char *) TablePtr, RSDT_SIG,
                        sizeof (RSDT_SIG) -1);
    }
    else
    {
        NoMatch = STRNCMP ((char *) TablePtr, XSDT_SIG,
                        sizeof (XSDT_SIG) -1);
    }


    if (NoMatch)
    {
        /* Invalid RSDT or XSDT signature */

        REPORT_ERROR (("Invalid signature where RSDP indicates RSDT/XSDT should be located\n"));

        DUMP_BUFFER (AcpiGbl_RSDP, 20);

        ACPI_DEBUG_PRINT_RAW ((ACPI_DB_ERROR,
            "RSDT/XSDT signature at %X is invalid\n",
            AcpiGbl_RSDP->RsdtPhysicalAddress));

        return (AE_BAD_SIGNATURE);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetTablePointer
 *
 * PARAMETERS:  PhysicalAddress     - Address from RSDT
 *              Flags               - virtual or physical addressing
 *              TablePtr            - Addressable address (output)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create an addressable pointer to an ACPI table
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetTablePointer (
    ACPI_PHYSICAL_ADDRESS   PhysicalAddress,
    UINT32                  Flags,
    UINT32                  *Size,
    ACPI_TABLE_HEADER       **TablePtr)
{
    ACPI_STATUS             Status;


    FUNCTION_ENTRY ();


    if ((Flags & ACPI_MEMORY_MODE) == ACPI_LOGICAL_ADDRESSING)
    {
        *Size = SIZE_IN_HEADER;
        Status = AcpiTbMapAcpiTable (PhysicalAddress, Size, TablePtr);
    }

    else
    {
        *Size = 0;
        *TablePtr = (ACPI_TABLE_HEADER *) (ACPI_TBLPTR) PhysicalAddress;

        Status = AE_OK;
    }

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetTableRsdt
 *
 * PARAMETERS:  NumberOfTables      - Where the table count is placed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load and validate the RSDP (ptr) and RSDT (table)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetTableRsdt (
    UINT32                  *NumberOfTables)
{
    ACPI_TABLE_DESC         TableInfo;
    ACPI_STATUS             Status;
    ACPI_PHYSICAL_ADDRESS   PhysicalAddress;


    FUNCTION_TRACE ("TbGetTableRsdt");


    /*
     * Get the RSDT from the RSDP
     */
    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
        "RSDP located at %p, RSDT physical=%8.8X%8.8X \n",
        AcpiGbl_RSDP, HIDWORD(AcpiGbl_RSDP->RsdtPhysicalAddress),
        LODWORD(AcpiGbl_RSDP->RsdtPhysicalAddress)));


    PhysicalAddress = AcpiTbGetRsdtAddress ();


    /* Get the RSDT/XSDT */

    Status = AcpiTbGetTable (PhysicalAddress, NULL, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Could not get the RSDT, %s\n",
            AcpiFormatException (Status)));
        return_ACPI_STATUS (Status);
    }


    /* Check the RSDT or XSDT signature */

    Status = AcpiTbValidateRsdt (TableInfo.Pointer);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }


    /*
     * Valid RSDT signature, verify the checksum.  If it fails, just
     * print a warning and ignore it.
     */
    Status = AcpiTbVerifyTableChecksum (TableInfo.Pointer);


    /* Convert and/or copy to an XSDT structure */

    Status = AcpiTbConvertToXsdt (&TableInfo, NumberOfTables);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Save the table pointers and allocation info */

    Status = AcpiTbInitTableDescriptor (ACPI_TABLE_XSDT, &TableInfo);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    AcpiGbl_XSDT = (XSDT_DESCRIPTOR *) TableInfo.Pointer;

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "XSDT located at %p\n", AcpiGbl_XSDT));

    return_ACPI_STATUS (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiTbGetTableFacs
 *
 * PARAMETERS:  *BufferPtr              - If BufferPtr is valid, read data from
 *                                        buffer rather than searching memory
 *              *TableInfo              - Where the table info is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Returns a pointer to the FACS as defined in FADT.  This
 *              function assumes the global variable FADT has been
 *              correctly initialized.  The value of FADT->FirmwareCtrl
 *              into a far pointer which is returned.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiTbGetTableFacs (
    ACPI_TABLE_HEADER       *BufferPtr,
    ACPI_TABLE_DESC         *TableInfo)
{
    ACPI_TABLE_HEADER       *TablePtr = NULL;
    UINT32                  Size;
    UINT8                   Allocation;
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("TbGetTableFacs");


    /* Must have a valid FADT pointer */

    if (!AcpiGbl_FADT)
    {
        return_ACPI_STATUS (AE_NO_ACPI_TABLES);
    }

    Size = sizeof (FACS_DESCRIPTOR);
    if (BufferPtr)
    {
        /*
         * Getting table from a file -- allocate a buffer and
         * read the table.
         */
        TablePtr = ACPI_MEM_ALLOCATE (Size);
        if(!TablePtr)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        MEMCPY (TablePtr, BufferPtr, Size);

        /* Save allocation type */

        Allocation = ACPI_MEM_ALLOCATED;
    }

    else
    {
        /* Just map the physical memory to our address space */

        Status = AcpiTbMapAcpiTable ((ACPI_PHYSICAL_ADDRESS) ACPI_GET_ADDRESS (AcpiGbl_FADT->XFirmwareCtrl),
                                        &Size, &TablePtr);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        /* Save allocation type */

        Allocation = ACPI_MEM_MAPPED;
    }


    /* Return values */

    TableInfo->Pointer      = TablePtr;
    TableInfo->Length       = Size;
    TableInfo->Allocation   = Allocation;
    TableInfo->BasePointer  = TablePtr;

    return_ACPI_STATUS (Status);
}

