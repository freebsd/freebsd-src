/******************************************************************************
 *
 * Module Name: asloffset - Generate a C "offset table" for BIOS use.
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

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acnamesp.h>


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("asloffset")


/* Local prototypes */

static void
LsEmitOffsetTableEntry (
    UINT32                  FileId,
    ACPI_NAMESPACE_NODE     *Node,
    UINT32                  Offset,
    UINT32                  Length,
    char                    *OpName,
    UINT64                  Value,
    UINT8                   AmlOpcode);


/*******************************************************************************
 *
 * FUNCTION:    LsAmlOffsetWalk
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Process one node during a offset table file generation.
 *
 * Three types of objects are currently emitted to the offset table:
 *   1) Tagged (named) resource descriptors
 *   2) Named integer objects with constant integer values
 *   3) Operation Regions that have constant Offset (address) parameters
 *
 * The offset table allows the BIOS to dynamically update the values of these
 * objects at boot time.
 *
 ******************************************************************************/

ACPI_STATUS
LsAmlOffsetWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    UINT32                  FileId = (UINT32) ACPI_TO_INTEGER (Context);
    ACPI_NAMESPACE_NODE     *Node;
    UINT32                  Length;
    UINT32                  OffsetOfOpcode;
    ACPI_PARSE_OBJECT       *AddressOp;


    /* Ignore actual data blocks for resource descriptors */

    if (Op->Asl.CompileFlags & NODE_IS_RESOURCE_DATA)
    {
        return (AE_OK); /* Do NOT update the global AML offset */
    }

    /* We are only interested in named objects (have a namespace node) */

    Node = Op->Asl.Node;
    if (!Node)
    {
        Gbl_CurrentAmlOffset += Op->Asl.FinalAmlLength;
        return (AE_OK);
    }

    /* Named resource descriptor (has a descriptor tag) */

    if ((Node->Type == ACPI_TYPE_LOCAL_RESOURCE) &&
        (Op->Asl.CompileFlags & NODE_IS_RESOURCE_DESC))
    {
        LsEmitOffsetTableEntry (FileId, Node, Gbl_CurrentAmlOffset,
            Op->Asl.FinalAmlLength, Op->Asl.ParseOpName, 0, Op->Asl.Extra);
    }

    /* Named object -- Name (NameString, DataRefObject) */

    else if (Op->Asl.AmlOpcode == AML_NAME_OP)
    {
        if (!Op->Asl.Child)
        {
            FlPrintFile (FileId, "%s NO CHILD!\n", MsgBuffer);
            return (AE_OK);
        }

        Length = Op->Asl.FinalAmlLength;

        /* Get to the NameSeg/NamePath Op (and length of the name) */

        Op = Op->Asl.Child;
        OffsetOfOpcode = Length + Op->Asl.FinalAmlLength;

        /* Get actual value associated with the name */

        Op = Op->Asl.Next;
        switch (Op->Asl.AmlOpcode)
        {
        /*
         * We are only interested in integer constants that can be changed
         * at boot time. Note, the One/Ones/Zero opcodes are considered
         * non-changeable, so we ignore them here.
         */
        case AML_BYTE_OP:
        case AML_WORD_OP:
        case AML_DWORD_OP:
        case AML_QWORD_OP:

            /* The +1/-1 is to handle the integer size prefix (opcode) */

            LsEmitOffsetTableEntry (FileId, Node,
                (Gbl_CurrentAmlOffset + OffsetOfOpcode + 1),
                (Op->Asl.FinalAmlLength - 1), Op->Asl.ParseOpName,
                Op->Asl.Value.Integer, (UINT8) Op->Asl.AmlOpcode);
            break;

        default:
            break;
        }

        Gbl_CurrentAmlOffset += Length;
        return (AE_OK);
    }

    /* OperationRegion (NameString, RegionSpace, RegionOffset, RegionLength) */

    else if (Op->Asl.AmlOpcode == AML_REGION_OP)
    {
        Length = Op->Asl.FinalAmlLength;

        /* Get the name/namepath node */

        AddressOp = Op->Asl.Child;
        OffsetOfOpcode = Length + AddressOp->Asl.FinalAmlLength + 1;

        /* Get the SpaceId node, then the Offset (address) node */

        AddressOp = AddressOp->Asl.Next;
        AddressOp = AddressOp->Asl.Next;

        switch (AddressOp->Asl.AmlOpcode)
        {
        /*
         * We are only interested in integer constants that can be changed
         * at boot time. Note, the One/Ones/Zero opcodes are considered
         * non-changeable, so we ignore them here.
         */
        case AML_BYTE_OP:
        case AML_WORD_OP:
        case AML_DWORD_OP:
        case AML_QWORD_OP:

            /* The +1/-1 is to handle the integer size prefix (opcode) */

            LsEmitOffsetTableEntry (FileId, Node,
                (Gbl_CurrentAmlOffset + OffsetOfOpcode + 1),
                (AddressOp->Asl.FinalAmlLength - 1), Op->Asl.ParseOpName,
                AddressOp->Asl.Value.Integer, (UINT8) AddressOp->Asl.AmlOpcode);

            Gbl_CurrentAmlOffset += Length;
            return (AE_OK);

        default:
            break;
        }
    }

    Gbl_CurrentAmlOffset += Op->Asl.FinalAmlLength;
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    LsEmitOffsetTableEntry
 *
 * PARAMETERS:  FileId          - ID of current listing file
 *              Node            - Namespace node associated with the name
 *              Offset          - Offset of the value within the AML table
 *              Length          - Length in bytes of the value
 *              OpName          - Name of the AML opcode
 *              Value           - Current value of the AML field
 *              AmlOpcode       - Opcode associated with the field
 *
 * RETURN:      None
 *
 * DESCRIPTION: Emit a line of the offset table (-so option)
 *
 ******************************************************************************/

