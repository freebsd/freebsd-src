/******************************************************************************
 *
 * Module Name: ahgrammar - AML grammar items
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

#include "acpihelp.h"

const AH_AML_TYPE           Gbl_AmlTypesInfo[] =
{
    {"ComputationalData",
        "ComputationalData :=\n"
        "ByteConst | WordConst | DWordConst | QWordConst |\n"
        "String | ConstObj | RevisionOp | DefBuffer\n\n"
        "DataObject := ComputationalData | DefPackage | DefVarPackage\n"
        "DataRefObject := DataObject | ObjectReference | DDBHandle\n\n"

        "ByteConst := BytePrefix ByteData\n"
        "BytePrefix := 0x0A\n"
        "ByteList := Nothing | <ByteData ByteList>\n"
        "ByteData := 0x00 - 0xFF\n\n"

        "WordConst := WordPrefix WordData\n"
        "WordPrefix := 0x0B\n"
        "WordData := 0x0000-0xFFFF\n\n"

        "DWordConst := DWordPrefix DWordData\n"
        "DWordPrefix := 0x0C\n"
        "DWordData := 0x00000000-0xFFFFFFFF\n\n"

        "QWordConst := QWordPrefix QWordData\n"
        "QWordPrefix := 0x0E\n"
        "QWordData := 0x0000000000000000-0xFFFFFFFFFFFFFFFF\n\n"

        "String := StringPrefix AsciiCharList NullChar\n"
        "StringPrefix := 0x0D\n"
        "AsciiCharList := Nothing | <AsciiChar AsciiCharList>\n"
        "AsciiChar := 0x01 - 0x7F\n"
        "NullChar := 0x00\n\n"

        "ConstObj := ZeroOp | OneOp | OnesOp\n\n"},

    {"DefinitionBlock",
        "DefinitionBlockHeader :=\n"
        "TableSignature TableLength SpecCompliance Checksum\n"
        "OemID OemTableID OemRevision CreatorID CreatorRevision\n\n"

        "TableSignature := AsciiChar AsciiChar AsciiChar AsciiChar\n"
        "TableLength := DWordData\n"
        "// Length of the table in bytes including\n"
        "// the block header.\n\n"

        "SpecCompliance := ByteData\n"
        "// The revision of the structure\n\n"

        "CheckSum := ByteData\n"
        "// Byte checksum of the entire table\n\n"

        "OemID := ByteData(6)\n"
        "// OEM ID of up to 6 characters. If the OEM\n"
        "// ID is shorter than 6 characters, it\n"
        "// can be terminated with a NULL\n"
        "// character.\n\n"

        "OemTableID := ByteData(8)\n"
        "// OEM Table ID of up to 8 characters. If\n"
        "// the OEM Table ID is shorter than 8\n"
        "// characters, it can be terminated with\n"
        "// a NULL character.\n"
        "OemRevision := DWordData\n"
        "// OEM Table Revision\n\n"
        "CreatorID := DWordData\n"
        "// Vendor ID of the ASL compiler\n"
        "CreatorRevision := DWordData\n"
        "// Revision of the ASL compiler\n"},

    {"FieldFlags",
        "FieldFlags := ByteData\n"
        "// bits 0-3: AccessType\n"
        "//     0 AnyAcc\n"
        "//     1 ByteAcc\n"
        "//     2 WordAcc\n"
        "//     3 DWordAcc\n"
        "//     4 QWordAcc\n"
        "//     5 BufferAcc\n"
        "//     6 Reserved\n"
        "//     7 Reserved\n"
        "// bit 4: LockRule\n"
        "//     0 NoLock\n"
        "//     1 Lock\n"
        "// bits 5-6: UpdateRule\n"
        "//     0 Preserve\n"
        "//     1 WriteAsOnes\n"
        "//     2 WriteAsZeros\n"
        "// bit 7:\n"
        "//     0 Reserved (must be 0)\n"},

    {"FieldList",
        "FieldList := Nothing | <FieldElement FieldList>\n\n"
        "FieldElement := NamedField | ReservedField | AccessField |\n"
        "    ExtendedAccessField | ConnectField\n\n"
        "NamedField := NameSeg PkgLength\n"
        "ReservedField := 0x00 PkgLength\n\n"

        "AccessField := 0x01 AccessType\n"
        "AccessField := 0x01 AccessType AccessAttrib\n\n"

        "AccessType := ByteData\n"
        "// Bits 0:3 - Same as AccessType bits of FieldFlags.\n"
        "// Bits 4:5 - Reserved\n"
        "// Bits 7:6 - 0 = AccessAttribute\n"
        "//     Normal Access Attributes\n"
        "//     1 = AttribBytes (x)\n"
        "//     2 = AttribRawBytes (x)\n"
        "//     3 = AttribRawProcessBytes (x)\n"
        "//     Note: 'x' is encoded as bits 0:7 of the AccessAttrib byte.\n\n"

        "AccessAttrib := ByteData\n"
        "// bits 0:7: Byte length\n"
        "//\n"
        "// If AccessType is BufferAcc for the SMB or\n"
        "// GPIO OpRegions, AccessAttrib can be one of\n"
        "// the following values:\n"
        "// 0x02 AttribQuick\n"
        "// 0x04 AttribSendReceive\n"
        "// 0x06 AttribByte\n"
        "// 0x08 AttribWord\n"
        "// 0x0A AttribBlock\n"
        "// 0x0C AttribProcessCall\n"
        "// 0x0D AttribBlockProcessCall\n\n"

        "ExtendedAccessField := 0x03 AccessType ExtendedAccessAttrib AccessLength\n"
        "ExtendedAccessAttrib := ByteData\n"
        "// 0x0B AttribBytes\n"
        "// 0x0E AttribRawBytes\n"
        "// 0x0F AttribRawProcess\n\n"

        "ConnectField := 0x02 NameString> | <0x02 BufferData\n"},

    {"MatchOpcode",
        "DefMatch := MatchOp SearchPkg MatchOpcode Operand MatchOpcode Operand StartIndex\n"
        "MatchOp := 0x89\n"
        "SearchPkg := TermArg => Package\n"
        "MatchOpcode := ByteData\n"
        "// 0 MTR\n"
        "// 1 MEQ\n"
        "// 2 MLE\n"
        "// 3 MLT\n"
        "// 4 MGE\n"
        "// 5 MGT\n"},

   {"MethodFlags",
        "DefMethod := MethodOp PkgLength NameString MethodFlags TermList\n"
        "MethodOp := 0x14\n"
        "MethodFlags := ByteData\n"
        "// bit 0-2: ArgCount (0-7)\n"
        "// bit 3: SerializeFlag\n"
        "//   0 NotSerialized\n"
        "//   1 Serialized\n"
        "// bit 4-7: SyncLevel (0x00-0x0f)\n"},

    {"Miscellaneous",
        "ZeroOp := 0x00\n"
        "OneOp := 0x01\n"
        "OnesOp := 0xFF\n"
        "RevisionOp := ExtOpPrefix 0x30\n"
        "ExtOpPrefix := 0x5B\n"},

    {"NameSeg",
        "NameSeg := <LeadNameChar NameChar NameChar NameChar>\n"
        "// Note: NameSegs shorter than 4 characters are filled with\n"
        "// trailing underscores.\n\n"
        "NameChar := DigitChar | LeadNameChar\n"
        "LeadNameChar := 'A'-'Z' | '_' (0x41 - 0x5A) | (0x5F)\n"
        "DigitChar := '0'-'9' (0x30 - 0x39)\n"},

    {"NameString",
        "NameString := <RootChar NamePath> | <PrefixPath NamePath>\n"
        "PrefixPath := Nothing | <ParentPrefixChar PrefixPath>\n"
        "RootChar := '\\' (0x5C)\n"
        "ParentPrefixChar := '^' (0x5E)\n"},

    {"NamePath",
        "NamePath := NameSeg | DualNamePath | MultiNamePath | NullName\n"
        "DualNamePath := DualNamePrefix NameSeg NameSeg\n"
        "DualNamePrefix := 0x2E\n"
        "MultiNamePath := MultiNamePrefix SegCount NameSeg(SegCount)\n"
        "MultiNamePrefix := 0x2F\n"
        "SegCount := ByteData\n"
        "// Note: SegCount can be from 1 to 255. For example: MultiNamePrefix(35)\n"
        "// is encoded as 0x2f 0x23 and followed by 35 NameSegs. So, the total\n"
        "// encoding length will be 1 + 1 + (35 * 4) = 142. Notice that:\n"
        "// DualNamePrefix NameSeg NameSeg has a smaller encoding than the\n"
        "// encoding of: MultiNamePrefix(2) NameSeg NameSeg\n\n"

        "SimpleName := NameString | ArgObj | LocalObj\n"
        "SuperName := SimpleName | DebugObj | Type6Opcode\n"
        "NullName := 0x00\n"
        "Target := SuperName | NullName\n"},

    {"PkgLength",
        "PkgLength := PkgLeadByte |\n"
        "<PkgLeadByte ByteData> |\n"
        "<PkgLeadByte ByteData ByteData> |\n"
        "<PkgLeadByte ByteData ByteData ByteData>\n\n"

        "PkgLeadByte :=\n"
        "bit 7-6: Count of ByteData that follows (0-3)\n"
        "bit 5-4: Only used if (PkgLength < 63)\n"
        "bit 3-0: Least significant package length nybble\n"
        "// Note: The high 2 bits of the first byte reveal how many follow bytes\n"
        "// are in the PkgLength. If the PkgLength has only one byte, bit 0 through 5\n"
        "// are used to encode the package length (in other words, values 0-63). If\n"
        "// the package length value is more than 63, more than one byte must be\n"
        "// used for the encoding in which case bit 4 and 5 of the PkgLeadByte are\n"
        "// reserved and must be zero. If the multiple bytes encoding is used, bits\n"
        "// 0-3 of the PkgLeadByte become the least significant 4 bits of the\n"
        "// resulting package length value. The next ByteData will become the next\n"
        "// least significant 8 bits of the resulting value and so on, up to 3\n"
        "// ByteData bytes. Thus, the maximum package length is 2**28.\n"},

    {"RegionSpace",
        "RegionSpace := ByteData\n"
        "// 0x00 SystemMemory\n"
        "// 0x01 SystemIO\n"
        "// 0x02 PCI_Config\n"
        "// 0x03 EmbeddedControl\n"
        "// 0x04 SMBus\n"
        "// 0x05 SystemCMOS\n"
        "// 0x06 PciBarTarget\n"
        "// 0x07 IPMI\n"
        "// 0x08 GeneralPurposeIO\n"
        "// 0x09 GenericSerialBus\n"
        "// 0x0A Platform Communications Channel\n"
        "// 0x0B-0x7E: Reserved\n"
        "// 0x7F: Functional Fixed Hardware\n"
        "// 0x80-0xBF: Reserved\n"
        "// 0xC0-0xFF: OEM Defined\n"},

    {"TermObj",
        "TermObj := NameSpaceModifierObj | NamedObj | Type1Opcode | Type2Opcode\n"
        "TermList := Nothing | <TermObj TermList>\n\n"

        "MethodInvocation := NameString TermArgList\n"
        "TermArgList := Nothing | <TermArg TermArgList>\n"
        "TermArg := Type2Opcode | DataObject | ArgObj | LocalObj\n\n"

        "ObjectList := Nothing | <Object ObjectList>\n"
        "Object := NameSpaceModifierObj | NamedObj\n"},

    {NULL, NULL}
};
