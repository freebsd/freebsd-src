/******************************************************************************
 *
 * Name: acutils.h -- prototypes for the common (subsystem-wide) procedures
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

#ifndef _ACUTILS_H
#define _ACUTILS_H


extern const UINT8                      AcpiGbl_ResourceAmlSizes[];
extern const UINT8                      AcpiGbl_ResourceAmlSerialBusSizes[];

/* Strings used by the disassembler and debugger resource dump routines */

#if defined(ACPI_DISASSEMBLER) || defined (ACPI_DEBUGGER)

extern const char                       *AcpiGbl_BmDecode[];
extern const char                       *AcpiGbl_ConfigDecode[];
extern const char                       *AcpiGbl_ConsumeDecode[];
extern const char                       *AcpiGbl_DecDecode[];
extern const char                       *AcpiGbl_HeDecode[];
extern const char                       *AcpiGbl_IoDecode[];
extern const char                       *AcpiGbl_LlDecode[];
extern const char                       *AcpiGbl_MaxDecode[];
extern const char                       *AcpiGbl_MemDecode[];
extern const char                       *AcpiGbl_MinDecode[];
extern const char                       *AcpiGbl_MtpDecode[];
extern const char                       *AcpiGbl_RngDecode[];
extern const char                       *AcpiGbl_RwDecode[];
extern const char                       *AcpiGbl_ShrDecode[];
extern const char                       *AcpiGbl_SizDecode[];
extern const char                       *AcpiGbl_TrsDecode[];
extern const char                       *AcpiGbl_TtpDecode[];
extern const char                       *AcpiGbl_TypDecode[];
extern const char                       *AcpiGbl_PpcDecode[];
extern const char                       *AcpiGbl_IorDecode[];
extern const char                       *AcpiGbl_DtsDecode[];
extern const char                       *AcpiGbl_CtDecode[];
extern const char                       *AcpiGbl_SbtDecode[];
extern const char                       *AcpiGbl_AmDecode[];
extern const char                       *AcpiGbl_SmDecode[];
extern const char                       *AcpiGbl_WmDecode[];
extern const char                       *AcpiGbl_CphDecode[];
extern const char                       *AcpiGbl_CpoDecode[];
extern const char                       *AcpiGbl_DpDecode[];
extern const char                       *AcpiGbl_EdDecode[];
extern const char                       *AcpiGbl_BpbDecode[];
extern const char                       *AcpiGbl_SbDecode[];
extern const char                       *AcpiGbl_FcDecode[];
extern const char                       *AcpiGbl_PtDecode[];
#endif

/* Types for Resource descriptor entries */

#define ACPI_INVALID_RESOURCE           0
#define ACPI_FIXED_LENGTH               1
#define ACPI_VARIABLE_LENGTH            2
#define ACPI_SMALL_VARIABLE_LENGTH      3

typedef
ACPI_STATUS (*ACPI_WALK_AML_CALLBACK) (
    UINT8                   *Aml,
    UINT32                  Length,
    UINT32                  Offset,
    UINT8                   ResourceIndex,
    void                    **Context);

typedef
ACPI_STATUS (*ACPI_PKG_CALLBACK) (
    UINT8                   ObjectType,
    ACPI_OPERAND_OBJECT     *SourceObject,
    ACPI_GENERIC_STATE      *State,
    void                    *Context);

typedef struct acpi_pkg_info
{
    UINT8                   *FreeSpace;
    ACPI_SIZE               Length;
    UINT32                  ObjectSpace;
    UINT32                  NumPackages;

} ACPI_PKG_INFO;

/* Object reference counts */

#define REF_INCREMENT       (UINT16) 0
#define REF_DECREMENT       (UINT16) 1

/* AcpiUtDumpBuffer */

#define DB_BYTE_DISPLAY     1
#define DB_WORD_DISPLAY     2
#define DB_DWORD_DISPLAY    4
#define DB_QWORD_DISPLAY    8

