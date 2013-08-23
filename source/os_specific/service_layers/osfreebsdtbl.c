/******************************************************************************
 *
 * Module Name: osfreebsdtbl - FreeBSD OSL for obtaining ACPI tables
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

#include "acpidump.h"

#include <kenv.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/sysctl.h>


#define _COMPONENT          ACPI_OS_SERVICES
        ACPI_MODULE_NAME    ("osfreebsdtbl")


/* Local prototypes */

static ACPI_STATUS
OslTableInitialize (
    void);

static ACPI_STATUS
OslMapTable (
    ACPI_SIZE               Address,
    char                    *Signature,
    ACPI_TABLE_HEADER       **Table);

static ACPI_STATUS
OslAddTablesToList (
    void);

static ACPI_STATUS
OslGetTableViaRoot (
    char                    *Signature,
    UINT32                  Instance,
    ACPI_TABLE_HEADER       **Table,
    ACPI_PHYSICAL_ADDRESS   *Address);


/* Hints for RSDP */

#define SYSTEM_KENV         "hint.acpi.0.rsdp"
#define SYSTEM_SYSCTL       "machdep.acpi_root"

/* Initialization flags */

UINT8                   Gbl_TableListInitialized = FALSE;
UINT8                   Gbl_MainTableObtained = FALSE;

/* Local copies of main ACPI tables */

ACPI_TABLE_RSDP         Gbl_Rsdp;
ACPI_TABLE_FADT         *Gbl_Fadt;
ACPI_TABLE_RSDT         *Gbl_Rsdt;
ACPI_TABLE_XSDT         *Gbl_Xsdt;

/* Fadt address */

ACPI_PHYSICAL_ADDRESS   Gbl_FadtAddress;

/* Revision of RSD PTR */

UINT8                   Gbl_Revision;

/* List of information about obtained ACPI tables */

typedef struct          table_info
{
    struct table_info       *Next;
    char                    Signature[4];
    UINT32                  Instance;
    ACPI_PHYSICAL_ADDRESS   Address;

} OSL_TABLE_INFO;

OSL_TABLE_INFO          *Gbl_TableListHead = NULL;


