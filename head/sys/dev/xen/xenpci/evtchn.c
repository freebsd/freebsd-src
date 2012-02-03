/******************************************************************************
 * evtchn.c
 *
 * A simplified event channel for para-drivers in unmodified linux
 *
 * Copyright (c) 2002-2005, K A Fraser
 * Copyright (c) 2005, Intel Corporation <xiaofeng.ling@intel.com>
 *
 * This file may be distributed separately from the Linux kernel, or
 * incorporated into other software packages, subject to the following license:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/interrupt.h>
#include <sys/pcpu.h>

#include <machine/xen/xen-os.h>
#include <machine/xen/xenvar.h>
#include <xen/hypervisor.h>
#include <xen/xen_intr.h>
#include <xen/evtchn.h>
#include <sys/smp.h>

#include <dev/xen/xenpci/xenpcivar.h>

#if defined(__i386__)
#define	__ffs(word)	(ffs(word) - 1)
#elif defined(__amd64__)
static inline unsigned long __ffs(unsigned long word)
{
        __asm__("bsfq %1,%0"
                :"=r" (word)
                :"rm" (word));	/* XXXRW: why no "cc"? */
        return word;
}
#else
#error "evtchn: unsupported architecture"
#endif

#define is_valid_evtchn(x)	((x) != 0)
#define evtchn_from_irq(x)	(irq_evtchn[irq].evtchn)

static struct {
	struct mtx lock;
	driver_intr_t *handler;
	void *arg;
	int evtchn;
	int close:1; /* close on unbind_from_irqhandler()? */
	int inuse:1;
	int in_handler:1;
	int mpsafe:1;
} irq_evtchn[256];
static int evtchn_to_irq[NR_EVENT_CHANNELS] = {
	[0 ...  NR_EVENT_CHANNELS-1] = -1 };

static struct mtx irq_alloc_lock;
static device_t xenpci_device;

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(a[0]))

static unsigned int
alloc_xen_irq(void)
{
	static int warned;
	unsigned int irq;

	mtx_lock(&irq_alloc_lock);

	for (irq = 1; irq < ARRAY_SIZE(irq_evtchn); irq++) {
		if (irq_evtchn[irq].inuse) 
			continue;
		irq_evtchn[irq].inuse = 1;
		mtx_unlock(&irq_alloc_lock);
		return irq;
	}

	if (!warned) {
		warned = 1;
		printf("alloc_xen_irq: No available IRQ to bind to: "
		       "increase irq_evtchn[] size in evtchn.c.\n");
	}

	mtx_unlock(&irq_alloc_lock);

	return -ENOSPC;
}

static void
free_xen_irq(int irq)
{

	mtx_lock(&irq_alloc_lock);
	irq_evtchn[irq].inuse = 0;
	mtx_unlock(&irq_alloc_lock);
}

int
irq_to_evtchn_port(int irq)
{

	return irq_evtchn[irq].evtchn;
}

void
mask_evtchn(int port)
{
	shared_info_t *s = HYPERVISOR_shared_info;

	synch_set_bit(port, &s->evtchn_mask[0]);
}

void
unmask_evtchn(int port)
{
	evtchn_unmask_t op = { .port = port };

	HYPERVISOR_event_channel_op(EVTCHNOP_unmask, &op);
}

int
bind_listening_port_to_irqhandler(unsigned int remote_domain,
    const char *devname, driver_intr_t handler, void *arg,
    unsigned long irqflags, unsigned int *irqp)
{
	struct evtchn_alloc_unbound alloc_unbound;
	unsigned int irq;
	int error;

	irq = alloc_xen_irq();
	if (irq < 0)
		return irq;

	mtx_lock(&irq_evtchn[irq].lock);

	alloc_unbound.dom        = DOMID_SELF;
	alloc_unbound.remote_dom = remote_domain;
	error = HYPERVISOR_event_channel_op(EVTCHNOP_alloc_unbound,
					  &alloc_unbound);
	if (error) {
		mtx_unlock(&irq_evtchn[irq].lock);
		free_xen_irq(irq);
		return (-error);
	}

	irq_evtchn[irq].handler = handler;
	irq_evtchn[irq].arg     = arg;
	irq_evtchn[irq].evtchn  = alloc_unbound.port;
	irq_evtchn[irq].close   = 1;
	irq_evtchn[irq].mpsafe  = (irqflags & INTR_MPSAFE) != 0;

	evtchn_to_irq[alloc_unbound.port] = irq;

	unmask_evtchn(alloc_unbound.port);

	mtx_unlock(&irq_evtchn[irq].lock);

	if (irqp)
		*irqp = irq;
	return (0);
}

