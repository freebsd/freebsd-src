/******************************************************************************
 *
 * Module Name: dmtbdump1 - Dump ACPI data tables that contain no AML code
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2025, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
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
 * to or modifications of the Original Intel Code. No other license or right
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
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
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
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
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
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acdisasm.h>
#include <contrib/dev/acpica/include/actables.h>
#include <contrib/dev/acpica/compiler/aslcompiler.h>

/* This module used for application-level code only */

#define _COMPONENT          ACPI_CA_DISASSEMBLER
        ACPI_MODULE_NAME    ("dmtbdump1")


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpAest
 *
 * PARAMETERS:  Table               - A AEST table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a AEST table
 *
 * NOTE: Assumes the following table structure:
 *      For all AEST Error Nodes:
 *          1) An AEST Error Node, followed immediately by:
 *          2) Any node-specific data
 *          3) An Interface Structure (one)
 *          4) A list (array) of Interrupt Structures
 *
 * AEST - ARM Error Source table. Conforms to:
 * ACPI for the Armv8 RAS Extensions 1.1 Platform Design Document Sep 2020
 *
 ******************************************************************************/

void
AcpiDmDumpAest (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    UINT32                  Offset = sizeof (ACPI_TABLE_HEADER);
    ACPI_AEST_HEADER        *Subtable;
    ACPI_AEST_HEADER        *NodeHeader;
    ACPI_AEST_PROCESSOR     *ProcessorSubtable;
    ACPI_DMTABLE_INFO       *InfoTable;
    ACPI_SIZE               Length;
    UINT8                   Type;
    UINT8                   Revision = Table->Revision;
    UINT32                  Count;
    ACPI_AEST_NODE_INTERFACE_HEADER *InterfaceHeader;


    /* Very small, generic main table. AEST consists of mostly subtables */

    while (Offset < Table->Length)
    {
        NodeHeader = ACPI_ADD_PTR (ACPI_AEST_HEADER, Table, Offset);

        /* Dump the common error node (subtable) header */

        Status = AcpiDmDumpTable (Table->Length, Offset, NodeHeader,
            NodeHeader->Length, AcpiDmTableInfoAestHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        Type = NodeHeader->Type;

        /* Setup the node-specific subtable based on the header Type field */

        switch (Type)
        {
        case ACPI_AEST_PROCESSOR_ERROR_NODE:
            InfoTable = AcpiDmTableInfoAestProcError;
            Length = sizeof (ACPI_AEST_PROCESSOR);
            break;

        case ACPI_AEST_MEMORY_ERROR_NODE:
            InfoTable = AcpiDmTableInfoAestMemError;
            Length = sizeof (ACPI_AEST_MEMORY);
            break;

        case ACPI_AEST_SMMU_ERROR_NODE:
            InfoTable = AcpiDmTableInfoAestSmmuError;
            Length = sizeof (ACPI_AEST_SMMU);
            break;

        case ACPI_AEST_VENDOR_ERROR_NODE:
            switch (Revision)
            {
            case 1:
                InfoTable = AcpiDmTableInfoAestVendorError;
                Length = sizeof (ACPI_AEST_VENDOR);
                break;

            case 2:
                InfoTable = AcpiDmTableInfoAestVendorV2Error;
                Length = sizeof (ACPI_AEST_VENDOR_V2);
                break;

            default:
                AcpiOsPrintf ("\n**** Unknown AEST revision 0x%X\n", Revision);
                return;
            }
            break;

        case ACPI_AEST_GIC_ERROR_NODE:
            InfoTable = AcpiDmTableInfoAestGicError;
            Length = sizeof (ACPI_AEST_GIC);
            break;

        case ACPI_AEST_PCIE_ERROR_NODE:
            InfoTable = AcpiDmTableInfoAestPCIeError;
            Length = sizeof (ACPI_AEST_PCIE);
            break;

        case ACPI_AEST_PROXY_ERROR_NODE:
            InfoTable = AcpiDmTableInfoAestProxyError;
            Length = sizeof (ACPI_AEST_PROXY);
            break;

        /* Error case below */
        default:

            AcpiOsPrintf ("\n**** Unknown AEST Error Subtable type 0x%X\n",
                Type);
            return;
        }

        /* Point past the common header (to the node-specific data) */

        Offset += sizeof (ACPI_AEST_HEADER);
        Subtable = ACPI_ADD_PTR (ACPI_AEST_HEADER, Table, Offset);
        AcpiOsPrintf ("\n");

        /* Dump the node-specific subtable */

        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable, Length,
            InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }
        AcpiOsPrintf ("\n");

        if (Type == ACPI_AEST_PROCESSOR_ERROR_NODE)
        {
            /*
             * Special handling for PROCESSOR_ERROR_NODE subtables
             * (to handle the Resource Substructure via the ResourceType
             * field).
             */

            /* Point past the node-specific data */

            Offset += Length;
            ProcessorSubtable = ACPI_CAST_PTR (ACPI_AEST_PROCESSOR, Subtable);

            switch (ProcessorSubtable->ResourceType)
            {
            /* Setup the Resource Substructure subtable */

            case ACPI_AEST_CACHE_RESOURCE:
                InfoTable = AcpiDmTableInfoAestCacheRsrc;
                Length = sizeof (ACPI_AEST_PROCESSOR_CACHE);
                break;

            case ACPI_AEST_TLB_RESOURCE:
                InfoTable = AcpiDmTableInfoAestTlbRsrc;
                Length = sizeof (ACPI_AEST_PROCESSOR_TLB);
                break;

            case ACPI_AEST_GENERIC_RESOURCE:
                InfoTable = AcpiDmTableInfoAestGenRsrc;
                Length = sizeof (ACPI_AEST_PROCESSOR_GENERIC);
                break;

            /* Error case below */
            default:
                AcpiOsPrintf ("\n**** Unknown AEST Processor Resource type 0x%X\n",
                    ProcessorSubtable->ResourceType);
                return;
            }

            ProcessorSubtable = ACPI_ADD_PTR (ACPI_AEST_PROCESSOR, Table,
                Offset);

            /* Dump the resource substructure subtable */

            Status = AcpiDmDumpTable (Table->Length, Offset, ProcessorSubtable,
                Length, InfoTable);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            AcpiOsPrintf ("\n");
        }

        /* Point past the resource substructure or the node-specific data */

        Offset += Length;

        /* Dump the interface structure, required to be present */

        Subtable = ACPI_ADD_PTR (ACPI_AEST_HEADER, Table, Offset);
        if (Subtable->Type >= ACPI_AEST_XFACE_RESERVED)
        {
            AcpiOsPrintf ("\n**** Unknown AEST Node Interface type 0x%X\n",
                Subtable->Type);
            return;
        }

        if (Revision == 1)
        {
            InfoTable = AcpiDmTableInfoAestXface;
            Length = sizeof (ACPI_AEST_NODE_INTERFACE);
        }
        else if (Revision == 2)
        {
            InfoTable = AcpiDmTableInfoAestXfaceHeader;
            Length = sizeof (ACPI_AEST_NODE_INTERFACE_HEADER);

            Status = AcpiDmDumpTable (Table->Length, Offset, Subtable, Length, InfoTable);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            Offset += Length;

            InterfaceHeader = ACPI_CAST_PTR (ACPI_AEST_NODE_INTERFACE_HEADER, Subtable);
            switch (InterfaceHeader->GroupFormat)
	        {
            case ACPI_AEST_NODE_GROUP_FORMAT_4K:
                InfoTable = AcpiDmTableInfoAestXface4k;
                Length = sizeof (ACPI_AEST_NODE_INTERFACE_4K);
                break;

            case ACPI_AEST_NODE_GROUP_FORMAT_16K:
                InfoTable = AcpiDmTableInfoAestXface16k;
                Length = sizeof (ACPI_AEST_NODE_INTERFACE_16K);
                break;

            case ACPI_AEST_NODE_GROUP_FORMAT_64K:
                InfoTable = AcpiDmTableInfoAestXface64k;
                Length = sizeof (ACPI_AEST_NODE_INTERFACE_64K);
                break;

            default:
                AcpiOsPrintf ("\n**** Unknown AEST Interface Group Format 0x%X\n",
                    InterfaceHeader->GroupFormat);
                return;
            }

            Subtable = ACPI_ADD_PTR (ACPI_AEST_HEADER, Table, Offset);
        }
        else
        {
            AcpiOsPrintf ("\n**** Unknown AEST revision 0x%X\n", Revision);
            return;
        }

        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable, Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point past the interface structure */

        AcpiOsPrintf ("\n");
        Offset += Length;

        /* Dump the entire interrupt structure array, if present */

        if (NodeHeader->NodeInterruptOffset)
        {
            Count = NodeHeader->NodeInterruptCount;
            Subtable = ACPI_ADD_PTR (ACPI_AEST_HEADER, Table, Offset);

            while (Count)
            {
                /* Dump the interrupt structure */

                switch (Revision) {
                case 1:
                    InfoTable = AcpiDmTableInfoAestXrupt;
                    Length = sizeof (ACPI_AEST_NODE_INTERRUPT);
                    break;

                case 2:
                    InfoTable = AcpiDmTableInfoAestXruptV2;
                    Length = sizeof (ACPI_AEST_NODE_INTERRUPT_V2);
                    break;
                default:
                    AcpiOsPrintf ("\n**** Unknown AEST revision 0x%X\n",
                        Revision);
                    return;
                }
                Status = AcpiDmDumpTable (Table->Length, Offset, Subtable,
                    Length, InfoTable);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                /* Point to the next interrupt structure */

                Offset += Length;
                Subtable = ACPI_ADD_PTR (ACPI_AEST_HEADER, Table, Offset);
                Count--;
                AcpiOsPrintf ("\n");
            }
        }
    }
}

