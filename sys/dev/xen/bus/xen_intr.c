/******************************************************************************
 * xen_intr.c
 *
 * Xen event and interrupt services for x86 HVM guests.
 *
 * Copyright (c) 2002-2005, K A Fraser
 * Copyright (c) 2005, Intel Corporation <xiaofeng.ling@intel.com>
 * Copyright (c) 2012, Spectra Logic Corporation
 * Copyright © 2021-2023, Elliott Mitchell
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
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/interrupt.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/refcount.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/smp.h>
#include <machine/stdarg.h>

#include <xen/xen-os.h>
#include <xen/hypervisor.h>
#include <xen/xen_intr.h>
#include <xen/evtchn/evtchnvar.h>

#include <machine/xen/arch-intr.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

/**
 * Per-cpu event channel processing state.
 */
struct xen_intr_pcpu_data {
	/**
	 * The last event channel bitmap section (level one bit) processed.
	 * This is used to ensure we scan all ports before
	 * servicing an already servied port again.
	 */
	u_int	last_processed_l1i;

	/**
	 * The last event channel processed within the event channel
	 * bitmap being scanned.
	 */
	u_int	last_processed_l2i;

	/**
	 * A bitmap of ports that can be serviced from this CPU.
	 * A set bit means interrupt handling is enabled.
	 */
	u_long	evtchn_enabled[sizeof(u_long) * 8];
};

/*
 * Start the scan at port 0 by initializing the last scanned
 * location as the highest numbered event channel port.
 */
DPCPU_DEFINE_STATIC(struct xen_intr_pcpu_data, xen_intr_pcpu) = {
	.last_processed_l1i = LONG_BIT - 1,
	.last_processed_l2i = LONG_BIT - 1
};

DPCPU_DECLARE(struct vcpu_info *, vcpu_info);

#define	INVALID_EVTCHN		(~(evtchn_port_t)0) /* Invalid event channel */
#define	is_valid_evtchn(x)	((uintmax_t)(x) < NR_EVENT_CHANNELS)

/*
 * Lock for interrupt core data.
 *
 * Modifying xen_intr_port_to_isrc[], or isrc->xi_port (implies the former)
 * requires this lock be held.  Any time this lock is not held, the condition
 * `!xen_intr_port_to_isrc[i] || (xen_intr_port_to_isrc[i]->ix_port == i)`
 * MUST be true for all values of i which are valid indicies of the array.
 *
 * Acquire/release operations for isrc->xi_refcount require this lock be held.
 */
static struct mtx	 xen_intr_isrc_lock;
static struct xenisrc	*xen_intr_port_to_isrc[NR_EVENT_CHANNELS];

/*------------------------- Private Functions --------------------------------*/

/**
 * Retrieve a handle for a Xen interrupt source.
 *
 * \param isrc  A valid Xen interrupt source structure.
 *
 * \returns  A handle suitable for use with xen_intr_isrc_from_handle()
 *           to retrieve the original Xen interrupt source structure.
 */

static inline xen_intr_handle_t
xen_intr_handle_from_isrc(struct xenisrc *isrc)
{
	return (isrc);
}

/**
 * Lookup a Xen interrupt source object given an interrupt binding handle.
 *
 * \param handle  A handle initialized by a previous call to
 *                xen_intr_bind_isrc().
 *
 * \returns  A pointer to the Xen interrupt source object associated
 *           with the given interrupt handle.  NULL if no association
 *           currently exists.
 */
static inline struct xenisrc *
xen_intr_isrc_from_handle(xen_intr_handle_t handle)
{
	return ((struct xenisrc *)handle);
}

/**
 * Disable signal delivery for an event channel port on the
 * specified CPU.
 *
 * \param port  The event channel port to mask.
 *
 * This API is used to manage the port<=>CPU binding of event
 * channel handlers.
 *
 * \note  This operation does not preclude reception of an event
 *        for this event channel on another CPU.  To mask the
 *        event channel globally, use evtchn_mask().
 */
static inline void
evtchn_cpu_mask_port(u_int cpu, evtchn_port_t port)
{
	struct xen_intr_pcpu_data *pcpu;

	pcpu = DPCPU_ID_PTR(cpu, xen_intr_pcpu);
	xen_clear_bit(port, pcpu->evtchn_enabled);
}

/**
 * Enable signal delivery for an event channel port on the
 * specified CPU.
 *
 * \param port  The event channel port to unmask.
 *
 * This API is used to manage the port<=>CPU binding of event
 * channel handlers.
 *
 * \note  This operation does not guarantee that event delivery
 *        is enabled for this event channel port.  The port must
 *        also be globally enabled.  See evtchn_unmask().
 */
