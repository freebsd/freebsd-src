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
#include <sys/lock.h>
#include <sys/ktr.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <machine/clock.h>
#include <machine/intr_machdep.h>
#ifdef DDB
#include <ddb/ddb.h>
#endif

#define	MAX_STRAY_LOG	5

typedef void (*mask_fn)(uintptr_t vector);

static int intrcnt_index;
static struct intsrc *interrupt_sources[NUM_IO_INTS];
static struct mtx intr_table_lock;

static void	intr_init(void *__dummy);
static void	intrcnt_setname(const char *name, int index);
static void	intrcnt_updatename(struct intsrc *is);
static void	intrcnt_register(struct intsrc *is);

/*
 * Register a new interrupt source with the global interrupt system.
 * The global interrupts need to be disabled when this function is
 * called.
 */
int
intr_register_source(struct intsrc *isrc)
{
	int error, vector;

	vector = isrc->is_pic->pic_vector(isrc);
	if (interrupt_sources[vector] != NULL)
		return (EEXIST);
	error = ithread_create(&isrc->is_ithread, (uintptr_t)isrc, 0,
	    (mask_fn)isrc->is_pic->pic_disable_source,
	    (mask_fn)isrc->is_pic->pic_enable_source, "irq%d:", vector);
	if (error)
		return (error);
	mtx_lock_spin(&intr_table_lock);
	if (interrupt_sources[vector] != NULL) {
		mtx_unlock_spin(&intr_table_lock);
		ithread_destroy(isrc->is_ithread);
		return (EEXIST);
	}
	intrcnt_register(isrc);
	interrupt_sources[vector] = isrc;
	mtx_unlock_spin(&intr_table_lock);
	return (0);
}

struct intsrc *
intr_lookup_source(int vector)
{

	return (interrupt_sources[vector]);
}

int
intr_add_handler(const char *name, int vector, driver_intr_t handler,
    void *arg, enum intr_type flags, void **cookiep)
{
	struct intsrc *isrc;
	int error;

	isrc = intr_lookup_source(vector);
	if (isrc == NULL)
		return (EINVAL);
	error = ithread_add_handler(isrc->is_ithread, name, handler, arg,
	    ithread_priority(flags), flags, cookiep);
	if (error == 0) {
		intrcnt_updatename(isrc);
		isrc->is_pic->pic_enable_intr(isrc);
		isrc->is_pic->pic_enable_source(isrc);
	}
	return (error);
}

