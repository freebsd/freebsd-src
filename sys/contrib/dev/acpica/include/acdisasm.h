/******************************************************************************
 *
 * Name: acdisasm.h - AML disassembler
 *
 *****************************************************************************/

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

#ifndef __ACDISASM_H__
#define __ACDISASM_H__

#include <contrib/dev/acpica/include/amlresrc.h>


#define BLOCK_NONE              0
#define BLOCK_PAREN             1
#define BLOCK_BRACE             2
#define BLOCK_COMMA_LIST        4
#define ACPI_DEFAULT_RESNAME    *(UINT32 *) "__RD"

/*
 * Raw table data header. Used by disassembler and data table compiler.
 * Do not change.
 */
#define ACPI_RAW_TABLE_DATA_HEADER      "Raw Table Data"


typedef const struct acpi_dmtable_info
{
    UINT8                       Opcode;
    UINT8                       Offset;
    char                        *Name;
    UINT8                       Flags;

} ACPI_DMTABLE_INFO;

#define DT_LENGTH                       0x01    /* Field is a subtable length */
#define DT_FLAG                         0x02    /* Field is a flag value */
#define DT_NON_ZERO                     0x04    /* Field must be non-zero */

/* TBD: Not used at this time */

#define DT_OPTIONAL                     0x08
#define DT_COUNT                        0x10

/*
 * Values for Opcode above.
 * Note: 0-7 must not change, used as a flag shift value
 */
#define ACPI_DMT_FLAG0                  0
#define ACPI_DMT_FLAG1                  1
#define ACPI_DMT_FLAG2                  2
#define ACPI_DMT_FLAG3                  3
#define ACPI_DMT_FLAG4                  4
#define ACPI_DMT_FLAG5                  5
#define ACPI_DMT_FLAG6                  6
#define ACPI_DMT_FLAG7                  7
#define ACPI_DMT_FLAGS0                 8
#define ACPI_DMT_FLAGS2                 9
#define ACPI_DMT_UINT8                  10
#define ACPI_DMT_UINT16                 11
#define ACPI_DMT_UINT24                 12
#define ACPI_DMT_UINT32                 13
#define ACPI_DMT_UINT56                 14
#define ACPI_DMT_UINT64                 15
#define ACPI_DMT_STRING                 16
#define ACPI_DMT_NAME4                  17
#define ACPI_DMT_NAME6                  18
#define ACPI_DMT_NAME8                  19
#define ACPI_DMT_CHKSUM                 20
#define ACPI_DMT_SPACEID                21
#define ACPI_DMT_GAS                    22
#define ACPI_DMT_ASF                    23
#define ACPI_DMT_DMAR                   24
#define ACPI_DMT_HEST                   25
#define ACPI_DMT_HESTNTFY               26
#define ACPI_DMT_HESTNTYP               27
#define ACPI_DMT_MADT                   28
#define ACPI_DMT_SRAT                   29
#define ACPI_DMT_EXIT                   30
#define ACPI_DMT_SIG                    31
#define ACPI_DMT_FADTPM                 32
#define ACPI_DMT_BUF16                  33
#define ACPI_DMT_IVRS                   34
#define ACPI_DMT_BUFFER                 35
#define ACPI_DMT_PCI_PATH               36
#define ACPI_DMT_EINJACT                37
#define ACPI_DMT_EINJINST               38
#define ACPI_DMT_ERSTACT                39
#define ACPI_DMT_ERSTINST               40
#define ACPI_DMT_ACCWIDTH               41
#define ACPI_DMT_UNICODE                42
#define ACPI_DMT_UUID                   43
#define ACPI_DMT_DEVICE_PATH            44


typedef
void (*ACPI_DMTABLE_HANDLER) (
    ACPI_TABLE_HEADER       *Table);

typedef
ACPI_STATUS (*ACPI_CMTABLE_HANDLER) (
    void                    **PFieldList);

typedef struct acpi_dmtable_data
{
    char                    *Signature;
    ACPI_DMTABLE_INFO       *TableInfo;
    ACPI_DMTABLE_HANDLER    TableHandler;
    ACPI_CMTABLE_HANDLER    CmTableHandler;
    const unsigned char     *Template;
    char                    *Name;

} ACPI_DMTABLE_DATA;


typedef struct acpi_op_walk_info
{
    UINT32                  Level;
    UINT32                  LastLevel;
    UINT32                  Count;
    UINT32                  BitOffset;
    UINT32                  Flags;
    ACPI_WALK_STATE         *WalkState;

} ACPI_OP_WALK_INFO;

/*
 * TBD - another copy of this is in asltypes.h, fix
 */
#ifndef ASL_WALK_CALLBACK_DEFINED
typedef
ACPI_STATUS (*ASL_WALK_CALLBACK) (
    ACPI_PARSE_OBJECT           *Op,
    UINT32                      Level,
    void                        *Context);
#define ASL_WALK_CALLBACK_DEFINED
#endif


