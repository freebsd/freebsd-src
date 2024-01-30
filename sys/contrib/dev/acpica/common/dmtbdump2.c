/******************************************************************************
 *
 * Module Name: dmtbdump2 - Dump ACPI data tables that contain no AML code
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2023, Intel Corp.
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

#include <wchar.h>
#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acdisasm.h>
#include <contrib/dev/acpica/include/actables.h>
#include <contrib/dev/acpica/compiler/aslcompiler.h>

/* This module used for application-level code only */

#define _COMPONENT          ACPI_CA_DISASSEMBLER
        ACPI_MODULE_NAME    ("dmtbdump2")


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpIort
 *
 * PARAMETERS:  Table               - A IORT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a IORT
 *
 ******************************************************************************/

void
AcpiDmDumpIort (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_TABLE_IORT         *Iort;
    ACPI_IORT_NODE          *IortNode;
    ACPI_IORT_ITS_GROUP     *IortItsGroup = NULL;
    ACPI_IORT_SMMU          *IortSmmu = NULL;
    ACPI_IORT_RMR           *IortRmr = NULL;
    UINT32                  Offset;
    UINT32                  NodeOffset;
    UINT32                  Length;
    ACPI_DMTABLE_INFO       *InfoTable;
    char                    *String;
    UINT32                  i;
    UINT32                  MappingByteLength;
    UINT8                   Revision;


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoIort);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    Revision = Table->Revision;

    /* IORT Revisions E, E.a and E.c have known issues and are not supported */

    if (Revision == 1 || Revision == 2 || Revision == 4)
    {
        AcpiOsPrintf ("\n**** Unsupported IORT revision 0x%X\n",
                      Revision);
        return;
    }

    Iort = ACPI_CAST_PTR (ACPI_TABLE_IORT, Table);
    Offset = sizeof (ACPI_TABLE_IORT);

    /* Dump the OptionalPadding (optional) */

    if (Iort->NodeOffset > Offset)
    {
        Status = AcpiDmDumpTable (Table->Length, Offset, Table,
            Iort->NodeOffset - Offset, AcpiDmTableInfoIortPad);
        if (ACPI_FAILURE (Status))
        {
            return;
        }
    }

    Offset = Iort->NodeOffset;
    while (Offset < Table->Length)
    {
        /* Common subtable header */

        IortNode = ACPI_ADD_PTR (ACPI_IORT_NODE, Table, Offset);
        AcpiOsPrintf ("\n");
        Length = ACPI_OFFSET (ACPI_IORT_NODE, NodeData);

        if (Revision == 0)
        {
            Status = AcpiDmDumpTable (Table->Length, Offset,
                IortNode, Length, AcpiDmTableInfoIortHdr);
        }
        else if (Revision >= 3)
        {
            Status = AcpiDmDumpTable (Table->Length, Offset,
                IortNode, Length, AcpiDmTableInfoIortHdr3);
        }

        if (ACPI_FAILURE (Status))
        {
            return;
        }

        NodeOffset = Length;

        switch (IortNode->Type)
        {
        case ACPI_IORT_NODE_ITS_GROUP:

            InfoTable = AcpiDmTableInfoIort0;
            Length = ACPI_OFFSET (ACPI_IORT_ITS_GROUP, Identifiers);
            IortItsGroup = ACPI_ADD_PTR (ACPI_IORT_ITS_GROUP, IortNode, NodeOffset);
            break;

        case ACPI_IORT_NODE_NAMED_COMPONENT:

            InfoTable = AcpiDmTableInfoIort1;
            Length = ACPI_OFFSET (ACPI_IORT_NAMED_COMPONENT, DeviceName);
            String = ACPI_ADD_PTR (char, IortNode, NodeOffset + Length);
            Length += strlen (String) + 1;
            break;

        case ACPI_IORT_NODE_PCI_ROOT_COMPLEX:

            InfoTable = AcpiDmTableInfoIort2;
            Length = IortNode->Length - NodeOffset;
            break;

        case ACPI_IORT_NODE_SMMU:

            InfoTable = AcpiDmTableInfoIort3;
            Length = ACPI_OFFSET (ACPI_IORT_SMMU, Interrupts);
            IortSmmu = ACPI_ADD_PTR (ACPI_IORT_SMMU, IortNode, NodeOffset);
            break;

        case ACPI_IORT_NODE_SMMU_V3:

            InfoTable = AcpiDmTableInfoIort4;
            Length = IortNode->Length - NodeOffset;
            break;

        case ACPI_IORT_NODE_PMCG:

            InfoTable = AcpiDmTableInfoIort5;
            Length = IortNode->Length - NodeOffset;
            break;

        case ACPI_IORT_NODE_RMR:

            InfoTable = AcpiDmTableInfoIort6;
            Length = IortNode->Length - NodeOffset;
            IortRmr = ACPI_ADD_PTR (ACPI_IORT_RMR, IortNode, NodeOffset);
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown IORT node type 0x%X\n",
                IortNode->Type);

            /* Attempt to continue */

            if (!IortNode->Length)
            {
                AcpiOsPrintf ("Invalid zero length IORT node\n");
                return;
            }
            goto NextSubtable;
        }

        /* Dump the node subtable header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset + NodeOffset,
            ACPI_ADD_PTR (ACPI_IORT_NODE, IortNode, NodeOffset),
            Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        NodeOffset += Length;

        /* Dump the node specific data */

        switch (IortNode->Type)
        {
        case ACPI_IORT_NODE_ITS_GROUP:

            /* Validate IortItsGroup to avoid compiler warnings */

            if (IortItsGroup)
            {
                for (i = 0; i < IortItsGroup->ItsCount; i++)
                {
                    Status = AcpiDmDumpTable (Table->Length, Offset + NodeOffset,
                        ACPI_ADD_PTR (ACPI_IORT_NODE, IortNode, NodeOffset),
                        4, AcpiDmTableInfoIort0a);
                    if (ACPI_FAILURE (Status))
                    {
                        return;
                    }

                    NodeOffset += 4;
                }
            }
            break;

        case ACPI_IORT_NODE_NAMED_COMPONENT:

            /* Dump the Padding (optional) */

            if (IortNode->Length > NodeOffset)
            {
                MappingByteLength =
                    IortNode->MappingCount * sizeof (ACPI_IORT_ID_MAPPING);
                Status = AcpiDmDumpTable (Table->Length, Offset + NodeOffset,
                    Table, IortNode->Length - NodeOffset - MappingByteLength,
                    AcpiDmTableInfoIort1a);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
            }
            break;

        case ACPI_IORT_NODE_SMMU:

            AcpiOsPrintf ("\n");

            /* Validate IortSmmu to avoid compiler warnings */

            if (IortSmmu)
            {
                Length = 2 * sizeof (UINT64);
                NodeOffset = IortSmmu->GlobalInterruptOffset;
                Status = AcpiDmDumpTable (Table->Length, Offset + NodeOffset,
                    ACPI_ADD_PTR (ACPI_IORT_NODE, IortNode, NodeOffset),
                    Length, AcpiDmTableInfoIort3a);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                NodeOffset = IortSmmu->ContextInterruptOffset;
                for (i = 0; i < IortSmmu->ContextInterruptCount; i++)
                {
                    Status = AcpiDmDumpTable (Table->Length, Offset + NodeOffset,
                        ACPI_ADD_PTR (ACPI_IORT_NODE, IortNode, NodeOffset),
                        8, AcpiDmTableInfoIort3b);
                    if (ACPI_FAILURE (Status))
                    {
                        return;
                    }

                    NodeOffset += 8;
                }

                NodeOffset = IortSmmu->PmuInterruptOffset;
                for (i = 0; i < IortSmmu->PmuInterruptCount; i++)
                {
                    Status = AcpiDmDumpTable (Table->Length, Offset + NodeOffset,
                        ACPI_ADD_PTR (ACPI_IORT_NODE, IortNode, NodeOffset),
                        8, AcpiDmTableInfoIort3c);
                    if (ACPI_FAILURE (Status))
                    {
                        return;
                    }

                    NodeOffset += 8;
                }
            }
            break;

        case ACPI_IORT_NODE_RMR:

            /* Validate IortRmr to avoid compiler warnings */
            if (IortRmr)
            {
                NodeOffset = IortRmr->RmrOffset;
                Length = sizeof (ACPI_IORT_RMR_DESC);
                for (i = 0; i < IortRmr->RmrCount; i++)
                {
                    AcpiOsPrintf ("\n");
                    Status = AcpiDmDumpTable (Table->Length, Offset + NodeOffset,
                        ACPI_ADD_PTR (ACPI_IORT_NODE, IortNode, NodeOffset),
                        Length, AcpiDmTableInfoIort6a);
                    if (ACPI_FAILURE (Status))
                    {
                        return;
                    }

                    NodeOffset += Length;
                }
            }
            break;

        default:

            break;
        }

        /* Dump the ID mappings */

        NodeOffset = IortNode->MappingOffset;
        for (i = 0; i < IortNode->MappingCount; i++)
        {
            AcpiOsPrintf ("\n");
            Length = sizeof (ACPI_IORT_ID_MAPPING);
            Status = AcpiDmDumpTable (Table->Length, Offset + NodeOffset,
                ACPI_ADD_PTR (ACPI_IORT_NODE, IortNode, NodeOffset),
                Length, AcpiDmTableInfoIortMap);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            NodeOffset += Length;
        }

NextSubtable:
        /* Point to next node subtable */

        Offset += IortNode->Length;
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
 * DESCRIPTION: Format the contents of a IVRS. Notes:
 *              The IVRS is essentially a flat table, with the following
 *              structure:
 *              <Main ACPI Table Header>
 *              <Main subtable - virtualization info>
 *              <IVHD>
 *                  <Device Entries>
 *              ...
 *              <IVHD>
 *                  <Device Entries>
 *              <IVMD>
 *              ...
 *
 ******************************************************************************/

