/******************************************************************************
 *
 * Module Name: tbxfroot - Find the root ACPI table (RSDT)
 *              $Revision: 64 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2002, Intel Corp.
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

#define __TBXFROOT_C__

#include "acpi.h"
#include "actables.h"


#define _COMPONENT          ACPI_TABLES
        ACPI_MODULE_NAME    ("tbxfroot")


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbFindTable
 *
 * PARAMETERS:  Signature           - String with ACPI table signature
 *              OemId               - String with the table OEM ID
 *              OemTableId          - String with the OEM Table ID.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Find an ACPI table (in the RSDT/XSDT) that matches the
 *              Signature, OEM ID and OEM Table ID.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbFindTable (
    NATIVE_CHAR             *Signature,
    NATIVE_CHAR             *OemId,
    NATIVE_CHAR             *OemTableId,
    ACPI_TABLE_HEADER       **TablePtr)
{
    ACPI_STATUS             Status;
    ACPI_TABLE_HEADER       *Table;


    ACPI_FUNCTION_TRACE ("TbFindTable");


    /* Validate string lengths */

    if ((ACPI_STRLEN (Signature)  > ACPI_NAME_SIZE) ||
        (ACPI_STRLEN (OemId)      > sizeof (Table->OemId)) ||
        (ACPI_STRLEN (OemTableId) > sizeof (Table->OemTableId)))
    {
        return_ACPI_STATUS (AE_AML_STRING_LIMIT);
    }

    /* Find the table */

    Status = AcpiGetFirmwareTable (Signature, 1,
                        ACPI_LOGICAL_ADDRESSING, &Table);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Check OemId and OemTableId */

    if ((OemId[0]      && ACPI_STRCMP (OemId, Table->OemId)) ||
        (OemTableId[0] && ACPI_STRCMP (OemTableId, Table->OemTableId)))
    {
        return_ACPI_STATUS (AE_AML_NAME_NOT_FOUND);
    }

    *TablePtr = Table;
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetFirmwareTable
 *
 * PARAMETERS:  Signature       - Any ACPI table signature
 *              Instance        - the non zero instance of the table, allows
 *                                support for multiple tables of the same type
 *              Flags           - Physical/Virtual support
 *              RetBuffer       - pointer to a structure containing a buffer to
 *                                receive the table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to get an ACPI table.  The caller
 *              supplies an OutBuffer large enough to contain the entire ACPI
 *              table.  Upon completion
 *              the OutBuffer->Length field will indicate the number of bytes
 *              copied into the OutBuffer->BufPtr buffer.  This table will be
 *              a complete table including the header.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiGetFirmwareTable (
    ACPI_STRING             Signature,
    UINT32                  Instance,
    UINT32                  Flags,
    ACPI_TABLE_HEADER       **TablePointer)
{
    ACPI_POINTER            RsdpAddress;
    ACPI_POINTER            Address;
    ACPI_STATUS             Status;
    ACPI_TABLE_HEADER       Header;
    ACPI_TABLE_DESC         TableInfo;
    ACPI_TABLE_DESC         RsdtInfo;
    UINT32                  TableCount;
    UINT32                  i;
    UINT32                  j;


    ACPI_FUNCTION_TRACE ("AcpiGetFirmwareTable");


    /*
     * Ensure that at least the table manager is initialized.  We don't
     * require that the entire ACPI subsystem is up for this interface
     */

    /*
     *  If we have a buffer, we must have a length too
     */
    if ((Instance == 0)                 ||
        (!Signature)                    ||
        (!TablePointer))
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    RsdtInfo.Pointer = NULL;

    if (!AcpiGbl_RSDP)
    {
        /* Get the RSDP */

        Status = AcpiOsGetRootPointer (Flags, &RsdpAddress);
        if (ACPI_FAILURE (Status))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "RSDP  not found\n"));
            return_ACPI_STATUS (AE_NO_ACPI_TABLES);
        }

        /* Map and validate the RSDP */

        if ((Flags & ACPI_MEMORY_MODE) == ACPI_LOGICAL_ADDRESSING)
        {
            Status = AcpiOsMapMemory (RsdpAddress.Pointer.Physical, sizeof (RSDP_DESCRIPTOR),
                                        (void **) &AcpiGbl_RSDP);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
        }
        else
        {
            AcpiGbl_RSDP = RsdpAddress.Pointer.Logical;
        }

        /*
         *  The signature and checksum must both be correct
         */
        if (ACPI_STRNCMP ((NATIVE_CHAR *) AcpiGbl_RSDP, RSDP_SIG, sizeof (RSDP_SIG)-1) != 0)
        {
            /* Nope, BAD Signature */

            return_ACPI_STATUS (AE_BAD_SIGNATURE);
        }

        if (AcpiTbChecksum (AcpiGbl_RSDP, ACPI_RSDP_CHECKSUM_LENGTH) != 0)
        {
            /* Nope, BAD Checksum */

            return_ACPI_STATUS (AE_BAD_CHECKSUM);
        }
    }

    /* Get the RSDT and validate it */

    AcpiTbGetRsdtAddress (&Address);

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
        "RSDP located at %p, RSDT physical=%8.8X%8.8X \n",
        AcpiGbl_RSDP,
        ACPI_HIDWORD (Address.Pointer.Value),
        ACPI_LODWORD (Address.Pointer.Value)));

    /* Insert ProcessorMode flags */

    Address.PointerType |= Flags;

    Status = AcpiTbGetTable (&Address, &RsdtInfo);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiTbValidateRsdt (RsdtInfo.Pointer);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    /* Get the number of table pointers within the RSDT */

    TableCount = AcpiTbGetTableCount (AcpiGbl_RSDP, RsdtInfo.Pointer);

    Address.PointerType = AcpiGbl_TableFlags | Flags;

    /*
     * Search the RSDT/XSDT for the correct instance of the
     * requested table
     */
    for (i = 0, j = 0; i < TableCount; i++)
    {
        /* Get the next table pointer, handle RSDT vs. XSDT */

        if (AcpiGbl_RSDP->Revision < 2)
        {
            Address.Pointer.Value = ((RSDT_DESCRIPTOR *) RsdtInfo.Pointer)->TableOffsetEntry[i];
        }
        else
        {
            Address.Pointer.Value = ACPI_GET_ADDRESS (
                ((XSDT_DESCRIPTOR *) RsdtInfo.Pointer)->TableOffsetEntry[i]);
        }

        /* Get the table header */

        Status = AcpiTbGetTableHeader (&Address, &Header);
        if (ACPI_FAILURE (Status))
        {
            goto Cleanup;
        }

        /* Compare table signatures and table instance */

        if (!ACPI_STRNCMP (Header.Signature, Signature, ACPI_NAME_SIZE))
        {
            /* An instance of the table was found */

            j++;
            if (j >= Instance)
            {
                /* Found the correct instance, get the entire table */

                Status = AcpiTbGetTableBody (&Address, &Header, &TableInfo);
                if (ACPI_FAILURE (Status))
                {
                    goto Cleanup;
                }

                *TablePointer = TableInfo.Pointer;
                goto Cleanup;
            }
        }
    }

    /* Did not find the table */

    Status = AE_NOT_EXIST;


