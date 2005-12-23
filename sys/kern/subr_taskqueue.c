/*-
 * Copyright (c) 2000 Doug Rabson
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <sys/unistd.h>

static MALLOC_DEFINE(M_TASKQUEUE, "taskqueue", "Task Queues");
static void	*taskqueue_giant_ih;
static void	*taskqueue_ih;
static STAILQ_HEAD(taskqueue_list, taskqueue) taskqueue_queues;
static struct mtx taskqueue_queues_mutex;

struct taskqueue {
	STAILQ_ENTRY(taskqueue)	tq_link;
	STAILQ_HEAD(, task)	tq_queue;
	const char		*tq_name;
	taskqueue_enqueue_fn	tq_enqueue;
	void			*tq_context;
	struct task		*tq_running;
	struct mtx		tq_mutex;
	struct proc		**tq_pproc;
};

static void	init_taskqueue_list(void *data);

static void
init_taskqueue_list(void *data __unused)
{

	mtx_init(&taskqueue_queues_mutex, "taskqueue list", NULL, MTX_DEF);
	STAILQ_INIT(&taskqueue_queues);
}
SYSINIT(taskqueue_list, SI_SUB_INTRINSIC, SI_ORDER_ANY, init_taskqueue_list,
    NULL);

struct taskqueue *
taskqueue_create(const char *name, int mflags,
		 taskqueue_enqueue_fn enqueue, void *context,
		 struct proc **pp)
{
	struct taskqueue *queue;

	queue = malloc(sizeof(struct taskqueue), M_TASKQUEUE, mflags | M_ZERO);
	if (!queue)
		return 0;

	STAILQ_INIT(&queue->tq_queue);
	queue->tq_name = name;
	queue->tq_enqueue = enqueue;
	queue->tq_context = context;
	queue->tq_pproc = pp;
	mtx_init(&queue->tq_mutex, "taskqueue", NULL, MTX_DEF);

	mtx_lock(&taskqueue_queues_mutex);
	STAILQ_INSERT_TAIL(&taskqueue_queues, queue, tq_link);
	mtx_unlock(&taskqueue_queues_mutex);

	return queue;
}

/*
 * Signal a taskqueue thread to terminate.
 */
static void
taskqueue_terminate(struct proc **pp, struct taskqueue *tq)
{
	struct proc *p;

	p = *pp;
	*pp = NULL;
	if (p) {
		wakeup_one(tq);
		PROC_LOCK(p);		   /* NB: insure we don't miss wakeup */
		mtx_unlock(&tq->tq_mutex); /* let taskqueue thread run */
		msleep(p, &p->p_mtx, PWAIT, "taskqueue_destroy", 0);
		PROC_UNLOCK(p);
		mtx_lock(&tq->tq_mutex);
	}
}

void
taskqueue_free(struct taskqueue *queue)
{

	mtx_lock(&taskqueue_queues_mutex);
	STAILQ_REMOVE(&taskqueue_queues, queue, taskqueue, tq_link);
	mtx_unlock(&taskqueue_queues_mutex);

	mtx_lock(&queue->tq_mutex);
	taskqueue_run(queue);
	taskqueue_terminate(queue->tq_pproc, queue);
	mtx_destroy(&queue->tq_mutex);
	free(queue, M_TASKQUEUE);
}

/*
 * Returns with the taskqueue locked.
 */
struct taskqueue *
taskqueue_find(const char *name)
{
	struct taskqueue *queue;

	mtx_lock(&taskqueue_queues_mutex);
	STAILQ_FOREACH(queue, &taskqueue_queues, tq_link) {
		if (strcmp(queue->tq_name, name) == 0) {
			mtx_lock(&queue->tq_mutex);
			mtx_unlock(&taskqueue_queues_mutex);
			return queue;
		}
	}
	mtx_unlock(&taskqueue_queues_mutex);
	return NULL;
}

