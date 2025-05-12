/*-
 * Copyright (c) 2017-2019 Hans Petter Selasky
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/compat.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/irq_work.h>

#include <sys/kernel.h>

/*
 * Define all work struct states
 */
enum {
	WORK_ST_IDLE,			/* idle - not started */
	WORK_ST_TIMER,			/* timer is being started */
	WORK_ST_TASK,			/* taskqueue is being queued */
	WORK_ST_EXEC,			/* callback is being called */
	WORK_ST_CANCEL,			/* cancel is being requested */
	WORK_ST_MAX,
};

/*
 * Define global workqueues
 */
static struct workqueue_struct *linux_system_short_wq;
static struct workqueue_struct *linux_system_long_wq;

struct workqueue_struct *system_wq;
struct workqueue_struct *system_long_wq;
struct workqueue_struct *system_unbound_wq;
struct workqueue_struct *system_highpri_wq;
struct workqueue_struct *system_power_efficient_wq;

struct taskqueue *linux_irq_work_tq;

static int linux_default_wq_cpus = 4;

static void linux_delayed_work_timer_fn(void *);

/*
 * This function atomically updates the work state and returns the
 * previous state at the time of update.
 */
static uint8_t
linux_update_state(atomic_t *v, const uint8_t *pstate)
{
	int c, old;

	c = v->counter;

	while ((old = atomic_cmpxchg(v, c, pstate[c])) != c)
		c = old;

	return (c);
}

/*
 * A LinuxKPI task is allowed to free itself inside the callback function
 * and cannot safely be referred after the callback function has
 * completed. This function gives the linux_work_fn() function a hint,
 * that the task is not going away and can have its state checked
 * again. Without this extra hint LinuxKPI tasks cannot be serialized
 * across multiple worker threads.
 */
static bool
linux_work_exec_unblock(struct work_struct *work)
{
	struct workqueue_struct *wq;
	struct work_exec *exec;
	bool retval = false;

	wq = work->work_queue;
	if (unlikely(wq == NULL))
		goto done;

	WQ_EXEC_LOCK(wq);
	TAILQ_FOREACH(exec, &wq->exec_head, entry) {
		if (exec->target == work) {
			exec->target = NULL;
			retval = true;
			break;
		}
	}
	WQ_EXEC_UNLOCK(wq);
done:
	return (retval);
}

static void
linux_delayed_work_enqueue(struct delayed_work *dwork)
{
	struct taskqueue *tq;

	tq = dwork->work.work_queue->taskqueue;
	taskqueue_enqueue(tq, &dwork->work.work_task);
}

/*
 * This function queues the given work structure on the given
 * workqueue. It returns non-zero if the work was successfully
 * [re-]queued. Else the work is already pending for completion.
 */
bool
linux_queue_work_on(int cpu __unused, struct workqueue_struct *wq,
    struct work_struct *work)
{
	static const uint8_t states[WORK_ST_MAX] __aligned(8) = {
		[WORK_ST_IDLE] = WORK_ST_TASK,		/* start queuing task */
		[WORK_ST_TIMER] = WORK_ST_TIMER,	/* NOP */
		[WORK_ST_TASK] = WORK_ST_TASK,		/* NOP */
		[WORK_ST_EXEC] = WORK_ST_TASK,		/* queue task another time */
		[WORK_ST_CANCEL] = WORK_ST_TASK,	/* start queuing task again */
	};

	if (atomic_read(&wq->draining) != 0)
		return (!work_pending(work));

	switch (linux_update_state(&work->state, states)) {
	case WORK_ST_EXEC:
	case WORK_ST_CANCEL:
		if (linux_work_exec_unblock(work) != 0)
			return (true);
		/* FALLTHROUGH */
	case WORK_ST_IDLE:
		work->work_queue = wq;
		taskqueue_enqueue(wq->taskqueue, &work->work_task);
		return (true);
	default:
		return (false);		/* already on a queue */
	}
}

/*
 * Callback func for linux_queue_rcu_work
 */
static void
rcu_work_func(struct rcu_head *rcu)
{
	struct rcu_work *rwork;

	rwork = container_of(rcu, struct rcu_work, rcu);
	linux_queue_work_on(WORK_CPU_UNBOUND, rwork->wq, &rwork->work);
}

