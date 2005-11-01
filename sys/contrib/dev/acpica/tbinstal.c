/******************************************************************************
 *
 * Module Name: tbinstal - ACPI table installation and removal
 *              $Revision: 1.80 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2005, Intel Corp.
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


#define __TBINSTAL_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/actables.h>


#define _COMPONENT          ACPI_TABLES
        ACPI_MODULE_NAME    ("tbinstal")

/* Local prototypes */

static ACPI_STATUS
AcpiTbMatchSignature (
    char                    *Signature,
    ACPI_TABLE_DESC         *TableInfo,
    UINT8                   SearchType);


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbMatchSignature
 *
 * PARAMETERS:  Signature           - Table signature to match
 *              TableInfo           - Return data
 *              SearchType          - Table type to match (primary/secondary)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Compare signature against the list of "ACPI-subsystem-owned"
 *              tables (DSDT/FADT/SSDT, etc.) Returns the TableTypeID on match.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiTbMatchSignature (
    char                    *Signature,
    ACPI_TABLE_DESC         *TableInfo,
    UINT8                   SearchType)
{
    ACPI_NATIVE_UINT        i;


    ACPI_FUNCTION_TRACE ("TbMatchSignature");


    /* Search for a signature match among the known table types */

    for (i = 0; i < NUM_ACPI_TABLE_TYPES; i++)
    {
        if (!(AcpiGbl_TableData[i].Flags & SearchType))
        {
            continue;
        }

        if (!ACPI_STRNCMP (Signature, AcpiGbl_TableData[i].Signature,
                      AcpiGbl_TableData[i].SigLength))
        {
            /* Found a signature match, return index if requested */

            if (TableInfo)
            {
                TableInfo->Type = (UINT8) i;
            }

            ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
                "Table [%4.4s] is an ACPI table consumed by the core subsystem\n",
                (char *) AcpiGbl_TableData[i].Signature));

            return_ACPI_STATUS (AE_OK);
        }
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
        "Table [%4.4s] is not an ACPI table consumed by the core subsystem - ignored\n",
        (char *) Signature));

    return_ACPI_STATUS (AE_TABLE_NOT_SUPPORTED);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbInstallTable
 *
 * PARAMETERS:  TableInfo           - Return value from AcpiTbGetTableBody
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Install the table into the global data structures.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbInstallTable (
    ACPI_TABLE_DESC         *TableInfo)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("TbInstallTable");


    /* Lock tables while installing */

    Status = AcpiUtAcquireMutex (ACPI_MTX_TABLES);
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR (("Could not acquire table mutex, %s\n",
            AcpiFormatException (Status)));
        return_ACPI_STATUS (Status);
    }

    /*
     * Ignore a table that is already installed. For example, some BIOS
     * ASL code will repeatedly attempt to load the same SSDT.
     */
    Status = AcpiTbIsTableInstalled (TableInfo);
    if (ACPI_FAILURE (Status))
    {
        goto UnlockAndExit;
    }

    /* Install the table into the global data structure */

    Status = AcpiTbInitTableDescriptor (TableInfo->Type, TableInfo);
    if (ACPI_FAILURE (Status))
    {
        ACPI_REPORT_ERROR (("Could not install table [%4.4s], %s\n",
            TableInfo->Pointer->Signature, AcpiFormatException (Status)));
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "%s located at %p\n",
        AcpiGbl_TableData[TableInfo->Type].Name, TableInfo->Pointer));


