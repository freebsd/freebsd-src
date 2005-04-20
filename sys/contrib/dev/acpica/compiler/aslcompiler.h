
/******************************************************************************
 *
 * Module Name: aslcompiler.h - common include file
 *              $Revision: 130 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2004, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
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
 * to or modifications of the Original Intel Code.  No other license or right
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
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
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
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
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
 *****************************************************************************/


#ifndef __ASLCOMPILER_H
#define __ASLCOMPILER_H


/* Microsoft-specific */

#if (defined WIN32 || defined WIN64)

/* warn : used #pragma pack */
#pragma warning(disable:4103)

/* warn : named type definition in parentheses */
#pragma warning(disable:4115)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>


#include "acpi.h"
#include "amlresrc.h"
#include "acdebug.h"
#include "asltypes.h"
#include "aslglobal.h"


/*
 * Compiler versions and names
 */

#define CompilerCreatorRevision     ACPI_CA_VERSION

#define IntelAcpiCA                 "Intel ACPI Component Architecture"
#define CompilerId                  "ASL Optimizing Compiler / AML Disassembler"
#define CompilerCopyright           "Copyright (C) 2000 - 2004 Intel Corporation"
#define CompilerCompliance          "ACPI 2.0c"
#define CompilerName                "iasl"
#define CompilerCreatorId           "INTL"


/* Configuration constants */

#define ASL_MAX_ERROR_COUNT         200
#define ASL_NODE_CACHE_SIZE         1024
#define ASL_STRING_CACHE_SIZE       32768

#define ASL_FIRST_PARSE_OPCODE      PARSEOP_ACCESSAS
#define ASL_YYTNAME_START           3

/*
 * Macros
 */

#define ASL_RESDESC_OFFSET(m)       ACPI_OFFSET (ASL_RESOURCE_DESC, m)
#define ASL_PTR_DIFF(a,b)           ((UINT8 *)(b) - (UINT8 *)(a))
#define ASL_PTR_ADD(a,b)            ((UINT8 *)(a) = ((UINT8 *)(a) + (b)))
#define ASL_GET_CHILD_NODE(a)       (a)->Asl.Child
#define ASL_GET_PEER_NODE(a)        (a)->Asl.Next
#define OP_TABLE_ENTRY(a,b,c,d)     {b,d,a,c}


#define ASL_PARSE_OPCODE_BASE       PARSEOP_ACCESSAS        /* First Lex type */


/* Internal AML opcodes */

#define AML_RAW_DATA_BYTE           (UINT16) 0xAA01 /* write one raw byte */
#define AML_RAW_DATA_WORD           (UINT16) 0xAA02 /* write 2 raw bytes */
#define AML_RAW_DATA_DWORD          (UINT16) 0xAA04 /* write 4 raw bytes */
#define AML_RAW_DATA_QWORD          (UINT16) 0xAA08 /* write 8 raw bytes */
#define AML_RAW_DATA_BUFFER         (UINT16) 0xAA0B /* raw buffer with length */
#define AML_RAW_DATA_CHAIN          (UINT16) 0xAA0C /* chain of raw buffers */
#define AML_PACKAGE_LENGTH          (UINT16) 0xAA10
#define AML_UNASSIGNED_OPCODE       (UINT16) 0xEEEE
#define AML_DEFAULT_ARG_OP          (UINT16) 0xDDDD


/* filename suffixes for output files */

#define FILE_SUFFIX_AML_CODE        "aml"
#define FILE_SUFFIX_LISTING         "lst"
#define FILE_SUFFIX_HEX_DUMP        "hex"
#define FILE_SUFFIX_DEBUG           "txt"
#define FILE_SUFFIX_SOURCE          "src"
#define FILE_SUFFIX_NAMESPACE       "nsp"
#define FILE_SUFFIX_ASM_SOURCE      "asm"
#define FILE_SUFFIX_C_SOURCE        "c"
#define FILE_SUFFIX_DISASSEMBLY     "dsl"
#define FILE_SUFFIX_ASM_INCLUDE     "inc"
#define FILE_SUFFIX_C_INCLUDE       "h"


