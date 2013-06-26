/*******************************************************************************
 *
 * Module Name: dmbuffer - AML disassembler, buffer and string support
 *
 ******************************************************************************/

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


#include "acpi.h"
#include "accommon.h"
#include "acdisasm.h"
#include "acparser.h"
#include "amlcode.h"


#ifdef ACPI_DISASSEMBLER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dmbuffer")

/* Local prototypes */

static void
AcpiDmUnicode (
    ACPI_PARSE_OBJECT       *Op);

static void
AcpiDmIsEisaIdElement (
    ACPI_PARSE_OBJECT       *Op);

static void
AcpiDmPldBuffer (
    UINT32                  Level,
    UINT8                   *ByteData,
    UINT32                  ByteCount);


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDisasmByteList
 *
 * PARAMETERS:  Level               - Current source code indentation level
 *              ByteData            - Pointer to the byte list
 *              ByteCount           - Length of the byte list
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump an AML "ByteList" in Hex format. 8 bytes per line, prefixed
 *              with the hex buffer offset.
 *
 ******************************************************************************/

void
AcpiDmDisasmByteList (
    UINT32                  Level,
    UINT8                   *ByteData,
    UINT32                  ByteCount)
{
    UINT32                  i;


    if (!ByteCount)
    {
        return;
    }

    /* Dump the byte list */

    for (i = 0; i < ByteCount; i++)
    {
        /* New line every 8 bytes */

        if (((i % 8) == 0) && (i < ByteCount))
        {
            if (i > 0)
            {
                AcpiOsPrintf ("\n");
            }

            AcpiDmIndent (Level);
            if (ByteCount > 8)
            {
                AcpiOsPrintf ("/* %04X */  ", i);
            }
        }

        AcpiOsPrintf (" 0x%2.2X", (UINT32) ByteData[i]);

        /* Add comma if there are more bytes to display */

        if (i < (ByteCount -1))
        {
            AcpiOsPrintf (",");
        }
    }

    if (Level)
    {
        AcpiOsPrintf ("\n");
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmByteList
 *
 * PARAMETERS:  Info            - Parse tree walk info
 *              Op              - Byte list op
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump a buffer byte list, handling the various types of buffers.
 *              Buffer type must be already set in the Op DisasmOpcode.
 *
 ******************************************************************************/

void
AcpiDmByteList (
    ACPI_OP_WALK_INFO       *Info,
    ACPI_PARSE_OBJECT       *Op)
{
    UINT8                   *ByteData;
    UINT32                  ByteCount;


    ByteData = Op->Named.Data;
    ByteCount = (UINT32) Op->Common.Value.Integer;

    /*
     * The byte list belongs to a buffer, and can be produced by either
     * a ResourceTemplate, Unicode, quoted string, or a plain byte list.
     */
    switch (Op->Common.Parent->Common.DisasmOpcode)
    {
    case ACPI_DASM_RESOURCE:

        AcpiDmResourceTemplate (Info, Op->Common.Parent, ByteData, ByteCount);
        break;

    case ACPI_DASM_STRING:

        AcpiDmIndent (Info->Level);
        AcpiUtPrintString ((char *) ByteData, ACPI_UINT16_MAX);
        AcpiOsPrintf ("\n");
        break;

    case ACPI_DASM_UNICODE:

        AcpiDmUnicode (Op);
        break;

    case ACPI_DASM_PLD_METHOD:

        AcpiDmDisasmByteList (Info->Level, ByteData, ByteCount);
        AcpiDmPldBuffer (Info->Level, ByteData, ByteCount);
        break;

    case ACPI_DASM_BUFFER:
    default:
        /*
         * Not a resource, string, or unicode string.
         * Just dump the buffer
         */
        AcpiDmDisasmByteList (Info->Level, ByteData, ByteCount);
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIsUnicodeBuffer
 *
 * PARAMETERS:  Op              - Buffer Object to be examined
 *
 * RETURN:      TRUE if buffer contains a UNICODE string
 *
 * DESCRIPTION: Determine if a buffer Op contains a Unicode string
 *
 ******************************************************************************/

BOOLEAN
AcpiDmIsUnicodeBuffer (
    ACPI_PARSE_OBJECT       *Op)
{
    UINT8                   *ByteData;
    UINT32                  ByteCount;
    UINT32                  WordCount;
    ACPI_PARSE_OBJECT       *SizeOp;
    ACPI_PARSE_OBJECT       *NextOp;
    UINT32                  i;


    /* Buffer size is the buffer argument */

    SizeOp = Op->Common.Value.Arg;

    /* Next, the initializer byte list to examine */

    NextOp = SizeOp->Common.Next;
    if (!NextOp)
    {
        return (FALSE);
    }

    /* Extract the byte list info */

    ByteData = NextOp->Named.Data;
    ByteCount = (UINT32) NextOp->Common.Value.Integer;
    WordCount = ACPI_DIV_2 (ByteCount);

    /*
     * Unicode string must have an even number of bytes and last
     * word must be zero
     */
    if ((!ByteCount)     ||
         (ByteCount < 4) ||
         (ByteCount & 1) ||
        ((UINT16 *) (void *) ByteData)[WordCount - 1] != 0)
    {
        return (FALSE);
    }

    /* For each word, 1st byte must be ascii, 2nd byte must be zero */

    for (i = 0; i < (ByteCount - 2); i += 2)
    {
        if ((!ACPI_IS_PRINT (ByteData[i])) ||
            (ByteData[(ACPI_SIZE) i + 1] != 0))
        {
            return (FALSE);
        }
    }

    /* Ignore the Size argument in the disassembly of this buffer op */

    SizeOp->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;
    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIsStringBuffer
 *
 * PARAMETERS:  Op              - Buffer Object to be examined
 *
 * RETURN:      TRUE if buffer contains a ASCII string, FALSE otherwise
 *
 * DESCRIPTION: Determine if a buffer Op contains a ASCII string
 *
 ******************************************************************************/

BOOLEAN
AcpiDmIsStringBuffer (
    ACPI_PARSE_OBJECT       *Op)
{
    UINT8                   *ByteData;
    UINT32                  ByteCount;
    ACPI_PARSE_OBJECT       *SizeOp;
    ACPI_PARSE_OBJECT       *NextOp;
    UINT32                  i;


    /* Buffer size is the buffer argument */

    SizeOp = Op->Common.Value.Arg;

    /* Next, the initializer byte list to examine */

    NextOp = SizeOp->Common.Next;
    if (!NextOp)
    {
        return (FALSE);
    }

    /* Extract the byte list info */

    ByteData = NextOp->Named.Data;
    ByteCount = (UINT32) NextOp->Common.Value.Integer;

    /* Last byte must be the null terminator */

    if ((!ByteCount)     ||
         (ByteCount < 2) ||
         (ByteData[ByteCount-1] != 0))
    {
        return (FALSE);
    }

    for (i = 0; i < (ByteCount - 1); i++)
    {
        /* TBD: allow some escapes (non-ascii chars).
         * they will be handled in the string output routine
         */

        if (!ACPI_IS_PRINT (ByteData[i]))
        {
            return (FALSE);
        }
    }

    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIsPldBuffer
 *
 * PARAMETERS:  Op                  - Buffer Object to be examined
 *
 * RETURN:      TRUE if buffer contains a ASCII string, FALSE otherwise
 *
 * DESCRIPTION: Determine if a buffer Op contains a _PLD structure
 *
 ******************************************************************************/

BOOLEAN
AcpiDmIsPldBuffer (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_PARSE_OBJECT       *ParentOp;


    ParentOp = Op->Common.Parent;
    if (!ParentOp)
    {
        return (FALSE);
    }

    /* Check for form: Name(_PLD, Buffer() {}). Not legal, however */

    if (ParentOp->Common.AmlOpcode == AML_NAME_OP)
    {
        Node = ParentOp->Common.Node;

        if (ACPI_COMPARE_NAME (Node->Name.Ascii, METHOD_NAME__PLD))
        {
            return (TRUE);
        }

        return (FALSE);
    }

    /* Check for proper form: Name(_PLD, Package() {Buffer() {}}) */

    if (ParentOp->Common.AmlOpcode == AML_PACKAGE_OP)
    {
        ParentOp = ParentOp->Common.Parent;
        if (!ParentOp)
        {
            return (FALSE);
        }

        if (ParentOp->Common.AmlOpcode == AML_NAME_OP)
        {
            Node = ParentOp->Common.Node;

            if (ACPI_COMPARE_NAME (Node->Name.Ascii, METHOD_NAME__PLD))
            {
                return (TRUE);
            }
        }
    }

    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmPldBuffer
 *
 * PARAMETERS:  Level               - Current source code indentation level
 *              ByteData            - Pointer to the byte list
 *              ByteCount           - Length of the byte list
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump and format the contents of a _PLD buffer object
 *
 ******************************************************************************/

#define ACPI_PLD_OUTPUT08     "%*.s/* %18s : %-6.2X */\n", ACPI_MUL_4 (Level), " "
#define ACPI_PLD_OUTPUT16   "%*.s/* %18s : %-6.4X */\n", ACPI_MUL_4 (Level), " "
#define ACPI_PLD_OUTPUT24   "%*.s/* %18s : %-6.6X */\n", ACPI_MUL_4 (Level), " "

static void
AcpiDmPldBuffer (
    UINT32                  Level,
    UINT8                   *ByteData,
    UINT32                  ByteCount)
{
    ACPI_PLD_INFO           *PldInfo;
    ACPI_STATUS             Status;


    /* Check for valid byte count */

    if (ByteCount < ACPI_PLD_REV1_BUFFER_SIZE)
    {
        return;
    }

    /* Convert _PLD buffer to local _PLD struct */

    Status = AcpiDecodePldBuffer (ByteData, ByteCount, &PldInfo);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    /* First 32-bit dword */

    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "Revision", PldInfo->Revision);
    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "IgnoreColor", PldInfo->IgnoreColor);
    AcpiOsPrintf (ACPI_PLD_OUTPUT24,"Color", PldInfo->Color);

    /* Second 32-bit dword */

    AcpiOsPrintf (ACPI_PLD_OUTPUT16,"Width", PldInfo->Width);
    AcpiOsPrintf (ACPI_PLD_OUTPUT16,"Height", PldInfo->Height);

    /* Third 32-bit dword */

    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "UserVisible", PldInfo->UserVisible);
    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "Dock", PldInfo->Dock);
    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "Lid", PldInfo->Lid);
    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "Panel", PldInfo->Panel);
    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "VerticalPosition", PldInfo->VerticalPosition);
    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "HorizontalPosition", PldInfo->HorizontalPosition);
    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "Shape", PldInfo->Shape);
    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "GroupOrientation", PldInfo->GroupOrientation);
    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "GroupToken", PldInfo->GroupToken);
    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "GroupPosition", PldInfo->GroupPosition);
    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "Bay", PldInfo->Bay);

    /* Fourth 32-bit dword */

    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "Ejectable", PldInfo->Ejectable);
    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "OspmEjectRequired", PldInfo->OspmEjectRequired);
    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "CabinetNumber", PldInfo->CabinetNumber);
    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "CardCageNumber", PldInfo->CardCageNumber);
    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "Reference", PldInfo->Reference);
    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "Rotation", PldInfo->Rotation);
    AcpiOsPrintf (ACPI_PLD_OUTPUT08,  "Order", PldInfo->Order);

    /* Fifth 32-bit dword */

    if (ByteCount >= ACPI_PLD_REV1_BUFFER_SIZE)
    {
        AcpiOsPrintf (ACPI_PLD_OUTPUT16,"VerticalOffset", PldInfo->VerticalOffset);
        AcpiOsPrintf (ACPI_PLD_OUTPUT16,"HorizontalOffset", PldInfo->HorizontalOffset);
    }

    ACPI_FREE (PldInfo);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmUnicode
 *
 * PARAMETERS:  Op              - Byte List op containing Unicode string
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump Unicode string as a standard ASCII string. (Remove
 *              the extra zero bytes).
 *
 ******************************************************************************/

