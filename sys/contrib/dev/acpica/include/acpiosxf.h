
/******************************************************************************
 *
 * Name: acpiosxf.h - All interfaces to the OS Services Layer (OSL).  These
 *                    interfaces must be implemented by OSL to interface the
 *                    ACPI components to the host operating system.
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

#ifndef __ACPIOSXF_H__
#define __ACPIOSXF_H__

#include <contrib/dev/acpica/include/platform/acenv.h>
#include <contrib/dev/acpica/include/actypes.h>


/* Types for AcpiOsExecute */

typedef enum
{
    OSL_GLOBAL_LOCK_HANDLER,
    OSL_NOTIFY_HANDLER,
    OSL_GPE_HANDLER,
    OSL_DEBUGGER_THREAD,
    OSL_EC_POLL_HANDLER,
    OSL_EC_BURST_HANDLER

} ACPI_EXECUTE_TYPE;

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
ACPI_PHYSICAL_ADDRESS
AcpiOsGetRootPointer (
    void);

ACPI_STATUS
AcpiOsPredefinedOverride (
    const ACPI_PREDEFINED_NAMES *InitVal,
    ACPI_STRING                 *NewVal);

ACPI_STATUS
AcpiOsTableOverride (
    ACPI_TABLE_HEADER       *ExistingTable,
    ACPI_TABLE_HEADER       **NewTable);


/*
 * Spinlock primitives
 */
ACPI_STATUS
AcpiOsCreateLock (
    ACPI_SPINLOCK           *OutHandle);

void
AcpiOsDeleteLock (
    ACPI_SPINLOCK           Handle);

ACPI_CPU_FLAGS
AcpiOsAcquireLock (
    ACPI_SPINLOCK           Handle);

void
AcpiOsReleaseLock (
    ACPI_SPINLOCK           Handle,
    ACPI_CPU_FLAGS          Flags);


/*
 * Semaphore primitives
 */
ACPI_STATUS
AcpiOsCreateSemaphore (
    UINT32                  MaxUnits,
    UINT32                  InitialUnits,
    ACPI_SEMAPHORE          *OutHandle);

ACPI_STATUS
AcpiOsDeleteSemaphore (
    ACPI_SEMAPHORE          Handle);

ACPI_STATUS
AcpiOsWaitSemaphore (
    ACPI_SEMAPHORE          Handle,
    UINT32                  Units,
    UINT16                  Timeout);

ACPI_STATUS
AcpiOsSignalSemaphore (
    ACPI_SEMAPHORE          Handle,
    UINT32                  Units);


/*
 * Mutex primitives. May be configured to use semaphores instead via
 * ACPI_MUTEX_TYPE (see platform/acenv.h)
 */
#if (ACPI_MUTEX_TYPE != ACPI_BINARY_SEMAPHORE)

ACPI_STATUS
AcpiOsCreateMutex (
    ACPI_MUTEX              *OutHandle);

void
AcpiOsDeleteMutex (
    ACPI_MUTEX              Handle);

ACPI_STATUS
AcpiOsAcquireMutex (
    ACPI_MUTEX              Handle,
    UINT16                  Timeout);

void
AcpiOsReleaseMutex (
    ACPI_MUTEX              Handle);
#endif


/*
 * Memory allocation and mapping
 */
void *
AcpiOsAllocate (
    ACPI_SIZE               Size);

void
AcpiOsFree (
    void *                  Memory);

void *
AcpiOsMapMemory (
    ACPI_PHYSICAL_ADDRESS   Where,
    ACPI_SIZE               Length);

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
ACPI_THREAD_ID
AcpiOsGetThreadId (
    void);

ACPI_STATUS
AcpiOsExecute (
    ACPI_EXECUTE_TYPE       Type,
    ACPI_OSD_EXEC_CALLBACK  Function,
    void                    *Context);

void
AcpiOsWaitEventsComplete (
    void                    *Context);

void
AcpiOsSleep (
    UINT64                  Milliseconds);

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
    UINT64                  *Value,
    UINT32                  Width);

ACPI_STATUS
AcpiOsWritePciConfiguration (
    ACPI_PCI_ID             *PciId,
    UINT32                  Reg,
    UINT64                  Value,
    UINT32                  Width);


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


#endif /* __ACPIOSXF_H__ */
