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

#ifndef	_LINUX_INTERRUPT_H_
#define	_LINUX_INTERRUPT_H_

#include <linux/device.h>

#include <sys/bus.h>
#include <sys/rman.h>

typedef	irqreturn_t	(*irq_handler_t)(int, void *);

#define	IRQ_RETVAL(x)	((x) != IRQ_NONE)

#define	IRQF_SHARED	RF_SHAREABLE

static void
_irq_handler(void *device)
{
	struct device *dev;

	dev = device;
	dev->irqhandler(0, dev->irqarg);
}

static inline int
request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
    const char *name, void *arg, struct device *dev)
{
	struct resource *res;
	int error;
	int rid;

	rid = 0;
	res = bus_alloc_resource_any(dev->bsddev, SYS_RES_IRQ, &rid,
	    flags | RF_ACTIVE);
	if (res == NULL)
		return (-ENXIO);
	error = bus_setup_intr(dev->bsddev, res, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, _irq_handler, dev, &dev->irqtag);
	if (error)
		return (-error);
	dev->irqhandler = handler;
	dev->irqarg = arg;

	return 0;
}

static inline void
free_irq(unsigned int irq, void *device)
{
	/* XXX */
}

#endif	/* _LINUX_INTERRUPT_H_ */