int 
bind_interdomain_evtchn_to_irqhandler(unsigned int remote_domain,
    unsigned int remote_port, const char *devname, driver_intr_t handler,
    void *arg, unsigned long irqflags, unsigned int *irqp)
{
	struct evtchn_bind_interdomain bind_interdomain;
	unsigned int irq;
	int error;

	irq = alloc_xen_irq();
	if (irq < 0)
		return irq;

	mtx_lock(&irq_evtchn[irq].lock);

	bind_interdomain.remote_dom  = remote_domain;
	bind_interdomain.remote_port = remote_port;
	error = HYPERVISOR_event_channel_op(EVTCHNOP_bind_interdomain,
					    &bind_interdomain);
	if (error) {
		mtx_unlock(&irq_evtchn[irq].lock);
		free_xen_irq(irq);
		return (-error);
	}

	irq_evtchn[irq].handler = handler;
	irq_evtchn[irq].arg     = arg;
	irq_evtchn[irq].evtchn  = bind_interdomain.local_port;
	irq_evtchn[irq].close   = 1;
	irq_evtchn[irq].mpsafe  = (irqflags & INTR_MPSAFE) != 0;

	evtchn_to_irq[bind_interdomain.local_port] = irq;

	unmask_evtchn(bind_interdomain.local_port);

	mtx_unlock(&irq_evtchn[irq].lock);

	if (irqp)
		*irqp = irq;
	return (0);
}


int
bind_caller_port_to_irqhandler(unsigned int caller_port,
    const char *devname, driver_intr_t handler, void *arg,
    unsigned long irqflags, unsigned int *irqp)
{
	unsigned int irq;

	irq = alloc_xen_irq();
	if (irq < 0)
		return irq;

	mtx_lock(&irq_evtchn[irq].lock);

	irq_evtchn[irq].handler = handler;
	irq_evtchn[irq].arg     = arg;
	irq_evtchn[irq].evtchn  = caller_port;
	irq_evtchn[irq].close   = 0;
	irq_evtchn[irq].mpsafe  = (irqflags & INTR_MPSAFE) != 0;

	evtchn_to_irq[caller_port] = irq;

	unmask_evtchn(caller_port);

	mtx_unlock(&irq_evtchn[irq].lock);

	if (irqp)
		*irqp = irq;
	return (0);
}

void
unbind_from_irqhandler(unsigned int irq)
{
	int evtchn;

	mtx_lock(&irq_evtchn[irq].lock);

	evtchn = evtchn_from_irq(irq);

	if (is_valid_evtchn(evtchn)) {
		evtchn_to_irq[evtchn] = -1;
		mask_evtchn(evtchn);
		if (irq_evtchn[irq].close) {
			struct evtchn_close close = { .port = evtchn };
			if (HYPERVISOR_event_channel_op(EVTCHNOP_close, &close))
				panic("EVTCHNOP_close failed");
		}
	}

	irq_evtchn[irq].handler = NULL;
	irq_evtchn[irq].evtchn  = 0;

	mtx_unlock(&irq_evtchn[irq].lock);

	while (irq_evtchn[irq].in_handler)
		cpu_relax();

	free_xen_irq(irq);
}

void notify_remote_via_irq(int irq)
{
	int evtchn;

	evtchn = evtchn_from_irq(irq);
	if (is_valid_evtchn(evtchn))
		notify_remote_via_evtchn(evtchn);
}

static inline unsigned long active_evtchns(unsigned int cpu, shared_info_t *sh,
						unsigned int idx)
{
	return (sh->evtchn_pending[idx] & ~sh->evtchn_mask[idx]);
}

