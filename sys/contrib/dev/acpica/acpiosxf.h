
/******************************************************************************
 *
 * Name: acpiosxf.h - All interfaces to the OS Services Layer (OSL).  These
 *                    interfaces must be implemented by OSL to interface the
 *                    ACPI components to the host operating system.
 *
 *****************************************************************************/


/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2005, Intel Corp.
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
 * solely to the minimum extent necessary to exer
 se the above copyright
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

#ifndef __ACPIOSXF_H__
#define __ACPIOSXF_H__

#include <contrib/dev/acpica/acenv.h>
#include <contrib/dev/acpica/actypes.h>


/* Priorities for AcpiOsQueueForExecution */

#define OSD_PRIORITY_GPE            1
#define OSD_PRIORITY_HIGH           2
#define OSD_PRIORITY_MED            3
#define OSD_PRIORITY_LO             4

#define ACPI_NO_UNIT_LIMIT          ((UINT32) -1)
#define ACPI_MUTEX_SEM              1


/* Functions for AcpiOsSignal */

#define ACPI_SIGNAL_FATAL           0
#define ACPI_SIGNAL_BREAKPOINT      1

typedef struct acpi_signal_fatal_info
{
    UINT32                  Type;
    UINT32                  Code;
    UINT32                  Argument;

} ACPI_SIGNAL_FATAL_INFO;


/*
 * OSL Initialization and shutdown primitives
 */
ACPI_STATUS
AcpiOsInitialize (
    void);

ACPI_STATUS
AcpiOsTerminate (
    void);


/*
 * ACPI Table interfaces
 */
ACPI_STATUS
AcpiOsGetRootPointer (
    UINT32                  Flags,
    ACPI_POINTER            *Address);

ACPI_STATUS
AcpiOsPredefinedOverride (
    const ACPI_PREDEFINED_NAMES *InitVal,
    ACPI_STRING                 *NewVal);

ACPI_STATUS
AcpiOsTableOverride (
    ACPI_TABLE_HEADER       *ExistingTable,
    ACPI_TABLE_HEADER       **NewTable);


/*
 * Synchronization primitives
 */
ACPI_STATUS
AcpiOsCreateSemaphore (
    UINT32                  MaxUnits,
    UINT32                  InitialUnits,
    ACPI_HANDLE             *OutHandle);

ACPI_STATUS
AcpiOsDeleteSemaphore (
    ACPI_HANDLE             Handle);

ACPI_STATUS
AcpiOsWaitSemaphore (
    ACPI_HANDLE             Handle,
    UINT32                  Units,
    UINT16                  Timeout);

ACPI_STATUS
AcpiOsSignalSemaphore (
    ACPI_HANDLE             Handle,
    UINT32                  Units);

ACPI_STATUS
AcpiOsCreateLock (
    ACPI_HANDLE             *OutHandle);

void
AcpiOsDeleteLock (
    ACPI_HANDLE             Handle);

ACPI_NATIVE_UINT
AcpiOsAcquireLock (
    ACPI_HANDLE             Handle);

void
AcpiOsReleaseLock (
    ACPI_HANDLE             Handle,
    ACPI_NATIVE_UINT        Flags);


/*
 * Memory allocation and mapping
 */
void *
AcpiOsAllocate (
    ACPI_SIZE               Size);

void
AcpiOsFree (
    void *                  Memory);

ACPI_STATUS
AcpiOsMapMemory (
    ACPI_PHYSICAL_ADDRESS   PhysicalAddress,
    ACPI_SIZE               Size,
    void                    **LogicalAddress);

void
AcpiOsUnmapMemory (
    void                    *LogicalAddress,
    ACPI_SIZE               Size);

ACPI_STATUS
AcpiOsGetPhysicalAddress (
    void                    *LogicalAddress,
    ACPI_PHYSICAL_ADDRESS   *PhysicalAddress);


/*
 * Memory/Object Cache
 */
ACPI_STATUS
AcpiOsCreateCache (
    char                    *CacheName,
    UINT16                  ObjectSize,
    UINT16                  MaxDepth,
    ACPI_CACHE_T            **ReturnCache);

ACPI_STATUS
AcpiOsDeleteCache (
    ACPI_CACHE_T            *Cache);

ACPI_STATUS
AcpiOsPurgeCache (
    ACPI_CACHE_T            *Cache);

