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
#include <sys/malloc.h>
#include <sys/mutex.h>

MALLOC_DEFINE(M_ACPISEM, "acpisem", "ACPI semaphore");

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
    struct acpi_semaphore	*as;

    if (OutHandle == NULL)
	return(AE_BAD_PARAMETER);
    if (InitialUnits > MaxUnits)
	return(AE_BAD_PARAMETER);

    if ((as = malloc(sizeof(*as), M_ACPISEM, M_NOWAIT)) == NULL)
	return(AE_NO_MEMORY);

    mtx_init(&as->as_mtx, "ACPI semaphore", MTX_DEF);
    as->as_units = InitialUnits;
    as->as_maxunits = MaxUnits;

    *OutHandle = (ACPI_HANDLE)as;
    return(AE_OK);
}

ACPI_STATUS
AcpiOsDeleteSemaphore (ACPI_HANDLE Handle)
{
    free(Handle, M_ACPISEM);
    return(AE_OK);
}

/*
 * This implementation has a bug, in that it has to stall for the entire
 * timeout before it will return AE_TIME.  A better implementation would
 * use getmicrotime() to correctly adjust the timeout after being woken up.
 */
ACPI_STATUS
AcpiOsWaitSemaphore(ACPI_HANDLE Handle, UINT32 Units, UINT32 Timeout)
{
    struct acpi_semaphore	*as = (struct acpi_semaphore *)Handle;
    int				result;

    if (as == NULL)
	return(AE_BAD_PARAMETER);

    mtx_enter(&as->as_mtx, MTX_DEF);
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
	if (msleep(as, &as->as_mtx, 0, "acpisem", Timeout / (1000 * hz)) == EWOULDBLOCK) {
	    result = AE_TIME;
	    break;
	}
    }
    mtx_exit(&as->as_mtx, MTX_DEF);

    return(result);
}

ACPI_STATUS
AcpiOsSignalSemaphore(ACPI_HANDLE Handle, UINT32 Units)
{
    struct acpi_semaphore	*as = (struct acpi_semaphore *)Handle;

    if (as == NULL)
	return(AE_BAD_PARAMETER);

    mtx_enter(&as->as_mtx, MTX_DEF);
    as->as_units += Units;
    if (as->as_units > as->as_maxunits)
	as->as_units = as->as_maxunits;
    wakeup(as);
    mtx_exit(&as->as_mtx, MTX_DEF);

    return(AE_OK);
}