/*
 * This function queue a work after a grace period
 * If the work was already pending it returns false,
 * if not it calls call_rcu and returns true.
 */
bool
linux_queue_rcu_work(struct workqueue_struct *wq, struct rcu_work *rwork)
{

	if (!linux_work_pending(&rwork->work)) {
		rwork->wq = wq;
		linux_call_rcu(RCU_TYPE_REGULAR, &rwork->rcu, rcu_work_func);
		return (true);
	}
	return (false);
}

/*
 * This function waits for the last execution of a work and then
 * flush the work.
 * It returns true if the work was pending and we waited, it returns
 * false otherwise.
 */
bool
linux_flush_rcu_work(struct rcu_work *rwork)
{

	if (linux_work_pending(&rwork->work)) {
		linux_rcu_barrier(RCU_TYPE_REGULAR);
		linux_flush_work(&rwork->work);
		return (true);
	}
	return (linux_flush_work(&rwork->work));
}

/*
 * This function queues the given work structure on the given
 * workqueue after a given delay in ticks. It returns true if the
 * work was successfully [re-]queued. Else the work is already pending
 * for completion.
 */
bool
linux_queue_delayed_work_on(int cpu, struct workqueue_struct *wq,
    struct delayed_work *dwork, unsigned long delay)
{
	static const uint8_t states[WORK_ST_MAX] __aligned(8) = {
		[WORK_ST_IDLE] = WORK_ST_TIMER,		/* start timeout */
		[WORK_ST_TIMER] = WORK_ST_TIMER,	/* NOP */
		[WORK_ST_TASK] = WORK_ST_TASK,		/* NOP */
		[WORK_ST_EXEC] = WORK_ST_TIMER,		/* start timeout */
		[WORK_ST_CANCEL] = WORK_ST_TIMER,	/* start timeout */
	};
	bool res;

	if (atomic_read(&wq->draining) != 0)
		return (!work_pending(&dwork->work));

	/*
	 * Clamp the delay to a valid ticks value, some consumers pass
	 * MAX_SCHEDULE_TIMEOUT.
	 */
	if (delay > INT_MAX)
		delay = INT_MAX;

	mtx_lock(&dwork->timer.mtx);
	switch (linux_update_state(&dwork->work.state, states)) {
	case WORK_ST_EXEC:
	case WORK_ST_CANCEL:
		if (delay == 0 && linux_work_exec_unblock(&dwork->work)) {
			dwork->timer.expires = jiffies;
			res = true;
			goto out;
		}
		/* FALLTHROUGH */
	case WORK_ST_IDLE:
		dwork->work.work_queue = wq;
		dwork->timer.expires = jiffies + delay;

		if (delay == 0) {
			linux_delayed_work_enqueue(dwork);
		} else if (unlikely(cpu != WORK_CPU_UNBOUND)) {
			callout_reset_on(&dwork->timer.callout, delay,
			    &linux_delayed_work_timer_fn, dwork, cpu);
		} else {
			callout_reset(&dwork->timer.callout, delay,
			    &linux_delayed_work_timer_fn, dwork);
		}
		res = true;
		break;
	default:
		res = false;
		break;
	}
out:
	mtx_unlock(&dwork->timer.mtx);
	return (res);
}

void
linux_work_fn(void *context, int pending)
{
	static const uint8_t states[WORK_ST_MAX] __aligned(8) = {
		[WORK_ST_IDLE] = WORK_ST_IDLE,		/* NOP */
		[WORK_ST_TIMER] = WORK_ST_EXEC,		/* delayed work w/o timeout */
		[WORK_ST_TASK] = WORK_ST_EXEC,		/* call callback */
		[WORK_ST_EXEC] = WORK_ST_IDLE,		/* complete callback */
		[WORK_ST_CANCEL] = WORK_ST_EXEC,	/* failed to cancel */
	};
	struct work_struct *work;
	struct workqueue_struct *wq;
	struct work_exec exec;
	struct task_struct *task;

	task = current;

	/* setup local variables */
	work = context;
	wq = work->work_queue;

	/* store target pointer */
	exec.target = work;

	/* insert executor into list */
	WQ_EXEC_LOCK(wq);
	TAILQ_INSERT_TAIL(&wq->exec_head, &exec, entry);
	while (1) {
		switch (linux_update_state(&work->state, states)) {
		case WORK_ST_TIMER:
		case WORK_ST_TASK:
		case WORK_ST_CANCEL:
			WQ_EXEC_UNLOCK(wq);

			/* set current work structure */
			task->work = work;

			/* call work function */
			work->func(work);

			/* set current work structure */
			task->work = NULL;

			WQ_EXEC_LOCK(wq);
			/* check if unblocked */
			if (exec.target != work) {
				/* reapply block */
				exec.target = work;
				break;
			}
			/* FALLTHROUGH */
		default:
			goto done;
		}
	}
done:
	/* remove executor from list */
	TAILQ_REMOVE(&wq->exec_head, &exec, entry);
	WQ_EXEC_UNLOCK(wq);
}

