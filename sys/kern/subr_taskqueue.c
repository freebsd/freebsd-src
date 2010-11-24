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

struct taskqueue_busy {
	struct task	*tb_running;
	TAILQ_ENTRY(taskqueue_busy) tb_link;
};

struct taskqueue {
	STAILQ_HEAD(, task)	tq_queue;
	taskqueue_enqueue_fn	tq_enqueue;
	void			*tq_context;
	TAILQ_HEAD(, taskqueue_busy) tq_active;
	struct mtx		tq_mutex;
	struct thread		**tq_threads;
	int			tq_tcount;
	int			tq_spin;
	int			tq_flags;
};

#define	TQ_FLAGS_ACTIVE		(1 << 0)
#define	TQ_FLAGS_BLOCKED	(1 << 1)
#define	TQ_FLAGS_PENDING	(1 << 2)

#define	TQ_LOCK(tq)							\
	do {								\
		if ((tq)->tq_spin)					\
			mtx_lock_spin(&(tq)->tq_mutex);			\
		else							\
			mtx_lock(&(tq)->tq_mutex);			\
	} while (0)

#define	TQ_UNLOCK(tq)							\
	do {								\
		if ((tq)->tq_spin)					\
			mtx_unlock_spin(&(tq)->tq_mutex);		\
		else							\
			mtx_unlock(&(tq)->tq_mutex);			\
	} while (0)

static __inline int
TQ_SLEEP(struct taskqueue *tq, void *p, struct mtx *m, int pri, const char *wm,
    int t)
{
	if (tq->tq_spin)
		return (msleep_spin(p, m, wm, t));
	return (msleep(p, m, pri, wm, t));
}