void
AcpiDmDumpIvrs (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    UINT32                  Offset = sizeof (ACPI_TABLE_IVRS);
    UINT32                  EntryOffset;
    UINT32                  EntryLength;
    UINT32                  EntryType;
    ACPI_IVRS_DEVICE_HID    *HidSubtable;
    ACPI_IVRS_DE_HEADER     *DeviceEntry;
    ACPI_IVRS_HEADER        *Subtable;
    ACPI_DMTABLE_INFO       *InfoTable;


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoIvrs);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_IVRS_HEADER, Table, Offset);

    while (Offset < Table->Length)
    {
        switch (Subtable->Type)
        {
        /* Type 10h, IVHD (I/O Virtualization Hardware Definition) */

        case ACPI_IVRS_TYPE_HARDWARE1:

            AcpiOsPrintf ("\n");
            InfoTable = AcpiDmTableInfoIvrsHware1;
            break;

        /* Types 11h, 40h, IVHD (I/O Virtualization Hardware Definition) */

        case ACPI_IVRS_TYPE_HARDWARE2:
        case ACPI_IVRS_TYPE_HARDWARE3:

            AcpiOsPrintf ("\n");
            InfoTable = AcpiDmTableInfoIvrsHware23;
            break;

        /* Types 20h-22h, IVMD (I/O Virtualization Memory Definition Block) */

        case ACPI_IVRS_TYPE_MEMORY1:
        case ACPI_IVRS_TYPE_MEMORY2:
        case ACPI_IVRS_TYPE_MEMORY3:

            AcpiOsPrintf ("\n");
            InfoTable = AcpiDmTableInfoIvrsMemory;
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown IVRS subtable type 0x%X\n",
                Subtable->Type);

            /* Attempt to continue */

            if (!Subtable->Length)
            {
                AcpiOsPrintf ("Invalid zero length subtable\n");
                return;
            }
            goto NextSubtable;
        }

        /* Dump the subtable */

        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable,
            Subtable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* The hardware subtables (IVHD) can contain multiple device entries */

        if (Subtable->Type == ACPI_IVRS_TYPE_HARDWARE1 ||
            Subtable->Type == ACPI_IVRS_TYPE_HARDWARE2 ||
            Subtable->Type == ACPI_IVRS_TYPE_HARDWARE3)
        {
            if (Subtable->Type == ACPI_IVRS_TYPE_HARDWARE1)
            {
                EntryOffset = Offset + sizeof (ACPI_IVRS_HARDWARE1);
                DeviceEntry = ACPI_ADD_PTR (ACPI_IVRS_DE_HEADER, Subtable,
                    sizeof (ACPI_IVRS_HARDWARE1));
            }
            else
            {
                /* ACPI_IVRS_TYPE_HARDWARE2, HARDWARE3 subtable types */

                EntryOffset = Offset + sizeof (ACPI_IVRS_HARDWARE2);
                DeviceEntry = ACPI_ADD_PTR (ACPI_IVRS_DE_HEADER, Subtable,
                    sizeof (ACPI_IVRS_HARDWARE2));
            }

            /* Process all of the Device Entries */

            while (EntryOffset < (Offset + Subtable->Length))
            {
                AcpiOsPrintf ("\n");

                /*
                 * Upper 2 bits of Type encode the length of the device entry
                 *
                 * 00 = 4 byte
                 * 01 = 8 byte
                 * 1x = variable length
                 */
                EntryType = DeviceEntry->Type;
                EntryLength = EntryType >> 6 == 1 ? 8 : 4;

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

                /* Variable-length entries */

                case ACPI_IVRS_TYPE_HID:

                    EntryLength = 4;
                    InfoTable = AcpiDmTableInfoIvrsHid;
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
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                HidSubtable = ACPI_CAST_PTR (ACPI_IVRS_DEVICE_HID, DeviceEntry);
                EntryOffset += EntryLength;
                DeviceEntry = ACPI_ADD_PTR (ACPI_IVRS_DE_HEADER, HidSubtable,
                    EntryLength);

                if (EntryType == ACPI_IVRS_TYPE_HID)
                {
                    /*
                     * Determine if the HID is an integer or a string.
                     * An integer is defined to be 32 bits, with the upper 32 bits
                     * set to zero. (from the ACPI Spec): "The HID can be a 32-bit
                     * integer or a character string. If an integer, the lower
                     * 4 bytes of the field contain the integer and the upper
                     * 4 bytes are padded with 0".
                     */
                    if (UtIsIdInteger ((UINT8 *) &HidSubtable->AcpiHid))
                    {
                        Status = AcpiDmDumpTable (Table->Length, EntryOffset,
                            &HidSubtable->AcpiHid, 8, AcpiDmTableInfoIvrsHidInteger);
                    }
                    else
                    {
                        Status = AcpiDmDumpTable (Table->Length, EntryOffset,
                            &HidSubtable->AcpiHid, 8, AcpiDmTableInfoIvrsHidString);
                    }
                    if (ACPI_FAILURE (Status))
                    {
                        return;
                    }

                    EntryOffset += 8;

                    /*
                     * Determine if the CID is an integer or a string. The format
                     * of the CID is the same as the HID above. From ACPI Spec:
                     * "If present, CID must be a single Compatible Device ID
                     * following the same format as the HID field."
                     */
                    if (UtIsIdInteger ((UINT8 *) &HidSubtable->AcpiCid))
                    {
                        Status = AcpiDmDumpTable (Table->Length, EntryOffset,
                            &HidSubtable->AcpiCid, 8, AcpiDmTableInfoIvrsCidInteger);
                    }
                    else
                    {
                        Status = AcpiDmDumpTable (Table->Length, EntryOffset,
                            &HidSubtable->AcpiCid, 8, AcpiDmTableInfoIvrsCidString);
                    }
                    if (ACPI_FAILURE (Status))
                    {
                        return;
                    }

                    EntryOffset += 8;
                    EntryLength = HidSubtable->UidLength;

                    if (EntryLength > ACPI_IVRS_UID_NOT_PRESENT)
                    {
                        /* Dump the UID based upon the UidType field (String or Integer) */

                        if (HidSubtable->UidType == ACPI_IVRS_UID_IS_STRING)
                        {
                            Status = AcpiDmDumpTable (Table->Length, EntryOffset,
                                &HidSubtable->UidType, EntryLength, AcpiDmTableInfoIvrsUidString);
                            if (ACPI_FAILURE (Status))
                            {
                                return;
                            }
                        }
                        else /* ACPI_IVRS_UID_IS_INTEGER */
                        {
                            Status = AcpiDmDumpTable (Table->Length, EntryOffset,
                                &HidSubtable->UidType, EntryLength, AcpiDmTableInfoIvrsUidInteger);
                            if (ACPI_FAILURE (Status))
                            {
                                return;
                            }
                        }
                    }

                    EntryOffset += EntryLength+2;
                    DeviceEntry = ACPI_ADD_PTR (ACPI_IVRS_DE_HEADER,
                        Table, EntryOffset);
                }
            }
        }

NextSubtable:
        /* Point to next subtable */

        Offset += Subtable->Length;
        Subtable = ACPI_ADD_PTR (ACPI_IVRS_HEADER, Subtable, Subtable->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpLpit
 *
 * PARAMETERS:  Table               - A LPIT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a LPIT. This table type consists
 *              of an open-ended number of subtables. Note: There are no
 *              entries in the main table. An LPIT consists of the table
 *              header and then subtables only.
 *
 ******************************************************************************/

void
AcpiDmDumpLpit (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_LPIT_HEADER        *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_LPIT);
    ACPI_DMTABLE_INFO       *InfoTable;
    UINT32                  SubtableLength;


    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_LPIT_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common subtable header */

        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            sizeof (ACPI_LPIT_HEADER), AcpiDmTableInfoLpitHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        switch (Subtable->Type)
        {
        case ACPI_LPIT_TYPE_NATIVE_CSTATE:

            InfoTable = AcpiDmTableInfoLpit0;
            SubtableLength = sizeof (ACPI_LPIT_NATIVE);
            break;

        default:

            /* Cannot continue on unknown type - no length */

            AcpiOsPrintf ("\n**** Unknown LPIT subtable type 0x%X\n",
                Subtable->Type);
            return;
        }

        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            SubtableLength, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        AcpiOsPrintf ("\n");

        /* Point to next subtable */

        Offset += SubtableLength;
        Subtable = ACPI_ADD_PTR (ACPI_LPIT_HEADER, Subtable, SubtableLength);
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
    ACPI_SUBTABLE_HEADER    *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_MADT);
    ACPI_DMTABLE_INFO       *InfoTable;


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoMadt);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_SUBTABLE_HEADER, Table, Offset);
    DbgPrint (ASL_PARSE_OUTPUT, "//0B) Offset %X, from table start: 0x%8.8X%8.8X\n",
        Offset, ACPI_FORMAT_UINT64 (ACPI_CAST_PTR (char, Subtable) - ACPI_CAST_PTR (char, Table)));
    while (Offset < Table->Length)
    {
        /* Common subtable header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoMadtHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        DbgPrint (ASL_PARSE_OUTPUT, "subtableType: %X\n", Subtable->Type);
        switch (Subtable->Type)
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

        case ACPI_MADT_TYPE_GENERIC_MSI_FRAME:

            InfoTable = AcpiDmTableInfoMadt13;
            break;

        case ACPI_MADT_TYPE_GENERIC_REDISTRIBUTOR:

            InfoTable = AcpiDmTableInfoMadt14;
            break;

        case ACPI_MADT_TYPE_GENERIC_TRANSLATOR:

            InfoTable = AcpiDmTableInfoMadt15;
            break;

        case ACPI_MADT_TYPE_MULTIPROC_WAKEUP:

            InfoTable = AcpiDmTableInfoMadt16;
            break;

        case ACPI_MADT_TYPE_CORE_PIC:

            InfoTable = AcpiDmTableInfoMadt17;
            break;

        case ACPI_MADT_TYPE_LIO_PIC:

            InfoTable = AcpiDmTableInfoMadt18;
            break;

        case ACPI_MADT_TYPE_HT_PIC:

            InfoTable = AcpiDmTableInfoMadt19;
            break;

        case ACPI_MADT_TYPE_EIO_PIC:

            InfoTable = AcpiDmTableInfoMadt20;
            break;

        case ACPI_MADT_TYPE_MSI_PIC:

            InfoTable = AcpiDmTableInfoMadt21;
            break;

        case ACPI_MADT_TYPE_BIO_PIC:

            InfoTable = AcpiDmTableInfoMadt22;
            break;

        case ACPI_MADT_TYPE_LPC_PIC:

            InfoTable = AcpiDmTableInfoMadt23;
            break;

        case ACPI_MADT_TYPE_RINTC:

            InfoTable = AcpiDmTableInfoMadt24;
            break;

        case ACPI_MADT_TYPE_IMSIC:

            InfoTable = AcpiDmTableInfoMadt25;
            break;

        case ACPI_MADT_TYPE_APLIC:

            InfoTable = AcpiDmTableInfoMadt26;
            break;

        case ACPI_MADT_TYPE_PLIC:

            InfoTable = AcpiDmTableInfoMadt27;
            break;

        default:

            if ((Subtable->Type >= ACPI_MADT_TYPE_RESERVED) &&
                (Subtable->Type < ACPI_MADT_TYPE_OEM_RESERVED))
            {
                AcpiOsPrintf ("\n**** Unknown MADT subtable type 0x%X\n\n",
                    Subtable->Type);
                goto NextSubtable;
            }
            else if (Subtable->Type >= ACPI_MADT_TYPE_OEM_RESERVED)
            {
                DbgPrint (ASL_PARSE_OUTPUT, "//[Found an OEM structure, type = %0x]\n",
                    Subtable->Type);
                Offset += sizeof (ACPI_SUBTABLE_HEADER);
                DbgPrint (ASL_PARSE_OUTPUT, "//[0) Subtable->Length = %X, Subtable = %p, Offset = %X]\n",
                    Subtable->Length, Subtable, Offset);
                DbgPrint (ASL_PARSE_OUTPUT, "//[0A) Offset from table start: 0x%8.8X%8.8X]\n",
                    ACPI_FORMAT_UINT64 (ACPI_CAST_PTR (char, Subtable) - ACPI_CAST_PTR (char, Table)));
            }

            /* Attempt to continue */

            if (!Subtable->Length)
            {
                AcpiOsPrintf ("Invalid zero length subtable\n");
                return;
            }

            /* Dump the OEM data */

            Status = AcpiDmDumpTable (Length, Offset, ACPI_CAST_PTR (UINT8, Table) + Offset,
                Subtable->Length - sizeof (ACPI_SUBTABLE_HEADER), AcpiDmTableInfoMadt128);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            DbgPrint (ASL_PARSE_OUTPUT, "//[1) Subtable->Length = %X, Offset = %X]\n",
                Subtable->Length, Offset);
            Offset -= sizeof (ACPI_SUBTABLE_HEADER);

            goto NextSubtable;
        }

        DbgPrint (ASL_PARSE_OUTPUT, "//[2) Subtable->Length = %X, Offset = %X]\n",
            Subtable->Length, Offset);
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