static void
LsEmitOffsetTableEntry (
    UINT32                  FileId,
    ACPI_NAMESPACE_NODE     *Node,
    UINT32                  Offset,
    UINT32                  Length,
    char                    *OpName,
    UINT64                  Value,
    UINT8                   AmlOpcode)
{
    ACPI_BUFFER             TargetPath;
    ACPI_STATUS             Status;


    /* Get the full pathname to the namespace node */

    TargetPath.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
    Status = AcpiNsHandleToPathname (Node, &TargetPath);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* [1] - Skip the opening backslash for the path */

    strcpy (MsgBuffer, "\"");
    strcat (MsgBuffer, &((char *) TargetPath.Pointer)[1]);
    strcat (MsgBuffer, "\",");
    ACPI_FREE (TargetPath.Pointer);

    /*
     * Max offset is 4G, constrained by 32-bit ACPI table length.
     * Max Length for Integers is 8 bytes.
     */
    FlPrintFile (FileId,
        "    {%-29s 0x%8.8X, 0x%2.2X, 0x%8.8X%8.8X}, /* %s */\n",
        MsgBuffer, Offset, AmlOpcode, ACPI_FORMAT_UINT64 (Value), OpName);
}


/*******************************************************************************
 *
 * FUNCTION:    LsDoOffsetTableHeader, LsDoOffsetTableFooter
 *
 * PARAMETERS:  FileId          - ID of current listing file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Header and footer for the offset table file.
 *
 ******************************************************************************/

void
LsDoOffsetTableHeader (
    UINT32                  FileId)
{

    Gbl_CurrentAmlOffset = 0;

    FlPrintFile (FileId,
        "#ifndef __AML_OFFSET_TABLE_H\n"
        "#define __AML_OFFSET_TABLE_H\n\n");

    FlPrintFile (FileId, "typedef struct {\n"
        "    char                   *Pathname;\n"
        "    unsigned long          Offset;\n"
        "    unsigned char          AmlOpcode;\n"
        "    unsigned long long     AmlValue;\n"
        "} AML_OFFSET_TABLE_ENTRY;\n\n");

    FlPrintFile (FileId,
        "#endif /* __AML_OFFSET_TABLE_H */\n\n");

    FlPrintFile (FileId,
        "AML_OFFSET_TABLE_ENTRY   %s_%s_OffsetTable[] =\n{\n",
        Gbl_TableSignature, Gbl_TableId);
}


void
LsDoOffsetTableFooter (
    UINT32                  FileId)
{

    FlPrintFile (FileId,
        "    {0,0,0,0} /* Table terminator */\n};\n\n");
    Gbl_CurrentAmlOffset = 0;
}