/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpApmt
 *
 * PARAMETERS:  Table               - A APMT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a APMT. This table type consists
 *              of an open-ended number of subtables.
 *
 *
 * APMT - ARM Performance Monitoring Unit table. Conforms to:
 * ARM Performance Monitoring Unit Architecture 1.0 Platform Design Document
 * ARM DEN0117 v1.0 November 25, 2021
 *
 ******************************************************************************/

void
AcpiDmDumpApmt (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS              Status;
    ACPI_APMT_NODE           *Subtable;
    UINT32                   Length = Table->Length;
    UINT32                   Offset = sizeof (ACPI_TABLE_APMT);
    UINT32                   NodeNum = 0;

    /* There is no main table (other than the standard ACPI header) */

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_APMT_NODE, Table, Offset);
    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");

        if (Subtable->Type >= ACPI_APMT_NODE_TYPE_COUNT)
        {
            AcpiOsPrintf ("\n**** Unknown APMT subtable type 0x%X\n",
                Subtable->Type);
            return;
        }

        AcpiOsPrintf ("/* APMT Node-%u */\n", NodeNum++);

        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoApmtNode);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next subtable */

        Offset += Subtable->Length;
        Subtable = ACPI_ADD_PTR (ACPI_APMT_NODE, Subtable,
            Subtable->Length);
        AcpiOsPrintf ("\n");
    }
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
    ACPI_ASF_INFO           *Subtable;
    ACPI_DMTABLE_INFO       *InfoTable;
    ACPI_DMTABLE_INFO       *DataInfoTable = NULL;
    UINT8                   *DataTable = NULL;
    UINT32                  DataCount = 0;
    UINT32                  DataLength = 0;
    UINT32                  DataOffset = 0;
    UINT32                  i;
    UINT8                   Type;


    /* No main table, only subtables */

    Subtable = ACPI_ADD_PTR (ACPI_ASF_INFO, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common subtable header */

        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable,
            Subtable->Header.Length, AcpiDmTableInfoAsfHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* The actual type is the lower 7 bits of Type */

        Type = (UINT8) (Subtable->Header.Type & 0x7F);

        switch (Type)
        {
        case ACPI_ASF_TYPE_INFO:

            InfoTable = AcpiDmTableInfoAsf0;
            break;

        case ACPI_ASF_TYPE_ALERT:

            InfoTable = AcpiDmTableInfoAsf1;
            DataInfoTable = AcpiDmTableInfoAsf1a;
            DataTable = ACPI_ADD_PTR (UINT8, Subtable, sizeof (ACPI_ASF_ALERT));
            DataCount = ACPI_CAST_PTR (ACPI_ASF_ALERT, Subtable)->Alerts;
            DataLength = ACPI_CAST_PTR (ACPI_ASF_ALERT, Subtable)->DataLength;
            DataOffset = Offset + sizeof (ACPI_ASF_ALERT);
            break;

        case ACPI_ASF_TYPE_CONTROL:

            InfoTable = AcpiDmTableInfoAsf2;
            DataInfoTable = AcpiDmTableInfoAsf2a;
            DataTable = ACPI_ADD_PTR (UINT8, Subtable, sizeof (ACPI_ASF_REMOTE));
            DataCount = ACPI_CAST_PTR (ACPI_ASF_REMOTE, Subtable)->Controls;
            DataLength = ACPI_CAST_PTR (ACPI_ASF_REMOTE, Subtable)->DataLength;
            DataOffset = Offset + sizeof (ACPI_ASF_REMOTE);
            break;

        case ACPI_ASF_TYPE_BOOT:

            InfoTable = AcpiDmTableInfoAsf3;
            break;

        case ACPI_ASF_TYPE_ADDRESS:

            InfoTable = AcpiDmTableInfoAsf4;
            DataTable = ACPI_ADD_PTR (UINT8, Subtable, sizeof (ACPI_ASF_ADDRESS));
            DataLength = ACPI_CAST_PTR (ACPI_ASF_ADDRESS, Subtable)->Devices;
            DataOffset = Offset + sizeof (ACPI_ASF_ADDRESS);
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown ASF subtable type 0x%X\n",
                Subtable->Header.Type);
            return;
        }

        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable,
            Subtable->Header.Length, InfoTable);
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
                    AcpiOsPrintf (
                        "**** ACPI table terminates in the middle of a "
                        "data structure! (ASF! table)\n");
                    return;
                }
            }

            AcpiOsPrintf ("\n");
            break;

        default:

            break;
        }

        AcpiOsPrintf ("\n");

        /* Point to next subtable */

        if (!Subtable->Header.Length)
        {
            AcpiOsPrintf ("Invalid zero subtable header length\n");
            return;
        }

        Offset += Subtable->Header.Length;
        Subtable = ACPI_ADD_PTR (ACPI_ASF_INFO, Subtable,
            Subtable->Header.Length);
    }
}