UnlockAndExit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_TABLES);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbRecognizeTable
 *
 * PARAMETERS:  TableInfo           - Return value from AcpiTbGetTableBody
 *              SearchType          - Table type to match (primary/secondary)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check a table signature for a match against known table types
 *
 * NOTE:  All table pointers are validated as follows:
 *          1) Table pointer must point to valid physical memory
 *          2) Signature must be 4 ASCII chars, even if we don't recognize the
 *             name
 *          3) Table must be readable for length specified in the header
 *          4) Table checksum must be valid (with the exception of the FACS
 *             which has no checksum for some odd reason)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbRecognizeTable (
    ACPI_TABLE_DESC         *TableInfo,
    UINT8                   SearchType)
{
    ACPI_TABLE_HEADER       *TableHeader;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("TbRecognizeTable");


    /* Ensure that we have a valid table pointer */

    TableHeader = (ACPI_TABLE_HEADER *) TableInfo->Pointer;
    if (!TableHeader)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     * We only "recognize" a limited number of ACPI tables -- namely, the
     * ones that are used by the subsystem (DSDT, FADT, etc.)
     *
     * An AE_TABLE_NOT_SUPPORTED means that the table was not recognized.
     * This can be any one of many valid ACPI tables, it just isn't one of
     * the tables that is consumed by the core subsystem
     */
    Status = AcpiTbMatchSignature (TableHeader->Signature,
                TableInfo, SearchType);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiTbValidateTableHeader (TableHeader);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Return the table type and length via the info struct */

    TableInfo->Length = (ACPI_SIZE) TableHeader->Length;

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbInitTableDescriptor
 *
 * PARAMETERS:  TableType           - The type of the table
 *              TableInfo           - A table info struct
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Install a table into the global data structs.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbInitTableDescriptor (
    ACPI_TABLE_TYPE         TableType,
    ACPI_TABLE_DESC         *TableInfo)
{
    ACPI_TABLE_LIST         *ListHead;
    ACPI_TABLE_DESC         *TableDesc;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE_U32 ("TbInitTableDescriptor", TableType);


    /* Allocate a descriptor for this table */

    TableDesc = ACPI_MEM_CALLOCATE (sizeof (ACPI_TABLE_DESC));
    if (!TableDesc)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Get a new owner ID for the table */

    Status = AcpiUtAllocateOwnerId (&TableDesc->OwnerId);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Install the table into the global data structure */

    ListHead = &AcpiGbl_TableLists[TableType];

    /*
     * Two major types of tables:  1) Only one instance is allowed.  This
     * includes most ACPI tables such as the DSDT.  2) Multiple instances of
     * the table are allowed.  This includes SSDT and PSDTs.
     */
    if (ACPI_IS_SINGLE_TABLE (AcpiGbl_TableData[TableType].Flags))
    {
        /*
         * Only one table allowed, and a table has alread been installed
         * at this location, so return an error.
         */
        if (ListHead->Next)
        {
            ACPI_MEM_FREE (TableDesc);
            return_ACPI_STATUS (AE_ALREADY_EXISTS);
        }

        TableDesc->Next = ListHead->Next;
        ListHead->Next = TableDesc;

        if (TableDesc->Next)
        {
            TableDesc->Next->Prev = TableDesc;
        }

        ListHead->Count++;
    }
    else
    {
        /*
         * Link the new table in to the list of tables of this type.
         * Insert at the end of the list, order IS IMPORTANT.
         *
         * TableDesc->Prev & Next are already NULL from calloc()
         */
        ListHead->Count++;

        if (!ListHead->Next)
        {
            ListHead->Next = TableDesc;
        }
        else
        {
            TableDesc->Next = ListHead->Next;

            while (TableDesc->Next->Next)
            {
                TableDesc->Next = TableDesc->Next->Next;
            }

            TableDesc->Next->Next = TableDesc;
            TableDesc->Prev = TableDesc->Next;
            TableDesc->Next = NULL;
        }
    }

    /* Finish initialization of the table descriptor */

    TableDesc->Type                 = (UINT8) TableType;
    TableDesc->Pointer              = TableInfo->Pointer;
    TableDesc->Length               = TableInfo->Length;
    TableDesc->Allocation           = TableInfo->Allocation;
    TableDesc->AmlStart             = (UINT8 *) (TableDesc->Pointer + 1),
    TableDesc->AmlLength            = (UINT32) (TableDesc->Length -
                                        (UINT32) sizeof (ACPI_TABLE_HEADER));
    TableDesc->LoadedIntoNamespace  = FALSE;

    /*
     * Set the appropriate global pointer (if there is one) to point to the
     * newly installed table
     */
    if (AcpiGbl_TableData[TableType].GlobalPtr)
    {
        *(AcpiGbl_TableData[TableType].GlobalPtr) = TableInfo->Pointer;
    }

    /* Return Data */

    TableInfo->OwnerId          = TableDesc->OwnerId;
    TableInfo->InstalledDesc    = TableDesc;

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbDeleteAllTables
 *
 * PARAMETERS:  None.
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Delete all internal ACPI tables
 *
 ******************************************************************************/

void
AcpiTbDeleteAllTables (
    void)
{
    ACPI_TABLE_TYPE         Type;


    /*
     * Free memory allocated for ACPI tables
     * Memory can either be mapped or allocated
     */
    for (Type = 0; Type < NUM_ACPI_TABLE_TYPES; Type++)
    {
        AcpiTbDeleteTablesByType (Type);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbDeleteTablesByType
 *
 * PARAMETERS:  Type                - The table type to be deleted
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Delete an internal ACPI table
 *              Locks the ACPI table mutex
 *
 ******************************************************************************/

void
AcpiTbDeleteTablesByType (
    ACPI_TABLE_TYPE         Type)
{
    ACPI_TABLE_DESC         *TableDesc;
    UINT32                  Count;
    UINT32                  i;


    ACPI_FUNCTION_TRACE_U32 ("TbDeleteTablesByType", Type);


    if (Type > ACPI_TABLE_MAX)
    {
        return_VOID;
    }

    if (ACPI_FAILURE (AcpiUtAcquireMutex (ACPI_MTX_TABLES)))
    {
        return;
    }

    /* Clear the appropriate "typed" global table pointer */

    switch (Type)
    {
    case ACPI_TABLE_RSDP:
        AcpiGbl_RSDP = NULL;
        break;

    case ACPI_TABLE_DSDT:
        AcpiGbl_DSDT = NULL;
        break;

    case ACPI_TABLE_FADT:
        AcpiGbl_FADT = NULL;
        break;

    case ACPI_TABLE_FACS:
        AcpiGbl_FACS = NULL;
        break;

    case ACPI_TABLE_XSDT:
        AcpiGbl_XSDT = NULL;
        break;

    case ACPI_TABLE_SSDT:
    case ACPI_TABLE_PSDT:
    default:
        break;
    }

    /*
     * Free the table
     * 1) Get the head of the list
     */
    TableDesc = AcpiGbl_TableLists[Type].Next;
    Count     = AcpiGbl_TableLists[Type].Count;

    /*
     * 2) Walk the entire list, deleting both the allocated tables
     *    and the table descriptors
     */
    for (i = 0; i < Count; i++)
    {
        TableDesc = AcpiTbUninstallTable (TableDesc);
    }

    (void) AcpiUtReleaseMutex (ACPI_MTX_TABLES);
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbDeleteSingleTable
 *
 * PARAMETERS:  TableInfo           - A table info struct
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Low-level free for a single ACPI table.  Handles cases where
 *              the table was allocated a buffer or was mapped.
 *
 ******************************************************************************/

void
AcpiTbDeleteSingleTable (
    ACPI_TABLE_DESC         *TableDesc)
{

    /* Must have a valid table descriptor and pointer */

    if ((!TableDesc) ||
         (!TableDesc->Pointer))
    {
        return;
    }

    /* Valid table, determine type of memory allocation */

    switch (TableDesc->Allocation)
    {
    case ACPI_MEM_NOT_ALLOCATED:
        break;

    case ACPI_MEM_ALLOCATED:

        ACPI_MEM_FREE (TableDesc->Pointer);
        break;

    case ACPI_MEM_MAPPED:

        AcpiOsUnmapMemory (TableDesc->Pointer, TableDesc->Length);
        break;

    default:
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbUninstallTable
 *
 * PARAMETERS:  TableInfo           - A table info struct
 *
 * RETURN:      Pointer to the next table in the list (of same type)
 *
 * DESCRIPTION: Free the memory associated with an internal ACPI table that
 *              is either installed or has never been installed.
 *              Table mutex should be locked.
 *
 ******************************************************************************/

ACPI_TABLE_DESC *
AcpiTbUninstallTable (
    ACPI_TABLE_DESC         *TableDesc)
{
    ACPI_TABLE_DESC         *NextDesc;


    ACPI_FUNCTION_TRACE_PTR ("TbUninstallTable", TableDesc);


    if (!TableDesc)
    {
        return_PTR (NULL);
    }

    /* Unlink the descriptor from the doubly linked list */

    if (TableDesc->Prev)
    {
        TableDesc->Prev->Next = TableDesc->Next;
    }
    else
    {
        /* Is first on list, update list head */

        AcpiGbl_TableLists[TableDesc->Type].Next = TableDesc->Next;
    }

    if (TableDesc->Next)
    {
        TableDesc->Next->Prev = TableDesc->Prev;
    }

    /* Free the memory allocated for the table itself */

    AcpiTbDeleteSingleTable (TableDesc);

    /* Free the table descriptor */

    NextDesc = TableDesc->Next;
    ACPI_MEM_FREE (TableDesc);

    /* Return pointer to the next descriptor */

    return_PTR (NextDesc);
}


