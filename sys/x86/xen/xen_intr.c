/******************************************************************************
 * xen_intr.c
 *
 * Xen event and interrupt services for x86 PV and HVM guests.
 *
 * Copyright (c) 2002-2005, K A Fraser
 * Copyright (c) 2005, Intel Corporation <xiaofeng.ling@intel.com>
 * Copyright (c) 2012, Spectra Logic Corporation
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
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/intr_machdep.h>
#include <machine/apicvar.h>
#include <machine/smp.h>
#include <machine/stdarg.h>

#include <machine/xen/synch_bitops.h>
#include <machine/xen/xen-os.h>
#include <machine/xen/xenvar.h>

#include <xen/hypervisor.h>
#include <xen/xen_intr.h>
#include <xen/evtchn/evtchnvar.h>

#include <dev/xen/xenpci/xenpcivar.h>

static MALLOC_DEFINE(M_XENINTR, "xen_intr", "Xen Interrupt Services");

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

	/** Pointer to this CPU's interrupt statistic counter. */
	u_long *evtchn_intrcnt;

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
DPCPU_DEFINE(struct xen_intr_pcpu_data, xen_intr_pcpu) = {
	.last_processed_l1i = LONG_BIT - 1,
	.last_processed_l2i = LONG_BIT - 1
};

DPCPU_DECLARE(struct vcpu_info *, vcpu_info);

#define is_valid_evtchn(x)	((x) != 0)

struct xenisrc {
	struct intsrc	xi_intsrc;
	enum evtchn_type xi_type;
	int		xi_cpu;		/* VCPU for delivery. */
	int		xi_vector;	/* Global isrc vector number. */
	evtchn_port_t	xi_port;
	int		xi_pirq;
	int		xi_virq;
	u_int		xi_close:1;	/* close on unbind? */
	u_int		xi_needs_eoi:1;
	u_int		xi_shared:1;	/* Shared with other domains. */
};

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof(a[0]))

static void	xen_intr_suspend(struct pic *);
static void	xen_intr_resume(struct pic *, bool suspend_cancelled);
static void	xen_intr_enable_source(struct intsrc *isrc);
static void	xen_intr_disable_source(struct intsrc *isrc, int eoi);
static void	xen_intr_eoi_source(struct intsrc *isrc);
static void	xen_intr_enable_intr(struct intsrc *isrc);
static void	xen_intr_disable_intr(struct intsrc *isrc);
static int	xen_intr_vector(struct intsrc *isrc);
static int	xen_intr_source_pending(struct intsrc *isrc);
static int	xen_intr_config_intr(struct intsrc *isrc,
		     enum intr_trigger trig, enum intr_polarity pol);
static int	xen_intr_assign_cpu(struct intsrc *isrc, u_int apic_id);

static void	xen_intr_pirq_enable_source(struct intsrc *isrc);
static void	xen_intr_pirq_disable_source(struct intsrc *isrc, int eoi);
static void	xen_intr_pirq_eoi_source(struct intsrc *isrc);
static void	xen_intr_pirq_enable_intr(struct intsrc *isrc);

/**
 * PIC interface for all event channel port types except physical IRQs.
 */
struct pic xen_intr_pic = {
	.pic_enable_source  = xen_intr_enable_source,
	.pic_disable_source = xen_intr_disable_source,
	.pic_eoi_source     = xen_intr_eoi_source,
	.pic_enable_intr    = xen_intr_enable_intr,
	.pic_disable_intr   = xen_intr_disable_intr,
	.pic_vector         = xen_intr_vector,
	.pic_source_pending = xen_intr_source_pending,
	.pic_suspend        = xen_intr_suspend,
	.pic_resume         = xen_intr_resume,
	.pic_config_intr    = xen_intr_config_intr,
	.pic_assign_cpu     = xen_intr_assign_cpu
};

/**
 * PIC interface for all event channel representing
 * physical interrupt sources.
 */