static inline void
evtchn_cpu_unmask_port(u_int cpu, evtchn_port_t port)
{
	struct xen_intr_pcpu_data *pcpu;

	pcpu = DPCPU_ID_PTR(cpu, xen_intr_pcpu);
	xen_set_bit(port, pcpu->evtchn_enabled);
}

/**
 * Attempt to free an active Xen interrupt source object.
 *
 * \param isrc  The interrupt source object to release.
 *
 * \returns  EBUSY if the source is still in use, otherwise 0.
 */
static int
xen_intr_release_isrc(struct xenisrc *isrc)
{

	mtx_lock(&xen_intr_isrc_lock);
	if (is_valid_evtchn(isrc->xi_port)) {
		evtchn_mask_port(isrc->xi_port);
		evtchn_clear_port(isrc->xi_port);

		/* Rebind port to CPU 0. */
		evtchn_cpu_mask_port(isrc->xi_cpu, isrc->xi_port);
		evtchn_cpu_unmask_port(0, isrc->xi_port);

		if (isrc->xi_close != 0) {
			struct evtchn_close close = { .port = isrc->xi_port };

			if (HYPERVISOR_event_channel_op(EVTCHNOP_close, &close))
				panic("EVTCHNOP_close failed");
		}

		xen_intr_port_to_isrc[isrc->xi_port] = NULL;
	}
	/* not reachable from xen_intr_port_to_isrc[], unlock */
	mtx_unlock(&xen_intr_isrc_lock);

	xen_arch_intr_release(isrc);
	return (0);
}

/**
 * Associate an interrupt handler with an already allocated local Xen
 * event channel port.
 *
 * \param isrcp       The returned Xen interrupt object associated with
 *                    the specified local port.
 * \param local_port  The event channel to bind.
 * \param type        The event channel type of local_port.
 * \param intr_owner  The device making this bind request.
 * \param filter      An interrupt filter handler.  Specify NULL
 *                    to always dispatch to the ithread handler.
 * \param handler     An interrupt ithread handler.  Optional (can
 *                    specify NULL) if all necessary event actions
 *                    are performed by filter.
 * \param arg         Argument to present to both filter and handler.
 * \param irqflags    Interrupt handler flags.  See sys/bus.h.
 * \param handlep     Pointer to an opaque handle used to manage this
 *                    registration.
 *
 * \returns  0 on success, otherwise an errno.
 */
static int
xen_intr_bind_isrc(struct xenisrc **isrcp, evtchn_port_t local_port,
    enum evtchn_type type, const char *intr_owner, driver_filter_t filter,
    driver_intr_t handler, void *arg, enum intr_type flags,
    xen_intr_handle_t *const port_handlep)
{
	struct xenisrc *isrc;
	int error;

	*isrcp = NULL;
	if (port_handlep == NULL) {
		printf("%s: %s: Bad event handle\n", intr_owner, __func__);
		return (EINVAL);
	}
	*port_handlep = NULL;

	isrc = xen_arch_intr_alloc();
	if (isrc == NULL)
		return (ENOSPC);

	isrc->xi_cookie = NULL;
	isrc->xi_type = type;
	isrc->xi_port = local_port;
	isrc->xi_close = false;
	isrc->xi_cpu = 0;
	refcount_init(&isrc->xi_refcount, 1);
	mtx_lock(&xen_intr_isrc_lock);
	xen_intr_port_to_isrc[isrc->xi_port] = isrc;
	mtx_unlock(&xen_intr_isrc_lock);

#ifdef SMP
	if (type == EVTCHN_TYPE_PORT) {
		/*
		 * By default all interrupts are assigned to vCPU#0
		 * unless specified otherwise, so shuffle them to balance
		 * the interrupt load.
		 */
		xen_intr_assign_cpu(isrc, xen_arch_intr_next_cpu(isrc));
	}
#endif

	/*
	 * If a filter or handler function is provided, add it to the event.
	 * Otherwise the event channel is left masked and without a handler,
	 * the caller is in charge of setting that up.
	 */
	if (filter != NULL || handler != NULL) {
		error = xen_intr_add_handler(intr_owner, filter, handler, arg,
		    flags, xen_intr_handle_from_isrc(isrc));
		if (error != 0) {
			xen_intr_release_isrc(isrc);
			return (error);
		}
	}

	*isrcp = isrc;
	/* Assign the opaque handler */
	*port_handlep = xen_intr_handle_from_isrc(isrc);
	return (0);
}

