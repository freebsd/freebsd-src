/******************************************************************************
 *
 * Module Name: dmtbdump - Dump ACPI data tables that contain no AML code
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

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acdisasm.h>
#include <contrib/dev/acpica/include/actables.h>

/* This module used for application-level code only */

#define _COMPONENT          ACPI_CA_DISASSEMBLER
        ACPI_MODULE_NAME    ("dmtbdump")


/* Local prototypes */

static void
AcpiDmValidateFadtLength (
    UINT32                  Revision,
    UINT32                  Length);

static void
AcpiDmDumpBuffer (
    void                    *Table,
    UINT32                  BufferOffset,
    UINT32                  Length,
    UINT32                  AbsoluteOffset,
    char                    *Header);


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpBuffer
 *
 * PARAMETERS:  Table               - ACPI Table or subtable
 *              BufferOffset        - Offset of buffer from Table above
 *              Length              - Length of the buffer
 *              AbsoluteOffset      - Offset of buffer in the main ACPI table
 *              Header              - Name of the buffer field (printed on the
 *                                    first line only.)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of an arbitrary length data buffer (in the
 *              disassembler output format.)
 *
 ******************************************************************************/

static void
AcpiDmDumpBuffer (
    void                    *Table,
    UINT32                  BufferOffset,
    UINT32                  Length,
    UINT32                  AbsoluteOffset,
    char                    *Header)
{
    UINT8                   *Buffer;
    UINT32                  i;


    if (!Length)
    {
        return;
    }

    Buffer = ACPI_CAST_PTR (UINT8, Table) + BufferOffset;
    i = 0;

    while (i < Length)
    {
        if (!(i % 16))
        {
            AcpiOsPrintf ("\n");
            AcpiDmLineHeader (AbsoluteOffset,
                ((Length - i) > 16) ? 16 : (Length - i), Header);
            Header = NULL;
        }

        AcpiOsPrintf ("%.02X ", *Buffer);
        i++;
        Buffer++;
        AbsoluteOffset++;
    }

    AcpiOsPrintf ("\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpRsdp
 *
 * PARAMETERS:  Table               - A RSDP
 *
 * RETURN:      Length of the table (there is not always a length field,
 *              use revision or length if available (ACPI 2.0+))
 *
 * DESCRIPTION: Format the contents of a RSDP
 *
 ******************************************************************************/

UINT32
AcpiDmDumpRsdp (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_TABLE_RSDP         *Rsdp = ACPI_CAST_PTR (ACPI_TABLE_RSDP, Table);
    UINT32                  Length = sizeof (ACPI_RSDP_COMMON);
    UINT8                   Checksum;


    /* Dump the common ACPI 1.0 portion */

    AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoRsdp1);

    /* Validate the first checksum */

    Checksum = AcpiDmGenerateChecksum (Rsdp, sizeof (ACPI_RSDP_COMMON),
                Rsdp->Checksum);
    if (Checksum != Rsdp->Checksum)
    {
        AcpiOsPrintf ("/* Incorrect Checksum above, should be 0x%2.2X */\n",
            Checksum);
    }

    /* The RSDP for ACPI 2.0+ contains more data and has a Length field */

    if (Rsdp->Revision > 0)
    {
        Length = Rsdp->Length;
        AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoRsdp2);

        /* Validate the extended checksum over entire RSDP */

        Checksum = AcpiDmGenerateChecksum (Rsdp, sizeof (ACPI_TABLE_RSDP),
                    Rsdp->ExtendedChecksum);
        if (Checksum != Rsdp->ExtendedChecksum)
        {
            AcpiOsPrintf (
                "/* Incorrect Extended Checksum above, should be 0x%2.2X */\n",
                Checksum);
        }
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
 * NOTE:        We cannot depend on the FADT version to indicate the actual
 *              contents of the FADT because of BIOS bugs. The table length
 *              is the only reliable indicator.
 *
 ******************************************************************************/

void
AcpiDmDumpFadt (
    ACPI_TABLE_HEADER       *Table)
{

    /* Always dump the minimum FADT revision 1 fields (ACPI 1.0) */

    AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoFadt1);

    /* Check for FADT revision 2 fields (ACPI 1.0B MS extensions) */

    if ((Table->Length > ACPI_FADT_V1_SIZE) &&
        (Table->Length <= ACPI_FADT_V2_SIZE))
    {
        AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoFadt2);
    }

    /* Check for FADT revision 3/4 fields and up (ACPI 2.0+ extended data) */

    else if (Table->Length > ACPI_FADT_V2_SIZE)
    {
        AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoFadt3);

        /* Check for FADT revision 5 fields and up (ACPI 5.0+) */

        if (Table->Length > ACPI_FADT_V3_SIZE)
        {
            AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoFadt5);
        }
    }

    /* Validate various fields in the FADT, including length */

    AcpiTbCreateLocalFadt (Table, Table->Length);

    /* Validate FADT length against the revision */

    AcpiDmValidateFadtLength (Table->Revision, Table->Length);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmValidateFadtLength
 *
 * PARAMETERS:  Revision            - FADT revision (Header->Revision)
 *              Length              - FADT length (Header->Length
 *
 * RETURN:      None
 *
 * DESCRIPTION: Check the FADT revision against the expected table length for
 *              that revision. Issue a warning if the length is not what was
 *              expected. This seems to be such a common BIOS bug that the
 *              FADT revision has been rendered virtually meaningless.
 *
 ******************************************************************************/

static void
AcpiDmValidateFadtLength (
    UINT32                  Revision,
    UINT32                  Length)
{
    UINT32                  ExpectedLength;


    switch (Revision)
    {
    case 0:

        AcpiOsPrintf ("// ACPI Warning: Invalid FADT revision: 0\n");
        return;

    case 1:

        ExpectedLength = ACPI_FADT_V1_SIZE;
        break;

    case 2:

        ExpectedLength = ACPI_FADT_V2_SIZE;
        break;

    case 3:
    case 4:

        ExpectedLength = ACPI_FADT_V3_SIZE;
        break;

    case 5:

        ExpectedLength = ACPI_FADT_V5_SIZE;
        break;

    default:

        return;
    }

    if (Length == ExpectedLength)
    {
        return;
    }

    AcpiOsPrintf (
        "\n// ACPI Warning: FADT revision %X does not match length: found %X expected %X\n",
        Revision, Length, ExpectedLength);
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
    ACPI_STATUS             Status;
    UINT32                  Offset = sizeof (ACPI_TABLE_HEADER);
    ACPI_ASF_INFO           *SubTable;
    ACPI_DMTABLE_INFO       *InfoTable;
    ACPI_DMTABLE_INFO       *DataInfoTable = NULL;
    UINT8                   *DataTable = NULL;
    UINT32                  DataCount = 0;
    UINT32                  DataLength = 0;
    UINT32                  DataOffset = 0;
    UINT32                  i;
    UINT8                   Type;


    /* No main table, only sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_ASF_INFO, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common sub-table header */

        Status = AcpiDmDumpTable (Table->Length, Offset, SubTable,
                    SubTable->Header.Length, AcpiDmTableInfoAsfHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* The actual type is the lower 7 bits of Type */

        Type = (UINT8) (SubTable->Header.Type & 0x7F);

        switch (Type)
        {
        case ACPI_ASF_TYPE_INFO:

            InfoTable = AcpiDmTableInfoAsf0;
            break;

        case ACPI_ASF_TYPE_ALERT:

            InfoTable = AcpiDmTableInfoAsf1;
            DataInfoTable = AcpiDmTableInfoAsf1a;
            DataTable = ACPI_ADD_PTR (UINT8, SubTable, sizeof (ACPI_ASF_ALERT));
            DataCount = ACPI_CAST_PTR (ACPI_ASF_ALERT, SubTable)->Alerts;
            DataLength = ACPI_CAST_PTR (ACPI_ASF_ALERT, SubTable)->DataLength;
            DataOffset = Offset + sizeof (ACPI_ASF_ALERT);
            break;

        case ACPI_ASF_TYPE_CONTROL:

            InfoTable = AcpiDmTableInfoAsf2;
            DataInfoTable = AcpiDmTableInfoAsf2a;
            DataTable = ACPI_ADD_PTR (UINT8, SubTable, sizeof (ACPI_ASF_REMOTE));
            DataCount = ACPI_CAST_PTR (ACPI_ASF_REMOTE, SubTable)->Controls;
            DataLength = ACPI_CAST_PTR (ACPI_ASF_REMOTE, SubTable)->DataLength;
            DataOffset = Offset + sizeof (ACPI_ASF_REMOTE);
            break;

        case ACPI_ASF_TYPE_BOOT:

            InfoTable = AcpiDmTableInfoAsf3;
            break;

        case ACPI_ASF_TYPE_ADDRESS:

            InfoTable = AcpiDmTableInfoAsf4;
            DataTable = ACPI_ADD_PTR (UINT8, SubTable, sizeof (ACPI_ASF_ADDRESS));
            DataLength = ACPI_CAST_PTR (ACPI_ASF_ADDRESS, SubTable)->Devices;
            DataOffset = Offset + sizeof (ACPI_ASF_ADDRESS);
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown ASF sub-table type 0x%X\n", SubTable->Header.Type);
            return;
        }

        Status = AcpiDmDumpTable (Table->Length, Offset, SubTable,
                    SubTable->Header.Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Dump variable-length extra data */

        switch (Type)
        {
        case ACPI_ASF_TYPE_ALERT:
        case ACPI_ASF_TYPE_CONTROL:

            for (i = 0; i < DataCount; i++)
            {
                AcpiOsPrintf ("\n");
                Status = AcpiDmDumpTable (Table->Length, DataOffset,
                            DataTable, DataLength, DataInfoTable);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

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
                if (DataOffset > Table->Length)
                {
                    AcpiOsPrintf ("**** ACPI table terminates in the middle of a data structure!\n");
                    return;
                }
            }

            AcpiOsPrintf ("\n");
            break;

        default:

            break;
        }

        AcpiOsPrintf ("\n");

        /* Point to next sub-table */

        if (!SubTable->Header.Length)
        {
            AcpiOsPrintf ("Invalid zero subtable header length\n");
            return;
        }

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
    ACPI_STATUS             Status;
    ACPI_CPEP_POLLING       *SubTable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_CPEP);


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoCpep);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_CPEP_POLLING, Table, Offset);
    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, SubTable,
                    SubTable->Header.Length, AcpiDmTableInfoCpep0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next sub-table */

        Offset += SubTable->Header.Length;
        SubTable = ACPI_ADD_PTR (ACPI_CPEP_POLLING, SubTable,
                    SubTable->Header.Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpCsrt
 *
 * PARAMETERS:  Table               - A CSRT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a CSRT. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpCsrt (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_CSRT_GROUP         *SubTable;
    ACPI_CSRT_SHARED_INFO   *SharedInfoTable;
    ACPI_CSRT_DESCRIPTOR    *SubSubTable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_CSRT);
    UINT32                  SubOffset;
    UINT32                  SubSubOffset;
    UINT32                  InfoLength;


    /* The main table only contains the ACPI header, thus already handled */

    /* Sub-tables (Resource Groups) */

    SubTable = ACPI_ADD_PTR (ACPI_CSRT_GROUP, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Resource group subtable */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, SubTable,
                    SubTable->Length, AcpiDmTableInfoCsrt0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Shared info subtable (One per resource group) */

        SubOffset = sizeof (ACPI_CSRT_GROUP);
        SharedInfoTable = ACPI_ADD_PTR (ACPI_CSRT_SHARED_INFO, Table,
            Offset + SubOffset);

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset + SubOffset, SharedInfoTable,
                    sizeof (ACPI_CSRT_SHARED_INFO), AcpiDmTableInfoCsrt1);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        SubOffset += SubTable->SharedInfoLength;

        /* Sub-Subtables (Resource Descriptors) */

        SubSubTable = ACPI_ADD_PTR (ACPI_CSRT_DESCRIPTOR, Table,
            Offset + SubOffset);

        while ((SubOffset < SubTable->Length) &&
              ((Offset + SubOffset) < Table->Length))
        {
            AcpiOsPrintf ("\n");
            Status = AcpiDmDumpTable (Length, Offset + SubOffset, SubSubTable,
                        SubSubTable->Length, AcpiDmTableInfoCsrt2);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            SubSubOffset = sizeof (ACPI_CSRT_DESCRIPTOR);

            /* Resource-specific info buffer */

            InfoLength = SubSubTable->Length - SubSubOffset;

            AcpiDmDumpBuffer (SubSubTable, SubSubOffset, InfoLength,
                Offset + SubOffset + SubSubOffset, "ResourceInfo");
            SubSubOffset += InfoLength;

            /* Point to next sub-subtable */

            SubOffset += SubSubTable->Length;
            SubSubTable = ACPI_ADD_PTR (ACPI_CSRT_DESCRIPTOR, SubSubTable,
                        SubSubTable->Length);
        }

        /* Point to next sub-table */

        Offset += SubTable->Length;
        SubTable = ACPI_ADD_PTR (ACPI_CSRT_GROUP, SubTable,
                    SubTable->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpDbg2
 *
 * PARAMETERS:  Table               - A DBG2 table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a DBG2. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpDbg2 (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_DBG2_DEVICE        *SubTable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_DBG2);
    UINT32                  i;
    UINT32                  ArrayOffset;
    UINT32                  AbsoluteOffset;
    UINT8                   *Array;


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoDbg2);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_DBG2_DEVICE, Table, Offset);
    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, SubTable,
                    SubTable->Length, AcpiDmTableInfoDbg2Device);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Dump the BaseAddress array */

        for (i = 0; i < SubTable->RegisterCount; i++)
        {
            ArrayOffset = SubTable->BaseAddressOffset +
                (sizeof (ACPI_GENERIC_ADDRESS) * i);
            AbsoluteOffset = Offset + ArrayOffset;
            Array = (UINT8 *) SubTable + ArrayOffset;

            Status = AcpiDmDumpTable (Length, AbsoluteOffset, Array,
                        SubTable->Length, AcpiDmTableInfoDbg2Addr);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
        }

        /* Dump the AddressSize array */

        for (i = 0; i < SubTable->RegisterCount; i++)
        {
            ArrayOffset = SubTable->AddressSizeOffset +
                (sizeof (UINT32) * i);
            AbsoluteOffset = Offset + ArrayOffset;
            Array = (UINT8 *) SubTable + ArrayOffset;

            Status = AcpiDmDumpTable (Length, AbsoluteOffset, Array,
                        SubTable->Length, AcpiDmTableInfoDbg2Size);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
        }

        /* Dump the Namestring (required) */

        AcpiOsPrintf ("\n");
        ArrayOffset = SubTable->NamepathOffset;
        AbsoluteOffset = Offset + ArrayOffset;
        Array = (UINT8 *) SubTable + ArrayOffset;

        Status = AcpiDmDumpTable (Length, AbsoluteOffset, Array,
                    SubTable->Length, AcpiDmTableInfoDbg2Name);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Dump the OemData (optional) */

        AcpiDmDumpBuffer (SubTable, SubTable->OemDataOffset, SubTable->OemDataLength,
            Offset + SubTable->OemDataOffset, "OEM Data");

        /* Point to next sub-table */

        Offset += SubTable->Length;
        SubTable = ACPI_ADD_PTR (ACPI_DBG2_DEVICE, SubTable,
                    SubTable->Length);
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
    ACPI_STATUS             Status;
    ACPI_DMAR_HEADER        *SubTable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_DMAR);
    ACPI_DMTABLE_INFO       *InfoTable;
    ACPI_DMAR_DEVICE_SCOPE  *ScopeTable;
    UINT32                  ScopeOffset;
    UINT8                   *PciPath;
    UINT32                  PathOffset;


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoDmar);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_DMAR_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common sub-table header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, SubTable,
                    SubTable->Length, AcpiDmTableInfoDmarHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }
        AcpiOsPrintf ("\n");

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

        case ACPI_DMAR_TYPE_ATSR:

            InfoTable = AcpiDmTableInfoDmar2;
            ScopeOffset = sizeof (ACPI_DMAR_ATSR);
            break;

        case ACPI_DMAR_HARDWARE_AFFINITY:

            InfoTable = AcpiDmTableInfoDmar3;
            ScopeOffset = sizeof (ACPI_DMAR_RHSA);
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown DMAR sub-table type 0x%X\n\n", SubTable->Type);
            return;
        }

        Status = AcpiDmDumpTable (Length, Offset, SubTable,
                    SubTable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Dump the device scope entries (if any) */

        ScopeTable = ACPI_ADD_PTR (ACPI_DMAR_DEVICE_SCOPE, SubTable, ScopeOffset);
        while (ScopeOffset < SubTable->Length)
        {
            AcpiOsPrintf ("\n");
            Status = AcpiDmDumpTable (Length, Offset + ScopeOffset, ScopeTable,
                        ScopeTable->Length, AcpiDmTableInfoDmarScope);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
            AcpiOsPrintf ("\n");

            /* Dump the PCI Path entries for this device scope */

            PathOffset = sizeof (ACPI_DMAR_DEVICE_SCOPE); /* Path entries start at this offset */

            PciPath = ACPI_ADD_PTR (UINT8, ScopeTable,
                sizeof (ACPI_DMAR_DEVICE_SCOPE));

            while (PathOffset < ScopeTable->Length)
            {
                AcpiDmLineHeader ((PathOffset + ScopeOffset + Offset), 2, "PCI Path");
                AcpiOsPrintf ("%2.2X,%2.2X\n", PciPath[0], PciPath[1]);

                /* Point to next PCI Path entry */

                PathOffset += 2;
                PciPath += 2;
                AcpiOsPrintf ("\n");
            }

            /* Point to next device scope entry */

            ScopeOffset += ScopeTable->Length;
            ScopeTable = ACPI_ADD_PTR (ACPI_DMAR_DEVICE_SCOPE,
                ScopeTable, ScopeTable->Length);
        }

        /* Point to next sub-table */

        Offset += SubTable->Length;
        SubTable = ACPI_ADD_PTR (ACPI_DMAR_HEADER, SubTable, SubTable->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpEinj
 *
 * PARAMETERS:  Table               - A EINJ table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a EINJ. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpEinj (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_WHEA_HEADER        *SubTable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_EINJ);


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoEinj);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_WHEA_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, SubTable,
                    sizeof (ACPI_WHEA_HEADER), AcpiDmTableInfoEinj0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next sub-table (each subtable is of fixed length) */

        Offset += sizeof (ACPI_WHEA_HEADER);
        SubTable = ACPI_ADD_PTR (ACPI_WHEA_HEADER, SubTable,
                        sizeof (ACPI_WHEA_HEADER));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpErst
 *
 * PARAMETERS:  Table               - A ERST table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a ERST. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpErst (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_WHEA_HEADER        *SubTable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_ERST);


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoErst);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_WHEA_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, SubTable,
                    sizeof (ACPI_WHEA_HEADER), AcpiDmTableInfoErst0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next sub-table (each subtable is of fixed length) */

        Offset += sizeof (ACPI_WHEA_HEADER);
        SubTable = ACPI_ADD_PTR (ACPI_WHEA_HEADER, SubTable,
                        sizeof (ACPI_WHEA_HEADER));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpFpdt
 *
 * PARAMETERS:  Table               - A FPDT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a FPDT. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpFpdt (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_FPDT_HEADER        *SubTable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_FPDT);
    ACPI_DMTABLE_INFO       *InfoTable;


    /* There is no main table (other than the standard ACPI header) */

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_FPDT_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common sub-table header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, SubTable,
                    SubTable->Length, AcpiDmTableInfoFpdtHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        switch (SubTable->Type)
        {
        case ACPI_FPDT_TYPE_BOOT:

            InfoTable = AcpiDmTableInfoFpdt0;
            break;

        case ACPI_FPDT_TYPE_S3PERF:

            InfoTable = AcpiDmTableInfoFpdt1;
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown FPDT sub-table type 0x%X\n\n", SubTable->Type);

            /* Attempt to continue */

            if (!SubTable->Length)
            {
                AcpiOsPrintf ("Invalid zero length subtable\n");
                return;
            }
            goto NextSubTable;
        }

        Status = AcpiDmDumpTable (Length, Offset, SubTable,
                    SubTable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

NextSubTable:
        /* Point to next sub-table */

        Offset += SubTable->Length;
        SubTable = ACPI_ADD_PTR (ACPI_FPDT_HEADER, SubTable, SubTable->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpHest
 *
 * PARAMETERS:  Table               - A HEST table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a HEST. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpHest (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_HEST_HEADER        *SubTable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_HEST);
    ACPI_DMTABLE_INFO       *InfoTable;
    UINT32                  SubTableLength;
    UINT32                  BankCount;
    ACPI_HEST_IA_ERROR_BANK *BankTable;


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoHest);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_HEST_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        BankCount = 0;
        switch (SubTable->Type)
        {
        case ACPI_HEST_TYPE_IA32_CHECK:

            InfoTable = AcpiDmTableInfoHest0;
            SubTableLength = sizeof (ACPI_HEST_IA_MACHINE_CHECK);
            BankCount = (ACPI_CAST_PTR (ACPI_HEST_IA_MACHINE_CHECK,
                            SubTable))->NumHardwareBanks;
            break;

        case ACPI_HEST_TYPE_IA32_CORRECTED_CHECK:

            InfoTable = AcpiDmTableInfoHest1;
            SubTableLength = sizeof (ACPI_HEST_IA_CORRECTED);
            BankCount = (ACPI_CAST_PTR (ACPI_HEST_IA_CORRECTED,
                            SubTable))->NumHardwareBanks;
            break;

        case ACPI_HEST_TYPE_IA32_NMI:

            InfoTable = AcpiDmTableInfoHest2;
            SubTableLength = sizeof (ACPI_HEST_IA_NMI);
            break;

        case ACPI_HEST_TYPE_AER_ROOT_PORT:

            InfoTable = AcpiDmTableInfoHest6;
            SubTableLength = sizeof (ACPI_HEST_AER_ROOT);
            break;

        case ACPI_HEST_TYPE_AER_ENDPOINT:

            InfoTable = AcpiDmTableInfoHest7;
            SubTableLength = sizeof (ACPI_HEST_AER);
            break;

        case ACPI_HEST_TYPE_AER_BRIDGE:

            InfoTable = AcpiDmTableInfoHest8;
            SubTableLength = sizeof (ACPI_HEST_AER_BRIDGE);
            break;

        case ACPI_HEST_TYPE_GENERIC_ERROR:

            InfoTable = AcpiDmTableInfoHest9;
            SubTableLength = sizeof (ACPI_HEST_GENERIC);
            break;

        default:

            /* Cannot continue on unknown type - no length */

            AcpiOsPrintf ("\n**** Unknown HEST sub-table type 0x%X\n", SubTable->Type);
            return;
        }

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, SubTable,
                    SubTableLength, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to end of current subtable (each subtable above is of fixed length) */

        Offset += SubTableLength;

        /* If there are any (fixed-length) Error Banks from above, dump them now */

        if (BankCount)
        {
            BankTable = ACPI_ADD_PTR (ACPI_HEST_IA_ERROR_BANK, SubTable, SubTableLength);
            SubTableLength += BankCount * sizeof (ACPI_HEST_IA_ERROR_BANK);

            while (BankCount)
            {
                AcpiOsPrintf ("\n");
                Status = AcpiDmDumpTable (Length, Offset, BankTable,
                            sizeof (ACPI_HEST_IA_ERROR_BANK), AcpiDmTableInfoHestBank);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
                Offset += sizeof (ACPI_HEST_IA_ERROR_BANK);
                BankTable++;
                BankCount--;
            }
        }

        /* Point to next sub-table */

        SubTable = ACPI_ADD_PTR (ACPI_HEST_HEADER, SubTable, SubTableLength);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpIvrs
 *
 * PARAMETERS:  Table               - A IVRS table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a IVRS
 *
 ******************************************************************************/

static UINT8 EntrySizes[] = {4,8,16,32};

void
AcpiDmDumpIvrs (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    UINT32                  Offset = sizeof (ACPI_TABLE_IVRS);
    UINT32                  EntryOffset;
    UINT32                  EntryLength;
    UINT32                  EntryType;
    ACPI_IVRS_DE_HEADER     *DeviceEntry;
    ACPI_IVRS_HEADER        *SubTable;
    ACPI_DMTABLE_INFO       *InfoTable;


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoIvrs);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_IVRS_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common sub-table header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, SubTable,
                    SubTable->Length, AcpiDmTableInfoIvrsHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        switch (SubTable->Type)
        {
        case ACPI_IVRS_TYPE_HARDWARE:

            InfoTable = AcpiDmTableInfoIvrs0;
            break;

        case ACPI_IVRS_TYPE_MEMORY1:
        case ACPI_IVRS_TYPE_MEMORY2:
        case ACPI_IVRS_TYPE_MEMORY3:

            InfoTable = AcpiDmTableInfoIvrs1;
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown IVRS sub-table type 0x%X\n",
                SubTable->Type);

            /* Attempt to continue */

            if (!SubTable->Length)
            {
                AcpiOsPrintf ("Invalid zero length subtable\n");
                return;
            }
            goto NextSubTable;
        }

        /* Dump the subtable */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, SubTable,
                    SubTable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* The hardware subtable can contain multiple device entries */

        if (SubTable->Type == ACPI_IVRS_TYPE_HARDWARE)
        {
            EntryOffset = Offset + sizeof (ACPI_IVRS_HARDWARE);
            DeviceEntry = ACPI_ADD_PTR (ACPI_IVRS_DE_HEADER, SubTable,
                            sizeof (ACPI_IVRS_HARDWARE));

            while (EntryOffset < (Offset + SubTable->Length))
            {
                AcpiOsPrintf ("\n");
                /*
                 * Upper 2 bits of Type encode the length of the device entry
                 *
                 * 00 = 4 byte
                 * 01 = 8 byte
                 * 10 = 16 byte - currently no entries defined
                 * 11 = 32 byte - currently no entries defined
                 */
                EntryType = DeviceEntry->Type;
                EntryLength = EntrySizes [EntryType >> 6];

                switch (EntryType)
                {
                /* 4-byte device entries */

                case ACPI_IVRS_TYPE_PAD4:
                case ACPI_IVRS_TYPE_ALL:
                case ACPI_IVRS_TYPE_SELECT:
                case ACPI_IVRS_TYPE_START:
                case ACPI_IVRS_TYPE_END:

                    InfoTable = AcpiDmTableInfoIvrs4;
                    break;

                /* 8-byte entries, type A */

                case ACPI_IVRS_TYPE_ALIAS_SELECT:
                case ACPI_IVRS_TYPE_ALIAS_START:

                    InfoTable = AcpiDmTableInfoIvrs8a;
                    break;

                /* 8-byte entries, type B */

                case ACPI_IVRS_TYPE_PAD8:
                case ACPI_IVRS_TYPE_EXT_SELECT:
                case ACPI_IVRS_TYPE_EXT_START:

                    InfoTable = AcpiDmTableInfoIvrs8b;
                    break;

                /* 8-byte entries, type C */

                case ACPI_IVRS_TYPE_SPECIAL:

                    InfoTable = AcpiDmTableInfoIvrs8c;
                    break;

                default:
                    InfoTable = AcpiDmTableInfoIvrs4;
                    AcpiOsPrintf (
                        "\n**** Unknown IVRS device entry type/length: "
                        "0x%.2X/0x%X at offset 0x%.4X: (header below)\n",
                        EntryType, EntryLength, EntryOffset);
                    break;
                }

                /* Dump the Device Entry */

                Status = AcpiDmDumpTable (Table->Length, EntryOffset,
                            DeviceEntry, EntryLength, InfoTable);

                EntryOffset += EntryLength;
                DeviceEntry = ACPI_ADD_PTR (ACPI_IVRS_DE_HEADER, DeviceEntry,
                                EntryLength);
            }
        }

