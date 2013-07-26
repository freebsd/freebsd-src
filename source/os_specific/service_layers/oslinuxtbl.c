/******************************************************************************
 *
 * Module Name: oslinuxtbl - Linux OSL for obtaining ACPI tables
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

#include <dirent.h>
#include <sys/mman.h>


#define _COMPONENT          ACPI_OS_SERVICES
        ACPI_MODULE_NAME    ("oslinuxtbl")


#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef PATH_MAX
#define PATH_MAX 256
#endif


/* Local prototypes */

static ACPI_STATUS
OslTableInitialize (
    void);

static ACPI_STATUS
OslReadTableFromFile (
    FILE                    *TableFile,
    ACPI_SIZE               FileOffset,
    char                    *Signature,
    ACPI_TABLE_HEADER       **Table);

static ACPI_STATUS
OslMapTable (
    ACPI_SIZE               Address,
    char                    *Signature,
    ACPI_TABLE_HEADER       **Table);

static ACPI_STATUS
OslGetOverrideTable (
    char                    *Signature,
    UINT32                  Instance,
    ACPI_TABLE_HEADER       **Table,
    ACPI_PHYSICAL_ADDRESS   *Address);

static ACPI_STATUS
OslGetDynamicSsdt (
    UINT32                  Instance,
    ACPI_TABLE_HEADER       **Table,
    ACPI_PHYSICAL_ADDRESS   *Address);

static ACPI_STATUS
OslAddTablesToList (
    char                    *Directory);

static ACPI_STATUS
OslGetTableViaRoot (
    char                    *Signature,
    UINT32                  Instance,
    ACPI_TABLE_HEADER       **Table,
    ACPI_PHYSICAL_ADDRESS   *Address);


/* File locations */

#define DYNAMIC_SSDT_DIR    "/sys/firmware/acpi/tables/dynamic"
#define OVERRIDE_TABLE_DIR  "/sys/firmware/acpi/tables"
#define SYSTEM_MEMORY       "/dev/mem"

/* Should we get dynamically loaded SSDTs from DYNAMIC_SSDT_DIR? */

