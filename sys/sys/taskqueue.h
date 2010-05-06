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
 *
 * $FreeBSD$
 */

#ifndef _SYS_TASKQUEUE_H_
#define _SYS_TASKQUEUE_H_

#ifndef _KERNEL
#error "no user-servicable parts inside"
#endif

#include <sys/queue.h>
#include <sys/_task.h>

struct taskqueue;
struct thread;

/*
 * A notification callback function which is called from
 * taskqueue_enqueue().  The context argument is given in the call to
 * taskqueue_create().  This function would normally be used to allow the
 * queue to arrange to run itself later (e.g., by scheduling a software
 * interrupt or waking a kernel thread).
 */
typedef void (*taskqueue_enqueue_fn)(void *context);

struct taskqueue *taskqueue_create(const char *name, int mflags,
				    taskqueue_enqueue_fn enqueue,
				    void *context);
int	taskqueue_start_threads(struct taskqueue **tqp, int count, int pri,
				const char *name, ...) __printflike(4, 5);
int	taskqueue_enqueue(struct taskqueue *queue, struct task *task);
void	taskqueue_drain(struct taskqueue *queue, struct task *task);
void	taskqueue_free(struct taskqueue *queue);
void	taskqueue_run(struct taskqueue *queue);
void	taskqueue_block(struct taskqueue *queue);
void	taskqueue_unblock(struct taskqueue *queue);
int	taskqueue_member(struct taskqueue *queue, struct thread *td);

/*
 * Functions for dedicated thread taskqueues
 */
void	taskqueue_thread_loop(void *arg);
void	taskqueue_thread_enqueue(void *context);

/*
 * Initialise a task structure.
 */
#define TASK_INIT(task, priority, func, context) do {	\
	(task)->ta_pending = 0;				\
	(task)->ta_priority = (priority);		\
	(task)->ta_func = (func);			\
	(task)->ta_context = (context);			\
	(task)->ta_flags = 0;				\
} while (0)

/*
 * Declare a reference to a taskqueue.
 */
#define TASKQUEUE_DECLARE(name)			\
extern struct taskqueue *taskqueue_##name

/*
 * Define and initialise a global taskqueue that uses sleep mutexes.
 */
#define TASKQUEUE_DEFINE(name, enqueue, context, init)			\
									\
struct taskqueue *taskqueue_##name;					\
									\
static void								\
taskqueue_define_##name(void *arg)					\
{									\
	taskqueue_##name =						\
	    taskqueue_create(#name, M_NOWAIT, (enqueue), (context));	\
	init;								\
}									\
									\
SYSINIT(taskqueue_##name, SI_SUB_CONFIGURE, SI_ORDER_SECOND,		\
	taskqueue_define_##name, NULL);					\
									\
struct __hack
#define TASKQUEUE_DEFINE_THREAD(name)					\
TASKQUEUE_DEFINE(name, taskqueue_thread_enqueue, &taskqueue_##name,	\
	taskqueue_start_threads(&taskqueue_##name, 1, PWAIT,		\
	"%s taskq", #name))

/*
 * Define and initialise a global taskqueue that uses spin mutexes.
 */
#define TASKQUEUE_FAST_DEFINE(name, enqueue, context, init)		\
									\
struct taskqueue *taskqueue_##name;					\
									\
static void								\
taskqueue_define_##name(void *arg)					\
{									\
	taskqueue_##name =						\
	    taskqueue_create_fast(#name, M_NOWAIT, (enqueue),		\
	    (context));							\
	init;								\
}									\
									\
SYSINIT(taskqueue_##name, SI_SUB_CONFIGURE, SI_ORDER_SECOND,		\
	taskqueue_define_##name, NULL);					\
									\
struct __hack
#define TASKQUEUE_FAST_DEFINE_THREAD(name)				\
TASKQUEUE_FAST_DEFINE(name, taskqueue_thread_enqueue,			\
	&taskqueue_##name, taskqueue_start_threads(&taskqueue_##name	\
	1, PWAIT, "%s taskq", #name))

/*
 * These queues are serviced by software interrupt handlers.  To enqueue
 * a task, call taskqueue_enqueue(taskqueue_swi, &task) or
 * taskqueue_enqueue(taskqueue_swi_giant, &task).
 */
TASKQUEUE_DECLARE(swi_giant);
TASKQUEUE_DECLARE(swi);

/*
 * This queue is serviced by a kernel thread.  To enqueue a task, call
 * taskqueue_enqueue(taskqueue_thread, &task).
 */
TASKQUEUE_DECLARE(thread);

/*
 * Queue for swi handlers dispatched from fast interrupt handlers.
 * These are necessarily different from the above because the queue
 * must be locked with spinlocks since sleep mutex's cannot be used
 * from a fast interrupt handler context.
 */
TASKQUEUE_DECLARE(fast);
int	taskqueue_enqueue_fast(struct taskqueue *queue, struct task *task);
struct taskqueue *taskqueue_create_fast(const char *name, int mflags,
				    taskqueue_enqueue_fn enqueue,
				    void *context);

#endif /* !_SYS_TASKQUEUE_H_ */
