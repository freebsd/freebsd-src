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
 *
 *	$FreeBSD$
 */

/*
 * 6.1 : Mutual Exclusion and Synchronisation
 */

#include "acpi.h"

#include "opt_acpi.h"
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#if __FreeBSD_version >= 500000
#include <sys/lock.h>
#include <sys/mutex.h>
#endif

#define _COMPONENT	ACPI_OS_SERVICES
ACPI_MODULE_NAME("SYNCH")

static MALLOC_DEFINE(M_ACPISEM, "acpisem", "ACPI semaphore");

#if __FreeBSD_version < 500000
# define AS_LOCK(as)		s = splhigh()
# define AS_UNLOCK(as)		splx(s)
# define AS_LOCK_DECL		int s
# define msleep(a, b, c, d, e)	tsleep(a, c, d, e)
#else
# define AS_LOCK(as)		mtx_lock(&(as)->as_mtx)
# define AS_UNLOCK(as)		mtx_unlock(&(as)->as_mtx)
# define AS_LOCK_DECL
#endif

/*
 * Simple counting semaphore implemented using a mutex. (Subsequently used
 * in the OSI code to implement a mutex.  Go figure.)
 */
struct acpi_semaphore {
#if __FreeBSD_version >= 500000
    struct mtx	as_mtx;
#endif
    UINT32	as_units;
    UINT32	as_maxunits;
    UINT32	as_pendings;
    UINT32	as_resetting;
    UINT32	as_timeouts;
};

#ifndef ACPI_NO_SEMAPHORES
#ifndef ACPI_SEMAPHORES_MAX_PENDING
#define ACPI_SEMAPHORES_MAX_PENDING	4
#endif
static int	acpi_semaphore_debug = 0;
TUNABLE_INT("debug.acpi_semaphore_debug", &acpi_semaphore_debug);
SYSCTL_INT(_debug, OID_AUTO, acpi_semaphore_debug, CTLFLAG_RW,
    &acpi_semaphore_debug, 0, "");
#endif

ACPI_STATUS
AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_HANDLE *OutHandle)
{
#ifndef ACPI_NO_SEMAPHORES
    struct acpi_semaphore	*as;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    if (OutHandle == NULL)
	return(AE_BAD_PARAMETER);
    if (InitialUnits > MaxUnits)
	return_ACPI_STATUS(AE_BAD_PARAMETER);

    if ((as = malloc(sizeof(*as), M_ACPISEM, M_NOWAIT)) == NULL)
	return_ACPI_STATUS(AE_NO_MEMORY);

    bzero(as, sizeof(*as));
#if __FreeBSD_version >= 500000
    mtx_init(&as->as_mtx, "ACPI semaphore", NULL, MTX_DEF);
#endif
    as->as_units = InitialUnits;
    as->as_maxunits = MaxUnits;
    as->as_pendings = as->as_resetting = as->as_timeouts = 0;

    ACPI_DEBUG_PRINT((ACPI_DB_MUTEX,
	"created semaphore %p max %d, initial %d\n", 
	as, InitialUnits, MaxUnits));

    *OutHandle = (ACPI_HANDLE)as;
    return_ACPI_STATUS(AE_OK);
#else
    *OutHandle = (ACPI_HANDLE)OutHandle;
    return(AE_OK);
#endif
}

ACPI_STATUS
AcpiOsDeleteSemaphore (ACPI_HANDLE Handle)
{
#ifndef ACPI_NO_SEMAPHORES
    struct acpi_semaphore *as = (struct acpi_semaphore *)Handle;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    ACPI_DEBUG_PRINT((ACPI_DB_MUTEX, "destroyed semaphore %p\n", as));
#if __FreeBSD_version >= 500000
    mtx_destroy(&as->as_mtx);
#endif
    free(Handle, M_ACPISEM);
    return_ACPI_STATUS(AE_OK);
#else
    return(AE_OK);
#endif
}

/*
 * This implementation has a bug, in that it has to stall for the entire
 * timeout before it will return AE_TIME.  A better implementation would
 * use getmicrotime() to correctly adjust the timeout after being woken up.
 */
ACPI_STATUS
AcpiOsWaitSemaphore(ACPI_HANDLE Handle, UINT32 Units, UINT32 Timeout)
{
#ifndef ACPI_NO_SEMAPHORES
    struct acpi_semaphore	*as = (struct acpi_semaphore *)Handle;
    ACPI_STATUS			result;
    int				rv, tmo;
    struct timeval		timeouttv, currenttv, timelefttv;
    AS_LOCK_DECL;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    if (as == NULL)
	return_ACPI_STATUS(AE_BAD_PARAMETER);

    if (cold)
	return_ACPI_STATUS(AE_OK);

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
	return_ACPI_STATUS(AE_TIME);
    }

    if (as->as_resetting) {
	return_ACPI_STATUS(AE_TIME);
    }
#endif

    /* a timeout of WAIT_FOREVER means "forever" */
    if (Timeout == WAIT_FOREVER) {
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

	/* limit number of pending treads */
	if (as->as_pendings >= ACPI_SEMAPHORES_MAX_PENDING) {
	    result = AE_TIME;
	    break;
	}

	/* if timeout values of zero is specified, return immediately */
	if (Timeout == 0) {
	    result = AE_TIME;
	    break;
	}

#if __FreeBSD_version >= 500000
	ACPI_DEBUG_PRINT((ACPI_DB_MUTEX,
	    "semaphore blocked, calling msleep(%p, %p, %d, \"acsem\", %d)\n",
	    as, &as->as_mtx, PCATCH, tmo));
#endif

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
	    ACPI_DEBUG_PRINT((ACPI_DB_MUTEX, "await semaphore %p timeout\n", as));
	    result = AE_TIME;
	    break;
	}

	/* adjust timeout for the next sleep */
	tmo = (timelefttv.tv_sec * 1000000 + timelefttv.tv_usec) / (1000000 / hz);
	if (tmo <= 0)
	    tmo = 1;

	if (acpi_semaphore_debug) {
	    printf("%s: Wakeup timeleft(%lu, %lu), tmo %u, semaphore %p, thread %d\n",
		__func__, timelefttv.tv_sec, timelefttv.tv_usec, tmo, as, AcpiOsGetThreadId());
	}
    }

    if (acpi_semaphore_debug) {
	if (result == AE_TIME && Timeout > 0) {
	    printf("%s: Timeout %d, pending %d, semaphore %p\n",
		__func__, Timeout, as->as_pendings, as);
	}
	if (result == AE_OK && (as->as_timeouts > 0 || as->as_pendings > 0)) {
	    printf("%s: Acquire %d, units %d, pending %d, semaphore %p, thread %d\n",
		__func__, Units, as->as_units, as->as_pendings, as, AcpiOsGetThreadId());
	}
    }

    if (result == AE_TIME) {
	as->as_timeouts++;
    } else {
	as->as_timeouts = 0;
    }

    AS_UNLOCK(as);

    return_ACPI_STATUS(result);
#else
    return(AE_OK);
#endif
}

ACPI_STATUS
AcpiOsSignalSemaphore(ACPI_HANDLE Handle, UINT32 Units)
{
#ifndef ACPI_NO_SEMAPHORES
    struct acpi_semaphore	*as = (struct acpi_semaphore *)Handle;
    AS_LOCK_DECL;

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
    return_ACPI_STATUS(AE_OK);
#else
    return(AE_OK);
#endif
}
