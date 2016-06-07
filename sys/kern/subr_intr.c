/*-
 * Copyright (c) 2015-2016 Svatopluk Kraus
 * Copyright (c) 2015-2016 Michal Meloun
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 *	New-style Interrupt Framework
 *
 *  TODO: - to support IPI (PPI) enabling on other CPUs if already started
 *        - to complete things for removable PICs
 */

#include "opt_acpi.h"
#include "opt_ddb.h"
#include "opt_hwpmc_hooks.h"
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
#include <sys/rman.h>
#include <sys/sched.h>
#include <sys/smp.h>
#ifdef HWPMC_HOOKS
#include <sys/pmckern.h>
#endif

#include <machine/atomic.h>
#include <machine/intr.h>
#include <machine/cpu.h>
#include <machine/smp.h>
#include <machine/stdarg.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include "pic_if.h"
#include "msi_if.h"

#define	INTRNAME_LEN	(2*MAXCOMLEN + 1)

#ifdef DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__);	\
    printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

MALLOC_DECLARE(M_INTRNG);
MALLOC_DEFINE(M_INTRNG, "intr", "intr interrupt handling");

/* Main interrupt handler called from assembler -> 'hidden' for C code. */
void intr_irq_handler(struct trapframe *tf);

/* Root interrupt controller stuff. */
device_t intr_irq_root_dev;
static intr_irq_filter_t *irq_root_filter;
static void *irq_root_arg;
static u_int irq_root_ipicount;

struct intr_pic_child {
	SLIST_ENTRY(intr_pic_child)	 pc_next;
	struct intr_pic			*pc_pic;
	intr_child_irq_filter_t		*pc_filter;
	void				*pc_filter_arg;
	uintptr_t			 pc_start;
	uintptr_t			 pc_length;
};

/* Interrupt controller definition. */
struct intr_pic {
	SLIST_ENTRY(intr_pic)	pic_next;
	intptr_t		pic_xref;	/* hardware identification */
	device_t		pic_dev;
#define	FLAG_PIC	(1 << 0)
#define	FLAG_MSI	(1 << 1)
	u_int			pic_flags;
	struct mtx		pic_child_lock;
	SLIST_HEAD(, intr_pic_child) pic_children;
};

static struct mtx pic_list_lock;
static SLIST_HEAD(, intr_pic) pic_list;

static struct intr_pic *pic_lookup(device_t dev, intptr_t xref);

/* Interrupt source definition. */
static struct mtx isrc_table_lock;
static struct intr_irqsrc *irq_sources[NIRQ];
u_int irq_next_free;

/*
 *  XXX - All stuff around struct intr_dev_data is considered as temporary
 *  until better place for storing struct intr_map_data will be find.
 *
 *  For now, there are two global interrupt numbers spaces:
 *  <0, NIRQ)                      ... interrupts without config data
 *                                     managed in irq_sources[]
 *  IRQ_DDATA_BASE + <0, 2 * NIRQ) ... interrupts with config data
 *                                     managed in intr_ddata_tab[]
 *
 *  Read intr_ddata_lookup() to see how these spaces are worked with.
 *  Note that each interrupt number from second space duplicates some number
 *  from first space at this moment. An interrupt number from first space can
 *  be duplicated even multiple times in second space.
 */
struct intr_dev_data {
	device_t		idd_dev;
	intptr_t		idd_xref;
	u_int			idd_irq;
	struct intr_map_data *	idd_data;
	struct intr_irqsrc *	idd_isrc;
};

static struct intr_dev_data *intr_ddata_tab[2 * NIRQ];
#if 0
static u_int intr_ddata_first_unused;
#endif

#define IRQ_DDATA_BASE	10000
CTASSERT(IRQ_DDATA_BASE > nitems(irq_sources));

#ifdef SMP
static boolean_t irq_assign_cpu = FALSE;
#endif

/*
 * - 2 counters for each I/O interrupt.
 * - MAXCPU counters for each IPI counters for SMP.
 */