struct pic xen_intr_pirq_pic = {
	.pic_enable_source  = xen_intr_pirq_enable_source,
	.pic_disable_source = xen_intr_pirq_disable_source,
	.pic_eoi_source     = xen_intr_pirq_eoi_source,
	.pic_enable_intr    = xen_intr_pirq_enable_intr,
	.pic_disable_intr   = xen_intr_disable_intr,
	.pic_vector         = xen_intr_vector,
	.pic_source_pending = xen_intr_source_pending,
	.pic_suspend        = xen_intr_suspend,
	.pic_resume         = xen_intr_resume,
	.pic_config_intr    = xen_intr_config_intr,
	.pic_assign_cpu     = xen_intr_assign_cpu
};

static struct mtx	xen_intr_isrc_lock;
static int		xen_intr_isrc_count;
static struct xenisrc  *xen_intr_port_to_isrc[NR_EVENT_CHANNELS];

/*------------------------- Private Functions --------------------------------*/
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
	clear_bit(port, pcpu->evtchn_enabled);
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
	set_bit(port, pcpu->evtchn_enabled);
}

/**
 * Allocate and register a per-cpu Xen upcall interrupt counter.
 *
 * \param cpu  The cpu for which to register this interrupt count.
 */
static void
xen_intr_intrcnt_add(u_int cpu)
{
	char buf[MAXCOMLEN + 1];
	struct xen_intr_pcpu_data *pcpu;

	pcpu = DPCPU_ID_PTR(cpu, xen_intr_pcpu);
	if (pcpu->evtchn_intrcnt != NULL)
		return;

	snprintf(buf, sizeof(buf), "cpu%d:xen", cpu);
	intrcnt_add(buf, &pcpu->evtchn_intrcnt);
}

/**
 * Search for an already allocated but currently unused Xen interrupt
 * source object.
 *
 * \param type  Restrict the search to interrupt sources of the given
 *              type.
 *
 * \return  A pointer to a free Xen interrupt source object or NULL.
 */
static struct xenisrc *
xen_intr_find_unused_isrc(enum evtchn_type type)
{
	int isrc_idx;

	KASSERT(mtx_owned(&xen_intr_isrc_lock), ("Evtchn isrc lock not held"));

	for (isrc_idx = 0; isrc_idx < xen_intr_isrc_count; isrc_idx ++) {
		struct xenisrc *isrc;
		u_int vector;

		vector = FIRST_EVTCHN_INT + isrc_idx;
		isrc = (struct xenisrc *)intr_lookup_source(vector);
		if (isrc != NULL
		 && isrc->xi_type == EVTCHN_TYPE_UNBOUND) {
			KASSERT(isrc->xi_intsrc.is_handlers == 0,
			    ("Free evtchn still has handlers"));
			isrc->xi_type = type;
			return (isrc);
		}
	}
	return (NULL);
}

/**
 * Allocate a Xen interrupt source object.
 *
 * \param type  The type of interrupt source to create.
 *
 * \return  A pointer to a newly allocated Xen interrupt source
 *          object or NULL.
 */