void
linux_delayed_work_fn(void *context, int pending)
{
	struct delayed_work *dwork = context;

	/*
	 * Make sure the timer belonging to the delayed work gets
	 * drained before invoking the work function. Else the timer
	 * mutex may still be in use which can lead to use-after-free
	 * situations, because the work function might free the work
	 * structure before returning.
	 */
	callout_drain(&dwork->timer.callout);

	linux_work_fn(&dwork->work, pending);
}

static void
linux_delayed_work_timer_fn(void *arg)
{
	static const uint8_t states[WORK_ST_MAX] __aligned(8) = {
		[WORK_ST_IDLE] = WORK_ST_IDLE,		/* NOP */
		[WORK_ST_TIMER] = WORK_ST_TASK,		/* start queueing task */
		[WORK_ST_TASK] = WORK_ST_TASK,		/* NOP */
		[WORK_ST_EXEC] = WORK_ST_EXEC,		/* NOP */
		[WORK_ST_CANCEL] = WORK_ST_TASK,	/* failed to cancel */
	};
	struct delayed_work *dwork = arg;

	switch (linux_update_state(&dwork->work.state, states)) {
	case WORK_ST_TIMER:
	case WORK_ST_CANCEL:
		linux_delayed_work_enqueue(dwork);
		break;
	default:
		break;
	}
}

/*
 * This function cancels the given work structure in a
 * non-blocking fashion. It returns non-zero if the work was
 * successfully cancelled. Else the work may still be busy or already
 * cancelled.
 */
bool
linux_cancel_work(struct work_struct *work)
{
	static const uint8_t states[WORK_ST_MAX] __aligned(8) = {
		[WORK_ST_IDLE] = WORK_ST_IDLE,		/* NOP */
		[WORK_ST_TIMER] = WORK_ST_TIMER,	/* can't happen */
		[WORK_ST_TASK] = WORK_ST_IDLE,		/* cancel */
		[WORK_ST_EXEC] = WORK_ST_EXEC,		/* NOP */
		[WORK_ST_CANCEL] = WORK_ST_IDLE,	/* can't happen */
	};
	struct taskqueue *tq;

	MPASS(atomic_read(&work->state) != WORK_ST_TIMER);
	MPASS(atomic_read(&work->state) != WORK_ST_CANCEL);

	switch (linux_update_state(&work->state, states)) {
	case WORK_ST_TASK:
		tq = work->work_queue->taskqueue;
		if (taskqueue_cancel(tq, &work->work_task, NULL) == 0)
			return (true);
		/* FALLTHROUGH */
	default:
		return (false);
	}
}

/*
 * This function cancels the given work structure in a synchronous
 * fashion. It returns non-zero if the work was successfully
 * cancelled. Else the work was already cancelled.
 */
