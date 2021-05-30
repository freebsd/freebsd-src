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
#ifndef	_LINUX_INTERRUPT_H_
#define	_LINUX_INTERRUPT_H_

#include <linux/cpu.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/irqreturn.h>
#include <linux/hardirq.h>

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/interrupt.h>

typedef	irqreturn_t	(*irq_handler_t)(int, void *);

#define	IRQF_SHARED	RF_SHAREABLE

struct irq_ent {
	struct list_head	links;
	struct device	*dev;
	struct resource	*res;
	void		*arg;
	irqreturn_t	(*handler)(int, void *);
	irqreturn_t	(*thread_handler)(int, void *);
	void		*tag;
	unsigned int	irq;
};

void linux_irq_handler(void *);
void lkpi_devm_irq_release(struct device *, void *);
void lkpi_irq_release(struct device *, struct irq_ent *);

static inline int
linux_irq_rid(struct device *dev, unsigned int irq)
{
	/* check for MSI- or MSIX- interrupt */
	if (irq >= dev->irq_start && irq < dev->irq_end)
		return (irq - dev->irq_start + 1);
	else
		return (0);
}

static inline struct irq_ent *
linux_irq_ent(struct device *dev, unsigned int irq)
{
	struct irq_ent *irqe;

	list_for_each_entry(irqe, &dev->irqents, links)
		if (irqe->irq == irq)
			return (irqe);

	return (NULL);
}

static inline int
_request_irq(struct device *xdev, unsigned int irq,
    irq_handler_t handler, irq_handler_t thread_handler,
    unsigned long flags, const char *name, void *arg)
{
	struct resource *res;
	struct irq_ent *irqe;
	struct device *dev;
	int error;
	int rid;

	dev = linux_pci_find_irq_dev(irq);
	if (dev == NULL)
		return -ENXIO;
	if (xdev != NULL && xdev != dev)
		return -ENXIO;
	rid = linux_irq_rid(dev, irq);
	res = bus_alloc_resource_any(dev->bsddev, SYS_RES_IRQ, &rid,
	    flags | RF_ACTIVE);
	if (res == NULL)
		return (-ENXIO);
	if (xdev != NULL)
		irqe = lkpi_devres_alloc(lkpi_devm_irq_release, sizeof(*irqe),
		    GFP_KERNEL | __GFP_ZERO);
	else
		irqe = kzalloc(sizeof(*irqe), GFP_KERNEL);
	irqe->dev = dev;
	irqe->res = res;
	irqe->arg = arg;
	irqe->handler = handler;
	irqe->thread_handler = thread_handler;
	irqe->irq = irq;

	error = bus_setup_intr(dev->bsddev, res, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, linux_irq_handler, irqe, &irqe->tag);
	if (error)
		goto errout;
	list_add(&irqe->links, &dev->irqents);
	if (xdev != NULL)
		devres_add(xdev, irqe);

	return 0;

errout:
	bus_release_resource(dev->bsddev, SYS_RES_IRQ, rid, irqe->res);
	if (xdev != NULL)
		devres_free(irqe);
	else
		kfree(irqe);
	return (-error);
}

static inline int
request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
    const char *name, void *arg)
{

	return (_request_irq(NULL, irq, handler, NULL, flags, name, arg));
}

static inline int
request_threaded_irq(int irq, irq_handler_t handler,
    irq_handler_t thread_handler, unsigned long flags,
    const char *name, void *arg)
{

	return (_request_irq(NULL, irq, handler, thread_handler,
	    flags, name, arg));
}

static inline int
devm_request_threaded_irq(struct device *dev, int irq,
    irq_handler_t handler, irq_handler_t thread_handler,
    unsigned long flags, const char *name, void *arg)
{

	return (_request_irq(dev, irq, handler, thread_handler,
	    flags, name, arg));
}

static inline int
enable_irq(unsigned int irq)
{
	struct irq_ent *irqe;
	struct device *dev;

	dev = linux_pci_find_irq_dev(irq);
	if (dev == NULL)
		return -EINVAL;
	irqe = linux_irq_ent(dev, irq);
	if (irqe == NULL || irqe->tag != NULL)
		return -EINVAL;
	return -bus_setup_intr(dev->bsddev, irqe->res, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, linux_irq_handler, irqe, &irqe->tag);
}

static inline void
disable_irq(unsigned int irq)
{
	struct irq_ent *irqe;
	struct device *dev;

	dev = linux_pci_find_irq_dev(irq);
	if (dev == NULL)
		return;
	irqe = linux_irq_ent(dev, irq);
	if (irqe == NULL)
		return;
	if (irqe->tag != NULL)
		bus_teardown_intr(dev->bsddev, irqe->res, irqe->tag);
	irqe->tag = NULL;
}

static inline int
bind_irq_to_cpu(unsigned int irq, int cpu_id)
{
	struct irq_ent *irqe;
	struct device *dev;

	dev = linux_pci_find_irq_dev(irq);
	if (dev == NULL)
		return (-ENOENT);

	irqe = linux_irq_ent(dev, irq);
	if (irqe == NULL)
		return (-ENOENT);

	return (-bus_bind_intr(dev->bsddev, irqe->res, cpu_id));
}

static inline void
free_irq(unsigned int irq, void *device __unused)
{
	struct irq_ent *irqe;
	struct device *dev;

	dev = linux_pci_find_irq_dev(irq);
	if (dev == NULL)
		return;
	irqe = linux_irq_ent(dev, irq);
	if (irqe == NULL)
		return;
	lkpi_irq_release(dev, irqe);
	kfree(irqe);
}

static inline void
devm_free_irq(struct device *xdev, unsigned int irq, void *p)
{
	struct device *dev;
	struct irq_ent *irqe;

	dev = linux_pci_find_irq_dev(irq);
	if (dev == NULL)
		return;
	if (xdev != dev)
		return;
	irqe = linux_irq_ent(dev, irq);
	if (irqe == NULL)
		return;
	lkpi_irq_release(dev, irqe);
	lkpi_devres_unlink(dev, irqe);
	lkpi_devres_free(irqe);
	return;
}

static inline int
irq_set_affinity_hint(int vector, cpumask_t *mask)
{
	int error;

	if (mask != NULL)
		error = intr_setaffinity(vector, CPU_WHICH_IRQ, mask);
	else
		error = intr_setaffinity(vector, CPU_WHICH_IRQ, cpuset_root);

	return (-error);
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

#endif	/* _LINUX_INTERRUPT_H_ */
