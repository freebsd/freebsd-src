/******************************************************************************
 *
 * Module Name: dmtbdump - Dump ACPI data tables that contain no AML code
 *              $Revision: 1.15 $
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

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acdisasm.h>
#include <contrib/dev/acpica/actables.h>

/* This module used for application-level code only */

#define _COMPONENT          ACPI_CA_DISASSEMBLER
        ACPI_MODULE_NAME    ("dmtbdump")


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpRsdp
 *
 * PARAMETERS:  Table               - A RSDP
 *
 * RETURN:      Length of the table (there is no length field, use revision)
 *
 * DESCRIPTION: Format the contents of a RSDP
 *
 ******************************************************************************/

UINT32
AcpiDmDumpRsdp (
    ACPI_TABLE_HEADER       *Table)
{
    UINT32                  Length = ACPI_RSDP_REV0_SIZE;


    /* Dump the common ACPI 1.0 portion */

    AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoRsdp1);

    /* ACPI 2.0+ contains more data and has a Length field */

    if (ACPI_CAST_PTR (ACPI_TABLE_RSDP, Table)->Revision > 0)
    {
        Length = ACPI_CAST_PTR (ACPI_TABLE_RSDP, Table)->Length;
        AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoRsdp2);
    }

    return (Length);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpRsdt
 *
 * PARAMETERS:  Table               - A RSDT
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a RSDT
 *
 ******************************************************************************/