Cleanup:
    AcpiOsUnmapMemory (RsdtInfo.Pointer, (ACPI_SIZE) RsdtInfo.Pointer->Length);
    return_ACPI_STATUS (Status);
}


/* TBD: Move to a new file */

#if ACPI_MACHINE_WIDTH != 16

/*******************************************************************************
 *
 * FUNCTION:    AcpiFindRootPointer
 *
 * PARAMETERS:  **RsdpAddress           - Where to place the RSDP address
 *              Flags                   - Logical/Physical addressing
 *
 * RETURN:      Status, Physical address of the RSDP
 *
 * DESCRIPTION: Find the RSDP
 *
 ******************************************************************************/

ACPI_STATUS
AcpiFindRootPointer (
    UINT32                  Flags,
    ACPI_POINTER            *RsdpAddress)
{
    ACPI_TABLE_DESC         TableInfo;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("AcpiFindRootPointer");


    /* Get the RSDP */

    Status = AcpiTbFindRsdp (&TableInfo, Flags);
    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "RSDP structure not found\n"));
        return_ACPI_STATUS (AE_NO_ACPI_TABLES);
    }

    RsdpAddress->PointerType = ACPI_PHYSICAL_POINTER;
    RsdpAddress->Pointer.Physical = TableInfo.PhysicalAddress;
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbScanMemoryForRsdp
 *
 * PARAMETERS:  StartAddress        - Starting pointer for search
 *              Length              - Maximum length to search
 *
 * RETURN:      Pointer to the RSDP if found, otherwise NULL.
 *
 * DESCRIPTION: Search a block of memory for the RSDP signature
 *
 ******************************************************************************/

