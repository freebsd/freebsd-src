/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * 6.1 : Mutual Exclusion and Synchronisation
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/acpica/Osd/OsdSynch.c,v 1.32.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <contrib/dev/acpica/acpi.h>

#include "opt_acpi.h"
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#define _COMPONENT	ACPI_OS_SERVICES
ACPI_MODULE_NAME("SYNCH")

MALLOC_DEFINE(M_ACPISEM, "acpisem", "ACPI semaphore");

#define AS_LOCK(as)	mtx_lock(&(as)->as_mtx)
#define AS_UNLOCK(as)	mtx_unlock(&(as)->as_mtx)

/*
 * Simple counting semaphore implemented using a mutex.  (Subsequently used
 * in the OSI code to implement a mutex.  Go figure.)
 */
struct acpi_semaphore {
    struct mtx	as_mtx;
    UINT32	as_units;
    UINT32	as_maxunits;
    UINT32	as_pendings;
    UINT32	as_resetting;
    UINT32	as_timeouts;
};

/* Default number of maximum pending threads. */
#ifndef ACPI_NO_SEMAPHORES
#ifndef ACPI_SEMAPHORES_MAX_PENDING
#define ACPI_SEMAPHORES_MAX_PENDING	4
#endif

static int	acpi_semaphore_debug = 0;
TUNABLE_INT("debug.acpi_semaphore_debug", &acpi_semaphore_debug);
SYSCTL_DECL(_debug_acpi);
SYSCTL_INT(_debug_acpi, OID_AUTO, semaphore_debug, CTLFLAG_RW,
	   &acpi_semaphore_debug, 0, "Enable ACPI semaphore debug messages");
#endif /* !ACPI_NO_SEMAPHORES */

ACPI_STATUS
AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits,
    ACPI_SEMAPHORE *OutHandle)
{
#ifndef ACPI_NO_SEMAPHORES
    struct acpi_semaphore	*as;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    if (OutHandle == NULL)
	return_ACPI_STATUS (AE_BAD_PARAMETER);
    if (InitialUnits > MaxUnits)
	return_ACPI_STATUS (AE_BAD_PARAMETER);

    if ((as = malloc(sizeof(*as), M_ACPISEM, M_NOWAIT | M_ZERO)) == NULL)
	return_ACPI_STATUS (AE_NO_MEMORY);

    mtx_init(&as->as_mtx, "ACPI semaphore", NULL, MTX_DEF);
    as->as_units = InitialUnits;
    as->as_maxunits = MaxUnits;
    as->as_pendings = as->as_resetting = as->as_timeouts = 0;

    ACPI_DEBUG_PRINT((ACPI_DB_MUTEX,
	"created semaphore %p max %d, initial %d\n", 
	as, InitialUnits, MaxUnits));

    *OutHandle = (ACPI_HANDLE)as;
#else
    *OutHandle = (ACPI_HANDLE)OutHandle;
#endif /* !ACPI_NO_SEMAPHORES */

    return_ACPI_STATUS (AE_OK);
}

ACPI_STATUS
AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle)
{
#ifndef ACPI_NO_SEMAPHORES
    struct acpi_semaphore *as = (struct acpi_semaphore *)Handle;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    ACPI_DEBUG_PRINT((ACPI_DB_MUTEX, "destroyed semaphore %p\n", as));
    mtx_destroy(&as->as_mtx);
    free(Handle, M_ACPISEM);
#endif /* !ACPI_NO_SEMAPHORES */

    return_ACPI_STATUS (AE_OK);
}

/*
 * This implementation has a bug, in that it has to stall for the entire
 * timeout before it will return AE_TIME.  A better implementation would
 * use getmicrotime() to correctly adjust the timeout after being woken up.
 */