NextSubtable:
        /* Point to next subtable */

        DbgPrint (ASL_PARSE_OUTPUT, "//[3) Subtable->Length = %X, Offset = %X]\n",
            Subtable->Length, Offset);
        DbgPrint (ASL_PARSE_OUTPUT, "//[4) Offset from table start: 0x%8.8X%8.8X (%p) %p]\n",
            ACPI_FORMAT_UINT64 (ACPI_CAST_PTR (UINT8, Subtable) - ACPI_CAST_PTR (UINT8, Table)), Subtable, Table);
        if (Offset > Table->Length)
        {
            return;
        }

        Subtable = ACPI_ADD_PTR (ACPI_SUBTABLE_HEADER, Subtable,
            Subtable->Length);

        Offset = ACPI_CAST_PTR (char, Subtable) - ACPI_CAST_PTR (char, Table);
        if (Offset >= Table->Length)
        {
            return;
        }

        DbgPrint (ASL_PARSE_OUTPUT, "//[5) Next Subtable %p, length %X]\n",
            Subtable, Subtable->Length);
        DbgPrint (ASL_PARSE_OUTPUT, "//[5B) Offset from table start: 0x%8.8X%8.8X (%p)]\n",
            ACPI_FORMAT_UINT64 (ACPI_CAST_PTR (char, Subtable) - ACPI_CAST_PTR (char, Table)), Subtable);
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
    ACPI_MCFG_ALLOCATION    *Subtable;


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoMcfg);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_MCFG_ALLOCATION, Table, Offset);
    while (Offset < Table->Length)
    {
        if (Offset + sizeof (ACPI_MCFG_ALLOCATION) > Table->Length)
        {
            AcpiOsPrintf ("Warning: there are %u invalid trailing bytes\n",
                (UINT32) sizeof (ACPI_MCFG_ALLOCATION) - (Offset - Table->Length));
            return;
        }

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable,
            sizeof (ACPI_MCFG_ALLOCATION), AcpiDmTableInfoMcfg0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next subtable (each subtable is of fixed length) */

        Offset += sizeof (ACPI_MCFG_ALLOCATION);
        Subtable = ACPI_ADD_PTR (ACPI_MCFG_ALLOCATION, Subtable,
            sizeof (ACPI_MCFG_ALLOCATION));
    }
}

/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpMpam
 *
 * PARAMETERS:  Table               - A MPAM table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a MPAM table
 *
 ******************************************************************************/

