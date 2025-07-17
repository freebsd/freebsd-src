/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpuset.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/libkern.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/epoch.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/stdarg.h>
#include <sys/taskqueue.h>
#include <sys/unistd.h>

static MALLOC_DEFINE(M_TASKQUEUE, "taskqueue", "Task Queues");
static void	*taskqueue_giant_ih;
static void	*taskqueue_ih;
static void	 taskqueue_fast_enqueue(void *);
static void	 taskqueue_swi_enqueue(void *);
static void	 taskqueue_swi_giant_enqueue(void *);

struct taskqueue_busy {
	struct task		*tb_running;
	u_int			 tb_seq;
	bool			 tb_canceling;
	LIST_ENTRY(taskqueue_busy) tb_link;
};

struct taskqueue {
	STAILQ_HEAD(, task)	tq_queue;
	LIST_HEAD(, taskqueue_busy) tq_active;
	struct task		*tq_hint;
	u_int			tq_seq;
	int			tq_callouts;
	struct mtx_padalign	tq_mutex;
	taskqueue_enqueue_fn	tq_enqueue;
	void			*tq_context;
	char			*tq_name;
	struct thread		**tq_threads;
	int			tq_tcount;
	int			tq_spin;
	int			tq_flags;
	taskqueue_callback_fn	tq_callbacks[TASKQUEUE_NUM_CALLBACKS];
	void			*tq_cb_contexts[TASKQUEUE_NUM_CALLBACKS];
};

#define	TQ_FLAGS_ACTIVE		(1 << 0)
#define	TQ_FLAGS_BLOCKED	(1 << 1)
#define	TQ_FLAGS_UNLOCKED_ENQUEUE	(1 << 2)

#define	DT_CALLOUT_ARMED	(1 << 0)
#define	DT_DRAIN_IN_PROGRESS	(1 << 1)

#define	TQ_LOCK(tq)							\
	do {								\
		if ((tq)->tq_spin)					\
			mtx_lock_spin(&(tq)->tq_mutex);			\
		else							\
			mtx_lock(&(tq)->tq_mutex);			\
	} while (0)
#define	TQ_ASSERT_LOCKED(tq)	mtx_assert(&(tq)->tq_mutex, MA_OWNED)

#define	TQ_UNLOCK(tq)							\
	do {								\
		if ((tq)->tq_spin)					\
			mtx_unlock_spin(&(tq)->tq_mutex);		\
		else							\
			mtx_unlock(&(tq)->tq_mutex);			\
	} while (0)
#define	TQ_ASSERT_UNLOCKED(tq)	mtx_assert(&(tq)->tq_mutex, MA_NOTOWNED)

void
_timeout_task_init(struct taskqueue *queue, struct timeout_task *timeout_task,
    int priority, task_fn_t func, void *context)
{

	TASK_INIT(&timeout_task->t, priority, func, context);
	callout_init_mtx(&timeout_task->c, &queue->tq_mutex,
	    CALLOUT_RETURNUNLOCKED);
	timeout_task->q = queue;
	timeout_task->f = 0;
}

static __inline int
TQ_SLEEP(struct taskqueue *tq, void *p, const char *wm)
{
	if (tq->tq_spin)
		return (msleep_spin(p, (struct mtx *)&tq->tq_mutex, wm, 0));
	return (msleep(p, &tq->tq_mutex, 0, wm, 0));
}

static struct taskqueue_busy *
task_get_busy(struct taskqueue *queue, struct task *task)
{
	struct taskqueue_busy *tb;

	TQ_ASSERT_LOCKED(queue);
	LIST_FOREACH(tb, &queue->tq_active, tb_link) {
		if (tb->tb_running == task)
			return (tb);
	}
	return (NULL);
}