int
intr_remove_handler(void *cookie)
{
	int error;

	error = ithread_remove_handler(cookie);
#ifdef XXX
	if (error == 0)
		intrcnt_updatename(/* XXX */);
#endif
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

void
intr_execute_handlers(struct intsrc *isrc, struct intrframe *iframe)
{
	struct thread *td;
	struct ithd *it;
	struct intrhand *ih;
	int error, vector;

	td = curthread;
	td->td_intr_nesting_level++;

	/*
	 * We count software interrupts when we process them.  The
	 * code here follows previous practice, but there's an
	 * argument for counting hardware interrupts when they're
	 * processed too.
	 */
	(*isrc->is_count)++;
	cnt.v_intr++;

	it = isrc->is_ithread;
	if (it == NULL)
		ih = NULL;
	else
		ih = TAILQ_FIRST(&it->it_handlers);

	/*
	 * XXX: We assume that IRQ 0 is only used for the ISA timer
	 * device (clk).
	 */
	vector = isrc->is_pic->pic_vector(isrc);
	if (vector == 0)
		clkintr_pending = 1;

	if (ih != NULL && ih->ih_flags & IH_FAST) {
		/*
		 * Execute fast interrupt handlers directly.
		 * To support clock handlers, if a handler registers
		 * with a NULL argument, then we pass it a pointer to
		 * a trapframe as its argument.
		 */
		critical_enter();
		TAILQ_FOREACH(ih, &it->it_handlers, ih_next) {
			MPASS(ih->ih_flags & IH_FAST);
			CTR3(KTR_INTR, "%s: executing handler %p(%p)",
			    __func__, ih->ih_handler,
			    ih->ih_argument == NULL ? iframe :
			    ih->ih_argument);
			if (ih->ih_argument == NULL)
				ih->ih_handler(iframe);
			else
				ih->ih_handler(ih->ih_argument);
		}
		isrc->is_pic->pic_eoi_source(isrc);
		error = 0;
		critical_exit();
	} else {
		/*
		 * For stray and threaded interrupts, we mask and EOI the
		 * source.
		 */
		isrc->is_pic->pic_disable_source(isrc, PIC_EOI);
		if (ih == NULL)
			error = EINVAL;
		else
			error = ithread_schedule(it);
	}
	if (error == EINVAL) {
		(*isrc->is_straycount)++;
		if (*isrc->is_straycount < MAX_STRAY_LOG)
			log(LOG_ERR, "stray irq%d\n", vector);
		else if (*isrc->is_straycount == MAX_STRAY_LOG)
			log(LOG_CRIT,
			    "too many stray irq %d's: not logging anymore\n",
			    vector);
	}
	td->td_intr_nesting_level--;
}

void
intr_resume(void)
{
	struct intsrc **isrc;
	int i;

	mtx_lock_spin(&intr_table_lock);
	for (i = 0, isrc = interrupt_sources; i < NUM_IO_INTS; i++, isrc++)
		if (*isrc != NULL && (*isrc)->is_pic->pic_resume != NULL)
			(*isrc)->is_pic->pic_resume(*isrc);
	mtx_unlock_spin(&intr_table_lock);
}

void
intr_suspend(void)
{
	struct intsrc **isrc;
	int i;

	mtx_lock_spin(&intr_table_lock);
	for (i = 0, isrc = interrupt_sources; i < NUM_IO_INTS; i++, isrc++)
		if (*isrc != NULL && (*isrc)->is_pic->pic_suspend != NULL)
			(*isrc)->is_pic->pic_suspend(*isrc);
	mtx_unlock_spin(&intr_table_lock);
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

	intrcnt_setname(is->is_ithread->it_td->td_proc->p_comm, is->is_index);
}

static void
intrcnt_register(struct intsrc *is)
{
	char straystr[MAXCOMLEN + 1];

	/* mtx_assert(&intr_table_lock, MA_OWNED); */
	KASSERT(is->is_ithread != NULL, ("%s: isrc with no ithread", __func__));
	is->is_index = intrcnt_index;
	intrcnt_index += 2;
	snprintf(straystr, MAXCOMLEN + 1, "stray irq%d",
	    is->is_pic->pic_vector(is));
	intrcnt_updatename(is);
	is->is_count = &intrcnt[is->is_index];
	intrcnt_setname(straystr, is->is_index + 1);
	is->is_straycount = &intrcnt[is->is_index + 1];
}

void
intrcnt_add(const char *name, u_long **countp)
{

	mtx_lock_spin(&intr_table_lock);
	*countp = &intrcnt[intrcnt_index];
	intrcnt_setname(name, intrcnt_index);
	intrcnt_index++;
	mtx_unlock_spin(&intr_table_lock);
}

static void
intr_init(void *dummy __unused)
{

	intrcnt_setname("???", 0);
	intrcnt_index = 1;
	mtx_init(&intr_table_lock, "intr table", NULL, MTX_SPIN);
}
SYSINIT(intr_init, SI_SUB_INTR, SI_ORDER_FIRST, intr_init, NULL)

#ifdef DDB
/*
 * Dump data about interrupt handlers
 */
DB_SHOW_COMMAND(irqs, db_show_irqs)
{
	struct intsrc **isrc;
	int i, quit, verbose;

	quit = 0;
	if (strcmp(modif, "v") == 0)
		verbose = 1;
	else
		verbose = 0;
	isrc = interrupt_sources;
	db_setup_paging(db_simple_pager, &quit, db_lines_per_page);
	for (i = 0; i < NUM_IO_INTS && !quit; i++, isrc++)
		if (*isrc != NULL)
			db_dump_ithread((*isrc)->is_ithread, verbose);
}
#endif