int
taskqueue_enqueue(struct taskqueue *queue, struct task *task)
{
	struct task *ins;
	struct task *prev;

	mtx_lock(&queue->tq_mutex);

	/*
	 * Count multiple enqueues.
	 */
	if (task->ta_pending) {
		task->ta_pending++;
		mtx_unlock(&queue->tq_mutex);
		return 0;
	}

	/*
	 * Optimise the case when all tasks have the same priority.
	 */
	prev = STAILQ_LAST(&queue->tq_queue, task, ta_link);
	if (!prev || prev->ta_priority >= task->ta_priority) {
		STAILQ_INSERT_TAIL(&queue->tq_queue, task, ta_link);
	} else {
		prev = 0;
		for (ins = STAILQ_FIRST(&queue->tq_queue); ins;
		     prev = ins, ins = STAILQ_NEXT(ins, ta_link))
			if (ins->ta_priority < task->ta_priority)
				break;

		if (prev)
			STAILQ_INSERT_AFTER(&queue->tq_queue, prev, task, ta_link);
		else
			STAILQ_INSERT_HEAD(&queue->tq_queue, task, ta_link);
	}

	task->ta_pending = 1;
	queue->tq_enqueue(queue->tq_context);

	mtx_unlock(&queue->tq_mutex);

	return 0;
}

void
taskqueue_run(struct taskqueue *queue)
{
	struct task *task;
	int owned, pending;

	owned = mtx_owned(&queue->tq_mutex);
	if (!owned)
		mtx_lock(&queue->tq_mutex);
	while (STAILQ_FIRST(&queue->tq_queue)) {
		/*
		 * Carefully remove the first task from the queue and
		 * zero its pending count.
		 */
		task = STAILQ_FIRST(&queue->tq_queue);
		STAILQ_REMOVE_HEAD(&queue->tq_queue, ta_link);
		pending = task->ta_pending;
		task->ta_pending = 0;
		queue->tq_running = task;
		mtx_unlock(&queue->tq_mutex);

		task->ta_func(task->ta_context, pending);

		mtx_lock(&queue->tq_mutex);
		queue->tq_running = NULL;
		wakeup(task);
	}

	/*
	 * For compatibility, unlock on return if the queue was not locked
	 * on entry, although this opens a race window.
	 */
	if (!owned)
		mtx_unlock(&queue->tq_mutex);
}

void
taskqueue_drain(struct taskqueue *queue, struct task *task)
{
	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL, "taskqueue_drain");

	mtx_lock(&queue->tq_mutex);
	while (task->ta_pending != 0 || task == queue->tq_running)
		msleep(task, &queue->tq_mutex, PWAIT, "-", 0);
	mtx_unlock(&queue->tq_mutex);
}

static void
taskqueue_swi_enqueue(void *context)
{
	swi_sched(taskqueue_ih, 0);
}

static void
taskqueue_swi_run(void *dummy)
{
	taskqueue_run(taskqueue_swi);
}

static void
taskqueue_swi_giant_enqueue(void *context)
{
	swi_sched(taskqueue_giant_ih, 0);
}

static void
taskqueue_swi_giant_run(void *dummy)
{
	taskqueue_run(taskqueue_swi_giant);
}

void
taskqueue_thread_loop(void *arg)
{
	struct taskqueue **tqp, *tq;

	tqp = arg;
	tq = *tqp;
	mtx_lock(&tq->tq_mutex);
	do {
		taskqueue_run(tq);
		msleep(tq, &tq->tq_mutex, PWAIT, "-", 0); 
	} while (*tq->tq_pproc != NULL);

	/* rendezvous with thread that asked us to terminate */
	wakeup_one(tq);
	mtx_unlock(&tq->tq_mutex);
	kthread_exit(0);
}

void
taskqueue_thread_enqueue(void *context)
{
	struct taskqueue **tqp, *tq;

	tqp = context;
	tq = *tqp;

	mtx_assert(&tq->tq_mutex, MA_OWNED);
	wakeup_one(tq);
}

TASKQUEUE_DEFINE(swi, taskqueue_swi_enqueue, 0,
		 swi_add(NULL, "task queue", taskqueue_swi_run, NULL, SWI_TQ,
		     INTR_MPSAFE, &taskqueue_ih)); 