/*
 * utglobal - Global data structures and procedures
 */
ACPI_STATUS
AcpiUtInitGlobals (
    void);

#if defined(ACPI_DEBUG_OUTPUT) || defined(ACPI_DEBUGGER)

char *
AcpiUtGetMutexName (
    UINT32                  MutexId);

const char *
AcpiUtGetNotifyName (
    UINT32                  NotifyValue);

#endif

char *
AcpiUtGetTypeName (
    ACPI_OBJECT_TYPE        Type);

char *
AcpiUtGetNodeName (
    void                    *Object);

char *
AcpiUtGetDescriptorName (
    void                    *Object);

const char *
AcpiUtGetReferenceName (
    ACPI_OPERAND_OBJECT     *Object);

char *
AcpiUtGetObjectTypeName (
    ACPI_OPERAND_OBJECT     *ObjDesc);

char *
AcpiUtGetRegionName (
    UINT8                   SpaceId);

char *
AcpiUtGetEventName (
    UINT32                  EventId);

char
AcpiUtHexToAsciiChar (
    UINT64                  Integer,
    UINT32                  Position);

BOOLEAN
AcpiUtValidObjectType (
    ACPI_OBJECT_TYPE        Type);


/*
 * utinit - miscellaneous initialization and shutdown
 */
ACPI_STATUS
AcpiUtHardwareInitialize (
    void);

void
AcpiUtSubsystemShutdown (
    void);


/*
 * utclib - Local implementations of C library functions
 */
#ifndef ACPI_USE_SYSTEM_CLIBRARY

ACPI_SIZE
AcpiUtStrlen (
    const char              *String);

char *
AcpiUtStrcpy (
    char                    *DstString,
    const char              *SrcString);

char *
AcpiUtStrncpy (
    char                    *DstString,
    const char              *SrcString,
    ACPI_SIZE               Count);

int
AcpiUtMemcmp (
    const char              *Buffer1,
    const char              *Buffer2,
    ACPI_SIZE               Count);

int
AcpiUtStrncmp (
    const char              *String1,
    const char              *String2,
    ACPI_SIZE               Count);

int
AcpiUtStrcmp (
    const char              *String1,
    const char              *String2);

char *
AcpiUtStrcat (
    char                    *DstString,
    const char              *SrcString);

char *
AcpiUtStrncat (
    char                    *DstString,
    const char              *SrcString,
    ACPI_SIZE               Count);

UINT32
AcpiUtStrtoul (
    const char              *String,
    char                    **Terminator,
    UINT32                  Base);

char *
AcpiUtStrstr (
    char                    *String1,
    char                    *String2);

void *
AcpiUtMemcpy (
    void                    *Dest,
    const void              *Src,
    ACPI_SIZE               Count);

void *
AcpiUtMemset (
    void                    *Dest,
    UINT8                   Value,
    ACPI_SIZE               Count);

int
AcpiUtToUpper (
    int                     c);

int
AcpiUtToLower (
    int                     c);

extern const UINT8 _acpi_ctype[];

#define _ACPI_XA     0x00    /* extra alphabetic - not supported */
#define _ACPI_XS     0x40    /* extra space */
#define _ACPI_BB     0x00    /* BEL, BS, etc. - not supported */
#define _ACPI_CN     0x20    /* CR, FF, HT, NL, VT */
#define _ACPI_DI     0x04    /* '0'-'9' */
#define _ACPI_LO     0x02    /* 'a'-'z' */
#define _ACPI_PU     0x10    /* punctuation */
#define _ACPI_SP     0x08    /* space */
#define _ACPI_UP     0x01    /* 'A'-'Z' */
#define _ACPI_XD     0x80    /* '0'-'9', 'A'-'F', 'a'-'f' */

