/******************************************************************************
 *
 * Module Name: aslopcode - AML opcode generation
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2015, Intel Corp.
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

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslopcodes")


/* Local prototypes */

static void
OpcDoAccessAs (
    ACPI_PARSE_OBJECT       *Op);

static void
OpcDoConnection (
    ACPI_PARSE_OBJECT       *Op);

static void
OpcDoUnicode (
    ACPI_PARSE_OBJECT       *Op);

static void
OpcDoEisaId (
    ACPI_PARSE_OBJECT       *Op);

static void
OpcDoPld (
    ACPI_PARSE_OBJECT       *Op);

static void
OpcDoUuId (
    ACPI_PARSE_OBJECT       *Op);

static UINT8 *
OpcEncodePldBuffer (
    ACPI_PLD_INFO           *PldInfo);


/* ToPld strings */

static char *AslPldPanelList[] =
{
    "TOP",
    "BOTTOM",
    "LEFT",
    "RIGHT",
    "FRONT",
    "BACK",
    "UNKNOWN",
    NULL
};

static char *AslPldVerticalPositionList[] =
{
    "UPPER",
    "CENTER",
    "LOWER",
    NULL
};

static char *AslPldHorizontalPositionList[] =
{
    "LEFT",
    "CENTER",
    "RIGHT",
    NULL
};

static char *AslPldShapeList[] =
{
    "ROUND",
    "OVAL",
    "SQUARE",
    "VERTICALRECTANGLE",
    "HORIZONTALRECTANGLE",
    "VERTICALTRAPEZOID",
    "HORIZONTALTRAPEZOID",
    "UNKNOWN",
    "CHAMFERED",
    NULL
};


/*******************************************************************************
 *
 * FUNCTION:    OpcAmlOpcodeUpdateWalk
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Opcode update walk, ascending callback
 *
 ******************************************************************************/