TASKQUEUE_DEFINE(swi_giant, taskqueue_swi_giant_enqueue, 0,
		 swi_add(NULL, "Giant taskq", taskqueue_swi_giant_run,
		     NULL, SWI_TQ_GIANT, 0, &taskqueue_giant_ih)); 

TASKQUEUE_DEFINE_THREAD(thread);

int
taskqueue_enqueue_fast(struct taskqueue *queue, struct task *task)
{
	struct task *ins;
	struct task *prev;

	mtx_lock_spin(&queue->tq_mutex);

	/*
	 * Count multiple enqueues.
	 */
	if (task->ta_pending) {
		task->ta_pending++;
		mtx_unlock_spin(&queue->tq_mutex);
		return 0;
	}

	/*
	 * Optimise the case when all tasks have the same priority.
	 */
	prev = STAILQ_LAST(&queue->tq_queue, task, ta_link);
	if (!prev || prev->ta_priority >= task->ta_priority) {
		STAILQ_INSERT_TAIL(&queue->tq_queue, task, ta_link);
	} else {
		prev = 0;
		for (ins = STAILQ_FIRST(&queue->tq_queue); ins;
		     prev = ins, ins = STAILQ_NEXT(ins, ta_link))
			if (ins->ta_priority < task->ta_priority)
				break;

		if (prev)
			STAILQ_INSERT_AFTER(&queue->tq_queue, prev, task, ta_link);
		else
			STAILQ_INSERT_HEAD(&queue->tq_queue, task, ta_link);
	}

	task->ta_pending = 1;
	queue->tq_enqueue(queue->tq_context);

	mtx_unlock_spin(&queue->tq_mutex);

	return 0;
}

static void
taskqueue_run_fast(struct taskqueue *queue)
{
	struct task *task;
	int pending;

	mtx_lock_spin(&queue->tq_mutex);
	while (STAILQ_FIRST(&queue->tq_queue)) {
		/*
		 * Carefully remove the first task from the queue and
		 * zero its pending count.
		 */
		task = STAILQ_FIRST(&queue->tq_queue);
		STAILQ_REMOVE_HEAD(&queue->tq_queue, ta_link);
		pending = task->ta_pending;
		task->ta_pending = 0;
		mtx_unlock_spin(&queue->tq_mutex);

		task->ta_func(task->ta_context, pending);

		mtx_lock_spin(&queue->tq_mutex);
	}
	mtx_unlock_spin(&queue->tq_mutex);
}

struct taskqueue *taskqueue_fast;
static void	*taskqueue_fast_ih;

static void
taskqueue_fast_schedule(void *context)
{
	swi_sched(taskqueue_fast_ih, 0);
}

static void
taskqueue_fast_run(void *dummy)
{
	taskqueue_run_fast(taskqueue_fast);
}

static void
taskqueue_define_fast(void *arg)
{

	taskqueue_fast = malloc(sizeof(struct taskqueue), M_TASKQUEUE,
	    M_NOWAIT | M_ZERO);
	if (!taskqueue_fast) {
		printf("%s: Unable to allocate fast task queue!\n", __func__);
		return;
	}

	STAILQ_INIT(&taskqueue_fast->tq_queue);
	taskqueue_fast->tq_name = "fast";
	taskqueue_fast->tq_enqueue = taskqueue_fast_schedule;
	mtx_init(&taskqueue_fast->tq_mutex, "taskqueue_fast", NULL, MTX_SPIN);

	mtx_lock(&taskqueue_queues_mutex);
	STAILQ_INSERT_TAIL(&taskqueue_queues, taskqueue_fast, tq_link);
	mtx_unlock(&taskqueue_queues_mutex);

	swi_add(NULL, "Fast taskq", taskqueue_fast_run,
		NULL, SWI_TQ_FAST, INTR_MPSAFE, &taskqueue_fast_ih);
}
SYSINIT(taskqueue_fast, SI_SUB_CONFIGURE, SI_ORDER_SECOND,
    taskqueue_define_fast, NULL);
