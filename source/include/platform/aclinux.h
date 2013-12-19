/******************************************************************************
 *
 * Name: aclinux.h - OS specific defines, etc. for Linux
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

#ifndef __ACLINUX_H__
#define __ACLINUX_H__

/* Common (in-kernel/user-space) ACPICA configuration */

#define ACPI_USE_SYSTEM_CLIBRARY
#define ACPI_USE_DO_WHILE_0
#define ACPI_MUTEX_TYPE             ACPI_BINARY_SEMAPHORE


#ifdef __KERNEL__

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/atomic.h>
#include <linux/math64.h>
#include <linux/slab.h>
#include <linux/spinlock_types.h>
#ifdef EXPORT_ACPI_INTERFACES
#include <linux/export.h>
#endif
#include <asm/acpi.h>

/* Host-dependent types and defines for in-kernel ACPICA */

#define ACPI_MACHINE_WIDTH          BITS_PER_LONG
#define ACPI_EXPORT_SYMBOL(symbol)  EXPORT_SYMBOL(symbol);
#define strtoul                     simple_strtoul

#define ACPI_CACHE_T                struct kmem_cache
#define ACPI_SPINLOCK               spinlock_t *
#define ACPI_CPU_FLAGS              unsigned long

#else /* !__KERNEL__ */

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

/* Host-dependent types and defines for user-space ACPICA */

#define ACPI_FLUSH_CPU_CACHE()
#define ACPI_CAST_PTHREAD_T(Pthread) ((ACPI_THREAD_ID) (Pthread))

#if defined(__ia64__) || defined(__x86_64__) || defined(__aarch64__)
#define ACPI_MACHINE_WIDTH          64
#define COMPILER_DEPENDENT_INT64    long
#define COMPILER_DEPENDENT_UINT64   unsigned long
#else
#define ACPI_MACHINE_WIDTH          32
#define COMPILER_DEPENDENT_INT64    long long
#define COMPILER_DEPENDENT_UINT64   unsigned long long
#define ACPI_USE_NATIVE_DIVIDE
#endif

#ifndef __cdecl
#define __cdecl
#endif

#endif /* __KERNEL__ */

/* Linux uses GCC */

#include "acgcc.h"


#ifdef __KERNEL__

/*
 * FIXME: Inclusion of actypes.h
 * Linux kernel need this before defining inline OSL interfaces as
 * actypes.h need to be included to find ACPICA type definitions.
 * Since from ACPICA's perspective, the actypes.h should be included after
 * acenv.h (aclinux.h), this leads to a inclusion mis-ordering issue.
 */
#include <acpi/actypes.h>

/*
 * Overrides for in-kernel ACPICA
 */
ACPI_STATUS __init AcpiOsInitialize (
    void);
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsInitialize

ACPI_STATUS AcpiOsTerminate (
    void);
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsTerminate

/*
 * Memory allocation/deallocation
 */

/*
 * The irqs_disabled() check is for resume from RAM.
 * Interrupts are off during resume, just like they are for boot.
 * However, boot has  (system_state != SYSTEM_RUNNING)
 * to quiet __might_sleep() in kmalloc() and resume does not.
 */
static inline void *
AcpiOsAllocate (
    ACPI_SIZE               Size)
{
    return kmalloc (Size, irqs_disabled () ? GFP_ATOMIC : GFP_KERNEL);
}
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsAllocate

/* Use native linux version of AcpiOsAllocateZeroed */

static inline void *
AcpiOsAllocateZeroed (
    ACPI_SIZE               Size)
{
    return kzalloc (Size, irqs_disabled () ? GFP_ATOMIC : GFP_KERNEL);
}
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsAllocateZeroed
#define USE_NATIVE_ALLOCATE_ZEROED

static inline void
AcpiOsFree (
    void                   *Memory)
{
    kfree (Memory);
}
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsFree

static inline void *
AcpiOsAcquireObject (
    ACPI_CACHE_T           *Cache)
{
    return kmem_cache_zalloc (Cache,
        irqs_disabled () ? GFP_ATOMIC : GFP_KERNEL);
}
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsAcquireObject

static inline ACPI_THREAD_ID
AcpiOsGetThreadId (
    void)
{
    return (ACPI_THREAD_ID) (unsigned long) current;
}
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsGetThreadId

#ifndef CONFIG_PREEMPT

/*
 * Used within ACPICA to show where it is safe to preempt execution
 * when CONFIG_PREEMPT=n
 */
#define ACPI_PREEMPTION_POINT() \
    do { \
        if (!irqs_disabled()) \
            cond_resched(); \
    } while (0)

#endif

/*
 * When lockdep is enabled, the spin_lock_init() macro stringifies it's
 * argument and uses that as a name for the lock in debugging.
 * By executing spin_lock_init() in a macro the key changes from "lock" for
 * all locks to the name of the argument of acpi_os_create_lock(), which
 * prevents lockdep from reporting false positives for ACPICA locks.
 */
#define AcpiOsCreateLock(__Handle) \
    ({ \
        spinlock_t *Lock = ACPI_ALLOCATE(sizeof(*Lock)); \
        if (Lock) { \
            *(__Handle) = Lock; \
            spin_lock_init(*(__Handle)); \
        } \
        Lock ? AE_OK : AE_NO_MEMORY; \
    })
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsCreateLock

void __iomem *
AcpiOsMapMemory (
    ACPI_PHYSICAL_ADDRESS   Where,
    ACPI_SIZE               Length);
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsMapMemory

void
AcpiOsUnmapMemory (
    void __iomem            *LogicalAddress,
    ACPI_SIZE               Size);
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsUnmapMemory

/*
 * OSL interfaces used by debugger/disassembler
 */
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsReadable
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsWritable

/*
 * OSL interfaces used by utilities
 */
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsRedirectOutput
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsGetLine
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsGetTableByName
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsGetTableByIndex
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsGetTableByAddress
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsOpenDirectory
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsGetNextFilename
#define ACPI_USE_ALTERNATE_PROTOTYPE_AcpiOsCloseDirectory

/*
 * OSL interfaces added by Linux
 */
void
EarlyAcpiOsUnmapMemory (
    void __iomem            *Virt,
    ACPI_SIZE               Size);

ACPI_STATUS
AcpiOsHotplugExecute (
    ACPI_OSD_EXEC_CALLBACK  Function,
    void                    *Context);

#endif /* __KERNEL__ */

#endif /* __ACLINUX_H__ */