ACPI_STATUS
AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout)
{
#ifndef ACPI_NO_SEMAPHORES
    ACPI_STATUS			result;
    struct acpi_semaphore	*as = (struct acpi_semaphore *)Handle;
    int				rv, tmo;
    struct timeval		timeouttv, currenttv, timelefttv;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    if (as == NULL)
	return_ACPI_STATUS (AE_BAD_PARAMETER);

    if (cold)
	return_ACPI_STATUS (AE_OK);

#if 0
    if (as->as_units < Units && as->as_timeouts > 10) {
	printf("%s: semaphore %p too many timeouts, resetting\n", __func__, as);
	AS_LOCK(as);
	as->as_units = as->as_maxunits;
	if (as->as_pendings)
	    as->as_resetting = 1;
	as->as_timeouts = 0;
	wakeup(as);
	AS_UNLOCK(as);
	return_ACPI_STATUS (AE_TIME);
    }

    if (as->as_resetting)
	return_ACPI_STATUS (AE_TIME);
#endif

    /* a timeout of ACPI_WAIT_FOREVER means "forever" */
    if (Timeout == ACPI_WAIT_FOREVER) {
	tmo = 0;
	timeouttv.tv_sec = ((0xffff/1000) + 1);	/* cf. ACPI spec */
	timeouttv.tv_usec = 0;
    } else {
	/* compute timeout using microseconds per tick */
	tmo = (Timeout * 1000) / (1000000 / hz);
	if (tmo <= 0)
	    tmo = 1;
	timeouttv.tv_sec  = Timeout / 1000;
	timeouttv.tv_usec = (Timeout % 1000) * 1000;
    }

    /* calculate timeout value in timeval */
    getmicrotime(&currenttv);
    timevaladd(&timeouttv, &currenttv);

    AS_LOCK(as);
    ACPI_DEBUG_PRINT((ACPI_DB_MUTEX,
	"get %d units from semaphore %p (has %d), timeout %d\n",
	Units, as, as->as_units, Timeout));
    for (;;) {
	if (as->as_maxunits == ACPI_NO_UNIT_LIMIT) {
	    result = AE_OK;
	    break;
	}
	if (as->as_units >= Units) {
	    as->as_units -= Units;
	    result = AE_OK;
	    break;
	}

	/* limit number of pending threads */
	if (as->as_pendings >= ACPI_SEMAPHORES_MAX_PENDING) {
	    result = AE_TIME;
	    break;
	}

	/* if timeout values of zero is specified, return immediately */
	if (Timeout == 0) {
	    result = AE_TIME;
	    break;
	}

	ACPI_DEBUG_PRINT((ACPI_DB_MUTEX,
	    "semaphore blocked, calling msleep(%p, %p, %d, \"acsem\", %d)\n",
	    as, &as->as_mtx, PCATCH, tmo));

	as->as_pendings++;

	if (acpi_semaphore_debug) {
	    printf("%s: Sleep %d, pending %d, semaphore %p, thread %d\n",
		__func__, Timeout, as->as_pendings, as, AcpiOsGetThreadId());
	}

	rv = msleep(as, &as->as_mtx, PCATCH, "acsem", tmo);

	as->as_pendings--;

#if 0
	if (as->as_resetting) {
	    /* semaphore reset, return immediately */
	    if (as->as_pendings == 0) {
		as->as_resetting = 0;
	    }
	    result = AE_TIME;
	    break;
	}
#endif

	ACPI_DEBUG_PRINT((ACPI_DB_MUTEX, "msleep(%d) returned %d\n", tmo, rv));
	if (rv == EWOULDBLOCK) {
	    result = AE_TIME;
	    break;
	}

	/* check if we already awaited enough */
	timelefttv = timeouttv;
	getmicrotime(&currenttv);
	timevalsub(&timelefttv, &currenttv);
	if (timelefttv.tv_sec < 0) {
	    ACPI_DEBUG_PRINT((ACPI_DB_MUTEX, "await semaphore %p timeout\n",
		as));
	    result = AE_TIME;
	    break;
	}

	/* adjust timeout for the next sleep */
	tmo = (timelefttv.tv_sec * 1000000 + timelefttv.tv_usec) /
	    (1000000 / hz);
	if (tmo <= 0)
	    tmo = 1;

	if (acpi_semaphore_debug) {
	    printf("%s: Wakeup timeleft(%jd, %lu), tmo %u, sem %p, thread %d\n",
		__func__, (intmax_t)timelefttv.tv_sec, timelefttv.tv_usec, tmo, as,
		AcpiOsGetThreadId());
	}
    }

    if (acpi_semaphore_debug) {
	if (result == AE_TIME && Timeout > 0) {
	    printf("%s: Timeout %d, pending %d, semaphore %p\n",
		__func__, Timeout, as->as_pendings, as);
	}
	if (result == AE_OK && (as->as_timeouts > 0 || as->as_pendings > 0)) {
	    printf("%s: Acquire %d, units %d, pending %d, sem %p, thread %d\n",
		__func__, Units, as->as_units, as->as_pendings, as,
		AcpiOsGetThreadId());
	}
    }

    if (result == AE_TIME)
	as->as_timeouts++;
    else
	as->as_timeouts = 0;

    AS_UNLOCK(as);
    return_ACPI_STATUS (result);
#else
    return_ACPI_STATUS (AE_OK);
#endif /* !ACPI_NO_SEMAPHORES */
}

