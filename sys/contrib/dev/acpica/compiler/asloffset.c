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
    UINT32                  NamepathOffset,
    UINT32                  Offset,
    char                    *OpName,
    UINT64                  Value,
    UINT8                   AmlOpcode,
    UINT16                  ParentOpcode);


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
 *   3) Named package objects
 *   4) Operation Regions that have constant Offset (address) parameters
 *   5) Control methods
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
    UINT32                  NamepathOffset;
    UINT32                  DataOffset;
    ACPI_PARSE_OBJECT       *NextOp;


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
        LsEmitOffsetTableEntry (FileId, Node, 0, Gbl_CurrentAmlOffset,
            Op->Asl.ParseOpName, 0, Op->Asl.Extra, AML_BUFFER_OP);

        Gbl_CurrentAmlOffset += Op->Asl.FinalAmlLength;
        return (AE_OK);
    }

    switch (Op->Asl.AmlOpcode)
    {
    case AML_NAME_OP:

        /* Named object -- Name (NameString, DataRefObject) */

        if (!Op->Asl.Child)
        {
            FlPrintFile (FileId, "%s NO CHILD!\n", MsgBuffer);
            return (AE_OK);
        }

        Length = Op->Asl.FinalAmlLength;
        NamepathOffset = Gbl_CurrentAmlOffset + Length;

        /* Get to the NameSeg/NamePath Op (and length of the name) */

        Op = Op->Asl.Child;

        /* Get offset of last nameseg and the actual data */

        NamepathOffset = Gbl_CurrentAmlOffset + Length +
            (Op->Asl.FinalAmlLength - ACPI_NAME_SIZE);

        DataOffset = Gbl_CurrentAmlOffset + Length +
            Op->Asl.FinalAmlLength;

        /* Get actual value associated with the name */

        Op = Op->Asl.Next;
        switch (Op->Asl.AmlOpcode)
        {
        case AML_BYTE_OP:
        case AML_WORD_OP:
        case AML_DWORD_OP:
        case AML_QWORD_OP:

            /* The +1 is to handle the integer size prefix (opcode) */

            LsEmitOffsetTableEntry (FileId, Node, NamepathOffset, (DataOffset + 1),
                Op->Asl.ParseOpName, Op->Asl.Value.Integer,
                (UINT8) Op->Asl.AmlOpcode, AML_NAME_OP);
            break;

        case AML_ONE_OP:
        case AML_ONES_OP:
        case AML_ZERO_OP:

            /* For these, offset will point to the opcode */

            LsEmitOffsetTableEntry (FileId, Node, NamepathOffset, DataOffset,
                Op->Asl.ParseOpName, Op->Asl.Value.Integer,
                (UINT8) Op->Asl.AmlOpcode, AML_NAME_OP);
            break;

        case AML_PACKAGE_OP:
        case AML_VAR_PACKAGE_OP:

            /* Get the package element count */

            NextOp = Op->Asl.Child;

            LsEmitOffsetTableEntry (FileId, Node, NamepathOffset, DataOffset,
                Op->Asl.ParseOpName, NextOp->Asl.Value.Integer,
                (UINT8) Op->Asl.AmlOpcode, AML_NAME_OP);
            break;

         default:
             break;
        }

        Gbl_CurrentAmlOffset += Length;
        return (AE_OK);

    case AML_REGION_OP:

        /* OperationRegion (NameString, RegionSpace, RegionOffset, RegionLength) */

        Length = Op->Asl.FinalAmlLength;

        /* Get the name/namepath node */

        NextOp = Op->Asl.Child;

        /* Get offset of last nameseg and the actual data */

        NamepathOffset = Gbl_CurrentAmlOffset + Length +
            (NextOp->Asl.FinalAmlLength - ACPI_NAME_SIZE);

        DataOffset = Gbl_CurrentAmlOffset + Length +
            (NextOp->Asl.FinalAmlLength + 1);

        /* Get the SpaceId node, then the Offset (address) node */

        NextOp = NextOp->Asl.Next;
        NextOp = NextOp->Asl.Next;

        switch (NextOp->Asl.AmlOpcode)
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

            LsEmitOffsetTableEntry (FileId, Node, NamepathOffset, (DataOffset + 1),
                Op->Asl.ParseOpName, NextOp->Asl.Value.Integer,
                (UINT8) NextOp->Asl.AmlOpcode, AML_REGION_OP);

            Gbl_CurrentAmlOffset += Length;
            return (AE_OK);

        default:
            break;
        }
        break;

    case AML_METHOD_OP:

        /* Method (Namepath, ...) */

        Length = Op->Asl.FinalAmlLength;

        /* Get the NameSeg/NamePath Op */

        NextOp = Op->Asl.Child;

        /* Get offset of last nameseg and the actual data (flags byte) */

        NamepathOffset = Gbl_CurrentAmlOffset + Length +
            (NextOp->Asl.FinalAmlLength - ACPI_NAME_SIZE);

        DataOffset = Gbl_CurrentAmlOffset + Length +
            NextOp->Asl.FinalAmlLength;

        /* Get the flags byte Op */

        NextOp = NextOp->Asl.Next;

        LsEmitOffsetTableEntry (FileId, Node, NamepathOffset, DataOffset,
            Op->Asl.ParseOpName, NextOp->Asl.Value.Integer,
            (UINT8) Op->Asl.AmlOpcode, AML_METHOD_OP);
        break;

    case AML_PROCESSOR_OP:

        /* Processor (Namepath, ProcessorId, Address, Length) */

        Length = Op->Asl.FinalAmlLength;
        NextOp = Op->Asl.Child;     /* Get Namepath */

        /* Get offset of last nameseg and the actual data (PBlock address) */

        NamepathOffset = Gbl_CurrentAmlOffset + Length +
            (NextOp->Asl.FinalAmlLength - ACPI_NAME_SIZE);

        DataOffset = Gbl_CurrentAmlOffset + Length +
            (NextOp->Asl.FinalAmlLength + 1);

        NextOp = NextOp->Asl.Next;  /* Get ProcessorID (BYTE) */
        NextOp = NextOp->Asl.Next;  /* Get Address (DWORD) */

        LsEmitOffsetTableEntry (FileId, Node, NamepathOffset, DataOffset,
            Op->Asl.ParseOpName, NextOp->Asl.Value.Integer,
            (UINT8) AML_DWORD_OP, AML_PROCESSOR_OP);
        break;

    case AML_DEVICE_OP:
    case AML_SCOPE_OP:
    case AML_THERMAL_ZONE_OP:

        /* Device/Scope/ThermalZone (Namepath) */

        Length = Op->Asl.FinalAmlLength;
        NextOp = Op->Asl.Child;     /* Get Namepath */

        /* Get offset of last nameseg */

        NamepathOffset = Gbl_CurrentAmlOffset + Length +
            (NextOp->Asl.FinalAmlLength - ACPI_NAME_SIZE);

        LsEmitOffsetTableEntry (FileId, Node, NamepathOffset, 0,
            Op->Asl.ParseOpName, 0, (UINT8) 0, Op->Asl.AmlOpcode);
        break;

    default:
        break;
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
 *              OpName          - Name of the AML opcode
 *              Value           - Current value of the AML field
 *              AmlOpcode       - Opcode associated with the field
 *              ObjectType      - ACPI object type
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
    UINT32                  NamepathOffset,
    UINT32                  Offset,
    char                    *OpName,
    UINT64                  Value,
    UINT8                   AmlOpcode,
    UINT16                  ParentOpcode)
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
        "    {%-29s 0x%4.4X, 0x%8.8X, 0x%2.2X, 0x%8.8X, 0x%8.8X%8.8X}, /* %s */\n",
        MsgBuffer, ParentOpcode, NamepathOffset, AmlOpcode,
        Offset, ACPI_FORMAT_UINT64 (Value), OpName);
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

    FlPrintFile (FileId,
        "#ifndef __AML_OFFSET_TABLE_H\n"
        "#define __AML_OFFSET_TABLE_H\n\n");

    FlPrintFile (FileId, "typedef struct {\n"
        "    char                   *Pathname;      /* Full pathname (from root) to the object */\n"
        "    unsigned short         ParentOpcode;   /* AML opcode for the parent object */\n"
        "    unsigned long          NamesegOffset;  /* Offset of last nameseg in the parent namepath */\n"
        "    unsigned char          Opcode;         /* AML opcode for the data */\n"
        "    unsigned long          Offset;         /* Offset for the data */\n"
        "    unsigned long long     Value;          /* Original value of the data (as applicable) */\n"
        "} AML_OFFSET_TABLE_ENTRY;\n\n");

    FlPrintFile (FileId,
        "#endif /* __AML_OFFSET_TABLE_H */\n\n");

    FlPrintFile (FileId,
        "/*\n"
        " * Information specific to the supported object types:\n"
        " *\n"
        " * Integers:\n"
        " *    Opcode is the integer prefix, indicates length of the data\n"
        " *        (One of: BYTE, WORD, DWORD, QWORD, ZERO, ONE, ONES)\n"
        " *    Offset points to the actual integer data\n"
        " *    Value is the existing value in the AML\n"
        " *\n"
        " * Packages:\n"
        " *    Opcode is the package or var_package opcode\n"
        " *    Offset points to the package opcode\n"
        " *    Value is the package element count\n"
        " *\n"
        " * Operation Regions:\n"
        " *    Opcode is the address integer prefix, indicates length of the data\n"
        " *    Offset points to the region address\n"
        " *    Value is the existing address value in the AML\n"
        " *\n"
        " * Control Methods:\n"
        " *    Offset points to the method flags byte\n"
        " *    Value is the existing flags value in the AML\n"
        " *\n"
        " * Processors:\n"
        " *    Offset points to the first byte of the PBlock Address\n"
        " *\n"
        " * Resource Descriptors:\n"
        " *    Opcode is the descriptor type\n"
        " *    Offset points to the start of the descriptor\n"
        " *\n"
        " * Scopes/Devices/ThermalZones:\n"
        " *    Nameseg offset only\n"
        " */\n");

    FlPrintFile (FileId,
        "AML_OFFSET_TABLE_ENTRY   %s_%s_OffsetTable[] =\n{\n",
        Gbl_TableSignature, Gbl_TableId);
}


void
LsDoOffsetTableFooter (
    UINT32                  FileId)
{

    FlPrintFile (FileId,
        "    {NULL,0,0,0,0,0} /* Table terminator */\n};\n\n");
    Gbl_CurrentAmlOffset = 0;
}
