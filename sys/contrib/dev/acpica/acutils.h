/******************************************************************************
 *
 * Name: acutils.h -- prototypes for the common (subsystem-wide) procedures
 *       $Revision: 100 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, 2000, 2001, Intel Corp.
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
    UINT32                  Length;
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

ACPI_STATUS
AcpiUtSubsystemShutdown (
    void);

ACPI_STATUS
AcpiUtValidateFadt (
    void);

/*
 * UtGlobal - Global data structures and procedures
 */

#ifdef ACPI_DEBUG

NATIVE_CHAR *
AcpiUtGetMutexName (
    UINT32                  MutexId);

NATIVE_CHAR *
AcpiUtGetTypeName (
    UINT32                  Type);

NATIVE_CHAR *
AcpiUtGetRegionName (
    UINT8                   SpaceId);

#endif


BOOLEAN
AcpiUtValidObjectType (
    UINT32                  Type);

ACPI_OWNER_ID
AcpiUtAllocateOwnerId (
    UINT32                  IdType);


/*
 * UtClib - Local implementations of C library functions
 */

#ifndef ACPI_USE_SYSTEM_CLIBRARY

UINT32
AcpiUtStrlen (
    const NATIVE_CHAR       *String);

NATIVE_CHAR *
AcpiUtStrcpy (
    NATIVE_CHAR             *DstString,
    const NATIVE_CHAR       *SrcString);

NATIVE_CHAR *
AcpiUtStrncpy (
    NATIVE_CHAR             *DstString,
    const NATIVE_CHAR       *SrcString,
    NATIVE_UINT             Count);

UINT32
AcpiUtStrncmp (
    const NATIVE_CHAR       *String1,
    const NATIVE_CHAR       *String2,
    NATIVE_UINT             Count);

UINT32
AcpiUtStrcmp (
    const NATIVE_CHAR       *String1,
    const NATIVE_CHAR       *String2);

NATIVE_CHAR *
AcpiUtStrcat (
    NATIVE_CHAR             *DstString,
    const NATIVE_CHAR       *SrcString);

NATIVE_CHAR *
AcpiUtStrncat (
    NATIVE_CHAR             *DstString,
    const NATIVE_CHAR       *SrcString,
    NATIVE_UINT             Count);

UINT32
AcpiUtStrtoul (
    const NATIVE_CHAR       *String,
    NATIVE_CHAR             **Terminator,
    UINT32                  Base);

NATIVE_CHAR *
AcpiUtStrstr (
    NATIVE_CHAR             *String1,
    NATIVE_CHAR             *String2);

void *
AcpiUtMemcpy (
    void                    *Dest,
    const void              *Src,
    NATIVE_UINT             Count);

void *
AcpiUtMemset (
    void                    *Dest,
    NATIVE_UINT             Value,
    NATIVE_UINT             Count);

UINT32
AcpiUtToUpper (
    UINT32                  c);

UINT32
AcpiUtToLower (
    UINT32                  c);

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
AcpiUtCopyIobjectToEobject (
    ACPI_OPERAND_OBJECT     *Obj,
    ACPI_BUFFER             *RetBuffer);

ACPI_STATUS
AcpiUtCopyEsimpleToIsimple(
    ACPI_OBJECT             *UserObj,
    ACPI_OPERAND_OBJECT     *Obj);

ACPI_STATUS
AcpiUtCopyEobjectToIobject (
    ACPI_OBJECT             *Obj,
    ACPI_OPERAND_OBJECT     *InternalObj);

ACPI_STATUS
AcpiUtCopyISimpleToIsimple (
    ACPI_OPERAND_OBJECT     *SourceObj,
    ACPI_OPERAND_OBJECT     *DestObj);

ACPI_STATUS
AcpiUtCopyIpackageToIpackage (
    ACPI_OPERAND_OBJECT     *SourceObj,
    ACPI_OPERAND_OBJECT     *DestObj,
    ACPI_WALK_STATE         *WalkState);


/*
 * UtCreate - Object creation
 */

ACPI_STATUS
AcpiUtUpdateObjectReference (
    ACPI_OPERAND_OBJECT     *Object,
    UINT16                  Action);

ACPI_OPERAND_OBJECT  *
_UtCreateInternalObject (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId,
    ACPI_OBJECT_TYPE8       Type);


/*
 * UtDebug - Debug interfaces
 */

UINT32
GetDebugLevel (
    void);

void
SetDebugLevel (
    UINT32                  level);

void
FunctionTrace (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId,
    NATIVE_CHAR             *FunctionName);

void
FunctionTracePtr (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId,
    NATIVE_CHAR             *FunctionName,
    void                    *Pointer);

void
FunctionTraceU32 (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId,
    NATIVE_CHAR             *FunctionName,
    UINT32                  Integer);

void
FunctionTraceStr (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId,
    NATIVE_CHAR             *FunctionName,
    NATIVE_CHAR             *String);

void
FunctionExit (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId,
    NATIVE_CHAR             *FunctionName);

