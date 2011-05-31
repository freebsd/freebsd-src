/******************************************************************************
 *
 * Module Name: tbinstal - ACPI table installation and removal
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
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


#define __TBINSTAL_C__

#include "acpi.h"
#include "accommon.h"
#include "acnamesp.h"
#include "actables.h"


#define _COMPONENT          ACPI_TABLES
        ACPI_MODULE_NAME    ("tbinstal")


/******************************************************************************
 *
 * FUNCTION:    AcpiTbVerifyTable
 *
 * PARAMETERS:  TableDesc           - table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: this function is called to verify and map table
 *
 *****************************************************************************/

ACPI_STATUS
AcpiTbVerifyTable (
    ACPI_TABLE_DESC         *TableDesc)
{
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE (TbVerifyTable);


    /* Map the table if necessary */

    if (!TableDesc->Pointer)
    {
        if ((TableDesc->Flags & ACPI_TABLE_ORIGIN_MASK) ==
            ACPI_TABLE_ORIGIN_MAPPED)
        {
            TableDesc->Pointer = AcpiOsMapMemory (
                TableDesc->Address, TableDesc->Length);
        }

        if (!TableDesc->Pointer)
        {
            return_ACPI_STATUS (AE_NO_MEMORY);
        }
    }

    /* FACS is the odd table, has no standard ACPI header and no checksum */

    if (!ACPI_COMPARE_NAME (&TableDesc->Signature, ACPI_SIG_FACS))
    {
        /* Always calculate checksum, ignore bad checksum if requested */

        Status = AcpiTbVerifyChecksum (TableDesc->Pointer, TableDesc->Length);
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbAddTable
 *
 * PARAMETERS:  TableDesc           - Table descriptor
 *              TableIndex          - Where the table index is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to add an ACPI table. It is used to
 *              dynamically load tables via the Load and LoadTable AML
 *              operators.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbAddTable (
    ACPI_TABLE_DESC         *TableDesc,
    UINT32                  *TableIndex)
{
    UINT32                  i;
    ACPI_STATUS             Status = AE_OK;
    ACPI_TABLE_HEADER       *OverrideTable = NULL;


    ACPI_FUNCTION_TRACE (TbAddTable);


    if (!TableDesc->Pointer)
    {
        Status = AcpiTbVerifyTable (TableDesc);
        if (ACPI_FAILURE (Status) || !TableDesc->Pointer)
        {
            return_ACPI_STATUS (Status);
        }
    }

    /*
     * Validate the incoming table signature.
     *
     * 1) Originally, we checked the table signature for "SSDT" or "PSDT".
     * 2) We added support for OEMx tables, signature "OEM".
     * 3) Valid tables were encountered with a null signature, so we just
     *    gave up on validating the signature, (05/2008).
     * 4) We encountered non-AML tables such as the MADT, which caused
     *    interpreter errors and kernel faults. So now, we once again allow
     *    only "SSDT", "OEMx", and now, also a null signature. (05/2011).
     */
    if ((TableDesc->Pointer->Signature[0] != 0x00) &&
       (!ACPI_COMPARE_NAME (TableDesc->Pointer->Signature, ACPI_SIG_SSDT)) &&
       (ACPI_STRNCMP (TableDesc->Pointer->Signature, "OEM", 3)))
    {
        ACPI_ERROR ((AE_INFO,
            "Table has invalid signature [%4.4s] (0x%8.8X), must be SSDT or OEMx",
            AcpiUtValidAcpiName (*(UINT32 *) TableDesc->Pointer->Signature) ?
                TableDesc->Pointer->Signature : "????",
            *(UINT32 *) TableDesc->Pointer->Signature));

        return_ACPI_STATUS (AE_BAD_SIGNATURE);
    }

    (void) AcpiUtAcquireMutex (ACPI_MTX_TABLES);

    /* Check if table is already registered */

    for (i = 0; i < AcpiGbl_RootTableList.CurrentTableCount; ++i)
    {
        if (!AcpiGbl_RootTableList.Tables[i].Pointer)
        {
            Status = AcpiTbVerifyTable (&AcpiGbl_RootTableList.Tables[i]);
            if (ACPI_FAILURE (Status) ||
                !AcpiGbl_RootTableList.Tables[i].Pointer)
            {
                continue;
            }
        }

        /*
         * Check for a table match on the entire table length,
         * not just the header.
         */
        if (TableDesc->Length != AcpiGbl_RootTableList.Tables[i].Length)
        {
            continue;
        }

        if (ACPI_MEMCMP (TableDesc->Pointer,
                AcpiGbl_RootTableList.Tables[i].Pointer,
                AcpiGbl_RootTableList.Tables[i].Length))
        {
            continue;
        }

        /*
         * Note: the current mechanism does not unregister a table if it is
         * dynamically unloaded. The related namespace entries are deleted,
         * but the table remains in the root table list.
         *
         * The assumption here is that the number of different tables that
         * will be loaded is actually small, and there is minimal overhead
         * in just keeping the table in case it is needed again.
         *
         * If this assumption changes in the future (perhaps on large
         * machines with many table load/unload operations), tables will
         * need to be unregistered when they are unloaded, and slots in the
         * root table list should be reused when empty.
         */

        /*
         * Table is already registered.
         * We can delete the table that was passed as a parameter.
         */
        AcpiTbDeleteTable (TableDesc);
        *TableIndex = i;

        if (AcpiGbl_RootTableList.Tables[i].Flags & ACPI_TABLE_IS_LOADED)
        {
            /* Table is still loaded, this is an error */

            Status = AE_ALREADY_EXISTS;
            goto Release;
        }
        else
        {
            /* Table was unloaded, allow it to be reloaded */

            TableDesc->Pointer = AcpiGbl_RootTableList.Tables[i].Pointer;
            TableDesc->Address = AcpiGbl_RootTableList.Tables[i].Address;
            Status = AE_OK;
            goto PrintHeader;
        }
    }

    /*
     * ACPI Table Override:
     * Allow the host to override dynamically loaded tables.
     */
    Status = AcpiOsTableOverride (TableDesc->Pointer, &OverrideTable);
    if (ACPI_SUCCESS (Status) && OverrideTable)
    {
        ACPI_INFO ((AE_INFO,
            "%4.4s @ 0x%p Table override, replaced with:",
            TableDesc->Pointer->Signature,
            ACPI_CAST_PTR (void, TableDesc->Address)));

        /* We can delete the table that was passed as a parameter */

        AcpiTbDeleteTable (TableDesc);

        /* Setup descriptor for the new table */

        TableDesc->Address = ACPI_PTR_TO_PHYSADDR (OverrideTable);
        TableDesc->Pointer = OverrideTable;
        TableDesc->Length = OverrideTable->Length;
        TableDesc->Flags = ACPI_TABLE_ORIGIN_OVERRIDE;
    }

    /* Add the table to the global root table list */

    Status = AcpiTbStoreTable (TableDesc->Address, TableDesc->Pointer,
                TableDesc->Length, TableDesc->Flags, TableIndex);
    if (ACPI_FAILURE (Status))
    {
        goto Release;
    }

PrintHeader:
    AcpiTbPrintTableHeader (TableDesc->Address, TableDesc->Pointer);

Release:
    (void) AcpiUtReleaseMutex (ACPI_MTX_TABLES);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbResizeRootTableList
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Expand the size of global table array
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbResizeRootTableList (
    void)
{
    ACPI_TABLE_DESC         *Tables;


    ACPI_FUNCTION_TRACE (TbResizeRootTableList);


    /* AllowResize flag is a parameter to AcpiInitializeTables */

    if (!(AcpiGbl_RootTableList.Flags & ACPI_ROOT_ALLOW_RESIZE))
    {
        ACPI_ERROR ((AE_INFO, "Resize of Root Table Array is not allowed"));
        return_ACPI_STATUS (AE_SUPPORT);
    }

    /* Increase the Table Array size */

    Tables = ACPI_ALLOCATE_ZEROED (
        ((ACPI_SIZE) AcpiGbl_RootTableList.MaxTableCount +
            ACPI_ROOT_TABLE_SIZE_INCREMENT) *
        sizeof (ACPI_TABLE_DESC));
    if (!Tables)
    {
        ACPI_ERROR ((AE_INFO, "Could not allocate new root table array"));
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Copy and free the previous table array */

    if (AcpiGbl_RootTableList.Tables)
    {
        ACPI_MEMCPY (Tables, AcpiGbl_RootTableList.Tables,
            (ACPI_SIZE) AcpiGbl_RootTableList.MaxTableCount * sizeof (ACPI_TABLE_DESC));

        if (AcpiGbl_RootTableList.Flags & ACPI_ROOT_ORIGIN_ALLOCATED)
        {
            ACPI_FREE (AcpiGbl_RootTableList.Tables);
        }
    }

    AcpiGbl_RootTableList.Tables = Tables;
    AcpiGbl_RootTableList.MaxTableCount += ACPI_ROOT_TABLE_SIZE_INCREMENT;
    AcpiGbl_RootTableList.Flags |= (UINT8) ACPI_ROOT_ORIGIN_ALLOCATED;

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbStoreTable
 *
 * PARAMETERS:  Address             - Table address
 *              Table               - Table header
 *              Length              - Table length
 *              Flags               - flags
 *
 * RETURN:      Status and table index.
 *
 * DESCRIPTION: Add an ACPI table to the global table list
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbStoreTable (
    ACPI_PHYSICAL_ADDRESS   Address,
    ACPI_TABLE_HEADER       *Table,
    UINT32                  Length,
    UINT8                   Flags,
    UINT32                  *TableIndex)
{
    ACPI_STATUS             Status;
    ACPI_TABLE_DESC         *NewTable;


    /* Ensure that there is room for the table in the Root Table List */

    if (AcpiGbl_RootTableList.CurrentTableCount >=
        AcpiGbl_RootTableList.MaxTableCount)
    {
        Status = AcpiTbResizeRootTableList();
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
    }

    NewTable = &AcpiGbl_RootTableList.Tables[AcpiGbl_RootTableList.CurrentTableCount];

    /* Initialize added table */

    NewTable->Address = Address;
    NewTable->Pointer = Table;
    NewTable->Length = Length;
    NewTable->OwnerId = 0;
    NewTable->Flags = Flags;

    ACPI_MOVE_32_TO_32 (&NewTable->Signature, Table->Signature);

    *TableIndex = AcpiGbl_RootTableList.CurrentTableCount;
    AcpiGbl_RootTableList.CurrentTableCount++;
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbDeleteTable
 *
 * PARAMETERS:  TableIndex          - Table index
 *
 * RETURN:      None
 *
 * DESCRIPTION: Delete one internal ACPI table
 *
 ******************************************************************************/

void
AcpiTbDeleteTable (
    ACPI_TABLE_DESC         *TableDesc)
{

    /* Table must be mapped or allocated */

    if (!TableDesc->Pointer)
    {
        return;
    }

    switch (TableDesc->Flags & ACPI_TABLE_ORIGIN_MASK)
    {
    case ACPI_TABLE_ORIGIN_MAPPED:
        AcpiOsUnmapMemory (TableDesc->Pointer, TableDesc->Length);
        break;

    case ACPI_TABLE_ORIGIN_ALLOCATED:
        ACPI_FREE (TableDesc->Pointer);
        break;

    default:
        break;
    }

    TableDesc->Pointer = NULL;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbTerminate
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Delete all internal ACPI tables
 *
 ******************************************************************************/

void
AcpiTbTerminate (
    void)
{
    UINT32                  i;


    ACPI_FUNCTION_TRACE (TbTerminate);


    (void) AcpiUtAcquireMutex (ACPI_MTX_TABLES);

    /* Delete the individual tables */

    for (i = 0; i < AcpiGbl_RootTableList.CurrentTableCount; i++)
    {
        AcpiTbDeleteTable (&AcpiGbl_RootTableList.Tables[i]);
    }

    /*
     * Delete the root table array if allocated locally. Array cannot be
     * mapped, so we don't need to check for that flag.
     */
    if (AcpiGbl_RootTableList.Flags & ACPI_ROOT_ORIGIN_ALLOCATED)
    {
        ACPI_FREE (AcpiGbl_RootTableList.Tables);
    }

    AcpiGbl_RootTableList.Tables = NULL;
    AcpiGbl_RootTableList.Flags = 0;
    AcpiGbl_RootTableList.CurrentTableCount = 0;

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "ACPI Tables freed\n"));
    (void) AcpiUtReleaseMutex (ACPI_MTX_TABLES);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbDeleteNamespaceByOwner
 *
 * PARAMETERS:  TableIndex          - Table index
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Delete all namespace objects created when this table was loaded.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbDeleteNamespaceByOwner (
    UINT32                  TableIndex)
{
    ACPI_OWNER_ID           OwnerId;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (TbDeleteNamespaceByOwner);


    Status = AcpiUtAcquireMutex (ACPI_MTX_TABLES);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    if (TableIndex >= AcpiGbl_RootTableList.CurrentTableCount)
    {
        /* The table index does not exist */

        (void) AcpiUtReleaseMutex (ACPI_MTX_TABLES);
        return_ACPI_STATUS (AE_NOT_EXIST);
    }

    /* Get the owner ID for this table, used to delete namespace nodes */

    OwnerId = AcpiGbl_RootTableList.Tables[TableIndex].OwnerId;
    (void) AcpiUtReleaseMutex (ACPI_MTX_TABLES);

    /*
     * Need to acquire the namespace writer lock to prevent interference
     * with any concurrent namespace walks. The interpreter must be
     * released during the deletion since the acquisition of the deletion
     * lock may block, and also since the execution of a namespace walk
     * must be allowed to use the interpreter.
     */
    (void) AcpiUtReleaseMutex (ACPI_MTX_INTERPRETER);
    Status = AcpiUtAcquireWriteLock (&AcpiGbl_NamespaceRwLock);

    AcpiNsDeleteNamespaceByOwner (OwnerId);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    AcpiUtReleaseWriteLock (&AcpiGbl_NamespaceRwLock);

    Status = AcpiUtAcquireMutex (ACPI_MTX_INTERPRETER);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbAllocateOwnerId
 *
 * PARAMETERS:  TableIndex          - Table index
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Allocates OwnerId in TableDesc
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbAllocateOwnerId (
    UINT32                  TableIndex)
{
    ACPI_STATUS             Status = AE_BAD_PARAMETER;


    ACPI_FUNCTION_TRACE (TbAllocateOwnerId);


    (void) AcpiUtAcquireMutex (ACPI_MTX_TABLES);
    if (TableIndex < AcpiGbl_RootTableList.CurrentTableCount)
    {
        Status = AcpiUtAllocateOwnerId
                    (&(AcpiGbl_RootTableList.Tables[TableIndex].OwnerId));
    }

    (void) AcpiUtReleaseMutex (ACPI_MTX_TABLES);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbReleaseOwnerId
 *
 * PARAMETERS:  TableIndex          - Table index
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Releases OwnerId in TableDesc
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbReleaseOwnerId (
    UINT32                  TableIndex)
{
    ACPI_STATUS             Status = AE_BAD_PARAMETER;


    ACPI_FUNCTION_TRACE (TbReleaseOwnerId);


    (void) AcpiUtAcquireMutex (ACPI_MTX_TABLES);
    if (TableIndex < AcpiGbl_RootTableList.CurrentTableCount)
    {
        AcpiUtReleaseOwnerId (
            &(AcpiGbl_RootTableList.Tables[TableIndex].OwnerId));
        Status = AE_OK;
    }

    (void) AcpiUtReleaseMutex (ACPI_MTX_TABLES);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbGetOwnerId
 *
 * PARAMETERS:  TableIndex          - Table index
 *              OwnerId             - Where the table OwnerId is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: returns OwnerId for the ACPI table
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbGetOwnerId (
    UINT32                  TableIndex,
    ACPI_OWNER_ID           *OwnerId)
{
    ACPI_STATUS             Status = AE_BAD_PARAMETER;


    ACPI_FUNCTION_TRACE (TbGetOwnerId);


    (void) AcpiUtAcquireMutex (ACPI_MTX_TABLES);
    if (TableIndex < AcpiGbl_RootTableList.CurrentTableCount)
    {
        *OwnerId = AcpiGbl_RootTableList.Tables[TableIndex].OwnerId;
        Status = AE_OK;
    }

    (void) AcpiUtReleaseMutex (ACPI_MTX_TABLES);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbIsTableLoaded
 *
 * PARAMETERS:  TableIndex          - Table index
 *
 * RETURN:      Table Loaded Flag
 *
 ******************************************************************************/

BOOLEAN
AcpiTbIsTableLoaded (
    UINT32                  TableIndex)
{
    BOOLEAN                 IsLoaded = FALSE;


    (void) AcpiUtAcquireMutex (ACPI_MTX_TABLES);
    if (TableIndex < AcpiGbl_RootTableList.CurrentTableCount)
    {
        IsLoaded = (BOOLEAN)
            (AcpiGbl_RootTableList.Tables[TableIndex].Flags &
            ACPI_TABLE_IS_LOADED);
    }

    (void) AcpiUtReleaseMutex (ACPI_MTX_TABLES);
    return (IsLoaded);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbSetTableLoadedFlag
 *
 * PARAMETERS:  TableIndex          - Table index
 *              IsLoaded            - TRUE if table is loaded, FALSE otherwise
 *
 * RETURN:      None
 *
 * DESCRIPTION: Sets the table loaded flag to either TRUE or FALSE.
 *
 ******************************************************************************/

void
AcpiTbSetTableLoadedFlag (
    UINT32                  TableIndex,
    BOOLEAN                 IsLoaded)
{

    (void) AcpiUtAcquireMutex (ACPI_MTX_TABLES);
    if (TableIndex < AcpiGbl_RootTableList.CurrentTableCount)
    {
        if (IsLoaded)
        {
            AcpiGbl_RootTableList.Tables[TableIndex].Flags |=
                ACPI_TABLE_IS_LOADED;
        }
        else
        {
            AcpiGbl_RootTableList.Tables[TableIndex].Flags &=
                ~ACPI_TABLE_IS_LOADED;
        }
    }

    (void) AcpiUtReleaseMutex (ACPI_MTX_TABLES);
}