static struct xenisrc *
xen_intr_alloc_isrc(enum evtchn_type type)
{
	static int warned;
	struct xenisrc *isrc;
	int vector;

	KASSERT(mtx_owned(&xen_intr_isrc_lock), ("Evtchn alloc lock not held"));

	if (xen_intr_isrc_count > NR_EVENT_CHANNELS) {
		if (!warned) {
			warned = 1;
			printf("xen_intr_alloc: Event channels exhausted.\n");
		}
		return (NULL);
	}
	vector = FIRST_EVTCHN_INT + xen_intr_isrc_count;
	xen_intr_isrc_count++;

	mtx_unlock(&xen_intr_isrc_lock);
	isrc = malloc(sizeof(*isrc), M_XENINTR, M_WAITOK | M_ZERO);
	isrc->xi_intsrc.is_pic = &xen_intr_pic;
	isrc->xi_vector = vector;
	isrc->xi_type = type;
	intr_register_source(&isrc->xi_intsrc);
	mtx_lock(&xen_intr_isrc_lock);

	return (isrc);
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
	if (isrc->xi_intsrc.is_handlers != 0) {
		mtx_unlock(&xen_intr_isrc_lock);
		return (EBUSY);
	}
	evtchn_mask_port(isrc->xi_port);
	evtchn_clear_port(isrc->xi_port);

	/* Rebind port to CPU 0. */
	evtchn_cpu_mask_port(isrc->xi_cpu, isrc->xi_port);
	evtchn_cpu_unmask_port(0, isrc->xi_port);

	if (isrc->xi_close != 0 && is_valid_evtchn(isrc->xi_port)) {
		struct evtchn_close close = { .port = isrc->xi_port };
		if (HYPERVISOR_event_channel_op(EVTCHNOP_close, &close))
			panic("EVTCHNOP_close failed");
	}

	xen_intr_port_to_isrc[isrc->xi_port] = NULL;
	isrc->xi_cpu = 0;
	isrc->xi_type = EVTCHN_TYPE_UNBOUND;
	isrc->xi_port = 0;
	mtx_unlock(&xen_intr_isrc_lock);
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
    enum evtchn_type type, device_t intr_owner, driver_filter_t filter,
    driver_intr_t handler, void *arg, enum intr_type flags,
    xen_intr_handle_t *port_handlep)
{
	struct xenisrc *isrc;
	int error;

	*isrcp = NULL;
	if (port_handlep == NULL) {
		device_printf(intr_owner,
			      "xen_intr_bind_isrc: Bad event handle\n");
		return (EINVAL);
	}

	mtx_lock(&xen_intr_isrc_lock);
	isrc = xen_intr_find_unused_isrc(type);
	if (isrc == NULL) {
		isrc = xen_intr_alloc_isrc(type);
		if (isrc == NULL) {
			mtx_unlock(&xen_intr_isrc_lock);
			return (ENOSPC);
		}
	}
	isrc->xi_port = local_port;
	xen_intr_port_to_isrc[local_port] = isrc;
	mtx_unlock(&xen_intr_isrc_lock);

	error = intr_add_handler(device_get_nameunit(intr_owner),
				 isrc->xi_vector, filter, handler, arg,
				 flags|INTR_EXCL, port_handlep);
	if (error != 0) {
		device_printf(intr_owner,
			      "xen_intr_bind_irq: intr_add_handler failed\n");
		xen_intr_release_isrc(isrc);
		return (error);
	}
	*isrcp = isrc;
	evtchn_unmask_port(local_port);
	return (0);
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
static struct xenisrc *
xen_intr_isrc(xen_intr_handle_t handle)
{
	struct intr_handler *ih;

	ih = handle;
	if (ih == NULL || ih->ih_event == NULL)
		return (NULL);

	return (ih->ih_event->ie_source);
}

/**
 * Determine the event channel ports at the given section of the
 * event port bitmap which have pending events for the given cpu.
 * 
 * \param pcpu  The Xen interrupt pcpu data for the cpu being querried.
 * \param sh    The Xen shared info area.
 * \param idx   The index of the section of the event channel bitmap to
 *              inspect.
 *
 * \returns  A u_long with bits set for every event channel with pending
 *           events.
 */
static inline u_long
xen_intr_active_ports(struct xen_intr_pcpu_data *pcpu, shared_info_t *sh,
    u_int idx)
{
	return (sh->evtchn_pending[idx]
	      & ~sh->evtchn_mask[idx]
	      & pcpu->evtchn_enabled[idx]);
}

/**
 * Interrupt handler for processing all Xen event channel events.
 * 
 * \param trap_frame  The trap frame context for the current interrupt.
 */
void
xen_intr_handle_upcall(struct trapframe *trap_frame)
{
	u_int l1i, l2i, port, cpu;
	u_long masked_l1, masked_l2;
	struct xenisrc *isrc;
	shared_info_t *s;
	vcpu_info_t *v;
	struct xen_intr_pcpu_data *pc;
	u_long l1, l2;

	/*
	 * Disable preemption in order to always check and fire events
	 * on the right vCPU
	 */
	critical_enter();

	cpu = PCPU_GET(cpuid);
	pc  = DPCPU_PTR(xen_intr_pcpu);
	s   = HYPERVISOR_shared_info;
	v   = DPCPU_GET(vcpu_info);

	if (xen_hvm_domain() && !xen_vector_callback_enabled) {
		KASSERT((cpu == 0), ("Fired PCI event callback on wrong CPU"));
	}

	v->evtchn_upcall_pending = 0;

#if 0
#ifndef CONFIG_X86 /* No need for a barrier -- XCHG is a barrier on x86. */
	/* Clear master flag /before/ clearing selector flag. */
	wmb();
#endif
#endif

	l1 = atomic_readandclear_long(&v->evtchn_pending_sel);

	l1i = pc->last_processed_l1i;
	l2i = pc->last_processed_l2i;
	(*pc->evtchn_intrcnt)++;

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
			l2 = xen_intr_active_ports(pc, s, l1i);

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
			synch_clear_bit(port, &s->evtchn_pending[0]);

			isrc = xen_intr_port_to_isrc[port];
			if (__predict_false(isrc == NULL))
				continue;

			/* Make sure we are firing on the right vCPU */
			KASSERT((isrc->xi_cpu == PCPU_GET(cpuid)),
				("Received unexpected event on vCPU#%d, event bound to vCPU#%d",
				PCPU_GET(cpuid), isrc->xi_cpu));

			intr_execute_handlers(&isrc->xi_intsrc, trap_frame);

			/*
			 * If this is the final port processed,
			 * we'll pick up here+1 next time.
			 */
			pc->last_processed_l1i = l1i;
			pc->last_processed_l2i = l2i;

		} while (l2i != LONG_BIT - 1);

		l2 = xen_intr_active_ports(pc, s, l1i);
		if (l2 == 0) {
			/*
			 * We handled all ports, so we can clear the
			 * selector bit.
			 */
			l1 &= ~(1UL << l1i);
		}
	}
	critical_exit();
}

