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
 * 6.3 : Scheduling services
 */

#include "acpi.h"

#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/taskqueue.h>
#include <machine/clock.h>

#define _COMPONENT	ACPI_OS_SERVICES
MODULE_NAME("SCHEDULE")

/*
 * This is a little complicated due to the fact that we need to build and then
 * free a 'struct task' for each task we enqueue.
 *
 * We use the default taskqueue_swi queue, since it really doesn't matter what
 * else we're queued along with.
 */

MALLOC_DEFINE(M_ACPITASK, "acpitask", "ACPI deferred task");

static void	AcpiOsExecuteQueue(void *arg, int pending);

struct acpi_task {
    struct task			at_task;
    OSD_EXECUTION_CALLBACK	at_function;
    void			*at_context;
};

ACPI_STATUS
AcpiOsQueueForExecution(UINT32 Priority, OSD_EXECUTION_CALLBACK Function, void *Context)
{
    struct acpi_task	*at;

    FUNCTION_TRACE(__func__);

    if (Function == NULL)
	return_ACPI_STATUS(AE_BAD_PARAMETER);

    at = malloc(sizeof(*at), M_ACPITASK, M_NOWAIT);	/* Interrupt Context */
    if (at == NULL)
	return_ACPI_STATUS(AE_NO_MEMORY);
    bzero(at, sizeof(*at));

    at->at_function = Function;
    at->at_context = Context;
    at->at_task.ta_func = AcpiOsExecuteQueue;
    at->at_task.ta_context = at;
    switch (Priority) {
    case OSD_PRIORITY_GPE:
	at->at_task.ta_priority = 4;
	break;
    case OSD_PRIORITY_HIGH:
	at->at_task.ta_priority = 3;
	break;
    case OSD_PRIORITY_MED:
	at->at_task.ta_priority = 2;
	break;
    case OSD_PRIORITY_LO:
	at->at_task.ta_priority = 1;
	break;
    default:
	free(at, M_ACPITASK);
	return_ACPI_STATUS(AE_BAD_PARAMETER);
    }

    taskqueue_enqueue(taskqueue_swi, (struct task *)at);
    return_ACPI_STATUS(AE_OK);
}

static void
AcpiOsExecuteQueue(void *arg, int pending)
{
    struct acpi_task		*at = (struct acpi_task *)arg;
    OSD_EXECUTION_CALLBACK	Function;
    void			*Context;

    FUNCTION_TRACE(__func__);

    Function = (OSD_EXECUTION_CALLBACK)at->at_function;
    Context = at->at_context;

    free(at, M_ACPITASK);

    Function(Context);
    return_VOID;
}

/*
 * We don't have any sleep granularity better than hz, so
 * make do with that.
 */
void
AcpiOsSleep (UINT32 Seconds, UINT32 Milliseconds)
{
    int		timo;
    static int	dummy;

    FUNCTION_TRACE(__func__);

    timo = (Seconds * hz) + Milliseconds * hz / 1000;
    if (timo == 0)
	timo = 1;
    tsleep(&dummy, 0, "acpislp", timo);
    return_VOID;
}

void
AcpiOsSleepUsec (UINT32 Microseconds)
{
    FUNCTION_TRACE(__func__);

    if (Microseconds > 1000) {	/* long enough to be worth the overhead of sleeping */
	AcpiOsSleep(0, Microseconds / 1000);
    } else {
	DELAY(Microseconds);
    }
    return_VOID;
}

UINT32
AcpiOsGetThreadId (void)
{
    /* XXX do not add FUNCTION_TRACE here, results in recursive call */

    KASSERT(curproc != NULL, (__func__ ": curproc is NULL!"));
    return(curproc->p_pid + 1);	/* can't return 0 */
}