NextSubTable:
        /* Point to next sub-table */

        Offset += SubTable->Length;
        SubTable = ACPI_ADD_PTR (ACPI_IVRS_HEADER, SubTable, SubTable->Length);
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
    ACPI_STATUS             Status;
    ACPI_SUBTABLE_HEADER    *SubTable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_MADT);
    ACPI_DMTABLE_INFO       *InfoTable;


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoMadt);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_SUBTABLE_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common sub-table header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, SubTable,
                    SubTable->Length, AcpiDmTableInfoMadtHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

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

        case ACPI_MADT_TYPE_LOCAL_X2APIC:

            InfoTable = AcpiDmTableInfoMadt9;
            break;

        case ACPI_MADT_TYPE_LOCAL_X2APIC_NMI:

            InfoTable = AcpiDmTableInfoMadt10;
            break;

        case ACPI_MADT_TYPE_GENERIC_INTERRUPT:

            InfoTable = AcpiDmTableInfoMadt11;
            break;

        case ACPI_MADT_TYPE_GENERIC_DISTRIBUTOR:

            InfoTable = AcpiDmTableInfoMadt12;
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown MADT sub-table type 0x%X\n\n", SubTable->Type);

            /* Attempt to continue */

            if (!SubTable->Length)
            {
                AcpiOsPrintf ("Invalid zero length subtable\n");
                return;
            }
            goto NextSubTable;
        }

        Status = AcpiDmDumpTable (Length, Offset, SubTable,
                    SubTable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

NextSubTable:
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
    ACPI_STATUS             Status;
    UINT32                  Offset = sizeof (ACPI_TABLE_MCFG);
    ACPI_MCFG_ALLOCATION    *SubTable;


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoMcfg);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_MCFG_ALLOCATION, Table, Offset);
    while (Offset < Table->Length)
    {
        if (Offset + sizeof (ACPI_MCFG_ALLOCATION) > Table->Length)
        {
            AcpiOsPrintf ("Warning: there are %u invalid trailing bytes\n",
                sizeof (ACPI_MCFG_ALLOCATION) - (Offset - Table->Length));
            return;
        }

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, SubTable,
                    sizeof (ACPI_MCFG_ALLOCATION), AcpiDmTableInfoMcfg0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next sub-table (each subtable is of fixed length) */

        Offset += sizeof (ACPI_MCFG_ALLOCATION);
        SubTable = ACPI_ADD_PTR (ACPI_MCFG_ALLOCATION, SubTable,
                        sizeof (ACPI_MCFG_ALLOCATION));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpMpst
 *
 * PARAMETERS:  Table               - A MPST Table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a MPST table
 *
 ******************************************************************************/

void
AcpiDmDumpMpst (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    UINT32                  Offset = sizeof (ACPI_TABLE_MPST);
    ACPI_MPST_POWER_NODE    *SubTable0;
    ACPI_MPST_POWER_STATE   *SubTable0A;
    ACPI_MPST_COMPONENT     *SubTable0B;
    ACPI_MPST_DATA_HDR      *SubTable1;
    ACPI_MPST_POWER_DATA    *SubTable2;
    UINT16                  SubtableCount;
    UINT32                  PowerStateCount;
    UINT32                  ComponentCount;


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoMpst);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtable: Memory Power Node(s) */

    SubtableCount = (ACPI_CAST_PTR (ACPI_TABLE_MPST, Table))->PowerNodeCount;
    SubTable0 = ACPI_ADD_PTR (ACPI_MPST_POWER_NODE, Table, Offset);

    while ((Offset < Table->Length) && SubtableCount)
    {
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, SubTable0,
                    sizeof (ACPI_MPST_POWER_NODE), AcpiDmTableInfoMpst0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Extract the sub-subtable counts */

        PowerStateCount = SubTable0->NumPowerStates;
        ComponentCount = SubTable0->NumPhysicalComponents;
        Offset += sizeof (ACPI_MPST_POWER_NODE);

        /* Sub-subtables - Memory Power State Structure(s) */

        SubTable0A = ACPI_ADD_PTR (ACPI_MPST_POWER_STATE, SubTable0,
            sizeof (ACPI_MPST_POWER_NODE));

        while (PowerStateCount)
        {
            AcpiOsPrintf ("\n");
            Status = AcpiDmDumpTable (Table->Length, Offset, SubTable0A,
                        sizeof (ACPI_MPST_POWER_STATE), AcpiDmTableInfoMpst0A);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            SubTable0A++;
            PowerStateCount--;
            Offset += sizeof (ACPI_MPST_POWER_STATE);
       }

        /* Sub-subtables - Physical Component ID Structure(s) */

        SubTable0B = ACPI_CAST_PTR (ACPI_MPST_COMPONENT, SubTable0A);

        if (ComponentCount)
        {
            AcpiOsPrintf ("\n");
        }

        while (ComponentCount)
        {
            Status = AcpiDmDumpTable (Table->Length, Offset, SubTable0B,
                        sizeof (ACPI_MPST_COMPONENT), AcpiDmTableInfoMpst0B);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            SubTable0B++;
            ComponentCount--;
            Offset += sizeof (ACPI_MPST_COMPONENT);
        }

        /* Point to next Memory Power Node subtable */

        SubtableCount--;
        SubTable0 = ACPI_ADD_PTR (ACPI_MPST_POWER_NODE, SubTable0,
            sizeof (ACPI_MPST_POWER_NODE) +
            (sizeof (ACPI_MPST_POWER_STATE) * SubTable0->NumPowerStates) +
            (sizeof (ACPI_MPST_COMPONENT) * SubTable0->NumPhysicalComponents));
    }

    /* Subtable: Count of Memory Power State Characteristic structures */

    AcpiOsPrintf ("\n");
    SubTable1 = ACPI_CAST_PTR (ACPI_MPST_DATA_HDR, SubTable0);
    Status = AcpiDmDumpTable (Table->Length, Offset, SubTable1,
                sizeof (ACPI_MPST_DATA_HDR), AcpiDmTableInfoMpst1);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    SubtableCount = SubTable1->CharacteristicsCount;
    Offset += sizeof (ACPI_MPST_DATA_HDR);

    /* Subtable: Memory Power State Characteristics structure(s) */

    SubTable2 = ACPI_ADD_PTR (ACPI_MPST_POWER_DATA, SubTable1, sizeof (ACPI_MPST_DATA_HDR));

    while ((Offset < Table->Length) && SubtableCount)
    {
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, SubTable2,
                    sizeof (ACPI_MPST_POWER_DATA), AcpiDmTableInfoMpst2);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        SubTable2++;
        SubtableCount--;
        Offset += sizeof (ACPI_MPST_POWER_DATA);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpMsct
 *
 * PARAMETERS:  Table               - A MSCT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a MSCT
 *
 ******************************************************************************/

void
AcpiDmDumpMsct (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    UINT32                  Offset = sizeof (ACPI_TABLE_MSCT);
    ACPI_MSCT_PROXIMITY     *SubTable;


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoMsct);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_MSCT_PROXIMITY, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common sub-table header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, SubTable,
                    sizeof (ACPI_MSCT_PROXIMITY), AcpiDmTableInfoMsct0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next sub-table */

        Offset += sizeof (ACPI_MSCT_PROXIMITY);
        SubTable = ACPI_ADD_PTR (ACPI_MSCT_PROXIMITY, SubTable, sizeof (ACPI_MSCT_PROXIMITY));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpMtmr
 *
 * PARAMETERS:  Table               - A MTMR table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a MTMR
 *
 ******************************************************************************/

void
AcpiDmDumpMtmr (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    UINT32                  Offset = sizeof (ACPI_TABLE_MTMR);
    ACPI_MTMR_ENTRY         *SubTable;


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoMtmr);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_MTMR_ENTRY, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common sub-table header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, SubTable,
                    sizeof (ACPI_MTMR_ENTRY), AcpiDmTableInfoMtmr0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next sub-table */

        Offset += sizeof (ACPI_MTMR_ENTRY);
        SubTable = ACPI_ADD_PTR (ACPI_MTMR_ENTRY, SubTable, sizeof (ACPI_MTMR_ENTRY));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpPcct
 *
 * PARAMETERS:  Table               - A PCCT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a PCCT. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpPcct (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_PCCT_SUBSPACE      *SubTable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_PCCT);


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoPcct);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_PCCT_SUBSPACE, Table, Offset);
    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, SubTable,
                    SubTable->Header.Length, AcpiDmTableInfoPcct0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next sub-table */

        Offset += SubTable->Header.Length;
        SubTable = ACPI_ADD_PTR (ACPI_PCCT_SUBSPACE, SubTable,
                    SubTable->Header.Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpPmtt
 *
 * PARAMETERS:  Table               - A PMTT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a PMTT. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpPmtt (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_PMTT_HEADER        *SubTable;
    ACPI_PMTT_HEADER        *MemSubTable;
    ACPI_PMTT_HEADER        *DimmSubTable;
    ACPI_PMTT_DOMAIN        *DomainArray;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_PMTT);
    UINT32                  MemOffset;
    UINT32                  DimmOffset;
    UINT32                  DomainOffset;
    UINT32                  DomainCount;


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoPmtt);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    SubTable = ACPI_ADD_PTR (ACPI_PMTT_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common subtable header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, SubTable,
                    SubTable->Length, AcpiDmTableInfoPmttHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Only Socket subtables are expected at this level */

        if (SubTable->Type != ACPI_PMTT_TYPE_SOCKET)
        {
            AcpiOsPrintf (
                "\n**** Unexpected or unknown PMTT subtable type 0x%X\n\n",
                SubTable->Type);
            return;
        }

        /* Dump the fixed-length portion of the subtable */

        Status = AcpiDmDumpTable (Length, Offset, SubTable,
                    SubTable->Length, AcpiDmTableInfoPmtt0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Walk the memory controller subtables */

        MemOffset = sizeof (ACPI_PMTT_SOCKET);
        MemSubTable = ACPI_ADD_PTR (ACPI_PMTT_HEADER, SubTable,
            sizeof (ACPI_PMTT_SOCKET));

        while (((Offset + MemOffset) < Table->Length) &&
            (MemOffset < SubTable->Length))
        {
            /* Common subtable header */

            AcpiOsPrintf ("\n");
            Status = AcpiDmDumpTable (Length,
                        Offset + MemOffset, MemSubTable,
                        MemSubTable->Length, AcpiDmTableInfoPmttHdr);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            /* Only memory controller subtables are expected at this level */

            if (MemSubTable->Type != ACPI_PMTT_TYPE_CONTROLLER)
            {
                AcpiOsPrintf (
                    "\n**** Unexpected or unknown PMTT subtable type 0x%X\n\n",
                    MemSubTable->Type);
                return;
            }

            /* Dump the fixed-length portion of the controller subtable */

            Status = AcpiDmDumpTable (Length,
                        Offset + MemOffset, MemSubTable,
                        MemSubTable->Length, AcpiDmTableInfoPmtt1);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            /* Walk the variable count of proximity domains */

            DomainCount = ((ACPI_PMTT_CONTROLLER *) MemSubTable)->DomainCount;
            DomainOffset = sizeof (ACPI_PMTT_CONTROLLER);
            DomainArray = ACPI_ADD_PTR (ACPI_PMTT_DOMAIN, MemSubTable,
                sizeof (ACPI_PMTT_CONTROLLER));

            while (((Offset + MemOffset + DomainOffset) < Table->Length) &&
                ((MemOffset + DomainOffset) < SubTable->Length) &&
                DomainCount)
            {
                Status = AcpiDmDumpTable (Length,
                            Offset + MemOffset + DomainOffset, DomainArray,
                            sizeof (ACPI_PMTT_DOMAIN), AcpiDmTableInfoPmtt1a);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                DomainOffset += sizeof (ACPI_PMTT_DOMAIN);
                DomainArray++;
                DomainCount--;
            }

            if (DomainCount)
            {
                AcpiOsPrintf (
                    "\n**** DomainCount exceeds subtable length\n\n",
                    MemSubTable->Type);
            }

            /* Walk the physical component (DIMM) subtables */

            DimmOffset = DomainOffset;
            DimmSubTable = ACPI_ADD_PTR (ACPI_PMTT_HEADER, MemSubTable,
                DomainOffset);

            while (((Offset + MemOffset + DimmOffset) < Table->Length) &&
                (DimmOffset < MemSubTable->Length))
            {
                /* Common subtable header */

                AcpiOsPrintf ("\n");
                Status = AcpiDmDumpTable (Length,
                            Offset + MemOffset + DimmOffset, DimmSubTable,
                            DimmSubTable->Length, AcpiDmTableInfoPmttHdr);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                /* Only DIMM subtables are expected at this level */

                if (DimmSubTable->Type != ACPI_PMTT_TYPE_DIMM)
                {
                    AcpiOsPrintf (
                        "\n**** Unexpected or unknown PMTT subtable type 0x%X\n\n",
                        DimmSubTable->Type);
                    return;
                }

                /* Dump the fixed-length DIMM subtable */

                Status = AcpiDmDumpTable (Length,
                            Offset + MemOffset + DimmOffset, DimmSubTable,
                            DimmSubTable->Length, AcpiDmTableInfoPmtt2);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                /* Point to next DIMM subtable */

                DimmOffset += DimmSubTable->Length;
                DimmSubTable = ACPI_ADD_PTR (ACPI_PMTT_HEADER,
                    DimmSubTable, DimmSubTable->Length);
            }

            /* Point to next Controller subtable */

            MemOffset += MemSubTable->Length;
            MemSubTable = ACPI_ADD_PTR (ACPI_PMTT_HEADER,
                MemSubTable, MemSubTable->Length);
        }

        /* Point to next Socket subtable */

        Offset += SubTable->Length;
        SubTable = ACPI_ADD_PTR (ACPI_PMTT_HEADER,
            SubTable, SubTable->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpS3pt
 *
 * PARAMETERS:  Table               - A S3PT table
 *
 * RETURN:      Length of the table
 *
 * DESCRIPTION: Format the contents of a S3PT
 *
 ******************************************************************************/

UINT32
AcpiDmDumpS3pt (
    ACPI_TABLE_HEADER       *Tables)
{
    ACPI_STATUS             Status;
    UINT32                  Offset = sizeof (ACPI_TABLE_S3PT);
    ACPI_S3PT_HEADER        *SubTable;
    ACPI_DMTABLE_INFO       *InfoTable;
    ACPI_TABLE_S3PT         *S3ptTable = ACPI_CAST_PTR (ACPI_TABLE_S3PT, Tables);


    /* Main table */

    Status = AcpiDmDumpTable (Offset, 0, S3ptTable, 0, AcpiDmTableInfoS3pt);
    if (ACPI_FAILURE (Status))
    {
        return 0;
    }

    SubTable = ACPI_ADD_PTR (ACPI_S3PT_HEADER, S3ptTable, Offset);
    while (Offset < S3ptTable->Length)
    {
        /* Common sub-table header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (S3ptTable->Length, Offset, SubTable,
                    SubTable->Length, AcpiDmTableInfoS3ptHdr);
        if (ACPI_FAILURE (Status))
        {
            return 0;
        }

        switch (SubTable->Type)
        {
        case ACPI_S3PT_TYPE_RESUME:

            InfoTable = AcpiDmTableInfoS3pt0;
            break;

        case ACPI_S3PT_TYPE_SUSPEND:

            InfoTable = AcpiDmTableInfoS3pt1;
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown S3PT sub-table type 0x%X\n", SubTable->Type);

            /* Attempt to continue */

            if (!SubTable->Length)
            {
                AcpiOsPrintf ("Invalid zero length subtable\n");
                return 0;
            }
            goto NextSubTable;
        }

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (S3ptTable->Length, Offset, SubTable,
                    SubTable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return 0;
        }

NextSubTable:
        /* Point to next sub-table */

        Offset += SubTable->Length;
        SubTable = ACPI_ADD_PTR (ACPI_S3PT_HEADER, SubTable, SubTable->Length);
    }

    return (S3ptTable->Length);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpSlic
 *
 * PARAMETERS:  Table               - A SLIC table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a SLIC
 *
 ******************************************************************************/

void
AcpiDmDumpSlic (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    UINT32                  Offset = sizeof (ACPI_TABLE_SLIC);
    ACPI_SLIC_HEADER        *SubTable;
    ACPI_DMTABLE_INFO       *InfoTable;


    /* There is no main SLIC table, only subtables */

    SubTable = ACPI_ADD_PTR (ACPI_SLIC_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common sub-table header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, SubTable,
                    SubTable->Length, AcpiDmTableInfoSlicHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        switch (SubTable->Type)
        {
        case ACPI_SLIC_TYPE_PUBLIC_KEY:

            InfoTable = AcpiDmTableInfoSlic0;
            break;

        case ACPI_SLIC_TYPE_WINDOWS_MARKER:

            InfoTable = AcpiDmTableInfoSlic1;
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown SLIC sub-table type 0x%X\n", SubTable->Type);

            /* Attempt to continue */

            if (!SubTable->Length)
            {
                AcpiOsPrintf ("Invalid zero length subtable\n");
                return;
            }
            goto NextSubTable;
        }

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, SubTable,
                    SubTable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

NextSubTable:
        /* Point to next sub-table */

        Offset += SubTable->Length;
        SubTable = ACPI_ADD_PTR (ACPI_SLIC_HEADER, SubTable, SubTable->Length);
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
    ACPI_STATUS             Status;
    UINT32                  Offset;
    UINT8                   *Row;
    UINT32                  Localities;
    UINT32                  i;
    UINT32                  j;


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoSlit);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

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

            AcpiOsPrintf ("%2.2X", Row[j]);
            Offset++;

            /* Display up to 16 bytes per output row */

            if ((j+1) < Localities)
            {
                AcpiOsPrintf (" ");

                if (j && (((j+1) % 16) == 0))
                {
                    AcpiOsPrintf ("\\\n"); /* With line continuation char */
                    AcpiDmLineHeader (Offset, 0, NULL);
                }
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
    ACPI_STATUS             Status;
    UINT32                  Offset = sizeof (ACPI_TABLE_SRAT);
    ACPI_SUBTABLE_HEADER    *SubTable;
    ACPI_DMTABLE_INFO       *InfoTable;


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoSrat);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_SUBTABLE_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common sub-table header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, SubTable,
                    SubTable->Length, AcpiDmTableInfoSratHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        switch (SubTable->Type)
        {
        case ACPI_SRAT_TYPE_CPU_AFFINITY:

            InfoTable = AcpiDmTableInfoSrat0;
            break;

        case ACPI_SRAT_TYPE_MEMORY_AFFINITY:

            InfoTable = AcpiDmTableInfoSrat1;
            break;

        case ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY:

            InfoTable = AcpiDmTableInfoSrat2;
            break;

        default:
            AcpiOsPrintf ("\n**** Unknown SRAT sub-table type 0x%X\n", SubTable->Type);

            /* Attempt to continue */

            if (!SubTable->Length)
            {
                AcpiOsPrintf ("Invalid zero length subtable\n");
                return;
            }
            goto NextSubTable;
        }

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, SubTable,
                    SubTable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

NextSubTable:
        /* Point to next sub-table */

        Offset += SubTable->Length;
        SubTable = ACPI_ADD_PTR (ACPI_SUBTABLE_HEADER, SubTable, SubTable->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpVrtc
 *
 * PARAMETERS:  Table               - A VRTC table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a VRTC
 *
 ******************************************************************************/

void
AcpiDmDumpVrtc (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    UINT32                  Offset = sizeof (ACPI_TABLE_VRTC);
    ACPI_VRTC_ENTRY         *SubTable;


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoVrtc);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_VRTC_ENTRY, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common sub-table header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, SubTable,
                    sizeof (ACPI_VRTC_ENTRY), AcpiDmTableInfoVrtc0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next sub-table */

        Offset += sizeof (ACPI_VRTC_ENTRY);
        SubTable = ACPI_ADD_PTR (ACPI_VRTC_ENTRY, SubTable, sizeof (ACPI_VRTC_ENTRY));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpWdat
 *
 * PARAMETERS:  Table               - A WDAT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a WDAT
 *
 ******************************************************************************/

void
AcpiDmDumpWdat (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    UINT32                  Offset = sizeof (ACPI_TABLE_WDAT);
    ACPI_WDAT_ENTRY         *SubTable;


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoWdat);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Sub-tables */

    SubTable = ACPI_ADD_PTR (ACPI_WDAT_ENTRY, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common sub-table header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, SubTable,
                    sizeof (ACPI_WDAT_ENTRY), AcpiDmTableInfoWdat0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next sub-table */

        Offset += sizeof (ACPI_WDAT_ENTRY);
        SubTable = ACPI_ADD_PTR (ACPI_WDAT_ENTRY, SubTable, sizeof (ACPI_WDAT_ENTRY));
    }
}
