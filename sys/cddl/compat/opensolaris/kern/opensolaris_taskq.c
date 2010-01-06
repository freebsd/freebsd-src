/*-
 * Copyright (c) 2009 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/taskq.h>

#include <vm/uma.h>

static uma_zone_t taskq_zone;

struct ostask {
	struct task	ost_task;
	task_func_t	*ost_func;
	void		*ost_arg;
};

taskq_t *system_taskq = NULL;

static void
system_taskq_init(void *arg)
{

	system_taskq = (taskq_t *)taskqueue_thread;
	taskq_zone = uma_zcreate("taskq_zone", sizeof(struct ostask),
	    NULL, NULL, NULL, NULL, 0, 0);
}
SYSINIT(system_taskq_init, SI_SUB_CONFIGURE, SI_ORDER_ANY, system_taskq_init, NULL);

static void
system_taskq_fini(void *arg)
{

	uma_zdestroy(taskq_zone);
}
SYSUNINIT(system_taskq_fini, SI_SUB_CONFIGURE, SI_ORDER_ANY, system_taskq_fini, NULL);

taskq_t *
taskq_create(const char *name, int nthreads, pri_t pri, int minalloc __unused,
    int maxalloc __unused, uint_t flags)
{
	taskq_t *tq;

	if ((flags & TASKQ_THREADS_CPU_PCT) != 0) {
		/* TODO: Calculate number od threads. */
		printf("%s: TASKQ_THREADS_CPU_PCT\n", __func__);
	}

	tq = kmem_alloc(sizeof(*tq), KM_SLEEP);
	tq->tq_queue = taskqueue_create(name, M_WAITOK, taskqueue_thread_enqueue,
	    &tq->tq_queue);
	(void) taskqueue_start_threads(&tq->tq_queue, nthreads, pri, name);

	return ((taskq_t *)tq);
}

void
taskq_destroy(taskq_t *tq)
{

	taskqueue_free(tq->tq_queue);
	kmem_free(tq, sizeof(*tq));
}

int
taskq_member(taskq_t *tq, kthread_t *thread)
{

	return (taskqueue_member(tq->tq_queue, thread));
}

static void
taskq_run(void *arg, int pending __unused)
{
	struct ostask *task = arg;

	task->ost_func(task->ost_arg);

	uma_zfree(taskq_zone, task);
}

taskqid_t
taskq_dispatch(taskq_t *tq, task_func_t func, void *arg, uint_t flags)
{
	struct ostask *task;
	int mflag;

	if ((flags & (TQ_SLEEP | TQ_NOQUEUE)) == TQ_SLEEP)
		mflag = M_WAITOK;
	else
		mflag = M_NOWAIT;

	task = uma_zalloc(taskq_zone, mflag);
	if (task == NULL)
		return (0);

	task->ost_func = func;
	task->ost_arg = arg;

	TASK_INIT(&task->ost_task, 0, taskq_run, task);
	taskqueue_enqueue(tq->tq_queue, &task->ost_task);

	return ((taskqid_t)(void *)task);
}