static int
xen_intr_init(void *dummy __unused)
{
	struct xen_intr_pcpu_data *pcpu;
	int i;

	if (!xen_domain())
		return (0);

	mtx_init(&xen_intr_isrc_lock, "xen-irq-lock", NULL, MTX_DEF);

	/*
	 * Register interrupt count manually as we aren't
	 * guaranteed to see a call to xen_intr_assign_cpu()
	 * before our first interrupt. Also set the per-cpu
	 * mask of CPU#0 to enable all, since by default
	 * all event channels are bound to CPU#0.
	 */
	CPU_FOREACH(i) {
		pcpu = DPCPU_ID_PTR(i, xen_intr_pcpu);
		memset(pcpu->evtchn_enabled, i == 0 ? ~0 : 0,
		       sizeof(pcpu->evtchn_enabled));
		xen_intr_intrcnt_add(i);
	}

	intr_register_pic(&xen_intr_pic);

	return (0);
}
SYSINIT(xen_intr_init, SI_SUB_INTR, SI_ORDER_MIDDLE, xen_intr_init, NULL);

/*--------------------------- Common PIC Functions ---------------------------*/
/**
 * Prepare this PIC for system suspension.
 */
static void
xen_intr_suspend(struct pic *unused)
{
}

static void
xen_rebind_ipi(struct xenisrc *isrc)
{
#ifdef SMP
	int cpu = isrc->xi_cpu;
	int vcpu_id = pcpu_find(cpu)->pc_vcpu_id;
	int error;
	struct evtchn_bind_ipi bind_ipi = { .vcpu = vcpu_id };

	error = HYPERVISOR_event_channel_op(EVTCHNOP_bind_ipi,
	                                    &bind_ipi);
	if (error != 0)
		panic("unable to rebind xen IPI: %d", error);

	isrc->xi_port = bind_ipi.port;
	isrc->xi_cpu = 0;
	xen_intr_port_to_isrc[bind_ipi.port] = isrc;

	error = xen_intr_assign_cpu(&isrc->xi_intsrc,
	                            cpu_apic_ids[cpu]);
	if (error)
		panic("unable to bind xen IPI to CPU#%d: %d",
		      cpu, error);

	evtchn_unmask_port(bind_ipi.port);
#else
	panic("Resume IPI event channel on UP");
#endif
}

