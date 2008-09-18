/*-
 * Copyright (c) 2003 John Baldwin <jhb@FreeBSD.org>
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

/*
 * Machine dependent interrupt code for i386.  For the i386, we have to
 * deal with different PICs.  Thus, we use the passed in vector to lookup
 * an interrupt source associated with that vector.  The interrupt source
 * describes which PIC the source belongs to and includes methods to handle
 * that source.
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/ktr.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/sx.h>
#include <machine/clock.h>
#include <machine/intr_machdep.h>
#include <machine/smp.h>
#ifdef DDB
#include <ddb/ddb.h>
#endif

#define	MAX_STRAY_LOG	5

typedef void (*mask_fn)(void *);

static int intrcnt_index;
static struct intsrc *interrupt_sources[NUM_IO_INTS];
static struct sx intr_table_lock;
static struct mtx intrcnt_lock;
static STAILQ_HEAD(, pic) pics;

#ifdef SMP
static int assign_cpu;

static void	intr_assign_next_cpu(struct intsrc *isrc);
#endif

static int	intr_assign_cpu(void *arg, u_char cpu);
static void	intr_disable_src(void *arg);
static void	intr_init(void *__dummy);
static int	intr_pic_registered(struct pic *pic);
static void	intrcnt_setname(const char *name, int index);
static void	intrcnt_updatename(struct intsrc *is);
static void	intrcnt_register(struct intsrc *is);

static int
intr_pic_registered(struct pic *pic)
{
	struct pic *p;

	STAILQ_FOREACH(p, &pics, pics) {
		if (p == pic)
			return (1);
	}
	return (0);
}

/*
 * Register a new interrupt controller (PIC).  This is to support suspend
 * and resume where we suspend/resume controllers rather than individual
 * sources.  This also allows controllers with no active sources (such as
 * 8259As in a system using the APICs) to participate in suspend and resume.
 */
int
intr_register_pic(struct pic *pic)
{
	int error;

	sx_xlock(&intr_table_lock);
	if (intr_pic_registered(pic))
		error = EBUSY;
	else {
		STAILQ_INSERT_TAIL(&pics, pic, pics);
		error = 0;
	}
	sx_xunlock(&intr_table_lock);
	return (error);
}

/*
 * Register a new interrupt source with the global interrupt system.
 * The global interrupts need to be disabled when this function is
 * called.
 */
int
intr_register_source(struct intsrc *isrc)
{
	int error, vector;

	KASSERT(intr_pic_registered(isrc->is_pic), ("unregistered PIC"));
	vector = isrc->is_pic->pic_vector(isrc);
	if (interrupt_sources[vector] != NULL)
		return (EEXIST);
	error = intr_event_create(&isrc->is_event, isrc, 0, vector,
	    intr_disable_src, (mask_fn)isrc->is_pic->pic_enable_source,
	    (mask_fn)isrc->is_pic->pic_eoi_source, intr_assign_cpu, "irq%d:",
	    vector);
	if (error)
		return (error);
	sx_xlock(&intr_table_lock);
	if (interrupt_sources[vector] != NULL) {
		sx_xunlock(&intr_table_lock);
		intr_event_destroy(isrc->is_event);
		return (EEXIST);
	}
	intrcnt_register(isrc);
	interrupt_sources[vector] = isrc;
	isrc->is_handlers = 0;
	sx_xunlock(&intr_table_lock);
	return (0);
}

struct intsrc *
intr_lookup_source(int vector)
{

	return (interrupt_sources[vector]);
}

int
intr_add_handler(const char *name, int vector, driver_filter_t filter,
    driver_intr_t handler, void *arg, enum intr_type flags, void **cookiep)
{
	struct intsrc *isrc;
	int error;

	isrc = intr_lookup_source(vector);
	if (isrc == NULL)
		return (EINVAL);
	error = intr_event_add_handler(isrc->is_event, name, filter, handler,
	    arg, intr_priority(flags), flags, cookiep);
	if (error == 0) {
		sx_xlock(&intr_table_lock);
		intrcnt_updatename(isrc);
		isrc->is_handlers++;
		if (isrc->is_handlers == 1) {
#ifdef SMP
			if (assign_cpu)
				intr_assign_next_cpu(isrc);
#endif
			isrc->is_pic->pic_enable_intr(isrc);
			isrc->is_pic->pic_enable_source(isrc);
		}
		sx_xunlock(&intr_table_lock);
	}
	return (error);
}