UINT8                   Gbl_DumpDynamicSsdts = TRUE;

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
    UINT32                  Instance;
    char                    Signature[4];

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


    /* Instance is only valid for SSDTs */

    if (Instance &&
        !ACPI_COMPARE_NAME (Signature, ACPI_SIG_SSDT) &&
        !ACPI_COMPARE_NAME (Signature, ACPI_SIG_UEFI))
    {
        return (AE_LIMIT);
    }

    /* Get main ACPI tables from memory on first invocation of this function */

    if (!Gbl_MainTableObtained)
    {
        Status = OslTableInitialize ();
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        Gbl_MainTableObtained = TRUE;
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
        /* Attempt to get the table from the override directory */

        Status = OslGetOverrideTable (Signature, Instance, Table, Address);
        if ((Status == AE_LIMIT) && Gbl_DumpDynamicSsdts)
        {
            /* Attempt to get a dynamic SSDT */

            Status = OslGetDynamicSsdt (Instance, Table, Address);
        }

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
    ACPI_PHYSICAL_ADDRESS   *Address)
{
    OSL_TABLE_INFO          *Info;
    ACPI_STATUS             Status;
    UINT32                  i;


    /* Initialize the table list on first invocation */

    if (!Gbl_TableListInitialized)
    {
        Gbl_TableListHead = calloc (1, sizeof (OSL_TABLE_INFO));

        /* List head records the length of the list */

        Gbl_TableListHead->Instance = 0;

        /* Add all tables found in the override directory */

        Status = OslAddTablesToList (OVERRIDE_TABLE_DIR);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        /* Add all dynamically loaded SSDTs in the dynamic directory */

        OslAddTablesToList (DYNAMIC_SSDT_DIR);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        Gbl_TableListInitialized = TRUE;
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

    /* Now we can just get the table via the signature */

    Status = AcpiOsGetTableByName (Info->Signature, Info->Instance,
        Table, Address);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsMapMemory
 *
 * PARAMETERS:  Where               - Physical address of memory to be mapped
 *              Length              - How much memory to map
 *
 * RETURN:      Pointer to mapped memory. Null on error.
 *
 * DESCRIPTION: Map physical memory into local address space.
 *
 *****************************************************************************/

void *
AcpiOsMapMemory (
    ACPI_PHYSICAL_ADDRESS   Where,
    ACPI_SIZE               Length)
{
    UINT8                   *MappedMemory;
    ACPI_PHYSICAL_ADDRESS   Offset;
    ACPI_SIZE               PageSize;
    int                     fd;


    fd = open (SYSTEM_MEMORY, O_RDONLY | O_BINARY);
    if (fd < 0)
    {
        fprintf (stderr, "Cannot open %s\n", SYSTEM_MEMORY);
        return (NULL);
    }

    /* Align the offset to use mmap */

    PageSize = sysconf (_SC_PAGESIZE);
    Offset = Where % PageSize;

    /* Map the table header to get the length of the full table */

    MappedMemory = mmap (NULL, (Length + Offset), PROT_READ, MAP_PRIVATE,
        fd, (Where - Offset));
    close (fd);

    if (MappedMemory == MAP_FAILED)
    {
        return (NULL);
    }

    return (ACPI_CAST8 (MappedMemory + Offset));
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsUnmapMemory
 *
 * PARAMETERS:  Where               - Logical address of memory to be unmapped
 *              Length              - How much memory to unmap
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Delete a previously created mapping. Where and Length must
 *              correspond to a previous mapping exactly.
 *
 *****************************************************************************/

void
AcpiOsUnmapMemory (
    void                    *Where,
    ACPI_SIZE               Length)
{
    ACPI_PHYSICAL_ADDRESS   Offset;
    ACPI_SIZE               PageSize;


    PageSize = sysconf (_SC_PAGESIZE);
    Offset = (ACPI_PHYSICAL_ADDRESS) Where % PageSize;
    munmap ((UINT8 *) Where - Offset, (Length + Offset));
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
 *              local variables. Main ACPI tables include RSDT, FADT, RSDT,
 *              and/or XSDT.
 *
 *****************************************************************************/

static ACPI_STATUS
OslTableInitialize (
    void)
{
    ACPI_TABLE_HEADER       *MappedTable;
    UINT8                   *TableData;
    UINT8                   *RsdpAddress;
    ACPI_PHYSICAL_ADDRESS   RsdpBase;
    ACPI_SIZE               RsdpSize;
    ACPI_STATUS             Status;


    /* Get RSDP from memory */

    RsdpBase = ACPI_HI_RSDP_WINDOW_BASE;
    RsdpSize = ACPI_HI_RSDP_WINDOW_SIZE;

    RsdpAddress = AcpiOsMapMemory (RsdpBase, RsdpSize);
    if (!RsdpAddress)
    {
        return (AE_BAD_ADDRESS);
    }

    /* Search low memory for the RSDP */

    MappedTable = ACPI_CAST_PTR (ACPI_TABLE_HEADER,
        AcpiTbScanMemoryForRsdp (RsdpAddress, RsdpSize));
    if (!MappedTable)
    {
        AcpiOsUnmapMemory (RsdpAddress, RsdpSize);
        return (AE_ERROR);
    }

    ACPI_MEMCPY (&Gbl_Rsdp, MappedTable, sizeof (ACPI_TABLE_RSDP));
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
            return (AE_NO_MEMORY);
        }

        ACPI_MEMCPY (Gbl_Rsdt, MappedTable, MappedTable->Length);
        AcpiOsUnmapMemory (MappedTable, MappedTable->Length);
    }

    /* Get FADT from memory */

    if (Gbl_Revision)
    {
        TableData = ACPI_CAST8 (Gbl_Xsdt) + sizeof (ACPI_TABLE_HEADER);
        Gbl_FadtAddress = (ACPI_PHYSICAL_ADDRESS) (*ACPI_CAST64 (TableData));
    }
    else
    {
        TableData = ACPI_CAST8 (Gbl_Rsdt) + sizeof (ACPI_TABLE_HEADER);
        Gbl_FadtAddress = (ACPI_PHYSICAL_ADDRESS) (*ACPI_CAST32 (TableData));
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
        return (AE_NO_MEMORY);
    }

    ACPI_MEMCPY (Gbl_Fadt, MappedTable, MappedTable->Length);
    AcpiOsUnmapMemory (MappedTable, MappedTable->Length);
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
    UINT8                   *TableData;
    UINT8                   NumberOfTables;
    UINT8                   ItemSize;
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

        if (!TableAddress)
        {
            fprintf (stderr,
                "Could not find a valid address for %4.4s in the FADT\n",
                Signature);

            return (AE_NOT_FOUND);
        }

        /* Now we can get the requested table (DSDT or FACS) */

        Status = OslMapTable (TableAddress, Signature, &MappedTable);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
    }
    else /* Case for a normal ACPI table */
    {
        if (Gbl_Revision)
        {
            ItemSize = sizeof (UINT64);
            TableData = ACPI_CAST8 (Gbl_Xsdt) + sizeof (ACPI_TABLE_HEADER);
            NumberOfTables =
                (Gbl_Xsdt->Header.Length - sizeof (ACPI_TABLE_HEADER))
                / ItemSize;
        }
        else /* Use RSDT if XSDT is not available */
        {
            ItemSize = sizeof (UINT32);
            TableData = ACPI_CAST8 (Gbl_Rsdt) + sizeof (ACPI_TABLE_HEADER);
            NumberOfTables =
                (Gbl_Rsdt->Header.Length - sizeof (ACPI_TABLE_HEADER))
                / ItemSize;
        }

        /* Search RSDT/XSDT for the requested table */

        for (i = 0; i < NumberOfTables; ++i, TableData += ItemSize)
        {
            if (Gbl_Revision)
            {
                TableAddress =
                    (ACPI_PHYSICAL_ADDRESS) (*ACPI_CAST64 (TableData));
            }
            else
            {
                TableAddress =
                    (ACPI_PHYSICAL_ADDRESS) (*ACPI_CAST32 (TableData));
            }

            Status = OslMapTable (TableAddress, NULL, &MappedTable);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }

            /* Does this table match the requested signature? */

            if (!ACPI_COMPARE_NAME (MappedTable->Signature, Signature))
            {
                AcpiOsUnmapMemory (MappedTable, MappedTable->Length);
                MappedTable = NULL;
                continue;
            }

            /* Match table instance (for SSDT/UEFI tables) */

            if (CurrentInstance != Instance)
            {
                AcpiOsUnmapMemory (MappedTable, MappedTable->Length);
                MappedTable = NULL;
                CurrentInstance++;
                continue;
            }

            break;
        }
    }

    if (CurrentInstance != Instance)
    {
        return (AE_LIMIT);
    }

    if (!MappedTable)
    {
        return (AE_NOT_FOUND);
    }

    /* Copy table to local buffer and return it */

    LocalTable = calloc (1, MappedTable->Length);
    if (!LocalTable)
    {
        return (AE_NO_MEMORY);
    }

    ACPI_MEMCPY (LocalTable, MappedTable, MappedTable->Length);
    AcpiOsUnmapMemory (MappedTable, MappedTable->Length);
    *Address = TableAddress;

    *Table = LocalTable;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    OslAddTablesToList
 *
 * PARAMETERS:  Directory           - Directory that contains the tables
 *
 * RETURN:      Status; Table list is initialized if AE_OK.
 *
 * DESCRIPTION: Add ACPI tables to the table list from a directory.
 *
 *****************************************************************************/

static ACPI_STATUS
OslAddTablesToList(
    char                    *Directory)
{
    struct stat             FileInfo;
    OSL_TABLE_INFO          *NewInfo;
    OSL_TABLE_INFO          *Info;
    struct dirent           *DirInfo;
    DIR                     *TableDir;
    char                    TempName[4];
    char                    Filename[PATH_MAX];
    UINT32                  i;


    /* Open the requested directory */

    if (stat (Directory, &FileInfo) == -1)
    {
        return (AE_NOT_FOUND);
    }

    if (!(TableDir = opendir (Directory)))
    {
        return (AE_ERROR);
    }

    /* Move pointer to the end of the list */

    if (!Gbl_TableListHead)
    {
        return (AE_ERROR);
    }

    Info = Gbl_TableListHead;
    for (i = 0; i < Gbl_TableListHead->Instance; i++)
    {
        Info = Info->Next;
    }

    /* Examine all entries in this directory */

    while ((DirInfo = readdir (TableDir)) != 0)
    {
        /* Ignore meaningless files */

        if (DirInfo->d_name[0] == '.')
        {
            continue;
        }

        /* Skip any subdirectories and create a new info node */

        sprintf (Filename, "%s/%s", Directory, DirInfo->d_name);

        if (stat (Filename, &FileInfo) == -1)
        {
            return (AE_ERROR);
        }

        if (!S_ISDIR (FileInfo.st_mode))
        {
            NewInfo = calloc (1, sizeof (OSL_TABLE_INFO));
            if (strlen (DirInfo->d_name) > ACPI_NAME_SIZE)
            {
                sscanf (DirInfo->d_name, "%[^1-9]%d",
                    TempName, &NewInfo->Instance);
            }

            /* Add new info node to global table list */

            sscanf (DirInfo->d_name, "%4s", NewInfo->Signature);
            Info->Next = NewInfo;
            Info = NewInfo;
            Gbl_TableListHead->Instance++;
        }
    }

    closedir (TableDir);
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

    MappedTable = AcpiOsMapMemory (Address, sizeof (ACPI_TABLE_HEADER));
    if (!MappedTable)
    {
        fprintf (stderr, "Could not map table header at 0x%8.8X%8.8X\n",
            ACPI_FORMAT_UINT64 (Address));
        return (AE_BAD_ADDRESS);
    }

    /* Check if table is valid */

    if (!ApIsValidHeader (MappedTable))
    {
        return (AE_BAD_HEADER);
    }

    /* If specified, signature must match */

    if (Signature &&
        !ACPI_COMPARE_NAME (Signature, MappedTable->Signature))
    {
        return (AE_NOT_EXIST);
    }

    /* Map the entire table */

    Length = MappedTable->Length;
    AcpiOsUnmapMemory (MappedTable, sizeof (ACPI_TABLE_HEADER));

    MappedTable = AcpiOsMapMemory (Address, Length);
    if (!MappedTable)
    {
        fprintf (stderr, "Could not map table at 0x%8.8X%8.8X length %8.8X\n",
            ACPI_FORMAT_UINT64 (Address), Length);
        return (AE_NO_MEMORY);
    }

    *Table = MappedTable;

    /*
     * Checksum for RSDP.
     * Note: Other checksums are computed during the table dump.
     */

    if (AcpiTbValidateRsdp (ACPI_CAST_PTR (ACPI_TABLE_RSDP, MappedTable)) ==
        AE_BAD_CHECKSUM)
    {
        fprintf (stderr, "Warning: wrong checksum for RSDP\n");
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    OslReadTableFromFile
 *
 * PARAMETERS:  TableFile           - File that contains the desired table
 *              FileOffset          - Offset of the table in file
 *              Signature           - Optional ACPI Signature for desired table.
 *                                    A null terminated 4-character string.
 *              Table               - Where a pointer to the table is returned
 *
 * RETURN:      Status; Table buffer is returned if AE_OK.
 *
 * DESCRIPTION: Read a ACPI table from a file.
 *
 *****************************************************************************/

static ACPI_STATUS
OslReadTableFromFile (
    FILE                    *TableFile,
    ACPI_SIZE               FileOffset,
    char                    *Signature,
    ACPI_TABLE_HEADER       **Table)
{
    ACPI_TABLE_HEADER       Header;
    ACPI_TABLE_RSDP         Rsdp;
    ACPI_TABLE_HEADER       *LocalTable;
    UINT32                  TableLength;
    UINT32                  Count;


    /* Read the table header */

    fseek (TableFile, FileOffset, SEEK_SET);

    Count = fread (&Header, 1, sizeof (ACPI_TABLE_HEADER), TableFile);
    if (Count != sizeof (ACPI_TABLE_HEADER))
    {
        fprintf (stderr, "Could not read ACPI table header from file\n");
        return (AE_BAD_HEADER);
    }

    /* Check if table is valid */

    if (!ApIsValidHeader (&Header))
    {
        return (AE_BAD_HEADER);
    }

    /* If signature is specified, it must match the table */

    if (Signature &&
        !ACPI_COMPARE_NAME (Signature, Header.Signature))
    {
        fprintf (stderr, "Incorrect signature: Expecting %4.4s, found %4.4s\n",
            Signature, Header.Signature);
        return (AE_NOT_FOUND);
    }

    /*
     * For RSDP, we must read the entire table, because the length field
     * is in a non-standard place, beyond the normal ACPI header.
     */
    if (ACPI_COMPARE_NAME (Header.Signature, ACPI_SIG_RSDP))
    {
        fseek (TableFile, FileOffset, SEEK_SET);

        Count = fread (&Rsdp, 1, sizeof (ACPI_TABLE_RSDP), TableFile);
        if (Count != sizeof (ACPI_TABLE_RSDP))
        {
            fprintf (stderr, "Error while reading RSDP\n");
            return (AE_NOT_FOUND);
        }

        TableLength = Rsdp.Length;
    }
    else
    {
        TableLength = Header.Length;
    }

    /* Read the entire table into a local buffer */

    LocalTable = calloc (1, TableLength);
    if (!LocalTable)
    {
        fprintf (stderr,
            "%4.4s: Could not allocate buffer for table of length %X\n",
            Header.Signature, TableLength);
        return (AE_NO_MEMORY);
    }

    fseek (TableFile, FileOffset, SEEK_SET);

    Count = fread (LocalTable, 1, TableLength, TableFile);
    if (Count != TableLength)
    {
        fprintf (stderr, "%4.4s: Error while reading table content\n",
            Header.Signature);
        return (AE_NOT_FOUND);
    }

    /* Validate checksum, except for special tables */

    if (!ACPI_COMPARE_NAME (Header.Signature, ACPI_SIG_S3PT) &&
        !ACPI_COMPARE_NAME (Header.Signature, ACPI_SIG_FACS))
    {
        if (AcpiTbChecksum ((UINT8 *) LocalTable, TableLength))
        {
            fprintf (stderr, "%4.4s: Warning: wrong checksum\n",
                Header.Signature);
        }
    }

    *Table = LocalTable;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    OslGetOverrideTable
 *
 * PARAMETERS:  Signature       - ACPI Signature for desired table. Must be
 *                                a null terminated 4-character string.
 *              Instance        - Multiple table support for SSDT/UEFI (0...n)
 *                                Must be 0 for other tables.
 *              Table           - Where a pointer to the table is returned
 *              Address         - Where the table physical address is returned
 *
 * RETURN:      Status; Table buffer is returned if AE_OK.
 *              AE_NOT_FOUND: A valid table was not found at the address
 *
 * DESCRIPTION: Get a table that was overridden and appears under the
 *              directory OVERRIDE_TABLE_DIR.
 *
 *****************************************************************************/

static ACPI_STATUS
OslGetOverrideTable (
    char                    *Signature,
    UINT32                  Instance,
    ACPI_TABLE_HEADER       **Table,
    ACPI_PHYSICAL_ADDRESS   *Address)
{
    ACPI_TABLE_HEADER       Header;
    struct stat             FileInfo;
    struct dirent           *DirInfo;
    DIR                     *TableDir;
    FILE                    *TableFile = NULL;
    UINT32                  CurrentInstance = 0;
    UINT32                  Count;
    char                    TempName[4];
    char                    TableFilename[PATH_MAX];
    ACPI_STATUS             Status;


    /* Open the directory for override tables */

    if (stat (OVERRIDE_TABLE_DIR, &FileInfo) == -1)
    {
        return (AE_NOT_FOUND);
    }

    if (!(TableDir = opendir (OVERRIDE_TABLE_DIR)))
    {
        return (AE_ERROR);
    }

    /* Attempt to find the table in the directory */

    while ((DirInfo = readdir (TableDir)) != 0)
    {
        /* Ignore meaningless files */

        if (DirInfo->d_name[0] == '.')
        {
            continue;
        }

        if (!ACPI_COMPARE_NAME (DirInfo->d_name, Signature))
        {
            continue;
        }

        if (strlen (DirInfo->d_name) > 4)
        {
            sscanf (DirInfo->d_name, "%[^1-9]%d", TempName, &CurrentInstance);
            if (CurrentInstance != Instance)
            {
                continue;
            }
        }

        /* Create the table pathname and open the file */

        sprintf (TableFilename, "%s/%s", OVERRIDE_TABLE_DIR, DirInfo->d_name);

        TableFile = fopen (TableFilename, "rb");
        if (TableFile == NULL)
        {
            perror (TableFilename);
            return (AE_ERROR);
        }

        /* Read the Table header to get the table length */

        Count = fread (&Header, 1, sizeof (ACPI_TABLE_HEADER), TableFile);
        if (Count != sizeof (ACPI_TABLE_HEADER))
        {
            fclose (TableFile);
            return (AE_ERROR);
        }

        break;
    }

    closedir (TableDir);
    if (ACPI_COMPARE_NAME (Signature, ACPI_SIG_SSDT) && !TableFile)
    {
        return (AE_LIMIT);
    }

    if (!TableFile)
    {
        return (AE_NOT_FOUND);
    }

    /* There is no physical address saved for override tables, use zero */

    *Address = 0;
    Status = OslReadTableFromFile (TableFile, 0, NULL, Table);

    fclose (TableFile);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    OslGetDynamicSsdt
 *
 * PARAMETERS:  Instance        - For SSDTs (0...n)
 *              Table           - Where a pointer to the table is returned
 *              Address         - Where the table physical address is returned
 *
 * RETURN:      Status; Table buffer is returned if AE_OK.
 *              AE_NOT_FOUND: A valid table was not found at the address
 *
 * DESCRIPTION: Get an SSDT table under directory DYNAMIC_SSDT_DIR.
 *
 *****************************************************************************/

static ACPI_STATUS
OslGetDynamicSsdt (
    UINT32                  Instance,
    ACPI_TABLE_HEADER       **Table,
    ACPI_PHYSICAL_ADDRESS   *Address)
{
    ACPI_TABLE_HEADER       Header;
    struct stat             FileInfo;
    struct dirent           *DirInfo;
    DIR                     *TableDir;
    FILE                    *TableFile = NULL;
    UINT32                  Count;
    UINT32                  CurrentInstance = 0;
    char                    TempName[4];
    char                    TableFilename[PATH_MAX];
    ACPI_STATUS             Status;


    /* Open the directory for dynamically loaded SSDTs */

    if (stat (DYNAMIC_SSDT_DIR, &FileInfo) == -1)
    {
        return (AE_NOT_FOUND);
    }

    if (!(TableDir = opendir (DYNAMIC_SSDT_DIR)))
    {
        return (AE_ERROR);
    }

    /* Search directory for correct SSDT instance */

    while ((DirInfo = readdir (TableDir)) != 0)
    {
        /* Ignore meaningless files */

        if (DirInfo->d_name[0] == '.')
        {
            continue;
        }

        /* Check if this table is what we need */

        sscanf (DirInfo->d_name, "%[^1-9]%d", TempName, &CurrentInstance);
        if (CurrentInstance != Instance)
        {
            continue;
        }

        /* Get the SSDT filename and open the file */

        sprintf (TableFilename, "%s/%s", DYNAMIC_SSDT_DIR, DirInfo->d_name);

        TableFile = fopen (TableFilename, "rb");
        if (TableFile == NULL)
        {
            perror (TableFilename);
            return (AE_ERROR);
        }

        /* Read the Table header to get the table length */

        Count = fread (&Header, 1, sizeof (ACPI_TABLE_HEADER), TableFile);
        if (Count != sizeof (ACPI_TABLE_HEADER))
        {
            fclose (TableFile);
            return (AE_ERROR);
        }

        break;
    }

    closedir (TableDir);
    if (CurrentInstance != Instance)
    {
        return (AE_LIMIT);
    }

    /* There is no physical address saved for dynamic SSDTs, use zero */

    *Address = 0;
    Status = OslReadTableFromFile (TableFile, Header.Length, NULL, Table);

    fclose (TableFile);
    return (Status);
}
