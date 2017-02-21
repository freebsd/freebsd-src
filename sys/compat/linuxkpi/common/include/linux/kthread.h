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
 *
 * $FreeBSD$
 */
#ifndef	_LINUX_KTHREAD_H_
#define	_LINUX_KTHREAD_H_

#include <linux/sched.h>

#include <sys/unistd.h>
#include <sys/kthread.h>

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

extern int kthread_stop(struct task_struct *);
extern bool kthread_should_stop_task(struct task_struct *);
extern bool kthread_should_stop(void);
extern void linux_kthread_fn(void *);
extern struct task_struct *linux_kthread_setup_and_run(struct thread *, linux_task_fn_t *, void *arg);

#endif	/* _LINUX_KTHREAD_H_ */