#define ACPI_IS_DIGIT(c)  (_acpi_ctype[(unsigned char)(c)] & (_ACPI_DI))
#define ACPI_IS_SPACE(c)  (_acpi_ctype[(unsigned char)(c)] & (_ACPI_SP))
#define ACPI_IS_XDIGIT(c) (_acpi_ctype[(unsigned char)(c)] & (_ACPI_XD))
#define ACPI_IS_UPPER(c)  (_acpi_ctype[(unsigned char)(c)] & (_ACPI_UP))
#define ACPI_IS_LOWER(c)  (_acpi_ctype[(unsigned char)(c)] & (_ACPI_LO))
#define ACPI_IS_PRINT(c)  (_acpi_ctype[(unsigned char)(c)] & (_ACPI_LO | _ACPI_UP | _ACPI_DI | _ACPI_SP | _ACPI_PU))
#define ACPI_IS_ALPHA(c)  (_acpi_ctype[(unsigned char)(c)] & (_ACPI_LO | _ACPI_UP))

#endif /* !ACPI_USE_SYSTEM_CLIBRARY */

#define ACPI_IS_ASCII(c)  ((c) < 0x80)


/*
 * utcopy - Object construction and conversion interfaces
 */
ACPI_STATUS
AcpiUtBuildSimpleObject(
    ACPI_OPERAND_OBJECT     *Obj,
    ACPI_OBJECT             *UserObj,
    UINT8                   *DataSpace,
    UINT32                  *BufferSpaceUsed);

ACPI_STATUS
AcpiUtBuildPackageObject (
    ACPI_OPERAND_OBJECT     *Obj,
    UINT8                   *Buffer,
    UINT32                  *SpaceUsed);

ACPI_STATUS
AcpiUtCopyIobjectToEobject (
    ACPI_OPERAND_OBJECT     *Obj,
    ACPI_BUFFER             *RetBuffer);

ACPI_STATUS
AcpiUtCopyEobjectToIobject (
    ACPI_OBJECT             *Obj,
    ACPI_OPERAND_OBJECT     **InternalObj);

ACPI_STATUS
AcpiUtCopyISimpleToIsimple (
    ACPI_OPERAND_OBJECT     *SourceObj,
    ACPI_OPERAND_OBJECT     *DestObj);

ACPI_STATUS
AcpiUtCopyIobjectToIobject (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     **DestDesc,
    ACPI_WALK_STATE         *WalkState);


/*
 * utcreate - Object creation
 */
ACPI_STATUS
AcpiUtUpdateObjectReference (
    ACPI_OPERAND_OBJECT     *Object,
    UINT16                  Action);


/*
 * utdebug - Debug interfaces
 */
void
AcpiUtInitStackPtrTrace (
    void);

void
AcpiUtTrackStackPtr (
    void);

void
AcpiUtTrace (
    UINT32                  LineNumber,
    const char              *FunctionName,
    const char              *ModuleName,
    UINT32                  ComponentId);

void
AcpiUtTracePtr (
    UINT32                  LineNumber,
    const char              *FunctionName,
    const char              *ModuleName,
    UINT32                  ComponentId,
    void                    *Pointer);

void
AcpiUtTraceU32 (
    UINT32                  LineNumber,
    const char              *FunctionName,
    const char              *ModuleName,
    UINT32                  ComponentId,
    UINT32                  Integer);

void
AcpiUtTraceStr (
    UINT32                  LineNumber,
    const char              *FunctionName,
    const char              *ModuleName,
    UINT32                  ComponentId,
    char                    *String);

void
AcpiUtExit (
    UINT32                  LineNumber,
    const char              *FunctionName,
    const char              *ModuleName,
    UINT32                  ComponentId);

void
AcpiUtStatusExit (
    UINT32                  LineNumber,
    const char              *FunctionName,
    const char              *ModuleName,
    UINT32                  ComponentId,
    ACPI_STATUS             Status);

void
AcpiUtValueExit (
    UINT32                  LineNumber,
    const char              *FunctionName,
    const char              *ModuleName,
    UINT32                  ComponentId,
    UINT64                  Value);