typedef struct acpi_resource_tag
{
    UINT32                  BitIndex;
    char                    *Tag;

} ACPI_RESOURCE_TAG;

/* Strings used for decoding flags to ASL keywords */

extern const char               *AcpiGbl_WordDecode[];
extern const char               *AcpiGbl_IrqDecode[];
extern const char               *AcpiGbl_LockRule[];
extern const char               *AcpiGbl_AccessTypes[];
extern const char               *AcpiGbl_UpdateRules[];
extern const char               *AcpiGbl_MatchOps[];

extern ACPI_DMTABLE_INFO        AcpiDmTableInfoAsf0[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoAsf1[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoAsf1a[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoAsf2[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoAsf2a[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoAsf3[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoAsf4[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoAsfHdr[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoBoot[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoBert[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoCpep[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoCpep0[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoDbgp[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoDmar[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoDmarHdr[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoDmarScope[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoDmar0[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoDmar1[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoDmar2[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoDmar3[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoEcdt[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoEinj[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoEinj0[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoErst[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoErst0[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoFacs[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoFadt1[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoFadt2[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoFadt3[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoGas[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoHeader[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoHest[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoHest0[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoHest1[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoHest2[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoHest6[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoHest7[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoHest8[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoHest9[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoHestNotify[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoHestBank[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoHpet[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoIvrs[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoIvrs0[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoIvrs1[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoIvrs4[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoIvrs8a[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoIvrs8b[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoIvrs8c[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoIvrsHdr[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoMadt[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoMadt0[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoMadt1[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoMadt2[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoMadt3[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoMadt4[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoMadt5[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoMadt6[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoMadt7[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoMadt8[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoMadt9[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoMadt10[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoMadtHdr[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoMcfg[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoMcfg0[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoMchi[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoMsct[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoMsct0[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoRsdp1[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoRsdp2[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoSbst[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoSlic[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoSlit[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoSpcr[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoSpmi[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoSrat[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoSratHdr[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoSrat0[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoSrat1[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoSrat2[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoTcpa[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoUefi[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoWaet[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoWdat[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoWdat0[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoWddt[];
extern ACPI_DMTABLE_INFO        AcpiDmTableInfoWdrt[];

extern ACPI_DMTABLE_INFO        AcpiDmTableInfoGeneric[][2];


/*
 * dmtable
 */
extern ACPI_DMTABLE_DATA        AcpiDmTableData[];

UINT8
AcpiDmGenerateChecksum (
    void                    *Table,
    UINT32                  Length,
    UINT8                   OriginalChecksum);

ACPI_DMTABLE_DATA *
AcpiDmGetTableData (
    char                    *Signature);

void
AcpiDmDumpDataTable (
    ACPI_TABLE_HEADER       *Table);

ACPI_STATUS
AcpiDmDumpTable (
    UINT32                  TableLength,
    UINT32                  TableOffset,
    void                    *Table,
    UINT32                  SubTableLength,
    ACPI_DMTABLE_INFO        *Info);

void
AcpiDmLineHeader (
    UINT32                  Offset,
    UINT32                  ByteLength,
    char                    *Name);

void
AcpiDmLineHeader2 (
    UINT32                  Offset,
    UINT32                  ByteLength,
    char                    *Name,
    UINT32                  Value);


/*
 * dmtbdump
 */
void
AcpiDmDumpAsf (
    ACPI_TABLE_HEADER       *Table);

void
AcpiDmDumpCpep (
    ACPI_TABLE_HEADER       *Table);

void
AcpiDmDumpDmar (
    ACPI_TABLE_HEADER       *Table);

void
AcpiDmDumpEinj (
    ACPI_TABLE_HEADER       *Table);

void
AcpiDmDumpErst (
    ACPI_TABLE_HEADER       *Table);

void
AcpiDmDumpFadt (
    ACPI_TABLE_HEADER       *Table);

void
AcpiDmDumpHest (
    ACPI_TABLE_HEADER       *Table);

void
AcpiDmDumpIvrs (
    ACPI_TABLE_HEADER       *Table);

void
AcpiDmDumpMcfg (
    ACPI_TABLE_HEADER       *Table);

void
AcpiDmDumpMadt (
    ACPI_TABLE_HEADER       *Table);

void
AcpiDmDumpMsct (
    ACPI_TABLE_HEADER       *Table);

UINT32
AcpiDmDumpRsdp (
    ACPI_TABLE_HEADER       *Table);

void
AcpiDmDumpRsdt (
    ACPI_TABLE_HEADER       *Table);

void
AcpiDmDumpSlit (
    ACPI_TABLE_HEADER       *Table);

void
AcpiDmDumpSrat (
    ACPI_TABLE_HEADER       *Table);

void
AcpiDmDumpWdat (
    ACPI_TABLE_HEADER       *Table);

void
AcpiDmDumpXsdt (
    ACPI_TABLE_HEADER       *Table);


/*
 * dmwalk
 */
void
AcpiDmDisassemble (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Origin,
    UINT32                  NumOpcodes);

void
AcpiDmWalkParseTree (
    ACPI_PARSE_OBJECT       *Op,
    ASL_WALK_CALLBACK       DescendingCallback,
    ASL_WALK_CALLBACK       AscendingCallback,
    void                    *Context);


/*
 * dmopcode
 */
void
AcpiDmDisassembleOneOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_OP_WALK_INFO       *Info,
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmDecodeInternalObject (
    ACPI_OPERAND_OBJECT     *ObjDesc);

UINT32
AcpiDmListType (
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmMethodFlags (
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmFieldFlags (
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmAddressSpace (
    UINT8                   SpaceId);

void
AcpiDmRegionFlags (
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmMatchOp (
    ACPI_PARSE_OBJECT       *Op);


/*
 * dmnames
 */
UINT32
AcpiDmDumpName (
    UINT32                  Name);

ACPI_STATUS
AcpiPsDisplayObjectPathname (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmNamestring (
    char                    *Name);


/*
 * dmobject
 */
void
AcpiDmDisplayInternalObject (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState);

void
AcpiDmDisplayArguments (
    ACPI_WALK_STATE         *WalkState);

void
AcpiDmDisplayLocals (
    ACPI_WALK_STATE         *WalkState);

void
AcpiDmDumpMethodInfo (
    ACPI_STATUS             Status,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);


/*
 * dmbuffer
 */
void
AcpiDmDisasmByteList (
    UINT32                  Level,
    UINT8                   *ByteData,
    UINT32                  ByteCount);

void
AcpiDmByteList (
    ACPI_OP_WALK_INFO       *Info,
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmIsEisaId (
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmEisaId (
    UINT32                  EncodedId);

BOOLEAN
AcpiDmIsUnicodeBuffer (
    ACPI_PARSE_OBJECT       *Op);

BOOLEAN
AcpiDmIsStringBuffer (
    ACPI_PARSE_OBJECT       *Op);


/*
 * dmextern
 */

ACPI_STATUS
AcpiDmAddToExternalFileList (
    char                    *PathList);

void
AcpiDmClearExternalFileList (
    void);

void
AcpiDmAddToExternalList (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Path,
    UINT8                   Type,
    UINT32                  Value);

void
AcpiDmAddExternalsToNamespace (
    void);

UINT32
AcpiDmGetExternalMethodCount (
    void);

void
AcpiDmClearExternalList (
    void);

void
AcpiDmEmitExternals (
    void);


/*
 * dmresrc
 */
void
AcpiDmDumpInteger8 (
    UINT8                   Value,
    char                    *Name);

void
AcpiDmDumpInteger16 (
    UINT16                  Value,
    char                    *Name);

void
AcpiDmDumpInteger32 (
    UINT32                  Value,
    char                    *Name);

void
AcpiDmDumpInteger64 (
    UINT64                  Value,
    char                    *Name);

void
AcpiDmResourceTemplate (
    ACPI_OP_WALK_INFO       *Info,
    ACPI_PARSE_OBJECT       *Op,
    UINT8                   *ByteData,
    UINT32                  ByteCount);

ACPI_STATUS
AcpiDmIsResourceTemplate (
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmBitList (
    UINT16                  Mask);

void
AcpiDmDescriptorName (
    void);


/*
 * dmresrcl
 */
void
AcpiDmWordDescriptor (
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmDwordDescriptor (
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmExtendedDescriptor (
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmQwordDescriptor (
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmMemory24Descriptor (
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmMemory32Descriptor (
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmFixedMemory32Descriptor (
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmGenericRegisterDescriptor (
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmInterruptDescriptor (
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmVendorLargeDescriptor (
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmVendorCommon (
    char                    *Name,
    UINT8                   *ByteData,
    UINT32                  Length,
    UINT32                  Level);


/*
 * dmresrcs
 */
void
AcpiDmIrqDescriptor (
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmDmaDescriptor (
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmIoDescriptor (
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmFixedIoDescriptor (
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmStartDependentDescriptor (
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmEndDependentDescriptor (
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level);

void
AcpiDmVendorSmallDescriptor (
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level);


/*
 * dmutils
 */
void
AcpiDmDecodeAttribute (
    UINT8                   Attribute);

void
AcpiDmIndent (
    UINT32                  Level);

BOOLEAN
AcpiDmCommaIfListMember (
    ACPI_PARSE_OBJECT       *Op);

void
AcpiDmCommaIfFieldMember (
    ACPI_PARSE_OBJECT       *Op);


/*
 * dmrestag
 */
void
AcpiDmFindResources (
    ACPI_PARSE_OBJECT       *Root);

void
AcpiDmCheckResourceReference (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState);


/*
 * acdisasm
 */
void
AdDisassemblerHeader (
    char                    *Filename);


#endif  /* __ACDISASM_H__ */