static struct taskqueue *
_taskqueue_create(const char *name, int mflags,
		 taskqueue_enqueue_fn enqueue, void *context,
		 int mtxflags, const char *mtxname __unused)
{
	struct taskqueue *queue;
	char *tq_name;

	tq_name = malloc(TASKQUEUE_NAMELEN, M_TASKQUEUE, mflags | M_ZERO);
	if (tq_name == NULL)
		return (NULL);

	queue = malloc(sizeof(struct taskqueue), M_TASKQUEUE, mflags | M_ZERO);
	if (queue == NULL) {
		free(tq_name, M_TASKQUEUE);
		return (NULL);
	}

	snprintf(tq_name, TASKQUEUE_NAMELEN, "%s", (name) ? name : "taskqueue");

	STAILQ_INIT(&queue->tq_queue);
	LIST_INIT(&queue->tq_active);
	queue->tq_enqueue = enqueue;
	queue->tq_context = context;
	queue->tq_name = tq_name;
	queue->tq_spin = (mtxflags & MTX_SPIN) != 0;
	queue->tq_flags |= TQ_FLAGS_ACTIVE;
	if (enqueue == taskqueue_fast_enqueue ||
	    enqueue == taskqueue_swi_enqueue ||
	    enqueue == taskqueue_swi_giant_enqueue ||
	    enqueue == taskqueue_thread_enqueue)
		queue->tq_flags |= TQ_FLAGS_UNLOCKED_ENQUEUE;
	mtx_init(&queue->tq_mutex, tq_name, NULL, mtxflags);

	return (queue);
}

struct taskqueue *
taskqueue_create(const char *name, int mflags,
		 taskqueue_enqueue_fn enqueue, void *context)
{

	return _taskqueue_create(name, mflags, enqueue, context,
			MTX_DEF, name);
}

void
taskqueue_set_callback(struct taskqueue *queue,
    enum taskqueue_callback_type cb_type, taskqueue_callback_fn callback,
    void *context)
{

	KASSERT(((cb_type >= TASKQUEUE_CALLBACK_TYPE_MIN) &&
	    (cb_type <= TASKQUEUE_CALLBACK_TYPE_MAX)),
	    ("Callback type %d not valid, must be %d-%d", cb_type,
	    TASKQUEUE_CALLBACK_TYPE_MIN, TASKQUEUE_CALLBACK_TYPE_MAX));
	KASSERT((queue->tq_callbacks[cb_type] == NULL),
	    ("Re-initialization of taskqueue callback?"));

	queue->tq_callbacks[cb_type] = callback;
	queue->tq_cb_contexts[cb_type] = context;
}

/*
 * Signal a taskqueue thread to terminate.
 */
static void
taskqueue_terminate(struct thread **pp, struct taskqueue *tq)
{

	while (tq->tq_tcount > 0 || tq->tq_callouts > 0) {
		wakeup(tq);
		TQ_SLEEP(tq, pp, "tq_destroy");
	}
}

void
taskqueue_free(struct taskqueue *queue)
{

	TQ_LOCK(queue);
	queue->tq_flags &= ~TQ_FLAGS_ACTIVE;
	taskqueue_terminate(queue->tq_threads, queue);
	KASSERT(LIST_EMPTY(&queue->tq_active), ("Tasks still running?"));
	KASSERT(queue->tq_callouts == 0, ("Armed timeout tasks"));
	mtx_destroy(&queue->tq_mutex);
	free(queue->tq_threads, M_TASKQUEUE);
	free(queue->tq_name, M_TASKQUEUE);
	free(queue, M_TASKQUEUE);
}