void
FunctionStatusExit (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId,
    NATIVE_CHAR             *FunctionName,
    ACPI_STATUS             Status);

void
FunctionValueExit (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId,
    NATIVE_CHAR             *FunctionName,
    ACPI_INTEGER            Value);

void
FunctionPtrExit (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId,
    NATIVE_CHAR             *FunctionName,
    UINT8                   *Ptr);

void
DebugPrintPrefix (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber);

void
DebugPrint (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId,
    UINT32                  PrintLevel,
    NATIVE_CHAR             *Format, ...);

void
DebugPrintRaw (
    NATIVE_CHAR             *Format, ...);

void
_ReportInfo (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId);

void
_ReportError (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId);

void
_ReportWarning (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId);

void
AcpiUtDumpBuffer (
    UINT8                   *Buffer,
    UINT32                  Count,
    UINT32                  Display,
    UINT32                  componentId);


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

ACPI_STATUS
AcpiUtDeleteInternalObjectList (
    ACPI_OPERAND_OBJECT     **ObjList);


/*
 * UtEval - object evaluation
 */

/* Method name strings */

#define METHOD_NAME__HID        "_HID"
#define METHOD_NAME__UID        "_UID"
#define METHOD_NAME__ADR        "_ADR"
#define METHOD_NAME__STA        "_STA"
#define METHOD_NAME__REG        "_REG"
#define METHOD_NAME__SEG        "_SEG"
#define METHOD_NAME__BBN        "_BBN"


ACPI_STATUS
AcpiUtEvaluateNumericObject (
    NATIVE_CHAR             *ObjectName,
    ACPI_NAMESPACE_NODE     *DeviceNode,
    ACPI_INTEGER            *Address);

ACPI_STATUS
AcpiUtExecute_HID (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    ACPI_DEVICE_ID          *Hid);

ACPI_STATUS
AcpiUtExecute_STA (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    UINT32                  *StatusFlags);

ACPI_STATUS
AcpiUtExecute_UID (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    ACPI_DEVICE_ID          *Uid);


/*
 * UtError - exception interfaces
 */

NATIVE_CHAR *
AcpiUtFormatException (
    ACPI_STATUS             Status);


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

void *
_UtAllocateObjectDesc (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId);

#define AcpiUtCreateInternalObject(t)   _UtCreateInternalObject(_THIS_MODULE,__LINE__,_COMPONENT,t)
#define AcpiUtAllocateObjectDesc()      _UtAllocateObjectDesc(_THIS_MODULE,__LINE__,_COMPONENT)

void
AcpiUtDeleteObjectDesc (
    ACPI_OPERAND_OBJECT     *Object);

BOOLEAN
AcpiUtValidInternalObject (
    void                    *Object);


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
    UINT32                  *ObjLength);

ACPI_STATUS
AcpiUtGetPackageObjectSize (
    ACPI_OPERAND_OBJECT     *Obj,
    UINT32                  *ObjLength);

ACPI_STATUS
AcpiUtGetObjectSize(
    ACPI_OPERAND_OBJECT     *Obj,
    UINT32                  *ObjLength);


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
 * Ututils
 */

BOOLEAN
AcpiUtValidAcpiName (
    UINT32                  Name);

BOOLEAN
AcpiUtValidAcpiCharacter (
    NATIVE_CHAR             Character);

NATIVE_CHAR *
AcpiUtStrupr (
    NATIVE_CHAR             *SrcString);

ACPI_STATUS
AcpiUtResolvePackageReferences (
    ACPI_OPERAND_OBJECT     *ObjDesc);


#ifdef ACPI_DEBUG
void
AcpiUtDisplayInitPathname (
    ACPI_HANDLE             ObjHandle,
    char                    *Path);

#endif


/*
 * Memory allocation functions and related macros.
 * Macros that expand to include filename and line number
 */

void *
_UtAllocate (
    UINT32                  Size,
    UINT32                  Component,
    NATIVE_CHAR             *Module,
    UINT32                  Line);

void *
_UtCallocate (
    UINT32                  Size,
    UINT32                  Component,
    NATIVE_CHAR             *Module,
    UINT32                  Line);

void
_UtFree (
    void                    *Address,
    UINT32                  Component,
    NATIVE_CHAR             *Module,
    UINT32                  Line);

void
AcpiUtInitStaticObject (
    ACPI_OPERAND_OBJECT     *ObjDesc);


#ifdef ACPI_DEBUG_TRACK_ALLOCATIONS
void
AcpiUtDumpAllocationInfo (
    void);

void
AcpiUtDumpCurrentAllocations (
    UINT32                  Component,
    NATIVE_CHAR             *Module);
#endif


#define AcpiUtAllocate(a)   _UtAllocate(a,_COMPONENT,_THIS_MODULE,__LINE__)
#define AcpiUtCallocate(a)  _UtCallocate(a, _COMPONENT,_THIS_MODULE,__LINE__)
#define AcpiUtFree(a)       _UtFree(a,_COMPONENT,_THIS_MODULE,__LINE__)


#endif /* _ACUTILS_H */