/**
 * Determine the event channel ports at the given section of the
 * event port bitmap which have pending events for the given cpu.
 * 
 * \param pcpu  The Xen interrupt pcpu data for the cpu being queried.
 * \param sh    The Xen shared info area.
 * \param idx   The index of the section of the event channel bitmap to
 *              inspect.
 *
 * \returns  A u_long with bits set for every event channel with pending
 *           events.
 */
static inline u_long
xen_intr_active_ports(const struct xen_intr_pcpu_data *const pcpu,
    const u_int idx)
{
	volatile const shared_info_t *const sh = HYPERVISOR_shared_info;

	CTASSERT(sizeof(sh->evtchn_mask[0]) == sizeof(sh->evtchn_pending[0]));
	CTASSERT(sizeof(sh->evtchn_mask[0]) == sizeof(pcpu->evtchn_enabled[0]));
	CTASSERT(sizeof(sh->evtchn_mask) == sizeof(sh->evtchn_pending));
	CTASSERT(sizeof(sh->evtchn_mask) == sizeof(pcpu->evtchn_enabled));
	return (sh->evtchn_pending[idx]
	      & ~sh->evtchn_mask[idx]
	      & pcpu->evtchn_enabled[idx]);
}

/**
 * Interrupt handler for processing all Xen event channel events.
 * 
 * \param trap_frame  The trap frame context for the current interrupt.
 */
int
xen_intr_handle_upcall(void *unused __unused)
{
	struct trapframe *trap_frame = curthread->td_intr_frame;
	u_int l1i, l2i, port, cpu __diagused;
	u_long masked_l1, masked_l2;
	struct xenisrc *isrc;
	vcpu_info_t *v;
	struct xen_intr_pcpu_data *pc;
	u_long l1, l2;

	/* We must remain on the same vCPU during this function */
	CRITICAL_ASSERT(curthread);

	cpu = PCPU_GET(cpuid);
	pc  = DPCPU_PTR(xen_intr_pcpu);
	v   = DPCPU_GET(vcpu_info);

	if (!xen_has_percpu_evtchn()) {
		KASSERT((cpu == 0), ("Fired PCI event callback on wrong CPU"));
	}

	v->evtchn_upcall_pending = 0;
/* No need for a barrier on x86 -- XCHG is a barrier on x86. */
#if !defined(__amd64__) && !defined(__i386__)
	/* Clear master flag /before/ clearing selector flag. */
	wmb();
#endif
	l1 = atomic_readandclear_long(&v->evtchn_pending_sel);

	l1i = pc->last_processed_l1i;
	l2i = pc->last_processed_l2i;

	while (l1 != 0) {
		l1i = (l1i + 1) % LONG_BIT;
		masked_l1 = l1 & ((~0UL) << l1i);

		if (masked_l1 == 0) {
			/*
			 * if we masked out all events, wrap around
			 * to the beginning.
			 */
			l1i = LONG_BIT - 1;
			l2i = LONG_BIT - 1;
			continue;
		}
		l1i = ffsl(masked_l1) - 1;

		do {
			l2 = xen_intr_active_ports(pc, l1i);

			l2i = (l2i + 1) % LONG_BIT;
			masked_l2 = l2 & ((~0UL) << l2i);

			if (masked_l2 == 0) {
				/* if we masked out all events, move on */
				l2i = LONG_BIT - 1;
				break;
			}
			l2i = ffsl(masked_l2) - 1;

			/* process port */
			port = (l1i * LONG_BIT) + l2i;
			evtchn_clear_port(port);

			isrc = xen_intr_port_to_isrc[port];
			if (__predict_false(isrc == NULL))
				continue;

			/* Make sure we are firing on the right vCPU */
			KASSERT((isrc->xi_cpu == PCPU_GET(cpuid)),
				("Received unexpected event on vCPU#%u, event bound to vCPU#%u",
				PCPU_GET(cpuid), isrc->xi_cpu));

			xen_arch_intr_execute_handlers(isrc, trap_frame);

			/*
			 * If this is the final port processed,
			 * we'll pick up here+1 next time.
			 */
			pc->last_processed_l1i = l1i;
			pc->last_processed_l2i = l2i;

		} while (l2i != LONG_BIT - 1);

		l2 = xen_intr_active_ports(pc, l1i);
		if (l2 == 0) {
			/*
			 * We handled all ports, so we can clear the
			 * selector bit.
			 */
			l1 &= ~(1UL << l1i);
		}
	}

	return (FILTER_HANDLED);
}