bool
linux_cancel_work_sync(struct work_struct *work)
{
	static const uint8_t states[WORK_ST_MAX] __aligned(8) = {
		[WORK_ST_IDLE] = WORK_ST_IDLE,		/* NOP */
		[WORK_ST_TIMER] = WORK_ST_TIMER,	/* can't happen */
		[WORK_ST_TASK] = WORK_ST_IDLE,		/* cancel and drain */
		[WORK_ST_EXEC] = WORK_ST_IDLE,		/* too late, drain */
		[WORK_ST_CANCEL] = WORK_ST_IDLE,	/* cancel and drain */
	};
	struct taskqueue *tq;
	bool retval = false;

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "linux_cancel_work_sync() might sleep");
retry:
	switch (linux_update_state(&work->state, states)) {
	case WORK_ST_IDLE:
	case WORK_ST_TIMER:
		return (retval);
	case WORK_ST_EXEC:
		tq = work->work_queue->taskqueue;
		if (taskqueue_cancel(tq, &work->work_task, NULL) != 0)
			taskqueue_drain(tq, &work->work_task);
		goto retry;	/* work may have restarted itself */
	default:
		tq = work->work_queue->taskqueue;
		if (taskqueue_cancel(tq, &work->work_task, NULL) != 0)
			taskqueue_drain(tq, &work->work_task);
		retval = true;
		goto retry;
	}
}

/*
 * This function atomically stops the timer and callback. The timer
 * callback will not be called after this function returns. This
 * functions returns true when the timeout was cancelled. Else the
 * timeout was not started or has already been called.
 */
static inline bool
linux_cancel_timer(struct delayed_work *dwork, bool drain)
{
	bool cancelled;

	mtx_lock(&dwork->timer.mtx);
	cancelled = (callout_stop(&dwork->timer.callout) == 1);
	mtx_unlock(&dwork->timer.mtx);

	/* check if we should drain */
	if (drain)
		callout_drain(&dwork->timer.callout);
	return (cancelled);
}

/*
 * This function cancels the given delayed work structure in a
 * non-blocking fashion. It returns non-zero if the work was
 * successfully cancelled. Else the work may still be busy or already
 * cancelled.
 */
bool
linux_cancel_delayed_work(struct delayed_work *dwork)
{
	static const uint8_t states[WORK_ST_MAX] __aligned(8) = {
		[WORK_ST_IDLE] = WORK_ST_IDLE,		/* NOP */
		[WORK_ST_TIMER] = WORK_ST_CANCEL,	/* try to cancel */
		[WORK_ST_TASK] = WORK_ST_CANCEL,	/* try to cancel */
		[WORK_ST_EXEC] = WORK_ST_EXEC,		/* NOP */
		[WORK_ST_CANCEL] = WORK_ST_CANCEL,	/* NOP */
	};
	struct taskqueue *tq;
	bool cancelled;

	mtx_lock(&dwork->timer.mtx);
	switch (linux_update_state(&dwork->work.state, states)) {
	case WORK_ST_TIMER:
	case WORK_ST_CANCEL:
		cancelled = (callout_stop(&dwork->timer.callout) == 1);
		if (cancelled) {
			atomic_cmpxchg(&dwork->work.state,
			    WORK_ST_CANCEL, WORK_ST_IDLE);
			mtx_unlock(&dwork->timer.mtx);
			return (true);
		}
		/* FALLTHROUGH */
	case WORK_ST_TASK:
		tq = dwork->work.work_queue->taskqueue;
		if (taskqueue_cancel(tq, &dwork->work.work_task, NULL) == 0) {
			atomic_cmpxchg(&dwork->work.state,
			    WORK_ST_CANCEL, WORK_ST_IDLE);
			mtx_unlock(&dwork->timer.mtx);
			return (true);
		}
		/* FALLTHROUGH */
	default:
		mtx_unlock(&dwork->timer.mtx);
		return (false);
	}
}

/*
 * This function cancels the given work structure in a synchronous
 * fashion. It returns true if the work was successfully
 * cancelled. Else the work was already cancelled.
 */
