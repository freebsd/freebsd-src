/******************************************************************************
 *
 * Name: acutils.h -- prototypes for the common (subsystem-wide) procedures
 *       $Revision: 155 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2003, Intel Corp.
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

#ifndef _ACUTILS_H
#define _ACUTILS_H


typedef
ACPI_STATUS (*ACPI_PKG_CALLBACK) (
    UINT8                   ObjectType,
    ACPI_OPERAND_OBJECT     *SourceObject,
    ACPI_GENERIC_STATE      *State,
    void                    *Context);


ACPI_STATUS
AcpiUtWalkPackageTree (
    ACPI_OPERAND_OBJECT     *SourceObject,
    void                    *TargetObject,
    ACPI_PKG_CALLBACK       WalkCallback,
    void                    *Context);


typedef struct acpi_pkg_info
{
    UINT8                   *FreeSpace;
    ACPI_SIZE               Length;
    UINT32                  ObjectSpace;
    UINT32                  NumPackages;

} ACPI_PKG_INFO;

#define REF_INCREMENT       (UINT16) 0
#define REF_DECREMENT       (UINT16) 1
#define REF_FORCE_DELETE    (UINT16) 2

/* AcpiUtDumpBuffer */

#define DB_BYTE_DISPLAY     1
#define DB_WORD_DISPLAY     2
#define DB_DWORD_DISPLAY    4
#define DB_QWORD_DISPLAY    8


/* Global initialization interfaces */

void
AcpiUtInitGlobals (
    void);

void
AcpiUtTerminate (
    void);


/*
 * UtInit - miscellaneous initialization and shutdown
 */

ACPI_STATUS
AcpiUtHardwareInitialize (
    void);

void
AcpiUtSubsystemShutdown (
    void);

ACPI_STATUS
AcpiUtValidateFadt (
    void);

/*
 * UtGlobal - Global data structures and procedures
 */

#if defined(ACPI_DEBUG_OUTPUT) || defined(ACPI_DEBUGGER)

char *
AcpiUtGetMutexName (
    UINT32                  MutexId);

#endif

char *
AcpiUtGetTypeName (
    ACPI_OBJECT_TYPE        Type);

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
    ACPI_INTEGER            Integer,
    UINT32                  Position);

BOOLEAN
AcpiUtValidObjectType (
    ACPI_OBJECT_TYPE        Type);

ACPI_OWNER_ID
AcpiUtAllocateOwnerId (
    UINT32                  IdType);


/*
 * UtClib - Local implementations of C library functions
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
    ACPI_NATIVE_UINT        Value,
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
#define ACPI_IS_ASCII(c)  ((c) < 0x80)

#endif /* ACPI_USE_SYSTEM_CLIBRARY */

/*
 * UtCopy - Object construction and conversion interfaces
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
AcpiUtCopyIelementToEelement (
    UINT8                   ObjectType,
    ACPI_OPERAND_OBJECT     *SourceObject,
    ACPI_GENERIC_STATE      *State,
    void                    *Context);

ACPI_STATUS
AcpiUtCopyIelementToIelement (
    UINT8                   ObjectType,
    ACPI_OPERAND_OBJECT     *SourceObject,
    ACPI_GENERIC_STATE      *State,
    void                    *Context);

ACPI_STATUS
AcpiUtCopyIobjectToEobject (
    ACPI_OPERAND_OBJECT     *Obj,
    ACPI_BUFFER             *RetBuffer);

ACPI_STATUS
AcpiUtCopyEsimpleToIsimple(
    ACPI_OBJECT             *UserObj,
    ACPI_OPERAND_OBJECT     **ReturnObj);

ACPI_STATUS
AcpiUtCopyEobjectToIobject (
    ACPI_OBJECT             *Obj,
    ACPI_OPERAND_OBJECT     **InternalObj);

ACPI_STATUS
AcpiUtCopyISimpleToIsimple (
    ACPI_OPERAND_OBJECT     *SourceObj,
    ACPI_OPERAND_OBJECT     *DestObj);

ACPI_STATUS
AcpiUtCopyIpackageToIpackage (
    ACPI_OPERAND_OBJECT     *SourceObj,
    ACPI_OPERAND_OBJECT     *DestObj,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiUtCopySimpleObject (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     *DestDesc);

ACPI_STATUS
AcpiUtCopyIobjectToIobject (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     **DestDesc,
    ACPI_WALK_STATE         *WalkState);


/*
 * UtCreate - Object creation
 */

ACPI_STATUS
AcpiUtUpdateObjectReference (
    ACPI_OPERAND_OBJECT     *Object,
    UINT16                  Action);