static int
xen_intr_init(void *dummy __unused)
{
	shared_info_t *s = HYPERVISOR_shared_info;
	struct xen_intr_pcpu_data *pcpu;
	int i;

	if (!xen_domain())
		return (0);

	_Static_assert(is_valid_evtchn(0),
	    "is_valid_evtchn(0) fails (unused by Xen, but valid by interface");
	_Static_assert(is_valid_evtchn(NR_EVENT_CHANNELS - 1),
	    "is_valid_evtchn(max) fails (is a valid channel)");
	_Static_assert(!is_valid_evtchn(NR_EVENT_CHANNELS),
	    "is_valid_evtchn(>max) fails (NOT a valid channel)");
	_Static_assert(!is_valid_evtchn(~(evtchn_port_t)0),
	    "is_valid_evtchn(maxint) fails (overflow?)");
	_Static_assert(!is_valid_evtchn(INVALID_EVTCHN),
	    "is_valid_evtchn(INVALID_EVTCHN) fails (must be invalid!)");
	_Static_assert(!is_valid_evtchn(-1),
	    "is_valid_evtchn(-1) fails (negative are invalid)");

	mtx_init(&xen_intr_isrc_lock, "xen-irq-lock", NULL, MTX_DEF);

	/*
	 * Set the per-cpu mask of CPU#0 to enable all, since by default all
	 * event channels are bound to CPU#0.
	 */
	CPU_FOREACH(i) {
		pcpu = DPCPU_ID_PTR(i, xen_intr_pcpu);
		memset(pcpu->evtchn_enabled, i == 0 ? ~0 : 0,
		    sizeof(pcpu->evtchn_enabled));
	}

	for (i = 0; i < nitems(s->evtchn_mask); i++)
		atomic_store_rel_long(&s->evtchn_mask[i], ~0);

	xen_arch_intr_init();

	if (bootverbose)
		printf("Xen interrupt system initialized\n");

	return (0);
}
SYSINIT(xen_intr_init, SI_SUB_INTR, SI_ORDER_SECOND, xen_intr_init, NULL);

/*--------------------------- Common PIC Functions ---------------------------*/

static void
xen_rebind_ipi(struct xenisrc *isrc)
{
#ifdef SMP
	u_int cpu = isrc->xi_cpu;
	u_int vcpu_id = XEN_CPUID_TO_VCPUID(cpu);
	int error;
	struct evtchn_bind_ipi bind_ipi = { .vcpu = vcpu_id };

	error = HYPERVISOR_event_channel_op(EVTCHNOP_bind_ipi,
	                                    &bind_ipi);
	if (error != 0)
		panic("unable to rebind xen IPI: %d", error);

	isrc->xi_port = bind_ipi.port;
#else
	panic("Resume IPI event channel on UP");
#endif
}

static void
xen_rebind_virq(struct xenisrc *isrc)
{
	u_int cpu = isrc->xi_cpu;
	u_int vcpu_id = XEN_CPUID_TO_VCPUID(cpu);
	int error;
	struct evtchn_bind_virq bind_virq = { .virq = isrc->xi_virq,
	                                      .vcpu = vcpu_id };

	error = HYPERVISOR_event_channel_op(EVTCHNOP_bind_virq,
	                                    &bind_virq);
	if (error != 0)
		panic("unable to rebind xen VIRQ#%u: %d", isrc->xi_virq, error);

	isrc->xi_port = bind_virq.port;
}

static struct xenisrc *
xen_intr_rebind_isrc(struct xenisrc *isrc)
{
#ifdef SMP
	u_int cpu = isrc->xi_cpu;
	int error;
#endif
	struct xenisrc *prev;

	switch (isrc->xi_type) {
	case EVTCHN_TYPE_IPI:
		xen_rebind_ipi(isrc);
		break;
	case EVTCHN_TYPE_VIRQ:
		xen_rebind_virq(isrc);
		break;
	default:
		return (NULL);
	}

	prev = xen_intr_port_to_isrc[isrc->xi_port];
	xen_intr_port_to_isrc[isrc->xi_port] = isrc;

#ifdef SMP
	isrc->xi_cpu = 0;
	error = xen_intr_assign_cpu(isrc, cpu);
	if (error)
		panic("%s(): unable to rebind Xen channel %u to vCPU%u: %d",
		    __func__, isrc->xi_port, cpu, error);
#endif

	evtchn_unmask_port(isrc->xi_port);

	return (prev);
}

/**
 * Return this PIC to service after being suspended.
 */
