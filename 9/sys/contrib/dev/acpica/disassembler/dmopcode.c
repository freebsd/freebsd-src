/*******************************************************************************
 *
 * Module Name: dmopcode - AML disassembler, specific AML opcodes
 *
 ******************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
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
#include <contrib/dev/acpica/include/acparser.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acdisasm.h>

#ifdef ACPI_DISASSEMBLER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dmopcode")

/* Local prototypes */

static void
AcpiDmMatchKeyword (
    ACPI_PARSE_OBJECT       *Op);


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmMethodFlags
 *
 * PARAMETERS:  Op              - Method Object to be examined
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode control method flags
 *
 ******************************************************************************/

void
AcpiDmMethodFlags (
    ACPI_PARSE_OBJECT       *Op)
{
    UINT32                  Flags;
    UINT32                  Args;


    /* The next Op contains the flags */

    Op = AcpiPsGetDepthNext (NULL, Op);
    Flags = (UINT8) Op->Common.Value.Integer;
    Args = Flags & 0x07;

    /* Mark the Op as completed */

    Op->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;

    /* 1) Method argument count */

    AcpiOsPrintf (", %u, ", Args);

    /* 2) Serialize rule */

    if (!(Flags & 0x08))
    {
        AcpiOsPrintf ("Not");
    }

    AcpiOsPrintf ("Serialized");

    /* 3) SyncLevel */

    if (Flags & 0xF0)
    {
        AcpiOsPrintf (", %u", Flags >> 4);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmFieldFlags
 *
 * PARAMETERS:  Op              - Field Object to be examined
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode Field definition flags
 *
 ******************************************************************************/

void
AcpiDmFieldFlags (
    ACPI_PARSE_OBJECT       *Op)
{
    UINT32                  Flags;


    Op = Op->Common.Next;
    Flags = (UINT8) Op->Common.Value.Integer;

    /* Mark the Op as completed */

    Op->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;

    AcpiOsPrintf ("%s, ", AcpiGbl_AccessTypes [Flags & 0x07]);
    AcpiOsPrintf ("%s, ", AcpiGbl_LockRule [(Flags & 0x10) >> 4]);
    AcpiOsPrintf ("%s)",  AcpiGbl_UpdateRules [(Flags & 0x60) >> 5]);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmAddressSpace
 *
 * PARAMETERS:  SpaceId         - ID to be translated
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a SpaceId to an AddressSpaceKeyword
 *
 ******************************************************************************/

void
AcpiDmAddressSpace (
    UINT8                   SpaceId)
{

    if (SpaceId >= ACPI_NUM_PREDEFINED_REGIONS)
    {
        if (SpaceId == 0x7F)
        {
            AcpiOsPrintf ("FFixedHW, ");
        }
        else
        {
            AcpiOsPrintf ("0x%.2X, ", SpaceId);
        }
    }
    else
    {
        AcpiOsPrintf ("%s, ", AcpiGbl_RegionTypes [SpaceId]);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmRegionFlags
 *
 * PARAMETERS:  Op              - Object to be examined
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode OperationRegion flags
 *
 ******************************************************************************/

void
AcpiDmRegionFlags (
    ACPI_PARSE_OBJECT       *Op)
{


    /* The next Op contains the SpaceId */

    Op = AcpiPsGetDepthNext (NULL, Op);

    /* Mark the Op as completed */

    Op->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;

    AcpiOsPrintf (", ");
    AcpiDmAddressSpace ((UINT8) Op->Common.Value.Integer);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmMatchOp
 *
 * PARAMETERS:  Op              - Match Object to be examined
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode Match opcode operands
 *
 ******************************************************************************/

void
AcpiDmMatchOp (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *NextOp;


    NextOp = AcpiPsGetDepthNext (NULL, Op);
    NextOp = NextOp->Common.Next;

    if (!NextOp)
    {
        /* Handle partial tree during single-step */

        return;
    }

    /* Mark the two nodes that contain the encoding for the match keywords */

    NextOp->Common.DisasmOpcode = ACPI_DASM_MATCHOP;

    NextOp = NextOp->Common.Next;
    NextOp = NextOp->Common.Next;
    NextOp->Common.DisasmOpcode = ACPI_DASM_MATCHOP;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmMatchKeyword
 *
 * PARAMETERS:  Op              - Match Object to be examined
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode Match opcode operands
 *
 ******************************************************************************/

static void
AcpiDmMatchKeyword (
    ACPI_PARSE_OBJECT       *Op)
{


    if (((UINT32) Op->Common.Value.Integer) > ACPI_MAX_MATCH_OPCODE)
    {
        AcpiOsPrintf ("/* Unknown Match Keyword encoding */");
    }
    else
    {
        AcpiOsPrintf ("%s", ACPI_CAST_PTR (char,
            AcpiGbl_MatchOps[(ACPI_SIZE) Op->Common.Value.Integer]));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDisassembleOneOp
 *
 * PARAMETERS:  WalkState           - Current walk info
 *              Info                - Parse tree walk info
 *              Op                  - Op that is to be printed
 *
 * RETURN:      None
 *
 * DESCRIPTION: Disassemble a single AML opcode
 *
 ******************************************************************************/

void
AcpiDmDisassembleOneOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_OP_WALK_INFO       *Info,
    ACPI_PARSE_OBJECT       *Op)
{
    const ACPI_OPCODE_INFO  *OpInfo = NULL;
    UINT32                  Offset;
    UINT32                  Length;
    ACPI_PARSE_OBJECT       *Child;
    ACPI_STATUS             Status;


    if (!Op)
    {
        AcpiOsPrintf ("<NULL OP PTR>");
        return;
    }

    switch (Op->Common.DisasmOpcode)
    {
    case ACPI_DASM_MATCHOP:

        AcpiDmMatchKeyword (Op);
        return;

    case ACPI_DASM_LNOT_SUFFIX:
        switch (Op->Common.AmlOpcode)
        {
        case AML_LEQUAL_OP:
            AcpiOsPrintf ("LNotEqual");
            break;

        case AML_LGREATER_OP:
            AcpiOsPrintf ("LLessEqual");
            break;

        case AML_LLESS_OP:
            AcpiOsPrintf ("LGreaterEqual");
            break;

        default:
            break;
        }
        Op->Common.DisasmOpcode = 0;
        Op->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;
        return;

    default:
        break;
    }


    OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);

    /* The op and arguments */

    switch (Op->Common.AmlOpcode)
    {
    case AML_LNOT_OP:

        Child = Op->Common.Value.Arg;
        if ((Child->Common.AmlOpcode == AML_LEQUAL_OP) ||
            (Child->Common.AmlOpcode == AML_LGREATER_OP) ||
            (Child->Common.AmlOpcode == AML_LLESS_OP))
        {
            Child->Common.DisasmOpcode = ACPI_DASM_LNOT_SUFFIX;
            Op->Common.DisasmOpcode = ACPI_DASM_LNOT_PREFIX;
        }
        else
        {
            AcpiOsPrintf ("%s", OpInfo->Name);
        }
        break;

    case AML_BYTE_OP:

        AcpiOsPrintf ("0x%2.2X", (UINT32) Op->Common.Value.Integer);
        break;


    case AML_WORD_OP:

        if (Op->Common.DisasmOpcode == ACPI_DASM_EISAID)
        {
            AcpiDmEisaId ((UINT32) Op->Common.Value.Integer);
        }
        else
        {
            AcpiOsPrintf ("0x%4.4X", (UINT32) Op->Common.Value.Integer);
        }
        break;


    case AML_DWORD_OP:

        if (Op->Common.DisasmOpcode == ACPI_DASM_EISAID)
        {
            AcpiDmEisaId ((UINT32) Op->Common.Value.Integer);
        }
        else
        {
            AcpiOsPrintf ("0x%8.8X", (UINT32) Op->Common.Value.Integer);
        }
        break;


    case AML_QWORD_OP:

        AcpiOsPrintf ("0x%8.8X%8.8X",
            ACPI_FORMAT_UINT64 (Op->Common.Value.Integer));
        break;


    case AML_STRING_OP:

        AcpiUtPrintString (Op->Common.Value.String, ACPI_UINT8_MAX);
        break;


    case AML_BUFFER_OP:

        /*
         * Determine the type of buffer.  We can have one of the following:
         *
         * 1) ResourceTemplate containing Resource Descriptors.
         * 2) Unicode String buffer
         * 3) ASCII String buffer
         * 4) Raw data buffer (if none of the above)
         *
         * Since there are no special AML opcodes to differentiate these
         * types of buffers, we have to closely look at the data in the
         * buffer to determine the type.
         */
        Status = AcpiDmIsResourceTemplate (Op);
        if (ACPI_SUCCESS (Status))
        {
            Op->Common.DisasmOpcode = ACPI_DASM_RESOURCE;
            AcpiOsPrintf ("ResourceTemplate");
            break;
        }
        else if (Status == AE_AML_NO_RESOURCE_END_TAG)
        {
            AcpiOsPrintf ("/**** Is ResourceTemplate, but EndTag not at buffer end ****/ ");
        }

        if (AcpiDmIsUnicodeBuffer (Op))
        {
            Op->Common.DisasmOpcode = ACPI_DASM_UNICODE;
            AcpiOsPrintf ("Unicode (");
        }
        else if (AcpiDmIsStringBuffer (Op))
        {
            Op->Common.DisasmOpcode = ACPI_DASM_STRING;
            AcpiOsPrintf ("Buffer");
        }
        else
        {
            Op->Common.DisasmOpcode = ACPI_DASM_BUFFER;
            AcpiOsPrintf ("Buffer");
        }
        break;


    case AML_INT_STATICSTRING_OP:

        if (Op->Common.Value.String)
        {
            AcpiOsPrintf ("%s", Op->Common.Value.String);
        }
        else
        {
            AcpiOsPrintf ("\"<NULL STATIC STRING PTR>\"");
        }
        break;


    case AML_INT_NAMEPATH_OP:

        AcpiDmNamestring (Op->Common.Value.Name);
        break;


    case AML_INT_NAMEDFIELD_OP:

        Length = AcpiDmDumpName (Op->Named.Name);
        AcpiOsPrintf (",%*.s  %u", (unsigned) (5 - Length), " ",
            (UINT32) Op->Common.Value.Integer);
        AcpiDmCommaIfFieldMember (Op);

        Info->BitOffset += (UINT32) Op->Common.Value.Integer;
        break;


    case AML_INT_RESERVEDFIELD_OP:

        /* Offset() -- Must account for previous offsets */

        Offset = (UINT32) Op->Common.Value.Integer;
        Info->BitOffset += Offset;

        if (Info->BitOffset % 8 == 0)
        {
            AcpiOsPrintf ("        Offset (0x%.2X)", ACPI_DIV_8 (Info->BitOffset));
        }
        else
        {
            AcpiOsPrintf ("    ,   %u", Offset);
        }

        AcpiDmCommaIfFieldMember (Op);
        break;


    case AML_INT_ACCESSFIELD_OP:

        AcpiOsPrintf ("        AccessAs (%s, ",
            AcpiGbl_AccessTypes [(UINT32) (Op->Common.Value.Integer >> 8) & 0x7]);

        AcpiDmDecodeAttribute ((UINT8) Op->Common.Value.Integer);
        AcpiOsPrintf (")");
        AcpiDmCommaIfFieldMember (Op);
        break;


    case AML_INT_BYTELIST_OP:

        AcpiDmByteList (Info, Op);
        break;


    case AML_INT_METHODCALL_OP:

        Op = AcpiPsGetDepthNext (NULL, Op);
        Op->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;

        AcpiDmNamestring (Op->Common.Value.Name);
        break;


    default:

        /* Just get the opcode name and print it */

        AcpiOsPrintf ("%s", OpInfo->Name);


#ifdef ACPI_DEBUGGER

        if ((Op->Common.AmlOpcode == AML_INT_RETURN_VALUE_OP) &&
            (WalkState) &&
            (WalkState->Results) &&
            (WalkState->ResultCount))
        {
            AcpiDmDecodeInternalObject (
                WalkState->Results->Results.ObjDesc [
                    (WalkState->ResultCount - 1) %
                        ACPI_RESULTS_FRAME_OBJ_NUM]);
        }
#endif

        break;
    }
}

#endif  /* ACPI_DISASSEMBLER */
