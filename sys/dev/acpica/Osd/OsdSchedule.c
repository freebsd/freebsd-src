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

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/taskqueue.h>
#include <machine/clock.h>

#include "acpi.h"
#include <dev/acpica/acpivar.h>

#define _COMPONENT	ACPI_OS_SERVICES
ACPI_MODULE_NAME("SCHEDULE")

/*
 * Allow the user to tune the number of task threads we start.  It seems
 * some systems have problems with increased parallelism.
 */
static int acpi_max_threads = ACPI_MAX_THREADS;
TUNABLE_INT("debug.acpi.max_threads", &acpi_max_threads);

/*
 * This is a little complicated due to the fact that we need to build and then
 * free a 'struct task' for each task we enqueue.
 */

MALLOC_DEFINE(M_ACPITASK, "acpitask", "ACPI deferred task");

static void	AcpiOsExecuteQueue(void *arg, int pending);

struct acpi_task {
    struct task			at_task;
    ACPI_OSD_EXEC_CALLBACK	at_function;
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
    swi_sched(taskqueue_acpi_ih, 0);
}

static void
taskqueue_acpi_run(void *dummy)
{
    taskqueue_run(taskqueue_acpi);
}

TASKQUEUE_DEFINE(acpi, taskqueue_acpi_enqueue, 0,
		 swi_add(NULL, "acpitaskq", taskqueue_acpi_run, NULL,
		     SWI_TQ, 0, &taskqueue_acpi_ih));

static STAILQ_HEAD(, acpi_task_queue) acpi_task_queue;
ACPI_LOCK_DECL(taskq, "ACPI task queue");

static void
acpi_task_thread(void *arg)
{
    struct acpi_task_queue	*atq;
    ACPI_OSD_EXEC_CALLBACK	Function;
    void			*Context;

    ACPI_LOCK(taskq);
    for (;;) {
	while ((atq = STAILQ_FIRST(&acpi_task_queue)) == NULL)
	    msleep(&acpi_task_queue, &taskq_mutex, PCATCH, "actask", 0);
	STAILQ_REMOVE_HEAD(&acpi_task_queue, at_q);
	ACPI_UNLOCK(taskq);

	Function = (ACPI_OSD_EXEC_CALLBACK)atq->at->at_function;
	Context = atq->at->at_context;

	Function(Context);

	free(atq->at, M_ACPITASK);
	free(atq, M_ACPITASK);
	ACPI_LOCK(taskq);
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

    for (i = 0; i < acpi_max_threads; i++) {
	err = kthread_create(acpi_task_thread, NULL, &acpi_kthread_proc,
			     0, 0, "acpi_task%d", i);
	if (err != 0) {
	    printf("%s: kthread_create failed(%d)\n", __func__, err);
	    break;
	}
    }
    return (err);
}

/* This function is called in interrupt context. */
ACPI_STATUS
AcpiOsQueueForExecution(UINT32 Priority, ACPI_OSD_EXEC_CALLBACK Function,
    void *Context)
{
    struct acpi_task	*at;
    int pri;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    if (Function == NULL)
	return_ACPI_STATUS (AE_BAD_PARAMETER);

    at = malloc(sizeof(*at), M_ACPITASK, M_NOWAIT | M_ZERO);
    if (at == NULL)
	return_ACPI_STATUS (AE_NO_MEMORY);

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
	return_ACPI_STATUS (AE_BAD_PARAMETER);
    }
    TASK_INIT(&at->at_task, pri, AcpiOsExecuteQueue, at);

    taskqueue_enqueue(taskqueue_acpi, (struct task *)at);

    return_ACPI_STATUS (AE_OK);
}

static void
AcpiOsExecuteQueue(void *arg, int pending)
{
    struct acpi_task_queue	*atq;
    ACPI_OSD_EXEC_CALLBACK	Function;
    void			*Context;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    atq = NULL;
    Function = NULL;
    Context = NULL;

    atq = malloc(sizeof(*atq), M_ACPITASK, M_NOWAIT);
    if (atq == NULL) {
	printf("%s: no memory\n", __func__);
	return;
    }
    atq->at = (struct acpi_task *)arg;

    ACPI_LOCK(taskq);
    STAILQ_INSERT_TAIL(&acpi_task_queue, atq, at_q);
    wakeup_one(&acpi_task_queue);
    ACPI_UNLOCK(taskq);

    return_VOID;
}

void
AcpiOsSleep(ACPI_INTEGER Milliseconds)
{
    int		timo;
    static int	dummy;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    timo = Milliseconds * hz / 1000;

    /* 
     * If requested sleep time is less than our hz resolution, use
     * DELAY instead for better granularity.
     */
    if (timo > 0)
	tsleep(&dummy, 0, "acpislp", timo);
    else
	DELAY(Milliseconds * 1000);

    return_VOID;
}

/*
 * Return the current time in 100 nanosecond units
 */
UINT64
AcpiOsGetTimer(void)
{
    struct bintime bt;
    UINT64 t;

    /* XXX During early boot there is no (decent) timer available yet. */
    if (cold)
	panic("acpi: timer op not yet supported during boot");

    binuptime(&bt);
    t = ((UINT64)10000000 * (uint32_t)(bt.frac >> 32)) >> 32;
    t += bt.sec * 10000000;

    return (t);
}

void
AcpiOsStall(UINT32 Microseconds)
{
    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    DELAY(Microseconds);
    return_VOID;
}

UINT32
AcpiOsGetThreadId(void)
{
    struct proc *p;

    /* XXX do not add ACPI_FUNCTION_TRACE here, results in recursive call. */

    p = curproc;
    KASSERT(p != NULL, ("%s: curproc is NULL!", __func__));

    /* Returning 0 is not allowed. */
    return (p->p_pid + 1);
}
