/*-
 * Copyright (c) 2015-2016 Svatopluk Kraus
 * Copyright (c) 2015-2016 Michal Meloun
 * All rights reserved.
 * Copyright (c) 2015-2016 The FreeBSD Foundation
 * Copyright (c) 2021 Jessica Clarke <jrtc27@FreeBSD.org>
 *
 * Portions of this software were developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
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
 */

#include <sys/cdefs.h>
/*
 *	New-style Interrupt Framework
 *
 *  TODO: - add support for disconnected PICs.
 *        - to support IPI (PPI) enabling on other CPUs if already started.
 *        - to complete things for removable PICs.
 */

#include "opt_ddb.h"

#include <sys/bitstring.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cpuset.h>
#include <sys/interrupt.h>
#include <sys/intr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/stdarg.h>
#include <sys/sysctl.h>

#include <machine/atomic.h>
#include <machine/smp.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include "pic_if.h"

#define	INTRNAME_LEN	(2*MAXCOMLEN + 1)

MALLOC_DECLARE(M_INTRNG);
MALLOC_DEFINE(M_INTRNG, "intr", "intr interrupt handling");

#ifdef SMP
#define INTR_IPI_NAMELEN	(MAXCOMLEN + 1)

struct intr_ipi {
	intr_ipi_handler_t	*ii_handler;
	void			*ii_handler_arg;
	struct intr_irqsrc	*ii_isrc;
	char			ii_name[INTR_IPI_NAMELEN];
	u_long			*ii_count;
};

static device_t intr_ipi_dev;
static u_int intr_ipi_dev_priority;
static bool intr_ipi_dev_frozen;
#endif

/* Interrupt source definition. */
static struct mtx isrc_table_lock;
static struct intr_irqsrc **irq_sources;
static u_int irq_next_free;

#ifdef SMP
#ifdef EARLY_AP_STARTUP
static bool irq_assign_cpu = true;
#else
static bool irq_assign_cpu = false;
#endif

static struct intr_ipi ipi_sources[INTR_IPI_COUNT];
#endif

u_int intr_nirq = NIRQ;
SYSCTL_UINT(_machdep, OID_AUTO, nirq, CTLFLAG_RDTUN, &intr_nirq, 0,
    "Number of IRQs");

/* Data for MI statistics reporting. */
u_long *intrcnt;
char *intrnames;
size_t sintrcnt;
size_t sintrnames;
int nintrcnt;
static bitstr_t *intrcnt_bitmap;

/*
 *  Interrupt framework initialization routine.
 */
static void
intr_irq_init(void *dummy __unused)
{

	mtx_init(&isrc_table_lock, "intr isrc table", NULL, MTX_DEF);

	/*
	 * - 2 counters for each I/O interrupt.
	 * - mp_maxid + 1 counters for each IPI counters for SMP.
	 */
	nintrcnt = intr_nirq * 2;
#ifdef SMP
	nintrcnt += INTR_IPI_COUNT * (mp_maxid + 1);
#endif

	intrcnt = mallocarray(nintrcnt, sizeof(u_long), M_INTRNG,
	    M_WAITOK | M_ZERO);
	intrnames = mallocarray(nintrcnt, INTRNAME_LEN, M_INTRNG,
	    M_WAITOK | M_ZERO);
	sintrcnt = nintrcnt * sizeof(u_long);
	sintrnames = nintrcnt * INTRNAME_LEN;

	/* Allocate the bitmap tracking counter allocations. */
	intrcnt_bitmap = bit_alloc(nintrcnt, M_INTRNG, M_WAITOK | M_ZERO);

	irq_sources = mallocarray(intr_nirq, sizeof(struct intr_irqsrc*),
	    M_INTRNG, M_WAITOK | M_ZERO);
}
SYSINIT(intr_irq_init, SI_SUB_INTR, SI_ORDER_FIRST, intr_irq_init, NULL);

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
intrcnt_updatename(struct intr_irqsrc *isrc)
{

	/* QQQ: What about stray counter name? */
	mtx_assert(&isrc_table_lock, MA_OWNED);
	intrcnt_setname(isrc->isrc_event->ie_fullname, isrc->isrc_index);
}

/*
 *  Virtualization for interrupt source interrupt counter increment.
 */
static inline void
isrc_increment_count(struct intr_irqsrc *isrc)
{

	if (isrc->isrc_flags & INTR_ISRCF_PPI)
		atomic_add_long(&isrc->isrc_count[0], 1);
	else
		isrc->isrc_count[0]++;
}

