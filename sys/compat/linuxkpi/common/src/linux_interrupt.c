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
 */

#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/pci.h>

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/interrupt.h>

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

static inline int
lkpi_irq_rid(struct device *dev, unsigned int irq)
{
	/* check for MSI- or MSIX- interrupt */
	if (irq >= dev->irq_start && irq < dev->irq_end)
		return (irq - dev->irq_start + 1);
	else
		return (0);
}

static inline struct irq_ent *
lkpi_irq_ent(struct device *dev, unsigned int irq)
{
	struct irq_ent *irqe;

	list_for_each_entry(irqe, &dev->irqents, links)
		if (irqe->irq == irq)
			return (irqe);

	return (NULL);
}

static void
lkpi_irq_handler(void *ent)
{
	struct irq_ent *irqe;

	if (linux_set_current_flags(curthread, M_NOWAIT))
		return;

	irqe = ent;
	if (irqe->handler(irqe->irq, irqe->arg) == IRQ_WAKE_THREAD &&
	    irqe->thread_handler != NULL) {
		THREAD_SLEEPING_OK();
		irqe->thread_handler(irqe->irq, irqe->arg);
		THREAD_NO_SLEEPING();
	}
}

static inline void
lkpi_irq_release(struct device *dev, struct irq_ent *irqe)
{
	if (irqe->tag != NULL)
		bus_teardown_intr(dev->bsddev, irqe->res, irqe->tag);
	if (irqe->res != NULL)
		bus_release_resource(dev->bsddev, SYS_RES_IRQ,
		    rman_get_rid(irqe->res), irqe->res);
	list_del(&irqe->links);
}

static void
lkpi_devm_irq_release(struct device *dev, void *p)
{
	struct irq_ent *irqe;

	if (dev == NULL || p == NULL)
		return;

	irqe = p;
	lkpi_irq_release(dev, irqe);
}

int
lkpi_request_irq(struct device *xdev, unsigned int irq,
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
	rid = lkpi_irq_rid(dev, irq);
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
	    NULL, lkpi_irq_handler, irqe, &irqe->tag);
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

int
lkpi_enable_irq(unsigned int irq)
{
	struct irq_ent *irqe;
	struct device *dev;

	dev = linux_pci_find_irq_dev(irq);
	if (dev == NULL)
		return -EINVAL;
	irqe = lkpi_irq_ent(dev, irq);
	if (irqe == NULL || irqe->tag != NULL)
		return -EINVAL;
	return -bus_setup_intr(dev->bsddev, irqe->res, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, lkpi_irq_handler, irqe, &irqe->tag);
}

void
lkpi_disable_irq(unsigned int irq)
{
	struct irq_ent *irqe;
	struct device *dev;

	dev = linux_pci_find_irq_dev(irq);
	if (dev == NULL)
		return;
	irqe = lkpi_irq_ent(dev, irq);
	if (irqe == NULL)
		return;
	if (irqe->tag != NULL)
		bus_teardown_intr(dev->bsddev, irqe->res, irqe->tag);
	irqe->tag = NULL;
}

int
lkpi_bind_irq_to_cpu(unsigned int irq, int cpu_id)
{
	struct irq_ent *irqe;
	struct device *dev;

	dev = linux_pci_find_irq_dev(irq);
	if (dev == NULL)
		return (-ENOENT);

	irqe = lkpi_irq_ent(dev, irq);
	if (irqe == NULL)
		return (-ENOENT);

	return (-bus_bind_intr(dev->bsddev, irqe->res, cpu_id));
}

void
lkpi_free_irq(unsigned int irq, void *device __unused)
{
	struct irq_ent *irqe;
	struct device *dev;

	dev = linux_pci_find_irq_dev(irq);
	if (dev == NULL)
		return;
	irqe = lkpi_irq_ent(dev, irq);
	if (irqe == NULL)
		return;
	lkpi_irq_release(dev, irqe);
	kfree(irqe);
}

void
lkpi_devm_free_irq(struct device *xdev, unsigned int irq, void *p __unused)
{
	struct device *dev;
	struct irq_ent *irqe;

	dev = linux_pci_find_irq_dev(irq);
	if (dev == NULL)
		return;
	if (xdev != dev)
		return;
	irqe = lkpi_irq_ent(dev, irq);
	if (irqe == NULL)
		return;
	lkpi_irq_release(dev, irqe);
	lkpi_devres_unlink(dev, irqe);
	lkpi_devres_free(irqe);
	return;
}
