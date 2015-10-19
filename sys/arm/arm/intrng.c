/*-
 * Copyright (c) 2012-2014 Jakub Wojciech Klama <jceel@FreeBSD.org>.
 * Copyright (c) 2015 Svatopluk Kraus
 * Copyright (c) 2015 Michal Meloun
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 *	ARM Interrupt Framework
 *
 *  TODO: - to support IPI (PPI) enabling on other CPUs if already started
 *        - to complete things for removable PICs
 */

#include "opt_ddb.h"
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/conf.h>
#include <sys/cpuset.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <machine/atomic.h>
#include <machine/intr.h>
#include <machine/cpu.h>
#include <machine/smp.h>
#include <machine/stdarg.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/fdt/fdt_common.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include "pic_if.h"

#define	INTRNAME_LEN	(2*MAXCOMLEN + 1)

#ifdef DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__);	\
    printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

MALLOC_DECLARE(M_INTRNG);
MALLOC_DEFINE(M_INTRNG, "intrng", "ARM interrupt handling");

/* Main ARM interrupt handler called from assembler -> 'hidden' for C code. */
void arm_irq_handler(struct trapframe *tf);

/* Root interrupt controller stuff. */
static struct arm_irqsrc *irq_root_isrc;
static device_t irq_root_dev;
static arm_irq_filter_t *irq_root_filter;
static void *irq_root_arg;
static u_int irq_root_ipicount;

/* Interrupt controller definition. */
struct arm_pic {
	SLIST_ENTRY(arm_pic)	pic_next;
	intptr_t		pic_xref;	/* hardware identification */
	device_t		pic_dev;
};

static struct mtx pic_list_lock;
static SLIST_HEAD(, arm_pic) pic_list;

static struct arm_pic *pic_lookup(device_t dev, intptr_t xref);

/* Interrupt source definition. */
static struct mtx isrc_table_lock;
static struct arm_irqsrc *irq_sources[NIRQ];
u_int irq_next_free;

#define IRQ_INVALID	nitems(irq_sources)

#ifdef SMP
static boolean_t irq_assign_cpu = FALSE;

static struct arm_irqsrc ipi_sources[ARM_IPI_COUNT];
static u_int ipi_next_num;
#endif

/*
 * - 2 counters for each I/O interrupt.
 * - MAXCPU counters for each IPI counters for SMP.
 */
#ifdef SMP
#define INTRCNT_COUNT   (NIRQ * 2 + ARM_IPI_COUNT * MAXCPU)
#else
#define INTRCNT_COUNT   (NIRQ * 2)
#endif

/* Data for MI statistics reporting. */
u_long intrcnt[INTRCNT_COUNT];
char intrnames[INTRCNT_COUNT * INTRNAME_LEN];
size_t sintrcnt = sizeof(intrcnt);
size_t sintrnames = sizeof(intrnames);
static u_int intrcnt_index;

/*
 *  Interrupt framework initialization routine.
 */
static void
arm_irq_init(void *dummy __unused)
{

	SLIST_INIT(&pic_list);
	mtx_init(&pic_list_lock, "arm pic list", NULL, MTX_DEF);
	mtx_init(&isrc_table_lock, "arm isrc table", NULL, MTX_DEF);
}
SYSINIT(arm_irq_init, SI_SUB_INTR, SI_ORDER_FIRST, arm_irq_init, NULL);

static void
intrcnt_setname(const char *name, int index)
{

	snprintf(intrnames + INTRNAME_LEN * index, INTRNAME_LEN, "%-*s",
	    INTRNAME_LEN - 1, name);
}

/*
 *  Update name for interrupt source with interrupt event.
 */
static void
intrcnt_updatename(struct arm_irqsrc *isrc)
{

	/* QQQ: What about stray counter name? */
	mtx_assert(&isrc_table_lock, MA_OWNED);
	intrcnt_setname(isrc->isrc_event->ie_fullname, isrc->isrc_index);
}

/*
 *  Virtualization for interrupt source interrupt counter increment.
 */
static inline void
isrc_increment_count(struct arm_irqsrc *isrc)
{

	/*
	 * XXX - It should be atomic for PPI interrupts. It was proven that
	 *       the lost is measurable easily for timer PPI interrupts.
	 */
	isrc->isrc_count[0]++;
	/*atomic_add_long(&isrc->isrc_count[0], 1);*/
}

/*
 *  Virtualization for interrupt source interrupt stray counter increment.
 */
static inline void
isrc_increment_straycount(struct arm_irqsrc *isrc)
{

	isrc->isrc_count[1]++;
}

/*
 *  Virtualization for interrupt source interrupt name update.
 */
static void
isrc_update_name(struct arm_irqsrc *isrc, const char *name)
{
	char str[INTRNAME_LEN];

	mtx_assert(&isrc_table_lock, MA_OWNED);

	if (name != NULL) {
		snprintf(str, INTRNAME_LEN, "%s: %s", isrc->isrc_name, name);
		intrcnt_setname(str, isrc->isrc_index);
		snprintf(str, INTRNAME_LEN, "stray %s: %s", isrc->isrc_name,
		    name);
		intrcnt_setname(str, isrc->isrc_index + 1);
	} else {
		snprintf(str, INTRNAME_LEN, "%s:", isrc->isrc_name);
		intrcnt_setname(str, isrc->isrc_index);
		snprintf(str, INTRNAME_LEN, "stray %s:", isrc->isrc_name);
		intrcnt_setname(str, isrc->isrc_index + 1);
	}
}