void
AcpiDmDumpRsdt (
    ACPI_TABLE_HEADER       *Table)
{
    UINT32                  *Array;
    UINT32                  Entries;
    UINT32                  Offset;
    UINT32                  i;


    /* Point to start of table pointer array */

    Array = ACPI_CAST_PTR (ACPI_TABLE_RSDT, Table)->TableOffsetEntry;
    Offset = sizeof (ACPI_TABLE_HEADER);

    /* RSDT uses 32-bit pointers */

    Entries = (Table->Length - sizeof (ACPI_TABLE_HEADER)) / sizeof (UINT32);

    for (i = 0; i < Entries; i++)
    {
        AcpiDmLineHeader2 (Offset, sizeof (UINT32), "ACPI Table Address", i);
        AcpiOsPrintf ("%8.8X\n", Array[i]);
        Offset += sizeof (UINT32);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpXsdt
 *
 * PARAMETERS:  Table               - A XSDT
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a XSDT
 *
 ******************************************************************************/

void
AcpiDmDumpXsdt (
    ACPI_TABLE_HEADER       *Table)
{
    UINT64                  *Array;
    UINT32                  Entries;
    UINT32                  Offset;
    UINT32                  i;


    /* Point to start of table pointer array */

    Array = ACPI_CAST_PTR (ACPI_TABLE_XSDT, Table)->TableOffsetEntry;
    Offset = sizeof (ACPI_TABLE_HEADER);

    /* XSDT uses 64-bit pointers */

    Entries = (Table->Length - sizeof (ACPI_TABLE_HEADER)) / sizeof (UINT64);

    for (i = 0; i < Entries; i++)
    {
        AcpiDmLineHeader2 (Offset, sizeof (UINT64), "ACPI Table Address", i);
        AcpiOsPrintf ("%8.8X%8.8X\n", ACPI_FORMAT_UINT64 (Array[i]));
        Offset += sizeof (UINT64);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpFadt
 *
 * PARAMETERS:  Table               - A FADT
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a FADT
 *
 ******************************************************************************/

void
AcpiDmDumpFadt (
    ACPI_TABLE_HEADER       *Table)
{

    /* Common ACPI 1.0 portion of FADT */

    AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoFadt1);

    /* Check for ACPI 2.0+ extended data (cannot depend on Revision field) */

    if (Table->Length >= sizeof (ACPI_TABLE_FADT))
    {
        AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoFadt2);
    }

    /* Validate various fields in the FADT, including length */

    AcpiTbCreateLocalFadt (Table, Table->Length);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpAsf
 *
 * PARAMETERS:  Table               - A ASF table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a ASF table
 *
 ******************************************************************************/

void
AcpiDmDumpAsf (
    ACPI_TABLE_HEADER       *Table)
{
    UINT32                  Offset = sizeof (ACPI_TABLE_HEADER);
    ACPI_ASF_INFO           *SubTable;
    ACPI_DMTABLE_INFO       *InfoTable;
    ACPI_DMTABLE_INFO       *DataInfoTable = NULL;
    UINT8                   *DataTable = NULL;
    ACPI_NATIVE_UINT        DataCount = 0;
    ACPI_NATIVE_UINT        DataLength = 0;
    ACPI_NATIVE_UINT        DataOffset = 0;
    ACPI_NATIVE_UINT        i;


    /* No main table, only sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_ASF_INFO, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common sub-table header */

        AcpiDmDumpTable (Table->Length, Offset, SubTable, 0, AcpiDmTableInfoAsfHdr);

        switch (SubTable->Header.Type & 0x7F) /* Mask off top bit */
        {
        case ACPI_ASF_TYPE_INFO:
            InfoTable = AcpiDmTableInfoAsf0;
            break;

        case ACPI_ASF_TYPE_ALERT:
            InfoTable = AcpiDmTableInfoAsf1;
            DataInfoTable = AcpiDmTableInfoAsf1a;
            DataTable = ACPI_ADD_PTR (UINT8, SubTable, sizeof (ACPI_ASF_ALERT));
            DataCount = ((ACPI_ASF_ALERT *) SubTable)->Alerts;
            DataLength = ((ACPI_ASF_ALERT *) SubTable)->DataLength;
            DataOffset = Offset + sizeof (ACPI_ASF_ALERT);
            break;

        case ACPI_ASF_TYPE_CONTROL:
            InfoTable = AcpiDmTableInfoAsf2;
            DataInfoTable = AcpiDmTableInfoAsf2a;
            DataTable = ACPI_ADD_PTR (UINT8, SubTable, sizeof (ACPI_ASF_REMOTE));
            DataCount = ((ACPI_ASF_REMOTE *) SubTable)->Controls;
            DataLength = ((ACPI_ASF_REMOTE *) SubTable)->DataLength;
            DataOffset = Offset + sizeof (ACPI_ASF_REMOTE);
            break;

        case ACPI_ASF_TYPE_BOOT:
            InfoTable = AcpiDmTableInfoAsf3;
            break;

        case ACPI_ASF_TYPE_ADDRESS:
            InfoTable = AcpiDmTableInfoAsf4;
            DataTable = ACPI_ADD_PTR (UINT8, SubTable, sizeof (ACPI_ASF_ADDRESS));
            DataLength = ((ACPI_ASF_ADDRESS *) SubTable)->Devices;
            DataOffset = Offset + sizeof (ACPI_ASF_ADDRESS);
            break;

        default:
            AcpiOsPrintf ("\n**** Unknown ASF sub-table type %X\n", SubTable->Header.Type);
            return;
        }

        AcpiDmDumpTable (Table->Length, Offset, SubTable, SubTable->Header.Length, InfoTable);


        /* Dump variable-length extra data */

        switch (SubTable->Header.Type & 0x7F) /* Mask off top bit */
        {
        case ACPI_ASF_TYPE_ALERT:
        case ACPI_ASF_TYPE_CONTROL:

            for (i = 0; i < DataCount; i++)
            {
                AcpiOsPrintf ("\n");
                AcpiDmDumpTable (Table->Length, DataOffset, DataTable, DataLength, DataInfoTable);

                DataTable = ACPI_ADD_PTR (UINT8, DataTable, DataLength);
                DataOffset += DataLength;
            }
            break;

        case ACPI_ASF_TYPE_ADDRESS:

            for (i = 0; i < DataLength; i++)
            {
                if (!(i % 16))
                {
                    AcpiDmLineHeader (DataOffset, 1, "Addresses");
                }

                AcpiOsPrintf ("%2.2X ", *DataTable);
                DataTable++;
                DataOffset++;
            }

            AcpiOsPrintf ("\n");
            break;
        }

        AcpiOsPrintf ("\n");

        /* Point to next sub-table */

        Offset += SubTable->Header.Length;
        SubTable = ACPI_ADD_PTR (ACPI_ASF_INFO, SubTable, SubTable->Header.Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpCpep
 *
 * PARAMETERS:  Table               - A CPEP table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a CPEP. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpCpep (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_CPEP_POLLING       *SubTable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_CPEP);


    /* Main table */

    AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoCpep);

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_CPEP_POLLING, Table, Offset);
    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");
        AcpiDmDumpTable (Length, Offset, SubTable, SubTable->Length, AcpiDmTableInfoCpep0);

        /* Point to next sub-table */

        Offset += SubTable->Length;
        SubTable = ACPI_ADD_PTR (ACPI_CPEP_POLLING, SubTable, SubTable->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpDmar
 *
 * PARAMETERS:  Table               - A DMAR table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a DMAR. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpDmar (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_DMAR_HEADER        *SubTable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_DMAR);
    ACPI_DMTABLE_INFO       *InfoTable;
    ACPI_DMAR_DEVICE_SCOPE  *ScopeTable;
    UINT32                  ScopeOffset;
    UINT8                   *PciPath;
    UINT32                  PathOffset;


    /* Main table */

    AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoDmar);

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_DMAR_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common sub-table header */

        AcpiOsPrintf ("\n");
        AcpiDmDumpTable (Length, Offset, SubTable, 0, AcpiDmTableInfoDmarHdr);

        switch (SubTable->Type)
        {
        case ACPI_DMAR_TYPE_HARDWARE_UNIT:
            InfoTable = AcpiDmTableInfoDmar0;
            ScopeOffset = sizeof (ACPI_DMAR_HARDWARE_UNIT);
            break;
        case ACPI_DMAR_TYPE_RESERVED_MEMORY:
            InfoTable = AcpiDmTableInfoDmar1;
            ScopeOffset = sizeof (ACPI_DMAR_RESERVED_MEMORY);
            break;
        default:
            AcpiOsPrintf ("\n**** Unknown DMAR sub-table type %X\n\n", SubTable->Type);
            return;
        }

        AcpiDmDumpTable (Length, Offset, SubTable, SubTable->Length, InfoTable);

        /*
         * Currently, a common flag indicates whether there are any
         * device scope entries present at the end of the subtable.
         */
        if ((SubTable->Flags & ACPI_DMAR_INCLUDE_ALL) == 0)
        {
            /* Dump the device scope entries */

            ScopeTable = ACPI_ADD_PTR (ACPI_DMAR_DEVICE_SCOPE, SubTable, ScopeOffset);
            while (ScopeOffset < SubTable->Length)
            {
                AcpiOsPrintf ("\n");
                AcpiDmDumpTable (Length, Offset + ScopeOffset, ScopeTable,
                    ScopeTable->Length, AcpiDmTableInfoDmarScope);

                /* Dump the PCI Path entries for this device scope */

                PathOffset = sizeof (ACPI_DMAR_DEVICE_SCOPE); /* Path entries start at this offset */

                PciPath = ACPI_ADD_PTR (UINT8, ScopeTable,
                    sizeof (ACPI_DMAR_DEVICE_SCOPE));

                while (PathOffset < ScopeTable->Length)
                {
                    AcpiDmLineHeader ((PathOffset + ScopeOffset + Offset), 2, "PCI Path");
                    AcpiOsPrintf ("[%2.2X, %2.2X]\n", PciPath[0], PciPath[1]);

                    /* Point to next PCI Path entry */

                    PathOffset += 2;
                    PciPath += 2;
                }

                /* Point to next device scope entry */

                ScopeOffset += ScopeTable->Length;
                ScopeTable = ACPI_ADD_PTR (ACPI_DMAR_DEVICE_SCOPE,
                    ScopeTable, ScopeTable->Length);
            }
        }

        /* Point to next sub-table */

        Offset += SubTable->Length;
        SubTable = ACPI_ADD_PTR (ACPI_DMAR_HEADER, SubTable, SubTable->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpMadt
 *
 * PARAMETERS:  Table               - A MADT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a MADT. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpMadt (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_SUBTABLE_HEADER    *SubTable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_MADT);
    ACPI_DMTABLE_INFO       *InfoTable;


    /* Main table */

    AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoMadt);

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_SUBTABLE_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common sub-table header */

        AcpiOsPrintf ("\n");
        AcpiDmDumpTable (Length, Offset, SubTable, 0, AcpiDmTableInfoMadtHdr);

        switch (SubTable->Type)
        {
        case ACPI_MADT_TYPE_LOCAL_APIC:
            InfoTable = AcpiDmTableInfoMadt0;
            break;
        case ACPI_MADT_TYPE_IO_APIC:
            InfoTable = AcpiDmTableInfoMadt1;
            break;
        case ACPI_MADT_TYPE_INTERRUPT_OVERRIDE:
            InfoTable = AcpiDmTableInfoMadt2;
            break;
        case ACPI_MADT_TYPE_NMI_SOURCE:
            InfoTable = AcpiDmTableInfoMadt3;
            break;
        case ACPI_MADT_TYPE_LOCAL_APIC_NMI:
            InfoTable = AcpiDmTableInfoMadt4;
            break;
        case ACPI_MADT_TYPE_LOCAL_APIC_OVERRIDE:
            InfoTable = AcpiDmTableInfoMadt5;
            break;
        case ACPI_MADT_TYPE_IO_SAPIC:
            InfoTable = AcpiDmTableInfoMadt6;
            break;
        case ACPI_MADT_TYPE_LOCAL_SAPIC:
            InfoTable = AcpiDmTableInfoMadt7;
            break;
        case ACPI_MADT_TYPE_INTERRUPT_SOURCE:
            InfoTable = AcpiDmTableInfoMadt8;
            break;
        default:
            AcpiOsPrintf ("\n**** Unknown MADT sub-table type %X\n\n", SubTable->Type);
            return;
        }

        AcpiDmDumpTable (Length, Offset, SubTable, SubTable->Length, InfoTable);

        /* Point to next sub-table */

        Offset += SubTable->Length;
        SubTable = ACPI_ADD_PTR (ACPI_SUBTABLE_HEADER, SubTable, SubTable->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpMcfg
 *
 * PARAMETERS:  Table               - A MCFG Table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a MCFG table
 *
 ******************************************************************************/

void
AcpiDmDumpMcfg (
    ACPI_TABLE_HEADER       *Table)
{
    UINT32                  Offset = sizeof (ACPI_TABLE_MCFG);
    ACPI_MCFG_ALLOCATION    *SubTable;


    /* Main table */

    AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoMcfg);

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_MCFG_ALLOCATION, Table, Offset);
    while (Offset < Table->Length)
    {
        if (Offset + sizeof (ACPI_MCFG_ALLOCATION) > Table->Length)
        {
            AcpiOsPrintf ("Warning: there are %d invalid trailing bytes\n",
                sizeof (ACPI_MCFG_ALLOCATION) - (Offset - Table->Length));
            return;
        }

        AcpiOsPrintf ("\n");
        AcpiDmDumpTable (Table->Length, Offset, SubTable, 0, AcpiDmTableInfoMcfg0);

        /* Point to next sub-table (each subtable is of fixed length) */

        Offset += sizeof (ACPI_MCFG_ALLOCATION);
        SubTable = ACPI_ADD_PTR (ACPI_MCFG_ALLOCATION, SubTable,
                        sizeof (ACPI_MCFG_ALLOCATION));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpSlit
 *
 * PARAMETERS:  Table               - An SLIT
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a SLIT
 *
 ******************************************************************************/

void
AcpiDmDumpSlit (
    ACPI_TABLE_HEADER       *Table)
{
    UINT32                  Offset;
    UINT8                   *Row;
    UINT32                  Localities;
    UINT32                  i;
    UINT32                  j;


    /* Main table */

    AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoSlit);

    /* Display the Locality NxN Matrix */

    Localities = (UINT32) ACPI_CAST_PTR (ACPI_TABLE_SLIT, Table)->LocalityCount;
    Offset = ACPI_OFFSET (ACPI_TABLE_SLIT, Entry[0]);
    Row = (UINT8 *) ACPI_CAST_PTR (ACPI_TABLE_SLIT, Table)->Entry;

    for (i = 0; i < Localities; i++)
    {
        /* Display one row of the matrix */

        AcpiDmLineHeader2 (Offset, Localities, "Locality", i);
        for  (j = 0; j < Localities; j++)
        {
            /* Check for beyond EOT */

            if (Offset >= Table->Length)
            {
                AcpiOsPrintf ("\n**** Not enough room in table for all localities\n");
                return;
            }

            AcpiOsPrintf ("%2.2X ", Row[j]);
            Offset++;

            /* Display up to 16 bytes per output row */

            if (j && (((j+1) % 16) == 0) && ((j+1) < Localities))
            {
                AcpiOsPrintf ("\n");
                AcpiDmLineHeader (Offset, 0, "");
            }
        }

        /* Point to next row */

        AcpiOsPrintf ("\n");
        Row += Localities;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpSrat
 *
 * PARAMETERS:  Table               - A SRAT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a SRAT
 *
 ******************************************************************************/

void
AcpiDmDumpSrat (
    ACPI_TABLE_HEADER       *Table)
{
    UINT32                  Offset = sizeof (ACPI_TABLE_SRAT);
    ACPI_SUBTABLE_HEADER    *SubTable;
    ACPI_DMTABLE_INFO       *InfoTable;


    /* Main table */

    AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoSrat);

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_SUBTABLE_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        switch (SubTable->Type)
        {
        case ACPI_SRAT_TYPE_CPU_AFFINITY:
            InfoTable = AcpiDmTableInfoSrat0;
            break;
        case ACPI_SRAT_TYPE_MEMORY_AFFINITY:
            InfoTable = AcpiDmTableInfoSrat1;
            break;
        default:
            AcpiOsPrintf ("\n**** Unknown SRAT sub-table type %X\n", SubTable->Type);
            return;
        }

        AcpiOsPrintf ("\n");
        AcpiDmDumpTable (Table->Length, Offset, SubTable, SubTable->Length, InfoTable);

        /* Point to next sub-table */

        Offset += SubTable->Length;
        SubTable = ACPI_ADD_PTR (ACPI_SUBTABLE_HEADER, SubTable, SubTable->Length);
    }
}

