/******************************************************************************
 *
 * Name: accommon.h -- prototypes for the common (subsystem-wide) procedures
 *       $Revision: 81 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, Intel Corp.  All rights
 * reserved.
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

#ifndef _ACCOMMON_H
#define _ACCOMMON_H


#define REF_INCREMENT       (UINT16) 0
#define REF_DECREMENT       (UINT16) 1
#define REF_FORCE_DELETE    (UINT16) 2

/* AcpiCmDumpBuffer */

#define DB_BYTE_DISPLAY     1
#define DB_WORD_DISPLAY     2
#define DB_DWORD_DISPLAY    4
#define DB_QWORD_DISPLAY    8


/* Global initialization interfaces */

void
AcpiCmInitGlobals (
    void);

void
AcpiCmTerminate (
    void);


/*
 * CmInit - miscellaneous initialization and shutdown
 */

ACPI_STATUS
AcpiCmHardwareInitialize (
    void);

ACPI_STATUS
AcpiCmSubsystemShutdown (
    void);

ACPI_STATUS
AcpiCmValidateFadt (
    void);

/*
 * CmGlobal - Global data structures and procedures
 */

NATIVE_CHAR *
AcpiCmGetMutexName (
    UINT32                  MutexId);

NATIVE_CHAR *
AcpiCmGetTypeName (
    UINT32                  Type);

BOOLEAN
AcpiCmValidObjectType (
    UINT32                  Type);

ACPI_OWNER_ID
AcpiCmAllocateOwnerId (
    UINT32                  IdType);


/*
 * CmClib - Local implementations of C library functions
 */

NATIVE_UINT
AcpiCmStrlen (
    const NATIVE_CHAR       *String);

NATIVE_CHAR *
AcpiCmStrcpy (
    NATIVE_CHAR             *DstString,
    const NATIVE_CHAR       *SrcString);

NATIVE_CHAR *
AcpiCmStrncpy (
    NATIVE_CHAR             *DstString,
    const NATIVE_CHAR       *SrcString,
    NATIVE_UINT             Count);

UINT32
AcpiCmStrncmp (
    const NATIVE_CHAR       *String1,
    const NATIVE_CHAR       *String2,
    NATIVE_UINT             Count);

UINT32
AcpiCmStrcmp (
    const NATIVE_CHAR       *String1,
    const NATIVE_CHAR       *String2);

NATIVE_CHAR *
AcpiCmStrcat (
    NATIVE_CHAR             *DstString,
    const NATIVE_CHAR       *SrcString);

NATIVE_CHAR *
AcpiCmStrncat (
    NATIVE_CHAR             *DstString,
    const NATIVE_CHAR       *SrcString,
    NATIVE_UINT             Count);

UINT32
AcpiCmStrtoul (
    const NATIVE_CHAR       *String,
    NATIVE_CHAR             **Terminator,
    UINT32                  Base);

NATIVE_CHAR *
AcpiCmStrstr (
    NATIVE_CHAR             *String1,
    NATIVE_CHAR             *String2);

NATIVE_CHAR *
AcpiCmStrupr (
    NATIVE_CHAR             *SrcString);

void *
AcpiCmMemcpy (
    void                    *Dest,
    const void              *Src,
    NATIVE_UINT             Count);

void *
AcpiCmMemset (
    void                    *Dest,
    UINT32                  Value,
    NATIVE_UINT             Count);

UINT32
AcpiCmToUpper (
    UINT32                  c);

UINT32
AcpiCmToLower (
    UINT32                  c);


/*
 * CmCopy - Object construction and conversion interfaces
 */

ACPI_STATUS
AcpiCmBuildSimpleObject(
    ACPI_OPERAND_OBJECT     *Obj,
    ACPI_OBJECT             *UserObj,
    UINT8                   *DataSpace,
    UINT32                  *BufferSpaceUsed);

ACPI_STATUS
AcpiCmBuildPackageObject (
    ACPI_OPERAND_OBJECT     *Obj,
    UINT8                   *Buffer,
    UINT32                  *SpaceUsed);

ACPI_STATUS
AcpiCmBuildExternalObject (
    ACPI_OPERAND_OBJECT     *Obj,
    ACPI_BUFFER             *RetBuffer);

ACPI_STATUS
AcpiCmBuildInternalSimpleObject(
    ACPI_OBJECT             *UserObj,
    ACPI_OPERAND_OBJECT     *Obj);

ACPI_STATUS
AcpiCmBuildInternalObject (
    ACPI_OBJECT             *Obj,
    ACPI_OPERAND_OBJECT     *InternalObj);

ACPI_STATUS
AcpiCmCopyInternalSimpleObject (
    ACPI_OPERAND_OBJECT     *SourceObj,
    ACPI_OPERAND_OBJECT     *DestObj);

ACPI_STATUS
AcpiCmBuildCopyInternalPackageObject (
    ACPI_OPERAND_OBJECT     *SourceObj,
    ACPI_OPERAND_OBJECT     *DestObj);