#ifdef SMP
#define INTRCNT_COUNT   (NIRQ * 2 + INTR_IPI_COUNT * MAXCPU)
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
intr_irq_init(void *dummy __unused)
{

	SLIST_INIT(&pic_list);
	mtx_init(&pic_list_lock, "intr pic list", NULL, MTX_DEF);

	mtx_init(&isrc_table_lock, "intr isrc table", NULL, MTX_DEF);
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

/*
 *  Virtualization for interrupt source interrupt counters release.
 */
static void
isrc_release_counters(struct intr_irqsrc *isrc)
{

	panic("%s: not implemented", __func__);
}

#ifdef SMP
/*
 *  Virtualization for interrupt source IPI counters setup.
 */
u_long *
intr_ipi_setup_counters(const char *name)
{
	u_int index, i;
	char str[INTRNAME_LEN];

	index = atomic_fetchadd_int(&intrcnt_index, MAXCPU);
	for (i = 0; i < MAXCPU; i++) {
		snprintf(str, INTRNAME_LEN, "cpu%d:%s", i, name);
		intrcnt_setname(str, index + i);
	}
	return (&intrcnt[index]);
}
#endif

/*
 *  Main interrupt dispatch handler. It's called straight
 *  from the assembler, where CPU interrupt is served.
 */
void
intr_irq_handler(struct trapframe *tf)
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
#ifdef HWPMC_HOOKS
	if (pmc_hook && TRAPF_USERMODE(tf) &&
	    (PCPU_GET(curthread)->td_pflags & TDP_CALLCHAIN))
		pmc_hook(PCPU_GET(curthread), PMC_FN_USER_CALLCHAIN, tf);
#endif
}

int
intr_child_irq_handler(struct intr_pic *parent, uintptr_t irq)
{
	struct intr_pic_child *child;
	bool found;

	found = false;
	mtx_lock_spin(&parent->pic_child_lock);
	SLIST_FOREACH(child, &parent->pic_children, pc_next) {
		if (child->pc_start <= irq &&
		    irq < (child->pc_start + child->pc_length)) {
			found = true;
			break;
		}
	}
	mtx_unlock_spin(&parent->pic_child_lock);

	if (found)
		return (child->pc_filter(child->pc_filter_arg, irq));

	return (FILTER_STRAY);
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

	irq_next_free = irq + 1;
	if (irq_next_free >= maxirqs)
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

	if (isrc->isrc_irq >= nitems(irq_sources))
		return (EINVAL);
	if (irq_sources[isrc->isrc_irq] != isrc)
		return (EINVAL);

	irq_sources[isrc->isrc_irq] = NULL;
	isrc->isrc_irq = INTR_IRQ_INVALID;	/* just to be safe */
	return (0);
}

/*
 *  Lookup interrupt source by interrupt number (resource handle).
 */