void
xen_intr_resume(void)
{
	shared_info_t *s = HYPERVISOR_shared_info;
	u_int isrc_idx;
	int i;

	/* Reset the per-CPU masks */
	CPU_FOREACH(i) {
		struct xen_intr_pcpu_data *pcpu;

		pcpu = DPCPU_ID_PTR(i, xen_intr_pcpu);
		memset(pcpu->evtchn_enabled, i == 0 ? ~0 : 0,
		    sizeof(pcpu->evtchn_enabled));
	}

	/* Mask all event channels. */
	for (i = 0; i < nitems(s->evtchn_mask); i++)
		atomic_store_rel_long(&s->evtchn_mask[i], ~0);

	/* Clear existing port mappings */
	for (isrc_idx = 0; isrc_idx < NR_EVENT_CHANNELS; ++isrc_idx)
		if (xen_intr_port_to_isrc[isrc_idx] != NULL)
			xen_intr_port_to_isrc[isrc_idx]->xi_port =
			    INVALID_EVTCHN;

	/* Remap in-use isrcs, using xen_intr_port_to_isrc as listing */
	for (isrc_idx = 0; isrc_idx < NR_EVENT_CHANNELS; ++isrc_idx) {
		struct xenisrc *cur = xen_intr_port_to_isrc[isrc_idx];

		/* empty or entry already taken care of */
		if (cur == NULL || cur->xi_port == isrc_idx)
			continue;

		xen_intr_port_to_isrc[isrc_idx] = NULL;

		do {
			KASSERT(!is_valid_evtchn(cur->xi_port),
			    ("%s(): Multiple channels on single intr?",
			    __func__));

			cur = xen_intr_rebind_isrc(cur);
		} while (cur != NULL);
	}
}

/**
 * Disable a Xen interrupt source.
 *
 * \param isrc  The interrupt source to disable.
 */
void
xen_intr_disable_intr(struct xenisrc *isrc)
{

	evtchn_mask_port(isrc->xi_port);
}

/**
 * Configure CPU affinity for interrupt source event delivery.
 *
 * \param isrc     The interrupt source to configure.
 * \param to_cpu   The id of the CPU for handling future events.
 *
 * \returns  0 if successful, otherwise an errno.
 */
int
xen_intr_assign_cpu(struct xenisrc *isrc, u_int to_cpu)
{
#ifdef SMP
	struct evtchn_bind_vcpu bind_vcpu;
	u_int vcpu_id = XEN_CPUID_TO_VCPUID(to_cpu);
	int error, masked;

	if (!xen_has_percpu_evtchn())
		return (EOPNOTSUPP);

	mtx_lock(&xen_intr_isrc_lock);
	if (!is_valid_evtchn(isrc->xi_port)) {
		mtx_unlock(&xen_intr_isrc_lock);
		return (EINVAL);
	}

	/*
	 * Mask the event channel while binding it to prevent interrupt
	 * delivery with an inconsistent state in isrc->xi_cpu.
	 */
	masked = evtchn_test_and_set_mask(isrc->xi_port);
	if ((isrc->xi_type == EVTCHN_TYPE_VIRQ) ||
		(isrc->xi_type == EVTCHN_TYPE_IPI)) {
		/*
		 * Virtual IRQs are associated with a cpu by
		 * the Hypervisor at evtchn_bind_virq time, so
		 * all we need to do is update the per-CPU masks.
		 */
		evtchn_cpu_mask_port(isrc->xi_cpu, isrc->xi_port);
		isrc->xi_cpu = to_cpu;
		evtchn_cpu_unmask_port(isrc->xi_cpu, isrc->xi_port);
		goto out;
	}

	bind_vcpu.port = isrc->xi_port;
	bind_vcpu.vcpu = vcpu_id;

	error = HYPERVISOR_event_channel_op(EVTCHNOP_bind_vcpu, &bind_vcpu);
	if (isrc->xi_cpu != to_cpu) {
		if (error == 0) {
			/* Commit to new binding by removing the old one. */
			evtchn_cpu_mask_port(isrc->xi_cpu, isrc->xi_port);
			isrc->xi_cpu = to_cpu;
			evtchn_cpu_unmask_port(isrc->xi_cpu, isrc->xi_port);
		}
	}

out:
	if (masked == 0)
		evtchn_unmask_port(isrc->xi_port);
	mtx_unlock(&xen_intr_isrc_lock);
	return (0);
#else
	return (EOPNOTSUPP);
#endif
}

/*------------------- Virtual Interrupt Source PIC Functions -----------------*/
/*
 * Mask a level triggered interrupt source.
 *
 * \param isrc  The interrupt source to mask (if necessary).
 */
void
xen_intr_disable_source(struct xenisrc *isrc)
{

	/*
	 * NB: checking if the event channel is already masked is
	 * needed because the event channel user-space device
	 * masks event channels on its filter as part of its
	 * normal operation, and those shouldn't be automatically
	 * unmasked by the generic interrupt code. The event channel
	 * device will unmask them when needed.
	 */
	isrc->xi_masked = !!evtchn_test_and_set_mask(isrc->xi_port);
}