/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpAspt
 *
 * PARAMETERS:  Table               - A ASPT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a ASPT table
 *
 ******************************************************************************/

void
AcpiDmDumpAspt (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    UINT32                  Offset = sizeof (ACPI_TABLE_ASPT);
    UINT32                  Length = Table->Length;
    ACPI_ASPT_HEADER        *Subtable;
    ACPI_DMTABLE_INFO       *InfoTable;
    UINT16                   Type;

    /* Main table */
    Status = AcpiDmDumpTable(Length, 0, Table, 0, AcpiDmTableInfoAspt);

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_ASPT_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");

        /* Common subtable header */
        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoAsptHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        Type = Subtable->Type;

        switch (Type)
        {
        case ACPI_ASPT_TYPE_GLOBAL_REGS:

            InfoTable = AcpiDmTableInfoAspt0;
            break;

        case ACPI_ASPT_TYPE_SEV_MBOX_REGS:

            InfoTable = AcpiDmTableInfoAspt1;
            break;

        case ACPI_ASPT_TYPE_ACPI_MBOX_REGS:

            InfoTable = AcpiDmTableInfoAspt2;
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown ASPT subtable type 0x%X\n",
                Subtable->Type);
            return;
        }

        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable,
            Subtable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        AcpiOsPrintf ("\n");

        /* Point to next subtable */
        if (!Subtable->Length)
        {
            AcpiOsPrintf ("Invalid zero subtable header length\n");
            return;
        }

        Offset += Subtable->Length;
        Subtable = ACPI_ADD_PTR (ACPI_ASPT_HEADER, Subtable,
            Subtable->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpCdat
 *
 * PARAMETERS:  InTable             - A CDAT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a CDAT. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpCdat (
    ACPI_TABLE_HEADER       *InTable)
{
    ACPI_TABLE_CDAT         *Table = ACPI_CAST_PTR (ACPI_TABLE_CDAT, InTable);
    ACPI_STATUS             Status;
    ACPI_CDAT_HEADER        *Subtable;
    ACPI_TABLE_CDAT         *CdatTable = ACPI_CAST_PTR (ACPI_TABLE_CDAT, Table);
    ACPI_DMTABLE_INFO       *InfoTable;
    UINT32                  Length = CdatTable->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_CDAT);
    UINT32                  SubtableLength;
    UINT32                  SubtableType;
    INT32                   EntriesLength;


    /* Main table */

    Status = AcpiDmDumpTable (Offset, 0, CdatTable, 0,
        AcpiDmTableInfoCdatTableHdr);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    Subtable = ACPI_ADD_PTR (ACPI_CDAT_HEADER, Table, sizeof (ACPI_TABLE_CDAT));
    while (Offset < Table->Length)
    {
        /* Dump the common subtable header */

        DbgPrint (ASL_DEBUG_OUTPUT, "0) HeaderOffset: %X\n", Offset);
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            sizeof (ACPI_CDAT_HEADER), AcpiDmTableInfoCdatHeader);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point past the common subtable header, decode the subtable type */

        Offset += sizeof (ACPI_CDAT_HEADER);
        SubtableType = Subtable->Type;

        switch (Subtable->Type)
        {
        case ACPI_CDAT_TYPE_DSMAS:
            Subtable = ACPI_ADD_PTR (ACPI_CDAT_HEADER, Table, Offset);
            SubtableLength = sizeof (ACPI_CDAT_DSMAS);

            InfoTable = AcpiDmTableInfoCdat0;
            break;

        case ACPI_CDAT_TYPE_DSLBIS:
            Subtable = ACPI_ADD_PTR (ACPI_CDAT_HEADER, Table, Offset);
            SubtableLength = sizeof (ACPI_CDAT_DSLBIS);
            DbgPrint (ASL_DEBUG_OUTPUT, "1) Offset: %X\n", Offset);

            InfoTable = AcpiDmTableInfoCdat1;
            break;

        case ACPI_CDAT_TYPE_DSMSCIS:
            Subtable = ACPI_ADD_PTR (ACPI_CDAT_HEADER, Table, Offset);
            SubtableLength = sizeof (ACPI_CDAT_DSMSCIS);

            InfoTable = AcpiDmTableInfoCdat2;
            break;

        case ACPI_CDAT_TYPE_DSIS:
            DbgPrint (ASL_DEBUG_OUTPUT, "2) Offset: %X ", Offset);
            SubtableLength = sizeof (ACPI_CDAT_DSIS);
            DbgPrint (ASL_DEBUG_OUTPUT, "1) input pointer: %p\n", Table);
            Subtable = ACPI_ADD_PTR (ACPI_CDAT_HEADER, Table, Offset);
            DbgPrint (ASL_DEBUG_OUTPUT, "1) output pointers: %p, %p, Offset: %X\n",
                Table, Subtable, Offset);
            DbgPrint (ASL_DEBUG_OUTPUT, "3) Offset: %X\n", Offset);

            InfoTable = AcpiDmTableInfoCdat3;
            break;

        case ACPI_CDAT_TYPE_DSEMTS:
            Subtable = ACPI_ADD_PTR (ACPI_CDAT_HEADER, Table, Offset);
            SubtableLength = sizeof (ACPI_CDAT_DSEMTS);

            InfoTable = AcpiDmTableInfoCdat4;
            break;

        case ACPI_CDAT_TYPE_SSLBIS:
            SubtableLength = Subtable->Length;

            InfoTable = AcpiDmTableInfoCdat5;
            Subtable = ACPI_ADD_PTR (ACPI_CDAT_HEADER, Table, Offset);
            break;

        default:
            fprintf (stderr, "ERROR: Unknown SubtableType: %X\n", Subtable->Type);
            return;
        }

        DbgPrint (ASL_DEBUG_OUTPUT, "SubtableType: %X, Length: %X Actual "
            "Length: %X Offset: %X tableptr: %p\n", SubtableType,
            Subtable->Length, SubtableLength, Offset, Table);

        /*
         * Do the subtable-specific fields
         */
        Status = AcpiDmDumpTable (Length, Offset, Subtable, Offset, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        DbgPrint (ASL_DEBUG_OUTPUT, "Subtable Type: %X, Offset: %X, SubtableLength: %X\n",
            SubtableType, Offset, SubtableLength);

        /* Additional sub-subtables, dependent on the main subtable type */

        switch (SubtableType)
        {
        case ACPI_CDAT_TYPE_SSLBIS:
            Offset += sizeof (ACPI_CDAT_SSLBIS);
            Subtable = ACPI_ADD_PTR (ACPI_CDAT_HEADER, Table,
                Offset);

            DbgPrint (ASL_DEBUG_OUTPUT, "Case SSLBIS, Offset: %X, SubtableLength: %X "
                "Subtable->Length %X\n", Offset, SubtableLength, Subtable->Length);

            /* Generate the total length of all the SSLBE entries */

            EntriesLength = SubtableLength - sizeof (ACPI_CDAT_HEADER) -
                sizeof (ACPI_CDAT_SSLBIS);
            DbgPrint (ASL_DEBUG_OUTPUT, "EntriesLength: %X, Offset: %X, Table->Length: %X\n",
                EntriesLength, Offset, Table->Length);

            /* Do each of the SSLBE Entries */

            while ((EntriesLength > 0) && (Offset < Table->Length))
            {
                AcpiOsPrintf ("\n");

                Status = AcpiDmDumpTable (Length, Offset, Subtable, Offset,
                    AcpiDmTableInfoCdatEntries);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                EntriesLength -= sizeof (ACPI_CDAT_SSLBE);
                Offset += sizeof (ACPI_CDAT_SSLBE);
                Subtable = ACPI_ADD_PTR (ACPI_CDAT_HEADER, Table, Offset);
            }

            SubtableLength = 0;
            break;

        default:
            break;
        }

        DbgPrint (ASL_DEBUG_OUTPUT, "Offset: %X, Subtable Length: %X\n",
            Offset, SubtableLength);

        /* Point to next subtable */

        Offset += SubtableLength;
        Subtable = ACPI_ADD_PTR (ACPI_CDAT_HEADER, Table, Offset);
    }

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpCedt
 *
 * PARAMETERS:  Table               - A CEDT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a CEDT. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpCedt (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_CEDT_HEADER        *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_CEDT);


    /* There is no main table (other than the standard ACPI header) */

    Subtable = ACPI_ADD_PTR (ACPI_CEDT_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common subtable header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoCedtHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        switch (Subtable->Type)
        {
        case ACPI_CEDT_TYPE_CHBS:
            Status = AcpiDmDumpTable (Length, Offset, Subtable,
                Subtable->Length, AcpiDmTableInfoCedt0);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
            break;

        case ACPI_CEDT_TYPE_CFMWS:
        {
            ACPI_CEDT_CFMWS *ptr = (ACPI_CEDT_CFMWS *) Subtable;
            unsigned int i, max;

            if (ptr->InterleaveWays < 8)
                max = 1 << (ptr->InterleaveWays);
            else
                max = 3 << (ptr->InterleaveWays - 8);

	    /* print out table with first "Interleave target" */

            Status = AcpiDmDumpTable (Length, Offset, Subtable,
                Subtable->Length, AcpiDmTableInfoCedt1);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            /* Now, print out any interleave targets beyond the first. */

            for (i = 1; i < max; i++)
            {
                unsigned int loc_offset = Offset + (i * 4) + ACPI_OFFSET (ACPI_CEDT_CFMWS, InterleaveTargets);
                unsigned int *trg = &(ptr->InterleaveTargets[i]);

                Status = AcpiDmDumpTable (Length, loc_offset, trg,
                        Subtable->Length, AcpiDmTableInfoCedt1_te);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
            }
            break;
        }

        case ACPI_CEDT_TYPE_CXIMS:
        {
            ACPI_CEDT_CXIMS *ptr = (ACPI_CEDT_CXIMS *) Subtable;
            unsigned int i, max = ptr->NrXormaps;

            /* print out table with first "XOR Map" */

            Status = AcpiDmDumpTable (Length, Offset, Subtable,
                Subtable->Length, AcpiDmTableInfoCedt2);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            /* Now, print out any XOR Map beyond the first. */

            for (i = 1; i < max; i++)
            {
                unsigned int loc_offset = Offset + (i * 1) + ACPI_OFFSET (ACPI_CEDT_CXIMS, XormapList);
                UINT64 *trg = &(ptr->XormapList[i]);

                Status = AcpiDmDumpTable (Length, loc_offset, trg,
                        Subtable->Length, AcpiDmTableInfoCedt2_te);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
            }
            break;
        }

        default:
            AcpiOsPrintf ("\n**** Unknown CEDT subtable type 0x%X\n\n",
                Subtable->Type);

            /* Attempt to continue */
            if (!Subtable->Length)
            {
                AcpiOsPrintf ("Invalid zero length subtable\n");
                return;
            }
        }

        /* Point to next subtable */
        Offset += Subtable->Length;
        Subtable = ACPI_ADD_PTR (ACPI_CEDT_HEADER, Subtable,
            Subtable->Length);
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
    ACPI_CPEP_POLLING       *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_CPEP);


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoCpep);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_CPEP_POLLING, Table, Offset);
    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Header.Length, AcpiDmTableInfoCpep0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next subtable */

        Offset += Subtable->Header.Length;
        Subtable = ACPI_ADD_PTR (ACPI_CPEP_POLLING, Subtable,
            Subtable->Header.Length);
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
    ACPI_CSRT_GROUP         *Subtable;
    ACPI_CSRT_SHARED_INFO   *SharedInfoTable;
    ACPI_CSRT_DESCRIPTOR    *SubSubtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_CSRT);
    UINT32                  SubOffset;
    UINT32                  SubSubOffset;
    UINT32                  InfoLength;


    /* The main table only contains the ACPI header, thus already handled */

    /* Subtables (Resource Groups) */

    Subtable = ACPI_ADD_PTR (ACPI_CSRT_GROUP, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Resource group subtable */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoCsrt0);
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

        SubOffset += Subtable->SharedInfoLength;

        /* Sub-Subtables (Resource Descriptors) */

        SubSubtable = ACPI_ADD_PTR (ACPI_CSRT_DESCRIPTOR, Table,
            Offset + SubOffset);

        while ((SubOffset < Subtable->Length) &&
              ((Offset + SubOffset) < Table->Length))
        {
            AcpiOsPrintf ("\n");
            Status = AcpiDmDumpTable (Length, Offset + SubOffset, SubSubtable,
                SubSubtable->Length, AcpiDmTableInfoCsrt2);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            SubSubOffset = sizeof (ACPI_CSRT_DESCRIPTOR);

            /* Resource-specific info buffer */

            InfoLength = SubSubtable->Length - SubSubOffset;
            if (InfoLength)
            {
                Status = AcpiDmDumpTable (Length,
                    Offset + SubOffset + SubSubOffset, Table,
                    InfoLength, AcpiDmTableInfoCsrt2a);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
            }

            /* Point to next sub-subtable */

            SubOffset += SubSubtable->Length;
            SubSubtable = ACPI_ADD_PTR (ACPI_CSRT_DESCRIPTOR, SubSubtable,
                SubSubtable->Length);
        }

        /* Point to next subtable */

        Offset += Subtable->Length;
        Subtable = ACPI_ADD_PTR (ACPI_CSRT_GROUP, Subtable,
            Subtable->Length);
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
    ACPI_DBG2_DEVICE        *Subtable;
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

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_DBG2_DEVICE, Table, Offset);
    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoDbg2Device);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Dump the BaseAddress array */

        for (i = 0; i < Subtable->RegisterCount; i++)
        {
            ArrayOffset = Subtable->BaseAddressOffset +
                (sizeof (ACPI_GENERIC_ADDRESS) * i);
            AbsoluteOffset = Offset + ArrayOffset;
            Array = (UINT8 *) Subtable + ArrayOffset;

            Status = AcpiDmDumpTable (Length, AbsoluteOffset, Array,
                Subtable->Length, AcpiDmTableInfoDbg2Addr);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
        }

        /* Dump the AddressSize array */

        for (i = 0; i < Subtable->RegisterCount; i++)
        {
            ArrayOffset = Subtable->AddressSizeOffset +
                (sizeof (UINT32) * i);
            AbsoluteOffset = Offset + ArrayOffset;
            Array = (UINT8 *) Subtable + ArrayOffset;

            Status = AcpiDmDumpTable (Length, AbsoluteOffset, Array,
                Subtable->Length, AcpiDmTableInfoDbg2Size);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
        }

        /* Dump the Namestring (required) */

        AcpiOsPrintf ("\n");
        ArrayOffset = Subtable->NamepathOffset;
        AbsoluteOffset = Offset + ArrayOffset;
        Array = (UINT8 *) Subtable + ArrayOffset;

        Status = AcpiDmDumpTable (Length, AbsoluteOffset, Array,
            Subtable->Length, AcpiDmTableInfoDbg2Name);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Dump the OemData (optional) */

        if (Subtable->OemDataOffset)
        {
            Status = AcpiDmDumpTable (Length, Subtable->OemDataOffset,
                Subtable, Subtable->OemDataLength,
                AcpiDmTableInfoDbg2OemData);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
        }

        /* Point to next subtable */

        Offset += Subtable->Length;
        Subtable = ACPI_ADD_PTR (ACPI_DBG2_DEVICE, Subtable,
            Subtable->Length);
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
    ACPI_DMAR_HEADER        *Subtable;
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

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_DMAR_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common subtable header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoDmarHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        AcpiOsPrintf ("\n");

        switch (Subtable->Type)
        {
        case ACPI_DMAR_TYPE_HARDWARE_UNIT:

            InfoTable = AcpiDmTableInfoDmar0;
            ScopeOffset = sizeof (ACPI_DMAR_HARDWARE_UNIT);
            break;

        case ACPI_DMAR_TYPE_RESERVED_MEMORY:

            InfoTable = AcpiDmTableInfoDmar1;
            ScopeOffset = sizeof (ACPI_DMAR_RESERVED_MEMORY);
            break;

        case ACPI_DMAR_TYPE_ROOT_ATS:

            InfoTable = AcpiDmTableInfoDmar2;
            ScopeOffset = sizeof (ACPI_DMAR_ATSR);
            break;

        case ACPI_DMAR_TYPE_HARDWARE_AFFINITY:

            InfoTable = AcpiDmTableInfoDmar3;
            ScopeOffset = sizeof (ACPI_DMAR_RHSA);
            break;

        case ACPI_DMAR_TYPE_NAMESPACE:

            InfoTable = AcpiDmTableInfoDmar4;
            ScopeOffset = sizeof (ACPI_DMAR_ANDD);
            break;

        case ACPI_DMAR_TYPE_SATC:

            InfoTable = AcpiDmTableInfoDmar5;
            ScopeOffset = sizeof (ACPI_DMAR_SATC);
            break;

        case ACPI_DMAR_TYPE_SIDP:

            InfoTable = AcpiDmTableInfoDmar6;
            ScopeOffset = sizeof (ACPI_DMAR_SIDP);
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown DMAR subtable type 0x%X\n\n",
                Subtable->Type);
            return;
        }

        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /*
         * Dump the optional device scope entries
         */
        if ((Subtable->Type == ACPI_DMAR_TYPE_HARDWARE_AFFINITY) ||
            (Subtable->Type == ACPI_DMAR_TYPE_NAMESPACE))
        {
            /* These types do not support device scopes */

            goto NextSubtable;
        }

        ScopeTable = ACPI_ADD_PTR (ACPI_DMAR_DEVICE_SCOPE, Subtable, ScopeOffset);
        while (ScopeOffset < Subtable->Length)
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
                AcpiDmLineHeader ((PathOffset + ScopeOffset + Offset), 2,
                    "PCI Path");
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

