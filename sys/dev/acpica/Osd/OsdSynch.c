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

#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>

#define _COMPONENT	ACPI_OS_SERVICES
MODULE_NAME("SYNCH")

static MALLOC_DEFINE(M_ACPISEM, "acpisem", "ACPI semaphore");

/* disable semaphores - AML in the field doesn't use them correctly */
#define ACPI_NO_SEMAPHORES

/*
 * Simple counting semaphore implemented using a mutex. (Subsequently used
 * in the OSI code to implement a mutex.  Go figure.)
 */
struct acpi_semaphore {
    struct mtx	as_mtx;
    UINT32	as_units;
    UINT32	as_maxunits;
};

ACPI_STATUS
AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_HANDLE *OutHandle)
{
#ifndef ACPI_NO_SEMAPHORES
    struct acpi_semaphore	*as;

    FUNCTION_TRACE(__func__);

    if (OutHandle == NULL)
	return(AE_BAD_PARAMETER);
    if (InitialUnits > MaxUnits)
	return_ACPI_STATUS(AE_BAD_PARAMETER);

    if ((as = malloc(sizeof(*as), M_ACPISEM, M_NOWAIT)) == NULL)
	return_ACPI_STATUS(AE_NO_MEMORY);

    mtx_init(&as->as_mtx, "ACPI semaphore", MTX_DEF);
    as->as_units = InitialUnits;
    as->as_maxunits = MaxUnits;

    DEBUG_PRINT(TRACE_MUTEX, ("created semaphore %p max %d, initial %d\n", 
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

    FUNCTION_TRACE(__func__);

    DEBUG_PRINT(TRACE_MUTEX, ("destroyed semaphore %p\n", as));
    mtx_destroy(&as->as_mtx);
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

    FUNCTION_TRACE(__func__);

    if (as == NULL)
	return_ACPI_STATUS(AE_BAD_PARAMETER);

    /* a timeout of -1 means "forever" */
    if (Timeout == -1) {
	tmo = 0;
    } else {
	/* compute timeout using microseconds per tick */
	tmo = (Timeout * 1000) / (1000000 / hz);
	if (tmo <= 0)
	    tmo = 1;
    }

    mtx_lock(&as->as_mtx);
    DEBUG_PRINT(TRACE_MUTEX, ("get %d units from semaphore %p (has %d), timeout %d\n",
			      Units, as, as->as_units, Timeout));
    for (;;) {
	if (as->as_units >= Units) {
	    as->as_units -= Units;
	    result = AE_OK;
	    break;
	}
	if (Timeout < 0) {
	    result = AE_TIME;
	    break;
	}
	DEBUG_PRINT(TRACE_MUTEX, ("semaphore blocked, calling msleep(%p, %p, %d, \"acpisem\", %d)\n",
				  as, as->as_mtx, 0, tmo));
	
	rv = msleep(as, &as->as_mtx, 0, "acpisem", tmo);
	DEBUG_PRINT(TRACE_MUTEX, ("msleep returned %d\n", rv));
	if (rv == EWOULDBLOCK) {
	    result = AE_TIME;
	    break;
	}
    }
    mtx_unlock(&as->as_mtx);

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

    FUNCTION_TRACE(__func__);

    if (as == NULL)
	return_ACPI_STATUS(AE_BAD_PARAMETER);

    mtx_lock(&as->as_mtx);
    DEBUG_PRINT(TRACE_MUTEX, ("return %d units to semaphore %p (has %d)\n",
			      Units, as, as->as_units));
    as->as_units += Units;
    if (as->as_units > as->as_maxunits)
	as->as_units = as->as_maxunits;
    wakeup(as);
    mtx_unlock(&as->as_mtx);
    return_ACPI_STATUS(AE_OK);
#else
    return(AE_OK);
#endif
}