/*
 *  Virtualization for interrupt source interrupt counters setup.
 */
static void
isrc_setup_counters(struct arm_irqsrc *isrc)
{
	u_int index;

	/*
	 *  XXX - it does not work well with removable controllers and
	 *        interrupt sources !!!
	 */
	index = atomic_fetchadd_int(&intrcnt_index, 2);
	isrc->isrc_index = index;
	isrc->isrc_count = &intrcnt[index];
	isrc_update_name(isrc, NULL);
}

#ifdef SMP
/*
 *  Virtualization for interrupt source IPI counter increment.
 */
static inline void
isrc_increment_ipi_count(struct arm_irqsrc *isrc, u_int cpu)
{

	isrc->isrc_count[cpu]++;
}

/*
 *  Virtualization for interrupt source IPI counters setup.
 */
static void
isrc_setup_ipi_counters(struct arm_irqsrc *isrc, const char *name)
{
	u_int index, i;
	char str[INTRNAME_LEN];

	index = atomic_fetchadd_int(&intrcnt_index, MAXCPU);
	isrc->isrc_index = index;
	isrc->isrc_count = &intrcnt[index];

	for (i = 0; i < MAXCPU; i++) {
		/*
		 * We do not expect any race in IPI case here,
		 * so locking is not needed.
		 */
		snprintf(str, INTRNAME_LEN, "cpu%d:%s", i, name);
		intrcnt_setname(str, index + i);
	}
}
#endif

/*
 *  Main ARM interrupt dispatch handler. It's called straight
 *  from the assembler, where CPU interrupt is served.
 */
void
arm_irq_handler(struct trapframe *tf)
{
	struct trapframe * oldframe;
	struct thread * td;

	KASSERT(irq_root_filter != NULL, ("%s: no filter", __func__));

	PCPU_INC(cnt.v_intr);
	critical_enter();
	td = curthread;
	oldframe = td->td_intr_frame;
	td->td_intr_frame = tf;
	irq_root_filter(irq_root_arg);
	td->td_intr_frame = oldframe;
	critical_exit();
}

/*
 *  ARM interrupt controller dispatch function for interrupts. It should
 *  be called straight from the interrupt controller, when associated interrupt
 *  source is learned.
 */
void
arm_irq_dispatch(struct arm_irqsrc *isrc, struct trapframe *tf)
{

	KASSERT(isrc != NULL, ("%s: no source", __func__));

	isrc_increment_count(isrc);

#ifdef INTR_SOLO
	if (isrc->isrc_filter != NULL) {
		int error;
		error = isrc->isrc_filter(isrc->isrc_arg, tf);
		PIC_POST_FILTER(isrc->isrc_dev, isrc);
		if (error == FILTER_HANDLED)
			return;
	} else 
#endif
	if (isrc->isrc_event != NULL) {
		if (intr_event_handle(isrc->isrc_event, tf) == 0)
			return;
	}

	isrc_increment_straycount(isrc);
	PIC_DISABLE_SOURCE(isrc->isrc_dev, isrc);

	device_printf(isrc->isrc_dev, "stray irq <%s> disabled",
	    isrc->isrc_name);
}

/*
 *  Allocate interrupt source.
 */
static struct arm_irqsrc *
isrc_alloc(u_int type, u_int extsize)
{
	struct arm_irqsrc *isrc;

	isrc = malloc(sizeof(*isrc) + extsize, M_INTRNG, M_WAITOK | M_ZERO);
	isrc->isrc_irq = IRQ_INVALID;	/* just to be safe */
	isrc->isrc_type = type;
	isrc->isrc_nspc_type = ARM_IRQ_NSPC_NONE;
	isrc->isrc_trig = INTR_TRIGGER_CONFORM;
	isrc->isrc_pol = INTR_POLARITY_CONFORM;
	CPU_ZERO(&isrc->isrc_cpu);
	return (isrc);
}

/*
 *  Free interrupt source.
 */
static void
isrc_free(struct arm_irqsrc *isrc)
{

	free(isrc, M_INTRNG);
}

void
arm_irq_set_name(struct arm_irqsrc *isrc, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(isrc->isrc_name, ARM_ISRC_NAMELEN, fmt, ap);
	va_end(ap);
}

/*
 *  Alloc unique interrupt number (resource handle) for interrupt source.
 *
 *  There could be various strategies how to allocate free interrupt number
 *  (resource handle) for new interrupt source.
 *
 *  1. Handles are always allocated forward, so handles are not recycled
 *     immediately. However, if only one free handle left which is reused
 *     constantly...
 */
static int
isrc_alloc_irq_locked(struct arm_irqsrc *isrc)
{
	u_int maxirqs, irq;

	mtx_assert(&isrc_table_lock, MA_OWNED);

	maxirqs = nitems(irq_sources);
	if (irq_next_free >= maxirqs)
		return (ENOSPC);

	for (irq = irq_next_free; irq < maxirqs; irq++) {
		if (irq_sources[irq] == NULL)
			goto found;
	}
	for (irq = 0; irq < irq_next_free; irq++) {
		if (irq_sources[irq] == NULL)
			goto found;
	}

	irq_next_free = maxirqs;
	return (ENOSPC);

found:
	isrc->isrc_irq = irq;
	irq_sources[irq] = isrc;

	arm_irq_set_name(isrc, "irq%u", irq);
	isrc_setup_counters(isrc);

	irq_next_free = irq + 1;
	if (irq_next_free >= maxirqs)
		irq_next_free = 0;
	return (0);
}
#ifdef notyet
/*
 *  Free unique interrupt number (resource handle) from interrupt source.
 */