/*
 * CmCreate - Object creation
 */

ACPI_STATUS
AcpiCmUpdateObjectReference (
    ACPI_OPERAND_OBJECT     *Object,
    UINT16                  Action);

ACPI_OPERAND_OBJECT  *
_CmCreateInternalObject (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId,
    OBJECT_TYPE_INTERNAL    Type);


/*
 * CmDebug - Debug interfaces
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
    NATIVE_UINT             Value);

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
AcpiCmDumpBuffer (
    UINT8                   *Buffer,
    UINT32                  Count,
    UINT32                  Display,
    UINT32                  componentId);


/*
 * CmDelete - Object deletion
 */

void
AcpiCmDeleteInternalObj (
    ACPI_OPERAND_OBJECT     *Object);

void
AcpiCmDeleteInternalPackageObject (
    ACPI_OPERAND_OBJECT     *Object);

void
AcpiCmDeleteInternalSimpleObject (
    ACPI_OPERAND_OBJECT     *Object);

ACPI_STATUS
AcpiCmDeleteInternalObjectList (
    ACPI_OPERAND_OBJECT     **ObjList);


/*
 * CmEval - object evaluation
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
AcpiCmEvaluateNumericObject (
    NATIVE_CHAR             *ObjectName,
    ACPI_NAMESPACE_NODE     *DeviceNode,
    ACPI_INTEGER            *Address);

ACPI_STATUS
AcpiCmExecute_HID (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    DEVICE_ID               *Hid);

ACPI_STATUS
AcpiCmExecute_STA (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    UINT32                  *StatusFlags);

ACPI_STATUS
AcpiCmExecute_UID (
    ACPI_NAMESPACE_NODE     *DeviceNode,
    DEVICE_ID               *Uid);


/*
 * CmError - exception interfaces
 */

NATIVE_CHAR *
AcpiCmFormatException (
    ACPI_STATUS             Status);


/*
 * CmMutex - mutual exclusion interfaces
 */

ACPI_STATUS
AcpiCmMutexInitialize (
    void);

void
AcpiCmMutexTerminate (
    void);

ACPI_STATUS
AcpiCmCreateMutex (
    ACPI_MUTEX_HANDLE       MutexId);

ACPI_STATUS
AcpiCmDeleteMutex (
    ACPI_MUTEX_HANDLE       MutexId);

ACPI_STATUS
AcpiCmAcquireMutex (
    ACPI_MUTEX_HANDLE       MutexId);

ACPI_STATUS
AcpiCmReleaseMutex (
    ACPI_MUTEX_HANDLE       MutexId);


/*
 * CmObject - internal object create/delete/cache routines
 */

void *
_CmAllocateObjectDesc (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId);

#define AcpiCmCreateInternalObject(t)   _CmCreateInternalObject(_THIS_MODULE,__LINE__,_COMPONENT,t)
#define AcpiCmAllocateObjectDesc()      _CmAllocateObjectDesc(_THIS_MODULE,__LINE__,_COMPONENT)

void
AcpiCmDeleteObjectDesc (
    ACPI_OPERAND_OBJECT     *Object);

BOOLEAN
AcpiCmValidInternalObject (
    void                    *Object);


/*
 * CmRefCnt - Object reference count management
 */

void
AcpiCmAddReference (
    ACPI_OPERAND_OBJECT     *Object);

void
AcpiCmRemoveReference (
    ACPI_OPERAND_OBJECT     *Object);

/*
 * CmSize - Object size routines
 */

ACPI_STATUS
AcpiCmGetSimpleObjectSize (
    ACPI_OPERAND_OBJECT     *Obj,
    UINT32                  *ObjLength);

ACPI_STATUS
AcpiCmGetPackageObjectSize (
    ACPI_OPERAND_OBJECT     *Obj,
    UINT32                  *ObjLength);

ACPI_STATUS
AcpiCmGetObjectSize(
    ACPI_OPERAND_OBJECT     *Obj,
    UINT32                  *ObjLength);


/*
 * CmState - Generic state creation/cache routines
 */

void
AcpiCmPushGenericState (
    ACPI_GENERIC_STATE      **ListHead,
    ACPI_GENERIC_STATE      *State);

ACPI_GENERIC_STATE *
AcpiCmPopGenericState (
    ACPI_GENERIC_STATE      **ListHead);


ACPI_GENERIC_STATE *
AcpiCmCreateGenericState (
    void);

ACPI_GENERIC_STATE *
AcpiCmCreateUpdateState (
    ACPI_OPERAND_OBJECT     *Object,
    UINT16                  Action);

ACPI_STATUS
AcpiCmCreateUpdateStateAndPush (
    ACPI_OPERAND_OBJECT     *Object,
    UINT16                  Action,
    ACPI_GENERIC_STATE      **StateList);

ACPI_GENERIC_STATE *
AcpiCmCreateControlState (
    void);

void
AcpiCmDeleteGenericState (
    ACPI_GENERIC_STATE      *State);