int
intr_remove_handler(void *cookie)
{
	struct intsrc *isrc;
	int error;

	isrc = intr_handler_source(cookie);
	error = intr_event_remove_handler(cookie);
	if (error == 0) {
		sx_xlock(&intr_table_lock);
		isrc->is_handlers--;
		if (isrc->is_handlers == 0) {
			isrc->is_pic->pic_disable_source(isrc, PIC_NO_EOI);
			isrc->is_pic->pic_disable_intr(isrc);
		}
		intrcnt_updatename(isrc);
		sx_xunlock(&intr_table_lock);
	}
	return (error);
}

int
intr_config_intr(int vector, enum intr_trigger trig, enum intr_polarity pol)
{
	struct intsrc *isrc;

	isrc = intr_lookup_source(vector);
	if (isrc == NULL)
		return (EINVAL);
	return (isrc->is_pic->pic_config_intr(isrc, trig, pol));
}

static void
intr_disable_src(void *arg)
{
	struct intsrc *isrc;

	isrc = arg;
	isrc->is_pic->pic_disable_source(isrc, PIC_EOI);
}

void
intr_execute_handlers(struct intsrc *isrc, struct trapframe *frame)
{
	struct intr_event *ie;
	struct thread *td;
	int vector;

	td = curthread;

	/*
	 * We count software interrupts when we process them.  The
	 * code here follows previous practice, but there's an
	 * argument for counting hardware interrupts when they're
	 * processed too.
	 */
	(*isrc->is_count)++;
	PCPU_INC(cnt.v_intr);

	ie = isrc->is_event;

	/*
	 * XXX: We assume that IRQ 0 is only used for the ISA timer
	 * device (clk).
	 */
	vector = isrc->is_pic->pic_vector(isrc);
	if (vector == 0)
		clkintr_pending = 1;

	/*
	 * For stray interrupts, mask and EOI the source, bump the
	 * stray count, and log the condition.
	 */
	if (intr_event_handle(ie, frame) != 0) {
		isrc->is_pic->pic_disable_source(isrc, PIC_EOI);
		(*isrc->is_straycount)++;
		if (*isrc->is_straycount < MAX_STRAY_LOG)
			log(LOG_ERR, "stray irq%d\n", vector);
		else if (*isrc->is_straycount == MAX_STRAY_LOG)
			log(LOG_CRIT,
			    "too many stray irq %d's: not logging anymore\n",
			    vector);
	}
}

void
intr_resume(void)
{
	struct pic *pic;

	sx_xlock(&intr_table_lock);
	STAILQ_FOREACH(pic, &pics, pics) {
		if (pic->pic_resume != NULL)
			pic->pic_resume(pic);
	}
	sx_xunlock(&intr_table_lock);
}

void
intr_suspend(void)
{
	struct pic *pic;

	sx_xlock(&intr_table_lock);
	STAILQ_FOREACH(pic, &pics, pics) {
		if (pic->pic_suspend != NULL)
			pic->pic_suspend(pic);
	}
	sx_xunlock(&intr_table_lock);
}

static int
intr_assign_cpu(void *arg, u_char cpu)
{
#ifdef SMP
	struct intsrc *isrc;	

	/*
	 * Don't do anything during early boot.  We will pick up the
	 * assignment once the APs are started.
	 */
	if (assign_cpu && cpu != NOCPU) {
		isrc = arg;
		sx_xlock(&intr_table_lock);
		isrc->is_pic->pic_assign_cpu(isrc, cpu_apic_ids[cpu]);
		sx_xunlock(&intr_table_lock);
	}
	return (0);
#else
	return (EOPNOTSUPP);
#endif
}

static void
intrcnt_setname(const char *name, int index)
{

	snprintf(intrnames + (MAXCOMLEN + 1) * index, MAXCOMLEN + 1, "%-*s",
	    MAXCOMLEN, name);
}

static void
intrcnt_updatename(struct intsrc *is)
{

	intrcnt_setname(is->is_event->ie_fullname, is->is_index);
}

static void
intrcnt_register(struct intsrc *is)
{
	char straystr[MAXCOMLEN + 1];

	KASSERT(is->is_event != NULL, ("%s: isrc with no event", __func__));
	mtx_lock_spin(&intrcnt_lock);
	is->is_index = intrcnt_index;
	intrcnt_index += 2;
	snprintf(straystr, MAXCOMLEN + 1, "stray irq%d",
	    is->is_pic->pic_vector(is));
	intrcnt_updatename(is);
	is->is_count = &intrcnt[is->is_index];
	intrcnt_setname(straystr, is->is_index + 1);
	is->is_straycount = &intrcnt[is->is_index + 1];
	mtx_unlock_spin(&intrcnt_lock);
}