ACPI_STATUS
OpcAmlOpcodeUpdateWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{

    /*
     * Handle the Package() case where the actual opcode cannot be determined
     * until the PackageLength operand has been folded and minimized.
     * (PackageOp versus VarPackageOp)
     *
     * This is (as of ACPI 3.0) the only case where the AML opcode can change
     * based upon the value of a parameter.
     *
     * The parser always inserts a VarPackage opcode, which can possibly be
     * optimized to a Package opcode.
     */
    if (Op->Asl.ParseOpcode == PARSEOP_VAR_PACKAGE)
    {
        OpnDoPackage (Op);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    OpcAmlOpcodeWalk
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Parse tree walk to generate both the AML opcodes and the AML
 *              operands.
 *
 ******************************************************************************/

ACPI_STATUS
OpcAmlOpcodeWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{

    TotalParseNodes++;

    OpcGenerateAmlOpcode (Op);
    OpnGenerateAmlOperands (Op);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    OpcGetIntegerWidth
 *
 * PARAMETERS:  Op          - DEFINITION BLOCK op
 *
 * RETURN:      none
 *
 * DESCRIPTION: Extract integer width from the table revision
 *
 ******************************************************************************/

void
OpcGetIntegerWidth (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *Child;


    if (!Op)
    {
        return;
    }

    if (Gbl_RevisionOverride)
    {
        AcpiUtSetIntegerWidth (Gbl_RevisionOverride);
    }
    else
    {
        Child = Op->Asl.Child;
        Child = Child->Asl.Next;
        Child = Child->Asl.Next;

        /* Use the revision to set the integer width */

        AcpiUtSetIntegerWidth ((UINT8) Child->Asl.Value.Integer);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    OpcSetOptimalIntegerSize
 *
 * PARAMETERS:  Op        - A parse tree node
 *
 * RETURN:      Integer width, in bytes. Also sets the node AML opcode to the
 *              optimal integer AML prefix opcode.
 *
 * DESCRIPTION: Determine the optimal AML encoding of an integer. All leading
 *              zeros can be truncated to squeeze the integer into the
 *              minimal number of AML bytes.
 *
 ******************************************************************************/

UINT32
OpcSetOptimalIntegerSize (
    ACPI_PARSE_OBJECT       *Op)
{

#if 0
    /*
     * TBD: - we don't want to optimize integers in the block header, but the
     * code below does not work correctly.
     */
    if (Op->Asl.Parent &&
        Op->Asl.Parent->Asl.Parent &&
       (Op->Asl.Parent->Asl.Parent->Asl.ParseOpcode == PARSEOP_DEFINITIONBLOCK))
    {
        return (0);
    }
#endif

    /*
     * Check for the special AML integers first - Zero, One, Ones.
     * These are single-byte opcodes that are the smallest possible
     * representation of an integer.
     *
     * This optimization is optional.
     */
    if (Gbl_IntegerOptimizationFlag)
    {
        switch (Op->Asl.Value.Integer)
        {
        case 0:

            Op->Asl.AmlOpcode = AML_ZERO_OP;
            AslError (ASL_OPTIMIZATION, ASL_MSG_INTEGER_OPTIMIZATION,
                Op, "Zero");
            return (1);

        case 1:

            Op->Asl.AmlOpcode = AML_ONE_OP;
            AslError (ASL_OPTIMIZATION, ASL_MSG_INTEGER_OPTIMIZATION,
                Op, "One");
            return (1);

        case ACPI_UINT32_MAX:

            /* Check for table integer width (32 or 64) */

            if (AcpiGbl_IntegerByteWidth == 4)
            {
                Op->Asl.AmlOpcode = AML_ONES_OP;
                AslError (ASL_OPTIMIZATION, ASL_MSG_INTEGER_OPTIMIZATION,
                    Op, "Ones");
                return (1);
            }
            break;

        case ACPI_UINT64_MAX:

            /* Check for table integer width (32 or 64) */

            if (AcpiGbl_IntegerByteWidth == 8)
            {
                Op->Asl.AmlOpcode = AML_ONES_OP;
                AslError (ASL_OPTIMIZATION, ASL_MSG_INTEGER_OPTIMIZATION,
                    Op, "Ones");
                return (1);
            }
            break;

        default:

            break;
        }
    }

    /* Find the best fit using the various AML integer prefixes */

    if (Op->Asl.Value.Integer <= ACPI_UINT8_MAX)
    {
        Op->Asl.AmlOpcode = AML_BYTE_OP;
        return (1);
    }
    if (Op->Asl.Value.Integer <= ACPI_UINT16_MAX)
    {
        Op->Asl.AmlOpcode = AML_WORD_OP;
        return (2);
    }
    if (Op->Asl.Value.Integer <= ACPI_UINT32_MAX)
    {
        Op->Asl.AmlOpcode = AML_DWORD_OP;
        return (4);
    }
    else
    {
        if (AcpiGbl_IntegerByteWidth == 4)
        {
            AslError (ASL_WARNING, ASL_MSG_INTEGER_LENGTH,
                Op, NULL);

            if (!Gbl_IgnoreErrors)
            {
                /* Truncate the integer to 32-bit */
                Op->Asl.AmlOpcode = AML_DWORD_OP;
                return (4);
            }
        }

        Op->Asl.AmlOpcode = AML_QWORD_OP;
        return (8);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    OpcDoAccessAs
 *
 * PARAMETERS:  Op        - Parse node
 *
 * RETURN:      None
 *
 * DESCRIPTION: Implement the ACCESS_AS ASL keyword.
 *
 ******************************************************************************/

static void
OpcDoAccessAs (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *TypeOp;
    ACPI_PARSE_OBJECT       *AttribOp;
    ACPI_PARSE_OBJECT       *LengthOp;
    UINT8                   Attribute;


    Op->Asl.AmlOpcodeLength = 1;
    TypeOp = Op->Asl.Child;

    /* First child is the access type */

    TypeOp->Asl.AmlOpcode = AML_RAW_DATA_BYTE;
    TypeOp->Asl.ParseOpcode = PARSEOP_RAW_DATA;

    /* Second child is the optional access attribute */

    AttribOp = TypeOp->Asl.Next;
    if (AttribOp->Asl.ParseOpcode == PARSEOP_DEFAULT_ARG)
    {
        AttribOp->Asl.Value.Integer = 0;
    }
    AttribOp->Asl.AmlOpcode = AML_RAW_DATA_BYTE;
    AttribOp->Asl.ParseOpcode = PARSEOP_RAW_DATA;

    /* Only a few AccessAttributes support AccessLength */

    Attribute = (UINT8) AttribOp->Asl.Value.Integer;
    if ((Attribute != AML_FIELD_ATTRIB_MULTIBYTE) &&
        (Attribute != AML_FIELD_ATTRIB_RAW_BYTES) &&
        (Attribute != AML_FIELD_ATTRIB_RAW_PROCESS))
    {
        return;
    }

    Op->Asl.AmlOpcode = AML_FIELD_EXT_ACCESS_OP;

    /*
     * Child of Attributes is the AccessLength (required for Multibyte,
     * RawBytes, RawProcess.)
     */
    LengthOp = AttribOp->Asl.Child;
    if (!LengthOp)
    {
        return;
    }

    /* TBD: probably can remove */

    if (LengthOp->Asl.ParseOpcode == PARSEOP_DEFAULT_ARG)
    {
        LengthOp->Asl.Value.Integer = 16;
    }

    LengthOp->Asl.AmlOpcode = AML_RAW_DATA_BYTE;
    LengthOp->Asl.ParseOpcode = PARSEOP_RAW_DATA;
}


/*******************************************************************************
 *
 * FUNCTION:    OpcDoConnection
 *
 * PARAMETERS:  Op        - Parse node
 *
 * RETURN:      None
 *
 * DESCRIPTION: Implement the Connection ASL keyword.
 *
 ******************************************************************************/

static void
OpcDoConnection (
    ACPI_PARSE_OBJECT       *Op)
{
    ASL_RESOURCE_NODE       *Rnode;
    ACPI_PARSE_OBJECT       *BufferOp;
    ACPI_PARSE_OBJECT       *BufferLengthOp;
    ACPI_PARSE_OBJECT       *BufferDataOp;
    ASL_RESOURCE_INFO       Info;
    UINT8                   State;


    Op->Asl.AmlOpcodeLength = 1;

    if (Op->Asl.Child->Asl.AmlOpcode == AML_INT_NAMEPATH_OP)
    {
        return;
    }

    BufferOp = Op->Asl.Child;
    BufferLengthOp = BufferOp->Asl.Child;
    BufferDataOp = BufferLengthOp->Asl.Next;

    Info.DescriptorTypeOp = BufferDataOp->Asl.Next;
    Info.CurrentByteOffset = 0;
    State = ACPI_RSTATE_NORMAL;
    Rnode = RsDoOneResourceDescriptor (&Info, &State);
    if (!Rnode)
    {
        return; /* error */
    }

    /*
     * Transform the nodes into the following
     *
     * Op           -> AML_BUFFER_OP
     * First Child  -> BufferLength
     * Second Child -> Descriptor Buffer (raw byte data)
     */
    BufferOp->Asl.ParseOpcode         = PARSEOP_BUFFER;
    BufferOp->Asl.AmlOpcode           = AML_BUFFER_OP;
    BufferOp->Asl.CompileFlags        = NODE_AML_PACKAGE | NODE_IS_RESOURCE_DESC;
    UtSetParseOpName (BufferOp);

    BufferLengthOp->Asl.ParseOpcode   = PARSEOP_INTEGER;
    BufferLengthOp->Asl.Value.Integer = Rnode->BufferLength;
    (void) OpcSetOptimalIntegerSize (BufferLengthOp);
    UtSetParseOpName (BufferLengthOp);

    BufferDataOp->Asl.ParseOpcode         = PARSEOP_RAW_DATA;
    BufferDataOp->Asl.AmlOpcode           = AML_RAW_DATA_CHAIN;
    BufferDataOp->Asl.AmlOpcodeLength     = 0;
    BufferDataOp->Asl.AmlLength           = Rnode->BufferLength;
    BufferDataOp->Asl.Value.Buffer        = (UINT8 *) Rnode;
    UtSetParseOpName (BufferDataOp);
}


/*******************************************************************************
 *
 * FUNCTION:    OpcDoUnicode
 *
 * PARAMETERS:  Op        - Parse node
 *
 * RETURN:      None
 *
 * DESCRIPTION: Implement the UNICODE ASL "macro".  Convert the input string
 *              to a unicode buffer. There is no Unicode AML opcode.
 *
 * Note:  The Unicode string is 16 bits per character, no leading signature,
 *        with a 16-bit terminating NULL.
 *
 ******************************************************************************/

static void
OpcDoUnicode (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *InitializerOp;
    UINT32                  Length;
    UINT32                  Count;
    UINT32                  i;
    UINT8                   *AsciiString;
    UINT16                  *UnicodeString;
    ACPI_PARSE_OBJECT       *BufferLengthOp;


    /* Change op into a buffer object */

    Op->Asl.CompileFlags &= ~NODE_COMPILE_TIME_CONST;
    Op->Asl.ParseOpcode = PARSEOP_BUFFER;
    UtSetParseOpName (Op);

    /* Buffer Length is first, followed by the string */

    BufferLengthOp = Op->Asl.Child;
    InitializerOp = BufferLengthOp->Asl.Next;

    AsciiString = (UINT8 *) InitializerOp->Asl.Value.String;

    /* Create a new buffer for the Unicode string */

    Count = strlen (InitializerOp->Asl.Value.String) + 1;
    Length = Count * sizeof (UINT16);
    UnicodeString = UtLocalCalloc (Length);

    /* Convert to Unicode string (including null terminator) */

    for (i = 0; i < Count; i++)
    {
        UnicodeString[i] = (UINT16) AsciiString[i];
    }

    /*
     * Just set the buffer size node to be the buffer length, regardless
     * of whether it was previously an integer or a default_arg placeholder
     */
    BufferLengthOp->Asl.ParseOpcode   = PARSEOP_INTEGER;
    BufferLengthOp->Asl.AmlOpcode     = AML_DWORD_OP;
    BufferLengthOp->Asl.Value.Integer = Length;
    UtSetParseOpName (BufferLengthOp);

    (void) OpcSetOptimalIntegerSize (BufferLengthOp);

    /* The Unicode string is a raw data buffer */

    InitializerOp->Asl.Value.Buffer   = (UINT8 *) UnicodeString;
    InitializerOp->Asl.AmlOpcode      = AML_RAW_DATA_BUFFER;
    InitializerOp->Asl.AmlLength      = Length;
    InitializerOp->Asl.ParseOpcode    = PARSEOP_RAW_DATA;
    InitializerOp->Asl.Child          = NULL;
    UtSetParseOpName (InitializerOp);
}


/*******************************************************************************
 *
 * FUNCTION:    OpcDoEisaId
 *
 * PARAMETERS:  Op        - Parse node
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert a string EISA ID to numeric representation. See the
 *              Pnp BIOS Specification for details. Here is an excerpt:
 *
 *              A seven character ASCII representation of the product
 *              identifier compressed into a 32-bit identifier. The seven
 *              character ID consists of a three character manufacturer code,
 *              a three character hexadecimal product identifier, and a one
 *              character hexadecimal revision number. The manufacturer code
 *              is a 3 uppercase character code that is compressed into 3 5-bit
 *              values as follows:
 *                  1) Find hex ASCII value for each letter
 *                  2) Subtract 40h from each ASCII value
 *                  3) Retain 5 least significant bits for each letter by
 *                     discarding upper 3 bits because they are always 0.
 *                  4) Compressed code = concatenate 0 and the 3 5-bit values
 *
 *              The format of the compressed product identifier is as follows:
 *              Byte 0: Bit 7       - Reserved (0)
 *                      Bits 6-2:   - 1st character of compressed mfg code
 *                      Bits 1-0    - Upper 2 bits of 2nd character of mfg code
 *              Byte 1: Bits 7-5    - Lower 3 bits of 2nd character of mfg code
 *                      Bits 4-0    - 3rd character of mfg code
 *              Byte 2: Bits 7-4    - 1st hex digit of product number
 *                      Bits 3-0    - 2nd hex digit of product number
 *              Byte 3: Bits 7-4    - 3st hex digit of product number
 *                      Bits 3-0    - Hex digit of the revision number
 *
 ******************************************************************************/

static void
OpcDoEisaId (
    ACPI_PARSE_OBJECT       *Op)
{
    UINT32                  EisaId = 0;
    UINT32                  BigEndianId;
    char                    *InString;
    ACPI_STATUS             Status = AE_OK;
    UINT32                  i;


    InString = (char *) Op->Asl.Value.String;

    /*
     * The EISAID string must be exactly 7 characters and of the form
     * "UUUXXXX" -- 3 uppercase letters and 4 hex digits (e.g., "PNP0001")
     */
    if (strlen (InString) != 7)
    {
        Status = AE_BAD_PARAMETER;
    }
    else
    {
        /* Check all 7 characters for correct format */

        for (i = 0; i < 7; i++)
        {
            /* First 3 characters must be uppercase letters */

            if (i < 3)
            {
                if (!isupper ((int) InString[i]))
                {
                    Status = AE_BAD_PARAMETER;
                }
            }

            /* Last 4 characters must be hex digits */

            else if (!isxdigit ((int) InString[i]))
            {
                Status = AE_BAD_PARAMETER;
            }
        }
    }

    if (ACPI_FAILURE (Status))
    {
        AslError (ASL_ERROR, ASL_MSG_INVALID_EISAID, Op, Op->Asl.Value.String);
    }
    else
    {
        /* Create ID big-endian first (bits are contiguous) */

        BigEndianId =
            (UINT32) ((UINT8) (InString[0] - 0x40)) << 26 |
            (UINT32) ((UINT8) (InString[1] - 0x40)) << 21 |
            (UINT32) ((UINT8) (InString[2] - 0x40)) << 16 |

            (AcpiUtAsciiCharToHex (InString[3])) << 12 |
            (AcpiUtAsciiCharToHex (InString[4])) << 8  |
            (AcpiUtAsciiCharToHex (InString[5])) << 4  |
             AcpiUtAsciiCharToHex (InString[6]);

        /* Swap to little-endian to get final ID (see function header) */

        EisaId = AcpiUtDwordByteSwap (BigEndianId);
    }

    /*
     * Morph the Op into an integer, regardless of whether there
     * was an error in the EISAID string
     */
    Op->Asl.Value.Integer = EisaId;

    Op->Asl.CompileFlags &= ~NODE_COMPILE_TIME_CONST;
    Op->Asl.ParseOpcode = PARSEOP_INTEGER;
    (void) OpcSetOptimalIntegerSize (Op);

    /* Op is now an integer */

    UtSetParseOpName (Op);
}


/*******************************************************************************
 *
 * FUNCTION:    OpcEncodePldBuffer
 *
 * PARAMETERS:  PldInfo             - _PLD buffer struct (Using local struct)
 *
 * RETURN:      Encode _PLD buffer suitable for return value from _PLD
 *
 * DESCRIPTION: Bit-packs a _PLD buffer struct.
 *
 ******************************************************************************/

static UINT8 *
OpcEncodePldBuffer (
    ACPI_PLD_INFO           *PldInfo)
{
    UINT32                  *Buffer;
    UINT32                  Dword;


    Buffer = ACPI_ALLOCATE_ZEROED (ACPI_PLD_BUFFER_SIZE);
    if (!Buffer)
    {
        return (NULL);
    }

    /* First 32 bits */

    Dword = 0;
    ACPI_PLD_SET_REVISION       (&Dword, PldInfo->Revision);
    ACPI_PLD_SET_IGNORE_COLOR   (&Dword, PldInfo->IgnoreColor);
    ACPI_PLD_SET_RED            (&Dword, PldInfo->Red);
    ACPI_PLD_SET_GREEN          (&Dword, PldInfo->Green);
    ACPI_PLD_SET_BLUE           (&Dword, PldInfo->Blue);
    ACPI_MOVE_32_TO_32          (&Buffer[0], &Dword);

    /* Second 32 bits */

    Dword = 0;
    ACPI_PLD_SET_WIDTH          (&Dword, PldInfo->Width);
    ACPI_PLD_SET_HEIGHT         (&Dword, PldInfo->Height);
    ACPI_MOVE_32_TO_32          (&Buffer[1], &Dword);

    /* Third 32 bits */

    Dword = 0;
    ACPI_PLD_SET_USER_VISIBLE   (&Dword, PldInfo->UserVisible);
    ACPI_PLD_SET_DOCK           (&Dword, PldInfo->Dock);
    ACPI_PLD_SET_LID            (&Dword, PldInfo->Lid);
    ACPI_PLD_SET_PANEL          (&Dword, PldInfo->Panel);
    ACPI_PLD_SET_VERTICAL       (&Dword, PldInfo->VerticalPosition);
    ACPI_PLD_SET_HORIZONTAL     (&Dword, PldInfo->HorizontalPosition);
    ACPI_PLD_SET_SHAPE          (&Dword, PldInfo->Shape);
    ACPI_PLD_SET_ORIENTATION    (&Dword, PldInfo->GroupOrientation);
    ACPI_PLD_SET_TOKEN          (&Dword, PldInfo->GroupToken);
    ACPI_PLD_SET_POSITION       (&Dword, PldInfo->GroupPosition);
    ACPI_PLD_SET_BAY            (&Dword, PldInfo->Bay);
    ACPI_MOVE_32_TO_32          (&Buffer[2], &Dword);

    /* Fourth 32 bits */

    Dword = 0;
    ACPI_PLD_SET_EJECTABLE      (&Dword, PldInfo->Ejectable);
    ACPI_PLD_SET_OSPM_EJECT     (&Dword, PldInfo->OspmEjectRequired);
    ACPI_PLD_SET_CABINET        (&Dword, PldInfo->CabinetNumber);
    ACPI_PLD_SET_CARD_CAGE      (&Dword, PldInfo->CardCageNumber);
    ACPI_PLD_SET_REFERENCE      (&Dword, PldInfo->Reference);
    ACPI_PLD_SET_ROTATION       (&Dword, PldInfo->Rotation);
    ACPI_PLD_SET_ORDER          (&Dword, PldInfo->Order);
    ACPI_MOVE_32_TO_32          (&Buffer[3], &Dword);

    if (PldInfo->Revision >= 2)
    {
        /* Fifth 32 bits */

        Dword = 0;
        ACPI_PLD_SET_VERT_OFFSET    (&Dword, PldInfo->VerticalOffset);
        ACPI_PLD_SET_HORIZ_OFFSET   (&Dword, PldInfo->HorizontalOffset);
        ACPI_MOVE_32_TO_32          (&Buffer[4], &Dword);
    }

    return (ACPI_CAST_PTR (UINT8, Buffer));
}


/*******************************************************************************
 *
 * FUNCTION:    OpcFindName
 *
 * PARAMETERS:  List                - Array of char strings to be searched
 *              Name                - Char string to string for
 *              Index               - Index value to set if found
 *
 * RETURN:      TRUE if any names matched, FALSE otherwise
 *
 * DESCRIPTION: Match PLD name to value in lookup table. Sets Value to
 *              equivalent parameter value.
 *
 ******************************************************************************/

static BOOLEAN
OpcFindName (
    char                    **List,
    char                    *Name,
    UINT64                  *Index)
{
    char                     *Str;
    UINT32                   i;


    AcpiUtStrupr (Name);

    for (i = 0, Str = List[0]; Str; i++, Str = List[i])
    {
        if (!(strncmp (Str, Name, strlen (Name))))
        {
            *Index = i;
            return (TRUE);
        }
    }

    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    OpcDoPld
 *
 * PARAMETERS:  Op                  - Parse node
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert ToPLD macro to 20-byte buffer
 *
 ******************************************************************************/

static void
OpcDoPld (
    ACPI_PARSE_OBJECT       *Op)
{
    UINT8                   *Buffer;
    ACPI_PARSE_OBJECT       *Node;
    ACPI_PLD_INFO           PldInfo;
    ACPI_PARSE_OBJECT       *NewOp;


    if (!Op)
    {
        AslError(ASL_ERROR, ASL_MSG_NOT_EXIST, Op, NULL);
        return;
    }

    if (Op->Asl.ParseOpcode != PARSEOP_TOPLD)
    {
        AslError(ASL_ERROR, ASL_MSG_INVALID_TYPE, Op, NULL);
        return;
    }

    memset (&PldInfo, 0, sizeof (ACPI_PLD_INFO));

    Node = Op->Asl.Child;
    while (Node)
    {
        switch (Node->Asl.ParseOpcode)
        {
        case PARSEOP_PLD_REVISION:

            if (Node->Asl.Child->Asl.ParseOpcode != PARSEOP_INTEGER)
            {
                AslError(ASL_ERROR, ASL_MSG_INVALID_TYPE, Node, NULL);
                break;
            }

            if (Node->Asl.Child->Asl.Value.Integer > 127)
            {
                AslError(ASL_ERROR, ASL_MSG_RANGE, Node, NULL);
                break;
            }

            PldInfo.Revision = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            break;

        case PARSEOP_PLD_IGNORECOLOR:

            if (Node->Asl.Child->Asl.ParseOpcode != PARSEOP_INTEGER)
            {
                AslError(ASL_ERROR, ASL_MSG_INVALID_TYPE, Node, NULL);
                break;
            }

            if (Node->Asl.Child->Asl.Value.Integer > 1)
            {
                AslError(ASL_ERROR, ASL_MSG_RANGE, Node, NULL);
                break;
            }

            PldInfo.IgnoreColor = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            break;

        case PARSEOP_PLD_RED:
        case PARSEOP_PLD_GREEN:
        case PARSEOP_PLD_BLUE:

            if (Node->Asl.Child->Asl.ParseOpcode != PARSEOP_INTEGER)
            {
                AslError(ASL_ERROR, ASL_MSG_RANGE, Node, NULL);
                break;
            }

            if (Node->Asl.Child->Asl.Value.Integer > 255)
            {
                AslError(ASL_ERROR, ASL_MSG_RANGE, Node, NULL);
                break;
            }

            if (Node->Asl.ParseOpcode == PARSEOP_PLD_RED)
            {
                PldInfo.Red = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            }
            else if (Node->Asl.ParseOpcode == PARSEOP_PLD_GREEN)
            {
                PldInfo.Green = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            }
            else /* PARSEOP_PLD_BLUE */
            {
                PldInfo.Blue = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            }
            break;

        case PARSEOP_PLD_WIDTH:
        case PARSEOP_PLD_HEIGHT:

            if (Node->Asl.Child->Asl.ParseOpcode != PARSEOP_INTEGER)
            {
                AslError(ASL_ERROR, ASL_MSG_INVALID_TYPE, Node, NULL);
                break;
            }

            if (Node->Asl.Child->Asl.Value.Integer > 65535)
            {
                AslError(ASL_ERROR, ASL_MSG_RANGE, Node, NULL);
                break;
            }

            if (Node->Asl.ParseOpcode == PARSEOP_PLD_WIDTH)
            {
                PldInfo.Width = (UINT16) Node->Asl.Child->Asl.Value.Integer;
            }
            else /* PARSEOP_PLD_HEIGHT */
            {
                PldInfo.Height = (UINT16) Node->Asl.Child->Asl.Value.Integer;
            }

            break;

        case PARSEOP_PLD_USERVISIBLE:
        case PARSEOP_PLD_DOCK:
        case PARSEOP_PLD_LID:

            if (Node->Asl.Child->Asl.ParseOpcode != PARSEOP_INTEGER)
            {
                AslError(ASL_ERROR, ASL_MSG_INVALID_TYPE, Node, NULL);
                break;
            }

            if (Node->Asl.Child->Asl.Value.Integer > 1)
            {
                AslError(ASL_ERROR, ASL_MSG_RANGE, Node, NULL);
                break;
            }

            if (Node->Asl.ParseOpcode == PARSEOP_PLD_USERVISIBLE)
            {
                PldInfo.UserVisible = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            }
            else if (Node->Asl.ParseOpcode == PARSEOP_PLD_DOCK)
            {
                PldInfo.Dock = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            }
            else
            {
                PldInfo.Lid = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            }

            break;

        case PARSEOP_PLD_PANEL:

            if (Node->Asl.Child->Asl.ParseOpcode == PARSEOP_INTEGER)
            {
                if (Node->Asl.Child->Asl.Value.Integer > 6)
                {
                    AslError(ASL_ERROR, ASL_MSG_RANGE, Node, NULL);
                    break;
                }
            }
            else /* PARSEOP_STRING */
            {
                if (!OpcFindName(AslPldPanelList,
                    Node->Asl.Child->Asl.Value.String,
                    &Node->Asl.Child->Asl.Value.Integer))
                {
                    AslError(ASL_ERROR, ASL_MSG_INVALID_OPERAND, Node, NULL);
                    break;
                }
            }

            PldInfo.Panel = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            break;

        case PARSEOP_PLD_VERTICALPOSITION:

            if (Node->Asl.Child->Asl.ParseOpcode == PARSEOP_INTEGER)
            {
                if (Node->Asl.Child->Asl.Value.Integer > 2)
                {
                    AslError(ASL_ERROR, ASL_MSG_RANGE, Node, NULL);
                    break;
                }
            }
            else /* PARSEOP_STRING */
            {
                if (!OpcFindName(AslPldVerticalPositionList,
                    Node->Asl.Child->Asl.Value.String,
                    &Node->Asl.Child->Asl.Value.Integer))
                {
                    AslError(ASL_ERROR, ASL_MSG_INVALID_OPERAND, Node, NULL);
                    break;
                }
            }

            PldInfo.VerticalPosition = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            break;

        case PARSEOP_PLD_HORIZONTALPOSITION:

            if (Node->Asl.Child->Asl.ParseOpcode == PARSEOP_INTEGER)
            {
                if (Node->Asl.Child->Asl.Value.Integer > 2)
                {
                    AslError(ASL_ERROR, ASL_MSG_RANGE, Node, NULL);
                    break;
                }
            }
            else /* PARSEOP_STRING */
            {
                if (!OpcFindName(AslPldHorizontalPositionList,
                    Node->Asl.Child->Asl.Value.String,
                    &Node->Asl.Child->Asl.Value.Integer))
                {
                    AslError(ASL_ERROR, ASL_MSG_INVALID_OPERAND, Node, NULL);
                    break;
                }
            }

            PldInfo.HorizontalPosition = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            break;

        case PARSEOP_PLD_SHAPE:

            if (Node->Asl.Child->Asl.ParseOpcode == PARSEOP_INTEGER)
            {
                if (Node->Asl.Child->Asl.Value.Integer > 8)
                {
                    AslError(ASL_ERROR, ASL_MSG_RANGE, Node, NULL);
                    break;
                }
            }
            else /* PARSEOP_STRING */
            {
                if (!OpcFindName(AslPldShapeList,
                    Node->Asl.Child->Asl.Value.String,
                    &Node->Asl.Child->Asl.Value.Integer))
                {
                    AslError(ASL_ERROR, ASL_MSG_INVALID_OPERAND, Node, NULL);
                    break;
                }
            }

            PldInfo.Shape = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            break;

        case PARSEOP_PLD_GROUPORIENTATION:

            if (Node->Asl.Child->Asl.ParseOpcode != PARSEOP_INTEGER)
            {
                AslError(ASL_ERROR, ASL_MSG_INVALID_TYPE, Node, NULL);
                break;
            }

            if (Node->Asl.Child->Asl.Value.Integer > 1)
            {
                AslError(ASL_ERROR, ASL_MSG_RANGE, Node, NULL);
                break;
            }

            PldInfo.GroupOrientation = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            break;

        case PARSEOP_PLD_GROUPTOKEN:
        case PARSEOP_PLD_GROUPPOSITION:

            if (Node->Asl.Child->Asl.ParseOpcode != PARSEOP_INTEGER)
            {
                AslError(ASL_ERROR, ASL_MSG_INVALID_TYPE, Node, NULL);
                break;
            }

            if (Node->Asl.Child->Asl.Value.Integer > 255)
            {
                AslError(ASL_ERROR, ASL_MSG_RANGE, Node, NULL);
                break;
            }


            if (Node->Asl.ParseOpcode == PARSEOP_PLD_GROUPTOKEN)
            {
                PldInfo.GroupToken = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            }
            else /* PARSEOP_PLD_GROUPPOSITION */
            {
                PldInfo.GroupPosition = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            }

            break;

        case PARSEOP_PLD_BAY:
        case PARSEOP_PLD_EJECTABLE:
        case PARSEOP_PLD_EJECTREQUIRED:

            if (Node->Asl.Child->Asl.ParseOpcode != PARSEOP_INTEGER)
            {
                AslError(ASL_ERROR, ASL_MSG_INVALID_TYPE, Node, NULL);
                break;
            }

            if (Node->Asl.Child->Asl.Value.Integer > 1)
            {
                AslError(ASL_ERROR, ASL_MSG_RANGE, Node, NULL);
                break;
            }

            if (Node->Asl.ParseOpcode == PARSEOP_PLD_BAY)
            {
                PldInfo.Bay = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            }
            else if (Node->Asl.ParseOpcode == PARSEOP_PLD_EJECTABLE)
            {
                PldInfo.Ejectable = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            }
            else /* PARSEOP_PLD_EJECTREQUIRED */
            {
                PldInfo.OspmEjectRequired = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            }

            break;

        case PARSEOP_PLD_CABINETNUMBER:
        case PARSEOP_PLD_CARDCAGENUMBER:

            if (Node->Asl.Child->Asl.ParseOpcode != PARSEOP_INTEGER)
            {
                AslError(ASL_ERROR, ASL_MSG_INVALID_TYPE, Node, NULL);
                break;
            }

            if (Node->Asl.Child->Asl.Value.Integer > 255)
            {
                AslError(ASL_ERROR, ASL_MSG_RANGE, Node, NULL);
                break;
            }

            if (Node->Asl.ParseOpcode == PARSEOP_PLD_CABINETNUMBER)
            {
                PldInfo.CabinetNumber = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            }
            else /* PARSEOP_PLD_CARDCAGENUMBER */
            {
                PldInfo.CardCageNumber = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            }

            break;

        case PARSEOP_PLD_REFERENCE:

            if (Node->Asl.Child->Asl.ParseOpcode != PARSEOP_INTEGER)
            {
                AslError(ASL_ERROR, ASL_MSG_INVALID_TYPE, Node, NULL);
                break;
            }

            if (Node->Asl.Child->Asl.Value.Integer > 1)
            {
                AslError(ASL_ERROR, ASL_MSG_RANGE, Node, NULL);
                break;
            }

            PldInfo.Reference = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            break;

        case PARSEOP_PLD_ROTATION:

            if (Node->Asl.Child->Asl.ParseOpcode != PARSEOP_INTEGER)
            {
                AslError(ASL_ERROR, ASL_MSG_INVALID_TYPE, Node, NULL);
                break;
            }

            if (Node->Asl.Child->Asl.Value.Integer > 7)
            {
                switch (Node->Asl.Child->Asl.Value.Integer)
                {
                case 45:

                    Node->Asl.Child->Asl.Value.Integer = 1;
                    break;

                case 90:

                    Node->Asl.Child->Asl.Value.Integer = 2;
                    break;

                case 135:

                    Node->Asl.Child->Asl.Value.Integer = 3;
                    break;

                case 180:

                    Node->Asl.Child->Asl.Value.Integer = 4;
                    break;

                case 225:

                    Node->Asl.Child->Asl.Value.Integer = 5;
                    break;

                case 270:

                    Node->Asl.Child->Asl.Value.Integer = 6;
                    break;

                case 315:

                    Node->Asl.Child->Asl.Value.Integer = 7;
                    break;

                default:

                    AslError(ASL_ERROR, ASL_MSG_RANGE, Node, NULL);
                    break;
                }
            }

            PldInfo.Rotation = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            break;

        case PARSEOP_PLD_ORDER:

            if (Node->Asl.Child->Asl.ParseOpcode != PARSEOP_INTEGER)
            {
                AslError(ASL_ERROR, ASL_MSG_INVALID_TYPE, Node, NULL);
                break;
            }

            if (Node->Asl.Child->Asl.Value.Integer > 31)
            {
                AslError(ASL_ERROR, ASL_MSG_RANGE, Node, NULL);
                break;
            }

            PldInfo.Order = (UINT8) Node->Asl.Child->Asl.Value.Integer;
            break;

        case PARSEOP_PLD_VERTICALOFFSET:
        case PARSEOP_PLD_HORIZONTALOFFSET:

            if (Node->Asl.Child->Asl.ParseOpcode != PARSEOP_INTEGER)
            {
                AslError(ASL_ERROR, ASL_MSG_INVALID_TYPE, Node, NULL);
                break;
            }

            if (Node->Asl.Child->Asl.Value.Integer > 65535)
            {
                AslError(ASL_ERROR, ASL_MSG_RANGE, Node, NULL);
                break;
            }

            if (Node->Asl.ParseOpcode == PARSEOP_PLD_VERTICALOFFSET)
            {
                PldInfo.VerticalOffset = (UINT16) Node->Asl.Child->Asl.Value.Integer;
            }
            else /* PARSEOP_PLD_HORIZONTALOFFSET */
            {
                PldInfo.HorizontalOffset = (UINT16) Node->Asl.Child->Asl.Value.Integer;
            }

            break;

        default:

            AslError(ASL_ERROR, ASL_MSG_INVALID_TYPE, Node, NULL);
            break;
        }

        Node = Node->Asl.Next;
    }

    Buffer = OpcEncodePldBuffer(&PldInfo);

    /* Change Op to a Buffer */

    Op->Asl.ParseOpcode = PARSEOP_BUFFER;
    Op->Common.AmlOpcode = AML_BUFFER_OP;

    /* Disable further optimization */

    Op->Asl.CompileFlags &= ~NODE_COMPILE_TIME_CONST;
    UtSetParseOpName (Op);

    /* Child node is the buffer length */

    NewOp = TrAllocateNode (PARSEOP_INTEGER);

    NewOp->Asl.AmlOpcode     = AML_BYTE_OP;
    NewOp->Asl.Value.Integer = 20;
    NewOp->Asl.Parent        = Op;

    Op->Asl.Child = NewOp;
    Op = NewOp;

    /* Peer to the child is the raw buffer data */

    NewOp = TrAllocateNode (PARSEOP_RAW_DATA);
    NewOp->Asl.AmlOpcode     = AML_RAW_DATA_BUFFER;
    NewOp->Asl.AmlLength     = 20;
    NewOp->Asl.Value.String  = ACPI_CAST_PTR (char, Buffer);
    NewOp->Asl.Parent        = Op->Asl.Parent;

    Op->Asl.Next = NewOp;
}


/*******************************************************************************
 *
 * FUNCTION:    OpcDoUuId
 *
 * PARAMETERS:  Op                  - Parse node
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert UUID string to 16-byte buffer
 *
 ******************************************************************************/

static void
OpcDoUuId (
    ACPI_PARSE_OBJECT       *Op)
{
    char                    *InString;
    UINT8                   *Buffer;
    ACPI_STATUS             Status = AE_OK;
    ACPI_PARSE_OBJECT       *NewOp;


    InString = ACPI_CAST_PTR (char, Op->Asl.Value.String);
    Buffer = UtLocalCalloc (16);

    Status = AuValidateUuid (InString);
    if (ACPI_FAILURE (Status))
    {
        AslError (ASL_ERROR, ASL_MSG_INVALID_UUID, Op, Op->Asl.Value.String);
    }
    else
    {
        AcpiUtConvertStringToUuid (InString, Buffer);
    }

    /* Change Op to a Buffer */

    Op->Asl.ParseOpcode = PARSEOP_BUFFER;
    Op->Common.AmlOpcode = AML_BUFFER_OP;

    /* Disable further optimization */

    Op->Asl.CompileFlags &= ~NODE_COMPILE_TIME_CONST;
    UtSetParseOpName (Op);

    /* Child node is the buffer length */

    NewOp = TrAllocateNode (PARSEOP_INTEGER);

    NewOp->Asl.AmlOpcode     = AML_BYTE_OP;
    NewOp->Asl.Value.Integer = 16;
    NewOp->Asl.Parent        = Op;

    Op->Asl.Child = NewOp;
    Op = NewOp;

    /* Peer to the child is the raw buffer data */

    NewOp = TrAllocateNode (PARSEOP_RAW_DATA);
    NewOp->Asl.AmlOpcode     = AML_RAW_DATA_BUFFER;
    NewOp->Asl.AmlLength     = 16;
    NewOp->Asl.Value.String  = ACPI_CAST_PTR (char, Buffer);
    NewOp->Asl.Parent        = Op->Asl.Parent;

    Op->Asl.Next = NewOp;
}


/*******************************************************************************
 *
 * FUNCTION:    OpcGenerateAmlOpcode
 *
 * PARAMETERS:  Op                  - Parse node
 *
 * RETURN:      None
 *
 * DESCRIPTION: Generate the AML opcode associated with the node and its
 *              parse (lex/flex) keyword opcode. Essentially implements
 *              a mapping between the parse opcodes and the actual AML opcodes.
 *
 ******************************************************************************/

void
OpcGenerateAmlOpcode (
    ACPI_PARSE_OBJECT       *Op)
{
    UINT16                  Index;


    Index = (UINT16) (Op->Asl.ParseOpcode - ASL_PARSE_OPCODE_BASE);

    Op->Asl.AmlOpcode     = AslKeywordMapping[Index].AmlOpcode;
    Op->Asl.AcpiBtype     = AslKeywordMapping[Index].AcpiBtype;
    Op->Asl.CompileFlags |= AslKeywordMapping[Index].Flags;

    if (!Op->Asl.Value.Integer)
    {
        Op->Asl.Value.Integer = AslKeywordMapping[Index].Value;
    }

    /* Special handling for some opcodes */

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_INTEGER:
        /*
         * Set the opcode based on the size of the integer
         */
        (void) OpcSetOptimalIntegerSize (Op);
        break;

    case PARSEOP_OFFSET:

        Op->Asl.AmlOpcodeLength = 1;
        break;

    case PARSEOP_ACCESSAS:

        OpcDoAccessAs (Op);
        break;

    case PARSEOP_CONNECTION:

        OpcDoConnection (Op);
        break;

    case PARSEOP_EISAID:

        OpcDoEisaId (Op);
        break;

    case PARSEOP_PRINTF:

        OpcDoPrintf (Op);
        break;

    case PARSEOP_FPRINTF:

        OpcDoFprintf (Op);
        break;

    case PARSEOP_TOPLD:

        OpcDoPld (Op);
        break;

    case PARSEOP_TOUUID:

        OpcDoUuId (Op);
        break;

    case PARSEOP_UNICODE:

        OpcDoUnicode (Op);
        break;

    case PARSEOP_INCLUDE:

        Op->Asl.Child->Asl.ParseOpcode = PARSEOP_DEFAULT_ARG;
        Gbl_HasIncludeFiles = TRUE;
        break;

    case PARSEOP_EXTERNAL:

        Op->Asl.Child->Asl.ParseOpcode = PARSEOP_DEFAULT_ARG;
        Op->Asl.Child->Asl.Next->Asl.ParseOpcode = PARSEOP_DEFAULT_ARG;
        break;

    case PARSEOP_TIMER:

        if (AcpiGbl_IntegerBitWidth == 32)
        {
            AslError (ASL_REMARK, ASL_MSG_TRUNCATION, Op, NULL);
        }
        break;

    default:

        /* Nothing to do for other opcodes */

        break;
    }

    return;
}