/* Misc */

#define ASL_EXTERNAL_METHOD         255
#define ASL_ABORT                   TRUE
#define ASL_NO_ABORT                FALSE


/*******************************************************************************
 *
 * Compiler prototypes
 *
 ******************************************************************************/


void
end_stmt (void);


/* parser */

int
AslCompilerparse(
    void);

ACPI_PARSE_OBJECT *
AslDoError (
    void);

int
AslCompilererror(
    char                    *s);

int
AslCompilerlex(
    void);

void
ResetCurrentLineBuffer (
    void);

void
InsertLineBuffer (
    int                     SourceChar);

int
AslPopInputFileStack (
    void);

void
AslPushInputFileStack (
    FILE                    *InputFile,
    char                    *Filename);

/* aslmain */

void
AslCompilerSignon (
    UINT32                  FileId);

void
AslCompilerFileHeader (
    UINT32                  FileId);

void
AslDoSourceOutputFile (
    char                    *Buffer);

#define ASL_DEBUG_OUTPUT    0
#define ASL_PARSE_OUTPUT    1
#define ASL_TREE_OUTPUT     2


void
DbgPrint (
    UINT32                  Type,
    char                    *Format,
    ...);

void
ErrorContext (void);

/* aslcompile */

int
CmDoCompile (void);

void
CmDoOutputFiles (void);

void
CmCleanupAndExit (void);


/* aslerror */

void
AslError (
    UINT8                   Level,
    UINT8                   MessageId,
    ACPI_PARSE_OBJECT       *Op,
    char                    *ExtraMessage);

void
AslCoreSubsystemError (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_STATUS             Status,
    char                    *ExtraMessage,
    BOOLEAN                 Abort);

void
AslCommonError (
    UINT8                   Level,
    UINT8                   MessageId,
    UINT32                  CurrentLineNumber,
    UINT32                  LogicalLineNumber,
    UINT32                  LogicalByteOffset,
    UINT32                  Column,
    char                    *Filename,
    char                    *ExtraMessage);

void
AePrintException (
    UINT32                  FileId,
    ASL_ERROR_MSG           *Enode,
    char                    *Header);

void
AePrintErrorLog (
    UINT32                  FileId);

ACPI_STATUS
AeLocalGetRootPointer (
    UINT32                  Flags,
    ACPI_PHYSICAL_ADDRESS   *RsdpPhysicalAddress);


/* asllisting */

void
LsWriteListingHexBytes (
    UINT8                   *Buffer,
    UINT32                  Length,
    UINT32                  FileId);

void
LsWriteNodeToListing (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  FileId);

void
LsWriteNodeToAsmListing (
    ACPI_PARSE_OBJECT       *Op);

void
LsWriteNode (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  FileId);

void
LsFinishSourceListing (
    UINT32                  FileId);

void
LsFlushListingBuffer (
    UINT32                  FileId);

void
LsDoHexOutput (
    void);

void
LsDoHexOutputC (
    void);

void
LsDoHexOutputAsm (
    void);

void
LsPushNode (
    char                    *Filename);

ASL_LISTING_NODE *
LsPopNode (
    void);


/*
 * aslopcodes - generate AML opcodes
 */

ACPI_STATUS
OpcAmlOpcodeWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

ACPI_STATUS
OpcAmlConstantWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

void
OpcGenerateAmlOpcode (
    ACPI_PARSE_OBJECT       *Op);

UINT32
OpcSetOptimalIntegerSize (
    ACPI_PARSE_OBJECT       *Op);

void
OpcGetIntegerWidth (
    ACPI_PARSE_OBJECT       *Op);

/*
 * asloperands - generate AML operands for the AML opcodes
 */

