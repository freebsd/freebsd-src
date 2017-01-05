/*******************************************************************************
 *
 * Module Name: dmopcode - AML disassembler, specific AML opcodes
 *
 ******************************************************************************/

/*
 * Copyright (C) 2000 - 2016, Intel Corp.
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
#include <contrib/dev/acpica/include/acinterp.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acdebug.h>


#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dmopcode")


/* Local prototypes */

static void
AcpiDmMatchKeyword (
    ACPI_PARSE_OBJECT       *Op);

static void
AcpiDmConvertToElseIf (
    ACPI_PARSE_OBJECT       *Op);

static void
AcpiDmPromoteSubtree (
    ACPI_PARSE_OBJECT       *StartOp);

static BOOLEAN
AcpiDmIsSwitchBlock (
    ACPI_PARSE_OBJECT       *Op);

static BOOLEAN
AcpiDmIsCaseBlock (
    ACPI_PARSE_OBJECT       *Op);

/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDisplayTargetPathname
 *
 * PARAMETERS:  Op              - Parse object
 *
 * RETURN:      None
 *
 * DESCRIPTION: For AML opcodes that have a target operand, display the full
 *              pathname for the target, in a comment field. Handles Return()
 *              statements also.
 *
 ******************************************************************************/

void
AcpiDmDisplayTargetPathname (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *NextOp;
    ACPI_PARSE_OBJECT       *PrevOp = NULL;
    char                    *Pathname;
    const ACPI_OPCODE_INFO  *OpInfo;


    if (Op->Common.AmlOpcode == AML_RETURN_OP)
    {
        PrevOp = Op->Asl.Value.Arg;
    }
    else
    {
        OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
        if (!(OpInfo->Flags & AML_HAS_TARGET))
        {
            return;
        }

        /* Target is the last Op in the arg list */

        NextOp = Op->Asl.Value.Arg;
        while (NextOp)
        {
            PrevOp = NextOp;
            NextOp = PrevOp->Asl.Next;
        }
    }

    if (!PrevOp)
    {
        return;
    }

    /* We must have a namepath AML opcode */

    if (PrevOp->Asl.AmlOpcode != AML_INT_NAMEPATH_OP)
    {
        return;
    }

    /* A null string is the "no target specified" case */

    if (!PrevOp->Asl.Value.String)
    {
        return;
    }

    /* No node means "unresolved external reference" */

    if (!PrevOp->Asl.Node)
    {
        AcpiOsPrintf (" /* External reference */");
        return;
    }

    /* Ignore if path is already from the root */

    if (*PrevOp->Asl.Value.String == '\\')
    {
        return;
    }

    /* Now: we can get the full pathname */

    Pathname = AcpiNsGetExternalPathname (PrevOp->Asl.Node);
    if (!Pathname)
    {
        return;
    }

    AcpiOsPrintf (" /* %s */", Pathname);
    ACPI_FREE (Pathname);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmNotifyDescription
 *
 * PARAMETERS:  Op              - Name() parse object
 *
 * RETURN:      None
 *
 * DESCRIPTION: Emit a description comment for the value associated with a
 *              Notify() operator.
 *
 ******************************************************************************/

void
AcpiDmNotifyDescription (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *NextOp;
    ACPI_NAMESPACE_NODE     *Node;
    UINT8                   NotifyValue;
    UINT8                   Type = ACPI_TYPE_ANY;


    /* The notify value is the second argument */

    NextOp = Op->Asl.Value.Arg;
    NextOp = NextOp->Asl.Next;

    switch (NextOp->Common.AmlOpcode)
    {
    case AML_ZERO_OP:
    case AML_ONE_OP:

        NotifyValue = (UINT8) NextOp->Common.AmlOpcode;
        break;

    case AML_BYTE_OP:

        NotifyValue = (UINT8) NextOp->Asl.Value.Integer;
        break;

    default:
        return;
    }

    /*
     * Attempt to get the namespace node so we can determine the object type.
     * Some notify values are dependent on the object type (Device, Thermal,
     * or Processor).
     */
    Node = Op->Asl.Node;
    if (Node)
    {
        Type = Node->Type;
    }

    AcpiOsPrintf (" // %s", AcpiUtGetNotifyName (NotifyValue, Type));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmPredefinedDescription
 *
 * PARAMETERS:  Op              - Name() parse object
 *
 * RETURN:      None
 *
 * DESCRIPTION: Emit a description comment for a predefined ACPI name.
 *              Used for iASL compiler only.
 *
 ******************************************************************************/

void
AcpiDmPredefinedDescription (
    ACPI_PARSE_OBJECT       *Op)
{
#ifdef ACPI_ASL_COMPILER
    const AH_PREDEFINED_NAME    *Info;
    char                        *NameString;
    int                         LastCharIsDigit;
    int                         LastCharsAreHex;


    if (!Op)
    {
        return;
    }

    /* Ensure that the comment field is emitted only once */

    if (Op->Common.DisasmFlags & ACPI_PARSEOP_PREDEFINED_CHECKED)
    {
        return;
    }
    Op->Common.DisasmFlags |= ACPI_PARSEOP_PREDEFINED_CHECKED;

    /* Predefined name must start with an underscore */

    NameString = ACPI_CAST_PTR (char, &Op->Named.Name);
    if (NameString[0] != '_')
    {
        return;
    }

    /*
     * Check for the special ACPI names:
     * _ACd, _ALd, _EJd, _Exx, _Lxx, _Qxx, _Wxx, _T_a
     * (where d=decimal_digit, x=hex_digit, a=anything)
     *
     * Convert these to the generic name for table lookup.
     * Note: NameString is guaranteed to be upper case here.
     */
    LastCharIsDigit =
        (isdigit ((int) NameString[3]));    /* d */
    LastCharsAreHex =
        (isxdigit ((int) NameString[2]) &&  /* xx */
         isxdigit ((int) NameString[3]));

    switch (NameString[1])
    {
    case 'A':

        if ((NameString[2] == 'C') && (LastCharIsDigit))
        {
            NameString = "_ACx";
        }
        else if ((NameString[2] == 'L') && (LastCharIsDigit))
        {
            NameString = "_ALx";
        }
        break;

    case 'E':

        if ((NameString[2] == 'J') && (LastCharIsDigit))
        {
            NameString = "_EJx";
        }
        else if (LastCharsAreHex)
        {
            NameString = "_Exx";
        }
        break;

    case 'L':

        if (LastCharsAreHex)
        {
            NameString = "_Lxx";
        }
        break;

    case 'Q':

        if (LastCharsAreHex)
        {
            NameString = "_Qxx";
        }
        break;

    case 'T':

        if (NameString[2] == '_')
        {
            NameString = "_T_x";
        }
        break;

    case 'W':

        if (LastCharsAreHex)
        {
            NameString = "_Wxx";
        }
        break;

    default:

        break;
    }

    /* Match the name in the info table */

    Info = AcpiAhMatchPredefinedName (NameString);
    if (Info)
    {
        AcpiOsPrintf ("  // %4.4s: %s",
            NameString, ACPI_CAST_PTR (char, Info->Description));
    }

#endif
    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmFieldPredefinedDescription
 *
 * PARAMETERS:  Op              - Parse object
 *
 * RETURN:      None
 *
 * DESCRIPTION: Emit a description comment for a resource descriptor tag
 *              (which is a predefined ACPI name.) Used for iASL compiler only.
 *
 ******************************************************************************/

void
AcpiDmFieldPredefinedDescription (
    ACPI_PARSE_OBJECT       *Op)
{
#ifdef ACPI_ASL_COMPILER
    ACPI_PARSE_OBJECT       *IndexOp;
    char                    *Tag;
    const ACPI_OPCODE_INFO  *OpInfo;
    const AH_PREDEFINED_NAME *Info;


    if (!Op)
    {
        return;
    }

    /* Ensure that the comment field is emitted only once */

    if (Op->Common.DisasmFlags & ACPI_PARSEOP_PREDEFINED_CHECKED)
    {
        return;
    }
    Op->Common.DisasmFlags |= ACPI_PARSEOP_PREDEFINED_CHECKED;

    /*
     * Op must be one of the Create* operators: CreateField, CreateBitField,
     * CreateByteField, CreateWordField, CreateDwordField, CreateQwordField
     */
    OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
    if (!(OpInfo->Flags & AML_CREATE))
    {
        return;
    }

    /* Second argument is the Index argument */

    IndexOp = Op->Common.Value.Arg;
    IndexOp = IndexOp->Common.Next;

    /* Index argument must be a namepath */

    if (IndexOp->Common.AmlOpcode != AML_INT_NAMEPATH_OP)
    {
        return;
    }

    /* Major cheat: We previously put the Tag ptr in the Node field */

    Tag = ACPI_CAST_PTR (char, IndexOp->Common.Node);
    if (!Tag)
    {
        return;
    }

    /* Match the name in the info table */

    Info = AcpiAhMatchPredefinedName (Tag);
    if (Info)
    {
        AcpiOsPrintf ("  // %4.4s: %s", Tag,
            ACPI_CAST_PTR (char, Info->Description));
    }

#endif
    return;
}


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
        AcpiOsPrintf ("%s",
            AcpiGbl_MatchOps[(ACPI_SIZE) Op->Common.Value.Integer]);
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
    UINT8                   *Aml;
    const AH_DEVICE_ID      *IdInfo;


    if (!Op)
    {
        AcpiOsPrintf ("<NULL OP PTR>");
        return;
    }

    if (Op->Common.DisasmFlags & ACPI_PARSEOP_ELSEIF)
    {
        return; /* ElseIf macro was already emitted */
    }

    switch (Op->Common.DisasmOpcode)
    {
    case ACPI_DASM_MATCHOP:

        AcpiDmMatchKeyword (Op);
        return;

    case ACPI_DASM_LNOT_SUFFIX:

        if (!AcpiGbl_CstyleDisassembly)
        {
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
            AcpiDmDecompressEisaId ((UINT32) Op->Common.Value.Integer);
        }
        else
        {
            AcpiOsPrintf ("0x%4.4X", (UINT32) Op->Common.Value.Integer);
        }
        break;

    case AML_DWORD_OP:

        if (Op->Common.DisasmOpcode == ACPI_DASM_EISAID)
        {
            AcpiDmDecompressEisaId ((UINT32) Op->Common.Value.Integer);
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

        AcpiUtPrintString (Op->Common.Value.String, ACPI_UINT16_MAX);

        /* For _HID/_CID strings, attempt to output a descriptive comment */

        if (Op->Common.DisasmOpcode == ACPI_DASM_HID_STRING)
        {
            /* If we know about the ID, emit the description */

            IdInfo = AcpiAhMatchHardwareId (Op->Common.Value.String);
            if (IdInfo)
            {
                AcpiOsPrintf (" /* %s */", IdInfo->Description);
            }
        }
        break;

    case AML_BUFFER_OP:
        /*
         * Determine the type of buffer. We can have one of the following:
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
        if (!AcpiGbl_NoResourceDisassembly)
        {
            Status = AcpiDmIsResourceTemplate (WalkState, Op);
            if (ACPI_SUCCESS (Status))
            {
                Op->Common.DisasmOpcode = ACPI_DASM_RESOURCE;
                AcpiOsPrintf ("ResourceTemplate");
                break;
            }
            else if (Status == AE_AML_NO_RESOURCE_END_TAG)
            {
                AcpiOsPrintf (
                    "/**** Is ResourceTemplate, "
                    "but EndTag not at buffer end ****/ ");
            }
        }

        if (AcpiDmIsUuidBuffer (Op))
        {
            Op->Common.DisasmOpcode = ACPI_DASM_UUID;
            AcpiOsPrintf ("ToUUID (");
        }
        else if (AcpiDmIsUnicodeBuffer (Op))
        {
            Op->Common.DisasmOpcode = ACPI_DASM_UNICODE;
            AcpiOsPrintf ("Unicode (");
        }
        else if (AcpiDmIsStringBuffer (Op))
        {
            Op->Common.DisasmOpcode = ACPI_DASM_STRING;
            AcpiOsPrintf ("Buffer");
        }
        else if (AcpiDmIsPldBuffer (Op))
        {
            Op->Common.DisasmOpcode = ACPI_DASM_PLD_METHOD;
            AcpiOsPrintf ("ToPLD (");
        }
        else
        {
            Op->Common.DisasmOpcode = ACPI_DASM_BUFFER;
            AcpiOsPrintf ("Buffer");
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
            AcpiOsPrintf ("Offset (0x%.2X)", ACPI_DIV_8 (Info->BitOffset));
        }
        else
        {
            AcpiOsPrintf ("    ,   %u", Offset);
        }

        AcpiDmCommaIfFieldMember (Op);
        break;

    case AML_INT_ACCESSFIELD_OP:
    case AML_INT_EXTACCESSFIELD_OP:

        AcpiOsPrintf ("AccessAs (%s, ",
            AcpiGbl_AccessTypes [(UINT32) (Op->Common.Value.Integer & 0x7)]);

        AcpiDmDecodeAttribute ((UINT8) (Op->Common.Value.Integer >> 8));

        if (Op->Common.AmlOpcode == AML_INT_EXTACCESSFIELD_OP)
        {
            AcpiOsPrintf (" (0x%2.2X)", (unsigned)
                ((Op->Common.Value.Integer >> 16) & 0xFF));
        }

        AcpiOsPrintf (")");
        AcpiDmCommaIfFieldMember (Op);
        break;

    case AML_INT_CONNECTION_OP:
        /*
         * Two types of Connection() - one with a buffer object, the
         * other with a namestring that points to a buffer object.
         */
        AcpiOsPrintf ("Connection (");
        Child = Op->Common.Value.Arg;

        if (Child->Common.AmlOpcode == AML_INT_BYTELIST_OP)
        {
            AcpiOsPrintf ("\n");

            Aml = Child->Named.Data;
            Length = (UINT32) Child->Common.Value.Integer;

            Info->Level += 1;
            Info->MappingOp = Op;
            Op->Common.DisasmOpcode = ACPI_DASM_RESOURCE;

            AcpiDmResourceTemplate (Info, Op->Common.Parent, Aml, Length);

            Info->Level -= 1;
            AcpiDmIndent (Info->Level);
        }
        else
        {
            AcpiDmNamestring (Child->Common.Value.Name);
        }

        AcpiOsPrintf (")");
        AcpiDmCommaIfFieldMember (Op);
        AcpiOsPrintf ("\n");

        Op->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE; /* for now, ignore in AcpiDmAscendingOp */
        Child->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;
        break;

    case AML_INT_BYTELIST_OP:

        AcpiDmByteList (Info, Op);
        break;

    case AML_INT_METHODCALL_OP:

        Op = AcpiPsGetDepthNext (NULL, Op);
        Op->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;

        AcpiDmNamestring (Op->Common.Value.Name);
        break;

    case AML_WHILE_OP:

        if (AcpiDmIsSwitchBlock(Op))
        {
            AcpiOsPrintf ("%s", "Switch");
            break;
        }

        AcpiOsPrintf ("%s", OpInfo->Name);
        break;

    case AML_IF_OP:

        if (Op->Common.DisasmOpcode == ACPI_DASM_CASE)
        {
            AcpiOsPrintf ("%s", "Case");
            break;
        }

        AcpiOsPrintf ("%s", OpInfo->Name);
        break;

    case AML_ELSE_OP:

        AcpiDmConvertToElseIf (Op);
        break;

    case AML_EXTERNAL_OP:

        if (AcpiGbl_DmEmitExternalOpcodes)
        {
            AcpiOsPrintf ("/* Opcode 0x15 */ ");

            /* Fallthrough */
        }
        else
        {
            break;
        }

    default:

        /* Just get the opcode name and print it */

        AcpiOsPrintf ("%s", OpInfo->Name);


#ifdef ACPI_DEBUGGER

        if ((Op->Common.AmlOpcode == AML_INT_RETURN_VALUE_OP) &&
            (WalkState) &&
            (WalkState->Results) &&
            (WalkState->ResultCount))
        {
            AcpiDbDecodeInternalObject (
                WalkState->Results->Results.ObjDesc [
                    (WalkState->ResultCount - 1) %
                        ACPI_RESULTS_FRAME_OBJ_NUM]);
        }
#endif

        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmConvertToElseIf
 *
 * PARAMETERS:  OriginalElseOp          - ELSE Object to be examined
 *
 * RETURN:      None. Emits either an "Else" or an "ElseIf" ASL operator.
 *
 * DESCRIPTION: Detect and convert an If..Else..If sequence to If..ElseIf
 *
 * EXAMPLE:
 *
 * This If..Else..If nested sequence:
 *
 *        If (Arg0 == 1)
 *        {
 *            Local0 = 4
 *        }
 *        Else
 *        {
 *            If (Arg0 == 2)
 *            {
 *                Local0 = 5
 *            }
 *        }
 *
 * Is converted to this simpler If..ElseIf sequence:
 *
 *        If (Arg0 == 1)
 *        {
 *            Local0 = 4
 *        }
 *        ElseIf (Arg0 == 2)
 *        {
 *            Local0 = 5
 *        }
 *
 * NOTE: There is no actual ElseIf AML opcode. ElseIf is essentially an ASL
 * macro that emits an Else opcode followed by an If opcode. This function
 * reverses these AML sequences back to an ElseIf macro where possible. This
 * can make the disassembled ASL code simpler and more like the original code.
 *
 ******************************************************************************/

static void
AcpiDmConvertToElseIf (
    ACPI_PARSE_OBJECT       *OriginalElseOp)
{
    ACPI_PARSE_OBJECT       *IfOp;
    ACPI_PARSE_OBJECT       *ElseOp;


    /*
     * To be able to perform the conversion, two conditions must be satisfied:
     * 1) The first child of the Else must be an If statement.
     * 2) The If block can only be followed by an Else block and these must
     *    be the only blocks under the original Else.
     */
    IfOp = OriginalElseOp->Common.Value.Arg;

    if (!IfOp ||
        (IfOp->Common.AmlOpcode != AML_IF_OP) ||
        (IfOp->Asl.Next && (IfOp->Asl.Next->Common.AmlOpcode != AML_ELSE_OP)))
    {
        /* Not a proper Else..If sequence, cannot convert to ElseIf */

        if (OriginalElseOp->Common.DisasmOpcode == ACPI_DASM_DEFAULT)
        {
            AcpiOsPrintf ("%s", "Default");
            return;
        }

        AcpiOsPrintf ("%s", "Else");
        return;
    }

    /* Cannot have anything following the If...Else block */

    ElseOp = IfOp->Common.Next;
    if (ElseOp && ElseOp->Common.Next)
    {
        if (OriginalElseOp->Common.DisasmOpcode == ACPI_DASM_DEFAULT)
        {
            AcpiOsPrintf ("%s", "Default");
            return;
        }

        AcpiOsPrintf ("%s", "Else");
        return;
    }

    if (OriginalElseOp->Common.DisasmOpcode == ACPI_DASM_DEFAULT)
    {
        /*
         * There is an ElseIf but in this case the Else is actually
         * a Default block for a Switch/Case statement. No conversion.
         */
        AcpiOsPrintf ("%s", "Default");
        return;
    }

    if (OriginalElseOp->Common.DisasmOpcode == ACPI_DASM_CASE)
    {
        /*
         * This ElseIf is actually a Case block for a Switch/Case
         * statement. Print Case but do not return so that we can
         * promote the subtree and keep the indentation level.
         */
        AcpiOsPrintf ("%s", "Case");
    }
    else
    {
       /* Emit ElseIf, mark the IF as now an ELSEIF */

        AcpiOsPrintf ("%s", "ElseIf");
    }

    IfOp->Common.DisasmFlags |= ACPI_PARSEOP_ELSEIF;

    /* The IF parent will now be the same as the original ELSE parent */

    IfOp->Common.Parent = OriginalElseOp->Common.Parent;

    /*
     * Update the NEXT pointers to restructure the parse tree, essentially
     * promoting an If..Else block up to the same level as the original
     * Else.
     *
     * Check if the IF has a corresponding ELSE peer
     */
    ElseOp = IfOp->Common.Next;
    if (ElseOp &&
        (ElseOp->Common.AmlOpcode == AML_ELSE_OP))
    {
        /* If an ELSE matches the IF, promote it also */

        ElseOp->Common.Parent = OriginalElseOp->Common.Parent;

        /* Promote the entire block under the ElseIf (All Next OPs) */

        AcpiDmPromoteSubtree (OriginalElseOp);
    }
    else
    {
        /* Otherwise, set the IF NEXT to the original ELSE NEXT */

        IfOp->Common.Next = OriginalElseOp->Common.Next;
    }

    /* Detach the child IF block from the original ELSE */

    OriginalElseOp->Common.Value.Arg = NULL;

    /* Ignore the original ELSE from now on */

    OriginalElseOp->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;
    OriginalElseOp->Common.DisasmOpcode = ACPI_DASM_LNOT_PREFIX;

    /* Insert IF (now ELSEIF) as next peer of the original ELSE */

    OriginalElseOp->Common.Next = IfOp;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmPromoteSubtree
 *
 * PARAMETERS:  StartOpOp           - Original parent of the entire subtree
 *
 * RETURN:      None
 *
 * DESCRIPTION: Promote an entire parse subtree up one level.
 *
 ******************************************************************************/

static void
AcpiDmPromoteSubtree (
    ACPI_PARSE_OBJECT       *StartOp)
{
    ACPI_PARSE_OBJECT       *Op;
    ACPI_PARSE_OBJECT       *ParentOp;


    /* New parent for subtree elements */

    ParentOp = StartOp->Common.Parent;

    /* First child starts the subtree */

    Op = StartOp->Common.Value.Arg;

    /* Walk the top-level elements of the subtree */

    while (Op)
    {
        Op->Common.Parent = ParentOp;
        if (!Op->Common.Next)
        {
            /* Last Op in list, update its next field */

            Op->Common.Next = StartOp->Common.Next;
            break;
        }
        Op = Op->Common.Next;
    }
}

/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIsTempName
 *
 * PARAMETERS:  Op              - Object to be examined
 *
 * RETURN:      TRUE if object is a temporary (_T_x) name
 *
 * DESCRIPTION: Determine if an object is a temporary name and ignore it.
 *              Temporary names are only used for Switch statements. This
 *              function depends on this restriced usage.
 *
 ******************************************************************************/

BOOLEAN
AcpiDmIsTempName (
    ACPI_PARSE_OBJECT       *Op)
{
    char                    *Temp;

    if (Op->Common.AmlOpcode != AML_NAME_OP)
    {
        return (FALSE);
    }

    Temp = (char *)(Op->Common.Aml);
    ++Temp;

    if (strncmp(Temp, "_T_", 3))
    {
        return (FALSE);
    }

    /* Ignore Op */

    Op->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;

    return (TRUE);
}

/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIsSwitchBlock
 *
 * PARAMETERS:  Op              - While Object
 *
 * RETURN:      TRUE if While block can be converted to a Switch/Case block
 *
 * DESCRIPTION: Determines if While block is a Switch/Case statement. Modifies
 *              parse tree to allow for Switch/Case disassembly during walk.
 *
 * EXAMPLE: Example of parse tree to be converted
 *
 *    While
 *        One
 *        Store
 *            ByteConst
 *             -NamePath-
 *        If
 *            LEqual
 *                -NamePath-
 *                Zero
 *            Return
 *                One
 *        Else
 *            Return
 *                WordConst
 *        Break
 *
 ******************************************************************************/

static BOOLEAN
AcpiDmIsSwitchBlock (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *OneOp;
    ACPI_PARSE_OBJECT       *StoreOp;
    ACPI_PARSE_OBJECT       *NamePathOp;
    ACPI_PARSE_OBJECT       *PredicateOp;
    ACPI_PARSE_OBJECT       *CurrentOp;
    ACPI_PARSE_OBJECT       *TempOp;

    /* Check for One Op Predicate */

    OneOp = AcpiPsGetArg (Op, 0);
    if (!OneOp || (OneOp->Common.AmlOpcode != AML_ONE_OP))
    {
        return (FALSE);
    }

    /* Check for Store Op */

    StoreOp = OneOp->Common.Next;
    if (!StoreOp || (StoreOp->Common.AmlOpcode != AML_STORE_OP))
    {
        return (FALSE);
    }

    /* Check for Name Op with _T_ string */

    NamePathOp = AcpiPsGetArg (StoreOp, 1);
    if (!NamePathOp || (NamePathOp->Common.AmlOpcode != AML_INT_NAMEPATH_OP))
    {
        return (FALSE);
    }

    if (strncmp((char *)(NamePathOp->Common.Aml), "_T_", 3))
    {
        return (FALSE);
    }

    /* This is a Switch/Case control block */

    /* Ignore the One Op Predicate */

    OneOp->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;

    /* Ignore the Store Op, but not the children */

    StoreOp->Common.DisasmOpcode = ACPI_DASM_IGNORE_SINGLE;

    /*
     * First arg of Store Op is the Switch condition.
     * Mark it as a Switch predicate and as a parameter list for paren
     * closing and correct indentation.
     */
    PredicateOp = AcpiPsGetArg (StoreOp, 0);
    PredicateOp->Common.DisasmOpcode = ACPI_DASM_SWITCH_PREDICATE;
    PredicateOp->Common.DisasmFlags |= ACPI_PARSEOP_PARAMETER_LIST;

    /* Ignore the Name Op */

    NamePathOp->Common.DisasmFlags = ACPI_PARSEOP_IGNORE;

    /* Remaining opcodes are the Case statements (If/ElseIf's) */

    CurrentOp = StoreOp->Common.Next;
    while (AcpiDmIsCaseBlock (CurrentOp))
    {
        /* Block is a Case structure */

        if (CurrentOp->Common.AmlOpcode == AML_ELSE_OP)
        {
            /* ElseIf */

            CurrentOp->Common.DisasmOpcode = ACPI_DASM_CASE;
            CurrentOp = AcpiPsGetArg (CurrentOp, 0);
        }

        /* If */

        CurrentOp->Common.DisasmOpcode = ACPI_DASM_CASE;

        /*
         * Mark the parse tree for Case disassembly. There are two
         * types of Case statements. The first type of statement begins with
         * an LEqual. The second starts with an LNot and uses a Match statement
         * on a Package of constants.
         */
        TempOp = AcpiPsGetArg (CurrentOp, 0);
        switch (TempOp->Common.AmlOpcode)
        {
            case (AML_LEQUAL_OP):

                /* Ignore just the LEqual Op */

                TempOp->Common.DisasmOpcode = ACPI_DASM_IGNORE_SINGLE;

                /* Ignore the NamePath Op */

                TempOp = AcpiPsGetArg (TempOp, 0);
                TempOp->Common.DisasmFlags = ACPI_PARSEOP_IGNORE;

                /*
                 * Second arg of LEqual will be the Case predicate.
                 * Mark it as a predicate and also as a parameter list for paren
                 * closing and correct indentation.
                 */
                PredicateOp = TempOp->Common.Next;
                PredicateOp->Common.DisasmOpcode = ACPI_DASM_SWITCH_PREDICATE;
                PredicateOp->Common.DisasmFlags |= ACPI_PARSEOP_PARAMETER_LIST;

                break;

            case (AML_LNOT_OP):

                /*
                 * The Package will be the predicate of the Case statement.
                 * It's under:
                 *            LNOT
                 *                LEQUAL
                 *                    MATCH
                 *                        PACKAGE
                 */

                /* Get the LEqual Op from LNot */

                TempOp = AcpiPsGetArg (TempOp, 0);

                /* Get the Match Op from LEqual */

                TempOp = AcpiPsGetArg (TempOp, 0);

                /* Get the Package Op from Match */

                PredicateOp = AcpiPsGetArg (TempOp, 0);

                /* Mark as parameter list for paren closing */

                PredicateOp->Common.DisasmFlags |= ACPI_PARSEOP_PARAMETER_LIST;

                /*
                 * The Package list would be too deeply indented if we
                 * chose to simply ignore the all the parent opcodes, so
                 * we rearrange the parse tree instead.
                 */

                /*
                 * Save the second arg of the If/Else Op which is the
                 * block code of code for this Case statement.
                 */
                TempOp = AcpiPsGetArg (CurrentOp, 1);

                /*
                 * Move the Package Op to the child (predicate) of the
                 * Case statement.
                 */
                CurrentOp->Common.Value.Arg = PredicateOp;
                PredicateOp->Common.Parent = CurrentOp;

                /* Add the block code */

                PredicateOp->Common.Next = TempOp;

                break;

            default:

                /* Should never get here */

                break;
        }

        /* Advance to next Case block */

        CurrentOp = CurrentOp->Common.Next;
    }

    /* If CurrentOp is now an Else, then this is a Default block */

    if (CurrentOp && CurrentOp->Common.AmlOpcode == AML_ELSE_OP)
    {
        CurrentOp->Common.DisasmOpcode = ACPI_DASM_DEFAULT;
    }

    /*
     * From the first If advance to the Break op. It's possible to
     * have an Else (Default) op here when there is only one Case
     * statement, so check for it.
     */
    CurrentOp = StoreOp->Common.Next->Common.Next;
    if (CurrentOp->Common.AmlOpcode == AML_ELSE_OP)
    {
        CurrentOp = CurrentOp->Common.Next;
    }

    /* Ignore the Break Op */

    CurrentOp->Common.DisasmFlags |= ACPI_PARSEOP_IGNORE;

    return (TRUE);
}

/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIsCaseBlock
 *
 * PARAMETERS:  Op              - Object to test
 *
 * RETURN:      TRUE if Object is beginning of a Case block.
 *
 * DESCRIPTION: Determines if an Object is the beginning of a Case block for a
 *              Switch/Case statement. Parse tree must be one of the following
 *              forms:
 *
 *              Else (Optional)
 *                  If
 *                      LEqual
 *                          -NamePath- _T_x
 *
 *              Else (Optional)
 *                  If
 *                      LNot
 *                          LEqual
 *                              Match
 *                                  Package
 *                                      ByteConst
 *                                      -NamePath- _T_x
 *
 ******************************************************************************/

static BOOLEAN
AcpiDmIsCaseBlock (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *CurrentOp;

    if (!Op)
    {
        return (FALSE);
    }

    /* Look for an If or ElseIf */

    CurrentOp = Op;
    if (CurrentOp->Common.AmlOpcode == AML_ELSE_OP)
    {
        CurrentOp = AcpiPsGetArg (CurrentOp, 0);
        if (!CurrentOp)
        {
            return (FALSE);
        }
    }

    if (!CurrentOp || CurrentOp->Common.AmlOpcode != AML_IF_OP)
    {
        return (FALSE);
    }

    /* Child must be LEqual or LNot */

    CurrentOp = AcpiPsGetArg (CurrentOp, 0);
    if (!CurrentOp)
    {
        return (FALSE);
    }

    switch (CurrentOp->Common.AmlOpcode)
    {
        case (AML_LEQUAL_OP):

            /* Next child must be NamePath with string _T_ */

            CurrentOp = AcpiPsGetArg (CurrentOp, 0);
            if (!CurrentOp || !CurrentOp->Common.Value.Name ||
                strncmp(CurrentOp->Common.Value.Name, "_T_", 3))
            {
                return (FALSE);
            }

            break;

        case (AML_LNOT_OP):

            /* Child of LNot must be LEqual op */

            CurrentOp = AcpiPsGetArg (CurrentOp, 0);
            if (!CurrentOp || (CurrentOp->Common.AmlOpcode != AML_LEQUAL_OP))
            {
                return (FALSE);
            }

            /* Child of LNot must be Match op */

            CurrentOp = AcpiPsGetArg (CurrentOp, 0);
            if (!CurrentOp || (CurrentOp->Common.AmlOpcode != AML_MATCH_OP))
            {
                return (FALSE);
            }

            /* First child of Match must be Package op */

            CurrentOp = AcpiPsGetArg (CurrentOp, 0);
            if (!CurrentOp || (CurrentOp->Common.AmlOpcode != AML_PACKAGE_OP))
            {
                return (FALSE);
            }

            /* Third child of Match must be NamePath with string _T_ */

            CurrentOp = AcpiPsGetArg (CurrentOp->Common.Parent, 2);
            if (!CurrentOp || !CurrentOp->Common.Value.Name ||
                strncmp(CurrentOp->Common.Value.Name, "_T_", 3))
            {
                return (FALSE);
            }

            break;

        default:

            return (FALSE);
    }

    return (TRUE);
}