NextSubtable:
        /* Point to next subtable */

        Offset += Subtable->Length;
        Subtable = ACPI_ADD_PTR (ACPI_DMAR_HEADER, Subtable,
            Subtable->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpDrtm
 *
 * PARAMETERS:  Table               - A DRTM table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a DRTM.
 *
 ******************************************************************************/

void
AcpiDmDumpDrtm (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    UINT32                  Offset;
    ACPI_DRTM_VTABLE_LIST   *DrtmVtl;
    ACPI_DRTM_RESOURCE_LIST *DrtmRl;
    ACPI_DRTM_DPS_ID        *DrtmDps;
    UINT32                  Count;


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0,
        AcpiDmTableInfoDrtm);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    Offset = sizeof (ACPI_TABLE_DRTM);

    /* Sub-tables */

    /* Dump ValidatedTable length */

    DrtmVtl = ACPI_ADD_PTR (ACPI_DRTM_VTABLE_LIST, Table, Offset);
    AcpiOsPrintf ("\n");
    Status = AcpiDmDumpTable (Table->Length, Offset,
        DrtmVtl, ACPI_OFFSET (ACPI_DRTM_VTABLE_LIST, ValidatedTables),
        AcpiDmTableInfoDrtm0);
    if (ACPI_FAILURE (Status))
    {
            return;
    }

    Offset += ACPI_OFFSET (ACPI_DRTM_VTABLE_LIST, ValidatedTables);

    /* Dump Validated table addresses */

    Count = 0;
    while ((Offset < Table->Length) &&
            (DrtmVtl->ValidatedTableCount > Count))
    {
        Status = AcpiDmDumpTable (Table->Length, Offset,
            ACPI_ADD_PTR (void, Table, Offset), sizeof (UINT64),
            AcpiDmTableInfoDrtm0a);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        Offset += sizeof (UINT64);
        Count++;
    }

    /* Dump ResourceList length */

    DrtmRl = ACPI_ADD_PTR (ACPI_DRTM_RESOURCE_LIST, Table, Offset);
    AcpiOsPrintf ("\n");
    Status = AcpiDmDumpTable (Table->Length, Offset,
        DrtmRl, ACPI_OFFSET (ACPI_DRTM_RESOURCE_LIST, Resources),
        AcpiDmTableInfoDrtm1);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    Offset += ACPI_OFFSET (ACPI_DRTM_RESOURCE_LIST, Resources);

    /* Dump the Resource List */

    Count = 0;
    while ((Offset < Table->Length) &&
           (DrtmRl->ResourceCount > Count))
    {
        Status = AcpiDmDumpTable (Table->Length, Offset,
            ACPI_ADD_PTR (void, Table, Offset),
            sizeof (ACPI_DRTM_RESOURCE), AcpiDmTableInfoDrtm1a);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        Offset += sizeof (ACPI_DRTM_RESOURCE);
        Count++;
    }

    /* Dump DPS */

    DrtmDps = ACPI_ADD_PTR (ACPI_DRTM_DPS_ID, Table, Offset);
    AcpiOsPrintf ("\n");
    (void) AcpiDmDumpTable (Table->Length, Offset,
        DrtmDps, sizeof (ACPI_DRTM_DPS_ID), AcpiDmTableInfoDrtm2);
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
    ACPI_WHEA_HEADER        *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_EINJ);


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoEinj);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_WHEA_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            sizeof (ACPI_WHEA_HEADER), AcpiDmTableInfoEinj0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next subtable (each subtable is of fixed length) */

        Offset += sizeof (ACPI_WHEA_HEADER);
        Subtable = ACPI_ADD_PTR (ACPI_WHEA_HEADER, Subtable,
            sizeof (ACPI_WHEA_HEADER));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpErdt
 *
 * PARAMETERS:  Table               - A ERDT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a ERDT. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpErdt (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_SUBTBL_HDR_16      *Subtable, *Subsubtable;
    ACPI_ERDT_DACD_PATHS    *ScopeTable;
    UINT32                  Offset = sizeof (ACPI_TABLE_ERDT);
    UINT32                  Suboffset;
    UINT32                  ScopeOffset;
    UINT32                  SubsubtableLength = 0;
    ACPI_DMTABLE_INFO       *InfoTable, *TrailEntries, *DacdEntries;
    UINT32                  NumTrailers = 0;

    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoErdt);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */
    Subtable = ACPI_ADD_PTR (ACPI_SUBTBL_HDR_16, Table, Offset);
    while (Offset < Table->Length)
    {

        /* Dump common header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoErdtHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoErdtRmdd);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Subtables of this RMDD table */

        Suboffset = Offset + sizeof(ACPI_ERDT_RMDD);
        Subsubtable = ACPI_ADD_PTR (ACPI_SUBTBL_HDR_16, Table, Suboffset);
        while (Suboffset < Offset + Subtable->Length)
        {
            AcpiOsPrintf ("\n");

            TrailEntries = NULL;
            DacdEntries = NULL;
            switch (Subsubtable->Type)
            {
            case ACPI_ERDT_TYPE_CACD:
                 InfoTable = AcpiDmTableInfoErdtCacd;
                 TrailEntries = AcpiDmTableInfoErdtCacdX2apic;
                 SubsubtableLength = sizeof(ACPI_ERDT_CACD);
                 break;

            case ACPI_ERDT_TYPE_DACD:
                 InfoTable = AcpiDmTableInfoErdtDacd;
                 DacdEntries = AcpiDmTableInfoErdtDacdScope;
                 SubsubtableLength = sizeof(ACPI_ERDT_DACD);
                 break;

            case ACPI_ERDT_TYPE_CMRC:
                 InfoTable = AcpiDmTableInfoErdtCmrc;
                 break;

            case ACPI_ERDT_TYPE_MMRC:
                 InfoTable = AcpiDmTableInfoErdtMmrc;
                 TrailEntries = AcpiDmTableInfoErdtMmrcCorrFactor;
                 SubsubtableLength = sizeof(ACPI_ERDT_MMRC);
                 break;

            case ACPI_ERDT_TYPE_MARC:
                 InfoTable = AcpiDmTableInfoErdtMarc;
                 break;

            case ACPI_ERDT_TYPE_CARC:
                 InfoTable = AcpiDmTableInfoErdtCarc;
                 break;

            case ACPI_ERDT_TYPE_CMRD:
                 InfoTable = AcpiDmTableInfoErdtCmrd;
                 break;

            case ACPI_ERDT_TYPE_IBRD:
                 InfoTable = AcpiDmTableInfoErdtIbrd;
                 TrailEntries = AcpiDmTableInfoErdtIbrdCorrFactor;
                 SubsubtableLength = sizeof(ACPI_ERDT_IBRD);
                 break;

            case ACPI_ERDT_TYPE_IBAD:
                 InfoTable = AcpiDmTableInfoErdtIbad;
                 break;

            case ACPI_ERDT_TYPE_CARD:
                 InfoTable = AcpiDmTableInfoErdtCard;
                 break;

            default:
                AcpiOsPrintf ("\n**** Unknown RMDD subtable type 0x%X\n",
                    Subsubtable->Type);

                /* Attempt to continue */

                if (!Subsubtable->Length)
                {
                    AcpiOsPrintf ("Invalid zero length subtable\n");
                    return;
                }
                goto NextSubsubtable;
            }

            /* Dump subtable header */

            Status = AcpiDmDumpTable (Table->Length, Suboffset, Subsubtable,
                Subsubtable->Length, AcpiDmTableInfoErdtHdr);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            /* Dump subtable body */

            Status = AcpiDmDumpTable (Table->Length, Suboffset, Subsubtable,
                Subsubtable->Length, InfoTable);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            /* CACD, MMRC, and IBRD subtables have simple flex array at end */

            if (TrailEntries)
            {
                NumTrailers = 0;
                while (NumTrailers < Subsubtable->Length - SubsubtableLength)
                {

                    /* Dump one flex array element */

                    Status = AcpiDmDumpTable (Table->Length, Suboffset +
                        SubsubtableLength + NumTrailers,
                        ACPI_ADD_PTR (ACPI_SUBTBL_HDR_16, Subsubtable,
                            SubsubtableLength + NumTrailers),
                        sizeof(UINT32), TrailEntries);
                    if (ACPI_FAILURE (Status))
                    {
                        return;
                    }
                    NumTrailers += sizeof(UINT32);
                }
            }

            /* DACD subtable has flex array of device agent structures */

            if (DacdEntries) {
                 ScopeOffset = Suboffset + SubsubtableLength;
                 ScopeTable = ACPI_ADD_PTR (ACPI_ERDT_DACD_PATHS,
                     Subsubtable, SubsubtableLength);
                 while (ScopeOffset < Suboffset + Subsubtable->Length)
                 {
                     /* Dump one device agent structure */

                     AcpiOsPrintf ("\n");
                     Status = AcpiDmDumpTable (Table->Length, ScopeOffset,
                         ScopeTable, ScopeTable->Header.Length, DacdEntries);
                     if (ACPI_FAILURE (Status))
                     {
                         return;
                     }

                     /* Flex array of UINT8 for device path */

                     NumTrailers = 0;
                     while (NumTrailers < ScopeTable->Header.Length - sizeof(ACPI_ERDT_DACD_PATHS))
                     {
                         /* Dump one UINT8 of the device path */

                         Status = AcpiDmDumpTable (Table->Length, ScopeOffset +
                             sizeof(ACPI_ERDT_DACD_PATHS) + NumTrailers,
                             ACPI_ADD_PTR (ACPI_SUBTBL_HDR_16, ScopeTable,
                                 sizeof(*ScopeTable) + NumTrailers),
                             sizeof(UINT32), AcpiDmTableInfoErdtDacdPath);
                         if (ACPI_FAILURE (Status))
                         {
                             return;
                         }
                         NumTrailers++;
                     }

                     ScopeOffset += ScopeTable->Header.Length;
                     ScopeTable = ACPI_ADD_PTR (ACPI_ERDT_DACD_PATHS,
                         ScopeTable, ScopeTable->Header.Length);
                 }
            }
NextSubsubtable:
            Suboffset += Subsubtable->Length;
            Subsubtable = ACPI_ADD_PTR (ACPI_SUBTBL_HDR_16, Table, Suboffset);
        }

        Offset += Subtable->Length;
        Subtable = ACPI_ADD_PTR (ACPI_SUBTBL_HDR_16, Subtable,
            Subtable->Length);
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
    ACPI_WHEA_HEADER        *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_ERST);


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoErst);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_WHEA_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            sizeof (ACPI_WHEA_HEADER), AcpiDmTableInfoErst0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next subtable (each subtable is of fixed length) */

        Offset += sizeof (ACPI_WHEA_HEADER);
        Subtable = ACPI_ADD_PTR (ACPI_WHEA_HEADER, Subtable,
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
    ACPI_FPDT_HEADER        *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_FPDT);
    ACPI_DMTABLE_INFO       *InfoTable;


    /* There is no main table (other than the standard ACPI header) */

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_FPDT_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common subtable header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoFpdtHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        switch (Subtable->Type)
        {
        case ACPI_FPDT_TYPE_BOOT:

            InfoTable = AcpiDmTableInfoFpdt0;
            break;

        case ACPI_FPDT_TYPE_S3PERF:

            InfoTable = AcpiDmTableInfoFpdt1;
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown FPDT subtable type 0x%X\n\n",
                Subtable->Type);

            /* Attempt to continue */

            if (!Subtable->Length)
            {
                AcpiOsPrintf ("Invalid zero length subtable\n");
                return;
            }
            goto NextSubtable;
        }

        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