static int
taskqueue_enqueue_locked(struct taskqueue *queue, struct task *task, int flags)
{
	struct task *ins;
	struct task *prev;
	struct taskqueue_busy *tb;

	KASSERT(task->ta_func != NULL, ("enqueueing task with NULL func"));
	/*
	 * Ignore canceling task if requested.
	 */
	if (__predict_false((flags & TASKQUEUE_FAIL_IF_CANCELING) != 0)) {
		tb = task_get_busy(queue, task);
		if (tb != NULL && tb->tb_canceling) {
			TQ_UNLOCK(queue);
			return (ECANCELED);
		}
	}

	/*
	 * Count multiple enqueues.
	 */
	if (task->ta_pending) {
		if (__predict_false((flags & TASKQUEUE_FAIL_IF_PENDING) != 0)) {
			TQ_UNLOCK(queue);
			return (EEXIST);
		}
		if (task->ta_pending < USHRT_MAX)
			task->ta_pending++;
		TQ_UNLOCK(queue);
		return (0);
	}

	/*
	 * Optimise cases when all tasks use small set of priorities.
	 * In case of only one priority we always insert at the end.
	 * In case of two tq_hint typically gives the insertion point.
	 * In case of more then two tq_hint should halve the search.
	 */
	prev = STAILQ_LAST(&queue->tq_queue, task, ta_link);
	if (!prev || prev->ta_priority >= task->ta_priority) {
		STAILQ_INSERT_TAIL(&queue->tq_queue, task, ta_link);
	} else {
		prev = queue->tq_hint;
		if (prev && prev->ta_priority >= task->ta_priority) {
			ins = STAILQ_NEXT(prev, ta_link);
		} else {
			prev = NULL;
			ins = STAILQ_FIRST(&queue->tq_queue);
		}
		for (; ins; prev = ins, ins = STAILQ_NEXT(ins, ta_link))
			if (ins->ta_priority < task->ta_priority)
				break;

		if (prev) {
			STAILQ_INSERT_AFTER(&queue->tq_queue, prev, task, ta_link);
			queue->tq_hint = task;
		} else
			STAILQ_INSERT_HEAD(&queue->tq_queue, task, ta_link);
	}

	task->ta_pending = 1;
	if ((queue->tq_flags & TQ_FLAGS_UNLOCKED_ENQUEUE) != 0)
		TQ_UNLOCK(queue);
	if ((queue->tq_flags & TQ_FLAGS_BLOCKED) == 0)
		queue->tq_enqueue(queue->tq_context);
	if ((queue->tq_flags & TQ_FLAGS_UNLOCKED_ENQUEUE) == 0)
		TQ_UNLOCK(queue);

	/* Return with lock released. */
	return (0);
}

int
taskqueue_enqueue_flags(struct taskqueue *queue, struct task *task, int flags)
{
	int res;

	TQ_LOCK(queue);
	res = taskqueue_enqueue_locked(queue, task, flags);
	/* The lock is released inside. */

	return (res);
}

int
taskqueue_enqueue(struct taskqueue *queue, struct task *task)
{
	return (taskqueue_enqueue_flags(queue, task, 0));
}

static void
taskqueue_timeout_func(void *arg)
{
	struct taskqueue *queue;
	struct timeout_task *timeout_task;

	timeout_task = arg;
	queue = timeout_task->q;
	KASSERT((timeout_task->f & DT_CALLOUT_ARMED) != 0, ("Stray timeout"));
	timeout_task->f &= ~DT_CALLOUT_ARMED;
	queue->tq_callouts--;
	taskqueue_enqueue_locked(timeout_task->q, &timeout_task->t, 0);
	/* The lock is released inside. */
}

int
taskqueue_enqueue_timeout_sbt(struct taskqueue *queue,
    struct timeout_task *timeout_task, sbintime_t sbt, sbintime_t pr, int flags)
{
	int res;

	TQ_LOCK(queue);
	KASSERT(timeout_task->q == NULL || timeout_task->q == queue,
	    ("Migrated queue"));
	timeout_task->q = queue;
	res = timeout_task->t.ta_pending;
	if (timeout_task->f & DT_DRAIN_IN_PROGRESS) {
		/* Do nothing */
		TQ_UNLOCK(queue);
		res = -1;
	} else if (sbt == 0) {
		taskqueue_enqueue_locked(queue, &timeout_task->t, 0);
		/* The lock is released inside. */
	} else {
		if ((timeout_task->f & DT_CALLOUT_ARMED) != 0) {
			res++;
		} else {
			queue->tq_callouts++;
			timeout_task->f |= DT_CALLOUT_ARMED;
			if (sbt < 0)
				sbt = -sbt; /* Ignore overflow. */
		}
		if (sbt > 0) {
			if (queue->tq_spin)
				flags |= C_DIRECT_EXEC;
			if (queue->tq_spin && queue->tq_tcount == 1 &&
			    queue->tq_threads[0] == curthread) {
				callout_reset_sbt_curcpu(&timeout_task->c, sbt, pr,
				    taskqueue_timeout_func, timeout_task, flags);
			} else {
				callout_reset_sbt(&timeout_task->c, sbt, pr,
				    taskqueue_timeout_func, timeout_task, flags);
			}
		}
		TQ_UNLOCK(queue);
	}
	return (res);
}

int
taskqueue_enqueue_timeout(struct taskqueue *queue,
    struct timeout_task *ttask, int ticks)
{

	return (taskqueue_enqueue_timeout_sbt(queue, ttask, ticks * tick_sbt,
	    0, C_HARDCLOCK));
}