static int
isrc_free_irq(struct arm_irqsrc *isrc)
{
	u_int maxirqs;

	mtx_assert(&isrc_table_lock, MA_NOTOWNED);

	maxirqs = nitems(irq_sources);
	if (isrc->isrc_irq >= maxirqs)
		return (EINVAL);

	mtx_lock(&isrc_table_lock);
	if (irq_sources[isrc->isrc_irq] != isrc) {
		mtx_unlock(&isrc_table_lock);
		return (EINVAL);
	}

	irq_sources[isrc->isrc_irq] = NULL;
	isrc->isrc_irq = IRQ_INVALID;	/* just to be safe */
	mtx_unlock(&isrc_table_lock);

	return (0);
}
#endif
/*
 *  Lookup interrupt source by interrupt number (resource handle).
 */
static struct arm_irqsrc *
isrc_lookup(u_int irq)
{

	if (irq < nitems(irq_sources))
		return (irq_sources[irq]);
	return (NULL);
}

/*
 *  Lookup interrupt source by namespace description.
 */
static struct arm_irqsrc *
isrc_namespace_lookup(device_t dev, uint16_t type, uint16_t num)
{
	u_int irq;
	struct arm_irqsrc *isrc;

	mtx_assert(&isrc_table_lock, MA_OWNED);

	for (irq = 0; irq < nitems(irq_sources); irq++) {
		isrc = irq_sources[irq];
		if (isrc != NULL && isrc->isrc_dev == dev &&
		    isrc->isrc_nspc_type == type && isrc->isrc_nspc_num == num)
			return (isrc);
	}
	return (NULL);
}

/*
 *  Map interrupt source according to namespace into framework. If such mapping
 *  does not exist, create it. Return unique interrupt number (resource handle)
 *  associated with mapped interrupt source.
 */
u_int
arm_namespace_map_irq(device_t dev, uint16_t type, uint16_t num)
{
	struct arm_irqsrc *isrc, *new_isrc;
	int error;

	new_isrc = isrc_alloc(ARM_ISRCT_NAMESPACE, 0);

	mtx_lock(&isrc_table_lock);
	isrc = isrc_namespace_lookup(dev, type, num);
	if (isrc != NULL) {
		mtx_unlock(&isrc_table_lock);
		isrc_free(new_isrc);
		return (isrc->isrc_irq);	/* already mapped */
	}

	error = isrc_alloc_irq_locked(new_isrc);
	if (error != 0) {
		mtx_unlock(&isrc_table_lock);
		isrc_free(new_isrc);
		return (IRQ_INVALID);		/* no space left */
	}

	new_isrc->isrc_dev = dev;
	new_isrc->isrc_nspc_type = type;
	new_isrc->isrc_nspc_num = num;
	mtx_unlock(&isrc_table_lock);

	return (new_isrc->isrc_irq);
}

#ifdef FDT
/*
 *  Lookup interrupt source by FDT description.
 */
static struct arm_irqsrc *
isrc_fdt_lookup(intptr_t xref, pcell_t *cells, u_int ncells)
{
	u_int irq, cellsize;
	struct arm_irqsrc *isrc;

	mtx_assert(&isrc_table_lock, MA_OWNED);

	cellsize = ncells * sizeof(*cells);
	for (irq = 0; irq < nitems(irq_sources); irq++) {
		isrc = irq_sources[irq];
		if (isrc != NULL && isrc->isrc_type == ARM_ISRCT_FDT &&
		    isrc->isrc_xref == xref && isrc->isrc_ncells == ncells &&
		    memcmp(isrc->isrc_cells, cells, cellsize) == 0)
			return (isrc);
	}
	return (NULL);
}

/*
 *  Map interrupt source according to FDT data into framework. If such mapping
 *  does not exist, create it. Return unique interrupt number (resource handle)
 *  associated with mapped interrupt source.
 */
u_int
arm_fdt_map_irq(phandle_t node, pcell_t *cells, u_int ncells)
{
	struct arm_irqsrc *isrc, *new_isrc;
	u_int cellsize;
	intptr_t xref;
	int error;

	xref = (intptr_t)node;	/* It's so simple for now. */

	cellsize = ncells * sizeof(*cells);
	new_isrc = isrc_alloc(ARM_ISRCT_FDT, cellsize);

	mtx_lock(&isrc_table_lock);
	isrc = isrc_fdt_lookup(xref, cells, ncells);
	if (isrc != NULL) {
		mtx_unlock(&isrc_table_lock);
		isrc_free(new_isrc);
		return (isrc->isrc_irq);	/* already mapped */
	}

	error = isrc_alloc_irq_locked(new_isrc);
	if (error != 0) {
		mtx_unlock(&isrc_table_lock);
		isrc_free(new_isrc);
		return (IRQ_INVALID);		/* no space left */
	}

	new_isrc->isrc_xref = xref;
	new_isrc->isrc_ncells = ncells;
	memcpy(new_isrc->isrc_cells, cells, cellsize);
	mtx_unlock(&isrc_table_lock);

	return (new_isrc->isrc_irq);
}
#endif

/*
 *  Register interrupt source into interrupt controller.
 */