void
AcpiDmDumpMpam (
    ACPI_TABLE_HEADER          *Table)
{
    ACPI_STATUS                Status;
    ACPI_MPAM_MSC_NODE         *MpamMscNode;
    ACPI_MPAM_RESOURCE_NODE    *MpamResourceNode;
    ACPI_DMTABLE_INFO          *InfoTable;
    UINT32                     Offset = sizeof(ACPI_TABLE_HEADER);
    UINT32                     MpamResourceNodeLength = 0;

    while (Offset < Table->Length)
    {
        MpamMscNode = ACPI_ADD_PTR (ACPI_MPAM_MSC_NODE, Table, Offset);

        /* Subtable: MSC */
        Status = AcpiDmDumpTable (MpamMscNode->Length, 0, MpamMscNode, 0,
            AcpiDmTableInfoMpam0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Offset the start of the array of resources */
        Offset += sizeof(ACPI_MPAM_MSC_NODE);

        /* Subtable: MSC RIS(es) */
        for (UINT32 ResourceIdx = 0; ResourceIdx < MpamMscNode->NumResouceNodes; ResourceIdx++)
        {
            MpamResourceNode = ACPI_ADD_PTR (ACPI_MPAM_RESOURCE_NODE, Table, Offset);

            MpamResourceNodeLength = sizeof(ACPI_MPAM_RESOURCE_NODE) +
                MpamResourceNode->NumFunctionalDeps * sizeof(ACPI_MPAM_FUNC_DEPS);

            Offset += MpamResourceNodeLength;

            /* Subtable: MSC RIS */
            Status = AcpiDmDumpTable (MpamResourceNodeLength, 0, MpamResourceNode, 0,
                AcpiDmTableInfoMpam1);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            switch (MpamResourceNode->LocatorType)
            {
                case ACPI_MPAM_LOCATION_TYPE_PROCESSOR_CACHE:
                    InfoTable = AcpiDmTableInfoMpam1A;
                    break;
                case ACPI_MPAM_LOCATION_TYPE_MEMORY:
                    InfoTable = AcpiDmTableInfoMpam1B;
                    break;
                case ACPI_MPAM_LOCATION_TYPE_SMMU:
                    InfoTable = AcpiDmTableInfoMpam1C;
                    break;
                case ACPI_MPAM_LOCATION_TYPE_MEMORY_CACHE:
                    InfoTable = AcpiDmTableInfoMpam1D;
                    break;
                case ACPI_MPAM_LOCATION_TYPE_ACPI_DEVICE:
                    InfoTable = AcpiDmTableInfoMpam1E;
                    break;
                case ACPI_MPAM_LOCATION_TYPE_INTERCONNECT:
                    InfoTable = AcpiDmTableInfoMpam1F;
                    break;
                case ACPI_MPAM_LOCATION_TYPE_UNKNOWN:
                    InfoTable = AcpiDmTableInfoMpam1G;
                default:
                    AcpiOsPrintf ("\n**** Unknown MPAM locator type 0x%X\n",
                        MpamResourceNode->LocatorType);
                    return;
            }

            /* Subtable: MSC Resource Locator(s) */
            Status = AcpiDmDumpTable (sizeof(ACPI_MPAM_RESOURCE_LOCATOR), 0,
                &MpamResourceNode->Locator, 0, InfoTable);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            /* Get the number of functional dependencies of an RIS */
            Status = AcpiDmDumpTable (sizeof(UINT32), 0, &MpamResourceNode->NumFunctionalDeps, 0,
                AcpiDmTableInfoMpam1Deps);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            /* Subtable: MSC functional dependencies */
            for (UINT32 funcDep = 0; funcDep < MpamResourceNode->NumFunctionalDeps; funcDep++)
            {
                Status = AcpiDmDumpTable (sizeof(ACPI_MPAM_FUNC_DEPS), 0,
                    &MpamResourceNode->NumFunctionalDeps, 0, AcpiDmTableInfoMpam2);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
            }

            AcpiOsPrintf ("\n\n");
        }

    }

    return;
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
    ACPI_MPST_POWER_NODE    *Subtable0;
    ACPI_MPST_POWER_STATE   *Subtable0A;
    ACPI_MPST_COMPONENT     *Subtable0B;
    ACPI_MPST_DATA_HDR      *Subtable1;
    ACPI_MPST_POWER_DATA    *Subtable2;
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
    Subtable0 = ACPI_ADD_PTR (ACPI_MPST_POWER_NODE, Table, Offset);

    while ((Offset < Table->Length) && SubtableCount)
    {
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable0,
            sizeof (ACPI_MPST_POWER_NODE), AcpiDmTableInfoMpst0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Extract the sub-subtable counts */

        PowerStateCount = Subtable0->NumPowerStates;
        ComponentCount = Subtable0->NumPhysicalComponents;
        Offset += sizeof (ACPI_MPST_POWER_NODE);

        /* Sub-subtables - Memory Power State Structure(s) */

        Subtable0A = ACPI_ADD_PTR (ACPI_MPST_POWER_STATE, Subtable0,
            sizeof (ACPI_MPST_POWER_NODE));

        while (PowerStateCount)
        {
            AcpiOsPrintf ("\n");
            Status = AcpiDmDumpTable (Table->Length, Offset, Subtable0A,
                sizeof (ACPI_MPST_POWER_STATE), AcpiDmTableInfoMpst0A);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            Subtable0A++;
            PowerStateCount--;
            Offset += sizeof (ACPI_MPST_POWER_STATE);
       }

        /* Sub-subtables - Physical Component ID Structure(s) */

        Subtable0B = ACPI_CAST_PTR (ACPI_MPST_COMPONENT, Subtable0A);

        if (ComponentCount)
        {
            AcpiOsPrintf ("\n");
        }

        while (ComponentCount)
        {
            Status = AcpiDmDumpTable (Table->Length, Offset, Subtable0B,
                sizeof (ACPI_MPST_COMPONENT), AcpiDmTableInfoMpst0B);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            Subtable0B++;
            ComponentCount--;
            Offset += sizeof (ACPI_MPST_COMPONENT);
        }

        /* Point to next Memory Power Node subtable */

        SubtableCount--;
        Subtable0 = ACPI_ADD_PTR (ACPI_MPST_POWER_NODE, Subtable0,
            sizeof (ACPI_MPST_POWER_NODE) +
            (sizeof (ACPI_MPST_POWER_STATE) * Subtable0->NumPowerStates) +
            (sizeof (ACPI_MPST_COMPONENT) * Subtable0->NumPhysicalComponents));
    }

    /* Subtable: Count of Memory Power State Characteristic structures */

    AcpiOsPrintf ("\n");
    Subtable1 = ACPI_CAST_PTR (ACPI_MPST_DATA_HDR, Subtable0);
    Status = AcpiDmDumpTable (Table->Length, Offset, Subtable1,
        sizeof (ACPI_MPST_DATA_HDR), AcpiDmTableInfoMpst1);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    SubtableCount = Subtable1->CharacteristicsCount;
    Offset += sizeof (ACPI_MPST_DATA_HDR);

    /* Subtable: Memory Power State Characteristics structure(s) */

    Subtable2 = ACPI_ADD_PTR (ACPI_MPST_POWER_DATA, Subtable1,
        sizeof (ACPI_MPST_DATA_HDR));

    while ((Offset < Table->Length) && SubtableCount)
    {
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable2,
            sizeof (ACPI_MPST_POWER_DATA), AcpiDmTableInfoMpst2);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        Subtable2++;
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
    ACPI_MSCT_PROXIMITY     *Subtable;


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoMsct);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_MSCT_PROXIMITY, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common subtable header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable,
            sizeof (ACPI_MSCT_PROXIMITY), AcpiDmTableInfoMsct0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next subtable */

        Offset += sizeof (ACPI_MSCT_PROXIMITY);
        Subtable = ACPI_ADD_PTR (ACPI_MSCT_PROXIMITY, Subtable,
            sizeof (ACPI_MSCT_PROXIMITY));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpNfit
 *
 * PARAMETERS:  Table               - A NFIT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of an NFIT.
 *
 ******************************************************************************/

void
AcpiDmDumpNfit (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    UINT32                  Offset = sizeof (ACPI_TABLE_NFIT);
    UINT32                  FieldOffset = 0;
    UINT32                  Length;
    ACPI_NFIT_HEADER        *Subtable;
    ACPI_DMTABLE_INFO       *InfoTable;
    ACPI_NFIT_INTERLEAVE    *Interleave = NULL;
    ACPI_NFIT_SMBIOS        *SmbiosInfo = NULL;
    ACPI_NFIT_FLUSH_ADDRESS *Hint = NULL;
    UINT32                  i;


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoNfit);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_NFIT_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        /* NFIT subtable header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoNfitHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        switch (Subtable->Type)
        {
        case ACPI_NFIT_TYPE_SYSTEM_ADDRESS:

            InfoTable = AcpiDmTableInfoNfit0;
            break;

        case ACPI_NFIT_TYPE_MEMORY_MAP:

            InfoTable = AcpiDmTableInfoNfit1;
            break;

        case ACPI_NFIT_TYPE_INTERLEAVE:

            /* Has a variable number of 32-bit values at the end */

            InfoTable = AcpiDmTableInfoNfit2;
            FieldOffset = sizeof (ACPI_NFIT_INTERLEAVE);
            break;

        case ACPI_NFIT_TYPE_SMBIOS:

            SmbiosInfo = ACPI_CAST_PTR (ACPI_NFIT_SMBIOS, Subtable);
            InfoTable = AcpiDmTableInfoNfit3;
            break;

        case ACPI_NFIT_TYPE_CONTROL_REGION:

            InfoTable = AcpiDmTableInfoNfit4;
            break;

        case ACPI_NFIT_TYPE_DATA_REGION:

            InfoTable = AcpiDmTableInfoNfit5;
            break;

        case ACPI_NFIT_TYPE_FLUSH_ADDRESS:

            /* Has a variable number of 64-bit addresses at the end */

            InfoTable = AcpiDmTableInfoNfit6;
            FieldOffset = sizeof (ACPI_NFIT_FLUSH_ADDRESS);
            break;

        case ACPI_NFIT_TYPE_CAPABILITIES:    /* ACPI 6.0A */

            InfoTable = AcpiDmTableInfoNfit7;
            break;

        default:
            AcpiOsPrintf ("\n**** Unknown NFIT subtable type 0x%X\n",
                Subtable->Type);

            /* Attempt to continue */

            if (!Subtable->Length)
            {
                AcpiOsPrintf ("Invalid zero length subtable\n");
                return;
            }
            goto NextSubtable;
        }

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable,
            Subtable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Per-subtable variable-length fields */

        switch (Subtable->Type)
        {
        case ACPI_NFIT_TYPE_INTERLEAVE:

            Interleave = ACPI_CAST_PTR (ACPI_NFIT_INTERLEAVE, Subtable);
            for (i = 0; i < Interleave->LineCount; i++)
            {
                Status = AcpiDmDumpTable (Table->Length, Offset + FieldOffset,
                    &Interleave->LineOffset[i],
                    sizeof (UINT32), AcpiDmTableInfoNfit2a);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                FieldOffset += sizeof (UINT32);
            }
            break;

        case ACPI_NFIT_TYPE_SMBIOS:

            Length = Subtable->Length -
                sizeof (ACPI_NFIT_SMBIOS);

            if (Length)
            {
                Status = AcpiDmDumpTable (Table->Length,
                    sizeof (ACPI_NFIT_SMBIOS),
                    SmbiosInfo,
                    Length, AcpiDmTableInfoNfit3a);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
            }

            break;

        case ACPI_NFIT_TYPE_FLUSH_ADDRESS:

            Hint = ACPI_CAST_PTR (ACPI_NFIT_FLUSH_ADDRESS, Subtable);
            for (i = 0; i < Hint->HintCount; i++)
            {
                Status = AcpiDmDumpTable (Table->Length, Offset + FieldOffset,
                    &Hint->HintAddress[i],
                    sizeof (UINT64), AcpiDmTableInfoNfit6a);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                FieldOffset += sizeof (UINT64);
            }
            break;

        default:
            break;
        }