static inline struct intr_irqsrc *
isrc_lookup(u_int irq)
{

	if (irq < nitems(irq_sources))
		return (irq_sources[irq]);
	return (NULL);
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

#ifdef SMP
/*
 *  A support function for a PIC to decide if provided ISRC should be inited
 *  on given cpu. The logic of INTR_ISRCF_BOUND flag and isrc_cpu member of
 *  struct intr_irqsrc is the following:
 *
 *     If INTR_ISRCF_BOUND is set, the ISRC should be inited only on cpus
 *     set in isrc_cpu. If not, the ISRC should be inited on every cpu and
 *     isrc_cpu is kept consistent with it. Thus isrc_cpu is always correct.
 */
bool
intr_isrc_init_on_cpu(struct intr_irqsrc *isrc, u_int cpu)
{

	if (isrc->isrc_handlers == 0)
		return (false);
	if ((isrc->isrc_flags & (INTR_ISRCF_PPI | INTR_ISRCF_IPI)) == 0)
		return (false);
	if (isrc->isrc_flags & INTR_ISRCF_BOUND)
		return (CPU_ISSET(cpu, &isrc->isrc_cpu));

	CPU_SET(cpu, &isrc->isrc_cpu);
	return (true);
}
#endif

#if 0
static struct intr_dev_data *
intr_ddata_alloc(u_int extsize)
{
	struct intr_dev_data *ddata;
	size_t size;

	size = sizeof(*ddata);
	ddata = malloc(size + extsize, M_INTRNG, M_WAITOK | M_ZERO);

	mtx_lock(&isrc_table_lock);
	if (intr_ddata_first_unused >= nitems(intr_ddata_tab)) {
		mtx_unlock(&isrc_table_lock);
		free(ddata, M_INTRNG);
		return (NULL);
	}
	intr_ddata_tab[intr_ddata_first_unused] = ddata;
	ddata->idd_irq = IRQ_DDATA_BASE + intr_ddata_first_unused++;
	mtx_unlock(&isrc_table_lock);

	ddata->idd_data = (struct intr_map_data *)((uintptr_t)ddata + size);
	return (ddata);
}
#endif

static struct intr_irqsrc *
intr_ddata_lookup(u_int irq, struct intr_map_data **datap)
{
	int error;
	struct intr_irqsrc *isrc;
	struct intr_dev_data *ddata;

	isrc = isrc_lookup(irq);
	if (isrc != NULL) {
		if (datap != NULL)
			*datap = NULL;
		return (isrc);
	}

	if (irq < IRQ_DDATA_BASE)
		return (NULL);

	irq -= IRQ_DDATA_BASE;
	if (irq >= nitems(intr_ddata_tab))
		return (NULL);

	ddata = intr_ddata_tab[irq];
	if (ddata->idd_isrc == NULL) {
		error = intr_map_irq(ddata->idd_dev, ddata->idd_xref,
		    ddata->idd_data, &irq);
		if (error != 0)
			return (NULL);
		ddata->idd_isrc = isrc_lookup(irq);
	}
	if (datap != NULL)
		*datap = ddata->idd_data;
	return (ddata->idd_isrc);
}

#ifdef DEV_ACPI
/*
 *  Map interrupt source according to ACPI info into framework. If such mapping
 *  does not exist, create it. Return unique interrupt number (resource handle)
 *  associated with mapped interrupt source.
 */
u_int
intr_acpi_map_irq(device_t dev, u_int irq, enum intr_polarity pol,
    enum intr_trigger trig)
{
	struct intr_map_data_acpi *daa;
	struct intr_dev_data *ddata;

	ddata = intr_ddata_alloc(sizeof(struct intr_map_data_acpi));
	if (ddata == NULL)
		return (INTR_IRQ_INVALID);	/* no space left */

	ddata->idd_dev = dev;
	ddata->idd_data->type = INTR_MAP_DATA_ACPI;

	daa = (struct intr_map_data_acpi *)ddata->idd_data;
	daa->irq = irq;
	daa->pol = pol;
	daa->trig = trig;

	return (ddata->idd_irq);
}
#endif

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
#ifdef SMP
	struct intr_irqsrc *isrc = arg;
	int error;

	if (isrc->isrc_dev != intr_irq_root_dev)
		return (EINVAL);

	mtx_lock(&isrc_table_lock);
	if (cpu == NOCPU) {
		CPU_ZERO(&isrc->isrc_cpu);
		isrc->isrc_flags &= ~INTR_ISRCF_BOUND;
	} else {
		CPU_SETOF(cpu, &isrc->isrc_cpu);
		isrc->isrc_flags |= INTR_ISRCF_BOUND;
	}

	/*
	 * In NOCPU case, it's up to PIC to either leave ISRC on same CPU or
	 * re-balance it to another CPU or enable it on more CPUs. However,
	 * PIC is expected to change isrc_cpu appropriately to keep us well
	 * informed if the call is successful.
	 */
	if (irq_assign_cpu) {
		error = PIC_BIND_INTR(isrc->isrc_dev, isrc);
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
static int
isrc_add_handler(struct intr_irqsrc *isrc, const char *name,
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
static inline struct intr_pic *
pic_lookup_locked(device_t dev, intptr_t xref)
{
	struct intr_pic *pic;

	mtx_assert(&pic_list_lock, MA_OWNED);

	if (dev == NULL && xref == 0)
		return (NULL);

	/* Note that pic->pic_dev is never NULL on registered PIC. */
	SLIST_FOREACH(pic, &pic_list, pic_next) {
		if (dev == NULL) {
			if (xref == pic->pic_xref)
				return (pic);
		} else if (xref == 0 || pic->pic_xref == 0) {
			if (dev == pic->pic_dev)
				return (pic);
		} else if (xref == pic->pic_xref && dev == pic->pic_dev)
				return (pic);
	}
	return (NULL);
}

/*
 *  Lookup interrupt controller.
 */
static struct intr_pic *
pic_lookup(device_t dev, intptr_t xref)
{
	struct intr_pic *pic;

	mtx_lock(&pic_list_lock);
	pic = pic_lookup_locked(dev, xref);
	mtx_unlock(&pic_list_lock);
	return (pic);
}

/*
 *  Create interrupt controller.
 */
static struct intr_pic *
pic_create(device_t dev, intptr_t xref)
{
	struct intr_pic *pic;

	mtx_lock(&pic_list_lock);
	pic = pic_lookup_locked(dev, xref);
	if (pic != NULL) {
		mtx_unlock(&pic_list_lock);
		return (pic);
	}
	pic = malloc(sizeof(*pic), M_INTRNG, M_NOWAIT | M_ZERO);
	if (pic == NULL) {
		mtx_unlock(&pic_list_lock);
		return (NULL);
	}
	pic->pic_xref = xref;
	pic->pic_dev = dev;
	mtx_init(&pic->pic_child_lock, "pic child lock", NULL, MTX_SPIN);
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
	struct intr_pic *pic;

	mtx_lock(&pic_list_lock);
	pic = pic_lookup_locked(dev, xref);
	if (pic == NULL) {
		mtx_unlock(&pic_list_lock);
		return;
	}
	SLIST_REMOVE(&pic_list, pic, intr_pic, pic_next);
	mtx_unlock(&pic_list_lock);

	free(pic, M_INTRNG);
}
#endif
/*
 *  Register interrupt controller.
 */
struct intr_pic *
intr_pic_register(device_t dev, intptr_t xref)
{
	struct intr_pic *pic;

	if (dev == NULL)
		return (NULL);
	pic = pic_create(dev, xref);
	if (pic == NULL)
		return (NULL);

	pic->pic_flags |= FLAG_PIC;

	debugf("PIC %p registered for %s <dev %p, xref %x>\n", pic,
	    device_get_nameunit(dev), dev, xref);
	return (pic);
}

/*
 *  Unregister interrupt controller.
 */
int
intr_pic_deregister(device_t dev, intptr_t xref)
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
intr_pic_claim_root(device_t dev, intptr_t xref, intr_irq_filter_t *filter,
    void *arg, u_int ipicount)
{
	struct intr_pic *pic;

	pic = pic_lookup(dev, xref);
	if (pic == NULL) {
		device_printf(dev, "not registered\n");
		return (EINVAL);
	}

	KASSERT((pic->pic_flags & FLAG_PIC) != 0,
	    ("%s: Found a non-PIC controller: %s", __func__,
	     device_get_name(pic->pic_dev)));

	if (filter == NULL) {
		device_printf(dev, "filter missing\n");
		return (EINVAL);
	}

	/*
	 * Only one interrupt controllers could be on the root for now.
	 * Note that we further suppose that there is not threaded interrupt
	 * routine (handler) on the root. See intr_irq_handler().
	 */
	if (intr_irq_root_dev != NULL) {
		device_printf(dev, "another root already set\n");
		return (EBUSY);
	}

	intr_irq_root_dev = dev;
	irq_root_filter = filter;
	irq_root_arg = arg;
	irq_root_ipicount = ipicount;

	debugf("irq root set to %s\n", device_get_nameunit(dev));
	return (0);
}

/*
 * Add a handler to manage a sub range of a parents interrupts.
 */
struct intr_pic *
intr_pic_add_handler(device_t parent, struct intr_pic *pic,
    intr_child_irq_filter_t *filter, void *arg, uintptr_t start,
    uintptr_t length)
{
	struct intr_pic *parent_pic;
	struct intr_pic_child *newchild;
#ifdef INVARIANTS
	struct intr_pic_child *child;
#endif

	parent_pic = pic_lookup(parent, 0);
	if (parent_pic == NULL)
		return (NULL);

	newchild = malloc(sizeof(*newchild), M_INTRNG, M_WAITOK | M_ZERO);
	newchild->pc_pic = pic;
	newchild->pc_filter = filter;
	newchild->pc_filter_arg = arg;
	newchild->pc_start = start;
	newchild->pc_length = length;

	mtx_lock_spin(&parent_pic->pic_child_lock);
#ifdef INVARIANTS
	SLIST_FOREACH(child, &parent_pic->pic_children, pc_next) {
		KASSERT(child->pc_pic != pic, ("%s: Adding a child PIC twice",
		    __func__));
	}
#endif
	SLIST_INSERT_HEAD(&parent_pic->pic_children, newchild, pc_next);
	mtx_unlock_spin(&parent_pic->pic_child_lock);

	return (pic);
}

int
intr_map_irq(device_t dev, intptr_t xref, struct intr_map_data *data,
    u_int *irqp)
{
	int error;
	struct intr_irqsrc *isrc;
	struct intr_pic *pic;

	if (data == NULL)
		return (EINVAL);

	pic = pic_lookup(dev, xref);
	if (pic == NULL)
		return (ESRCH);

	KASSERT((pic->pic_flags & FLAG_PIC) != 0,
	    ("%s: Found a non-PIC controller: %s", __func__,
	     device_get_name(pic->pic_dev)));

	error = PIC_MAP_INTR(pic->pic_dev, data, &isrc);
	if (error == 0)
		*irqp = isrc->isrc_irq;
	return (error);
}

int
intr_alloc_irq(device_t dev, struct resource *res)
{
	struct intr_map_data *data;
	struct intr_irqsrc *isrc;

	KASSERT(rman_get_start(res) == rman_get_end(res),
	    ("%s: more interrupts in resource", __func__));

	data = rman_get_virtual(res);
	if (data == NULL)
		isrc = intr_ddata_lookup(rman_get_start(res), &data);
	else
		isrc = isrc_lookup(rman_get_start(res));
	if (isrc == NULL)
		return (EINVAL);

	return (PIC_ALLOC_INTR(isrc->isrc_dev, isrc, res, data));
}

int
intr_release_irq(device_t dev, struct resource *res)
{
	struct intr_map_data *data;
	struct intr_irqsrc *isrc;

	KASSERT(rman_get_start(res) == rman_get_end(res),
	    ("%s: more interrupts in resource", __func__));

	data = rman_get_virtual(res);
	if (data == NULL)
		isrc = intr_ddata_lookup(rman_get_start(res), &data);
	else
		isrc = isrc_lookup(rman_get_start(res));
	if (isrc == NULL)
		return (EINVAL);

	return (PIC_RELEASE_INTR(isrc->isrc_dev, isrc, res, data));
}

int
intr_setup_irq(device_t dev, struct resource *res, driver_filter_t filt,
    driver_intr_t hand, void *arg, int flags, void **cookiep)
{
	int error;
	struct intr_map_data *data;
	struct intr_irqsrc *isrc;
	const char *name;

	KASSERT(rman_get_start(res) == rman_get_end(res),
	    ("%s: more interrupts in resource", __func__));

	data = rman_get_virtual(res);
	if (data == NULL)
		isrc = intr_ddata_lookup(rman_get_start(res), &data);
	else
		isrc = isrc_lookup(rman_get_start(res));
	if (isrc == NULL)
		return (EINVAL);

	name = device_get_nameunit(dev);

#ifdef INTR_SOLO
	/*
	 * Standard handling is done through MI interrupt framework. However,
	 * some interrupts could request solely own special handling. This
	 * non standard handling can be used for interrupt controllers without
	 * handler (filter only), so in case that interrupt controllers are
	 * chained, MI interrupt framework is called only in leaf controller.
	 *
	 * Note that root interrupt controller routine is served as well,
	 * however in intr_irq_handler(), i.e. main system dispatch routine.
	 */
	if (flags & INTR_SOLO && hand != NULL) {
		debugf("irq %u cannot solo on %s\n", irq, name);
		return (EINVAL);
	}

	if (flags & INTR_SOLO) {
		error = iscr_setup_filter(isrc, name, (intr_irq_filter_t *)filt,
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
	error = PIC_SETUP_INTR(isrc->isrc_dev, isrc, res, data);
	if (error == 0) {
		isrc->isrc_handlers++;
		if (isrc->isrc_handlers == 1)
			PIC_ENABLE_INTR(isrc->isrc_dev, isrc);
	}
	mtx_unlock(&isrc_table_lock);
	if (error != 0)
		intr_event_remove_handler(*cookiep);
	return (error);
}

int
intr_teardown_irq(device_t dev, struct resource *res, void *cookie)
{
	int error;
	struct intr_map_data *data;
	struct intr_irqsrc *isrc;

	KASSERT(rman_get_start(res) == rman_get_end(res),
	    ("%s: more interrupts in resource", __func__));

	data = rman_get_virtual(res);
	if (data == NULL)
		isrc = intr_ddata_lookup(rman_get_start(res), &data);
	else
		isrc = isrc_lookup(rman_get_start(res));
	if (isrc == NULL || isrc->isrc_handlers == 0)
		return (EINVAL);

#ifdef INTR_SOLO
	if (isrc->isrc_filter != NULL) {
		if (isrc != cookie)
			return (EINVAL);

		mtx_lock(&isrc_table_lock);
		isrc->isrc_filter = NULL;
		isrc->isrc_arg = NULL;
		isrc->isrc_handlers = 0;
		PIC_DISABLE_INTR(isrc->isrc_dev, isrc);
		PIC_TEARDOWN_INTR(isrc->isrc_dev, isrc, res, data);
		isrc_update_name(isrc, NULL);
		mtx_unlock(&isrc_table_lock);
		return (0);
	}
#endif
	if (isrc != intr_handler_source(cookie))
		return (EINVAL);

	error = intr_event_remove_handler(cookie);
	if (error == 0) {
		mtx_lock(&isrc_table_lock);
		isrc->isrc_handlers--;
		if (isrc->isrc_handlers == 0)
			PIC_DISABLE_INTR(isrc->isrc_dev, isrc);
		PIC_TEARDOWN_INTR(isrc->isrc_dev, isrc, res, data);
		intrcnt_updatename(isrc);
		mtx_unlock(&isrc_table_lock);
	}
	return (error);
}

int
intr_describe_irq(device_t dev, struct resource *res, void *cookie,
    const char *descr)
{
	int error;
	struct intr_irqsrc *isrc;

	KASSERT(rman_get_start(res) == rman_get_end(res),
	    ("%s: more interrupts in resource", __func__));

	isrc = intr_ddata_lookup(rman_get_start(res), NULL);
	if (isrc == NULL || isrc->isrc_handlers == 0)
		return (EINVAL);
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
intr_bind_irq(device_t dev, struct resource *res, int cpu)
{
	struct intr_irqsrc *isrc;

	KASSERT(rman_get_start(res) == rman_get_end(res),
	    ("%s: more interrupts in resource", __func__));

	isrc = intr_ddata_lookup(rman_get_start(res), NULL);
	if (isrc == NULL || isrc->isrc_handlers == 0)
		return (EINVAL);
#ifdef INTR_SOLO
	if (isrc->isrc_filter != NULL)
		return (intr_isrc_assign_cpu(isrc, cpu));
#endif
	return (intr_event_bind(isrc->isrc_event, cpu));
}

/*
 * Return the CPU that the next interrupt source should use.
 * For now just returns the next CPU according to round-robin.
 */
u_int
intr_irq_next_cpu(u_int last_cpu, cpuset_t *cpumask)
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
intr_irq_shuffle(void *arg __unused)
{
	struct intr_irqsrc *isrc;
	u_int i;

	if (mp_ncpus == 1)
		return;

	mtx_lock(&isrc_table_lock);
	irq_assign_cpu = TRUE;
	for (i = 0; i < NIRQ; i++) {
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

#else
u_int
intr_irq_next_cpu(u_int current_cpu, cpuset_t *cpumask)
{

	return (PCPU_GET(cpuid));
}
#endif

/*
 *  Register a MSI/MSI-X interrupt controller
 */
int
intr_msi_register(device_t dev, intptr_t xref)
{
	struct intr_pic *pic;

	if (dev == NULL)
		return (EINVAL);
	pic = pic_create(dev, xref);
	if (pic == NULL)
		return (ENOMEM);

	pic->pic_flags |= FLAG_MSI;

	debugf("PIC %p registered for %s <dev %p, xref %jx>\n", pic,
	    device_get_nameunit(dev), dev, (uintmax_t)xref);
	return (0);
}

int
intr_alloc_msi(device_t pci, device_t child, intptr_t xref, int count,
    int maxcount, int *irqs)
{
	struct intr_irqsrc **isrc;
	struct intr_pic *pic;
	device_t pdev;
	int err, i;

	pic = pic_lookup(NULL, xref);
	if (pic == NULL)
		return (ESRCH);

	KASSERT((pic->pic_flags & FLAG_MSI) != 0,
	    ("%s: Found a non-MSI controller: %s", __func__,
	     device_get_name(pic->pic_dev)));

	isrc = malloc(sizeof(*isrc) * count, M_INTRNG, M_WAITOK);
	err = MSI_ALLOC_MSI(pic->pic_dev, child, count, maxcount, &pdev, isrc);
	if (err == 0) {
		for (i = 0; i < count; i++) {
			irqs[i] = isrc[i]->isrc_irq;
		}
	}

	free(isrc, M_INTRNG);

	return (err);
}

int
intr_release_msi(device_t pci, device_t child, intptr_t xref, int count,
    int *irqs)
{
	struct intr_irqsrc **isrc;
	struct intr_pic *pic;
	int i, err;

	pic = pic_lookup(NULL, xref);
	if (pic == NULL)
		return (ESRCH);

	KASSERT((pic->pic_flags & FLAG_MSI) != 0,
	    ("%s: Found a non-MSI controller: %s", __func__,
	     device_get_name(pic->pic_dev)));

	isrc = malloc(sizeof(*isrc) * count, M_INTRNG, M_WAITOK);

	for (i = 0; i < count; i++) {
		isrc[i] = isrc_lookup(irqs[i]);
		if (isrc == NULL) {
			free(isrc, M_INTRNG);
			return (EINVAL);
		}
	}

	err = MSI_RELEASE_MSI(pic->pic_dev, child, count, isrc);
	free(isrc, M_INTRNG);
	return (err);
}

int
intr_alloc_msix(device_t pci, device_t child, intptr_t xref, int *irq)
{
	struct intr_irqsrc *isrc;
	struct intr_pic *pic;
	device_t pdev;
	int err;

	pic = pic_lookup(NULL, xref);
	if (pic == NULL)
		return (ESRCH);

	KASSERT((pic->pic_flags & FLAG_MSI) != 0,
	    ("%s: Found a non-MSI controller: %s", __func__,
	     device_get_name(pic->pic_dev)));

	err = MSI_ALLOC_MSIX(pic->pic_dev, child, &pdev, &isrc);
	if (err != 0)
		return (err);

	*irq = isrc->isrc_irq;
	return (0);
}

int
intr_release_msix(device_t pci, device_t child, intptr_t xref, int irq)
{
	struct intr_irqsrc *isrc;
	struct intr_pic *pic;
	int err;

	pic = pic_lookup(NULL, xref);
	if (pic == NULL)
		return (ESRCH);

	KASSERT((pic->pic_flags & FLAG_MSI) != 0,
	    ("%s: Found a non-MSI controller: %s", __func__,
	     device_get_name(pic->pic_dev)));

	isrc = isrc_lookup(irq);
	if (isrc == NULL)
		return (EINVAL);

	err = MSI_RELEASE_MSIX(pic->pic_dev, child, isrc);
	return (err);
}

int
intr_map_msi(device_t pci, device_t child, intptr_t xref, int irq,
    uint64_t *addr, uint32_t *data)
{
	struct intr_irqsrc *isrc;
	struct intr_pic *pic;
	int err;

	pic = pic_lookup(NULL, xref);
	if (pic == NULL)
		return (ESRCH);

	KASSERT((pic->pic_flags & FLAG_MSI) != 0,
	    ("%s: Found a non-MSI controller: %s", __func__,
	     device_get_name(pic->pic_dev)));

	isrc = isrc_lookup(irq);
	if (isrc == NULL)
		return (EINVAL);

	err = MSI_MAP_MSI(pic->pic_dev, child, isrc, addr, data);
	return (err);
}


void dosoftints(void);
void
dosoftints(void)
{
}

#ifdef SMP
/*
 *  Init interrupt controller on another CPU.
 */
void
intr_pic_init_secondary(void)
{

	/*
	 * QQQ: Only root PIC is aware of other CPUs ???
	 */
	KASSERT(intr_irq_root_dev != NULL, ("%s: no root attached", __func__));

	//mtx_lock(&isrc_table_lock);
	PIC_INIT_SECONDARY(intr_irq_root_dev);
	//mtx_unlock(&isrc_table_lock);
}
#endif

#ifdef DDB
DB_SHOW_COMMAND(irqs, db_show_irqs)
{
	u_int i, irqsum;
	u_long num;
	struct intr_irqsrc *isrc;

	for (irqsum = 0, i = 0; i < NIRQ; i++) {
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