void
AcpiUtPtrExit (
    UINT32                  LineNumber,
    const char              *FunctionName,
    const char              *ModuleName,
    UINT32                  ComponentId,
    UINT8                   *Ptr);

void
AcpiUtDebugDumpBuffer (
    UINT8                   *Buffer,
    UINT32                  Count,
    UINT32                  Display,
    UINT32                  ComponentId);

void
AcpiUtDumpBuffer (
    UINT8                   *Buffer,
    UINT32                  Count,
    UINT32                  Display,
    UINT32                  Offset);

void
AcpiUtReportError (
    char                    *ModuleName,
    UINT32                  LineNumber);

void
AcpiUtReportInfo (
    char                    *ModuleName,
    UINT32                  LineNumber);

void
AcpiUtReportWarning (
    char                    *ModuleName,
    UINT32                  LineNumber);

/*
 * utdelete - Object deletion and reference counts
 */
void
AcpiUtAddReference (
    ACPI_OPERAND_OBJECT     *Object);

void
AcpiUtRemoveReference (
    ACPI_OPERAND_OBJECT     *Object);

void
AcpiUtDeleteInternalPackageObject (
    ACPI_OPERAND_OBJECT     *Object);

void
AcpiUtDeleteInternalSimpleObject (
    ACPI_OPERAND_OBJECT     *Object);

void
AcpiUtDeleteInternalObjectList (
    ACPI_OPERAND_OBJECT     **ObjList);


/*
 * uteval - object evaluation
 */
ACPI_STATUS
AcpiUtEvaluateObject (
    ACPI_NAMESPACE_NODE     *PrefixNode,
    char                    *Path,
    UINT32                  ExpectedReturnBtypes,
    ACPI_OPERAND_OBJECT     **ReturnDesc);

ACPI_STATUS
AcpiUtEvaluateNumericObject (
    char                    *ObjectName,
    ACPI_NAMESPACE_NODE     *DeviceNode,
    UINT64                  *Value);

ACPI_STATUS
AcpiUtExecute_STA (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    UINT32                  *StatusFlags);

ACPI_STATUS
AcpiUtExecutePowerMethods (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    const char              **MethodNames,
    UINT8                   MethodCount,
    UINT8                   *OutValues);


/*
 * utids - device ID support
 */
ACPI_STATUS
AcpiUtExecute_HID (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    ACPI_PNP_DEVICE_ID      **ReturnId);

ACPI_STATUS
AcpiUtExecute_UID (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    ACPI_PNP_DEVICE_ID      **ReturnId);

ACPI_STATUS
AcpiUtExecute_SUB (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    ACPI_PNP_DEVICE_ID      **ReturnId);

ACPI_STATUS
AcpiUtExecute_CID (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    ACPI_PNP_DEVICE_ID_LIST **ReturnCidList);


/*
 * utlock - reader/writer locks
 */
ACPI_STATUS
AcpiUtCreateRwLock (
    ACPI_RW_LOCK            *Lock);

void
AcpiUtDeleteRwLock (
    ACPI_RW_LOCK            *Lock);

ACPI_STATUS
AcpiUtAcquireReadLock (
    ACPI_RW_LOCK            *Lock);

ACPI_STATUS
AcpiUtReleaseReadLock (
    ACPI_RW_LOCK            *Lock);

ACPI_STATUS
AcpiUtAcquireWriteLock (
    ACPI_RW_LOCK            *Lock);

void
AcpiUtReleaseWriteLock (
    ACPI_RW_LOCK            *Lock);


/*
 * utobject - internal object create/delete/cache routines
 */
ACPI_OPERAND_OBJECT  *
AcpiUtCreateInternalObjectDbg (
    const char              *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId,
    ACPI_OBJECT_TYPE        Type);

void *
AcpiUtAllocateObjectDescDbg (
    const char              *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId);