static void
taskqueue_task_nop_fn(void *context, int pending)
{
}

/*
 * Block until all currently queued tasks in this taskqueue
 * have begun execution.  Tasks queued during execution of
 * this function are ignored.
 */
static int
taskqueue_drain_tq_queue(struct taskqueue *queue)
{
	struct task t_barrier;

	if (STAILQ_EMPTY(&queue->tq_queue))
		return (0);

	/*
	 * Enqueue our barrier after all current tasks, but with
	 * the highest priority so that newly queued tasks cannot
	 * pass it.  Because of the high priority, we can not use
	 * taskqueue_enqueue_locked directly (which drops the lock
	 * anyway) so just insert it at tail while we have the
	 * queue lock.
	 */
	TASK_INIT(&t_barrier, UCHAR_MAX, taskqueue_task_nop_fn, &t_barrier);
	STAILQ_INSERT_TAIL(&queue->tq_queue, &t_barrier, ta_link);
	queue->tq_hint = &t_barrier;
	t_barrier.ta_pending = 1;

	/*
	 * Once the barrier has executed, all previously queued tasks
	 * have completed or are currently executing.
	 */
	while (t_barrier.ta_pending != 0)
		TQ_SLEEP(queue, &t_barrier, "tq_qdrain");
	return (1);
}

/*
 * Block until all currently executing tasks for this taskqueue
 * complete.  Tasks that begin execution during the execution
 * of this function are ignored.
 */
static int
taskqueue_drain_tq_active(struct taskqueue *queue)
{
	struct taskqueue_busy *tb;
	u_int seq;

	if (LIST_EMPTY(&queue->tq_active))
		return (0);

	/* Block taskq_terminate().*/
	queue->tq_callouts++;

	/* Wait for any active task with sequence from the past. */
	seq = queue->tq_seq;
restart:
	LIST_FOREACH(tb, &queue->tq_active, tb_link) {
		if ((int)(tb->tb_seq - seq) <= 0) {
			TQ_SLEEP(queue, tb->tb_running, "tq_adrain");
			goto restart;
		}
	}

	/* Release taskqueue_terminate(). */
	queue->tq_callouts--;
	if ((queue->tq_flags & TQ_FLAGS_ACTIVE) == 0)
		wakeup_one(queue->tq_threads);
	return (1);
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
	if (!STAILQ_EMPTY(&queue->tq_queue))
		queue->tq_enqueue(queue->tq_context);
	TQ_UNLOCK(queue);
}

static void
taskqueue_run_locked(struct taskqueue *queue)
{
	struct epoch_tracker et;
	struct taskqueue_busy tb;
	struct task *task;
	bool in_net_epoch;
	int pending;

	KASSERT(queue != NULL, ("tq is NULL"));
	TQ_ASSERT_LOCKED(queue);
	tb.tb_running = NULL;
	LIST_INSERT_HEAD(&queue->tq_active, &tb, tb_link);
	in_net_epoch = false;

	while ((task = STAILQ_FIRST(&queue->tq_queue)) != NULL) {
		STAILQ_REMOVE_HEAD(&queue->tq_queue, ta_link);
		if (queue->tq_hint == task)
			queue->tq_hint = NULL;
		pending = task->ta_pending;
		task->ta_pending = 0;
		tb.tb_running = task;
		tb.tb_seq = ++queue->tq_seq;
		tb.tb_canceling = false;
		TQ_UNLOCK(queue);

		KASSERT(task->ta_func != NULL, ("task->ta_func is NULL"));
		if (!in_net_epoch && TASK_IS_NET(task)) {
			in_net_epoch = true;
			NET_EPOCH_ENTER(et);
		} else if (in_net_epoch && !TASK_IS_NET(task)) {
			NET_EPOCH_EXIT(et);
			in_net_epoch = false;
		}
		task->ta_func(task->ta_context, pending);

		TQ_LOCK(queue);
		wakeup(task);
	}
	if (in_net_epoch)
		NET_EPOCH_EXIT(et);
	LIST_REMOVE(&tb, tb_link);
}

void
taskqueue_run(struct taskqueue *queue)
{

	TQ_LOCK(queue);
	taskqueue_run_locked(queue);
	TQ_UNLOCK(queue);
}

/*
 * Only use this function in single threaded contexts. It returns
 * non-zero if the given task is either pending or running. Else the
 * task is idle and can be queued again or freed.
 */