static int
isrc_register(struct arm_irqsrc *isrc)
{
	struct arm_pic *pic;
	boolean_t is_percpu;
	int error;

	if (isrc->isrc_flags & ARM_ISRCF_REGISTERED)
		return (0);

	if (isrc->isrc_dev == NULL) {
		pic = pic_lookup(NULL, isrc->isrc_xref);
		if (pic == NULL || pic->pic_dev == NULL)
			return (ESRCH);
		isrc->isrc_dev = pic->pic_dev;
	}

	error = PIC_REGISTER(isrc->isrc_dev, isrc, &is_percpu);
	if (error != 0)
		return (error);

	mtx_lock(&isrc_table_lock);
	isrc->isrc_flags |= ARM_ISRCF_REGISTERED;
	if (is_percpu)
		isrc->isrc_flags |= ARM_ISRCF_PERCPU;
	isrc_update_name(isrc, NULL);
	mtx_unlock(&isrc_table_lock);
	return (0);
}

#ifdef INTR_SOLO
/*
 *  Setup filter into interrupt source.
 */
static int
iscr_setup_filter(struct arm_irqsrc *isrc, const char *name,
    arm_irq_filter_t *filter, void *arg, void **cookiep)
{

	if (filter == NULL)
		return (EINVAL);

	mtx_lock(&isrc_table_lock);
	/*
	 * Make sure that we do not mix the two ways
	 * how we handle interrupt sources.
	 */
	if (isrc->isrc_filter != NULL || isrc->isrc_event != NULL) {
		mtx_unlock(&isrc_table_lock);
		return (EBUSY);
	}
	isrc->isrc_filter = filter;
	isrc->isrc_arg = arg;
	isrc_update_name(isrc, name);
	mtx_unlock(&isrc_table_lock);

	*cookiep = isrc;
	return (0);
}
#endif

/*
 *  Interrupt source pre_ithread method for MI interrupt framework.
 */
static void
arm_isrc_pre_ithread(void *arg)
{
	struct arm_irqsrc *isrc = arg;

	PIC_PRE_ITHREAD(isrc->isrc_dev, isrc);
}

/*
 *  Interrupt source post_ithread method for MI interrupt framework.
 */
static void
arm_isrc_post_ithread(void *arg)
{
	struct arm_irqsrc *isrc = arg;

	PIC_POST_ITHREAD(isrc->isrc_dev, isrc);
}

/*
 *  Interrupt source post_filter method for MI interrupt framework.
 */
static void
arm_isrc_post_filter(void *arg)
{
	struct arm_irqsrc *isrc = arg;

	PIC_POST_FILTER(isrc->isrc_dev, isrc);
}

/*
 *  Interrupt source assign_cpu method for MI interrupt framework.
 */
static int
arm_isrc_assign_cpu(void *arg, int cpu)
{
#ifdef SMP
	struct arm_irqsrc *isrc = arg;
	int error;

	if (isrc->isrc_dev != irq_root_dev)
		return (EINVAL);

	mtx_lock(&isrc_table_lock);
	if (cpu == NOCPU) {
		CPU_ZERO(&isrc->isrc_cpu);
		isrc->isrc_flags &= ~ARM_ISRCF_BOUND;
	} else {
		CPU_SETOF(cpu, &isrc->isrc_cpu);
		isrc->isrc_flags |= ARM_ISRCF_BOUND;
	}

	/*
	 * In NOCPU case, it's up to PIC to either leave ISRC on same CPU or
	 * re-balance it to another CPU or enable it on more CPUs. However,
	 * PIC is expected to change isrc_cpu appropriately to keep us well
	 * informed if the call is successfull.
	 */
	if (irq_assign_cpu) {
		error = PIC_BIND(isrc->isrc_dev, isrc);
		if (error) {
			CPU_ZERO(&isrc->isrc_cpu);
			mtx_unlock(&isrc_table_lock);
			return (error);
		}
	}
	mtx_unlock(&isrc_table_lock);
	return (0);
#else
	return (EOPNOTSUPP);
#endif
}

/*
 *  Create interrupt event for interrupt source.
 */
static int
isrc_event_create(struct arm_irqsrc *isrc)
{
	struct intr_event *ie;
	int error;

	error = intr_event_create(&ie, isrc, 0, isrc->isrc_irq,
	    arm_isrc_pre_ithread, arm_isrc_post_ithread, arm_isrc_post_filter,
	    arm_isrc_assign_cpu, "%s:", isrc->isrc_name);
	if (error)
		return (error);

	mtx_lock(&isrc_table_lock);
	/*
	 * Make sure that we do not mix the two ways
	 * how we handle interrupt sources. Let contested event wins.
	 */
	if (isrc->isrc_filter != NULL || isrc->isrc_event != NULL) {
		mtx_unlock(&isrc_table_lock);
		intr_event_destroy(ie);
		return (isrc->isrc_event != NULL ? EBUSY : 0);
	}
	isrc->isrc_event = ie;
	mtx_unlock(&isrc_table_lock);

	return (0);
}
#ifdef notyet
/*
 *  Destroy interrupt event for interrupt source.
 */
static void
isrc_event_destroy(struct arm_irqsrc *isrc)
{
	struct intr_event *ie;

	mtx_lock(&isrc_table_lock);
	ie = isrc->isrc_event;
	isrc->isrc_event = NULL;
	mtx_unlock(&isrc_table_lock);

	if (ie != NULL)
		intr_event_destroy(ie);
}
#endif
/*
 *  Add handler to interrupt source.
 */
static int
isrc_add_handler(struct arm_irqsrc *isrc, const char *name,
    driver_filter_t filter, driver_intr_t handler, void *arg,
    enum intr_type flags, void **cookiep)
{
	int error;