static bool
linux_cancel_delayed_work_sync_int(struct delayed_work *dwork)
{
	static const uint8_t states[WORK_ST_MAX] __aligned(8) = {
		[WORK_ST_IDLE] = WORK_ST_IDLE,		/* NOP */
		[WORK_ST_TIMER] = WORK_ST_IDLE,		/* cancel and drain */
		[WORK_ST_TASK] = WORK_ST_IDLE,		/* cancel and drain */
		[WORK_ST_EXEC] = WORK_ST_IDLE,		/* too late, drain */
		[WORK_ST_CANCEL] = WORK_ST_IDLE,	/* cancel and drain */
	};
	struct taskqueue *tq;
	int ret, state;
	bool cancelled;

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "linux_cancel_delayed_work_sync() might sleep");
	mtx_lock(&dwork->timer.mtx);

	state = linux_update_state(&dwork->work.state, states);
	switch (state) {
	case WORK_ST_IDLE:
		mtx_unlock(&dwork->timer.mtx);
		return (false);
	case WORK_ST_TIMER:
	case WORK_ST_CANCEL:
		cancelled = (callout_stop(&dwork->timer.callout) == 1);

		tq = dwork->work.work_queue->taskqueue;
		ret = taskqueue_cancel(tq, &dwork->work.work_task, NULL);
		mtx_unlock(&dwork->timer.mtx);

		callout_drain(&dwork->timer.callout);
		taskqueue_drain(tq, &dwork->work.work_task);
		return (cancelled || (ret != 0));
	default:
		tq = dwork->work.work_queue->taskqueue;
		ret = taskqueue_cancel(tq, &dwork->work.work_task, NULL);
		mtx_unlock(&dwork->timer.mtx);
		if (ret != 0)
			taskqueue_drain(tq, &dwork->work.work_task);
		return (ret != 0);
	}
}

bool
linux_cancel_delayed_work_sync(struct delayed_work *dwork)
{
	bool res;

	res = false;
	while (linux_cancel_delayed_work_sync_int(dwork))
		res = true;
	return (res);
}

/*
 * This function waits until the given work structure is completed.
 * It returns non-zero if the work was successfully
 * waited for. Else the work was not waited for.
 */
bool
linux_flush_work(struct work_struct *work)
{
	struct taskqueue *tq;
	bool retval;

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "linux_flush_work() might sleep");

	switch (atomic_read(&work->state)) {
	case WORK_ST_IDLE:
		return (false);
	default:
		tq = work->work_queue->taskqueue;
		retval = taskqueue_poll_is_busy(tq, &work->work_task);
		taskqueue_drain(tq, &work->work_task);
		return (retval);
	}
}

/*
 * This function waits until the given delayed work structure is
 * completed. It returns non-zero if the work was successfully waited
 * for. Else the work was not waited for.
 */
bool
linux_flush_delayed_work(struct delayed_work *dwork)
{
	struct taskqueue *tq;
	bool retval;

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
	    "linux_flush_delayed_work() might sleep");

	switch (atomic_read(&dwork->work.state)) {
	case WORK_ST_IDLE:
		return (false);
	case WORK_ST_TIMER:
		if (linux_cancel_timer(dwork, 1))
			linux_delayed_work_enqueue(dwork);
		/* FALLTHROUGH */
	default:
		tq = dwork->work.work_queue->taskqueue;
		retval = taskqueue_poll_is_busy(tq, &dwork->work.work_task);
		taskqueue_drain(tq, &dwork->work.work_task);
		return (retval);
	}
}

/*
 * This function returns true if the given work is pending, and not
 * yet executing:
 */
bool
linux_work_pending(struct work_struct *work)
{
	switch (atomic_read(&work->state)) {
	case WORK_ST_TIMER:
	case WORK_ST_TASK:
	case WORK_ST_CANCEL:
		return (true);
	default:
		return (false);
	}
}

/*
 * This function returns true if the given work is busy.
 */
bool
linux_work_busy(struct work_struct *work)
{
	struct taskqueue *tq;

	switch (atomic_read(&work->state)) {
	case WORK_ST_IDLE:
		return (false);
	case WORK_ST_EXEC:
		tq = work->work_queue->taskqueue;
		return (taskqueue_poll_is_busy(tq, &work->work_task));
	default:
		return (true);
	}
}

struct workqueue_struct *
linux_create_workqueue_common(const char *name, int cpus)
{
	struct workqueue_struct *wq;

	/*
	 * If zero CPUs are specified use the default number of CPUs:
	 */
	if (cpus == 0)
		cpus = linux_default_wq_cpus;

	wq = kmalloc(sizeof(*wq), M_WAITOK | M_ZERO);
	wq->taskqueue = taskqueue_create(name, M_WAITOK,
	    taskqueue_thread_enqueue, &wq->taskqueue);
	atomic_set(&wq->draining, 0);
	taskqueue_start_threads(&wq->taskqueue, cpus, PWAIT, "%s", name);
	TAILQ_INIT(&wq->exec_head);
	mtx_init(&wq->exec_mtx, "linux_wq_exec", NULL, MTX_DEF);

	return (wq);
}