void *
AcpiOsAcquireObject (
    ACPI_CACHE_T            *Cache);

ACPI_STATUS
AcpiOsReleaseObject (
    ACPI_CACHE_T            *Cache,
    void                    *Object);


/*
 * Interrupt handlers
 */
ACPI_STATUS
AcpiOsInstallInterruptHandler (
    UINT32                  InterruptNumber,
    ACPI_OSD_HANDLER        ServiceRoutine,
    void                    *Context);

ACPI_STATUS
AcpiOsRemoveInterruptHandler (
    UINT32                  InterruptNumber,
    ACPI_OSD_HANDLER        ServiceRoutine);


/*
 * Threads and Scheduling
 */
UINT32
AcpiOsGetThreadId (
    void);

ACPI_STATUS
AcpiOsQueueForExecution (
    UINT32                  Priority,
    ACPI_OSD_EXEC_CALLBACK  Function,
    void                    *Context);

void
AcpiOsWaitEventsComplete (
    void                    *Context);

void
AcpiOsSleep (
    ACPI_INTEGER            Milliseconds);

void
AcpiOsStall (
    UINT32                  Microseconds);


/*
 * Platform and hardware-independent I/O interfaces
 */
ACPI_STATUS
AcpiOsReadPort (
    ACPI_IO_ADDRESS         Address,
    UINT32                  *Value,
    UINT32                  Width);

ACPI_STATUS
AcpiOsWritePort (
    ACPI_IO_ADDRESS         Address,
    UINT32                  Value,
    UINT32                  Width);


/*
 * Platform and hardware-independent physical memory interfaces
 */
ACPI_STATUS
AcpiOsReadMemory (
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  *Value,
    UINT32                  Width);

ACPI_STATUS
AcpiOsWriteMemory (
    ACPI_PHYSICAL_ADDRESS   Address,
    UINT32                  Value,
    UINT32                  Width);


/*
 * Platform and hardware-independent PCI configuration space access
 * Note: Can't use "Register" as a parameter, changed to "Reg" --
 * certain compilers complain.
 */
ACPI_STATUS
AcpiOsReadPciConfiguration (
    ACPI_PCI_ID             *PciId,
    UINT32                  Reg,
    void                    *Value,
    UINT32                  Width);

ACPI_STATUS
AcpiOsWritePciConfiguration (
    ACPI_PCI_ID             *PciId,
    UINT32                  Reg,
    ACPI_INTEGER            Value,
    UINT32                  Width);

/*
 * Interim function needed for PCI IRQ routing
 */
void
AcpiOsDerivePciId(
    ACPI_HANDLE             Rhandle,
    ACPI_HANDLE             Chandle,
    ACPI_PCI_ID             **PciId);

/*
 * Miscellaneous
 */
BOOLEAN
AcpiOsReadable (
    void                    *Pointer,
    ACPI_SIZE               Length);

BOOLEAN
AcpiOsWritable (
    void                    *Pointer,
    ACPI_SIZE               Length);

UINT64
AcpiOsGetTimer (
    void);

ACPI_STATUS
AcpiOsSignal (
    UINT32                  Function,
    void                    *Info);

/*
 * Debug print routines
 */
void ACPI_INTERNAL_VAR_XFACE
AcpiOsPrintf (
    const char              *Format,
    ...);

void
AcpiOsVprintf (
    const char              *Format,
    va_list                 Args);

void
AcpiOsRedirectOutput (
    void                    *Destination);


/*
 * Debug input
 */
UINT32
AcpiOsGetLine (
    char                    *Buffer);


/*
 * Directory manipulation
 */
void *
AcpiOsOpenDirectory (
    char                    *Pathname,
    char                    *WildcardSpec,
    char                    RequestedFileType);

/* RequesteFileType values */

#define REQUEST_FILE_ONLY                   0
#define REQUEST_DIR_ONLY                    1


char *
AcpiOsGetNextFilename (
    void                    *DirHandle);

void
AcpiOsCloseDirectory (
    void                    *DirHandle);

/*
 * Debug
 */
void
AcpiOsDbgAssert(
    void                    *FailedAssertion,
    void                    *FileName,
    UINT32                  LineNumber,
    char                    *Message);

#endif /* __ACPIOSXF_H__ */