NextSubtable:
        /* Point to next subtable */

        Offset += Subtable->Length;
        Subtable = ACPI_ADD_PTR (ACPI_NFIT_HEADER, Subtable, Subtable->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpNhlt
 *
 * PARAMETERS:  Table               - A NHLT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of an NHLT.
 *
 ******************************************************************************/

void
AcpiDmDumpNhlt (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    UINT32                  Offset;
    UINT32                  TableLength = Table->Length;
    UINT32                  EndpointCount;
    UINT8                   FormatsCount;
    ACPI_NHLT_ENDPOINT      *Subtable;
    ACPI_NHLT_FORMAT_CONFIG *FormatSubtable;
    ACPI_TABLE_NHLT         *InfoTable;
    UINT32                  CapabilitiesSize;
    UINT32                  i;
    UINT32                  j;
    UINT32                  EndpointEndOffset;
    UINT8                   ConfigType = 0;
    UINT8                   ArrayType;
    UINT8                   MicrophoneCount;
    ACPI_NHLT_VENDOR_MIC_COUNT          *MicCount;
    ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_A  *DevSpecific;
    ACPI_NHLT_FORMATS_CONFIG            *FormatsConfig;
    ACPI_NHLT_DEVICE_INFO_COUNT         *Count;
    ACPI_NHLT_DEVICE_INFO               *DeviceInfo;
    ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_B  *Capabilities;


    /* Main table */

    AcpiOsPrintf ("    /* Main table */\n");

    Status = AcpiDmDumpTable (TableLength, 0, Table, 0, AcpiDmTableInfoNhlt);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Get the Endpoint Descriptor Count */

    InfoTable = ACPI_ADD_PTR (ACPI_TABLE_NHLT, Table, 0);
    EndpointCount = InfoTable->EndpointCount;

    /* Subtables */

    Offset = sizeof (ACPI_TABLE_NHLT);

    while (Offset < TableLength)
    {
        /* A variable number of Endpoint Descriptors - process each */

        for (i = 0; i < EndpointCount; i++)
        {
            /* Do the Endpoint Descriptor table */

            Subtable = ACPI_ADD_PTR (ACPI_NHLT_ENDPOINT, Table, Offset);

            /* Check for endpoint descriptor length beyond end-of-table */

            if (Subtable->DescriptorLength > TableLength)
            {
                Offset += 1;
                AcpiOsPrintf ("\n    /* Endpoint Descriptor Length larger than"
                    " table size: %X, table %X, adjusting table offset (+1) */\n",
                    Subtable->DescriptorLength, TableLength);

                Subtable = ACPI_ADD_PTR (ACPI_NHLT_ENDPOINT, Table, Offset);
            }

            AcpiOsPrintf ("\n    /* Endpoint Descriptor #%u */\n", i+1);
            Status = AcpiDmDumpTable (TableLength, Offset, Subtable,
                Subtable->DescriptorLength, AcpiDmTableInfoNhlt0);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            EndpointEndOffset = Subtable->DescriptorLength + Offset;

            /* Check for endpoint descriptor beyond end-of-table */

            if (Subtable->DescriptorLength > TableLength)
            {
                AcpiOsPrintf ("\n    /* Endpoint Descriptor Length larger than table size: %X, table %X */\n",
                    Subtable->DescriptorLength, TableLength);
            }

            Offset += sizeof (ACPI_NHLT_ENDPOINT);
            Subtable = ACPI_ADD_PTR (ACPI_NHLT_ENDPOINT, Table, Offset);

            /* Do the Device Specific table */

            AcpiOsPrintf ("\n    /* Endpoint Device_Specific_Config table */\n");
            DevSpecific = ACPI_CAST_PTR (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_A, Subtable);
            CapabilitiesSize = DevSpecific->CapabilitiesSize;
            Status = AcpiDmDumpTable (TableLength, Offset, DevSpecific,
                sizeof (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_B), AcpiDmTableInfoNhlt5b);
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            ArrayType = 0;

            /* Different subtables based upon capabilities_size */

            switch (CapabilitiesSize)
            {
            case 0:
                Offset += sizeof (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_B);
                break;

            case 1:
                Status = AcpiDmDumpTable (TableLength, Offset, DevSpecific,
                    sizeof (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_C), AcpiDmTableInfoNhlt5c);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
                Offset += sizeof (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_C);
                break;

            case 2:
                Status = AcpiDmDumpTable (TableLength, Offset, DevSpecific,
                    sizeof (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG), AcpiDmTableInfoNhlt5);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
                Offset += sizeof (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG);
                break;

            case 3:
            default:
                /* Extract the ConfigType and ArrayType */

                ConfigType = DevSpecific->ConfigType;
                ArrayType = DevSpecific->ArrayType;

                Status = AcpiDmDumpTable (TableLength, Offset, DevSpecific,
                    sizeof (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_A), AcpiDmTableInfoNhlt5a);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                /* Capabilities Size == 3 */
                Offset += sizeof (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_A);
                break;

            case 7:
                ConfigType = DevSpecific->ConfigType;
                Subtable = ACPI_ADD_PTR (ACPI_NHLT_ENDPOINT, Table, Offset);
                DevSpecific = ACPI_CAST_PTR (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_A, Subtable);

                AcpiOsPrintf ("\n    /* Render Feedback Device-Specific table */\n");
                Status = AcpiDmDumpTable (TableLength, Offset, DevSpecific,
                    sizeof (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG), AcpiDmTableInfoNhlt5);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                /* Capabilities Size = 7 */
                Offset += sizeof (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG);

                if (ConfigType == ACPI_NHLT_CONFIG_TYPE_RENDER_FEEDBACK)
                {
                    Subtable = ACPI_ADD_PTR (ACPI_NHLT_ENDPOINT, Table, Offset);
                    DevSpecific = ACPI_CAST_PTR (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_A, Subtable);

                    Status = AcpiDmDumpTable (TableLength, Offset, DevSpecific,
                        sizeof (ACPI_NHLT_RENDER_FEEDBACK_DEVICE_SPECIFIC_CONFIG), AcpiDmTableInfoNhlt6b);
                    if (ACPI_FAILURE (Status))
                    {
                        return;
                    }
                    Offset += sizeof (ACPI_NHLT_RENDER_FEEDBACK_DEVICE_SPECIFIC_CONFIG);
                }
                break;
           }

            /* Check for a vendor-defined mic array */

            if (ConfigType == ACPI_NHLT_CONFIG_TYPE_MIC_ARRAY)
            {
                if ((ArrayType & ACPI_NHLT_ARRAY_TYPE_MASK) == ACPI_NHLT_VENDOR_DEFINED)
                {
                    /* Vendor-defined microphone array; get the microphone count first */

                    AcpiOsPrintf ("\n    /* Vendor-defined microphone count */\n");
                    MicCount = ACPI_ADD_PTR (ACPI_NHLT_VENDOR_MIC_COUNT, Table, Offset);
                    MicrophoneCount = MicCount->MicrophoneCount;

                    Status = AcpiDmDumpTable (TableLength, Offset, MicCount,
                        sizeof (ACPI_NHLT_VENDOR_MIC_COUNT), AcpiDmTableInfoNhlt6a);
                    Offset += sizeof (ACPI_NHLT_VENDOR_MIC_COUNT);
                    if (ACPI_FAILURE (Status))
                    {
                        return;
                    }

                    /* Get the vendor microphone config structure(s) */

                    for (j = 0; j < MicrophoneCount; j++)
                    {
                        AcpiOsPrintf ("\n    /* Vendor-defined microphone array #%u*/\n", j+1);
                        DevSpecific = ACPI_ADD_PTR (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_A, Table, Offset);

                        Status = AcpiDmDumpTable (TableLength, Offset, DevSpecific,
                            sizeof (ACPI_NHLT_VENDOR_MIC_CONFIG), AcpiDmTableInfoNhlt6);
                        if (ACPI_FAILURE (Status))
                        {
                            return;
                        }

                        Offset += sizeof (ACPI_NHLT_VENDOR_MIC_CONFIG);
                    }

                    /* Check for Microphone SNR and sensitivity extension */

                    if ((ArrayType & ACPI_NHLT_ARRAY_TYPE_EXT_MASK) == ACPI_NHLT_MIC_SNR_SENSITIVITY_EXT)
                    {
                        AcpiOsPrintf ("\n    /* Microphone SNR and sensitivity array */\n");
                        DevSpecific = ACPI_ADD_PTR (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_A, Table, Offset);

                        Status = AcpiDmDumpTable (TableLength, Offset, DevSpecific,
                            sizeof (ACPI_NHLT_MIC_SNR_SENSITIVITY_EXTENSION), AcpiDmTableInfoNhlt9);
                        if (ACPI_FAILURE (Status))
                        {
                            return;
                        }

                        Offset += sizeof (ACPI_NHLT_MIC_SNR_SENSITIVITY_EXTENSION);
                    }
                }
            }

            /* Do the Formats_Config table - starts with the FormatsCount field */

            FormatsConfig = ACPI_ADD_PTR (ACPI_NHLT_FORMATS_CONFIG, Table, Offset);
            FormatsCount = FormatsConfig->FormatsCount;

            AcpiOsPrintf ("\n    /* Formats_Config table */\n");

            /* Dump the FormatsCount value */

            if (FormatsCount > 0)
            {
                Status = AcpiDmDumpTable (TableLength, Offset, FormatsConfig,
                    sizeof (ACPI_NHLT_FORMATS_CONFIG), AcpiDmTableInfoNhlt4);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
            }
            Offset += sizeof (ACPI_NHLT_FORMATS_CONFIG);

            /* A variable number of Format_Config Descriptors - process each */

            for (j = 0; j < FormatsCount; j++)
            {
                FormatSubtable = ACPI_ADD_PTR (ACPI_NHLT_FORMAT_CONFIG, Table, Offset);
                CapabilitiesSize = FormatSubtable->CapabilitySize;

                /* Do the Wave_extensible struct */

                AcpiOsPrintf ("\n    /* Wave_Format_Extensible table #%u */\n", j+1);
                Status = AcpiDmDumpTable (TableLength, Offset, FormatSubtable,
                    sizeof (ACPI_NHLT_FORMAT_CONFIG), AcpiDmTableInfoNhlt3);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                Offset += sizeof (ACPI_NHLT_FORMAT_CONFIG);

                if (CapabilitiesSize > 0)
                {
                    UINT8* CapabilitiesBuf = ACPI_ADD_PTR (UINT8, Table, Offset);
                    /* Do the Capabilities array (of bytes) */

                    AcpiOsPrintf ("\n    /* Specific_Config table #%u */\n", j+1);

                    Status = AcpiDmDumpTable (TableLength, Offset, CapabilitiesBuf,
                        CapabilitiesSize, AcpiDmTableInfoNhlt3a);
                    if (ACPI_FAILURE (Status))
                    {
                        return;
                    }

                    Offset += CapabilitiesSize; /* + sizeof (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_B); */
                }

            } /* for (j = 0; j < FormatsCount; j++) */

            /*
             * If we are not done with the current Endpoint yet, then there must be
             * some non documented structure(s) yet to be processed. First, get
             * the count of such structure(s).
             */
            if (Offset < EndpointEndOffset)
            {
                AcpiOsPrintf ("\n    /* Structures that are not part of NHLT spec */\n");
                Count = ACPI_ADD_PTR (ACPI_NHLT_DEVICE_INFO_COUNT, Table, Offset);
                Status = AcpiDmDumpTable (TableLength, Offset, Count,
                    sizeof (ACPI_NHLT_DEVICE_INFO_COUNT), AcpiDmTableInfoNhlt7);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
                Offset += sizeof (ACPI_NHLT_DEVICE_INFO_COUNT);

                /* Variable number of device structures */

                for (j = 0; j < Count->StructureCount; j++)
                {
                    DeviceInfo = ACPI_ADD_PTR (ACPI_NHLT_DEVICE_INFO, Table, Offset);
                    AcpiOsPrintf ("\n    /* Device Info structure #%u (not part of NHLT spec) */\n", j+1);

                    /*
                     * Dump the following Device Info fields:
                     *  1) Device ID
                     *  2) Device Instance ID
                     *  3) Device Port ID
                     */
                    Status = AcpiDmDumpTable (TableLength, Offset, DeviceInfo,
                        sizeof (ACPI_NHLT_DEVICE_INFO), AcpiDmTableInfoNhlt7a);
                    if (ACPI_FAILURE (Status))
                    {
                        return;
                    }

                    Offset += sizeof (ACPI_NHLT_DEVICE_INFO);
                }

                /*
                 * Check that the current offset is not beyond the end of
                 * this endpoint descriptor. If it is not, print those
                 * undocumented bytes.
                 */
                if (Offset < EndpointEndOffset)
                {
                    /* Unknown data at the end of the Endpoint */
                    UINT32 size = EndpointEndOffset - Offset;
                    UINT8* buffer = ACPI_ADD_PTR (UINT8, Table, Offset);
                    AcpiOsPrintf ("\n    /* Unknown data at the end of the Endpoint, size: %X */\n", size);
                    Status = AcpiDmDumpTable (TableLength, Offset, buffer,
                        size, AcpiDmTableInfoNhlt7b);
                    Offset = EndpointEndOffset;
                }

                /* Should be at the end of the Endpoint structure. */
            }

        } /* for (i = 0; i < EndpointCount; i++) */


        /*
         * Done with all of the Endpoint Descriptors, Emit the table terminator
         * (if such a legacy structure is present -- not in NHLT specification)
         */
        if (Offset < TableLength)
        {
            Capabilities = ACPI_ADD_PTR (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_B, Table, Offset);
            AcpiOsPrintf ("\n/* Terminating specific config (not part of NHLT spec) */\n");

            Status = AcpiDmDumpTable (TableLength, Offset, Capabilities,
                sizeof (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_B), AcpiDmTableInfoNhlt5b);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
            Offset += sizeof (ACPI_NHLT_DEVICE_SPECIFIC_CONFIG_B);

            if (Capabilities->CapabilitiesSize > 0)
            {
                UINT32 remainingBytes = TableLength - Offset;
                UINT8* buffer = ACPI_ADD_PTR (UINT8, Table, Offset);

                if (remainingBytes != Capabilities->CapabilitiesSize)
                    AcpiOsPrintf ("\n/* Incorrect config size, should be %X, is %X */\n",
                        Capabilities->CapabilitiesSize, remainingBytes);
                Status = AcpiDmDumpTable (TableLength, Offset, buffer,
                        remainingBytes, AcpiDmTableInfoNhlt3a);
            }
        }

        return;
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
    ACPI_PCCT_SUBSPACE      *Subtable;
    ACPI_DMTABLE_INFO       *InfoTable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_PCCT);


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoPcct);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_PCCT_SUBSPACE, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common subtable header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Header.Length, AcpiDmTableInfoPcctHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        switch (Subtable->Header.Type)
        {
        case ACPI_PCCT_TYPE_GENERIC_SUBSPACE:

            InfoTable = AcpiDmTableInfoPcct0;
            break;

        case ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE:

            InfoTable = AcpiDmTableInfoPcct1;
            break;

        case ACPI_PCCT_TYPE_HW_REDUCED_SUBSPACE_TYPE2:

            InfoTable = AcpiDmTableInfoPcct2;
            break;

        case ACPI_PCCT_TYPE_EXT_PCC_MASTER_SUBSPACE:

            InfoTable = AcpiDmTableInfoPcct3;
            break;

        case ACPI_PCCT_TYPE_EXT_PCC_SLAVE_SUBSPACE:

            InfoTable = AcpiDmTableInfoPcct4;
            break;

        case ACPI_PCCT_TYPE_HW_REG_COMM_SUBSPACE:

            InfoTable = AcpiDmTableInfoPcct5;
            break;

        default:

            AcpiOsPrintf (
                "\n**** Unexpected or unknown PCCT subtable type 0x%X\n\n",
                Subtable->Header.Type);
            return;
        }

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            Subtable->Header.Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next subtable */

        Offset += Subtable->Header.Length;
        Subtable = ACPI_ADD_PTR (ACPI_PCCT_SUBSPACE, Subtable,
            Subtable->Header.Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpPdtt
 *
 * PARAMETERS:  Table               - A PDTT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a Pdtt. This is a variable-length
 *              table that contains an open-ended number of IDs
 *              at the end of the table.
 *
 ******************************************************************************/

void
AcpiDmDumpPdtt (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_PDTT_CHANNEL       *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_PDTT);


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoPdtt);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables. Currently there is only one type, but can be multiples */

    Subtable = ACPI_ADD_PTR (ACPI_PDTT_CHANNEL, Table, Offset);
    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            sizeof (ACPI_PDTT_CHANNEL), AcpiDmTableInfoPdtt0);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        /* Point to next subtable */

        Offset += sizeof (ACPI_PDTT_CHANNEL);
        Subtable = ACPI_ADD_PTR (ACPI_PDTT_CHANNEL, Subtable,
            sizeof (ACPI_PDTT_CHANNEL));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpPhat
 *
 * PARAMETERS:  Table               - A PHAT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a PHAT.
 *
 ******************************************************************************/