void
intrcnt_add(const char *name, u_long **countp)
{

	mtx_lock_spin(&intrcnt_lock);
	*countp = &intrcnt[intrcnt_index];
	intrcnt_setname(name, intrcnt_index);
	intrcnt_index++;
	mtx_unlock_spin(&intrcnt_lock);
}

static void
intr_init(void *dummy __unused)
{

	intrcnt_setname("???", 0);
	intrcnt_index = 1;
	STAILQ_INIT(&pics);
	sx_init(&intr_table_lock, "intr sources");
	mtx_init(&intrcnt_lock, "intrcnt", NULL, MTX_SPIN);
}
SYSINIT(intr_init, SI_SUB_INTR, SI_ORDER_FIRST, intr_init, NULL);

#ifdef DDB
/*
 * Dump data about interrupt handlers
 */
DB_SHOW_COMMAND(irqs, db_show_irqs)
{
	struct intsrc **isrc;
	int i, verbose;

	if (strcmp(modif, "v") == 0)
		verbose = 1;
	else
		verbose = 0;
	isrc = interrupt_sources;
	for (i = 0; i < NUM_IO_INTS && !db_pager_quit; i++, isrc++)
		if (*isrc != NULL)
			db_dump_intr_event((*isrc)->is_event, verbose);
}
#endif

#ifdef SMP
/*
 * Support for balancing interrupt sources across CPUs.  For now we just
 * allocate CPUs round-robin.
 */

/* The BSP is always a valid target. */
static cpumask_t intr_cpus = (1 << 0);
static int current_cpu;

static void
intr_assign_next_cpu(struct intsrc *isrc)
{

	/*
	 * Assign this source to a local APIC in a round-robin fashion.
	 */
	isrc->is_pic->pic_assign_cpu(isrc, cpu_apic_ids[current_cpu]);
	do {
		current_cpu++;
		if (current_cpu > mp_maxid)
			current_cpu = 0;
	} while (!(intr_cpus & (1 << current_cpu)));
}

/* Attempt to bind the specified IRQ to the specified CPU. */
int
intr_bind(u_int vector, u_char cpu)
{
	struct intsrc *isrc;

	isrc = intr_lookup_source(vector);
	if (isrc == NULL)
		return (EINVAL);
	return (intr_event_bind(isrc->is_event, cpu));
}

/*
 * Add a CPU to our mask of valid CPUs that can be destinations of
 * interrupts.
 */
void
intr_add_cpu(u_int cpu)
{

	if (cpu >= MAXCPU)
		panic("%s: Invalid CPU ID", __func__);
	if (bootverbose)
		printf("INTR: Adding local APIC %d as a target\n",
		    cpu_apic_ids[cpu]);

	intr_cpus |= (1 << cpu);
}

/*
 * Distribute all the interrupt sources among the available CPUs once the
 * AP's have been launched.
 */
static void
intr_shuffle_irqs(void *arg __unused)
{
	struct intsrc *isrc;
	int i;

#ifdef XEN
	/*
	 * Doesn't work yet
	 */
	return;
#endif	
	
	/* Don't bother on UP. */
	if (mp_ncpus == 1)
		return;

	/* Round-robin assign a CPU to each enabled source. */
	sx_xlock(&intr_table_lock);
	assign_cpu = 1;
	for (i = 0; i < NUM_IO_INTS; i++) {
		isrc = interrupt_sources[i];
		if (isrc != NULL && isrc->is_handlers > 0) {
			/*
			 * If this event is already bound to a CPU,
			 * then assign the source to that CPU instead
			 * of picking one via round-robin.
			 */
			if (isrc->is_event->ie_cpu != NOCPU)
				isrc->is_pic->pic_assign_cpu(isrc,
				    cpu_apic_ids[isrc->is_event->ie_cpu]);
			else
				intr_assign_next_cpu(isrc);
		}
	}
	sx_xunlock(&intr_table_lock);
}
SYSINIT(intr_shuffle_irqs, SI_SUB_SMP, SI_ORDER_SECOND, intr_shuffle_irqs,
    NULL);
#endif