UINT8 *
AcpiTbScanMemoryForRsdp (
    UINT8                   *StartAddress,
    UINT32                  Length)
{
    UINT32                  Offset;
    UINT8                   *MemRover;


    ACPI_FUNCTION_TRACE ("TbScanMemoryForRsdp");


    /* Search from given start addr for the requested length  */

    for (Offset = 0, MemRover = StartAddress;
         Offset < Length;
         Offset += RSDP_SCAN_STEP, MemRover += RSDP_SCAN_STEP)
    {

        /* The signature and checksum must both be correct */

        if (ACPI_STRNCMP ((NATIVE_CHAR *) MemRover,
                RSDP_SIG, sizeof (RSDP_SIG)-1) == 0 &&
            AcpiTbChecksum (MemRover, ACPI_RSDP_CHECKSUM_LENGTH) == 0)
        {
            /* If so, we have found the RSDP */

            ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
                "RSDP located at physical address %p\n",MemRover));
            return_PTR (MemRover);
        }
    }

    /* Searched entire block, no RSDP was found */

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO,"Searched entire block, no RSDP was found.\n"));
    return_PTR (NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbFindRsdp
 *
 * PARAMETERS:  *TableInfo              - Where the table info is returned
 *              Flags                   - Current memory mode (logical vs.
 *                                        physical addressing)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Search lower 1Mbyte of memory for the root system descriptor
 *              pointer structure.  If it is found, set *RSDP to point to it.
 *
 *              NOTE: The RSDP must be either in the first 1K of the Extended
 *              BIOS Data Area or between E0000 and FFFFF (ACPI 1.0 section
 *              5.2.2; assertion #421).
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbFindRsdp (
    ACPI_TABLE_DESC         *TableInfo,
    UINT32                  Flags)
{
    UINT8                   *TablePtr;
    UINT8                   *MemRover;
    UINT64                  PhysAddr;
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE ("TbFindRsdp");


    /*
     * Scan supports either 1) Logical addressing or 2) Physical addressing
     */
    if ((Flags & ACPI_MEMORY_MODE) == ACPI_LOGICAL_ADDRESSING)
    {
        /*
         * 1) Search EBDA (low memory) paragraphs
         */
        Status = AcpiOsMapMemory ((UINT64) LO_RSDP_WINDOW_BASE, LO_RSDP_WINDOW_SIZE,
                                    (void **) &TablePtr);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        MemRover = AcpiTbScanMemoryForRsdp (TablePtr, LO_RSDP_WINDOW_SIZE);
        AcpiOsUnmapMemory (TablePtr, LO_RSDP_WINDOW_SIZE);

        if (MemRover)
        {
            /* Found it, return the physical address */

            PhysAddr = LO_RSDP_WINDOW_BASE;
            PhysAddr += ACPI_PTR_DIFF (MemRover,TablePtr);

            TableInfo->PhysicalAddress = PhysAddr;
            return_ACPI_STATUS (AE_OK);
        }

        /*
         * 2) Search upper memory: 16-byte boundaries in E0000h-F0000h
         */
        Status = AcpiOsMapMemory ((UINT64) HI_RSDP_WINDOW_BASE, HI_RSDP_WINDOW_SIZE,
                                    (void **) &TablePtr);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        MemRover = AcpiTbScanMemoryForRsdp (TablePtr, HI_RSDP_WINDOW_SIZE);
        AcpiOsUnmapMemory (TablePtr, HI_RSDP_WINDOW_SIZE);

        if (MemRover)
        {
            /* Found it, return the physical address */

            PhysAddr = HI_RSDP_WINDOW_BASE;
            PhysAddr += ACPI_PTR_DIFF (MemRover, TablePtr);

            TableInfo->PhysicalAddress = PhysAddr;
            return_ACPI_STATUS (AE_OK);
        }
    }

    /*
     * Physical addressing
     */
    else
    {
        /*
         * 1) Search EBDA (low memory) paragraphs
         */
        MemRover = AcpiTbScanMemoryForRsdp (ACPI_PHYSADDR_TO_PTR (LO_RSDP_WINDOW_BASE),
                        LO_RSDP_WINDOW_SIZE);
        if (MemRover)
        {
            /* Found it, return the physical address */

            TableInfo->PhysicalAddress = ACPI_TO_INTEGER (MemRover);
            return_ACPI_STATUS (AE_OK);
        }

        /*
         * 2) Search upper memory: 16-byte boundaries in E0000h-F0000h
         */
        MemRover = AcpiTbScanMemoryForRsdp (ACPI_PHYSADDR_TO_PTR (HI_RSDP_WINDOW_BASE),
                        HI_RSDP_WINDOW_SIZE);
        if (MemRover)
        {
            /* Found it, return the physical address */

            TableInfo->PhysicalAddress = ACPI_TO_INTEGER (MemRover);
            return_ACPI_STATUS (AE_OK);
        }
    }

    /* RSDP signature was not found */

    return_ACPI_STATUS (AE_NOT_FOUND);
}

#endif