void
AcpiCmDeleteGenericStateCache (
    void);

void
AcpiCmDeleteObjectCache (
    void);

/*
 * Cmutils
 */

BOOLEAN
AcpiCmValidAcpiName (
    UINT32                  Name);

BOOLEAN
AcpiCmValidAcpiCharacter (
    NATIVE_CHAR             Character);

ACPI_STATUS
AcpiCmResolvePackageReferences (
    ACPI_OPERAND_OBJECT     *ObjDesc);


/*
 * Memory allocation functions and related macros.
 * Macros that expand to include filename and line number
 */

void *
_CmAllocate (
    UINT32                  Size,
    UINT32                  Component,
    NATIVE_CHAR             *Module,
    UINT32                  Line);

void *
_CmCallocate (
    UINT32                  Size,
    UINT32                  Component,
    NATIVE_CHAR             *Module,
    UINT32                  Line);

void
_CmFree (
    void                    *Address,
    UINT32                  Component,
    NATIVE_CHAR             *Module,
    UINT32                  Line);

void
AcpiCmInitStaticObject (
    ACPI_OPERAND_OBJECT     *ObjDesc);

#define AcpiCmAllocate(a)               _CmAllocate(a,_COMPONENT,_THIS_MODULE,__LINE__)
#define AcpiCmCallocate(a)              _CmCallocate(a, _COMPONENT,_THIS_MODULE,__LINE__)
#define AcpiCmFree(a)                   _CmFree(a,_COMPONENT,_THIS_MODULE,__LINE__)

#ifndef ACPI_DEBUG

#define AcpiCmAddElementToAllocList(a,b,c,d,e,f)
#define AcpiCmDeleteElementFromAllocList(a,b,c,d)
#define AcpiCmDumpCurrentAllocations(a,b)
#define AcpiCmDumpAllocationInfo()

#define DECREMENT_OBJECT_METRICS(a)
#define INCREMENT_OBJECT_METRICS(a)
#define INITIALIZE_ALLOCATION_METRICS()
#define DECREMENT_NAME_TABLE_METRICS(a)
#define INCREMENT_NAME_TABLE_METRICS(a)

#else

#define INITIALIZE_ALLOCATION_METRICS() \
    AcpiGbl_CurrentObjectCount = 0; \
    AcpiGbl_CurrentObjectSize = 0; \
    AcpiGbl_RunningObjectCount = 0; \
    AcpiGbl_RunningObjectSize = 0; \
    AcpiGbl_MaxConcurrentObjectCount = 0; \
    AcpiGbl_MaxConcurrentObjectSize = 0; \
    AcpiGbl_CurrentAllocSize = 0; \
    AcpiGbl_CurrentAllocCount = 0; \
    AcpiGbl_RunningAllocSize = 0; \
    AcpiGbl_RunningAllocCount = 0; \
    AcpiGbl_MaxConcurrentAllocSize = 0; \
    AcpiGbl_MaxConcurrentAllocCount = 0; \
    AcpiGbl_CurrentNodeCount = 0; \
    AcpiGbl_CurrentNodeSize = 0; \
    AcpiGbl_MaxConcurrentNodeCount = 0


#define DECREMENT_OBJECT_METRICS(a) \
    AcpiGbl_CurrentObjectCount--; \
    AcpiGbl_CurrentObjectSize -= a

#define INCREMENT_OBJECT_METRICS(a) \
    AcpiGbl_CurrentObjectCount++; \
    AcpiGbl_RunningObjectCount++; \
    if (AcpiGbl_MaxConcurrentObjectCount < AcpiGbl_CurrentObjectCount) \
    { \
        AcpiGbl_MaxConcurrentObjectCount = AcpiGbl_CurrentObjectCount; \
    } \
    AcpiGbl_RunningObjectSize += a; \
    AcpiGbl_CurrentObjectSize += a; \
    if (AcpiGbl_MaxConcurrentObjectSize < AcpiGbl_CurrentObjectSize) \
    { \
        AcpiGbl_MaxConcurrentObjectSize = AcpiGbl_CurrentObjectSize; \
    }

#define DECREMENT_NAME_TABLE_METRICS(a) \
    AcpiGbl_CurrentNodeCount--; \
    AcpiGbl_CurrentNodeSize -= (a)

#define INCREMENT_NAME_TABLE_METRICS(a) \
    AcpiGbl_CurrentNodeCount++; \
    AcpiGbl_CurrentNodeSize+= (a); \
    if (AcpiGbl_MaxConcurrentNodeCount < AcpiGbl_CurrentNodeCount) \
    { \
        AcpiGbl_MaxConcurrentNodeCount = AcpiGbl_CurrentNodeCount; \
    } \


void
AcpiCmDumpAllocationInfo (
    void);

void
AcpiCmDumpCurrentAllocations (
    UINT32                  Component,
    NATIVE_CHAR             *Module);

#endif


#endif /* _ACCOMMON_H */