static void
xen_rebind_virq(struct xenisrc *isrc)
{
	int cpu = isrc->xi_cpu;
	int vcpu_id = pcpu_find(cpu)->pc_vcpu_id;
	int error;
	struct evtchn_bind_virq bind_virq = { .virq = isrc->xi_virq,
	                                      .vcpu = vcpu_id };

	error = HYPERVISOR_event_channel_op(EVTCHNOP_bind_virq,
	                                    &bind_virq);
	if (error != 0)
		panic("unable to rebind xen VIRQ#%d: %d", isrc->xi_virq, error);

	isrc->xi_port = bind_virq.port;
	isrc->xi_cpu = 0;
	xen_intr_port_to_isrc[bind_virq.port] = isrc;

#ifdef SMP
	error = xen_intr_assign_cpu(&isrc->xi_intsrc,
	                            cpu_apic_ids[cpu]);
	if (error)
		panic("unable to bind xen VIRQ#%d to CPU#%d: %d",
		      isrc->xi_virq, cpu, error);
#endif

	evtchn_unmask_port(bind_virq.port);
}

/**
 * Return this PIC to service after being suspended.
 */
static void
xen_intr_resume(struct pic *unused, bool suspend_cancelled)
{
	shared_info_t *s = HYPERVISOR_shared_info;
	struct xenisrc *isrc;
	u_int isrc_idx;
	int i;

	if (suspend_cancelled)
		return;

	/* Reset the per-CPU masks */
	CPU_FOREACH(i) {
		struct xen_intr_pcpu_data *pcpu;

		pcpu = DPCPU_ID_PTR(i, xen_intr_pcpu);
		memset(pcpu->evtchn_enabled,
		       i == 0 ? ~0 : 0, sizeof(pcpu->evtchn_enabled));
	}

	/* Mask all event channels. */
	for (i = 0; i < nitems(s->evtchn_mask); i++)
		atomic_store_rel_long(&s->evtchn_mask[i], ~0);

	/* Remove port -> isrc mappings */
	memset(xen_intr_port_to_isrc, 0, sizeof(xen_intr_port_to_isrc));

	/* Free unused isrcs and rebind VIRQs and IPIs */
	for (isrc_idx = 0; isrc_idx < xen_intr_isrc_count; isrc_idx++) {
		u_int vector;

		vector = FIRST_EVTCHN_INT + isrc_idx;
		isrc = (struct xenisrc *)intr_lookup_source(vector);
		if (isrc != NULL) {
			isrc->xi_port = 0;
			switch (isrc->xi_type) {
			case EVTCHN_TYPE_IPI:
				xen_rebind_ipi(isrc);
				break;
			case EVTCHN_TYPE_VIRQ:
				xen_rebind_virq(isrc);
				break;
			default:
				isrc->xi_cpu = 0;
				break;
			}
		}
	}
}

/**
 * Disable a Xen interrupt source.
 *
 * \param isrc  The interrupt source to disable.
 */
static void
xen_intr_disable_intr(struct intsrc *base_isrc)
{
	struct xenisrc *isrc = (struct xenisrc *)base_isrc;

	evtchn_mask_port(isrc->xi_port);
}

/**
 * Determine the global interrupt vector number for
 * a Xen interrupt source.
 *
 * \param isrc  The interrupt source to query.
 *
 * \return  The vector number corresponding to the given interrupt source.
 */
static int
xen_intr_vector(struct intsrc *base_isrc)
{
	struct xenisrc *isrc = (struct xenisrc *)base_isrc;

	return (isrc->xi_vector);
}

/**
 * Determine whether or not interrupt events are pending on the
 * the given interrupt source.
 *
 * \param isrc  The interrupt source to query.
 *
 * \returns  0 if no events are pending, otherwise non-zero.
 */
static int
xen_intr_source_pending(struct intsrc *isrc)
{
	/*
	 * EventChannels are edge triggered and never masked.
	 * There can be no pending events.
	 */
	return (0);
}

/**
 * Perform configuration of an interrupt source.
 *
 * \param isrc  The interrupt source to configure.
 * \param trig  Edge or level.
 * \param pol   Active high or low.
 *
 * \returns  0 if no events are pending, otherwise non-zero.
 */
static int
xen_intr_config_intr(struct intsrc *isrc, enum intr_trigger trig,
    enum intr_polarity pol)
{
	/* Configuration is only possible via the evtchn apis. */
	return (ENODEV);
}

/**
 * Configure CPU affinity for interrupt source event delivery.
 *
 * \param isrc     The interrupt source to configure.
 * \param apic_id  The apic id of the CPU for handling future events.
 *
 * \returns  0 if successful, otherwise an errno.
 */
