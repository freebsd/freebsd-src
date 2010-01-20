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
#include <sys/sched.h>
#include <sys/taskqueue.h>
#include <sys/unistd.h>
#include <machine/stdarg.h>

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
	int			tq_pcount;
	int			tq_spin;
	int			tq_flags;
};

#define	TQ_FLAGS_ACTIVE		(1 << 0)
#define	TQ_FLAGS_BLOCKED	(1 << 1)
#define	TQ_FLAGS_PENDING	(1 << 2)

static __inline void
TQ_LOCK(struct taskqueue *tq)
{
	if (tq->tq_spin)
		mtx_lock_spin(&tq->tq_mutex);
	else
		mtx_lock(&tq->tq_mutex);
}

static __inline void
TQ_UNLOCK(struct taskqueue *tq)
{
	if (tq->tq_spin)
		mtx_unlock_spin(&tq->tq_mutex);
	else
		mtx_unlock(&tq->tq_mutex);
}

static void	init_taskqueue_list(void *data);

static __inline int
TQ_SLEEP(struct taskqueue *tq, void *p, struct mtx *m, int pri, const char *wm,
    int t)
{
	if (tq->tq_spin)
		return (msleep_spin(p, m, wm, t));
	return (msleep(p, m, pri, wm, t));
}

static void
init_taskqueue_list(void *data __unused)
{

	mtx_init(&taskqueue_queues_mutex, "taskqueue list", NULL, MTX_DEF);
	STAILQ_INIT(&taskqueue_queues);
}
SYSINIT(taskqueue_list, SI_SUB_INTRINSIC, SI_ORDER_ANY, init_taskqueue_list,
    NULL);

static struct taskqueue *
_taskqueue_create(const char *name, int mflags,
		 taskqueue_enqueue_fn enqueue, void *context,
		 int mtxflags, const char *mtxname)
{
	struct taskqueue *queue;

	queue = malloc(sizeof(struct taskqueue), M_TASKQUEUE, mflags | M_ZERO);
	if (!queue)
		return 0;

	STAILQ_INIT(&queue->tq_queue);
	queue->tq_name = name;
	queue->tq_enqueue = enqueue;
	queue->tq_context = context;
	queue->tq_spin = (mtxflags & MTX_SPIN) != 0;
	queue->tq_flags |= TQ_FLAGS_ACTIVE;
	mtx_init(&queue->tq_mutex, mtxname, NULL, mtxflags);

	mtx_lock(&taskqueue_queues_mutex);
	STAILQ_INSERT_TAIL(&taskqueue_queues, queue, tq_link);
	mtx_unlock(&taskqueue_queues_mutex);

	return queue;
}

struct taskqueue *
taskqueue_create(const char *name, int mflags,
		 taskqueue_enqueue_fn enqueue, void *context)
{
	return _taskqueue_create(name, mflags, enqueue, context,
			MTX_DEF, "taskqueue");
}

/*
 * Signal a taskqueue thread to terminate.
 */
static void
taskqueue_terminate(struct proc **pp, struct taskqueue *tq)
{

	while (tq->tq_pcount > 0) {
		wakeup(tq);
		TQ_SLEEP(tq, pp, &tq->tq_mutex, PWAIT, "taskqueue_destroy", 0);
	}
}

void
taskqueue_free(struct taskqueue *queue)
{

	mtx_lock(&taskqueue_queues_mutex);
	STAILQ_REMOVE(&taskqueue_queues, queue, taskqueue, tq_link);
	mtx_unlock(&taskqueue_queues_mutex);

	TQ_LOCK(queue);
	queue->tq_flags &= ~TQ_FLAGS_ACTIVE;
	taskqueue_run(queue);
	taskqueue_terminate(queue->tq_pproc, queue);
	mtx_destroy(&queue->tq_mutex);
	free(queue->tq_pproc, M_TASKQUEUE);
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
			TQ_LOCK(queue);
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

	TQ_LOCK(queue);

	/*
	 * Count multiple enqueues.
	 */
	if (task->ta_pending) {
		task->ta_pending++;
		TQ_UNLOCK(queue);
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
	if ((queue->tq_flags & TQ_FLAGS_BLOCKED) == 0)
		queue->tq_enqueue(queue->tq_context);
	else
		queue->tq_flags |= TQ_FLAGS_PENDING;

	TQ_UNLOCK(queue);

	return 0;
}

void
taskqueue_block(struct taskqueue *queue)
{

	TQ_LOCK(queue);
	queue->tq_flags |= TQ_FLAGS_BLOCKED;
	TQ_UNLOCK(queue);
}

void
taskqueue_unblock(struct taskqueue *queue)
{

	TQ_LOCK(queue);
	queue->tq_flags &= ~TQ_FLAGS_BLOCKED;
	if (queue->tq_flags & TQ_FLAGS_PENDING) {
		queue->tq_flags &= ~TQ_FLAGS_PENDING;
		queue->tq_enqueue(queue->tq_context);
	}
	TQ_UNLOCK(queue);
}

void
taskqueue_run(struct taskqueue *queue)
{
	struct task *task;
	int owned, pending;

	owned = mtx_owned(&queue->tq_mutex);
	if (!owned)
		TQ_LOCK(queue);
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
		TQ_UNLOCK(queue);

		task->ta_func(task->ta_context, pending);

		TQ_LOCK(queue);
		queue->tq_running = NULL;
		wakeup(task);
	}

	/*
	 * For compatibility, unlock on return if the queue was not locked
	 * on entry, although this opens a race window.
	 */
	if (!owned)
		TQ_UNLOCK(queue);
}