void
OpnGenerateAmlOperands (
    ACPI_PARSE_OBJECT       *Op);

void
OpnDoField (
    ACPI_PARSE_OBJECT       *Op);

void
OpnDoBankField (
    ACPI_PARSE_OBJECT       *Op);

void
OpnDoBuffer (
    ACPI_PARSE_OBJECT       *Op);

void
OpnDoDefinitionBlock (
    ACPI_PARSE_OBJECT       *Op);

void
OpnDoFieldCommon (
    ACPI_PARSE_OBJECT       *FieldOp,
    ACPI_PARSE_OBJECT       *Op);

void
OpnDoIndexField (
    ACPI_PARSE_OBJECT       *Op);

void
OpnDoLoadTable (
    ACPI_PARSE_OBJECT       *Op);

void
OpnDoMethod (
    ACPI_PARSE_OBJECT       *Op);

void
OpnDoPackage (
    ACPI_PARSE_OBJECT       *Op);

void
OpnDoRegion (
    ACPI_PARSE_OBJECT       *Op);

/*
 * aslopt - optmization
 */

void
OptOptimizeNamePath (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Flags,
    ACPI_WALK_STATE         *WalkState,
    char                    *AmlNameString,
    ACPI_NAMESPACE_NODE     *TargetNode);


/*
 * aslresource - resource template generation
 */

void
RsDoResourceTemplate (
    ACPI_PARSE_OBJECT       *Op);


void
CgGenerateAmlOutput (void);

void
CgGenerateListing (
    UINT32                  FileId);

void
LsDoListings (void);

void
CgGenerateAmlLengths (
    ACPI_PARSE_OBJECT       *Op);

ACPI_STATUS
CgOpenOutputFile (
    char                    *InputFilename);


/* asllength */

ACPI_STATUS
LnPackageLengthWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

ACPI_STATUS
LnInitLengthsWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);


ACPI_STATUS
CgAmlWriteWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

void
CgGenerateOutput(
    void);

void
CgCloseTable (void);


void
CgWriteNode (
    ACPI_PARSE_OBJECT       *Op);

/*
 * aslmap
 */

ACPI_OBJECT_TYPE
AslMapNamedOpcodeToDataType (
    UINT16                  Opcode);

/*
 * asltransform - parse tree transformations
 */

ACPI_STATUS
TrAmlTransformWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);


void
TrTransformSubtree (
    ACPI_PARSE_OBJECT       *Op);

void
TrDoSwitch (
    ACPI_PARSE_OBJECT       *Op);

void
TrDoDefinitionBlock (
    ACPI_PARSE_OBJECT       *Op);

/*
 * asltree - parse tree support
 */

ACPI_STATUS
TrWalkParseTree (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Visitation,
    ASL_WALK_CALLBACK       DescendingCallback,
    ASL_WALK_CALLBACK       AscendingCallback,
    void                    *Context);

ACPI_PARSE_OBJECT *
TrAllocateNode (
    UINT32                  ParseOpcode);


/* Values for "Visitation" parameter above */

#define ASL_WALK_VISIT_DOWNWARD     0x01
#define ASL_WALK_VISIT_UPWARD       0x02
#define ASL_WALK_VISIT_TWICE        (ASL_WALK_VISIT_DOWNWARD | ASL_WALK_VISIT_UPWARD)


char *
TrAddNode (
    void                    *Thing);

ACPI_PARSE_OBJECT *
TrUpdateNode (
    UINT32                  ParseOpcode,
    ACPI_PARSE_OBJECT       *Op);

ACPI_PARSE_OBJECT *
TrCreateNode (
    UINT32                  ParseOpcode,
    UINT32                  NumChildren,
    ...);

ACPI_PARSE_OBJECT *
TrCreateLeafNode (
    UINT32                  ParseOpcode);