#define AcpiUtCreateInternalObject(t)   AcpiUtCreateInternalObjectDbg (_AcpiModuleName,__LINE__,_COMPONENT,t)
#define AcpiUtAllocateObjectDesc()      AcpiUtAllocateObjectDescDbg (_AcpiModuleName,__LINE__,_COMPONENT)

void
AcpiUtDeleteObjectDesc (
    ACPI_OPERAND_OBJECT     *Object);

BOOLEAN
AcpiUtValidInternalObject (
    void                    *Object);

ACPI_OPERAND_OBJECT *
AcpiUtCreatePackageObject (
    UINT32                  Count);

ACPI_OPERAND_OBJECT *
AcpiUtCreateIntegerObject (
    UINT64                  Value);

ACPI_OPERAND_OBJECT *
AcpiUtCreateBufferObject (
    ACPI_SIZE               BufferSize);

ACPI_OPERAND_OBJECT *
AcpiUtCreateStringObject (
    ACPI_SIZE               StringSize);

ACPI_STATUS
AcpiUtGetObjectSize(
    ACPI_OPERAND_OBJECT     *Obj,
    ACPI_SIZE               *ObjLength);


/*
 * utosi - Support for the _OSI predefined control method
 */
ACPI_STATUS
AcpiUtInitializeInterfaces (
    void);

ACPI_STATUS
AcpiUtInterfaceTerminate (
    void);

ACPI_STATUS
AcpiUtInstallInterface (
    ACPI_STRING             InterfaceName);

ACPI_STATUS
AcpiUtRemoveInterface (
    ACPI_STRING             InterfaceName);

ACPI_INTERFACE_INFO *
AcpiUtGetInterface (
    ACPI_STRING             InterfaceName);

ACPI_STATUS
AcpiUtOsiImplementation (
    ACPI_WALK_STATE         *WalkState);


/*
 * utpredef - support for predefined names
 */
const ACPI_PREDEFINED_INFO *
AcpiUtGetNextPredefinedMethod (
    const ACPI_PREDEFINED_INFO  *ThisName);

const ACPI_PREDEFINED_INFO *
AcpiUtMatchPredefinedMethod (
    char                        *Name);

const ACPI_PREDEFINED_INFO *
AcpiUtMatchResourceName (
    char                        *Name);

void
AcpiUtDisplayPredefinedMethod (
    char                        *Buffer,
    const ACPI_PREDEFINED_INFO  *ThisName,
    BOOLEAN                     MultiLine);

void
AcpiUtGetExpectedReturnTypes (
    char                    *Buffer,
    UINT32                  ExpectedBtypes);

UINT32
AcpiUtGetResourceBitWidth (
    char                    *Buffer,
    UINT16                  Types);


/*
 * utstate - Generic state creation/cache routines
 */
void
AcpiUtPushGenericState (
    ACPI_GENERIC_STATE      **ListHead,
    ACPI_GENERIC_STATE      *State);

ACPI_GENERIC_STATE *
AcpiUtPopGenericState (
    ACPI_GENERIC_STATE      **ListHead);


ACPI_GENERIC_STATE *
AcpiUtCreateGenericState (
    void);

ACPI_THREAD_STATE *
AcpiUtCreateThreadState (
    void);

ACPI_GENERIC_STATE *
AcpiUtCreateUpdateState (
    ACPI_OPERAND_OBJECT     *Object,
    UINT16                  Action);

ACPI_GENERIC_STATE *
AcpiUtCreatePkgState (
    void                    *InternalObject,
    void                    *ExternalObject,
    UINT16                  Index);

ACPI_STATUS
AcpiUtCreateUpdateStateAndPush (
    ACPI_OPERAND_OBJECT     *Object,
    UINT16                  Action,
    ACPI_GENERIC_STATE      **StateList);

ACPI_STATUS
AcpiUtCreatePkgStateAndPush (
    void                    *InternalObject,
    void                    *ExternalObject,
    UINT16                  Index,
    ACPI_GENERIC_STATE      **StateList);