ACPI_STATUS
AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units)
{
#ifndef ACPI_NO_SEMAPHORES
    struct acpi_semaphore	*as = (struct acpi_semaphore *)Handle;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    if (as == NULL)
	return_ACPI_STATUS(AE_BAD_PARAMETER);

    AS_LOCK(as);
    ACPI_DEBUG_PRINT((ACPI_DB_MUTEX,
	"return %d units to semaphore %p (has %d)\n",
	Units, as, as->as_units));
    if (as->as_maxunits != ACPI_NO_UNIT_LIMIT) {
	as->as_units += Units;
	if (as->as_units > as->as_maxunits)
	    as->as_units = as->as_maxunits;
    }

    if (acpi_semaphore_debug && (as->as_timeouts > 0 || as->as_pendings > 0)) {
	printf("%s: Release %d, units %d, pending %d, semaphore %p, thread %d\n",
	    __func__, Units, as->as_units, as->as_pendings, as, AcpiOsGetThreadId());
    }

    wakeup(as);
    AS_UNLOCK(as);
#endif /* !ACPI_NO_SEMAPHORES */

    return_ACPI_STATUS (AE_OK);
}

/* Combined mutex + mutex name storage since the latter must persist. */
struct acpi_spinlock {
    struct mtx	lock;
    char	name[32];
};

ACPI_STATUS
AcpiOsCreateLock (ACPI_SPINLOCK *OutHandle)
{
    struct acpi_spinlock *h;

    if (OutHandle == NULL)
	return (AE_BAD_PARAMETER);
    h = malloc(sizeof(*h), M_ACPISEM, M_NOWAIT | M_ZERO);
    if (h == NULL)
	return (AE_NO_MEMORY);

    /* Build a unique name based on the address of the handle. */
    if (OutHandle == &AcpiGbl_GpeLock)
	snprintf(h->name, sizeof(h->name), "acpi subsystem GPE lock");
    else if (OutHandle == &AcpiGbl_HardwareLock)
	snprintf(h->name, sizeof(h->name), "acpi subsystem HW lock");
    else
	snprintf(h->name, sizeof(h->name), "acpi subsys %p", OutHandle);
    mtx_init(&h->lock, h->name, NULL, MTX_DEF);
    *OutHandle = (ACPI_SPINLOCK)h;
    return (AE_OK);
}

void
AcpiOsDeleteLock (ACPI_SPINLOCK Handle)
{
    struct acpi_spinlock *h = (struct acpi_spinlock *)Handle;

    if (Handle == NULL)
        return;
    mtx_destroy(&h->lock);
    free(h, M_ACPISEM);
}

/*
 * The Flags parameter seems to state whether or not caller is an ISR
 * (and thus can't block) but since we have ithreads, we don't worry
 * about potentially blocking.
 */
ACPI_NATIVE_UINT
AcpiOsAcquireLock (ACPI_SPINLOCK Handle)
{
    struct acpi_spinlock *h = (struct acpi_spinlock *)Handle;

    if (Handle == NULL)
	return (0);
    mtx_lock(&h->lock);
    return (0);
}

void
AcpiOsReleaseLock (ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags)
{
    struct acpi_spinlock *h = (struct acpi_spinlock *)Handle;

    if (Handle == NULL)
	return;
    mtx_unlock(&h->lock);
}

/* Section 5.2.9.1:  global lock acquire/release functions */
#define GL_ACQUIRED	(-1)
#define GL_BUSY		0
#define GL_BIT_PENDING	0x1
#define GL_BIT_OWNED	0x2
#define GL_BIT_MASK	(GL_BIT_PENDING | GL_BIT_OWNED)

/*
 * Acquire the global lock.  If busy, set the pending bit.  The caller
 * will wait for notification from the BIOS that the lock is available
 * and then attempt to acquire it again.
 */
int
acpi_acquire_global_lock(uint32_t *lock)
{
	uint32_t new, old;

	do {
		old = *lock;
		new = ((old & ~GL_BIT_MASK) | GL_BIT_OWNED) |
			((old >> 1) & GL_BIT_PENDING);
	} while (atomic_cmpset_acq_int(lock, old, new) == 0);

	return ((new < GL_BIT_MASK) ? GL_ACQUIRED : GL_BUSY);
}

/*
 * Release the global lock, returning whether there is a waiter pending.
 * If the BIOS set the pending bit, OSPM must notify the BIOS when it
 * releases the lock.
 */
int
acpi_release_global_lock(uint32_t *lock)
{
	uint32_t new, old;

	do {
		old = *lock;
		new = old & ~GL_BIT_MASK;
	} while (atomic_cmpset_rel_int(lock, old, new) == 0);

	return (old & GL_BIT_PENDING);
}
