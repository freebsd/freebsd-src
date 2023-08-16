/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2017 Mellanox Technologies, Ltd.
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
#ifndef	_LINUXKPI_LINUX_KTHREAD_H_
#define	_LINUXKPI_LINUX_KTHREAD_H_

#include <linux/sched.h>

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/unistd.h>

struct task_struct;
struct kthread_work;

typedef void (*kthread_work_func_t)(struct kthread_work *work);

struct kthread_worker {
	struct task_struct	*task;
	struct taskqueue	*tq;
};

struct kthread_work {
	struct taskqueue	*tq;
	struct task		task;
	kthread_work_func_t	func;
};

#define	kthread_run(fn, data, fmt, ...)	({				\
	struct task_struct *__task;					\
	struct thread *__td;						\
									\
	if (kthread_add(linux_kthread_fn, NULL, NULL, &__td,		\
	    RFSTOPPED, 0, fmt, ## __VA_ARGS__))				\
		__task = NULL;						\
	else								\
		__task = linux_kthread_setup_and_run(__td, fn, data);	\
	__task;								\
})

int linux_kthread_stop(struct task_struct *);
bool linux_kthread_should_stop_task(struct task_struct *);
bool linux_kthread_should_stop(void);
int linux_kthread_park(struct task_struct *);
void linux_kthread_parkme(void);
bool linux_kthread_should_park(void);
void linux_kthread_unpark(struct task_struct *);
void linux_kthread_fn(void *);
struct task_struct *linux_kthread_setup_and_run(struct thread *,
    linux_task_fn_t *, void *arg);
int linux_in_atomic(void);

#define	kthread_stop(task)		linux_kthread_stop(task)
#define	kthread_should_stop()		linux_kthread_should_stop()
#define	kthread_should_stop_task(task)	linux_kthread_should_stop_task(task)
#define	kthread_park(task)		linux_kthread_park(task)
#define	kthread_parkme()		linux_kthread_parkme()
#define	kthread_should_park()		linux_kthread_should_park()
#define	kthread_unpark(task)		linux_kthread_unpark(task)

#define	in_atomic()			linux_in_atomic()

/* Only kthread_(create|destroy)_worker interface is allowed */
#define	kthread_init_worker(worker)	\
	_Static_assert(false, "pre-4.9 worker interface is not supported");

task_fn_t lkpi_kthread_work_fn;
task_fn_t lkpi_kthread_worker_init_fn;

#define kthread_create_worker(flags, fmt, ...) ({			\
	struct kthread_worker *__w;					\
	struct task __task;						\
									\
	__w = malloc(sizeof(*__w), M_KMALLOC, M_WAITOK | M_ZERO);	\
	__w->tq = taskqueue_create("lkpi kthread taskq", M_WAITOK,	\
	    taskqueue_thread_enqueue, &__w->tq);			\
	taskqueue_start_threads(&__w->tq, 1, PWAIT, fmt, ##__VA_ARGS__);\
	TASK_INIT(&__task, 0, lkpi_kthread_worker_init_fn, __w);	\
	taskqueue_enqueue(__w->tq, &__task);				\
	taskqueue_drain(__w->tq, &__task);				\
	__w;								\
})

static inline void
kthread_destroy_worker(struct kthread_worker *worker)
{
	taskqueue_drain_all(worker->tq);
	taskqueue_free(worker->tq);
	free(worker, M_KMALLOC);
}

static inline void
kthread_init_work(struct kthread_work *work, kthread_work_func_t func)
{
	work->tq = NULL;
	work->func = func;
	TASK_INIT(&work->task, 0, lkpi_kthread_work_fn, work);
}

static inline bool
kthread_queue_work(struct kthread_worker *worker, struct kthread_work *work)
{
	int error;

	error = taskqueue_enqueue_flags(worker->tq, &work->task,
	    TASKQUEUE_FAIL_IF_CANCELING | TASKQUEUE_FAIL_IF_PENDING);
	if (error == 0)
		work->tq = worker->tq;
	return (error == 0);
}

static inline bool
kthread_cancel_work_sync(struct kthread_work *work)
{
	u_int pending = 0;

	if (work->tq != NULL &&
	    taskqueue_cancel(work->tq, &work->task, &pending) != 0)
		taskqueue_drain(work->tq, &work->task);

	return (pending != 0);
}

static inline void
kthread_flush_work(struct kthread_work *work)
{
	if (work->tq != NULL)
		taskqueue_drain(work->tq, &work->task);
}

static inline void
kthread_flush_worker(struct kthread_worker *worker)
{
	taskqueue_drain_all(worker->tq);
}

#endif /* _LINUXKPI_LINUX_KTHREAD_H_ */