ACPI_GENERIC_STATE *
AcpiUtCreateControlState (
    void);

void
AcpiUtDeleteGenericState (
    ACPI_GENERIC_STATE      *State);


/*
 * utmath
 */
ACPI_STATUS
AcpiUtDivide (
    UINT64                  InDividend,
    UINT64                  InDivisor,
    UINT64                  *OutQuotient,
    UINT64                  *OutRemainder);

ACPI_STATUS
AcpiUtShortDivide (
    UINT64                  InDividend,
    UINT32                  Divisor,
    UINT64                  *OutQuotient,
    UINT32                  *OutRemainder);


/*
 * utmisc
 */
const ACPI_EXCEPTION_INFO *
AcpiUtValidateException (
    ACPI_STATUS             Status);

BOOLEAN
AcpiUtIsPciRootBridge (
    char                    *Id);

BOOLEAN
AcpiUtIsAmlTable (
    ACPI_TABLE_HEADER       *Table);

ACPI_STATUS
AcpiUtWalkPackageTree (
    ACPI_OPERAND_OBJECT     *SourceObject,
    void                    *TargetObject,
    ACPI_PKG_CALLBACK       WalkCallback,
    void                    *Context);


/* Values for Base above (16=Hex, 10=Decimal) */

#define ACPI_ANY_BASE        0

UINT32
AcpiUtDwordByteSwap (
    UINT32                  Value);

void
AcpiUtSetIntegerWidth (
    UINT8                   Revision);

#ifdef ACPI_DEBUG_OUTPUT
void
AcpiUtDisplayInitPathname (
    UINT8                   Type,
    ACPI_NAMESPACE_NODE     *ObjHandle,
    char                    *Path);
#endif


/*
 * utownerid - Support for Table/Method Owner IDs
 */
ACPI_STATUS
AcpiUtAllocateOwnerId (
    ACPI_OWNER_ID           *OwnerId);

void
AcpiUtReleaseOwnerId (
    ACPI_OWNER_ID           *OwnerId);


/*
 * utresrc
 */
ACPI_STATUS
AcpiUtWalkAmlResources (
    ACPI_WALK_STATE         *WalkState,
    UINT8                   *Aml,
    ACPI_SIZE               AmlLength,
    ACPI_WALK_AML_CALLBACK  UserFunction,
    void                    **Context);

ACPI_STATUS
AcpiUtValidateResource (
    ACPI_WALK_STATE         *WalkState,
    void                    *Aml,
    UINT8                   *ReturnIndex);

UINT32
AcpiUtGetDescriptorLength (
    void                    *Aml);

UINT16
AcpiUtGetResourceLength (
    void                    *Aml);

UINT8
AcpiUtGetResourceHeaderLength (
    void                    *Aml);

UINT8
AcpiUtGetResourceType (
    void                    *Aml);

ACPI_STATUS
AcpiUtGetResourceEndTag (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    UINT8                   **EndTag);


/*
 * utstring - String and character utilities
 */
void
AcpiUtStrupr (
    char                    *SrcString);

void
AcpiUtStrlwr (
    char                    *SrcString);

int
AcpiUtStricmp (
    char                    *String1,
    char                    *String2);

ACPI_STATUS
AcpiUtStrtoul64 (
    char                    *String,
    UINT32                  Base,
    UINT64                  *RetInteger);

void
AcpiUtPrintString (
    char                    *String,
    UINT8                   MaxLength);

void
UtConvertBackslashes (
    char                    *Pathname);

BOOLEAN
AcpiUtValidAcpiName (
    UINT32                  Name);

BOOLEAN
AcpiUtValidAcpiChar (
    char                    Character,
    UINT32                  Position);

void
AcpiUtRepairName (
    char                    *Name);


/*
 * utmutex - mutex support
 */
ACPI_STATUS
AcpiUtMutexInitialize (
    void);

void
AcpiUtMutexTerminate (
    void);

ACPI_STATUS
AcpiUtAcquireMutex (
    ACPI_MUTEX_HANDLE       MutexId);