	if (isrc->isrc_event == NULL) {
		error = isrc_event_create(isrc);
		if (error)
			return (error);
	}

	error = intr_event_add_handler(isrc->isrc_event, name, filter, handler,
	    arg, intr_priority(flags), flags, cookiep);
	if (error == 0) {
		mtx_lock(&isrc_table_lock);
		intrcnt_updatename(isrc);
		mtx_unlock(&isrc_table_lock);
	}

	return (error);
}

/*
 *  Lookup interrupt controller locked.
 */
static struct arm_pic *
pic_lookup_locked(device_t dev, intptr_t xref)
{
	struct arm_pic *pic;

	mtx_assert(&pic_list_lock, MA_OWNED);

	SLIST_FOREACH(pic, &pic_list, pic_next) {
		if (pic->pic_xref != xref)
			continue;
		if (pic->pic_xref != 0 || pic->pic_dev == dev)
			return (pic);
	}
	return (NULL);
}

/*
 *  Lookup interrupt controller.
 */
static struct arm_pic *
pic_lookup(device_t dev, intptr_t xref)
{
	struct arm_pic *pic;

	mtx_lock(&pic_list_lock);
	pic = pic_lookup_locked(dev, xref);
	mtx_unlock(&pic_list_lock);

	return (pic);
}

/*
 *  Create interrupt controller.
 */
static struct arm_pic *
pic_create(device_t dev, intptr_t xref)
{
	struct arm_pic *pic;

	mtx_lock(&pic_list_lock);
	pic = pic_lookup_locked(dev, xref);
	if (pic != NULL) {
		mtx_unlock(&pic_list_lock);
		return (pic);
	}
	pic = malloc(sizeof(*pic), M_INTRNG, M_NOWAIT | M_ZERO);
	pic->pic_xref = xref;
	pic->pic_dev = dev;
	SLIST_INSERT_HEAD(&pic_list, pic, pic_next);
	mtx_unlock(&pic_list_lock);

	return (pic);
}
#ifdef notyet
/*
 *  Destroy interrupt controller.
 */
static void
pic_destroy(device_t dev, intptr_t xref)
{
	struct arm_pic *pic;

	mtx_lock(&pic_list_lock);
	pic = pic_lookup_locked(dev, xref);
	if (pic == NULL) {
		mtx_unlock(&pic_list_lock);
		return;
	}
	SLIST_REMOVE(&pic_list, pic, arm_pic, pic_next);
	mtx_unlock(&pic_list_lock);

	free(pic, M_INTRNG);
}
#endif
/*
 *  Register interrupt controller.
 */
int
arm_pic_register(device_t dev, intptr_t xref)
{
	struct arm_pic *pic;

	pic = pic_create(dev, xref);
	if (pic == NULL)
		return (ENOMEM);
	if (pic->pic_dev != dev)
		return (EINVAL);	/* XXX it could be many things. */

	debugf("PIC %p registered for %s <xref %x>\n", pic,
	    device_get_nameunit(dev), xref);
	return (0);
}

/*
 *  Unregister interrupt controller.
 */
int
arm_pic_unregister(device_t dev, intptr_t xref)
{

	panic("%s: not implemented", __func__);
}

/*
 *  Mark interrupt controller (itself) as a root one.
 *
 *  Note that only an interrupt controller can really know its position
 *  in interrupt controller's tree. So root PIC must claim itself as a root.
 *
 *  In FDT case, according to ePAPR approved version 1.1 from 08 April 2011,
 *  page 30:
 *    "The root of the interrupt tree is determined when traversal
 *     of the interrupt tree reaches an interrupt controller node without
 *     an interrupts property and thus no explicit interrupt parent."
 */
int
arm_pic_claim_root(device_t dev, intptr_t xref, arm_irq_filter_t *filter,
    void *arg, u_int ipicount)
{
	int error;
	u_int rootirq;

	if (pic_lookup(dev, xref) == NULL) {
		device_printf(dev, "not registered\n");
		return (EINVAL);
	}
	if (filter == NULL) {
		device_printf(dev, "filter missing\n");
		return (EINVAL);
	}

	/*
	 * Only one interrupt controllers could be on the root for now.
	 * Note that we further suppose that there is not threaded interrupt
	 * routine (handler) on the root. See arm_irq_handler().
	 */
	if (irq_root_dev != NULL) {
		device_printf(dev, "another root already set\n");
		return (EBUSY);
	}

	rootirq = arm_namespace_map_irq(device_get_parent(dev), 0, 0);
	if (rootirq == IRQ_INVALID) {
		device_printf(dev, "failed to map an irq for the root pic\n");
		return (ENOMEM);
	}

        /* Create the isrc. */
	irq_root_isrc = isrc_lookup(rootirq);

        /* XXX "register" with the PIC.  We are the "pic" here, so fake it. */
	irq_root_isrc->isrc_flags |= ARM_ISRCF_REGISTERED;

	error = arm_irq_add_handler(device_get_parent(dev), 
		(void*)filter, NULL, arg, rootirq, INTR_TYPE_CLK, NULL);
	if (error != 0) {
		device_printf(dev, "failed to install root pic handler\n");
		return (error);
	}
	irq_root_dev = dev;
	irq_root_filter = filter;
	irq_root_arg = arg;
	irq_root_ipicount = ipicount;

	debugf("irq root set to %s\n", device_get_nameunit(dev));
	return (0);
}

