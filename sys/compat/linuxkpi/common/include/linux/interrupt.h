/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2015 Mellanox Technologies, Ltd.
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
#ifndef	_LINUXKPI_LINUX_INTERRUPT_H_
#define	_LINUXKPI_LINUX_INTERRUPT_H_

#include <linux/cpu.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/irqreturn.h>
#include <linux/hardirq.h>

#include <sys/param.h>
#include <sys/interrupt.h>

typedef	irqreturn_t	(*irq_handler_t)(int, void *);

#define	IRQF_SHARED	RF_SHAREABLE
#define	IRQF_NOBALANCING	0

#define	IRQ_DISABLE_UNLAZY	0

#define	IRQ_NOTCONNECTED	(1U << 31)

struct irq_ent;

void linux_irq_handler(void *);
void lkpi_devm_irq_release(struct device *, void *);
void lkpi_irq_release(struct device *, struct irq_ent *);
int  lkpi_request_irq(struct device *, unsigned int, irq_handler_t,
	irq_handler_t, unsigned long, const char *, void *);
int  lkpi_enable_irq(unsigned int);
void lkpi_disable_irq(unsigned int);
int  lkpi_bind_irq_to_cpu(unsigned int, int);
void lkpi_free_irq(unsigned int, void *);
void lkpi_devm_free_irq(struct device *, unsigned int, void *);

static inline int
request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
    const char *name, void *arg)
{

	return (lkpi_request_irq(NULL, irq, handler, NULL, flags, name, arg));
}

static inline int
request_threaded_irq(int irq, irq_handler_t handler,
    irq_handler_t thread_handler, unsigned long flags,
    const char *name, void *arg)
{

	return (lkpi_request_irq(NULL, irq, handler, thread_handler,
	    flags, name, arg));
}

static inline int
devm_request_irq(struct device *dev, int irq,
    irq_handler_t handler, unsigned long flags, const char *name, void *arg)
{

	return (lkpi_request_irq(dev, irq, handler, NULL, flags, name, arg));
}

static inline int
devm_request_threaded_irq(struct device *dev, int irq,
    irq_handler_t handler, irq_handler_t thread_handler,
    unsigned long flags, const char *name, void *arg)
{

	return (lkpi_request_irq(dev, irq, handler, thread_handler,
	    flags, name, arg));
}

static inline int
enable_irq(unsigned int irq)
{
	return (lkpi_enable_irq(irq));
}

static inline void
disable_irq(unsigned int irq)
{
	lkpi_disable_irq(irq);
}

static inline void
disable_irq_nosync(unsigned int irq)
{
	lkpi_disable_irq(irq);
}

static inline int
bind_irq_to_cpu(unsigned int irq, int cpu_id)
{
	return (lkpi_bind_irq_to_cpu(irq, cpu_id));
}

static inline void
free_irq(unsigned int irq, void *device)
{
	lkpi_free_irq(irq, device);
}

static inline void
devm_free_irq(struct device *xdev, unsigned int irq, void *p)
{
	lkpi_devm_free_irq(xdev, irq, p);
}

static inline int
irq_set_affinity_hint(int vector, const cpumask_t *mask)
{
	int error;

	if (mask != NULL)
		error = intr_setaffinity(vector, CPU_WHICH_IRQ, __DECONST(cpumask_t *, mask));
	else
		error = intr_setaffinity(vector, CPU_WHICH_IRQ, cpuset_root);

	return (-error);
}

static inline struct msi_desc *
irq_get_msi_desc(unsigned int irq)
{

	return (lkpi_pci_msi_desc_alloc(irq));
}

static inline void
irq_set_status_flags(unsigned int irq __unused, unsigned long flags __unused)
{
}

/*
 * LinuxKPI tasklet support
 */
typedef void tasklet_func_t(unsigned long);

struct tasklet_struct {
	TAILQ_ENTRY(tasklet_struct) entry;
	tasklet_func_t *func;
	/* Our "state" implementation is different. Avoid same name as Linux. */
	volatile u_int tasklet_state;
	atomic_t count;
	unsigned long data;
};

#define	DECLARE_TASKLET(_name, _func, _data)	\
struct tasklet_struct _name = { .func = (_func), .data = (_data) }

#define	tasklet_hi_schedule(t)	tasklet_schedule(t)

extern void tasklet_schedule(struct tasklet_struct *);
extern void tasklet_kill(struct tasklet_struct *);
extern void tasklet_init(struct tasklet_struct *, tasklet_func_t *,
    unsigned long data);
extern void tasklet_enable(struct tasklet_struct *);
extern void tasklet_disable(struct tasklet_struct *);
extern void tasklet_disable_nosync(struct tasklet_struct *);
extern int tasklet_trylock(struct tasklet_struct *);
extern void tasklet_unlock(struct tasklet_struct *);
extern void tasklet_unlock_wait(struct tasklet_struct *ts);
#define	tasklet_unlock_spin_wait(ts)	tasklet_unlock_wait(ts)

#endif	/* _LINUXKPI_LINUX_INTERRUPT_H_ */