ACPI_STATUS
AcpiUtReleaseMutex (
    ACPI_MUTEX_HANDLE       MutexId);


/*
 * utalloc - memory allocation and object caching
 */
ACPI_STATUS
AcpiUtCreateCaches (
    void);

ACPI_STATUS
AcpiUtDeleteCaches (
    void);

ACPI_STATUS
AcpiUtValidateBuffer (
    ACPI_BUFFER             *Buffer);

ACPI_STATUS
AcpiUtInitializeBuffer (
    ACPI_BUFFER             *Buffer,
    ACPI_SIZE               RequiredLength);

void *
AcpiUtAllocate (
    ACPI_SIZE               Size,
    UINT32                  Component,
    const char              *Module,
    UINT32                  Line);

void *
AcpiUtAllocateZeroed (
    ACPI_SIZE               Size,
    UINT32                  Component,
    const char              *Module,
    UINT32                  Line);

#ifdef ACPI_DBG_TRACK_ALLOCATIONS
void *
AcpiUtAllocateAndTrack (
    ACPI_SIZE               Size,
    UINT32                  Component,
    const char              *Module,
    UINT32                  Line);

void *
AcpiUtAllocateZeroedAndTrack (
    ACPI_SIZE               Size,
    UINT32                  Component,
    const char              *Module,
    UINT32                  Line);

void
AcpiUtFreeAndTrack (
    void                    *Address,
    UINT32                  Component,
    const char              *Module,
    UINT32                  Line);

void
AcpiUtDumpAllocationInfo (
    void);

void
AcpiUtDumpAllocations (
    UINT32                  Component,
    const char              *Module);

ACPI_STATUS
AcpiUtCreateList (
    char                    *ListName,
    UINT16                  ObjectSize,
    ACPI_MEMORY_LIST        **ReturnCache);

#endif /* ACPI_DBG_TRACK_ALLOCATIONS */

/*
 * utaddress - address range check
 */
ACPI_STATUS
AcpiUtAddAddressRange (
    ACPI_ADR_SPACE_TYPE     SpaceId,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  Length,
    ACPI_NAMESPACE_NODE     *RegionNode);

void
AcpiUtRemoveAddressRange (
    ACPI_ADR_SPACE_TYPE     SpaceId,
    ACPI_NAMESPACE_NODE     *RegionNode);

UINT32
AcpiUtCheckAddressRange (
    ACPI_ADR_SPACE_TYPE     SpaceId,
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  Length,
    BOOLEAN                 Warn);

void
AcpiUtDeleteAddressLists (
    void);

/*
 * utxferror - various error/warning output functions
 */
void ACPI_INTERNAL_VAR_XFACE
AcpiUtPredefinedWarning (
    const char              *ModuleName,
    UINT32                  LineNumber,
    char                    *Pathname,
    UINT8                   NodeFlags,
    const char              *Format,
    ...);

void ACPI_INTERNAL_VAR_XFACE
AcpiUtPredefinedInfo (
    const char              *ModuleName,
    UINT32                  LineNumber,
    char                    *Pathname,
    UINT8                   NodeFlags,
    const char              *Format,
    ...);

void ACPI_INTERNAL_VAR_XFACE
AcpiUtPredefinedBiosError (
    const char              *ModuleName,
    UINT32                  LineNumber,
    char                    *Pathname,
    UINT8                   NodeFlags,
    const char              *Format,
    ...);

void
AcpiUtNamespaceError (
    const char              *ModuleName,
    UINT32                  LineNumber,
    const char              *InternalName,
    ACPI_STATUS             LookupStatus);

void
AcpiUtMethodError (
    const char              *ModuleName,
    UINT32                  LineNumber,
    const char              *Message,
    ACPI_NAMESPACE_NODE     *Node,
    const char              *Path,
    ACPI_STATUS             LookupStatus);

#endif /* _ACUTILS_H */