void
AcpiDmDumpPhat (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_DMTABLE_INFO       *InfoTable;
    ACPI_PHAT_HEADER        *Subtable;
    ACPI_PHAT_VERSION_DATA  *VersionData;
    UINT32                  RecordCount;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_PHAT);
    UINT32                  OriginalOffset;
    UINT32                  SubtableLength;
    UINT32                  PathLength;
    UINT32                  VendorLength;
    UINT16                  RecordType;
    const wchar_t           *WideString;


    Subtable = ACPI_ADD_PTR (ACPI_PHAT_HEADER, Table, sizeof (ACPI_TABLE_PHAT));

    while (Offset < Table->Length)
    {
        /* Common subtable header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Length, Offset, Subtable,
            sizeof (ACPI_PHAT_HEADER), AcpiDmTableInfoPhatHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        DbgPrint (ASL_DEBUG_OUTPUT, "\n/* %u, Subtable->Type %X */\n",
            __LINE__, Subtable->Type);

        switch (Subtable->Type)
        {
        case ACPI_PHAT_TYPE_FW_VERSION_DATA:

            InfoTable = AcpiDmTableInfoPhat0;
            SubtableLength = Offset += sizeof (ACPI_PHAT_VERSION_DATA);
            break;

        case ACPI_PHAT_TYPE_FW_HEALTH_DATA:

            InfoTable = AcpiDmTableInfoPhat1;
            SubtableLength = Offset += sizeof (ACPI_PHAT_TYPE_FW_HEALTH_DATA);
            break;

        default:

            DbgPrint (ASL_DEBUG_OUTPUT, "\n**** Unknown PHAT subtable type 0x%X\n\n",
                Subtable->Type);

            return;
        }

        Status = AcpiDmDumpTable (Length, SubtableLength, Subtable,
            SubtableLength, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        OriginalOffset = Offset;
        switch (Subtable->Type)
        {
        case ACPI_PHAT_TYPE_FW_VERSION_DATA:

            VersionData = ACPI_CAST_PTR (ACPI_PHAT_VERSION_DATA, Subtable);
            RecordCount = VersionData->ElementCount;
            RecordType = *ACPI_CAST_PTR (UINT8, Subtable);

            /*
             * Skip past a zero-valued block (not part of the ACPI PHAT specification).
             * First, check for a zero length record and a zero element count
             */
            if (!VersionData->Header.Length && !VersionData->ElementCount)
            {
                while (RecordType == 0)
                {
                    Subtable = ACPI_ADD_PTR (ACPI_PHAT_HEADER, Table, Offset);
                    RecordType = *ACPI_CAST_PTR (UINT8, Subtable);
                    RecordCount = VersionData->ElementCount;
                    Offset += 1;
                }

                Offset -= 1;
                AcpiOsPrintf ("\n/* Warning: Block of zeros found above starting at Offset %X Length %X */\n"
                    "/* (not compliant to PHAT specification -- ignoring block) */\n",
                    OriginalOffset - 12, Offset - OriginalOffset + 12);
            }

            DbgPrint (ASL_DEBUG_OUTPUT, "/* %u, RecordCount: %X, Offset %X, SubtableLength %X */\n",
                __LINE__, RecordCount, Offset, SubtableLength);

            /* Emit each of the version elements */

            while (RecordCount && VersionData->Header.Length)
            {
                AcpiOsPrintf ("\n/* Version Element #%Xh Offset %Xh */\n\n",
                    VersionData->ElementCount - RecordCount + 1, Offset);

                Subtable = ACPI_ADD_PTR (ACPI_PHAT_HEADER, Table, Offset);
                Status = AcpiDmDumpTable (Length, Offset, Subtable,
                    sizeof (ACPI_PHAT_VERSION_ELEMENT), AcpiDmTableInfoPhat0a);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                Offset += sizeof (ACPI_PHAT_VERSION_ELEMENT);
                RecordCount--;
            }

            break;

        case ACPI_PHAT_TYPE_FW_HEALTH_DATA:

            /*
             * Get the length of the Device Path (UEFI wide string).
             * Include the wide null terminator (+2),
             */
            WideString = ACPI_ADD_PTR (wchar_t, Subtable,
                sizeof (ACPI_PHAT_HEALTH_DATA));

            PathLength = (wcslen (WideString) * 2) + 2;
            DbgPrint (ASL_DEBUG_OUTPUT, "/* %u, PathLength %X, Offset %X, Table->Length %X */\n",
                __LINE__, PathLength, Offset, Length);

            Status = AcpiDmDumpTable (Length, Offset,
                ACPI_ADD_PTR (ACPI_PHAT_HEADER, Subtable, sizeof (ACPI_PHAT_HEALTH_DATA)),
                PathLength, AcpiDmTableInfoPhat1a);
            Offset += PathLength;
            if (ACPI_FAILURE (Status))
            {
                return;
            }

            /* Get Device-Specific Data - length of which is the remaining subtable length. */

            VendorLength =
                Subtable->Length - sizeof (ACPI_PHAT_HEALTH_DATA) - PathLength;
            DbgPrint (ASL_DEBUG_OUTPUT, "%u, Subtable->Length %X, VendorLength %X, Offset %X PathLength: %X\n",
                __LINE__, Subtable->Length, VendorLength, Offset, PathLength);

            if (VendorLength)
            {
                /* Point past the Device Path, Compile the Device-Specific Data */

                Status = AcpiDmDumpTable (Length, Offset,
                    ACPI_ADD_PTR (ACPI_PHAT_HEADER, Subtable, sizeof (ACPI_PHAT_HEALTH_DATA) + PathLength),
                    VendorLength, AcpiDmTableInfoPhat1b);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                Offset += VendorLength;
            }

            if (ACPI_FAILURE (Status))
            {
                return;
            }
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown PHAT subtable type 0x%X\n\n",
                Subtable->Type);
            return;
        }

        /* Next subtable */

        DbgPrint (ASL_DEBUG_OUTPUT, "/* %u, Bottom of main loop: Offset %X, "
            "Subtable->Length %X, Table->Length %X */\n",
            __LINE__, Offset, Subtable->Length, Table->Length);

        Subtable = ACPI_ADD_PTR (ACPI_PHAT_HEADER, Table,
            Offset);
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
    ACPI_PMTT_HEADER        *Subtable;
    UINT32                  Length = Table->Length;
    UINT32                  Offset = sizeof (ACPI_TABLE_PMTT);


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoPmtt);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_PMTT_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Each of the types below contain the common subtable header */

        AcpiOsPrintf ("\n");
        switch (Subtable->Type)
        {
        case ACPI_PMTT_TYPE_SOCKET:

            Status = AcpiDmDumpTable (Length, Offset, Subtable,
                Subtable->Length, AcpiDmTableInfoPmtt0);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
            break;

        case ACPI_PMTT_TYPE_CONTROLLER:
            Status = AcpiDmDumpTable (Length, Offset, Subtable,
                Subtable->Length, AcpiDmTableInfoPmtt1);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
            break;

       case ACPI_PMTT_TYPE_DIMM:
            Status = AcpiDmDumpTable (Length, Offset, Subtable,
                Subtable->Length, AcpiDmTableInfoPmtt2);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
            break;

        case ACPI_PMTT_TYPE_VENDOR:
            Status = AcpiDmDumpTable (Length, Offset, Subtable,
                Subtable->Length, AcpiDmTableInfoPmttVendor);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
            break;

        default:
            AcpiOsPrintf (
                "\n**** Unexpected or unknown PMTT subtable type 0x%X\n\n",
                Subtable->Type);
            return;
        }

        /* Point to next subtable */

        Offset += Subtable->Length;
        Subtable = ACPI_ADD_PTR (ACPI_PMTT_HEADER,
            Subtable, Subtable->Length);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpPptt
 *
 * PARAMETERS:  Table               - A PMTT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a PPTT. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpPptt (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_SUBTABLE_HEADER    *Subtable;
    ACPI_PPTT_PROCESSOR     *PpttProcessor;
    UINT8                   Length;
    UINT8                   SubtableOffset;
    UINT32                  Offset = sizeof (ACPI_TABLE_FPDT);
    ACPI_DMTABLE_INFO       *InfoTable;
    UINT32                  i;


    /* There is no main table (other than the standard ACPI header) */

    /* Subtables */

    Offset = sizeof (ACPI_TABLE_HEADER);
    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");

        /* Common subtable header */

        Subtable = ACPI_ADD_PTR (ACPI_SUBTABLE_HEADER, Table, Offset);
        if (Subtable->Length < sizeof (ACPI_SUBTABLE_HEADER))
        {
            AcpiOsPrintf ("Invalid subtable length\n");
            return;
        }
        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoPpttHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        switch (Subtable->Type)
        {
        case ACPI_PPTT_TYPE_PROCESSOR:

            InfoTable = AcpiDmTableInfoPptt0;
            Length = sizeof (ACPI_PPTT_PROCESSOR);
            break;

        case ACPI_PPTT_TYPE_CACHE:

            InfoTable = AcpiDmTableInfoPptt1;
            Length = sizeof (ACPI_PPTT_CACHE);
            break;

        case ACPI_PPTT_TYPE_ID:

            InfoTable = AcpiDmTableInfoPptt2;
            Length = sizeof (ACPI_PPTT_ID);
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown PPTT subtable type 0x%X\n\n",
                Subtable->Type);

            /* Attempt to continue */

            goto NextSubtable;
        }

        if (Subtable->Length < Length)
        {
            AcpiOsPrintf ("Invalid subtable length\n");
            return;
        }
        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable,
            Subtable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }
        SubtableOffset = Length;

        switch (Subtable->Type)
        {
        case ACPI_PPTT_TYPE_PROCESSOR:

            PpttProcessor = ACPI_CAST_PTR (ACPI_PPTT_PROCESSOR, Subtable);

            /* Dump SMBIOS handles */

            if ((UINT8)(Subtable->Length - SubtableOffset) <
                (UINT8)(PpttProcessor->NumberOfPrivResources * 4))
            {
                AcpiOsPrintf ("Invalid private resource number\n");
                return;
            }
            for (i = 0; i < PpttProcessor->NumberOfPrivResources; i++)
            {
                Status = AcpiDmDumpTable (Table->Length, Offset + SubtableOffset,
                    ACPI_ADD_PTR (ACPI_SUBTABLE_HEADER, Subtable, SubtableOffset),
                    4, AcpiDmTableInfoPptt0a);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                SubtableOffset += 4;
            }
            break;

        case ACPI_PPTT_TYPE_CACHE:

            if (Table->Revision < 3)
            {
                break;
            }
            Status = AcpiDmDumpTable (Table->Length, Offset + SubtableOffset,
                ACPI_ADD_PTR (ACPI_SUBTABLE_HEADER, Subtable, SubtableOffset),
                sizeof (ACPI_PPTT_CACHE_V1), AcpiDmTableInfoPptt1a);
            if (ACPI_FAILURE (Status))
            {
                return;
            }
            break;

        default:

            break;
        }