/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetTableByAddress
 *
 * PARAMETERS:  Address         - Physical address of the ACPI table
 *              Table           - Where a pointer to the table is returned
 *
 * RETURN:      Status; Table buffer is returned if AE_OK.
 *              AE_NOT_FOUND: A valid table was not found at the address
 *
 * DESCRIPTION: Get an ACPI table via a physical memory address.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsGetTableByAddress (
    ACPI_PHYSICAL_ADDRESS   Address,
    ACPI_TABLE_HEADER       **Table)
{
    ACPI_TABLE_HEADER       *MappedTable;
    ACPI_TABLE_HEADER       *LocalTable;
    ACPI_STATUS             Status;


    /* Validate the input physical address to avoid program crash */

    if (Address < ACPI_HI_RSDP_WINDOW_BASE)
    {
        fprintf (stderr, "Invalid table address: 0x%8.8X%8.8X\n",
            ACPI_FORMAT_UINT64 (Address));
        return (AE_BAD_ADDRESS);
    }

    /* Map the table and validate it */

    Status = OslMapTable (Address, NULL, &MappedTable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Copy table to local buffer and return it */

    LocalTable = calloc (1, MappedTable->Length);
    if (!LocalTable)
    {
        AcpiOsUnmapMemory (MappedTable, MappedTable->Length);
        return (AE_NO_MEMORY);
    }

    ACPI_MEMCPY (LocalTable, MappedTable, MappedTable->Length);
    AcpiOsUnmapMemory (MappedTable, MappedTable->Length);

    *Table = LocalTable;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetTableByName
 *
 * PARAMETERS:  Signature       - ACPI Signature for desired table. Must be
 *                                a null terminated 4-character string.
 *              Instance        - Multiple table support for SSDT/UEFI (0...n)
 *                                Must be 0 for other tables.
 *              Table           - Where a pointer to the table is returned
 *              Address         - Where the table physical address is returned
 *
 * RETURN:      Status; Table buffer and physical address returned if AE_OK.
 *              AE_LIMIT: Instance is beyond valid limit
 *              AE_NOT_FOUND: A table with the signature was not found
 *
 * NOTE:        Assumes the input signature is uppercase.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsGetTableByName (
    char                    *Signature,
    UINT32                  Instance,
    ACPI_TABLE_HEADER       **Table,
    ACPI_PHYSICAL_ADDRESS   *Address)
{
    ACPI_STATUS             Status;


    /* Instance is only valid for SSDT/UEFI tables */

    if (Instance &&
        !ACPI_COMPARE_NAME (Signature, ACPI_SIG_SSDT) &&
        !ACPI_COMPARE_NAME (Signature, ACPI_SIG_UEFI))
    {
        return (AE_LIMIT);
    }

    /* Initialize main tables */

    Status = OslTableInitialize ();
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /*
     * If one of the main ACPI tables was requested (RSDT/XSDT/FADT),
     * simply return it immediately.
     */
    if (ACPI_COMPARE_NAME (Signature, ACPI_SIG_XSDT))
    {
        if (!Gbl_Revision)
        {
            return (AE_NOT_FOUND);
        }

        *Address = Gbl_Rsdp.XsdtPhysicalAddress;
        *Table = (ACPI_TABLE_HEADER *) Gbl_Xsdt;
        return (AE_OK);
    }

    if (ACPI_COMPARE_NAME (Signature, ACPI_SIG_RSDT))
    {
        if (!Gbl_Rsdp.RsdtPhysicalAddress)
        {
            return (AE_NOT_FOUND);
        }

        *Address = Gbl_Rsdp.RsdtPhysicalAddress;
        *Table = (ACPI_TABLE_HEADER *) Gbl_Rsdt;
        return (AE_OK);
    }

    if (ACPI_COMPARE_NAME (Signature, ACPI_SIG_FADT))
    {
        *Address = Gbl_FadtAddress;
        *Table = (ACPI_TABLE_HEADER *) Gbl_Fadt;
        return (AE_OK);
    }

    /* Not a main ACPI table, attempt to extract it from the RSDT/XSDT */

    Status = OslGetTableViaRoot (Signature, Instance, Table, Address);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsGetTableByIndex
 *
 * PARAMETERS:  Index           - Which table to get
 *              Table           - Where a pointer to the table is returned
 *              Instance        - Where a pointer to the table instance no. is
 *                                returned
 *              Address         - Where the table physical address is returned
 *
 * RETURN:      Status; Table buffer and physical address returned if AE_OK.
 *              AE_LIMIT: Index is beyond valid limit
 *
 * DESCRIPTION: Get an ACPI table via an index value (0 through n). Returns
 *              AE_LIMIT when an invalid index is reached. Index is not
 *              necessarily an index into the RSDT/XSDT.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsGetTableByIndex (
    UINT32                  Index,
    ACPI_TABLE_HEADER       **Table,
    UINT32                  *Instance,
    ACPI_PHYSICAL_ADDRESS   *Address)
{
    OSL_TABLE_INFO          *Info;
    ACPI_STATUS             Status;
    UINT32                  i;


    /* Initialize main tables */

    Status = OslTableInitialize ();
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Add all tables to list */

    Status = OslAddTablesToList ();
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Validate Index */

    if (Index >= Gbl_TableListHead->Instance)
    {
        return (AE_LIMIT);
    }

    /* Point to the table list entry specified by the Index argument */

    Info = Gbl_TableListHead;
    for (i = 0; i <= Index; i++)
    {
        Info = Info->Next;
    }

    /* Now we can just get the table via the address or name */

    if (Info->Address)
    {
        Status = AcpiOsGetTableByAddress (Info->Address, Table);
        if (ACPI_SUCCESS (Status))
        {
            *Address = Info->Address;
        }
    }
    else
    {
        Status = AcpiOsGetTableByName (Info->Signature, Info->Instance,
            Table, Address);
    }

    if (ACPI_SUCCESS (Status))
    {
        *Instance = Info->Instance;
    }
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    OslTableInitialize
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Initialize ACPI table data. Get and store main ACPI tables to
 *              local variables. Main ACPI tables include RSDP, FADT, RSDT,
 *              and/or XSDT.
 *
 *****************************************************************************/

static ACPI_STATUS
OslTableInitialize (
    void)
{
    char                    Buffer[32];
    ACPI_TABLE_HEADER       *MappedTable;
    UINT8                   *TableAddress;
    UINT8                   *RsdpAddress;
    ACPI_PHYSICAL_ADDRESS   RsdpBase;
    ACPI_SIZE               RsdpSize;
    ACPI_STATUS             Status;
    u_long                  Address = 0;
    size_t                  Length = sizeof (Address);


    /* Get main ACPI tables from memory on first invocation of this function */

    if (Gbl_MainTableObtained)
    {
        return (AE_OK);
    }

    /* Attempt to use kenv or sysctl to find RSD PTR record. */

    if (Gbl_RsdpBase)
    {
        Address = Gbl_RsdpBase;
    }
    else if (kenv (KENV_GET, SYSTEM_KENV, Buffer, sizeof (Buffer)) > 0)
    {
        Address = ACPI_STRTOUL (Buffer, NULL, 0);
    }
    if (!Address)
    {
        if (sysctlbyname (SYSTEM_SYSCTL, &Address, &Length, NULL, 0) != 0)
        {
            Address = 0;
        }
    }
    if (Address)
    {
        RsdpBase = Address;
        RsdpSize = sizeof (Gbl_Rsdp);
    }
    else
    {
        RsdpBase = ACPI_HI_RSDP_WINDOW_BASE;
        RsdpSize = ACPI_HI_RSDP_WINDOW_SIZE;
    }

    /* Get RSDP from memory */

    RsdpAddress = AcpiOsMapMemory (RsdpBase, RsdpSize);
    if (!RsdpAddress)
    {
        return (AE_BAD_ADDRESS);
    }

    /* Search low memory for the RSDP */

    TableAddress = AcpiTbScanMemoryForRsdp (RsdpAddress, RsdpSize);
    if (!TableAddress)
    {
        AcpiOsUnmapMemory (RsdpAddress, RsdpSize);
        return (AE_ERROR);
    }

    ACPI_MEMCPY (&Gbl_Rsdp, TableAddress, sizeof (Gbl_Rsdp));
    AcpiOsUnmapMemory (RsdpAddress, RsdpSize);

    /* Get XSDT from memory */

    if (Gbl_Rsdp.Revision)
    {
        Status = OslMapTable (Gbl_Rsdp.XsdtPhysicalAddress,
            ACPI_SIG_XSDT, &MappedTable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        Gbl_Revision = 2;
        Gbl_Xsdt = calloc (1, MappedTable->Length);
        if (!Gbl_Xsdt)
        {
            fprintf (stderr,
                "XSDT: Could not allocate buffer for table of length %X\n",
                MappedTable->Length);
            AcpiOsUnmapMemory (MappedTable, MappedTable->Length);
            return (AE_NO_MEMORY);
        }

        ACPI_MEMCPY (Gbl_Xsdt, MappedTable, MappedTable->Length);
        AcpiOsUnmapMemory (MappedTable, MappedTable->Length);
    }

    /* Get RSDT from memory */

    if (Gbl_Rsdp.RsdtPhysicalAddress)
    {
        Status = OslMapTable (Gbl_Rsdp.RsdtPhysicalAddress,
            ACPI_SIG_RSDT, &MappedTable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        Gbl_Rsdt = calloc (1, MappedTable->Length);
        if (!Gbl_Rsdt)
        {
            fprintf (stderr,
                "RSDT: Could not allocate buffer for table of length %X\n",
                MappedTable->Length);
            AcpiOsUnmapMemory (MappedTable, MappedTable->Length);
            return (AE_NO_MEMORY);
        }

        ACPI_MEMCPY (Gbl_Rsdt, MappedTable, MappedTable->Length);
        AcpiOsUnmapMemory (MappedTable, MappedTable->Length);
    }

    /* Get FADT from memory */

    if (Gbl_Revision)
    {
        Gbl_FadtAddress = Gbl_Xsdt->TableOffsetEntry[0];
    }
    else
    {
        Gbl_FadtAddress = Gbl_Rsdt->TableOffsetEntry[0];
    }

    if (!Gbl_FadtAddress)
    {
        fprintf(stderr, "FADT: Table could not be found\n");
        return (AE_ERROR);
    }

    Status = OslMapTable (Gbl_FadtAddress, ACPI_SIG_FADT, &MappedTable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    Gbl_Fadt = calloc (1, MappedTable->Length);
    if (!Gbl_Fadt)
    {
        fprintf (stderr,
            "FADT: Could not allocate buffer for table of length %X\n",
            MappedTable->Length);
        AcpiOsUnmapMemory (MappedTable, MappedTable->Length);
        return (AE_NO_MEMORY);
    }

    ACPI_MEMCPY (Gbl_Fadt, MappedTable, MappedTable->Length);
    AcpiOsUnmapMemory (MappedTable, MappedTable->Length);
    Gbl_MainTableObtained = TRUE;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    OslGetTableViaRoot
 *
 * PARAMETERS:  Signature       - ACPI Signature for common table. Must be
 *                                a null terminated 4-character string.
 *              Instance        - Multiple table support for SSDT/UEFI (0...n)
 *                                Must be 0 for other tables.
 *              Table           - Where a pointer to the table is returned
 *              Address         - Where the table physical address is returned
 *
 * RETURN:      Status; Table buffer and physical address returned if AE_OK.
 *              AE_LIMIT: Instance is beyond valid limit
 *              AE_NOT_FOUND: A table with the signature was not found
 *
 * DESCRIPTION: Get an ACPI table via the root table (RSDT/XSDT)
 *
 * NOTE:        Assumes the input signature is uppercase.
 *
 *****************************************************************************/

static ACPI_STATUS
OslGetTableViaRoot (
    char                    *Signature,
    UINT32                  Instance,
    ACPI_TABLE_HEADER       **Table,
    ACPI_PHYSICAL_ADDRESS   *Address)
{
    ACPI_TABLE_HEADER       *LocalTable = NULL;
    ACPI_TABLE_HEADER       *MappedTable = NULL;
    UINT8                   NumberOfTables;
    UINT32                  CurrentInstance = 0;
    ACPI_PHYSICAL_ADDRESS   TableAddress = 0;
    ACPI_STATUS             Status;
    UINT32                  i;


    /* DSDT and FACS address must be extracted from the FADT */

    if (ACPI_COMPARE_NAME (Signature, ACPI_SIG_DSDT) ||
        ACPI_COMPARE_NAME (Signature, ACPI_SIG_FACS))
    {
        /*
         * Get the appropriate address, either 32-bit or 64-bit. Be very
         * careful about the FADT length and validate table addresses.
         * Note: The 64-bit addresses have priority.
         */
        if (ACPI_COMPARE_NAME (Signature, ACPI_SIG_DSDT))
        {
            if ((Gbl_Fadt->Header.Length >= MIN_FADT_FOR_XDSDT) &&
                Gbl_Fadt->XDsdt)
            {
                TableAddress = (ACPI_PHYSICAL_ADDRESS) Gbl_Fadt->XDsdt;
            }
            else if ((Gbl_Fadt->Header.Length >= MIN_FADT_FOR_DSDT) &&
                Gbl_Fadt->Dsdt)
            {
                TableAddress = (ACPI_PHYSICAL_ADDRESS) Gbl_Fadt->Dsdt;
            }
        }
        else /* FACS */
        {
            if ((Gbl_Fadt->Header.Length >= MIN_FADT_FOR_XFACS) &&
                Gbl_Fadt->XFacs)
            {
                TableAddress = (ACPI_PHYSICAL_ADDRESS) Gbl_Fadt->XFacs;
            }
            else if ((Gbl_Fadt->Header.Length >= MIN_FADT_FOR_FACS) &&
                Gbl_Fadt->Facs)
            {
                TableAddress = (ACPI_PHYSICAL_ADDRESS) Gbl_Fadt->Facs;
            }
        }
    }
    else /* Case for a normal ACPI table */
    {
        if (Gbl_Revision)
        {
            NumberOfTables =
                (Gbl_Xsdt->Header.Length - sizeof (Gbl_Xsdt->Header))
                / sizeof (Gbl_Xsdt->TableOffsetEntry[0]);
        }
        else /* Use RSDT if XSDT is not available */
        {
            NumberOfTables =
                (Gbl_Rsdt->Header.Length - sizeof (Gbl_Rsdt->Header))
                / sizeof (Gbl_Rsdt->TableOffsetEntry[0]);
        }

        /* Search RSDT/XSDT for the requested table */

        for (i = 0; i < NumberOfTables; i++)
        {
            if (Gbl_Revision)
            {
                TableAddress = Gbl_Xsdt->TableOffsetEntry[i];
            }
            else
            {
                TableAddress = Gbl_Rsdt->TableOffsetEntry[i];
            }

            MappedTable = AcpiOsMapMemory (TableAddress, sizeof (*MappedTable));
            if (!MappedTable)
            {
                return (AE_BAD_ADDRESS);
            }

            /* Does this table match the requested signature? */

            if (ACPI_COMPARE_NAME (MappedTable->Signature, Signature))
            {

                /* Match table instance (for SSDT/UEFI tables) */

                if (CurrentInstance == Instance)
                {
                    AcpiOsUnmapMemory (MappedTable, sizeof (*MappedTable));
                    break;
                }

                CurrentInstance++;
            }

            AcpiOsUnmapMemory (MappedTable, MappedTable->Length);
            TableAddress = 0;
        }
    }

    if (!TableAddress)
    {
        if (CurrentInstance)
        {
            return (AE_LIMIT);
        }
        return (AE_NOT_FOUND);
    }

    /* Now we can get the requested table */

    Status = OslMapTable (TableAddress, Signature, &MappedTable);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /* Copy table to local buffer and return it */

    LocalTable = calloc (1, MappedTable->Length);
    if (!LocalTable)
    {
        AcpiOsUnmapMemory (MappedTable, MappedTable->Length);
        return (AE_NO_MEMORY);
    }

    ACPI_MEMCPY (LocalTable, MappedTable, MappedTable->Length);
    AcpiOsUnmapMemory (MappedTable, MappedTable->Length);
    *Table = LocalTable;
    *Address = TableAddress;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    OslAddTablesToList
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status; Table list is initialized if AE_OK.
 *
 * DESCRIPTION: Add ACPI tables to the table list.
 *
 *****************************************************************************/

static ACPI_STATUS
OslAddTablesToList(
    void)
{
    ACPI_PHYSICAL_ADDRESS   TableAddress;
    OSL_TABLE_INFO          *Info = NULL;
    OSL_TABLE_INFO          *NewInfo;
    ACPI_TABLE_HEADER       *Table;
    UINT8                   Instance;
    UINT8                   NumberOfTables;
    int                     i;


    /* Initialize the table list on first invocation */

    if (Gbl_TableListInitialized)
    {
        return (AE_OK);
    }

    /* Add mandatory tables to global table list first */

    for (i = 0; i < 4; i++)
    {
        NewInfo = calloc (1, sizeof (*NewInfo));
        if (!NewInfo)
        {
            return (AE_NO_MEMORY);
        }

        switch (i) {
        case 0:

            Gbl_TableListHead = Info = NewInfo;
            continue;

        case 1:

            ACPI_MOVE_NAME (NewInfo->Signature,
                Gbl_Revision ? ACPI_SIG_XSDT : ACPI_SIG_RSDT);
            break;

        case 2:

            ACPI_MOVE_NAME (NewInfo->Signature, ACPI_SIG_FACS);
            break;

        default:

            ACPI_MOVE_NAME (NewInfo->Signature, ACPI_SIG_DSDT);

        }

        Info->Next = NewInfo;
        Info = NewInfo;
        Gbl_TableListHead->Instance++;
    }

    /* Add normal tables from RSDT/XSDT to global list */

    if (Gbl_Revision)
    {
        NumberOfTables =
            (Gbl_Xsdt->Header.Length - sizeof (Gbl_Xsdt->Header))
            / sizeof (Gbl_Xsdt->TableOffsetEntry[0]);
    }
    else
    {
        NumberOfTables =
            (Gbl_Rsdt->Header.Length - sizeof (Gbl_Rsdt->Header))
            / sizeof (Gbl_Rsdt->TableOffsetEntry[0]);
    }

    for (i = 0; i < NumberOfTables; i++)
    {
        if (Gbl_Revision)
        {
            TableAddress = Gbl_Xsdt->TableOffsetEntry[i];
        }
        else
        {
            TableAddress = Gbl_Rsdt->TableOffsetEntry[i];
        }

        Table = AcpiOsMapMemory (TableAddress, sizeof (*Table));
        if (!Table)
        {
            return (AE_BAD_ADDRESS);
        }

        Instance = 0;
        NewInfo = Gbl_TableListHead;
        while (NewInfo->Next != NULL)
        {
            NewInfo = NewInfo->Next;
            if (ACPI_COMPARE_NAME (Table->Signature, NewInfo->Signature))
            {
                Instance++;
            }
        }

        NewInfo = calloc (1, sizeof (*NewInfo));
        if (!NewInfo)
        {
            AcpiOsUnmapMemory (Table, sizeof (*Table));
            return (AE_NO_MEMORY);
        }

        ACPI_MOVE_NAME (NewInfo->Signature, Table->Signature);

        AcpiOsUnmapMemory (Table, sizeof (*Table));

        NewInfo->Instance = Instance;
        NewInfo->Address = TableAddress;
        Info->Next = NewInfo;
        Info = NewInfo;
        Gbl_TableListHead->Instance++;
    }

    Gbl_TableListInitialized = TRUE;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    OslMapTable
 *
 * PARAMETERS:  Address             - Address of the table in memory
 *              Signature           - Optional ACPI Signature for desired table.
 *                                    Null terminated 4-character string.
 *              Table               - Where a pointer to the mapped table is
 *                                    returned
 *
 * RETURN:      Status; Mapped table is returned if AE_OK.
 *
 * DESCRIPTION: Map entire ACPI table into caller's address space. Also
 *              validates the table and checksum.
 *
 *****************************************************************************/

static ACPI_STATUS
OslMapTable (
    ACPI_SIZE               Address,
    char                    *Signature,
    ACPI_TABLE_HEADER       **Table)
{
    ACPI_TABLE_HEADER       *MappedTable;
    UINT32                  Length;


    /* Map the header so we can get the table length */

    MappedTable = AcpiOsMapMemory (Address, sizeof (*MappedTable));
    if (!MappedTable)
    {
        return (AE_BAD_ADDRESS);
    }

    /* Check if table is valid */

    if (!ApIsValidHeader (MappedTable))
    {
        AcpiOsUnmapMemory (MappedTable, sizeof (*MappedTable));
        return (AE_BAD_HEADER);
    }

    /* If specified, signature must match */

    if (Signature &&
        !ACPI_COMPARE_NAME (Signature, MappedTable->Signature))
    {
        AcpiOsUnmapMemory (MappedTable, sizeof (*MappedTable));
        return (AE_NOT_EXIST);
    }

    /* Map the entire table */

    Length = MappedTable->Length;
    AcpiOsUnmapMemory (MappedTable, sizeof (*MappedTable));

    MappedTable = AcpiOsMapMemory (Address, Length);
    if (!MappedTable)
    {
        return (AE_BAD_ADDRESS);
    }

    (void) ApIsValidChecksum (MappedTable);

    *Table = MappedTable;

    return (AE_OK);
}