static void
AcpiDmUnicode (
    ACPI_PARSE_OBJECT       *Op)
{
    UINT16                  *WordData;
    UINT32                  WordCount;
    UINT32                  i;


    /* Extract the buffer info as a WORD buffer */

    WordData = ACPI_CAST_PTR (UINT16, Op->Named.Data);
    WordCount = ACPI_DIV_2 (((UINT32) Op->Common.Value.Integer));

    /* Write every other byte as an ASCII character */

    AcpiOsPrintf ("\"");
    for (i = 0; i < (WordCount - 1); i++)
    {
        AcpiOsPrintf ("%c", (int) WordData[i]);
    }

    AcpiOsPrintf ("\")");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIsEisaIdElement
 *
 * PARAMETERS:  Op              - Op to be examined
 *
 * RETURN:      None
 *
 * DESCRIPTION: Determine if an Op (argument to _HID or _CID) can be converted
 *              to an EISA ID.
 *
 ******************************************************************************/

static void
AcpiDmIsEisaIdElement (
    ACPI_PARSE_OBJECT       *Op)
{
    UINT32                  BigEndianId;
    UINT32                  Prefix[3];
    UINT32                  i;


    /* The parameter must be either a word or a dword */

    if ((Op->Common.AmlOpcode != AML_DWORD_OP) &&
        (Op->Common.AmlOpcode != AML_WORD_OP))
    {
        return;
    }

    /* Swap from little-endian to big-endian to simplify conversion */

    BigEndianId = AcpiUtDwordByteSwap ((UINT32) Op->Common.Value.Integer);

    /* Create the 3 leading ASCII letters */

    Prefix[0] = ((BigEndianId >> 26) & 0x1F) + 0x40;
    Prefix[1] = ((BigEndianId >> 21) & 0x1F) + 0x40;
    Prefix[2] = ((BigEndianId >> 16) & 0x1F) + 0x40;

    /* Verify that all 3 are ascii and alpha */

    for (i = 0; i < 3; i++)
    {
        if (!ACPI_IS_ASCII (Prefix[i]) ||
            !ACPI_IS_ALPHA (Prefix[i]))
        {
            return;
        }
    }

    /* OK - mark this node as convertable to an EISA ID */

    Op->Common.DisasmOpcode = ACPI_DASM_EISAID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIsEisaId
 *
 * PARAMETERS:  Op              - Op to be examined
 *
 * RETURN:      None
 *
 * DESCRIPTION: Determine if a Name() Op can be converted to an EisaId.
 *
 ******************************************************************************/

void
AcpiDmIsEisaId (
    ACPI_PARSE_OBJECT       *Op)
{
    UINT32                  Name;
    ACPI_PARSE_OBJECT       *NextOp;


    /* Get the NameSegment */

    Name = AcpiPsGetName (Op);
    if (!Name)
    {
        return;
    }

    NextOp = AcpiPsGetDepthNext (NULL, Op);
    if (!NextOp)
    {
        return;
    }

    /* Check for _HID - has one argument */

    if (ACPI_COMPARE_NAME (&Name, METHOD_NAME__HID))
    {
        AcpiDmIsEisaIdElement (NextOp);
        return;
    }

    /* Exit if not _CID */

    if (!ACPI_COMPARE_NAME (&Name, METHOD_NAME__CID))
    {
        return;
    }

    /* _CID can contain a single argument or a package */

    if (NextOp->Common.AmlOpcode != AML_PACKAGE_OP)
    {
        AcpiDmIsEisaIdElement (NextOp);
        return;
    }

    /* _CID with Package: get the package length */

    NextOp = AcpiPsGetDepthNext (NULL, NextOp);

    /* Don't need to use the length, just walk the peer list */

    NextOp = NextOp->Common.Next;
    while (NextOp)
    {
        AcpiDmIsEisaIdElement (NextOp);
        NextOp = NextOp->Common.Next;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmEisaId
 *
 * PARAMETERS:  EncodedId       - Raw encoded EISA ID.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert an encoded EISAID back to the original ASCII String.
 *
 ******************************************************************************/

void
AcpiDmEisaId (
    UINT32                  EncodedId)
{
    UINT32                  BigEndianId;


    /* Swap from little-endian to big-endian to simplify conversion */

    BigEndianId = AcpiUtDwordByteSwap (EncodedId);


    /* Split to form "AAANNNN" string */

    AcpiOsPrintf ("EisaId (\"%c%c%c%4.4X\")",

        /* Three Alpha characters (AAA), 5 bits each */

        (int) ((BigEndianId >> 26) & 0x1F) + 0x40,
        (int) ((BigEndianId >> 21) & 0x1F) + 0x40,
        (int) ((BigEndianId >> 16) & 0x1F) + 0x40,

        /* Numeric part (NNNN) is simply the lower 16 bits */

        (UINT32) (BigEndianId & 0xFFFF));
}

#endif