/*
 *  Virtualization for interrupt source interrupt stray counter increment.
 */
static inline void
isrc_increment_straycount(struct intr_irqsrc *isrc)
{

	isrc->isrc_count[1]++;
}

/*
 *  Virtualization for interrupt source interrupt name update.
 */
static void
isrc_update_name(struct intr_irqsrc *isrc, const char *name)
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
isrc_setup_counters(struct intr_irqsrc *isrc)
{
	int index;

	mtx_assert(&isrc_table_lock, MA_OWNED);

	/*
	 * Allocate two counter values, the second tracking "stray" interrupts.
	 */
	bit_ffc_area(intrcnt_bitmap, nintrcnt, 2, &index);
	if (index == -1)
		panic("Failed to allocate 2 counters. Array exhausted?");
	bit_nset(intrcnt_bitmap, index, index + 1);
	isrc->isrc_index = index;
	isrc->isrc_count = &intrcnt[index];
	isrc_update_name(isrc, NULL);
}

/*
 *  Virtualization for interrupt source interrupt counters release.
 */
static void
isrc_release_counters(struct intr_irqsrc *isrc)
{
	int idx = isrc->isrc_index;

	mtx_assert(&isrc_table_lock, MA_OWNED);

	bit_nclear(intrcnt_bitmap, idx, idx + 1);
}

/*
 *  interrupt controller dispatch function for interrupts. It should
 *  be called straight from the interrupt controller, when associated interrupt
 *  source is learned.
 */