/*
 * UtDebug - Debug interfaces
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
    ACPI_DEBUG_PRINT_INFO   *DbgInfo);

void
AcpiUtTracePtr (
    UINT32                  LineNumber,
    ACPI_DEBUG_PRINT_INFO   *DbgInfo,
    void                    *Pointer);

void
AcpiUtTraceU32 (
    UINT32                  LineNumber,
    ACPI_DEBUG_PRINT_INFO   *DbgInfo,
    UINT32                  Integer);

void
AcpiUtTraceStr (
    UINT32                  LineNumber,
    ACPI_DEBUG_PRINT_INFO   *DbgInfo,
    char                    *String);

void
AcpiUtExit (
    UINT32                  LineNumber,
    ACPI_DEBUG_PRINT_INFO   *DbgInfo);

void
AcpiUtStatusExit (
    UINT32                  LineNumber,
    ACPI_DEBUG_PRINT_INFO   *DbgInfo,
    ACPI_STATUS             Status);

void
AcpiUtValueExit (
    UINT32                  LineNumber,
    ACPI_DEBUG_PRINT_INFO   *DbgInfo,
    ACPI_INTEGER            Value);

void
AcpiUtPtrExit (
    UINT32                  LineNumber,
    ACPI_DEBUG_PRINT_INFO   *DbgInfo,
    UINT8                   *Ptr);

void
AcpiUtReportInfo (
    char                    *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId);

void
AcpiUtReportError (
    char                    *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId);

void
AcpiUtReportWarning (
    char                    *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId);

void
AcpiUtDumpBuffer (
    UINT8                   *Buffer,
    UINT32                  Count,
    UINT32                  Display,
    UINT32                  componentId);

void ACPI_INTERNAL_VAR_XFACE
AcpiUtDebugPrint (
    UINT32                  RequestedDebugLevel,
    UINT32                  LineNumber,
    ACPI_DEBUG_PRINT_INFO   *DbgInfo,
    char                    *Format,
    ...) ACPI_PRINTF_LIKE_FUNC;

void ACPI_INTERNAL_VAR_XFACE
AcpiUtDebugPrintRaw (
    UINT32                  RequestedDebugLevel,
    UINT32                  LineNumber,
    ACPI_DEBUG_PRINT_INFO   *DbgInfo,
    char                    *Format,
    ...) ACPI_PRINTF_LIKE_FUNC;


/*
 * UtDelete - Object deletion
 */

void
AcpiUtDeleteInternalObj (
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
 * UtEval - object evaluation
 */

/* Method name strings */

#define METHOD_NAME__HID        "_HID"
#define METHOD_NAME__CID        "_CID"
#define METHOD_NAME__UID        "_UID"
#define METHOD_NAME__ADR        "_ADR"
#define METHOD_NAME__STA        "_STA"
#define METHOD_NAME__REG        "_REG"
#define METHOD_NAME__SEG        "_SEG"
#define METHOD_NAME__BBN        "_BBN"
#define METHOD_NAME__PRT        "_PRT"
#define METHOD_NAME__CRS        "_CRS"
#define METHOD_NAME__PRS        "_PRS"


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
    ACPI_INTEGER            *Address);

ACPI_STATUS
AcpiUtExecute_HID (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    ACPI_DEVICE_ID          *Hid);

ACPI_STATUS
AcpiUtExecute_CID (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    ACPI_COMPATIBLE_ID_LIST **ReturnCidList);

ACPI_STATUS
AcpiUtExecute_STA (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    UINT32                  *StatusFlags);

ACPI_STATUS
AcpiUtExecute_UID (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    ACPI_DEVICE_ID          *Uid);


/*
 * UtMutex - mutual exclusion interfaces
 */

ACPI_STATUS
AcpiUtMutexInitialize (
    void);

void
AcpiUtMutexTerminate (
    void);

ACPI_STATUS
AcpiUtCreateMutex (
    ACPI_MUTEX_HANDLE       MutexId);

ACPI_STATUS
AcpiUtDeleteMutex (
    ACPI_MUTEX_HANDLE       MutexId);

ACPI_STATUS
AcpiUtAcquireMutex (
    ACPI_MUTEX_HANDLE       MutexId);

ACPI_STATUS
AcpiUtReleaseMutex (
    ACPI_MUTEX_HANDLE       MutexId);


/*
 * UtObject - internal object create/delete/cache routines
 */

ACPI_OPERAND_OBJECT  *
AcpiUtCreateInternalObjectDbg (
    char                    *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId,
    ACPI_OBJECT_TYPE        Type);

void *
AcpiUtAllocateObjectDescDbg (
    char                    *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId);

#define AcpiUtCreateInternalObject(t)   AcpiUtCreateInternalObjectDbg (_THIS_MODULE,__LINE__,_COMPONENT,t)
#define AcpiUtAllocateObjectDesc()      AcpiUtAllocateObjectDescDbg (_THIS_MODULE,__LINE__,_COMPONENT)

void
AcpiUtDeleteObjectDesc (
    ACPI_OPERAND_OBJECT     *Object);

BOOLEAN
AcpiUtValidInternalObject (
    void                    *Object);

ACPI_OPERAND_OBJECT *
AcpiUtCreateBufferObject (
    ACPI_SIZE               BufferSize);


