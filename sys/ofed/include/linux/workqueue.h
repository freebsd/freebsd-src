/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
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
#ifndef	_LINUX_WORKQUEUE_H_
#define	_LINUX_WORKQUEUE_H_

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/slab.h>

#include <sys/taskqueue.h>

struct workqueue_struct {
	struct taskqueue	*taskqueue;
};

struct work_struct {
	struct	task 		work_task;
	struct	taskqueue	*taskqueue;
	void			(*fn)(struct work_struct *);
};

struct delayed_work {
	struct work_struct	work;
	struct callout		timer;
};

static inline struct delayed_work *
to_delayed_work(struct work_struct *work)
{

 	return container_of(work, struct delayed_work, work);
}


static inline void
_work_fn(void *context, int pending)
{
	struct work_struct *work;

	work = context;
	work->fn(work);
}

#define	INIT_WORK(work, func) 	 					\
do {									\
	(work)->fn = (func);						\
	(work)->taskqueue = NULL;					\
	TASK_INIT(&(work)->work_task, 0, _work_fn, (work));		\
} while (0)

#define	INIT_DELAYED_WORK(_work, func)					\
do {									\
	INIT_WORK(&(_work)->work, func);				\
	callout_init(&(_work)->timer, CALLOUT_MPSAFE);			\
} while (0)

#define	INIT_DELAYED_WORK_DEFERRABLE	INIT_DELAYED_WORK

#define	schedule_work(work)						\
do {									\
	(work)->taskqueue = taskqueue_thread;				\
	taskqueue_enqueue(taskqueue_thread, &(work)->work_task);	\
} while (0)

#define	flush_scheduled_work()	flush_taskqueue(taskqueue_thread)

#define	queue_work(q, work)						\
do {									\
	(work)->taskqueue = (q)->taskqueue;				\
	taskqueue_enqueue((q)->taskqueue, &(work)->work_task);		\
} while (0)

static inline void
_delayed_work_fn(void *arg)
{
	struct delayed_work *work;

	work = arg;
	taskqueue_enqueue(work->work.taskqueue, &work->work.work_task);
}

static inline int
queue_delayed_work(struct workqueue_struct *wq, struct delayed_work *work,
    unsigned long delay)
{
	int pending;

	pending = work->work.work_task.ta_pending;
	work->work.taskqueue = wq->taskqueue;
	if (delay != 0)
		callout_reset(&work->timer, delay, _delayed_work_fn, work);
	else
		_delayed_work_fn((void *)work);

	return (!pending);
}

static inline struct workqueue_struct *
_create_workqueue_common(char *name, int cpus)
{
	struct workqueue_struct *wq;

	wq = kmalloc(sizeof(*wq), M_WAITOK);
	wq->taskqueue = taskqueue_create((name), M_WAITOK,
	    taskqueue_thread_enqueue,  &wq->taskqueue);
	taskqueue_start_threads(&wq->taskqueue, cpus, PWAIT, (name));

	return (wq);
}


#define	create_singlethread_workqueue(name)				\
	_create_workqueue_common(name, 1)

#define	create_workqueue(name)						\
	_create_workqueue_common(name, MAXCPU)

static inline void
destroy_workqueue(struct workqueue_struct *wq)
{
	taskqueue_free(wq->taskqueue);
	kfree(wq);
}

#define	flush_workqueue(wq)	flush_taskqueue((wq)->taskqueue)

static inline void
_flush_fn(void *context, int pending)
{
}

static inline void
flush_taskqueue(struct taskqueue *tq)
{
	struct task flushtask;

	TASK_INIT(&flushtask, 0, _flush_fn, NULL);
	taskqueue_enqueue(tq, &flushtask);
	taskqueue_drain(tq, &flushtask);
}

#define	cancel_work_sync(work)						\
	(work)->taskqueue ?						\
	taskqueue_cancel((work)->taskqueue, &(work)->work_task, 1) : 0

/*
 * This may leave work running on another CPU as it does on Linux.
 */
static inline int
cancel_delayed_work(struct delayed_work *work)
{
	int error;

	callout_stop(&work->timer);
	if (work->work.taskqueue)
		error = taskqueue_cancel(work->work.taskqueue,
		    &work->work.work_task, 0);
	return error;
}

#endif	/* _LINUX_WORKQUEUE_H_ */
