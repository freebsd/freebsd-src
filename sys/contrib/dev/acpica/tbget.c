/******************************************************************************
 *
 * Module Name: tbget - ACPI Table get* routines
 *              $Revision: 87 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2003, Intel Corp.
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
#include "actables.h"


#define _COMPONENT          ACPI_TABLES
        ACPI_MODULE_NAME    ("tbget")


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetTable
 *
 * PARAMETERS:  Address             - Address of table to retrieve.  Can be
 *                                    Logical or Physical
 *              TableInfo           - Where table info is returned
 *
 * RETURN:      None
 *
 * DESCRIPTION: Get entire table of unknown size.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetTable (
    ACPI_POINTER            *Address,
    ACPI_TABLE_DESC         *TableInfo)
{
    ACPI_STATUS             Status;
    ACPI_TABLE_HEADER       Header;


    ACPI_FUNCTION_TRACE ("TbGetTable");


    /*
     * Get the header in order to get signature and table size
     */
    Status = AcpiTbGetTableHeader (Address, &Header);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Get the entire table */

    Status = AcpiTbGetTableBody (Address, &Header, TableInfo);
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR (("Could not get ACPI table (size %X), %s\n",
            Header.Length, AcpiFormatException (Status)));
        return_ACPI_STATUS (Status);
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetTableHeader
 *
 * PARAMETERS:  Address             - Address of table to retrieve.  Can be
 *                                    Logical or Physical
 *              ReturnHeader        - Where the table header is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get an ACPI table header.  Works in both physical or virtual
 *              addressing mode.  Works with both physical or logical pointers.
 *              Table is either copied or mapped, depending on the pointer
 *              type and mode of the processor.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetTableHeader (
    ACPI_POINTER            *Address,
    ACPI_TABLE_HEADER       *ReturnHeader)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_TABLE_HEADER       *Header = NULL;


    ACPI_FUNCTION_TRACE ("TbGetTableHeader");


    /*
     * Flags contains the current processor mode (Virtual or Physical addressing)
     * The PointerType is either Logical or Physical
     */
    switch (Address->PointerType)
    {
    case ACPI_PHYSMODE_PHYSPTR:
    case ACPI_LOGMODE_LOGPTR:

        /* Pointer matches processor mode, copy the header */

        ACPI_MEMCPY (ReturnHeader, Address->Pointer.Logical, sizeof (ACPI_TABLE_HEADER));
        break;


    case ACPI_LOGMODE_PHYSPTR:

        /* Create a logical address for the physical pointer*/

        Status = AcpiOsMapMemory (Address->Pointer.Physical, sizeof (ACPI_TABLE_HEADER),
                                    (void *) &Header);
        if (ACPI_FAILURE (Status))
        {
            ACPI_REPORT_ERROR (("Could not map memory at %8.8X%8.8X for length %X\n",
                ACPI_FORMAT_UINT64 (Address->Pointer.Physical),
                sizeof (ACPI_TABLE_HEADER)));
            return_ACPI_STATUS (Status);
        }

        /* Copy header and delete mapping */

        ACPI_MEMCPY (ReturnHeader, Header, sizeof (ACPI_TABLE_HEADER));
        AcpiOsUnmapMemory (Header, sizeof (ACPI_TABLE_HEADER));
        break;


    default:

        ACPI_REPORT_ERROR (("Invalid address flags %X\n",
            Address->PointerType));
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetTableBody
 *
 * PARAMETERS:  Address             - Address of table to retrieve.  Can be
 *                                    Logical or Physical
 *              Header              - Header of the table to retrieve
 *              TableInfo           - Where the table info is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get an entire ACPI table with support to allow the host OS to
 *              replace the table with a newer version (table override.)
 *              Works in both physical or virtual
 *              addressing mode.  Works with both physical or logical pointers.
 *              Table is either copied or mapped, depending on the pointer
 *              type and mode of the processor.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetTableBody (
    ACPI_POINTER            *Address,
    ACPI_TABLE_HEADER       *Header,
    ACPI_TABLE_DESC         *TableInfo)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("TbGetTableBody");


    if (!TableInfo || !Address)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     * Attempt table override.
     */
    Status = AcpiTbTableOverride (Header, TableInfo);
    if (ACPI_SUCCESS (Status))
    {
        /* Table was overridden by the host OS */

        return_ACPI_STATUS (Status);
    }

    /* No override, get the original table */

    Status = AcpiTbGetThisTable (Address, Header, TableInfo);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbTableOverride
 *
 * PARAMETERS:  Header              - Pointer to table header
 *              TableInfo           - Return info if table is overridden
 *
 * RETURN:      None
 *
 * DESCRIPTION: Attempts override of current table with a new one if provided
 *              by the host OS.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbTableOverride (
    ACPI_TABLE_HEADER       *Header,
    ACPI_TABLE_DESC         *TableInfo)
{
    ACPI_TABLE_HEADER       *NewTable;
    ACPI_STATUS             Status;
    ACPI_POINTER            Address;


    ACPI_FUNCTION_TRACE ("TbTableOverride");


    /*
     * The OSL will examine the header and decide whether to override this
     * table.  If it decides to override, a table will be returned in NewTable,
     * which we will then copy.
     */
    Status = AcpiOsTableOverride (Header, &NewTable);
    if (ACPI_FAILURE (Status))
    {
        /* Some severe error from the OSL, but we basically ignore it */

        ACPI_REPORT_ERROR (("Could not override ACPI table, %s\n",
            AcpiFormatException (Status)));
        return_ACPI_STATUS (Status);
    }

    if (!NewTable)
    {
        /* No table override */

        return_ACPI_STATUS (AE_NO_ACPI_TABLES);
    }

    /*
     * We have a new table to override the old one.  Get a copy of
     * the new one.  We know that the new table has a logical pointer.
     */
    Address.PointerType     = ACPI_LOGICAL_POINTER | ACPI_LOGICAL_ADDRESSING;
    Address.Pointer.Logical = NewTable;

    Status = AcpiTbGetThisTable (&Address, NewTable, TableInfo);
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR (("Could not copy override ACPI table, %s\n",
            AcpiFormatException (Status)));
        return_ACPI_STATUS (Status);
    }

    /* Copy the table info */

    ACPI_REPORT_INFO (("Table [%4.4s] replaced by host OS\n",
        TableInfo->Pointer->Signature));

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetThisTable
 *
 * PARAMETERS:  Address             - Address of table to retrieve.  Can be
 *                                    Logical or Physical
 *              Header              - Header of the table to retrieve
 *              TableInfo           - Where the table info is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get an entire ACPI table.  Works in both physical or virtual
 *              addressing mode.  Works with both physical or logical pointers.
 *              Table is either copied or mapped, depending on the pointer
 *              type and mode of the processor.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetThisTable (
    ACPI_POINTER            *Address,
    ACPI_TABLE_HEADER       *Header,
    ACPI_TABLE_DESC         *TableInfo)
{
    ACPI_TABLE_HEADER       *FullTable = NULL;
    UINT8                   Allocation;
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE ("TbGetThisTable");


    /*
     * Flags contains the current processor mode (Virtual or Physical addressing)
     * The PointerType is either Logical or Physical
     */
    switch (Address->PointerType)
    {
    case ACPI_PHYSMODE_PHYSPTR:
    case ACPI_LOGMODE_LOGPTR:

        /* Pointer matches processor mode, copy the table to a new buffer */

        FullTable = ACPI_MEM_ALLOCATE (Header->Length);
        if (!FullTable)
        {
            ACPI_REPORT_ERROR (("Could not allocate table memory for [%4.4s] length %X\n",
                Header->Signature, Header->Length));
            return_ACPI_STATUS (AE_NO_MEMORY);
        }

        /* Copy the entire table (including header) to the local buffer */

        ACPI_MEMCPY (FullTable, Address->Pointer.Logical, Header->Length);

        /* Save allocation type */

        Allocation = ACPI_MEM_ALLOCATED;
        break;


    case ACPI_LOGMODE_PHYSPTR:

        /*
         * Just map the table's physical memory
         * into our address space.
         */
        Status = AcpiOsMapMemory (Address->Pointer.Physical, (ACPI_SIZE) Header->Length,
                                    (void *) &FullTable);
        if (ACPI_FAILURE (Status))
        {
            ACPI_REPORT_ERROR (("Could not map memory for table [%4.4s] at %8.8X%8.8X for length %X\n",
                Header->Signature,
                ACPI_FORMAT_UINT64 (Address->Pointer.Physical), Header->Length));
            return (Status);
        }

        /* Save allocation type */

        Allocation = ACPI_MEM_MAPPED;
        break;


    default:

        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Invalid address flags %X\n",
            Address->PointerType));
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     * Validate checksum for _most_ tables,
     * even the ones whose signature we don't recognize
     */
    if (TableInfo->Type != ACPI_TABLE_FACS)
    {
        Status = AcpiTbVerifyTableChecksum (FullTable);

#if (!ACPI_CHECKSUM_ABORT)
        if (ACPI_FAILURE (Status))
        {
            /* Ignore the error if configuration says so */

            Status = AE_OK;
        }
#endif
    }

    /* Return values */

    TableInfo->Pointer      = FullTable;
    TableInfo->Length       = (ACPI_SIZE) Header->Length;
    TableInfo->Allocation   = Allocation;

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
        "Found table [%4.4s] at %8.8X%8.8X, mapped/copied to %p\n",
        FullTable->Signature,
        ACPI_FORMAT_UINT64 (Address->Pointer.Physical), FullTable));

    return_ACPI_STATUS (Status);
}


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


    ACPI_FUNCTION_TRACE ("TbGetTablePtr");


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
        /* Get the first */

        *TablePtrLoc = NULL;
        if (AcpiGbl_TableLists[TableType].Next)
        {
            *TablePtrLoc = AcpiGbl_TableLists[TableType].Next->Pointer;
        }
        return_ACPI_STATUS (AE_OK);
    }

    /*
     * Check for instance out of range
     */
    if (Instance > AcpiGbl_TableLists[TableType].Count)
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
    TableDesc = AcpiGbl_TableLists[TableType].Next;
    for (i = 2; i < Instance; i++)
    {
        TableDesc = TableDesc->Next;
    }

    /* We are now pointing to the requested table's descriptor */

    *TablePtrLoc = TableDesc->Pointer;

    return_ACPI_STATUS (AE_OK);
}