int
taskqueue_poll_is_busy(struct taskqueue *queue, struct task *task)
{
	int retval;

	TQ_LOCK(queue);
	retval = task->ta_pending > 0 || task_get_busy(queue, task) != NULL;
	TQ_UNLOCK(queue);

	return (retval);
}

static int
taskqueue_cancel_locked(struct taskqueue *queue, struct task *task,
    u_int *pendp)
{
	struct taskqueue_busy *tb;
	int retval = 0;

	if (task->ta_pending > 0) {
		STAILQ_REMOVE(&queue->tq_queue, task, task, ta_link);
		if (queue->tq_hint == task)
			queue->tq_hint = NULL;
	}
	if (pendp != NULL)
		*pendp = task->ta_pending;
	task->ta_pending = 0;
	tb = task_get_busy(queue, task);
	if (tb != NULL) {
		tb->tb_canceling = true;
		retval = EBUSY;
	}

	return (retval);
}

int
taskqueue_cancel(struct taskqueue *queue, struct task *task, u_int *pendp)
{
	int error;

	TQ_LOCK(queue);
	error = taskqueue_cancel_locked(queue, task, pendp);
	TQ_UNLOCK(queue);

	return (error);
}

int
taskqueue_cancel_timeout(struct taskqueue *queue,
    struct timeout_task *timeout_task, u_int *pendp)
{
	u_int pending, pending1;
	int error;

	TQ_LOCK(queue);
	pending = !!(callout_stop(&timeout_task->c) > 0);
	error = taskqueue_cancel_locked(queue, &timeout_task->t, &pending1);
	if ((timeout_task->f & DT_CALLOUT_ARMED) != 0) {
		timeout_task->f &= ~DT_CALLOUT_ARMED;
		queue->tq_callouts--;
	}
	TQ_UNLOCK(queue);

	if (pendp != NULL)
		*pendp = pending + pending1;
	return (error);
}

void
taskqueue_drain(struct taskqueue *queue, struct task *task)
{

	if (!queue->tq_spin)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL, __func__);

	TQ_LOCK(queue);
	while (task->ta_pending != 0 || task_get_busy(queue, task) != NULL)
		TQ_SLEEP(queue, task, "tq_drain");
	TQ_UNLOCK(queue);
}

void
taskqueue_drain_all(struct taskqueue *queue)
{

	if (!queue->tq_spin)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL, __func__);

	TQ_LOCK(queue);
	(void)taskqueue_drain_tq_queue(queue);
	(void)taskqueue_drain_tq_active(queue);
	TQ_UNLOCK(queue);
}

void
taskqueue_drain_timeout(struct taskqueue *queue,
    struct timeout_task *timeout_task)
{

	/*
	 * Set flag to prevent timer from re-starting during drain:
	 */
	TQ_LOCK(queue);
	KASSERT((timeout_task->f & DT_DRAIN_IN_PROGRESS) == 0,
	    ("Drain already in progress"));
	timeout_task->f |= DT_DRAIN_IN_PROGRESS;
	TQ_UNLOCK(queue);

	callout_drain(&timeout_task->c);
	taskqueue_drain(queue, &timeout_task->t);

	/*
	 * Clear flag to allow timer to re-start:
	 */
	TQ_LOCK(queue);
	timeout_task->f &= ~DT_DRAIN_IN_PROGRESS;
	TQ_UNLOCK(queue);
}