static int
xen_intr_assign_cpu(struct intsrc *base_isrc, u_int apic_id)
{
#ifdef SMP
	struct evtchn_bind_vcpu bind_vcpu;
	struct xenisrc *isrc;
	u_int to_cpu, vcpu_id;
	int error;

#ifdef XENHVM
	if (xen_vector_callback_enabled == 0)
		return (EOPNOTSUPP);
#endif

	to_cpu = apic_cpuid(apic_id);
	vcpu_id = pcpu_find(to_cpu)->pc_vcpu_id;
	xen_intr_intrcnt_add(to_cpu);

	mtx_lock(&xen_intr_isrc_lock);
	isrc = (struct xenisrc *)base_isrc;
	if (!is_valid_evtchn(isrc->xi_port)) {
		mtx_unlock(&xen_intr_isrc_lock);
		return (EINVAL);
	}

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
		mtx_unlock(&xen_intr_isrc_lock);
		return (0);
	}

	bind_vcpu.port = isrc->xi_port;
	bind_vcpu.vcpu = vcpu_id;

	/*
	 * Allow interrupts to be fielded on the new VCPU before
	 * we ask the hypervisor to deliver them there.
	 */
	evtchn_cpu_unmask_port(to_cpu, isrc->xi_port);
	error = HYPERVISOR_event_channel_op(EVTCHNOP_bind_vcpu, &bind_vcpu);
	if (isrc->xi_cpu != to_cpu) {
		if (error == 0) {
			/* Commit to new binding by removing the old one. */
			evtchn_cpu_mask_port(isrc->xi_cpu, isrc->xi_port);
			isrc->xi_cpu = to_cpu;
		} else {
			/* Roll-back to previous binding. */
			evtchn_cpu_mask_port(to_cpu, isrc->xi_port);
		}
	}
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
 * \param eoi   If non-zero, perform any necessary end-of-interrupt
 *              acknowledgements.
 */
static void
xen_intr_disable_source(struct intsrc *isrc, int eoi)
{
}

/*
 * Unmask a level triggered interrupt source.
 *
 * \param isrc  The interrupt source to unmask (if necessary).
 */
static void
xen_intr_enable_source(struct intsrc *isrc)
{
}

/*
 * Perform any necessary end-of-interrupt acknowledgements.
 *
 * \param isrc  The interrupt source to EOI.
 */
static void
xen_intr_eoi_source(struct intsrc *isrc)
{
}

/*
 * Enable and unmask the interrupt source.
 *
 * \param isrc  The interrupt source to enable.
 */
static void
xen_intr_enable_intr(struct intsrc *base_isrc)
{
	struct xenisrc *isrc = (struct xenisrc *)base_isrc;

	evtchn_unmask_port(isrc->xi_port);
}

/*------------------ Physical Interrupt Source PIC Functions -----------------*/
/*
 * Mask a level triggered interrupt source.
 *
 * \param isrc  The interrupt source to mask (if necessary).
 * \param eoi   If non-zero, perform any necessary end-of-interrupt
 *              acknowledgements.
 */
static void
xen_intr_pirq_disable_source(struct intsrc *base_isrc, int eoi)
{
	struct xenisrc *isrc;

	isrc = (struct xenisrc *)base_isrc;
	evtchn_mask_port(isrc->xi_port);
}

/*
 * Unmask a level triggered interrupt source.
 *
 * \param isrc  The interrupt source to unmask (if necessary).
 */
static void
xen_intr_pirq_enable_source(struct intsrc *base_isrc)
{
	struct xenisrc *isrc;

	isrc = (struct xenisrc *)base_isrc;
	evtchn_unmask_port(isrc->xi_port);
}

/*
 * Perform any necessary end-of-interrupt acknowledgements.
 *
 * \param isrc  The interrupt source to EOI.
 */
static void
xen_intr_pirq_eoi_source(struct intsrc *base_isrc)
{
	struct xenisrc *isrc;

	/* XXX Use shared page of flags for this. */
	isrc = (struct xenisrc *)base_isrc;
	if (isrc->xi_needs_eoi != 0) {
		struct physdev_eoi eoi = { .irq = isrc->xi_pirq };

		(void)HYPERVISOR_physdev_op(PHYSDEVOP_eoi, &eoi);
	}
}

