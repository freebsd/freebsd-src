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

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/taskqueue.h>
#include <machine/clock.h>

#include <sys/bus.h>

#include <dev/acpica/acpivar.h>

#define _COMPONENT	ACPI_OS_SERVICES
MODULE_NAME("SCHEDULE")

/*
 * This is a little complicated due to the fact that we need to build and then
 * free a 'struct task' for each task we enqueue.
 */

MALLOC_DEFINE(M_ACPITASK, "acpitask", "ACPI deferred task");

static void	AcpiOsExecuteQueue(void *arg, int pending);

struct acpi_task {
    struct task			at_task;
    OSD_EXECUTION_CALLBACK	at_function;
    void			*at_context;
};

struct acpi_task_queue {
    STAILQ_ENTRY(acpi_task_queue) at_q;
    struct acpi_task		*at;
};

/*
 * Private task queue definition for ACPI
 */
TASKQUEUE_DECLARE(acpi);
static void	*taskqueue_acpi_ih;

static void
taskqueue_acpi_enqueue(void *context)
{  
    swi_sched(taskqueue_acpi_ih, SWI_NOSWITCH);
}

static void
taskqueue_acpi_run(void *dummy)
{
    taskqueue_run(taskqueue_acpi);
}

TASKQUEUE_DEFINE(acpi, taskqueue_acpi_enqueue, 0,
		 swi_add(NULL, "acpitaskq", taskqueue_acpi_run, NULL,
		     SWI_TQ, 0, &taskqueue_acpi_ih));

#if defined(ACPI_MAX_THREADS) && ACPI_MAX_THREADS > 0
#define ACPI_USE_THREADS
#endif

#ifdef ACPI_USE_THREADS
STAILQ_HEAD(, acpi_task_queue) acpi_task_queue;
static struct mtx	acpi_task_mtx;

static void
acpi_task_thread(void *arg)
{
    struct acpi_task_queue	*atq;
    OSD_EXECUTION_CALLBACK	Function;
    void			*Context;

    for (;;) {
	mtx_lock(&acpi_task_mtx);
	if ((atq = STAILQ_FIRST(&acpi_task_queue)) == NULL) {
	    msleep(&acpi_task_queue, &acpi_task_mtx, PCATCH, "actask", 0);
	    mtx_unlock(&acpi_task_mtx);
	    continue;
	}

	STAILQ_REMOVE_HEAD(&acpi_task_queue, at_q);
	mtx_unlock(&acpi_task_mtx);

	Function = (OSD_EXECUTION_CALLBACK)atq->at->at_function;
	Context = atq->at->at_context;

	mtx_lock(&Giant);
	Function(Context);

	free(atq->at, M_ACPITASK);
	free(atq, M_ACPITASK);
	mtx_unlock(&Giant);
    }

    kthread_exit(0);
}

int
acpi_task_thread_init(void)
{
    int		i, err;
    struct proc	*acpi_kthread_proc;

    err = 0;
    STAILQ_INIT(&acpi_task_queue);
    mtx_init(&acpi_task_mtx, "ACPI task", MTX_DEF);

    for (i = 0; i < ACPI_MAX_THREADS; i++) {
	err = kthread_create(acpi_task_thread, NULL, &acpi_kthread_proc,
			     0, "acpi_task%d", i);
	if (err != 0) {
	    printf("%s: kthread_create failed(%d)\n", __func__, err);
	    break;
	}
    }
    return (err);
}
#endif

ACPI_STATUS
AcpiOsQueueForExecution(UINT32 Priority, OSD_EXECUTION_CALLBACK Function, void *Context)
{
    struct acpi_task	*at;
    int pri;

    FUNCTION_TRACE(__func__);

    if (Function == NULL)
	return_ACPI_STATUS(AE_BAD_PARAMETER);

    at = malloc(sizeof(*at), M_ACPITASK, M_NOWAIT);	/* Interrupt Context */
    if (at == NULL)
	return_ACPI_STATUS(AE_NO_MEMORY);
    bzero(at, sizeof(*at));

    at->at_function = Function;
    at->at_context = Context;
    switch (Priority) {
    case OSD_PRIORITY_GPE:
	pri = 4;
	break;
    case OSD_PRIORITY_HIGH:
	pri = 3;
	break;
    case OSD_PRIORITY_MED:
	pri = 2;
	break;
    case OSD_PRIORITY_LO:
	pri = 1;
	break;
    default:
	free(at, M_ACPITASK);
	return_ACPI_STATUS(AE_BAD_PARAMETER);
    }
    TASK_INIT(&at->at_task, pri, AcpiOsExecuteQueue, at);

    taskqueue_enqueue(taskqueue_acpi, (struct task *)at);
    return_ACPI_STATUS(AE_OK);
}

static void
AcpiOsExecuteQueue(void *arg, int pending)
{
    struct acpi_task		*at;
    struct acpi_task_queue	*atq;
    OSD_EXECUTION_CALLBACK	Function;
    void			*Context;

    FUNCTION_TRACE(__func__);

    at = (struct acpi_task *)arg;
    atq = NULL;
    Function = NULL;
    Context = NULL;

#ifdef ACPI_USE_THREADS
    atq = malloc(sizeof(*atq), M_ACPITASK, M_NOWAIT);
    if (atq == NULL) {
	printf("%s: no memory\n", __func__);
	return;
    }

    atq->at = at;

    mtx_lock(&acpi_task_mtx);
    STAILQ_INSERT_TAIL(&acpi_task_queue, atq, at_q);
    mtx_unlock(&acpi_task_mtx);
    wakeup_one(&acpi_task_queue);
#else
    Function = (OSD_EXECUTION_CALLBACK)at->at_function;
    Context = at->at_context;

    Function(Context);
    free(at, M_ACPITASK);
#endif

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
AcpiOsStall (UINT32 Microseconds)
{
    FUNCTION_TRACE(__func__);

    DELAY(Microseconds);
    return_VOID;
}

UINT32
AcpiOsGetThreadId (void)
{
    /* XXX do not add FUNCTION_TRACE here, results in recursive call */

    KASSERT(curproc != NULL, (__func__ ": curproc is NULL!"));
    return(curproc->p_pid + 1);	/* can't return 0 */
}
