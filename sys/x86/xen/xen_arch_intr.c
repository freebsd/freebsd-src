/*-
 * SPDX-License-Identifier: MIT OR GPL-2.0-only
 *
 * Copyright © 2015 Julien Grall
 * Copyright © 2013 Spectra Logic Corporation
 * Copyright © 2018 John Baldwin/The FreeBSD Foundation
 * Copyright © 2019 Roger Pau Monné/Citrix Systems R&D
 * Copyright © 2021 Elliott Mitchell
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
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/stddef.h>

#include <xen/xen-os.h>
#include <xen/xen_intr.h>
#include <machine/xen/arch-intr.h>

#include <x86/apicvar.h>

/************************ Xen x86 interrupt interface ************************/

/*
 * Pointers to the interrupt counters
 */
DPCPU_DEFINE_STATIC(u_long *, pintrcnt);

static void
xen_intrcnt_init(void *dummy __unused)
{
	unsigned int i;

	if (!xen_domain())
		return;

	CPU_FOREACH(i) {
		char buf[MAXCOMLEN + 1];

		snprintf(buf, sizeof(buf), "cpu%d:xen", i);
		intrcnt_add(buf, DPCPU_ID_PTR(i, pintrcnt));
	}
}
SYSINIT(xen_intrcnt_init, SI_SUB_INTR, SI_ORDER_MIDDLE, xen_intrcnt_init, NULL);

/*
 * Transition from assembly language, called from
 * sys/{amd64/amd64|i386/i386}/apic_vector.S
 */
extern void xen_arch_intr_handle_upcall(struct trapframe *);
void
xen_arch_intr_handle_upcall(struct trapframe *trap_frame)
{
	struct trapframe *old;

	/*
	 * Disable preemption in order to always check and fire events
	 * on the right vCPU
	 */
	critical_enter();

	++*DPCPU_GET(pintrcnt);

	++curthread->td_intr_nesting_level;
	old = curthread->td_intr_frame;
	curthread->td_intr_frame = trap_frame;

	xen_intr_handle_upcall(NULL);

	curthread->td_intr_frame = old;
	--curthread->td_intr_nesting_level;

	if (xen_evtchn_needs_ack)
		lapic_eoi();

	critical_exit();
}

/******************************** EVTCHN PIC *********************************/

static MALLOC_DEFINE(M_XENINTR, "xen_intr", "Xen Interrupt Services");

/*
 * Lock for x86-related structures.  Notably modifying
 * xen_intr_auto_vector_count, and allocating interrupts require this lock be
 * held.
 */
static struct mtx	xen_intr_x86_lock;

static u_int		first_evtchn_irq;

static u_int		xen_intr_auto_vector_count;

/*
 * list of released isrcs
 * This is meant to overlay struct xenisrc, with only the xen_arch_isrc_t
 * portion being preserved, everything else can be wiped.
 */
struct avail_list {
	xen_arch_isrc_t preserve;
	SLIST_ENTRY(avail_list) free;
};
static SLIST_HEAD(free, avail_list) avail_list =
    SLIST_HEAD_INITIALIZER(avail_list);

void
xen_intr_alloc_irqs(void)
{

	if (num_io_irqs > UINT_MAX - NR_EVENT_CHANNELS)
		panic("IRQ allocation overflow (num_msi_irqs too high?)");
	first_evtchn_irq = num_io_irqs;
	num_io_irqs += NR_EVENT_CHANNELS;
}

static void
xen_intr_pic_enable_source(struct intsrc *isrc)
{

	_Static_assert(offsetof(struct xenisrc, xi_arch.intsrc) == 0,
	    "xi_arch MUST be at top of xenisrc for x86");
	xen_intr_enable_source((struct xenisrc *)isrc);
}

/*
 * Perform any necessary end-of-interrupt acknowledgements.
 *
 * \param isrc  The interrupt source to EOI.
 */
static void
xen_intr_pic_disable_source(struct intsrc *isrc, int eoi)
{

	_Static_assert(offsetof(struct xenisrc, xi_arch.intsrc) == 0,
	    "xi_arch MUST be at top of xenisrc for x86");
	xen_intr_disable_source((struct xenisrc *)isrc);
}

static void
xen_intr_pic_eoi_source(struct intsrc *isrc)
{

	/* Nothing to do on end-of-interrupt */
}

static void
xen_intr_pic_enable_intr(struct intsrc *isrc)
{

	_Static_assert(offsetof(struct xenisrc, xi_arch.intsrc) == 0,
	    "xi_arch MUST be at top of xenisrc for x86");
	xen_intr_enable_intr((struct xenisrc *)isrc);
}