ACPI_PARSE_OBJECT *
TrCreateValuedLeafNode (
    UINT32                  ParseOpcode,
    ACPI_INTEGER            Value);

ACPI_PARSE_OBJECT *
TrLinkChildren (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  NumChildren,
    ...);

void
TrSetEndLineNumber (
    ACPI_PARSE_OBJECT       *Op);

void
TrWalkTree (void);

ACPI_PARSE_OBJECT *
TrLinkPeerNode (
    ACPI_PARSE_OBJECT       *Op1,
    ACPI_PARSE_OBJECT       *Op2);

ACPI_PARSE_OBJECT *
TrLinkChildNode (
    ACPI_PARSE_OBJECT       *Op1,
    ACPI_PARSE_OBJECT       *Op2);

ACPI_PARSE_OBJECT *
TrSetNodeFlags (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Flags);

ACPI_PARSE_OBJECT *
TrLinkPeerNodes (
    UINT32                  NumPeers,
    ...);

void
TrReleaseNode (
    ACPI_PARSE_OBJECT       *Op);

/* Analyze */

ACPI_STATUS
AnOtherSemanticAnalysisWalkBegin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

ACPI_STATUS
AnOtherSemanticAnalysisWalkEnd (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

ACPI_STATUS
AnOperandTypecheckWalkBegin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

ACPI_STATUS
AnOperandTypecheckWalkEnd (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

ACPI_STATUS
AnMethodAnalysisWalkBegin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

ACPI_STATUS
AnMethodAnalysisWalkEnd (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

ACPI_STATUS
AnMethodTypingWalkBegin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

ACPI_STATUS
AnMethodTypingWalkEnd (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);


/*
 * aslfiles - File I/O support
 */

void
AslAbort (void);

FILE *
FlOpenLocalFile (
    char                    *LocalName,
    char                    *Mode);

void
FlOpenIncludeFile (
    ACPI_PARSE_OBJECT       *Op);

void
FlFileError (
    UINT32                  FileId,
    UINT8                   ErrorId);

void
FlOpenFile (
    UINT32                  FileId,
    char                    *Filename,
    char                    *Mode);

ACPI_STATUS
FlReadFile (
    UINT32                  FileId,
    void                    *Buffer,
    UINT32                  Length);

void
FlWriteFile (
    UINT32                  FileId,
    void                    *Buffer,
    UINT32                  Length);

void
FlSeekFile (
    UINT32                  FileId,
    long                    Offset);

void
FlCloseFile (
    UINT32                  FileId);

void
FlPrintFile (
    UINT32                  FileId,
    char                    *Format,
    ...);

void
FlSetLineNumber (
    ACPI_PARSE_OBJECT       *Op);

ACPI_STATUS
FlParseInputPathname (
    char                    *InputFilename);

ACPI_STATUS
FlOpenInputFile (
    char                    *InputFilename);

ACPI_STATUS
FlOpenAmlOutputFile (
    char                    *InputFilename);

ACPI_STATUS
FlOpenMiscOutputFiles (
    char                    *InputFilename);

void
MpDisplayReservedNames (
    void);


/* Load */

ACPI_STATUS
LdLoadNamespace (
    ACPI_PARSE_OBJECT       *RootOp);


ACPI_STATUS
LdNamespace1Begin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

ACPI_STATUS
LdNamespace1End (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);


/* Lookup */

ACPI_STATUS
LkCrossReferenceNamespace (void);

ACPI_STATUS
LkNamespaceLocateBegin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

ACPI_STATUS
LkNamespaceLocateEnd (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

ACPI_STATUS
LsDisplayNamespace (
    void);

ACPI_STATUS
LsCompareOneNamespaceObject (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue);


/* Utils */

void
UtDisplayConstantOpcodes (
    void);

void
UtBeginEvent (
    UINT32                  Event,
    char                    *Name);

void
UtEndEvent (
    UINT32                  Event);

void *
UtLocalCalloc (
    UINT32                  Size);

void
UtPrintFormattedName (
    UINT16                  ParseOpcode,
    UINT32                  Level);

void
UtDisplaySummary (
    UINT32                  FileId);

UINT8
UtHexCharToValue (
    int                     hc);

void
UtConvertByteToHex (
    UINT8                   RawByte,
    UINT8                   *Buffer);

void
UtConvertByteToAsmHex (
    UINT8                   RawByte,
    UINT8                   *Buffer);

char *
UtGetOpName (
    UINT32                  ParseOpcode);

void
UtSetParseOpName (
    ACPI_PARSE_OBJECT       *Op);

ACPI_PARSE_OBJECT  *
UtGetArg (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Argn);

char *
UtGetStringBuffer (
    UINT32                  Length);

ACPI_STATUS
UtInternalizeName (
    char                    *ExternalName,
    char                    **ConvertedName);

void
UtAttachNamepathToOwner (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_PARSE_OBJECT       *NameNode);

ACPI_PARSE_OBJECT *
UtCheckIntegerRange (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  LowValue,
    UINT32                  HighValue);

ACPI_STATUS
UtStrtoul64 (
    char                    *String,
    UINT32                  Base,
    ACPI_INTEGER            *RetInteger);

ACPI_INTEGER
UtDoConstant (
    char                    *String);


/* Find */

void
LnAdjustLengthToRoot (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  LengthDelta);


#define NEXT_RESOURCE_DESC(a,b)     (ASL_RESOURCE_DESC *) (((char *) (a)) + sizeof(b))

#define DEFAULT_RESOURCE_DESC_SIZE  (sizeof (ASL_RESOURCE_DESC) + sizeof (ASL_END_TAG_DESC))


/*
 * Resource utilities
 */

ASL_RESOURCE_NODE *
RsAllocateResourceNode (
    UINT32                  Size);

    void
RsCreateBitField (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Name,
    UINT32                  ByteOffset,
    UINT32                  BitOffset);

void
RsCreateByteField (
    ACPI_PARSE_OBJECT       *Op,
    char                    *Name,
    UINT32                  ByteOffset);

void
RsSetFlagBits (
    UINT8                   *Flags,
    ACPI_PARSE_OBJECT       *Op,
    UINT8                   Position,
    UINT8                   DefaultBit);

ACPI_PARSE_OBJECT *
RsCompleteNodeAndGetNext (
    ACPI_PARSE_OBJECT       *Op);

ASL_RESOURCE_NODE *
RsDoOneResourceDescriptor (
    ACPI_PARSE_OBJECT       *DescriptorTypeOp,
    UINT32                  CurrentByteOffset,
    UINT8                   *State);

#define ACPI_RSTATE_NORMAL              0
#define ACPI_RSTATE_START_DEPENDENT     1
#define ACPI_RSTATE_DEPENDENT_LIST      2

UINT32
RsLinkDescriptorChain (
    ASL_RESOURCE_NODE       **PreviousRnode,
    ASL_RESOURCE_NODE       *Rnode);


/*
 * Small descriptors
 */

ASL_RESOURCE_NODE *
RsDoDmaDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoEndDependentDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoFixedIoDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoInterruptDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoIoDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoIrqDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoIrqNoFlagsDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoMemory24Descriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoMemory32Descriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoMemory32FixedDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoStartDependentDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoStartDependentNoPriDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoVendorSmallDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);


/*
 * Large descriptors
 */

UINT32
RsGetStringDataLength (
    ACPI_PARSE_OBJECT       *InitializerOp);

ASL_RESOURCE_NODE *
RsDoDwordIoDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoDwordMemoryDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoQwordIoDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoQwordMemoryDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoWordIoDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoWordBusNumberDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoVendorLargeDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);

ASL_RESOURCE_NODE *
RsDoGeneralRegisterDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset);


#endif /*  __ASLCOMPILER_H */