/*
 * Unmask a level triggered interrupt source.
 *
 * \param isrc  The interrupt source to unmask (if necessary).
 */
void
xen_intr_enable_source(struct xenisrc *isrc)
{

	if (isrc->xi_masked == 0)
		evtchn_unmask_port(isrc->xi_port);
}

/*
 * Enable and unmask the interrupt source.
 *
 * \param isrc  The interrupt source to enable.
 */
void
xen_intr_enable_intr(struct xenisrc *isrc)
{

	evtchn_unmask_port(isrc->xi_port);
}

/*--------------------------- Public Functions -------------------------------*/
/*------- API comments for these methods can be found in xen/xenintr.h -------*/
int
xen_intr_bind_local_port(device_t dev, evtchn_port_t local_port,
    driver_filter_t filter, driver_intr_t handler, void *arg,
    enum intr_type flags, xen_intr_handle_t *port_handlep)
{
	struct xenisrc *isrc;
	int error;

	error = xen_intr_bind_isrc(&isrc, local_port, EVTCHN_TYPE_PORT,
	    device_get_nameunit(dev), filter, handler, arg, flags,
	    port_handlep);
	if (error != 0)
		return (error);

	/*
	 * The Event Channel API didn't open this port, so it is not
	 * responsible for closing it automatically on unbind.
	 */
	isrc->xi_close = 0;
	return (0);
}

int
xen_intr_alloc_and_bind_local_port(device_t dev, u_int remote_domain,
    driver_filter_t filter, driver_intr_t handler, void *arg,
    enum intr_type flags, xen_intr_handle_t *port_handlep)
{
	struct xenisrc *isrc;
	struct evtchn_alloc_unbound alloc_unbound;
	int error;

	alloc_unbound.dom        = DOMID_SELF;
	alloc_unbound.remote_dom = remote_domain;
	error = HYPERVISOR_event_channel_op(EVTCHNOP_alloc_unbound,
		    &alloc_unbound);
	if (error != 0) {
		/*
		 * XXX Trap Hypercall error code Linuxisms in
		 *     the HYPERCALL layer.
		 */
		return (-error);
	}

	error = xen_intr_bind_isrc(&isrc, alloc_unbound.port, EVTCHN_TYPE_PORT,
	    device_get_nameunit(dev), filter, handler, arg, flags,
	    port_handlep);
	if (error != 0) {
		evtchn_close_t close = { .port = alloc_unbound.port };
		if (HYPERVISOR_event_channel_op(EVTCHNOP_close, &close))
			panic("EVTCHNOP_close failed");
		return (error);
	}

	isrc->xi_close = 1;
	return (0);
}

int 
xen_intr_bind_remote_port(device_t dev, u_int remote_domain,
    u_int remote_port, driver_filter_t filter, driver_intr_t handler,
    void *arg, enum intr_type flags, xen_intr_handle_t *port_handlep)
{
	struct xenisrc *isrc;
	struct evtchn_bind_interdomain bind_interdomain;
	int error;

	bind_interdomain.remote_dom  = remote_domain;
	bind_interdomain.remote_port = remote_port;
	error = HYPERVISOR_event_channel_op(EVTCHNOP_bind_interdomain,
					    &bind_interdomain);
	if (error != 0) {
		/*
		 * XXX Trap Hypercall error code Linuxisms in
		 *     the HYPERCALL layer.
		 */
		return (-error);
	}

	error = xen_intr_bind_isrc(&isrc, bind_interdomain.local_port,
	    EVTCHN_TYPE_PORT, device_get_nameunit(dev), filter, handler, arg,
	    flags, port_handlep);
	if (error) {
		evtchn_close_t close = { .port = bind_interdomain.local_port };
		if (HYPERVISOR_event_channel_op(EVTCHNOP_close, &close))
			panic("EVTCHNOP_close failed");
		return (error);
	}

	/*
	 * The Event Channel API opened this port, so it is
	 * responsible for closing it automatically on unbind.
	 */
	isrc->xi_close = 1;
	return (0);
}

int 
xen_intr_bind_virq(device_t dev, u_int virq, u_int cpu,
    driver_filter_t filter, driver_intr_t handler, void *arg,
    enum intr_type flags, xen_intr_handle_t *port_handlep)
{
	u_int vcpu_id = XEN_CPUID_TO_VCPUID(cpu);
	struct xenisrc *isrc;
	struct evtchn_bind_virq bind_virq = { .virq = virq, .vcpu = vcpu_id };
	int error;