/*
 * UtRefCnt - Object reference count management
 */

void
AcpiUtAddReference (
    ACPI_OPERAND_OBJECT     *Object);

void
AcpiUtRemoveReference (
    ACPI_OPERAND_OBJECT     *Object);

/*
 * UtSize - Object size routines
 */

ACPI_STATUS
AcpiUtGetSimpleObjectSize (
    ACPI_OPERAND_OBJECT     *Obj,
    ACPI_SIZE               *ObjLength);

ACPI_STATUS
AcpiUtGetPackageObjectSize (
    ACPI_OPERAND_OBJECT     *Obj,
    ACPI_SIZE               *ObjLength);

ACPI_STATUS
AcpiUtGetObjectSize(
    ACPI_OPERAND_OBJECT     *Obj,
    ACPI_SIZE               *ObjLength);

ACPI_STATUS
AcpiUtGetElementLength (
    UINT8                   ObjectType,
    ACPI_OPERAND_OBJECT     *SourceObject,
    ACPI_GENERIC_STATE      *State,
    void                    *Context);


/*
 * UtState - Generic state creation/cache routines
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

void
AcpiUtDeleteGenericStateCache (
    void);

void
AcpiUtDeleteObjectCache (
    void);

/*
 * utmisc
 */

void
AcpiUtPrintString (
    char                    *String,
    UINT8                   MaxLength);

ACPI_STATUS
AcpiUtDivide (
    ACPI_INTEGER            *InDividend,
    ACPI_INTEGER            *InDivisor,
    ACPI_INTEGER            *OutQuotient,
    ACPI_INTEGER            *OutRemainder);

ACPI_STATUS
AcpiUtShortDivide (
    ACPI_INTEGER            *InDividend,
    UINT32                  Divisor,
    ACPI_INTEGER            *OutQuotient,
    UINT32                  *OutRemainder);

BOOLEAN
AcpiUtValidAcpiName (
    UINT32                  Name);

BOOLEAN
AcpiUtValidAcpiCharacter (
    char                    Character);

ACPI_STATUS
AcpiUtStrtoul64 (
    char                    *String,
    UINT32                  Base,
    ACPI_INTEGER            *RetInteger);

char *
AcpiUtStrupr (
    char                    *SrcString);

UINT8 *
AcpiUtGetResourceEndTag (
    ACPI_OPERAND_OBJECT     *ObjDesc);

UINT8
AcpiUtGenerateChecksum (
    UINT8                   *Buffer,
    UINT32                  Length);

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
 * Utalloc - memory allocation and object caching
 */

void *
AcpiUtAcquireFromCache (
    UINT32                  ListId);

void
AcpiUtReleaseToCache (
    UINT32                  ListId,
    void                    *Object);

void
AcpiUtDeleteGenericCache (
    UINT32                  ListId);

ACPI_STATUS
AcpiUtValidateBuffer (
    ACPI_BUFFER             *Buffer);

ACPI_STATUS
AcpiUtInitializeBuffer (
    ACPI_BUFFER             *Buffer,
    ACPI_SIZE               RequiredLength);


/* Memory allocation functions */

void *
AcpiUtAllocate (
    ACPI_SIZE               Size,
    UINT32                  Component,
    char                    *Module,
    UINT32                  Line);

void *
AcpiUtCallocate (
    ACPI_SIZE               Size,
    UINT32                  Component,
    char                    *Module,
    UINT32                  Line);


#ifdef ACPI_DBG_TRACK_ALLOCATIONS

void *
AcpiUtAllocateAndTrack (
    ACPI_SIZE               Size,
    UINT32                  Component,
    char                    *Module,
    UINT32                  Line);

void *
AcpiUtCallocateAndTrack (
    ACPI_SIZE               Size,
    UINT32                  Component,
    char                    *Module,
    UINT32                  Line);

void
AcpiUtFreeAndTrack (
    void                    *Address,
    UINT32                  Component,
    char                    *Module,
    UINT32                  Line);

ACPI_DEBUG_MEM_BLOCK *
AcpiUtFindAllocation (
    UINT32                  ListId,
    void                    *Allocation);

ACPI_STATUS
AcpiUtTrackAllocation (
    UINT32                  ListId,
    ACPI_DEBUG_MEM_BLOCK    *Address,
    ACPI_SIZE               Size,
    UINT8                   AllocType,
    UINT32                  Component,
    char                    *Module,
    UINT32                  Line);

ACPI_STATUS
AcpiUtRemoveAllocation (
    UINT32                  ListId,
    ACPI_DEBUG_MEM_BLOCK    *Address,
    UINT32                  Component,
    char                    *Module,
    UINT32                  Line);

void
AcpiUtDumpAllocationInfo (
    void);

void
AcpiUtDumpAllocations (
    UINT32                  Component,
    char                    *Module);
#endif


#endif /* _ACUTILS_H */
