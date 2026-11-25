/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
 * Copyright (c) 2007-2012 Jung-uk Kim <jkim@FreeBSD.org>
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
 * 6.3 : Scheduling services
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/taskqueue.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>

#define _COMPONENT	ACPI_OS_SERVICES
ACPI_MODULE_NAME("SCHEDULE")

/*
 * Allow the user to tune the maximum number of tasks we may enqueue.
 */
static int acpi_max_tasks = ACPI_MAX_TASKS;
SYSCTL_INT(_debug_acpi, OID_AUTO, max_tasks, CTLFLAG_RDTUN, &acpi_max_tasks,
    0, "Maximum acpi tasks");

/*
 * Track and report the system's demand for task slots.
 */
static int acpi_boot_tasks_nb;
SYSCTL_INT(_debug_acpi, OID_AUTO, boot_tasks_nb, CTLFLAG_RD,
    &acpi_boot_tasks_nb, 0,
    "Amount of ACPI event tasks queued at boot before processing starts.");
static int acpi_tasks_high_water;
SYSCTL_INT(_debug_acpi, OID_AUTO, tasks_hiwater, CTLFLAG_RD,
    &acpi_tasks_high_water, 0,
    "Peak demand for ACPI event task slots (after boot).");

/*
 * Allow the user to tune the number of task threads we start.  It seems
 * some systems have problems with increased parallelism.
 */
static int acpi_max_threads = ACPI_MAX_THREADS;
SYSCTL_INT(_debug_acpi, OID_AUTO, max_threads, CTLFLAG_RDTUN, &acpi_max_threads,
    0, "Maximum acpi threads");

static MALLOC_DEFINE(M_ACPITASK, "acpitask", "ACPI deferred task");

struct acpi_task_ctx {
    struct task			at_task;
    ACPI_OSD_EXEC_CALLBACK	at_function;
    void			*at_context;
    int				at_flag;
#define	ACPI_TASK_FREE		0
#define	ACPI_TASK_USED		1
};

struct taskqueue		*acpi_taskq;
static struct acpi_task_ctx	*acpi_tasks;
static int			acpi_taskq_started;

/*
 * Preallocate memory for tasks, as tasks can be posted from interrupt contexts
 * where malloc(9) cannot be called, and initialize the taskqueue so we can
 * enqueue tasks on behalf of drivers.
 */
static void
acpi_taskq_init(void *arg)
{
    if (acpi_max_tasks < 0) {
	acpi_max_tasks = ACPI_MAX_TASKS;
	printf("ACPI: Negative number of tasks, defaulting to %d.\n",
	       acpi_max_tasks);
    }

    acpi_tasks = malloc(sizeof(*acpi_tasks) * acpi_max_tasks, M_ACPITASK,
	M_WAITOK | M_ZERO);
    acpi_taskq = taskqueue_create_fast("acpi_task", M_WAITOK,
	&taskqueue_thread_enqueue, &acpi_taskq);
}

SYSINIT(acpi_tasks, SI_SUB_DRIVERS, SI_ORDER_FIRST, acpi_taskq_init, NULL);

/*
 * Start processing of the taskqueue.
 */
static void
acpi_taskq_kick(void *arg)
{
    int boot_tasks_nb;

    if (bootverbose && (boot_tasks_nb = atomic_load_int(&acpi_boot_tasks_nb),
			boot_tasks_nb > 0))
	printf("ACPI: Unleashing pending %d tasks\n", boot_tasks_nb);

    if (acpi_max_threads < 1)
	acpi_max_threads = 1;

    taskqueue_start_threads(&acpi_taskq, acpi_max_threads, PWAIT, "acpi_task");

    atomic_store_int(&acpi_taskq_started, 1);
}

SYSINIT(acpi_taskq, SI_SUB_KICK_SCHEDULER, SI_ORDER_ANY, acpi_taskq_kick, NULL);

/*
 * Bounce through this wrapper function since ACPI-CA doesn't understand
 * the pending argument for its callbacks.
 */
static void
acpi_task_execute(void *context, int pending)
{
    struct acpi_task_ctx *at;

    at = (struct acpi_task_ctx *)context;
    at->at_function(at->at_context);
    atomic_store_int(&at->at_flag, ACPI_TASK_FREE);
}