NextSubtable:
        /* Point to next subtable */

        Offset += Subtable->Length;
        Subtable = ACPI_ADD_PTR (ACPI_FPDT_HEADER, Subtable,
            Subtable->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpGtdt
 *
 * PARAMETERS:  Table               - A GTDT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a GTDT. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpGtdt (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_GTDT_HEADER        *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_GTDT);
    ACPI_DMTABLE_INFO       *InfoTable;
    UINT32                  SubtableLength;
    UINT32                  GtCount;
    ACPI_GTDT_TIMER_ENTRY   *GtxTable;


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoGtdt);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Rev 3 fields */

    Subtable = ACPI_ADD_PTR (ACPI_GTDT_HEADER, Table, Offset);

    if (Table->Revision > 2)
    {
        SubtableLength = sizeof (ACPI_GTDT_EL2);
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            SubtableLength, AcpiDmTableInfoGtdtEl2);
        if (ACPI_FAILURE (Status))
        {
            return;
        }
        Offset += SubtableLength;
    }

    Subtable = ACPI_ADD_PTR (ACPI_GTDT_HEADER, Table, Offset);

    /* Subtables */

    while (Offset < Table->Length)
    {
        /* Common subtable header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoGtdtHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        GtCount = 0;
        switch (Subtable->Type)
        {
        case ACPI_GTDT_TYPE_TIMER_BLOCK:

            SubtableLength = sizeof (ACPI_GTDT_TIMER_BLOCK);
            GtCount = (ACPI_CAST_PTR (ACPI_GTDT_TIMER_BLOCK,
                Subtable))->TimerCount;

            InfoTable = AcpiDmTableInfoGtdt0;
            break;

        case ACPI_GTDT_TYPE_WATCHDOG:

            SubtableLength = sizeof (ACPI_GTDT_WATCHDOG);

            InfoTable = AcpiDmTableInfoGtdt1;
            break;

        default:

            /* Cannot continue on unknown type - no length */

            AcpiOsPrintf ("\n**** Unknown GTDT subtable type 0x%X\n",
                Subtable->Type);
            return;
        }

        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to end of current subtable (each subtable above is of fixed length) */

        Offset += SubtableLength;

        /* If there are any Gt Timer Blocks from above, dump them now */

        if (GtCount)
        {
            GtxTable = ACPI_ADD_PTR (
                ACPI_GTDT_TIMER_ENTRY, Subtable, SubtableLength);
            SubtableLength += GtCount * sizeof (ACPI_GTDT_TIMER_ENTRY);

            while (GtCount)
            {
                AcpiOsPrintf ("\n");
                Status = AcpiDmDumpTable (Length, Offset, GtxTable,
                    sizeof (ACPI_GTDT_TIMER_ENTRY), AcpiDmTableInfoGtdt0a);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
                Offset += sizeof (ACPI_GTDT_TIMER_ENTRY);
                GtxTable++;
                GtCount--;
            }
        }

        /* Point to next subtable */

        Subtable = ACPI_ADD_PTR (ACPI_GTDT_HEADER, Subtable, SubtableLength);
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
    ACPI_HEST_HEADER        *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_HEST);
    ACPI_DMTABLE_INFO       *InfoTable;
    UINT32                  SubtableLength;
    UINT32                  BankCount;
    ACPI_HEST_IA_ERROR_BANK *BankTable;


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoHest);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_HEST_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        BankCount = 0;
        switch (Subtable->Type)
        {
        case ACPI_HEST_TYPE_IA32_CHECK:

            InfoTable = AcpiDmTableInfoHest0;
            SubtableLength = sizeof (ACPI_HEST_IA_MACHINE_CHECK);
            BankCount = (ACPI_CAST_PTR (ACPI_HEST_IA_MACHINE_CHECK,
                Subtable))->NumHardwareBanks;
            break;

        case ACPI_HEST_TYPE_IA32_CORRECTED_CHECK:

            InfoTable = AcpiDmTableInfoHest1;
            SubtableLength = sizeof (ACPI_HEST_IA_CORRECTED);
            BankCount = (ACPI_CAST_PTR (ACPI_HEST_IA_CORRECTED,
                Subtable))->NumHardwareBanks;
            break;

        case ACPI_HEST_TYPE_IA32_NMI:

            InfoTable = AcpiDmTableInfoHest2;
            SubtableLength = sizeof (ACPI_HEST_IA_NMI);
            break;

        case ACPI_HEST_TYPE_AER_ROOT_PORT:

            InfoTable = AcpiDmTableInfoHest6;
            SubtableLength = sizeof (ACPI_HEST_AER_ROOT);
            break;

        case ACPI_HEST_TYPE_AER_ENDPOINT:

            InfoTable = AcpiDmTableInfoHest7;
            SubtableLength = sizeof (ACPI_HEST_AER);
            break;

        case ACPI_HEST_TYPE_AER_BRIDGE:

            InfoTable = AcpiDmTableInfoHest8;
            SubtableLength = sizeof (ACPI_HEST_AER_BRIDGE);
            break;

        case ACPI_HEST_TYPE_GENERIC_ERROR:

            InfoTable = AcpiDmTableInfoHest9;
            SubtableLength = sizeof (ACPI_HEST_GENERIC);
            break;

        case ACPI_HEST_TYPE_GENERIC_ERROR_V2:

            InfoTable = AcpiDmTableInfoHest10;
            SubtableLength = sizeof (ACPI_HEST_GENERIC_V2);
            break;

        case ACPI_HEST_TYPE_IA32_DEFERRED_CHECK:

            InfoTable = AcpiDmTableInfoHest11;
            SubtableLength = sizeof (ACPI_HEST_IA_DEFERRED_CHECK);
            BankCount = (ACPI_CAST_PTR (ACPI_HEST_IA_DEFERRED_CHECK,
                Subtable))->NumHardwareBanks;
            break;

        default:

            /* Cannot continue on unknown type - no length */

            AcpiOsPrintf ("\n**** Unknown HEST subtable type 0x%X\n",
                Subtable->Type);
            return;
        }

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            SubtableLength, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to end of current subtable (each subtable above is of fixed length) */

        Offset += SubtableLength;

        /* If there are any (fixed-length) Error Banks from above, dump them now */

        if (BankCount)
        {
            BankTable = ACPI_ADD_PTR (ACPI_HEST_IA_ERROR_BANK, Subtable,
                SubtableLength);
            SubtableLength += BankCount * sizeof (ACPI_HEST_IA_ERROR_BANK);

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

        /* Point to next subtable */

        Subtable = ACPI_ADD_PTR (ACPI_HEST_HEADER, Subtable, SubtableLength);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpHmat
 *
 * PARAMETERS:  Table               - A HMAT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a HMAT.
 *
 ******************************************************************************/

void
AcpiDmDumpHmat (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_HMAT_STRUCTURE     *HmatStruct;
    ACPI_HMAT_LOCALITY      *HmatLocality;
    ACPI_HMAT_CACHE         *HmatCache;
    UINT32                  Offset;
    UINT32                  SubtableOffset;
    UINT32                  Length;
    ACPI_DMTABLE_INFO       *InfoTable;
    UINT32                  i, j;


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoHmat);
    if (ACPI_FAILURE (Status))
    {
        return;
    }
    Offset = sizeof (ACPI_TABLE_HMAT);

    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");

        /* Dump HMAT structure header */

        HmatStruct = ACPI_ADD_PTR (ACPI_HMAT_STRUCTURE, Table, Offset);
        if (HmatStruct->Length < sizeof (ACPI_HMAT_STRUCTURE))
        {
            AcpiOsPrintf ("Invalid HMAT structure length\n");
            return;
        }
        Status = AcpiDmDumpTable (Table->Length, Offset, HmatStruct,
            HmatStruct->Length, AcpiDmTableInfoHmatHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        switch (HmatStruct->Type)
        {
        case ACPI_HMAT_TYPE_ADDRESS_RANGE:

            InfoTable = AcpiDmTableInfoHmat0;
            Length = sizeof (ACPI_HMAT_PROXIMITY_DOMAIN);
            break;

        case ACPI_HMAT_TYPE_LOCALITY:

            InfoTable = AcpiDmTableInfoHmat1;
            Length = sizeof (ACPI_HMAT_LOCALITY);
            break;

        case ACPI_HMAT_TYPE_CACHE:

            InfoTable = AcpiDmTableInfoHmat2;
            Length = sizeof (ACPI_HMAT_CACHE);
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown HMAT structure type 0x%X\n",
                HmatStruct->Type);

            /* Attempt to continue */

            goto NextSubtable;
        }

        /* Dump HMAT structure body */

        if (HmatStruct->Length < Length)
        {
            AcpiOsPrintf ("Invalid HMAT structure length\n");
            return;
        }
        Status = AcpiDmDumpTable (Table->Length, Offset, HmatStruct,
            HmatStruct->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Dump HMAT structure additional */

        switch (HmatStruct->Type)
        {
        case ACPI_HMAT_TYPE_LOCALITY:

            HmatLocality = ACPI_CAST_PTR (ACPI_HMAT_LOCALITY, HmatStruct);
            SubtableOffset = sizeof (ACPI_HMAT_LOCALITY);

            /* Dump initiator proximity domains */

            if ((UINT32)(HmatStruct->Length - SubtableOffset) <
                (UINT32)(HmatLocality->NumberOfInitiatorPDs * 4))
            {
                AcpiOsPrintf ("Invalid initiator proximity domain number\n");
                return;
            }
            for (i = 0; i < HmatLocality->NumberOfInitiatorPDs; i++)
            {
                Status = AcpiDmDumpTable (Table->Length, Offset + SubtableOffset,
                    ACPI_ADD_PTR (ACPI_HMAT_STRUCTURE, HmatStruct, SubtableOffset),
                    4, AcpiDmTableInfoHmat1a);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                SubtableOffset += 4;
            }

            /* Dump target proximity domains */

            if ((UINT32)(HmatStruct->Length - SubtableOffset) <
                (UINT32)(HmatLocality->NumberOfTargetPDs * 4))
            {
                AcpiOsPrintf ("Invalid target proximity domain number\n");
                return;
            }
            for (i = 0; i < HmatLocality->NumberOfTargetPDs; i++)
            {
                Status = AcpiDmDumpTable (Table->Length, Offset + SubtableOffset,
                    ACPI_ADD_PTR (ACPI_HMAT_STRUCTURE, HmatStruct, SubtableOffset),
                    4, AcpiDmTableInfoHmat1b);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                SubtableOffset += 4;
            }

            /* Dump latency/bandwidth entris */

            if ((UINT32)(HmatStruct->Length - SubtableOffset) <
                (UINT32)(HmatLocality->NumberOfInitiatorPDs *
                         HmatLocality->NumberOfTargetPDs * 2))
            {
                AcpiOsPrintf ("Invalid latency/bandwidth entry number\n");
                return;
            }
            for (i = 0; i < HmatLocality->NumberOfInitiatorPDs; i++)
            {
                for (j = 0; j < HmatLocality->NumberOfTargetPDs; j++)
                {
                    Status = AcpiDmDumpTable (Table->Length, Offset + SubtableOffset,
                        ACPI_ADD_PTR (ACPI_HMAT_STRUCTURE, HmatStruct, SubtableOffset),
                        2, AcpiDmTableInfoHmat1c);
                    if (ACPI_FAILURE(Status))
                    {
                        return;
                    }

                    SubtableOffset += 2;
                }
            }
            break;

        case ACPI_HMAT_TYPE_CACHE:

            HmatCache = ACPI_CAST_PTR (ACPI_HMAT_CACHE, HmatStruct);
            SubtableOffset = sizeof (ACPI_HMAT_CACHE);

            /* Dump SMBIOS handles */

            if ((UINT32)(HmatStruct->Length - SubtableOffset) <
                (UINT32)(HmatCache->NumberOfSMBIOSHandles * 2))
            {
                AcpiOsPrintf ("Invalid SMBIOS handle number\n");
                return;
            }
            for (i = 0; i < HmatCache->NumberOfSMBIOSHandles; i++)
            {
                Status = AcpiDmDumpTable (Table->Length, Offset + SubtableOffset,
                    ACPI_ADD_PTR (ACPI_HMAT_STRUCTURE, HmatStruct, SubtableOffset),
                    2, AcpiDmTableInfoHmat2a);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                SubtableOffset += 2;
            }
            break;

        default:

            break;
        }

NextSubtable:
        /* Point to next HMAT structure subtable */

        Offset += (HmatStruct->Length);
    }
}
