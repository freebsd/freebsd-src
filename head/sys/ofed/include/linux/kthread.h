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
#ifndef	_LINUX_KTHREAD_H_
#define	_LINUX_KTHREAD_H_

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/sleepqueue.h>

#include <linux/slab.h>
#include <linux/sched.h>

static inline void
_kthread_fn(void *arg)
{
	struct task_struct *task;

	task = arg;
	task_struct_set(curthread, task);
	if (task->should_stop == 0)
		task->task_ret = task->task_fn(task->task_data);
	PROC_LOCK(task->task_thread->td_proc);
	task->should_stop = TASK_STOPPED;
	wakeup(task);
	PROC_UNLOCK(task->task_thread->td_proc);
	kthread_exit();
}

static inline struct task_struct *
_kthread_create(int (*threadfn)(void *data), void *data)
{
	struct task_struct *task;

	task = kzalloc(sizeof(*task), GFP_KERNEL);
	task->task_fn = threadfn;
	task->task_data = data;

	return (task);
}

struct task_struct *kthread_create(int (*threadfn)(void *data),
                                   void *data,
                                   const char namefmt[], ...)
        __attribute__((format(printf, 3, 4)));

#define	kthread_run(fn, data, fmt, ...)					\
({									\
	struct task_struct *_task;					\
									\
	_task = _kthread_create((fn), (data));				\
	if (kthread_add(_kthread_fn, _task, NULL, &_task->task_thread,	\
	    0, 0, fmt, ## __VA_ARGS__)) {				\
		kfree(_task);						\
		_task = NULL;						\
	} else								\
		task_struct_set(_task->task_thread, _task);		\
	_task;								\
})

#define	kthread_should_stop()	current->should_stop

static inline int
kthread_stop(struct task_struct *task)
{

	PROC_LOCK(task->task_thread->td_proc);
	task->should_stop = TASK_SHOULD_STOP;
	wake_up_process(task);
	while (task->should_stop != TASK_STOPPED)
		msleep(task, &task->task_thread->td_proc->p_mtx, PWAIT,
		    "kstop", hz);
	PROC_UNLOCK(task->task_thread->td_proc);
	return task->task_ret;
}

#endif	/* _LINUX_KTHREAD_H_ */
