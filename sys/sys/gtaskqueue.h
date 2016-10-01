/*-
 * Copyright (c) 2014 Jeffrey Roberson <jeff@freebsd.org>
 * Copyright (c) 2016 Matthew Macy <mmacy@nextbsd.org>
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

#ifndef _SYS_GTASKQUEUE_H_
#define _SYS_GTASKQUEUE_H_
#include <sys/taskqueue.h>

#ifndef _KERNEL
#error "no user-servicable parts inside"
#endif

struct gtaskqueue;
typedef void (*gtaskqueue_enqueue_fn)(void *context);

/*
 * Taskqueue groups.  Manages dynamic thread groups and irq binding for
 * device and other tasks.
 */

void	gtaskqueue_block(struct gtaskqueue *queue);
void	gtaskqueue_unblock(struct gtaskqueue *queue);

int	gtaskqueue_cancel(struct gtaskqueue *queue, struct gtask *gtask);
void	gtaskqueue_drain(struct gtaskqueue *queue, struct gtask *task);
void	gtaskqueue_drain_all(struct gtaskqueue *queue);

int grouptaskqueue_enqueue(struct gtaskqueue *queue, struct gtask *task);
void	taskqgroup_attach(struct taskqgroup *qgroup, struct grouptask *grptask,
	    void *uniq, int irq, char *name);
int		taskqgroup_attach_cpu(struct taskqgroup *qgroup, struct grouptask *grptask,
		void *uniq, int cpu, int irq, char *name);
void	taskqgroup_detach(struct taskqgroup *qgroup, struct grouptask *gtask);
struct taskqgroup *taskqgroup_create(char *name);
void	taskqgroup_destroy(struct taskqgroup *qgroup);
int	taskqgroup_adjust(struct taskqgroup *qgroup, int cnt, int stride);

#define TASK_ENQUEUED			0x1
#define TASK_SKIP_WAKEUP		0x2


#define GTASK_INIT(task, flags, priority, func, context) do {	\
	(task)->ta_flags = flags;				\
	(task)->ta_priority = (priority);		\
	(task)->ta_func = (func);			\
	(task)->ta_context = (context);			\
} while (0)

#define	GROUPTASK_INIT(gtask, priority, func, context)	\
	GTASK_INIT(&(gtask)->gt_task, TASK_SKIP_WAKEUP, priority, func, context)

#define	GROUPTASK_ENQUEUE(gtask)			\
	grouptaskqueue_enqueue((gtask)->gt_taskqueue, &(gtask)->gt_task)

#define TASKQGROUP_DECLARE(name)			\
extern struct taskqgroup *qgroup_##name


#ifdef EARLY_AP_STARTUP
#define TASKQGROUP_DEFINE(name, cnt, stride)				\
									\
struct taskqgroup *qgroup_##name;					\
									\
static void								\
taskqgroup_define_##name(void *arg)					\
{									\
	qgroup_##name = taskqgroup_create(#name);			\
	taskqgroup_adjust(qgroup_##name, (cnt), (stride));		\
}									\
									\
SYSINIT(taskqgroup_##name, SI_SUB_INIT_IF, SI_ORDER_FIRST,		\
	taskqgroup_define_##name, NULL)
#else
#define TASKQGROUP_DEFINE(name, cnt, stride)				\
									\
struct taskqgroup *qgroup_##name;					\
									\
static void								\
taskqgroup_define_##name(void *arg)					\
{									\
	qgroup_##name = taskqgroup_create(#name);			\
}									\
									\
SYSINIT(taskqgroup_##name, SI_SUB_INIT_IF, SI_ORDER_FIRST,		\
	taskqgroup_define_##name, NULL);				\
									\
static void								\
taskqgroup_adjust_##name(void *arg)					\
{									\
	taskqgroup_adjust(qgroup_##name, (cnt), (stride));		\
}									\
									\
SYSINIT(taskqgroup_adj_##name, SI_SUB_SMP, SI_ORDER_ANY,		\
	taskqgroup_adjust_##name, NULL);				\
									\
struct __hack
#endif
TASKQGROUP_DECLARE(net);

#endif /* !_SYS_GTASKQUEUE_H_ */