NextSubtable:
        /* Point to next subtable */

        Offset += Subtable->Length;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpPrmt
 *
 * PARAMETERS:  Table               - A PRMT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a PRMT. This table type consists
 *              of an open-ended number of subtables.
 *
 ******************************************************************************/

void
AcpiDmDumpPrmt (
    ACPI_TABLE_HEADER       *Table)
{
    UINT32                  CurrentOffset = sizeof (ACPI_TABLE_HEADER);
    ACPI_TABLE_PRMT_HEADER  *PrmtHeader;
    ACPI_PRMT_MODULE_INFO   *PrmtModuleInfo;
    ACPI_PRMT_HANDLER_INFO  *PrmtHandlerInfo;
    ACPI_STATUS             Status;
    UINT32                  i, j;


    /* Main table header */

    PrmtHeader = ACPI_ADD_PTR (ACPI_TABLE_PRMT_HEADER, Table, CurrentOffset);
    Status = AcpiDmDumpTable (Table->Length, CurrentOffset, PrmtHeader,
        sizeof (ACPI_TABLE_PRMT_HEADER), AcpiDmTableInfoPrmtHdr);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Invalid PRMT header\n");
        return;
    }

    CurrentOffset += sizeof (ACPI_TABLE_PRMT_HEADER);

    /* PRM Module Information Structure array */

    for (i = 0; i < PrmtHeader->ModuleInfoCount; ++i)
    {
        PrmtModuleInfo = ACPI_ADD_PTR (ACPI_PRMT_MODULE_INFO, Table, CurrentOffset);
        Status = AcpiDmDumpTable (Table->Length, CurrentOffset, PrmtModuleInfo,
            sizeof (ACPI_PRMT_MODULE_INFO), AcpiDmTableInfoPrmtModule);

        CurrentOffset += sizeof (ACPI_PRMT_MODULE_INFO);

        /* PRM handler information structure array */

        for (j = 0; j < PrmtModuleInfo->HandlerInfoCount; ++j)
        {
            PrmtHandlerInfo = ACPI_ADD_PTR (ACPI_PRMT_HANDLER_INFO, Table, CurrentOffset);
            Status = AcpiDmDumpTable (Table->Length, CurrentOffset, PrmtHandlerInfo,
                sizeof (ACPI_PRMT_HANDLER_INFO), AcpiDmTableInfoPrmtHandler);

            CurrentOffset += sizeof (ACPI_PRMT_HANDLER_INFO);
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpRgrt
 *
 * PARAMETERS:  Table               - A RGRT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a RGRT
 *
 ******************************************************************************/

void
AcpiDmDumpRgrt (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_TABLE_RGRT         *Subtable = ACPI_CAST_PTR (ACPI_TABLE_RGRT, Table);
    UINT32                  Offset = sizeof (ACPI_TABLE_RGRT);


    /* Main table */

    Status = AcpiDmDumpTable (Table->Length, 0, Table, 0, AcpiDmTableInfoRgrt);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Dump the binary image as a subtable */

    Status = AcpiDmDumpTable (Table->Length, Offset, &Subtable->Image,
        Table->Length - Offset, AcpiDmTableInfoRgrt0);
    if (ACPI_FAILURE (Status))
    {
        return;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpRhct
 *
 * PARAMETERS:  Table               - A RHCT table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a RHCT.
 *
 ******************************************************************************/

void
AcpiDmDumpRhct (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status;
    ACPI_RHCT_NODE_HEADER   *Subtable;
    ACPI_RHCT_HART_INFO     *RhctHartInfo;
    ACPI_RHCT_ISA_STRING    *RhctIsaString;
    ACPI_RHCT_CMO_NODE      *RhctCmoNode;
    ACPI_RHCT_MMU_NODE      *RhctMmuNode;
    UINT32                  Length = Table->Length;
    UINT8                   SubtableOffset, IsaPadOffset;
    UINT32                  Offset = sizeof (ACPI_TABLE_RHCT);
    UINT32                  i;

    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoRhct);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    while (Offset < Table->Length)
    {
        AcpiOsPrintf ("\n");

        /* Common subtable header */

        Subtable = ACPI_ADD_PTR (ACPI_RHCT_NODE_HEADER, Table, Offset);
        if (Subtable->Length < sizeof (ACPI_RHCT_NODE_HEADER))
        {
            AcpiOsPrintf ("Invalid subtable length\n");
            return;
        }
        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoRhctNodeHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        Length = sizeof (ACPI_RHCT_NODE_HEADER);

        if (Subtable->Length < Length)
        {
            AcpiOsPrintf ("Invalid subtable length\n");
            return;
        }
        SubtableOffset = (UINT8) Length;

        switch (Subtable->Type)
        {
        case ACPI_RHCT_NODE_TYPE_HART_INFO:
            Status = AcpiDmDumpTable (Table->Length, Offset + SubtableOffset,
                    ACPI_ADD_PTR (ACPI_RHCT_HART_INFO, Subtable, SubtableOffset),
                    sizeof (ACPI_RHCT_HART_INFO), AcpiDmTableInfoRhctHartInfo1);

            RhctHartInfo = ACPI_ADD_PTR (ACPI_RHCT_HART_INFO, Subtable, SubtableOffset);

            if ((UINT16)(Subtable->Length - SubtableOffset) <
                (UINT16)(RhctHartInfo->NumOffsets * 4))
            {
                AcpiOsPrintf ("Invalid number of offsets\n");
                return;
            }
            SubtableOffset += sizeof (ACPI_RHCT_HART_INFO);
            for (i = 0; i < RhctHartInfo->NumOffsets; i++)
            {
                Status = AcpiDmDumpTable (Table->Length, Offset + SubtableOffset,
                    ACPI_ADD_PTR (UINT32, Subtable, SubtableOffset),
                    4, AcpiDmTableInfoRhctHartInfo2);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                SubtableOffset += 4;
            }
            break;

        case ACPI_RHCT_NODE_TYPE_ISA_STRING:
            RhctIsaString = ACPI_ADD_PTR (ACPI_RHCT_ISA_STRING, Subtable, SubtableOffset);
            IsaPadOffset = (UINT8) (SubtableOffset + 2 + RhctIsaString->IsaLength);
            Status = AcpiDmDumpTable (Table->Length, Offset + SubtableOffset,
                    RhctIsaString, RhctIsaString->IsaLength, AcpiDmTableInfoRhctIsa1);
            if (Subtable->Length > IsaPadOffset)
            {
                Status = AcpiDmDumpTable (Table->Length, Offset + SubtableOffset,
                         ACPI_ADD_PTR (UINT8, Subtable, IsaPadOffset),
                         (Subtable->Length - IsaPadOffset), AcpiDmTableInfoRhctIsaPad);
            }

            break;

        case ACPI_RHCT_NODE_TYPE_CMO:
            RhctCmoNode = ACPI_ADD_PTR (ACPI_RHCT_CMO_NODE, Subtable, SubtableOffset);
            Status = AcpiDmDumpTable (Table->Length, Offset + SubtableOffset,
                                      RhctCmoNode, 4, AcpiDmTableInfoRhctCmo1);
            break;

        case ACPI_RHCT_NODE_TYPE_MMU:
            RhctMmuNode = ACPI_ADD_PTR (ACPI_RHCT_MMU_NODE, Subtable, SubtableOffset);
            Status = AcpiDmDumpTable (Table->Length, Offset + SubtableOffset,
                                      RhctMmuNode, 2, AcpiDmTableInfoRhctMmu1);
            break;

        default:
            break;
        }

        /* Point to next subtable */

        Offset += Subtable->Length;
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
    ACPI_FPDT_HEADER        *Subtable;
    ACPI_DMTABLE_INFO       *InfoTable;
    ACPI_TABLE_S3PT         *S3ptTable = ACPI_CAST_PTR (ACPI_TABLE_S3PT, Tables);


    /* Main table */

    Status = AcpiDmDumpTable (Offset, 0, S3ptTable, 0, AcpiDmTableInfoS3pt);
    if (ACPI_FAILURE (Status))
    {
        return 0;
    }

    Subtable = ACPI_ADD_PTR (ACPI_FPDT_HEADER, S3ptTable, Offset);
    while (Offset < S3ptTable->Length)
    {
        /* Common subtable header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (S3ptTable->Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoS3ptHdr);
        if (ACPI_FAILURE (Status))
        {
            return 0;
        }

        switch (Subtable->Type)
        {
        case ACPI_S3PT_TYPE_RESUME:

            InfoTable = AcpiDmTableInfoS3pt0;
            break;

        case ACPI_S3PT_TYPE_SUSPEND:

            InfoTable = AcpiDmTableInfoS3pt1;
            break;

        default:

            AcpiOsPrintf ("\n**** Unknown S3PT subtable type 0x%X\n",
                Subtable->Type);

            /* Attempt to continue */

            if (!Subtable->Length)
            {
                AcpiOsPrintf ("Invalid zero length subtable\n");
                return 0;
            }
            goto NextSubtable;
        }

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (S3ptTable->Length, Offset, Subtable,
            Subtable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return 0;
        }

NextSubtable:
        /* Point to next subtable */

        Offset += Subtable->Length;
        Subtable = ACPI_ADD_PTR (ACPI_FPDT_HEADER, Subtable, Subtable->Length);
    }

    return (S3ptTable->Length);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpSdev
 *
 * PARAMETERS:  Table               - A SDEV table
 *
 * RETURN:      None
 *
 * DESCRIPTION: Format the contents of a SDEV. This is a variable-length
 *              table that contains variable strings and vendor data.
 *
 ******************************************************************************/

void
AcpiDmDumpSdev (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS                 Status;
    ACPI_SDEV_HEADER            *Subtable;
    ACPI_SDEV_PCIE              *Pcie;
    ACPI_SDEV_NAMESPACE         *Namesp;
    ACPI_DMTABLE_INFO           *InfoTable;
    ACPI_DMTABLE_INFO           *SecureComponentInfoTable;
    UINT32                      Length = Table->Length;
    UINT32                      Offset = sizeof (ACPI_TABLE_SDEV);
    UINT16                      PathOffset;
    UINT16                      PathLength;
    UINT16                      VendorDataOffset;
    UINT16                      VendorDataLength;
    ACPI_SDEV_SECURE_COMPONENT  *SecureComponent = NULL;
    UINT32                      CurrentOffset = 0;


    /* Main table */

    Status = AcpiDmDumpTable (Length, 0, Table, 0, AcpiDmTableInfoSdev);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* Subtables */

    Subtable = ACPI_ADD_PTR (ACPI_SDEV_HEADER, Table, Offset);
    while (Offset < Table->Length)
    {
        /* Common subtable header */

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, Offset, Subtable,
            Subtable->Length, AcpiDmTableInfoSdevHdr);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        switch (Subtable->Type)
        {
        case ACPI_SDEV_TYPE_NAMESPACE_DEVICE:

            InfoTable = AcpiDmTableInfoSdev0;
            break;

        case ACPI_SDEV_TYPE_PCIE_ENDPOINT_DEVICE:

            InfoTable = AcpiDmTableInfoSdev1;
            break;

        default:
            goto NextSubtable;
        }

        AcpiOsPrintf ("\n");
        Status = AcpiDmDumpTable (Table->Length, 0, Subtable,
            Subtable->Length, InfoTable);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        switch (Subtable->Type)
        {
        case ACPI_SDEV_TYPE_NAMESPACE_DEVICE:

            CurrentOffset = sizeof (ACPI_SDEV_NAMESPACE);
            if (Subtable->Flags & ACPI_SDEV_SECURE_COMPONENTS_PRESENT)
            {
                SecureComponent = ACPI_CAST_PTR (ACPI_SDEV_SECURE_COMPONENT,
                    ACPI_ADD_PTR (UINT8, Subtable, sizeof (ACPI_SDEV_NAMESPACE)));

                Status = AcpiDmDumpTable (Table->Length, CurrentOffset,
                    ACPI_ADD_PTR(UINT8, Subtable, sizeof (ACPI_SDEV_NAMESPACE)),
                    sizeof (ACPI_SDEV_SECURE_COMPONENT), AcpiDmTableInfoSdev0b);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
                CurrentOffset += sizeof (ACPI_SDEV_SECURE_COMPONENT);

                Status = AcpiDmDumpTable (Table->Length, CurrentOffset,
                    ACPI_ADD_PTR(UINT8, Subtable, SecureComponent->SecureComponentOffset),
                    sizeof (ACPI_SDEV_HEADER), AcpiDmTableInfoSdevSecCompHdr);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
                CurrentOffset += sizeof (ACPI_SDEV_HEADER);

                switch (Subtable->Type)
                {
                case ACPI_SDEV_TYPE_ID_COMPONENT:

                    SecureComponentInfoTable = AcpiDmTableInfoSdevSecCompId;
                    break;

                case ACPI_SDEV_TYPE_MEM_COMPONENT:

                    SecureComponentInfoTable = AcpiDmTableInfoSdevSecCompMem;
                    break;

                default:
                    goto NextSubtable;
                }

                Status = AcpiDmDumpTable (Table->Length, CurrentOffset,
                    ACPI_ADD_PTR(UINT8, Subtable, SecureComponent->SecureComponentOffset),
                    SecureComponent->SecureComponentLength, SecureComponentInfoTable);
                CurrentOffset += SecureComponent->SecureComponentLength;
            }

            /* Dump the PCIe device ID(s) */

            Namesp = ACPI_CAST_PTR (ACPI_SDEV_NAMESPACE, Subtable);
            PathOffset = Namesp->DeviceIdOffset;
            PathLength = Namesp->DeviceIdLength;

            if (PathLength)
            {
                Status = AcpiDmDumpTable (Table->Length, CurrentOffset,
                    ACPI_ADD_PTR (UINT8, Namesp, PathOffset),
                    PathLength, AcpiDmTableInfoSdev0a);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
                CurrentOffset += PathLength;
            }

            /* Dump the vendor-specific data */

            VendorDataLength =
                Namesp->VendorDataLength;
            VendorDataOffset =
                Namesp->DeviceIdOffset + Namesp->DeviceIdLength;

            if (VendorDataLength)
            {
                Status = AcpiDmDumpTable (Table->Length, 0,
                    ACPI_ADD_PTR (UINT8, Namesp, VendorDataOffset),
                    VendorDataLength, AcpiDmTableInfoSdev1b);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
            }
            break;

        case ACPI_SDEV_TYPE_PCIE_ENDPOINT_DEVICE:

            /* PCI path substructures */

            Pcie = ACPI_CAST_PTR (ACPI_SDEV_PCIE, Subtable);
            PathOffset = Pcie->PathOffset;
            PathLength = Pcie->PathLength;

            while (PathLength)
            {
                Status = AcpiDmDumpTable (Table->Length,
                    PathOffset + Offset,
                    ACPI_ADD_PTR (UINT8, Pcie, PathOffset),
                    sizeof (ACPI_SDEV_PCIE_PATH), AcpiDmTableInfoSdev1a);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }

                PathOffset += sizeof (ACPI_SDEV_PCIE_PATH);
                PathLength -= sizeof (ACPI_SDEV_PCIE_PATH);
            }

            /* VendorData */

            VendorDataLength = Pcie->VendorDataLength;
            VendorDataOffset = Pcie->PathOffset + Pcie->PathLength;

            if (VendorDataLength)
            {
                Status = AcpiDmDumpTable (Table->Length, 0,
                    ACPI_ADD_PTR (UINT8, Pcie, VendorDataOffset),
                    VendorDataLength, AcpiDmTableInfoSdev1b);
                if (ACPI_FAILURE (Status))
                {
                    return;
                }
            }
            break;

        default:
            goto NextSubtable;
        }

NextSubtable:
        /* Point to next subtable */

        Offset += Subtable->Length;
        Subtable = ACPI_ADD_PTR (ACPI_SDEV_HEADER, Subtable,
            Subtable->Length);
    }
}