	isrc = NULL;
	error = HYPERVISOR_event_channel_op(EVTCHNOP_bind_virq, &bind_virq);
	if (error != 0) {
		/*
		 * XXX Trap Hypercall error code Linuxisms in
		 *     the HYPERCALL layer.
		 */
		return (-error);
	}

	error = xen_intr_bind_isrc(&isrc, bind_virq.port, EVTCHN_TYPE_VIRQ,
	    device_get_nameunit(dev), filter, handler, arg, flags,
	    port_handlep);

#ifdef SMP
	if (error == 0)
		error = xen_arch_intr_event_bind(isrc, cpu);
#endif

	if (error != 0) {
		evtchn_close_t close = { .port = bind_virq.port };

		xen_intr_unbind(port_handlep);
		if (HYPERVISOR_event_channel_op(EVTCHNOP_close, &close))
			panic("EVTCHNOP_close failed");
		return (error);
	}

#ifdef SMP
	if (isrc->xi_cpu != cpu) {
		/*
		 * Too early in the boot process for the generic interrupt
		 * code to perform the binding.  Update our event channel
		 * masks manually so events can't fire on the wrong cpu
		 * during AP startup.
		 */
		xen_intr_assign_cpu(isrc, cpu);
	}
#endif

	/*
	 * The Event Channel API opened this port, so it is
	 * responsible for closing it automatically on unbind.
	 */
	isrc->xi_close = 1;
	isrc->xi_virq = virq;

	return (0);
}

int
xen_intr_alloc_and_bind_ipi(u_int cpu, driver_filter_t filter,
    enum intr_type flags, xen_intr_handle_t *port_handlep)
{
#ifdef SMP
	u_int vcpu_id = XEN_CPUID_TO_VCPUID(cpu);
	struct xenisrc *isrc;
	struct evtchn_bind_ipi bind_ipi = { .vcpu = vcpu_id };
	/* Same size as the one used by intr_handler->ih_name. */
	char name[MAXCOMLEN + 1];
	int error;

	isrc = NULL;
	error = HYPERVISOR_event_channel_op(EVTCHNOP_bind_ipi, &bind_ipi);
	if (error != 0) {
		/*
		 * XXX Trap Hypercall error code Linuxisms in
		 *     the HYPERCALL layer.
		 */
		return (-error);
	}

	snprintf(name, sizeof(name), "cpu%u", cpu);

	error = xen_intr_bind_isrc(&isrc, bind_ipi.port, EVTCHN_TYPE_IPI,
	    name, filter, NULL, NULL, flags, port_handlep);
	if (error != 0) {
		evtchn_close_t close = { .port = bind_ipi.port };

		xen_intr_unbind(port_handlep);
		if (HYPERVISOR_event_channel_op(EVTCHNOP_close, &close))
			panic("EVTCHNOP_close failed");
		return (error);
	}

	if (isrc->xi_cpu != cpu) {
		/*
		 * Too early in the boot process for the generic interrupt
		 * code to perform the binding.  Update our event channel
		 * masks manually so events can't fire on the wrong cpu
		 * during AP startup.
		 */
		xen_intr_assign_cpu(isrc, cpu);
	}

	/*
	 * The Event Channel API opened this port, so it is
	 * responsible for closing it automatically on unbind.
	 */
	isrc->xi_close = 1;
	return (0);
#else
	return (EOPNOTSUPP);
#endif
}

int
xen_intr_describe(xen_intr_handle_t port_handle, const char *fmt, ...)
{
	char descr[MAXCOMLEN + 1];
	struct xenisrc *isrc;
	va_list ap;

	isrc = xen_intr_isrc_from_handle(port_handle);
	if (isrc == NULL)
		return (EINVAL);

	va_start(ap, fmt);
	vsnprintf(descr, sizeof(descr), fmt, ap);
	va_end(ap);
	return (xen_arch_intr_describe(isrc, isrc->xi_cookie, descr));
}

void
xen_intr_unbind(xen_intr_handle_t *port_handlep)
{
	struct xenisrc *isrc;

	KASSERT(port_handlep != NULL,
	    ("NULL xen_intr_handle_t passed to %s", __func__));

	isrc = xen_intr_isrc_from_handle(*port_handlep);
	*port_handlep = NULL;
	if (isrc == NULL)
		return;

	mtx_lock(&xen_intr_isrc_lock);
	if (refcount_release(&isrc->xi_refcount) == 0) {
		mtx_unlock(&xen_intr_isrc_lock);
		return;
	}
	mtx_unlock(&xen_intr_isrc_lock);

	if (isrc->xi_cookie != NULL)
		xen_arch_intr_remove_handler(isrc, isrc->xi_cookie);
	xen_intr_release_isrc(isrc);
}