void
linux_destroy_workqueue(struct workqueue_struct *wq)
{
	atomic_inc(&wq->draining);
	drain_workqueue(wq);
	taskqueue_free(wq->taskqueue);
	mtx_destroy(&wq->exec_mtx);
	kfree(wq);
}

void
linux_init_delayed_work(struct delayed_work *dwork, work_func_t func)
{
	memset(dwork, 0, sizeof(*dwork));
	dwork->work.func = func;
	TASK_INIT(&dwork->work.work_task, 0, linux_delayed_work_fn, dwork);
	mtx_init(&dwork->timer.mtx, spin_lock_name("lkpi-dwork"), NULL,
	    MTX_DEF | MTX_NOWITNESS);
	callout_init_mtx(&dwork->timer.callout, &dwork->timer.mtx, 0);
}

struct work_struct *
linux_current_work(void)
{
	return (current->work);
}

static void
linux_work_init(void *arg)
{
	int max_wq_cpus = mp_ncpus + 1;

	/* avoid deadlock when there are too few threads */
	if (max_wq_cpus < 4)
		max_wq_cpus = 4;

	/* set default number of CPUs */
	linux_default_wq_cpus = max_wq_cpus;

	linux_system_short_wq = alloc_workqueue("linuxkpi_short_wq", 0, max_wq_cpus);
	linux_system_long_wq = alloc_workqueue("linuxkpi_long_wq", 0, max_wq_cpus);

	/* populate the workqueue pointers */
	system_long_wq = linux_system_long_wq;
	system_wq = linux_system_short_wq;
	system_power_efficient_wq = linux_system_short_wq;
	system_unbound_wq = linux_system_short_wq;
	system_highpri_wq = linux_system_short_wq;
}
SYSINIT(linux_work_init, SI_SUB_TASKQ, SI_ORDER_THIRD, linux_work_init, NULL);

static void
linux_work_uninit(void *arg)
{
	destroy_workqueue(linux_system_short_wq);
	destroy_workqueue(linux_system_long_wq);

	/* clear workqueue pointers */
	system_long_wq = NULL;
	system_wq = NULL;
	system_power_efficient_wq = NULL;
	system_unbound_wq = NULL;
	system_highpri_wq = NULL;
}
SYSUNINIT(linux_work_uninit, SI_SUB_TASKQ, SI_ORDER_THIRD, linux_work_uninit, NULL);

void
linux_irq_work_fn(void *context, int pending)
{
	struct irq_work *irqw = context;

	irqw->func(irqw);
}

static void
linux_irq_work_init_fn(void *context, int pending)
{
	/*
	 * LinuxKPI performs lazy allocation of memory structures required by
	 * current on the first access to it.  As some irq_work clients read
	 * it with spinlock taken, we have to preallocate td_lkpi_task before
	 * first call to irq_work_queue().  As irq_work uses a single thread,
	 * it is enough to read current once at SYSINIT stage.
	 */
	if (current == NULL)
		panic("irq_work taskqueue is not initialized");
}
static struct task linux_irq_work_init_task =
    TASK_INITIALIZER(0, linux_irq_work_init_fn, &linux_irq_work_init_task);

static void
linux_irq_work_init(void *arg)
{
	linux_irq_work_tq = taskqueue_create_fast("linuxkpi_irq_wq",
	    M_WAITOK, taskqueue_thread_enqueue, &linux_irq_work_tq);
	taskqueue_start_threads(&linux_irq_work_tq, 1, PWAIT,
	    "linuxkpi_irq_wq");
	taskqueue_enqueue(linux_irq_work_tq, &linux_irq_work_init_task);
}
SYSINIT(linux_irq_work_init, SI_SUB_TASKQ, SI_ORDER_SECOND,
    linux_irq_work_init, NULL);

static void
linux_irq_work_uninit(void *arg)
{
	taskqueue_drain_all(linux_irq_work_tq);
	taskqueue_free(linux_irq_work_tq);
}
SYSUNINIT(linux_irq_work_uninit, SI_SUB_TASKQ, SI_ORDER_SECOND,
    linux_irq_work_uninit, NULL);
