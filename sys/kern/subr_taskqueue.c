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
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/taskqueue.h>
#include <sys/kthread.h>
#include <sys/unistd.h>

static MALLOC_DEFINE(M_TASKQUEUE, "taskqueue", "Task Queues");

static STAILQ_HEAD(taskqueue_list, taskqueue) taskqueue_queues;

static void	*taskqueue_ih;
static void	*taskqueue_giant_ih;
static struct mtx taskqueue_queues_mutex;
static struct proc *taskqueue_thread_proc;

struct taskqueue {
	STAILQ_ENTRY(taskqueue)	tq_link;
	STAILQ_HEAD(, task)	tq_queue;
	const char		*tq_name;
	taskqueue_enqueue_fn	tq_enqueue;
	void			*tq_context;
	int			tq_draining;
	struct mtx		tq_mutex;
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
		 taskqueue_enqueue_fn enqueue, void *context)
{
	struct taskqueue *queue;

	queue = malloc(sizeof(struct taskqueue), M_TASKQUEUE, mflags | M_ZERO);
	if (!queue)
		return 0;

	STAILQ_INIT(&queue->tq_queue);
	queue->tq_name = name;
	queue->tq_enqueue = enqueue;
	queue->tq_context = context;
	queue->tq_draining = 0;
	mtx_init(&queue->tq_mutex, "taskqueue", NULL, MTX_DEF);

	mtx_lock(&taskqueue_queues_mutex);
	STAILQ_INSERT_TAIL(&taskqueue_queues, queue, tq_link);
	mtx_unlock(&taskqueue_queues_mutex);

	return queue;
}

void
taskqueue_free(struct taskqueue *queue)
{

	mtx_lock(&queue->tq_mutex);
	KASSERT(queue->tq_draining == 0, ("free'ing a draining taskqueue"));
	queue->tq_draining = 1;
	mtx_unlock(&queue->tq_mutex);

	taskqueue_run(queue);

	mtx_lock(&taskqueue_queues_mutex);
	STAILQ_REMOVE(&taskqueue_queues, queue, taskqueue, tq_link);
	mtx_unlock(&taskqueue_queues_mutex);

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
		mtx_lock(&queue->tq_mutex);
		if (!strcmp(queue->tq_name, name)) {
			mtx_unlock(&taskqueue_queues_mutex);
			return queue;
		}
		mtx_unlock(&queue->tq_mutex);
	}
	mtx_unlock(&taskqueue_queues_mutex);
	return 0;
}

int
taskqueue_enqueue(struct taskqueue *queue, struct task *task)
{
	struct task *ins;
	struct task *prev;

	mtx_lock(&queue->tq_mutex);

	/*
	 * Don't allow new tasks on a queue which is being freed.
	 */
	if (queue->tq_draining) {
		mtx_unlock(&queue->tq_mutex);
		return EPIPE;
	}

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
	if (queue->tq_enqueue)
		queue->tq_enqueue(queue->tq_context);

	mtx_unlock(&queue->tq_mutex);

	return 0;
}

void
taskqueue_run(struct taskqueue *queue)
{
	struct task *task;
	int pending;

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
		mtx_unlock(&queue->tq_mutex);

		task->ta_func(task->ta_context, pending);

		mtx_lock(&queue->tq_mutex);
	}
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

static void
taskqueue_kthread(void *arg)
{
	struct mtx kthread_mutex;

	bzero(&kthread_mutex, sizeof(kthread_mutex));

	mtx_init(&kthread_mutex, "taskqueue kthread", NULL, MTX_DEF);

	mtx_lock(&kthread_mutex);

	for (;;) {
		mtx_unlock(&kthread_mutex);
		taskqueue_run(taskqueue_thread);
		mtx_lock(&kthread_mutex);
		msleep(&taskqueue_thread, &kthread_mutex, PWAIT, "tqthr", 0); 
	}
}

static void
taskqueue_thread_enqueue(void *context)
{
	wakeup(&taskqueue_thread);
}

TASKQUEUE_DEFINE(swi, taskqueue_swi_enqueue, 0,
		 swi_add(NULL, "task queue", taskqueue_swi_run, NULL, SWI_TQ,
		     INTR_MPSAFE, &taskqueue_ih)); 

TASKQUEUE_DEFINE(swi_giant, taskqueue_swi_giant_enqueue, 0,
		 swi_add(NULL, "Giant task queue", taskqueue_swi_giant_run,
		     NULL, SWI_TQ_GIANT, 0, &taskqueue_giant_ih)); 

TASKQUEUE_DEFINE(thread, taskqueue_thread_enqueue, 0,
		 kthread_create(taskqueue_kthread, NULL,
		 &taskqueue_thread_proc, RFNOWAIT, 0, "taskqueue"));

int
taskqueue_enqueue_fast(struct taskqueue *queue, struct task *task)
{
	struct task *ins;
	struct task *prev;

	mtx_lock_spin(&queue->tq_mutex);

	/*
	 * Don't allow new tasks on a queue which is being freed.
	 */
	if (queue->tq_draining) {
		mtx_unlock_spin(&queue->tq_mutex);
		return EPIPE;
	}

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
	if (queue->tq_enqueue)
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
	taskqueue_fast = malloc(sizeof(struct taskqueue),
		M_TASKQUEUE, M_NOWAIT | M_ZERO);
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

	swi_add(NULL, "Fast task queue", taskqueue_fast_run,
		NULL, SWI_TQ_FAST, 0, &taskqueue_fast_ih);
}
SYSINIT(taskqueue_fast, SI_SUB_CONFIGURE, SI_ORDER_SECOND,
	taskqueue_define_fast, NULL);