void
xen_intr_signal(xen_intr_handle_t handle)
{
	struct xenisrc *isrc;

	isrc = xen_intr_isrc_from_handle(handle);
	if (isrc != NULL) {
		KASSERT(isrc->xi_type == EVTCHN_TYPE_PORT ||
			isrc->xi_type == EVTCHN_TYPE_IPI,
			("evtchn_signal on something other than a local port"));
		struct evtchn_send send = { .port = isrc->xi_port };
		(void)HYPERVISOR_event_channel_op(EVTCHNOP_send, &send);
	}
}

evtchn_port_t
xen_intr_port(xen_intr_handle_t handle)
{
	struct xenisrc *isrc;

	isrc = xen_intr_isrc_from_handle(handle);
	if (isrc == NULL)
		return (0);

	return (isrc->xi_port);
}

int
xen_intr_add_handler(const char *name, driver_filter_t filter,
    driver_intr_t handler, void *arg, enum intr_type flags,
    xen_intr_handle_t handle)
{
	struct xenisrc *isrc;
	int error;

	isrc = xen_intr_isrc_from_handle(handle);
	if (isrc == NULL || isrc->xi_cookie != NULL)
		return (EINVAL);

	error = xen_arch_intr_add_handler(name, filter, handler, arg,
	    flags | INTR_EXCL, isrc, &isrc->xi_cookie);
	if (error != 0)
		printf("%s: %s: add handler failed: %d\n", name, __func__,
		    error);

	return (error);
}

int
xen_intr_get_evtchn_from_port(evtchn_port_t port, xen_intr_handle_t *handlep)
{

	if (!is_valid_evtchn(port))
		return (EINVAL);

	if (handlep == NULL) {
		return (EINVAL);
	}

	mtx_lock(&xen_intr_isrc_lock);
	if (xen_intr_port_to_isrc[port] == NULL) {
		mtx_unlock(&xen_intr_isrc_lock);
		return (EINVAL);
	}
	refcount_acquire(&xen_intr_port_to_isrc[port]->xi_refcount);
	mtx_unlock(&xen_intr_isrc_lock);

	/* Assign the opaque handler */
	*handlep = xen_intr_handle_from_isrc(xen_intr_port_to_isrc[port]);

	return (0);
}

#ifdef DDB
static const char *
xen_intr_print_type(enum evtchn_type type)
{
	static const char *evtchn_type_to_string[EVTCHN_TYPE_COUNT] = {
		[EVTCHN_TYPE_UNBOUND]	= "UNBOUND",
		[EVTCHN_TYPE_VIRQ]	= "VIRQ",
		[EVTCHN_TYPE_IPI]	= "IPI",
		[EVTCHN_TYPE_PORT]	= "PORT",
	};

	if (type >= EVTCHN_TYPE_COUNT)
		return ("UNKNOWN");

	return (evtchn_type_to_string[type]);
}

static void
xen_intr_dump_port(struct xenisrc *isrc)
{
	struct xen_intr_pcpu_data *pcpu;
	shared_info_t *s = HYPERVISOR_shared_info;
	u_int i;

	db_printf("Port %d Type: %s\n",
	    isrc->xi_port, xen_intr_print_type(isrc->xi_type));
	if (isrc->xi_type == EVTCHN_TYPE_VIRQ)
		db_printf("\tVirq: %u\n", isrc->xi_virq);

	db_printf("\tMasked: %d Pending: %d\n",
	    !!xen_test_bit(isrc->xi_port, &s->evtchn_mask[0]),
	    !!xen_test_bit(isrc->xi_port, &s->evtchn_pending[0]));

	db_printf("\tPer-CPU Masks: ");
	CPU_FOREACH(i) {
		pcpu = DPCPU_ID_PTR(i, xen_intr_pcpu);
		db_printf("cpu#%u: %d ", i,
		    !!xen_test_bit(isrc->xi_port, pcpu->evtchn_enabled));
	}
	db_printf("\n");
}

DB_SHOW_COMMAND(xen_evtchn, db_show_xen_evtchn)
{
	u_int i;

	if (!xen_domain()) {
		db_printf("Only available on Xen guests\n");
		return;
	}

	for (i = 0; i < NR_EVENT_CHANNELS; i++) {
		struct xenisrc *isrc;

		isrc = xen_intr_port_to_isrc[i];
		if (isrc == NULL)
			continue;

		xen_intr_dump_port(isrc);
	}
}
#endif /* DDB */
