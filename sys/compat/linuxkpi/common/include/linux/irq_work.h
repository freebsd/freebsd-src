/*-
 * Copyright (c) 2020 The FreeBSD Foundation
 *
 * This software was developed by Emmanuel Vadot under sponsorship
 * from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef _LINUXKPI_LINUX_IRQ_WORK_H_
#define	_LINUXKPI_LINUX_IRQ_WORK_H_

#include <sys/param.h>
#include <sys/taskqueue.h>

#include <linux/llist.h>
#include <linux/workqueue.h>

#define	LKPI_IRQ_WORK_STD_TQ	system_wq->taskqueue
#define	LKPI_IRQ_WORK_FAST_TQ	linux_irq_work_tq

#ifdef LKPI_IRQ_WORK_USE_FAST_TQ
#define	LKPI_IRQ_WORK_TQ	LKPI_IRQ_WORK_FAST_TQ
#else
#define	LKPI_IRQ_WORK_TQ	LKPI_IRQ_WORK_STD_TQ
#endif

struct irq_work;
typedef void (*irq_work_func_t)(struct irq_work *);

struct irq_work {
	struct task irq_task;
	struct llist_node llnode;
	irq_work_func_t func;
};

extern struct taskqueue *linux_irq_work_tq;

#define	DEFINE_IRQ_WORK(name, _func)	struct irq_work name = {	\
	.irq_task = TASK_INITIALIZER(0, linux_irq_work_fn, &(name)),	\
	.func  = (_func),						\
}

void	linux_irq_work_fn(void *, int);

static inline void
init_irq_work(struct irq_work *irqw, irq_work_func_t func)
{
	TASK_INIT(&irqw->irq_task, 0, linux_irq_work_fn, irqw);
	irqw->func = func;
}

static inline bool
irq_work_queue(struct irq_work *irqw)
{
	return (taskqueue_enqueue_flags(LKPI_IRQ_WORK_TQ, &irqw->irq_task,
	    TASKQUEUE_FAIL_IF_PENDING) == 0);
}

static inline void
irq_work_sync(struct irq_work *irqw)
{
	taskqueue_drain(LKPI_IRQ_WORK_TQ, &irqw->irq_task);
}

#endif /* _LINUXKPI_LINUX_IRQ_WORK_H_ */