void
taskqueue_drain(struct taskqueue *queue, struct task *task)
{
	if (queue->tq_spin) {		/* XXX */
		mtx_lock_spin(&queue->tq_mutex);
		while (task->ta_pending != 0 || task == queue->tq_running)
			msleep_spin(task, &queue->tq_mutex, "-", 0);
		mtx_unlock_spin(&queue->tq_mutex);
	} else {
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL, __func__);

		mtx_lock(&queue->tq_mutex);
		while (task->ta_pending != 0 || task == queue->tq_running)
			msleep(task, &queue->tq_mutex, PWAIT, "-", 0);
		mtx_unlock(&queue->tq_mutex);
	}
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

int
taskqueue_start_threads(struct taskqueue **tqp, int count, int pri,
			const char *name, ...)
{
	va_list ap;
	struct taskqueue *tq;
	struct thread *td;
	char ktname[MAXCOMLEN];
	int i, error;

	if (count <= 0)
		return (EINVAL);
	tq = *tqp;

	va_start(ap, name);
	vsnprintf(ktname, MAXCOMLEN, name, ap);
	va_end(ap);

	tq->tq_pproc = malloc(sizeof(struct proc *) * count, M_TASKQUEUE,
	    M_NOWAIT | M_ZERO);
	if (tq->tq_pproc == NULL) {
		printf("%s: no memory for %s threads\n", __func__, ktname);
		return (ENOMEM);
	}

	for (i = 0; i < count; i++) {
		if (count == 1)
			error = kthread_create(taskqueue_thread_loop, tqp,
			    &tq->tq_pproc[i], RFSTOPPED, 0, ktname);
		else
			error = kthread_create(taskqueue_thread_loop, tqp,
			    &tq->tq_pproc[i], RFSTOPPED, 0, "%s_%d", ktname, i);
		if (error) {
			/* should be ok to continue, taskqueue_free will dtrt */
			printf("%s: kthread_create(%s): error %d",
				__func__, ktname, error);
			tq->tq_pproc[i] = NULL;		/* paranoid */
		} else
			tq->tq_pcount++;
	}
	for (i = 0; i < count; i++) {
		if (tq->tq_pproc[i] == NULL)
			continue;
		td = FIRST_THREAD_IN_PROC(tq->tq_pproc[i]);
		thread_lock(td);
		sched_prio(td, pri);
		sched_add(td, SRQ_BORING);
		thread_unlock(td);
	}

	return (0);
}

void
taskqueue_thread_loop(void *arg)
{
	struct taskqueue **tqp, *tq;

	tqp = arg;
	tq = *tqp;
	TQ_LOCK(tq);
	while ((tq->tq_flags & TQ_FLAGS_ACTIVE) != 0) {
		taskqueue_run(tq);
		TQ_SLEEP(tq, tq, &tq->tq_mutex, 0, "-", 0);
	}

	/* rendezvous with thread that asked us to terminate */
	tq->tq_pcount--;
	wakeup_one(tq->tq_pproc);
	TQ_UNLOCK(tq);
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

struct taskqueue *
taskqueue_create_fast(const char *name, int mflags,
		 taskqueue_enqueue_fn enqueue, void *context)
{
	return _taskqueue_create(name, mflags, enqueue, context,
			MTX_SPIN, "fast_taskqueue");
}

/* NB: for backwards compatibility */
int
taskqueue_enqueue_fast(struct taskqueue *queue, struct task *task)
{
	return taskqueue_enqueue(queue, task);
}

static void	*taskqueue_fast_ih;

static void
taskqueue_fast_enqueue(void *context)
{
	swi_sched(taskqueue_fast_ih, 0);
}

static void
taskqueue_fast_run(void *dummy)
{
	taskqueue_run(taskqueue_fast);
}

TASKQUEUE_FAST_DEFINE(fast, taskqueue_fast_enqueue, 0,
	swi_add(NULL, "Fast task queue", taskqueue_fast_run, NULL,
	SWI_TQ_FAST, INTR_MPSAFE, &taskqueue_fast_ih));

int
taskqueue_member(struct taskqueue *queue, struct thread *td)
{
	int i, j, ret = 0;
	struct thread *ptd;

	TQ_LOCK(queue);
	for (i = 0, j = 0; ; i++) {
		if (queue->tq_pproc[i] == NULL)
			continue;
		ptd = FIRST_THREAD_IN_PROC(queue->tq_pproc[i]);
		/*
		 * In releng7 all kprocs have only one kthread, so there is
		 * no need to use FOREACH_THREAD_IN_PROC instead.
		 * If this changes at some point, only the first 'if' needs
		 * to be included in the FOREACH_..., the second one can
		 * stay as it is.
		 */
		if (ptd == td) {
			ret = 1;
			break;
		}
		if (++j >= queue->tq_pcount)
			break;
	}
	TQ_UNLOCK(queue);
	return (ret);
}