int
intr_isrc_dispatch(struct intr_irqsrc *isrc, struct trapframe *tf)
{

	KASSERT(isrc != NULL, ("%s: no source", __func__));

	if ((isrc->isrc_flags & INTR_ISRCF_IPI) == 0)
		isrc_increment_count(isrc);

#ifdef INTR_SOLO
	if (isrc->isrc_filter != NULL) {
		int error;
		error = isrc->isrc_filter(isrc->isrc_arg, tf);
		PIC_POST_FILTER(isrc->isrc_dev, isrc);
		if (error == FILTER_HANDLED)
			return (0);
	} else
#endif
	if (isrc->isrc_event != NULL) {
		if (intr_event_handle(isrc->isrc_event, tf) == 0)
			return (0);
	}

	if ((isrc->isrc_flags & INTR_ISRCF_IPI) == 0)
		isrc_increment_straycount(isrc);
	return (EINVAL);
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
static inline int
isrc_alloc_irq(struct intr_irqsrc *isrc)
{
	u_int irq;

	mtx_assert(&isrc_table_lock, MA_OWNED);

	if (irq_next_free >= intr_nirq)
		return (ENOSPC);

	for (irq = irq_next_free; irq < intr_nirq; irq++) {
		if (irq_sources[irq] == NULL)
			goto found;
	}
	for (irq = 0; irq < irq_next_free; irq++) {
		if (irq_sources[irq] == NULL)
			goto found;
	}

	irq_next_free = intr_nirq;
	return (ENOSPC);

found:
	isrc->isrc_irq = irq;
	irq_sources[irq] = isrc;

	irq_next_free = irq + 1;
	if (irq_next_free >= intr_nirq)
		irq_next_free = 0;
	return (0);
}

/*
 *  Free unique interrupt number (resource handle) from interrupt source.
 */
static inline int
isrc_free_irq(struct intr_irqsrc *isrc)
{

	mtx_assert(&isrc_table_lock, MA_OWNED);

	if (isrc->isrc_irq >= intr_nirq)
		return (EINVAL);
	if (irq_sources[isrc->isrc_irq] != isrc)
		return (EINVAL);

	irq_sources[isrc->isrc_irq] = NULL;
	isrc->isrc_irq = INTR_IRQ_INVALID;	/* just to be safe */

	/*
	 * If we are recovering from the state irq_sources table is full,
	 * then the following allocation should check the entire table. This
	 * will ensure maximum separation of allocation order from release
	 * order.
	 */
	if (irq_next_free >= intr_nirq)
		irq_next_free = 0;

	return (0);
}

/*
 *  Initialize interrupt source and register it into global interrupt table.
 */
int
intr_isrc_register(struct intr_irqsrc *isrc, device_t dev, u_int flags,
    const char *fmt, ...)
{
	int error;
	va_list ap;

	bzero(isrc, sizeof(struct intr_irqsrc));
	isrc->isrc_dev = dev;
	isrc->isrc_irq = INTR_IRQ_INVALID;	/* just to be safe */
	isrc->isrc_flags = flags;

	va_start(ap, fmt);
	vsnprintf(isrc->isrc_name, INTR_ISRC_NAMELEN, fmt, ap);
	va_end(ap);

	mtx_lock(&isrc_table_lock);
	error = isrc_alloc_irq(isrc);
	if (error != 0) {
		mtx_unlock(&isrc_table_lock);
		return (error);
	}
	/*
	 * Setup interrupt counters, but not for IPI sources. Those are setup
	 * later and only for used ones (up to INTR_IPI_COUNT) to not exhaust
	 * our counter pool.
	 */
	if ((isrc->isrc_flags & INTR_ISRCF_IPI) == 0)
		isrc_setup_counters(isrc);
	mtx_unlock(&isrc_table_lock);
	return (0);
}

/*
 *  Deregister interrupt source from global interrupt table.
 */
int
intr_isrc_deregister(struct intr_irqsrc *isrc)
{
	int error;

	mtx_lock(&isrc_table_lock);
	if ((isrc->isrc_flags & INTR_ISRCF_IPI) == 0)
		isrc_release_counters(isrc);
	error = isrc_free_irq(isrc);
	mtx_unlock(&isrc_table_lock);
	return (error);
}

#ifdef INTR_SOLO
/*
 *  Setup filter into interrupt source.
 */
static int
iscr_setup_filter(struct intr_irqsrc *isrc, const char *name,
    intr_irq_filter_t *filter, void *arg, void **cookiep)
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
intr_isrc_pre_ithread(void *arg)
{
	struct intr_irqsrc *isrc = arg;

	PIC_PRE_ITHREAD(isrc->isrc_dev, isrc);
}

/*
 *  Interrupt source post_ithread method for MI interrupt framework.
 */
static void
intr_isrc_post_ithread(void *arg)
{
	struct intr_irqsrc *isrc = arg;

	PIC_POST_ITHREAD(isrc->isrc_dev, isrc);
}

/*
 *  Interrupt source post_filter method for MI interrupt framework.
 */
static void
intr_isrc_post_filter(void *arg)
{
	struct intr_irqsrc *isrc = arg;

	PIC_POST_FILTER(isrc->isrc_dev, isrc);
}

/*
 *  Interrupt source assign_cpu method for MI interrupt framework.
 */
static int
intr_isrc_assign_cpu(void *arg, int cpu)
{
	struct intr_irqsrc *isrc = arg;

	return (intr_assign_irq(isrc, cpu, irq_assign_cpu));
}

/*
 *  Create interrupt event for interrupt source.
 */
static int
isrc_event_create(struct intr_irqsrc *isrc)
{
	struct intr_event *ie;
	int error;

	error = intr_event_create(&ie, isrc, 0, isrc->isrc_irq,
	    intr_isrc_pre_ithread, intr_isrc_post_ithread, intr_isrc_post_filter,
	    intr_isrc_assign_cpu, "%s:", isrc->isrc_name);
	if (error)
		return (error);

	mtx_lock(&isrc_table_lock);
	/*
	 * Make sure that we do not mix the two ways
	 * how we handle interrupt sources. Let contested event wins.
	 */
#ifdef INTR_SOLO
	if (isrc->isrc_filter != NULL || isrc->isrc_event != NULL) {
#else
	if (isrc->isrc_event != NULL) {
#endif
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
isrc_event_destroy(struct intr_irqsrc *isrc)
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
int
intr_add_handler(struct intr_irqsrc *isrc, const char *name,
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
 *  Describe an interrupt
 */
int
intr_describe(struct intr_irqsrc *isrc, void *cookie, const char *descr)
{
	int error;

#ifdef INTR_SOLO
	if (isrc->isrc_filter != NULL) {
		if (isrc != cookie)
			return (EINVAL);

		mtx_lock(&isrc_table_lock);
		isrc_update_name(isrc, descr);
		mtx_unlock(&isrc_table_lock);
		return (0);
	}
#endif
	if (cookie != NULL)
		error = intr_event_describe_handler(isrc->isrc_event, cookie,
		    descr);
	else
		error = 0;
	if (error == 0) {
		mtx_lock(&isrc_table_lock);
		intrcnt_updatename(isrc);
		mtx_unlock(&isrc_table_lock);
	}
	return (error);
}

#ifdef SMP
/*
 * Return the CPU that the next interrupt source should use.
 * For now just returns the next CPU according to round-robin.
 */
u_int
intr_irq_next_cpu(u_int last_cpu, cpuset_t *cpumask)
{
	u_int cpu;

	KASSERT(!CPU_EMPTY(cpumask), ("%s: Empty CPU mask", __func__));
	if (!irq_assign_cpu || mp_ncpus == 1) {
		cpu = PCPU_GET(cpuid);

		if (CPU_ISSET(cpu, cpumask))
			return (curcpu);

		return (CPU_FFS(cpumask) - 1);
	}

	do {
		last_cpu++;
		if (last_cpu > mp_maxid)
			last_cpu = 0;
	} while (!CPU_ISSET(last_cpu, cpumask));
	return (last_cpu);
}

#ifndef EARLY_AP_STARTUP
/*
 *  Distribute all the interrupt sources among the available
 *  CPUs once the AP's have been launched.
 */
static void
intr_irq_shuffle(void *arg __unused)
{
	struct intr_irqsrc *isrc;
	u_int i;

	if (mp_ncpus == 1)
		return;

	mtx_lock(&isrc_table_lock);
	irq_assign_cpu = true;
	for (i = 0; i < intr_nirq; i++) {
		isrc = irq_sources[i];
		if (isrc == NULL || isrc->isrc_handlers == 0 ||
		    isrc->isrc_flags & (INTR_ISRCF_PPI | INTR_ISRCF_IPI))
			continue;

		if (isrc->isrc_event != NULL &&
		    isrc->isrc_flags & INTR_ISRCF_BOUND &&
		    isrc->isrc_event->ie_cpu != CPU_FFS(&isrc->isrc_cpu) - 1)
			panic("%s: CPU inconsistency", __func__);

		if ((isrc->isrc_flags & INTR_ISRCF_BOUND) == 0)
			CPU_ZERO(&isrc->isrc_cpu); /* start again */

		/*
		 * We are in wicked position here if the following call fails
		 * for bound ISRC. The best thing we can do is to clear
		 * isrc_cpu so inconsistency with ie_cpu will be detectable.
		 */
		if (PIC_BIND_INTR(isrc->isrc_dev, isrc) != 0)
			CPU_ZERO(&isrc->isrc_cpu);
	}
	mtx_unlock(&isrc_table_lock);
}
SYSINIT(intr_irq_shuffle, SI_SUB_SMP, SI_ORDER_SECOND, intr_irq_shuffle, NULL);
#endif /* !EARLY_AP_STARTUP */

#else
u_int
intr_irq_next_cpu(u_int current_cpu, cpuset_t *cpumask)
{

	return (PCPU_GET(cpuid));
}
#endif /* SMP */

void dosoftints(void);
void
dosoftints(void)
{
}

#ifdef DDB
DB_SHOW_COMMAND_FLAGS(irqs, db_show_irqs, DB_CMD_MEMSAFE)
{
	u_int i, irqsum;
	u_long num;
	struct intr_irqsrc *isrc;

	for (irqsum = 0, i = 0; i < intr_nirq; i++) {
		isrc = irq_sources[i];
		if (isrc == NULL)
			continue;

		num = isrc->isrc_count != NULL ? isrc->isrc_count[0] : 0;
		db_printf("irq%-3u <%s>: cpu %02lx%s cnt %lu\n", i,
		    isrc->isrc_name, isrc->isrc_cpu.__bits[0],
		    isrc->isrc_flags & INTR_ISRCF_BOUND ? " (bound)" : "", num);
		irqsum += num;
	}
	db_printf("irq total %u\n", irqsum);
}
#endif

#ifdef SMP
/* Virtualization for interrupt source IPI counter increment. */
static inline void
intr_ipi_increment_count(u_long *counter, u_int cpu)
{

	KASSERT(cpu < mp_maxid + 1, ("%s: too big cpu %u", __func__, cpu));
	counter[cpu]++;
}

/*
 *  Virtualization for interrupt source IPI counters setup.
 */
static u_long *
intr_ipi_setup_counters(const char *name)
{
	u_int index, i;
	char str[INTRNAME_LEN];

	mtx_lock(&isrc_table_lock);

	/*
	 * We should never have a problem finding mp_maxid + 1 contiguous
	 * counters, in practice. Interrupts will be allocated sequentially
	 * during boot, so the array should fill from low to high index. Once
	 * reserved, the IPI counters will never be released. Similarly, we
	 * will not need to allocate more IPIs once the system is running.
	 */
	bit_ffc_area(intrcnt_bitmap, nintrcnt, mp_maxid + 1, &index);
	if (index == -1)
		panic("Failed to allocate %d counters. Array exhausted?",
		    mp_maxid + 1);
	bit_nset(intrcnt_bitmap, index, index + mp_maxid);
	for (i = 0; i < mp_maxid + 1; i++) {
		snprintf(str, INTRNAME_LEN, "cpu%d:%s", i, name);
		intrcnt_setname(str, index + i);
	}
	mtx_unlock(&isrc_table_lock);
	return (&intrcnt[index]);
}

/*
 *  Lookup IPI source.
 */
static struct intr_ipi *
intr_ipi_lookup(u_int ipi)
{

	if (ipi >= INTR_IPI_COUNT)
		panic("%s: no such IPI %u", __func__, ipi);

	return (&ipi_sources[ipi]);
}

int
intr_ipi_pic_register(device_t dev, u_int priority)
{
	if (intr_ipi_dev_frozen) {
		device_printf(dev, "IPI device already frozen");
		return (EBUSY);
	}

	if (intr_ipi_dev == NULL || priority > intr_ipi_dev_priority) {
		intr_ipi_dev_priority = priority;
		intr_ipi_dev = dev;
	}

	return (0);
}

/*
 *  Setup IPI handler on interrupt controller.
 *
 *  Not SMP coherent.
 */
void
intr_ipi_setup(u_int ipi, const char *name, intr_ipi_handler_t *hand,
    void *arg)
{
	struct intr_irqsrc *isrc;
	struct intr_ipi *ii;
	int error;

	if (!intr_ipi_dev_frozen) {
		if (intr_ipi_dev == NULL)
			panic("%s: no IPI PIC attached", __func__);

		intr_ipi_dev_frozen = true;
		device_printf(intr_ipi_dev, "using for IPIs\n");
	}

	KASSERT(hand != NULL, ("%s: ipi %u no handler", __func__, ipi));

	error = PIC_IPI_SETUP(intr_ipi_dev, ipi, &isrc);
	if (error != 0)
		return;

	isrc->isrc_handlers++;

	ii = intr_ipi_lookup(ipi);
	KASSERT(ii->ii_count == NULL, ("%s: ipi %u reused", __func__, ipi));

	ii->ii_handler = hand;
	ii->ii_handler_arg = arg;
	ii->ii_isrc = isrc;
	strlcpy(ii->ii_name, name, INTR_IPI_NAMELEN);
	ii->ii_count = intr_ipi_setup_counters(name);

	PIC_ENABLE_INTR(intr_ipi_dev, isrc);
}

void
intr_ipi_send(cpuset_t cpus, u_int ipi)
{
	struct intr_ipi *ii;

	KASSERT(intr_ipi_dev_frozen,
	    ("%s: IPI device not yet frozen", __func__));

	ii = intr_ipi_lookup(ipi);
	if (ii->ii_count == NULL)
		panic("%s: not setup IPI %u", __func__, ipi);

	/*
	 * XXX: Surely needed on other architectures too? Either way should be
	 * some kind of MI hook defined in an MD header, or the responsibility
	 * of the MD caller if not widespread.
	 */
#ifdef __aarch64__
	/*
	 * Ensure that this CPU's stores will be visible to IPI
	 * recipients before starting to send the interrupts.
	 */
	dsb(ishst);
#endif

	PIC_IPI_SEND(intr_ipi_dev, ii->ii_isrc, cpus, ipi);
}

/*
 *  interrupt controller dispatch function for IPIs. It should
 *  be called straight from the interrupt controller, when associated
 *  interrupt source is learned. Or from anybody who has an interrupt
 *  source mapped.
 */
void
intr_ipi_dispatch(u_int ipi)
{
	struct intr_ipi *ii;

	ii = intr_ipi_lookup(ipi);
	if (ii->ii_count == NULL)
		panic("%s: not setup IPI %u", __func__, ipi);

	intr_ipi_increment_count(ii->ii_count, PCPU_GET(cpuid));

	ii->ii_handler(ii->ii_handler_arg);
}
#endif