static struct taskqueue *
_taskqueue_create(const char *name __unused, int mflags,
		 taskqueue_enqueue_fn enqueue, void *context,
		 int mtxflags, const char *mtxname)
{
	struct taskqueue *queue;

	queue = malloc(sizeof(struct taskqueue), M_TASKQUEUE, mflags | M_ZERO);
	if (!queue)
		return NULL;

	STAILQ_INIT(&queue->tq_queue);
	TAILQ_INIT(&queue->tq_active);
	queue->tq_enqueue = enqueue;
	queue->tq_context = context;
	queue->tq_spin = (mtxflags & MTX_SPIN) != 0;
	queue->tq_flags |= TQ_FLAGS_ACTIVE;
	mtx_init(&queue->tq_mutex, mtxname, NULL, mtxflags);

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
taskqueue_terminate(struct thread **pp, struct taskqueue *tq)
{

	while (tq->tq_tcount > 0) {
		wakeup(tq);
		TQ_SLEEP(tq, pp, &tq->tq_mutex, PWAIT, "taskqueue_destroy", 0);
	}
}

void
taskqueue_free(struct taskqueue *queue)
{

	TQ_LOCK(queue);
	queue->tq_flags &= ~TQ_FLAGS_ACTIVE;
	taskqueue_terminate(queue->tq_threads, queue);
	KASSERT(TAILQ_EMPTY(&queue->tq_active), ("Tasks still running?"));
	mtx_destroy(&queue->tq_mutex);
	free(queue->tq_threads, M_TASKQUEUE);
	free(queue, M_TASKQUEUE);
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
		prev = NULL;
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

static void
taskqueue_run_locked(struct taskqueue *queue)
{
	struct taskqueue_busy tb;
	struct task *task;
	int pending;

	mtx_assert(&queue->tq_mutex, MA_OWNED);
	tb.tb_running = NULL;
	TAILQ_INSERT_TAIL(&queue->tq_active, &tb, tb_link);

	while (STAILQ_FIRST(&queue->tq_queue)) {
		/*
		 * Carefully remove the first task from the queue and
		 * zero its pending count.
		 */
		task = STAILQ_FIRST(&queue->tq_queue);
		STAILQ_REMOVE_HEAD(&queue->tq_queue, ta_link);
		pending = task->ta_pending;
		task->ta_pending = 0;
		tb.tb_running = task;
		TQ_UNLOCK(queue);

		task->ta_func(task->ta_context, pending);

		TQ_LOCK(queue);
		tb.tb_running = NULL;
		wakeup(task);
	}
	TAILQ_REMOVE(&queue->tq_active, &tb, tb_link);
}

void
taskqueue_run(struct taskqueue *queue)
{

	TQ_LOCK(queue);
	taskqueue_run_locked(queue);
	TQ_UNLOCK(queue);
}

static int
task_is_running(struct taskqueue *queue, struct task *task)
{
	struct taskqueue_busy *tb;

	mtx_assert(&queue->tq_mutex, MA_OWNED);
	TAILQ_FOREACH(tb, &queue->tq_active, tb_link) {
		if (tb->tb_running == task)
			return (1);
	}
	return (0);
}

int
taskqueue_cancel(struct taskqueue *queue, struct task *task, u_int *pendp)
{
	u_int pending;
	int error;

	TQ_LOCK(queue);
	if ((pending = task->ta_pending) > 0)
		STAILQ_REMOVE(&queue->tq_queue, task, task, ta_link);
	task->ta_pending = 0;
	error = task_is_running(queue, task) ? EBUSY : 0;
	TQ_UNLOCK(queue);

	if (pendp != NULL)
		*pendp = pending;
	return (error);
}

void
taskqueue_drain(struct taskqueue *queue, struct task *task)
{

	if (!queue->tq_spin)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL, __func__);

	TQ_LOCK(queue);
	while (task->ta_pending != 0 || task_is_running(queue, task))
		TQ_SLEEP(queue, task, &queue->tq_mutex, PWAIT, "-", 0);
	TQ_UNLOCK(queue);
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
	struct thread *td;
	struct taskqueue *tq;
	int i, error;
	char ktname[MAXCOMLEN + 1];

	if (count <= 0)
		return (EINVAL);

	tq = *tqp;

	va_start(ap, name);
	vsnprintf(ktname, sizeof(ktname), name, ap);
	va_end(ap);

	tq->tq_threads = malloc(sizeof(struct thread *) * count, M_TASKQUEUE,
	    M_NOWAIT | M_ZERO);
	if (tq->tq_threads == NULL) {
		printf("%s: no memory for %s threads\n", __func__, ktname);
		return (ENOMEM);
	}

	for (i = 0; i < count; i++) {
		if (count == 1)
			error = kthread_add(taskqueue_thread_loop, tqp, NULL,
			    &tq->tq_threads[i], RFSTOPPED, 0, "%s", ktname);
		else
			error = kthread_add(taskqueue_thread_loop, tqp, NULL,
			    &tq->tq_threads[i], RFSTOPPED, 0,
			    "%s_%d", ktname, i);
		if (error) {
			/* should be ok to continue, taskqueue_free will dtrt */
			printf("%s: kthread_add(%s): error %d", __func__,
			    ktname, error);
			tq->tq_threads[i] = NULL;		/* paranoid */
		} else
			tq->tq_tcount++;
	}
	for (i = 0; i < count; i++) {
		if (tq->tq_threads[i] == NULL)
			continue;
		td = tq->tq_threads[i];
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
		taskqueue_run_locked(tq);
		/*
		 * Because taskqueue_run() can drop tq_mutex, we need to
		 * check if the TQ_FLAGS_ACTIVE flag wasn't removed in the
		 * meantime, which means we missed a wakeup.
		 */
		if ((tq->tq_flags & TQ_FLAGS_ACTIVE) == 0)
			break;
		TQ_SLEEP(tq, tq, &tq->tq_mutex, 0, "-", 0);
	}
	taskqueue_run_locked(tq);

	/* rendezvous with thread that asked us to terminate */
	tq->tq_tcount--;
	wakeup_one(tq->tq_threads);
	TQ_UNLOCK(tq);
	kthread_exit();
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

TASKQUEUE_DEFINE(swi, taskqueue_swi_enqueue, NULL,
		 swi_add(NULL, "task queue", taskqueue_swi_run, NULL, SWI_TQ,
		     INTR_MPSAFE, &taskqueue_ih)); 

TASKQUEUE_DEFINE(swi_giant, taskqueue_swi_giant_enqueue, NULL,
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

TASKQUEUE_FAST_DEFINE(fast, taskqueue_fast_enqueue, NULL,
	swi_add(NULL, "Fast task queue", taskqueue_fast_run, NULL,
	SWI_TQ_FAST, INTR_MPSAFE, &taskqueue_fast_ih));

int
taskqueue_member(struct taskqueue *queue, struct thread *td)
{
	int i, j, ret = 0;

	TQ_LOCK(queue);
	for (i = 0, j = 0; ; i++) {
		if (queue->tq_threads[i] == NULL)
			continue;
		if (queue->tq_threads[i] == td) {
			ret = 1;
			break;
		}
		if (++j >= queue->tq_tcount)
			break;
	}
	TQ_UNLOCK(queue);
	return (ret);
}