int
arm_irq_add_handler(device_t dev, driver_filter_t filt, driver_intr_t hand,
    void *arg, u_int irq, int flags, void **cookiep)
{
	const char *name;
	struct arm_irqsrc *isrc;
	int error;

	name = device_get_nameunit(dev);

#ifdef INTR_SOLO
	/*
	 * Standard handling is done thru MI interrupt framework. However,
	 * some interrupts could request solely own special handling. This
	 * non standard handling can be used for interrupt controllers without
	 * handler (filter only), so in case that interrupt controllers are
	 * chained, MI interrupt framework is called only in leaf controller.
	 *
	 * Note that root interrupt controller routine is served as well,
	 * however in arm_irq_handler(), i.e. main system dispatch routine.
	 */
	if (flags & INTR_SOLO && hand != NULL) {
		debugf("irq %u cannot solo on %s\n", irq, name);
		return (EINVAL);
	}
#endif

	isrc = isrc_lookup(irq);
	if (isrc == NULL) {
		debugf("irq %u without source on %s\n", irq, name);
		return (EINVAL);
	}

	error = isrc_register(isrc);
	if (error != 0) {
		debugf("irq %u map error %d on %s\n", irq, error, name);
		return (error);
	}

#ifdef INTR_SOLO
	if (flags & INTR_SOLO) {
		error = iscr_setup_filter(isrc, name, (arm_irq_filter_t *)filt,
		    arg, cookiep);
		debugf("irq %u setup filter error %d on %s\n", irq, error,
		    name);
	} else
#endif
		{
		error = isrc_add_handler(isrc, name, filt, hand, arg, flags,
		    cookiep);
		debugf("irq %u add handler error %d on %s\n", irq, error, name);
	}
	if (error != 0)
		return (error);

	mtx_lock(&isrc_table_lock);
	isrc->isrc_handlers++;
	if (isrc->isrc_handlers == 1) {
		PIC_ENABLE_INTR(isrc->isrc_dev, isrc);
		PIC_ENABLE_SOURCE(isrc->isrc_dev, isrc);
	}
	mtx_unlock(&isrc_table_lock);
	return (0);
}

int
arm_irq_remove_handler(device_t dev, u_int irq, void *cookie)
{
	struct arm_irqsrc *isrc;
	int error;

	isrc = isrc_lookup(irq);
	if (isrc == NULL || isrc->isrc_handlers == 0)
		return (EINVAL);

	if (isrc->isrc_filter != NULL) {
		if (isrc != cookie)
			return (EINVAL);

		mtx_lock(&isrc_table_lock);
		isrc->isrc_filter = NULL;
		isrc->isrc_arg = NULL;
		isrc->isrc_handlers = 0;
		PIC_DISABLE_SOURCE(isrc->isrc_dev, isrc);
		PIC_DISABLE_INTR(isrc->isrc_dev, isrc);
		isrc_update_name(isrc, NULL);
		mtx_unlock(&isrc_table_lock);
		return (0);
	}

	if (isrc != intr_handler_source(cookie))
		return (EINVAL);

	error = intr_event_remove_handler(cookie);
	if (error == 0) {
		mtx_lock(&isrc_table_lock);
		isrc->isrc_handlers--;
		if (isrc->isrc_handlers == 0) {
			PIC_DISABLE_SOURCE(isrc->isrc_dev, isrc);
			PIC_DISABLE_INTR(isrc->isrc_dev, isrc);
		}
		intrcnt_updatename(isrc);
		mtx_unlock(&isrc_table_lock);
	}
	return (error);
}

int
arm_irq_config(u_int irq, enum intr_trigger trig, enum intr_polarity pol)
{
	struct arm_irqsrc *isrc;

	isrc = isrc_lookup(irq);
	if (isrc == NULL)
		return (EINVAL);

	if (isrc->isrc_handlers != 0)
		return (EBUSY);	/* interrrupt is enabled (active) */

	/*
	 * Once an interrupt is enabled, we do not change its configuration.
	 * A controller PIC_ENABLE_INTR() method is called when an interrupt
	 * is going to be enabled. In this method, a controller should setup
	 * the interrupt according to saved configuration parameters.
	 */
	isrc->isrc_trig = trig;
	isrc->isrc_pol = pol;

	return (0);
}

int
arm_irq_describe(u_int irq, void *cookie, const char *descr)
{
	struct arm_irqsrc *isrc;
	int error;

	isrc = isrc_lookup(irq);
	if (isrc == NULL || isrc->isrc_handlers == 0)
		return (EINVAL);

	if (isrc->isrc_filter != NULL) {
		if (isrc != cookie)
			return (EINVAL);

		mtx_lock(&isrc_table_lock);
		isrc_update_name(isrc, descr);
		mtx_unlock(&isrc_table_lock);
		return (0);
	}

	error = intr_event_describe_handler(isrc->isrc_event, cookie, descr);
	if (error == 0) {
		mtx_lock(&isrc_table_lock);
		intrcnt_updatename(isrc);
		mtx_unlock(&isrc_table_lock);
	}
	return (error);
}

#ifdef SMP
int
arm_irq_bind(u_int irq, int cpu)
{
	struct arm_irqsrc *isrc;

	isrc = isrc_lookup(irq);
	if (isrc == NULL || isrc->isrc_handlers == 0)
		return (EINVAL);

	if (isrc->isrc_filter != NULL)
		return (arm_isrc_assign_cpu(isrc, cpu));

	return (intr_event_bind(isrc->isrc_event, cpu));
}