static void
xen_intr_pic_disable_intr(struct intsrc *isrc)
{

	_Static_assert(offsetof(struct xenisrc, xi_arch.intsrc) == 0,
	    "xi_arch MUST be at top of xenisrc for x86");
	xen_intr_disable_intr((struct xenisrc *)isrc);
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
xen_intr_pic_vector(struct intsrc *isrc)
{

	_Static_assert(offsetof(struct xenisrc, xi_arch.intsrc) == 0,
	    "xi_arch MUST be at top of xenisrc for x86");

	return (((struct xenisrc *)isrc)->xi_arch.vector);
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
xen_intr_pic_source_pending(struct intsrc *isrc)
{
	/*
	 * EventChannels are edge triggered and never masked.
	 * There can be no pending events.
	 */
	return (0);
}

/**
 * Prepare this PIC for system suspension.
 */
static void
xen_intr_pic_suspend(struct pic *pic)
{

	/* Nothing to do on suspend */
}

static void
xen_intr_pic_resume(struct pic *pic, bool suspend_cancelled)
{

	if (!suspend_cancelled)
		xen_intr_resume();
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
xen_intr_pic_config_intr(struct intsrc *isrc, enum intr_trigger trig,
    enum intr_polarity pol)
{
	/* Configuration is only possible via the evtchn apis. */
	return (ENODEV);
}


static int
xen_intr_pic_assign_cpu(struct intsrc *isrc, u_int apic_id)
{

	_Static_assert(offsetof(struct xenisrc, xi_arch.intsrc) == 0,
	    "xi_arch MUST be at top of xenisrc for x86");
	return (xen_intr_assign_cpu((struct xenisrc *)isrc,
	    apic_cpuid(apic_id)));
}

/**
 * PIC interface for all event channel port types except physical IRQs.
 */
static struct pic xen_intr_pic = {
	.pic_enable_source  = xen_intr_pic_enable_source,
	.pic_disable_source = xen_intr_pic_disable_source,
	.pic_eoi_source     = xen_intr_pic_eoi_source,
	.pic_enable_intr    = xen_intr_pic_enable_intr,
	.pic_disable_intr   = xen_intr_pic_disable_intr,
	.pic_vector         = xen_intr_pic_vector,
	.pic_source_pending = xen_intr_pic_source_pending,
	.pic_suspend        = xen_intr_pic_suspend,
	.pic_resume         = xen_intr_pic_resume,
	.pic_config_intr    = xen_intr_pic_config_intr,
	.pic_assign_cpu     = xen_intr_pic_assign_cpu,
};

/******************************* ARCH wrappers *******************************/

void
xen_arch_intr_init(void)
{
	int error;

	mtx_init(&xen_intr_x86_lock, "xen-x86-table-lock", NULL, MTX_DEF);

	error = intr_register_pic(&xen_intr_pic);
	if (error != 0)
		panic("%s(): failed registering Xen/x86 PIC, error=%d\n",
		    __func__, error);
}

/**
 * Allocate a Xen interrupt source object.
 *
 * \param type  The type of interrupt source to create.
 *
 * \return  A pointer to a newly allocated Xen interrupt source
 *          object or NULL.
 */
struct xenisrc *
xen_arch_intr_alloc(void)
{
	static int warned;
	struct xenisrc *isrc;
	unsigned int vector;
	int error;

	mtx_lock(&xen_intr_x86_lock);
	isrc = (struct xenisrc *)SLIST_FIRST(&avail_list);
	if (isrc != NULL) {
		SLIST_REMOVE_HEAD(&avail_list, free);
		mtx_unlock(&xen_intr_x86_lock);

		KASSERT(isrc->xi_arch.intsrc.is_pic == &xen_intr_pic,
		    ("interrupt not owned by Xen code?"));

		KASSERT(isrc->xi_arch.intsrc.is_handlers == 0,
		    ("Free evtchn still has handlers"));

		return (isrc);
	}

	if (xen_intr_auto_vector_count >= NR_EVENT_CHANNELS) {
		if (!warned) {
			warned = 1;
			printf("%s: Xen interrupts exhausted.\n", __func__);
		}
		mtx_unlock(&xen_intr_x86_lock);
		return (NULL);
	}

	vector = first_evtchn_irq + xen_intr_auto_vector_count;
	xen_intr_auto_vector_count++;

	KASSERT((intr_lookup_source(vector) == NULL),
	    ("Trying to use an already allocated vector"));

	mtx_unlock(&xen_intr_x86_lock);
	isrc = malloc(sizeof(*isrc), M_XENINTR, M_WAITOK | M_ZERO);
	isrc->xi_arch.intsrc.is_pic = &xen_intr_pic;
	isrc->xi_arch.vector = vector;
	error = intr_register_source(&isrc->xi_arch.intsrc);
	if (error != 0)
		panic("%s(): failed registering interrupt %u, error=%d\n",
		    __func__, vector, error);

	return (isrc);
}

void
xen_arch_intr_release(struct xenisrc *isrc)
{

	KASSERT(isrc->xi_arch.intsrc.is_handlers == 0,
	    ("Release called, but xenisrc still in use"));

	_Static_assert(sizeof(struct xenisrc) >= sizeof(struct avail_list),
	    "unused structure MUST be no larger than in-use structure");
	_Static_assert(offsetof(struct xenisrc, xi_arch) ==
	    offsetof(struct avail_list, preserve),
	    "unused structure does not properly overlay in-use structure");

	mtx_lock(&xen_intr_x86_lock);
	SLIST_INSERT_HEAD(&avail_list, (struct avail_list *)isrc, free);
	mtx_unlock(&xen_intr_x86_lock);
}