/*
 * Enable and unmask the interrupt source.
 *
 * \param isrc  The interrupt source to enable.
 */
static void
xen_intr_pirq_enable_intr(struct intsrc *isrc)
{
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

	error = xen_intr_bind_isrc(&isrc, local_port, EVTCHN_TYPE_PORT, dev,
		    filter, handler, arg, flags, port_handlep);
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
				 dev, filter, handler, arg, flags,
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
				 EVTCHN_TYPE_PORT, dev, filter, handler,
				 arg, flags, port_handlep);
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
	int vcpu_id = pcpu_find(cpu)->pc_vcpu_id;
	struct xenisrc *isrc;
	struct evtchn_bind_virq bind_virq = { .virq = virq, .vcpu = vcpu_id };
	int error;

	/* Ensure the target CPU is ready to handle evtchn interrupts. */
	xen_intr_intrcnt_add(cpu);

	isrc = NULL;
	error = HYPERVISOR_event_channel_op(EVTCHNOP_bind_virq, &bind_virq);
	if (error != 0) {
		/*
		 * XXX Trap Hypercall error code Linuxisms in
		 *     the HYPERCALL layer.
		 */
		return (-error);
	}

	error = xen_intr_bind_isrc(&isrc, bind_virq.port, EVTCHN_TYPE_VIRQ, dev,
				 filter, handler, arg, flags, port_handlep);

#ifdef SMP
	if (error == 0)
		error = intr_event_bind(isrc->xi_intsrc.is_event, cpu);
#endif

	if (error != 0) {
		evtchn_close_t close = { .port = bind_virq.port };

		xen_intr_unbind(*port_handlep);
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
		xen_intr_assign_cpu(&isrc->xi_intsrc, cpu_apic_ids[cpu]);
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
xen_intr_alloc_and_bind_ipi(device_t dev, u_int cpu,
    driver_filter_t filter, enum intr_type flags,
    xen_intr_handle_t *port_handlep)
{
#ifdef SMP
	int vcpu_id = pcpu_find(cpu)->pc_vcpu_id;
	struct xenisrc *isrc;
	struct evtchn_bind_ipi bind_ipi = { .vcpu = vcpu_id };
	int error;

	/* Ensure the target CPU is ready to handle evtchn interrupts. */
	xen_intr_intrcnt_add(cpu);

	isrc = NULL;
	error = HYPERVISOR_event_channel_op(EVTCHNOP_bind_ipi, &bind_ipi);
	if (error != 0) {
		/*
		 * XXX Trap Hypercall error code Linuxisms in
		 *     the HYPERCALL layer.
		 */
		return (-error);
	}

	error = xen_intr_bind_isrc(&isrc, bind_ipi.port, EVTCHN_TYPE_IPI,
	                           dev, filter, NULL, NULL, flags,
	                           port_handlep);
	if (error == 0)
		error = intr_event_bind(isrc->xi_intsrc.is_event, cpu);

	if (error != 0) {
		evtchn_close_t close = { .port = bind_ipi.port };

		xen_intr_unbind(*port_handlep);
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
		xen_intr_assign_cpu(&isrc->xi_intsrc, cpu_apic_ids[cpu]);
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

	isrc = xen_intr_isrc(port_handle);
	if (isrc == NULL)
		return (EINVAL);

	va_start(ap, fmt);
	vsnprintf(descr, sizeof(descr), fmt, ap);
	va_end(ap);
	return (intr_describe(isrc->xi_vector, port_handle, descr));
}

void
xen_intr_unbind(xen_intr_handle_t *port_handlep)
{
	struct intr_handler *handler;
	struct xenisrc *isrc;

	handler = *port_handlep;
	*port_handlep = NULL;
	isrc = xen_intr_isrc(handler);
	if (isrc == NULL)
		return;

	intr_remove_handler(handler);
	xen_intr_release_isrc(isrc);
}

void
xen_intr_signal(xen_intr_handle_t handle)
{
	struct xenisrc *isrc;

	isrc = xen_intr_isrc(handle);
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

	isrc = xen_intr_isrc(handle);
	if (isrc == NULL)
		return (0);
	
	return (isrc->xi_port);
}