/*
 * Return the CPU that the next interrupt source should use.
 * For now just returns the next CPU according to round-robin.
 */
u_int
arm_irq_next_cpu(u_int last_cpu, cpuset_t *cpumask)
{

	if (!irq_assign_cpu || mp_ncpus == 1)
		return (PCPU_GET(cpuid));

	do {
		last_cpu++;
		if (last_cpu > mp_maxid)
			last_cpu = 0;
	} while (!CPU_ISSET(last_cpu, cpumask));
	return (last_cpu);
}

/*
 *  Distribute all the interrupt sources among the available
 *  CPUs once the AP's have been launched.
 */
static void
arm_irq_shuffle(void *arg __unused)
{
	struct arm_irqsrc *isrc;
	u_int i;

	if (mp_ncpus == 1)
		return;

	mtx_lock(&isrc_table_lock);
	irq_assign_cpu = TRUE;
	for (i = 0; i < NIRQ; i++) {
		isrc = irq_sources[i];
		if (isrc == NULL || isrc->isrc_handlers == 0 ||
		    isrc->isrc_flags & ARM_ISRCF_PERCPU)
			continue;

		if (isrc->isrc_event != NULL &&
		    isrc->isrc_flags & ARM_ISRCF_BOUND &&
		    isrc->isrc_event->ie_cpu != CPU_FFS(&isrc->isrc_cpu) - 1)
			panic("%s: CPU inconsistency", __func__);

		if ((isrc->isrc_flags & ARM_ISRCF_BOUND) == 0)
			CPU_ZERO(&isrc->isrc_cpu); /* start again */

		/*
		 * We are in wicked position here if the following call fails
		 * for bound ISRC. The best thing we can do is to clear
		 * isrc_cpu so inconsistency with ie_cpu will be detectable.
		 */
		if (PIC_BIND(isrc->isrc_dev, isrc) != 0)
			CPU_ZERO(&isrc->isrc_cpu);
	}
	mtx_unlock(&isrc_table_lock);
}
SYSINIT(arm_irq_shuffle, SI_SUB_SMP, SI_ORDER_SECOND, arm_irq_shuffle, NULL);

#else
u_int
arm_irq_next_cpu(u_int current_cpu, cpuset_t *cpumask)
{

	return (PCPU_GET(cpuid));
}
#endif

void dosoftints(void);
void
dosoftints(void)
{
}

/*
 * arm_irq_memory_barrier()
 *
 * Ensure all writes to device memory have reached devices before proceeding.
 *
 * This is intended to be called from the post-filter and post-thread routines
 * of an interrupt controller implementation.  A peripheral device driver should
 * use bus_space_barrier() if it needs to ensure a write has reached the
 * hardware for some reason other than clearing interrupt conditions.
 *
 * The need for this function arises from the ARM weak memory ordering model.
 * Writes to locations mapped with the Device attribute bypass any caches, but
 * are buffered.  Multiple writes to the same device will be observed by that
 * device in the order issued by the cpu.  Writes to different devices may
 * appear at those devices in a different order than issued by the cpu.  That
 * is, if the cpu writes to device A then device B, the write to device B could
 * complete before the write to device A.
 *
 * Consider a typical device interrupt handler which services the interrupt and
 * writes to a device status-acknowledge register to clear the interrupt before
 * returning.  That write is posted to the L2 controller which "immediately"
 * places it in a store buffer and automatically drains that buffer.  This can
 * be less immediate than you'd think... There may be no free slots in the store
 * buffers, so an existing buffer has to be drained first to make room.  The
 * target bus may be busy with other traffic (such as DMA for various devices),
 * delaying the drain of the store buffer for some indeterminate time.  While
 * all this delay is happening, execution proceeds on the CPU, unwinding its way
 * out of the interrupt call stack to the point where the interrupt driver code
 * is ready to EOI and unmask the interrupt.  The interrupt controller may be
 * accessed via a faster bus than the hardware whose handler just ran; the write
 * to unmask and EOI the interrupt may complete quickly while the device write
 * to ack and clear the interrupt source is still lingering in a store buffer
 * waiting for access to a slower bus.  With the interrupt unmasked at the
 * interrupt controller but still active at the device, as soon as interrupts
 * are enabled on the core the device re-interrupts immediately: now you've got
 * a spurious interrupt on your hands.
 *
 * The right way to fix this problem is for every device driver to use the
 * proper bus_space_barrier() calls in its interrupt handler.  For ARM a single
 * barrier call at the end of the handler would work.  This would have to be
 * done to every driver in the system, not just arm-specific drivers.
 *
 * Another potential fix is to map all device memory as Strongly-Ordered rather
 * than Device memory, which takes the store buffers out of the picture.  This
 * has a pretty big impact on overall system performance, because each strongly
 * ordered memory access causes all L2 store buffers to be drained.
 *
 * A compromise solution is to have the interrupt controller implementation call
 * this function to establish a barrier between writes to the interrupt-source
 * device and writes to the interrupt controller device.
 *
 * This takes the interrupt number as an argument, and currently doesn't use it.
 * The plan is that maybe some day there is a way to flag certain interrupts as
 * "memory barrier safe" and we can avoid this overhead with them.
 */
void
arm_irq_memory_barrier(uintptr_t irq)
{

	dsb();
	cpu_l2cache_drain_writebuf();
}

#ifdef SMP
/*
 *  Lookup IPI source.
 */
static struct arm_irqsrc *
arm_ipi_lookup(u_int ipi)
{

	if (ipi >= ARM_IPI_COUNT)
		panic("%s: no such IPI %u", __func__, ipi);

	return (&ipi_sources[ipi]);
}