static ACPI_STATUS
acpi_task_enqueue(int priority, ACPI_OSD_EXEC_CALLBACK Function, void *Context)
{
    const int started = atomic_load_int(&acpi_taskq_started);
    struct acpi_task_ctx *at;
    int *high_water_p;
    int high_water, i;

    at = NULL;
    for (i = 0; i < acpi_max_tasks; i++)
	if (atomic_cmpset_int(&acpi_tasks[i].at_flag, ACPI_TASK_FREE,
	    ACPI_TASK_USED)) {
	    at = &acpi_tasks[i];
	    break;
	}

    /* Update the appropriate high water mark. */
    high_water_p = started ? &acpi_tasks_high_water : &acpi_boot_tasks_nb;
    while (high_water = atomic_load_int(high_water_p), i + 1 > high_water) {
	if (atomic_cmpset_int(high_water_p, high_water, i + 1))
	    break;
	cpu_spinwait();
    }

    if (at == NULL) {
	printf("AcpiOsExecute: Failed to enqueue task! Consider increasing "
	       "the 'debug.acpi.max_tasks' tunable.\n");
	return (AE_NO_MEMORY);
    }

    TASK_INIT(&at->at_task, priority, acpi_task_execute, at);
    at->at_function = Function;
    at->at_context = Context;
    taskqueue_enqueue(acpi_taskq, &at->at_task);

    if (bootverbose && !started)
	printf("AcpiOsExecute: Task enqueued but processing not started yet\n");

    return (AE_OK);
}

/*
 * This function may be called in interrupt context, i.e. when a GPE fires.
 * We allocate and queue a task for one of our taskqueue threads to process.
 */
ACPI_STATUS
AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function,
    void *Context)
{
    ACPI_STATUS status;
    int pri;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    if (Function == NULL)
	return_ACPI_STATUS(AE_BAD_PARAMETER);

    switch (Type) {
    case OSL_GPE_HANDLER:
    case OSL_NOTIFY_HANDLER:
	/*
	 * Run GPEs and Notifies at the same priority.  This allows
	 * Notifies that are generated by running a GPE's method (e.g., _L00)
	 * to not be pre-empted by a later GPE that arrives during the
	 * Notify handler execution.
	 */
	pri = 10;
	break;
    case OSL_GLOBAL_LOCK_HANDLER:
    case OSL_EC_POLL_HANDLER:
    case OSL_EC_BURST_HANDLER:
	pri = 5;
	break;
    case OSL_DEBUGGER_MAIN_THREAD:
    case OSL_DEBUGGER_EXEC_THREAD:
	pri = 0;
	break;
    default:
	return_ACPI_STATUS(AE_BAD_PARAMETER);
    }

    status = acpi_task_enqueue(pri, Function, Context);
    return_ACPI_STATUS(status);
}

void
AcpiOsWaitEventsComplete(void)
{
    static bool warning_printed = false;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    if (atomic_load_int(&acpi_taskq_started))
	taskqueue_drain_all(acpi_taskq);
    else if (!warning_printed && atomic_load_int(&acpi_boot_tasks_nb) > 0) {
	warning_printed = true;
	printf("%s: Task processing not started, ignoring request\n", __func__);
    }

    return_VOID;
}

void
AcpiOsSleep(UINT64 Milliseconds)
{
    int		timo;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    timo = Milliseconds * hz / 1000;

    /*
     * If requested sleep time is less than our hz resolution, use
     * DELAY instead for better granularity.
     */
    if (timo > 0)
	pause("acpislp", timo);
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

    binuptime(&bt);
    t = (uint64_t)bt.sec * 10000000;
    t += ((uint64_t)10000000 * (uint32_t)(bt.frac >> 32)) >> 32;

    return (t);
}

void
AcpiOsStall(UINT32 Microseconds)
{
    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    DELAY(Microseconds);
    return_VOID;
}

ACPI_THREAD_ID
AcpiOsGetThreadId(void)
{

    /* XXX do not add ACPI_FUNCTION_TRACE here, results in recursive call. */

    /* Returning 0 is not allowed. */
    return (curthread->td_tid);
}