static void
evtchn_interrupt(void *arg)
{
	unsigned int l1i, l2i, port;
	unsigned long masked_l1, masked_l2;
	/* XXX: All events are bound to vcpu0 but irq may be redirected. */
	int cpu = 0; /*smp_processor_id();*/
	driver_intr_t *handler;
	void *handler_arg;
	int irq, handler_mpsafe;
	shared_info_t *s = HYPERVISOR_shared_info;
	vcpu_info_t *v = &s->vcpu_info[cpu];
	struct pcpu *pc = pcpu_find(cpu);
	unsigned long l1, l2;

	v->evtchn_upcall_pending = 0;

#if 0
#ifndef CONFIG_X86 /* No need for a barrier -- XCHG is a barrier on x86. */
	/* Clear master flag /before/ clearing selector flag. */
	wmb();
#endif
#endif

	l1 = atomic_readandclear_long(&v->evtchn_pending_sel);

	l1i = pc->pc_last_processed_l1i;
	l2i = pc->pc_last_processed_l2i;

	while (l1 != 0) {

		l1i = (l1i + 1) % LONG_BIT;
		masked_l1 = l1 & ((~0UL) << l1i);

		if (masked_l1 == 0) { /* if we masked out all events, wrap around to the beginning */
			l1i = LONG_BIT - 1;
			l2i = LONG_BIT - 1;
			continue;
		}
		l1i = __ffs(masked_l1);

		do {
			l2 = active_evtchns(cpu, s, l1i);

			l2i = (l2i + 1) % LONG_BIT;
			masked_l2 = l2 & ((~0UL) << l2i);

			if (masked_l2 == 0) { /* if we masked out all events, move on */
				l2i = LONG_BIT - 1;
				break;
			}
			l2i = __ffs(masked_l2);

			/* process port */
			port = (l1i * LONG_BIT) + l2i;
			synch_clear_bit(port, &s->evtchn_pending[0]);

			irq = evtchn_to_irq[port];
			if (irq < 0)
				continue;

			mtx_lock(&irq_evtchn[irq].lock);
			handler = irq_evtchn[irq].handler;
			handler_arg = irq_evtchn[irq].arg;
			handler_mpsafe = irq_evtchn[irq].mpsafe;
			if (unlikely(handler == NULL)) {
				printf("Xen IRQ%d (port %d) has no handler!\n",
				       irq, port);
				mtx_unlock(&irq_evtchn[irq].lock);
				continue;
			}
			irq_evtchn[irq].in_handler = 1;
			mtx_unlock(&irq_evtchn[irq].lock);

			//local_irq_enable();
			if (!handler_mpsafe)
				mtx_lock(&Giant);
			handler(handler_arg);
			if (!handler_mpsafe)
				mtx_unlock(&Giant);
			//local_irq_disable();

			mtx_lock(&irq_evtchn[irq].lock);
			irq_evtchn[irq].in_handler = 0;
			mtx_unlock(&irq_evtchn[irq].lock);

			/* if this is the final port processed, we'll pick up here+1 next time */
			pc->pc_last_processed_l1i = l1i;
			pc->pc_last_processed_l2i = l2i;

		} while (l2i != LONG_BIT - 1);

		l2 = active_evtchns(cpu, s, l1i);
		if (l2 == 0) /* we handled all ports, so we can clear the selector bit */
			l1 &= ~(1UL << l1i);
	}
}

void
irq_suspend(void)
{
	struct xenpci_softc *scp = device_get_softc(xenpci_device);

	/*
	 * Take our interrupt handler out of the list of handlers
	 * that can handle this irq.
	 */
	if (scp->intr_cookie != NULL) {
		if (BUS_TEARDOWN_INTR(device_get_parent(xenpci_device),
			xenpci_device, scp->res_irq, scp->intr_cookie) != 0)
			printf("intr teardown failed.. continuing\n");
		scp->intr_cookie = NULL;
	}
}

void
irq_resume(void)
{
	struct xenpci_softc *scp = device_get_softc(xenpci_device);
	int evtchn, irq;

	for (evtchn = 0; evtchn < NR_EVENT_CHANNELS; evtchn++) {
		mask_evtchn(evtchn);
		evtchn_to_irq[evtchn] = -1;
	}

	for (irq = 0; irq < ARRAY_SIZE(irq_evtchn); irq++)
		irq_evtchn[irq].evtchn = 0;

	BUS_SETUP_INTR(device_get_parent(xenpci_device),
	    xenpci_device, scp->res_irq, INTR_TYPE_MISC,
	    NULL, evtchn_interrupt, NULL, &scp->intr_cookie);
}

int
xenpci_irq_init(device_t device, struct xenpci_softc *scp)
{
	int irq, cpu;
	int error;

	mtx_init(&irq_alloc_lock, "xen-irq-lock", NULL, MTX_DEF);

	for (irq = 0; irq < ARRAY_SIZE(irq_evtchn); irq++)
		mtx_init(&irq_evtchn[irq].lock, "irq-evtchn", NULL, MTX_DEF);

	for (cpu = 0; cpu < mp_ncpus; cpu++) {
		pcpu_find(cpu)->pc_last_processed_l1i = LONG_BIT - 1;
		pcpu_find(cpu)->pc_last_processed_l2i = LONG_BIT - 1;
	}

	error = BUS_SETUP_INTR(device_get_parent(device), device,
	    scp->res_irq, INTR_MPSAFE|INTR_TYPE_MISC, NULL, evtchn_interrupt,
	    NULL, &scp->intr_cookie);
	if (error)
		return (error);

	xenpci_device = device;

	return (0);
}