/*
 *  ARM interrupt controller dispatch function for IPIs. It should
 *  be called straight from the interrupt controller, when associated
 *  interrupt source is learned. Or from anybody who has an interrupt
 *  source mapped.
 */
void
arm_ipi_dispatch(struct arm_irqsrc *isrc, struct trapframe *tf)
{
	void *arg;

	KASSERT(isrc != NULL, ("%s: no source", __func__));

	isrc_increment_ipi_count(isrc, PCPU_GET(cpuid));

	/*
	 * Supply ipi filter with trapframe argument
	 * if none is registered.
	 */
	arg = isrc->isrc_arg != NULL ? isrc->isrc_arg : tf;
	isrc->isrc_ipifilter(arg);
}

/*
 *  Map IPI into interrupt controller.
 *
 *  Not SMP coherent.
 */
static int
ipi_map(struct arm_irqsrc *isrc, u_int ipi)
{
	boolean_t is_percpu;
	int error;

	if (ipi >= ARM_IPI_COUNT)
		panic("%s: no such IPI %u", __func__, ipi);

	KASSERT(irq_root_dev != NULL, ("%s: no root attached", __func__));

	isrc->isrc_type = ARM_ISRCT_NAMESPACE;
	isrc->isrc_nspc_type = ARM_IRQ_NSPC_IPI;
	isrc->isrc_nspc_num = ipi_next_num;

	error = PIC_REGISTER(irq_root_dev, isrc, &is_percpu);

	debugf("ipi %u mapped to %u on %s - error %d\n", ipi, ipi_next_num,
	    device_get_nameunit(irq_root_dev), error);

	if (error == 0) {
		isrc->isrc_dev = irq_root_dev;
		ipi_next_num++;
	}
	return (error);
}

/*
 *  Setup IPI handler to interrupt source.
 *
 *  Note that there could be more ways how to send and receive IPIs
 *  on a platform like fast interrupts for example. In that case,
 *  one can call this function with ASIF_NOALLOC flag set and then
 *  call arm_ipi_dispatch() when appropriate.
 *
 *  Not SMP coherent.
 */
int
arm_ipi_set_handler(u_int ipi, const char *name, arm_ipi_filter_t *filter,
    void *arg, u_int flags)
{
	struct arm_irqsrc *isrc;
	int error;

	if (filter == NULL)
		return(EINVAL);

	isrc = arm_ipi_lookup(ipi);
	if (isrc->isrc_ipifilter != NULL)
		return (EEXIST);

	if ((flags & AISHF_NOALLOC) == 0) {
		error = ipi_map(isrc, ipi);
		if (error != 0)
			return (error);
	}

	isrc->isrc_ipifilter = filter;
	isrc->isrc_arg = arg;
	isrc->isrc_handlers = 1;
	isrc_setup_ipi_counters(isrc, name);

	if (isrc->isrc_dev != NULL) {
		mtx_lock(&isrc_table_lock);
		PIC_ENABLE_INTR(isrc->isrc_dev, isrc);
		PIC_ENABLE_SOURCE(isrc->isrc_dev, isrc);
		mtx_unlock(&isrc_table_lock);
	}
	return (0);
}

/*
 *  Send IPI thru interrupt controller.
 */
void
pic_ipi_send(cpuset_t cpus, u_int ipi)
{
	struct arm_irqsrc *isrc;

	isrc = arm_ipi_lookup(ipi);

	KASSERT(irq_root_dev != NULL, ("%s: no root attached", __func__));
	PIC_IPI_SEND(irq_root_dev, isrc, cpus);
}

/*
 *  Init interrupt controller on another CPU.
 */
void
arm_pic_init_secondary(void)
{

	/*
	 * QQQ: Only root PIC is aware of other CPUs ???
	 */
	KASSERT(irq_root_dev != NULL, ("%s: no root attached", __func__));

	//mtx_lock(&isrc_table_lock);
	PIC_INIT_SECONDARY(irq_root_dev);
	//mtx_unlock(&isrc_table_lock);
}
#endif

#ifdef DDB
DB_SHOW_COMMAND(irqs, db_show_irqs)
{
	u_int i, irqsum;
	struct arm_irqsrc *isrc;

#ifdef SMP
	for (i = 0; i <= mp_maxid; i++) {
		struct pcpu *pc;
		u_int ipi, ipisum;

		pc = pcpu_find(i);
		if (pc != NULL) {
			for (ipisum = 0, ipi = 0; ipi < ARM_IPI_COUNT; ipi++) {
				isrc = arm_ipi_lookup(ipi);
				if (isrc->isrc_count != NULL)
					ipisum += isrc->isrc_count[i];
			}
			printf ("cpu%u: total %u ipis %u\n", i,
			    pc->pc_cnt.v_intr, ipisum);
		}
	}
	db_printf("\n");
#endif

	for (irqsum = 0, i = 0; i < NIRQ; i++) {
		isrc = irq_sources[i];
		if (isrc == NULL)
			continue;

		db_printf("irq%-3u <%s>: cpu %02lx%s cnt %lu\n", i,
		    isrc->isrc_name, isrc->isrc_cpu.__bits[0],
		    isrc->isrc_flags & ARM_ISRCF_BOUND ? " (bound)" : "",
		    isrc->isrc_count[0]);
		irqsum += isrc->isrc_count[0];
	}
	db_printf("irq total %u\n", irqsum);
}
#endif