void
taskqueue_quiesce(struct taskqueue *queue)
{
	int ret;

	TQ_LOCK(queue);
	do {
		ret = taskqueue_drain_tq_queue(queue);
		if (ret == 0)
			ret = taskqueue_drain_tq_active(queue);
	} while (ret != 0);
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

static int
_taskqueue_start_threads(struct taskqueue **tqp, int count, int pri,
    cpuset_t *mask, struct proc *p, const char *name, va_list ap)
{
	char ktname[MAXCOMLEN + 1];
	struct thread *td;
	struct taskqueue *tq;
	int i, error;

	if (count <= 0)
		return (EINVAL);

	vsnprintf(ktname, sizeof(ktname), name, ap);
	tq = *tqp;

	tq->tq_threads = malloc(sizeof(struct thread *) * count, M_TASKQUEUE,
	    M_NOWAIT | M_ZERO);
	if (tq->tq_threads == NULL) {
		printf("%s: no memory for %s threads\n", __func__, ktname);
		return (ENOMEM);
	}

	for (i = 0; i < count; i++) {
		if (count == 1)
			error = kthread_add(taskqueue_thread_loop, tqp, p,
			    &tq->tq_threads[i], RFSTOPPED, 0, "%s", ktname);
		else
			error = kthread_add(taskqueue_thread_loop, tqp, p,
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
	if (tq->tq_tcount == 0) {
		free(tq->tq_threads, M_TASKQUEUE);
		tq->tq_threads = NULL;
		return (ENOMEM);
	}
	for (i = 0; i < count; i++) {
		if (tq->tq_threads[i] == NULL)
			continue;
		td = tq->tq_threads[i];
		if (mask) {
			error = cpuset_setthread(td->td_tid, mask);
			/*
			 * Failing to pin is rarely an actual fatal error;
			 * it'll just affect performance.
			 */
			if (error)
				printf("%s: curthread=%llu: can't pin; "
				    "error=%d\n",
				    __func__,
				    (unsigned long long) td->td_tid,
				    error);
		}
		thread_lock(td);
		sched_prio(td, pri);
		sched_add(td, SRQ_BORING);
	}

	return (0);
}

int
taskqueue_start_threads(struct taskqueue **tqp, int count, int pri,
    const char *name, ...)
{
	va_list ap;
	int error;

	va_start(ap, name);
	error = _taskqueue_start_threads(tqp, count, pri, NULL, NULL, name, ap);
	va_end(ap);
	return (error);
}

int
taskqueue_start_threads_in_proc(struct taskqueue **tqp, int count, int pri,
    struct proc *proc, const char *name, ...)
{
	va_list ap;
	int error;

	va_start(ap, name);
	error = _taskqueue_start_threads(tqp, count, pri, NULL, proc, name, ap);
	va_end(ap);
	return (error);
}

int
taskqueue_start_threads_cpuset(struct taskqueue **tqp, int count, int pri,
    cpuset_t *mask, const char *name, ...)
{
	va_list ap;
	int error;

	va_start(ap, name);
	error = _taskqueue_start_threads(tqp, count, pri, mask, NULL, name, ap);
	va_end(ap);
	return (error);
}

static inline void
taskqueue_run_callback(struct taskqueue *tq,
    enum taskqueue_callback_type cb_type)
{
	taskqueue_callback_fn tq_callback;

	TQ_ASSERT_UNLOCKED(tq);
	tq_callback = tq->tq_callbacks[cb_type];
	if (tq_callback != NULL)
		tq_callback(tq->tq_cb_contexts[cb_type]);
}

void
taskqueue_thread_loop(void *arg)
{
	struct taskqueue **tqp, *tq;

	tqp = arg;
	tq = *tqp;
	taskqueue_run_callback(tq, TASKQUEUE_CALLBACK_TYPE_INIT);
	TQ_LOCK(tq);
	while ((tq->tq_flags & TQ_FLAGS_ACTIVE) != 0) {
		/* XXX ? */
		taskqueue_run_locked(tq);
		/*
		 * Because taskqueue_run() can drop tq_mutex, we need to
		 * check if the TQ_FLAGS_ACTIVE flag wasn't removed in the
		 * meantime, which means we missed a wakeup.
		 */
		if ((tq->tq_flags & TQ_FLAGS_ACTIVE) == 0)
			break;
		TQ_SLEEP(tq, tq, "-");
	}
	taskqueue_run_locked(tq);
	/*
	 * This thread is on its way out, so just drop the lock temporarily
	 * in order to call the shutdown callback.  This allows the callback
	 * to look at the taskqueue, even just before it dies.
	 */
	TQ_UNLOCK(tq);
	taskqueue_run_callback(tq, TASKQUEUE_CALLBACK_TYPE_SHUTDOWN);
	TQ_LOCK(tq);

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
	wakeup_any(tq);
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
	swi_add(NULL, "fast taskq", taskqueue_fast_run, NULL,
	SWI_TQ_FAST, INTR_MPSAFE, &taskqueue_fast_ih));

int
taskqueue_member(struct taskqueue *queue, struct thread *td)
{
	int i, j, ret = 0;

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
	return (ret);
}
