/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*-
 * Copyright (c) 2002 Benno Rice.
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
 *	from: @(#)isa.c	7.2 (Berkeley) 5/13/91
 *	form: src/sys/i386/isa/intr_machdep.c,v 1.57 2001/07/20
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/vmmeter.h>
#include <sys/proc.h>

#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/trap.h>

#define	MAX_STRAY_LOG	5

MALLOC_DEFINE(M_INTR, "intr", "interrupt handler data");

struct ppc_intr {
	struct intr_event *event;
	long	*cntp;
	int	cntidx;
};

static struct mtx ppc_intrs_lock;
static struct ppc_intr **ppc_intrs;
static u_int ppc_nintrs;

static int intrcnt_index;

static void (*irq_enable)(uintptr_t);

static void
intrcnt_setname(const char *name, int index)
{
	snprintf(intrnames + (MAXCOMLEN + 1) * index, MAXCOMLEN + 1, "%-*s",
	    MAXCOMLEN, name);
}

void
intr_init(void (*handler)(void), int nirq, void (*irq_e)(uintptr_t),
    void (*irq_d)(uintptr_t))
{
	uint32_t msr;

	if (ppc_intrs != NULL)
		panic("intr_init: interrupts initialized twice\n");

	ppc_nintrs = nirq;
	ppc_intrs = malloc(nirq * sizeof(struct ppc_intr *), M_INTR,
	    M_NOWAIT|M_ZERO);
	if (ppc_intrs == NULL)
		panic("intr_init: unable to allocate interrupt handler array");

	mtx_init(&ppc_intrs_lock, "intr table", NULL, MTX_SPIN);

	irq_enable = irq_e;

	intrcnt_setname("???", 0);
	intrcnt_index = 1;

	msr = mfmsr();
	mtmsr(msr & ~PSL_EE);
	ext_intr_install(handler);
	mtmsr(msr);
}

int
inthand_add(const char *name, u_int irq, driver_filter_t *filter, 
    void (*handler)(void *), void *arg, int flags, void **cookiep)
{
	struct ppc_intr *i, *orphan;
	u_int idx;
	int error;

	/*
	 * Work around a race where more than one CPU may be registering
	 * handlers on the same IRQ at the same time.
	 */
	mtx_lock_spin(&ppc_intrs_lock);
	i = ppc_intrs[irq];
	mtx_unlock_spin(&ppc_intrs_lock);

	if (i == NULL) {
		i = malloc(sizeof(*i), M_INTR, M_NOWAIT);
		if (i == NULL)
			return (ENOMEM);
		error = intr_event_create(&i->event, (void *)irq, 0,
		    (void (*)(void *))irq_enable, "irq%d:", irq);
		if (error) {
			free(i, M_INTR);
			return (error);
		}

		mtx_lock_spin(&ppc_intrs_lock);
		if (ppc_intrs[irq] != NULL) {
			orphan = i;
			i = ppc_intrs[irq];
			mtx_unlock_spin(&ppc_intrs_lock);

			intr_event_destroy(orphan->event);
			free(orphan, M_INTR);
		} else {
			ppc_intrs[irq] = i;
			idx = intrcnt_index++;
			mtx_unlock_spin(&ppc_intrs_lock);

			i->cntidx = idx;
			i->cntp = &intrcnt[idx];
			intrcnt_setname(i->event->ie_fullname, idx);
		}
	}

	error = intr_event_add_handler(i->event, name, filter, handler, arg,
	    intr_priority(flags), flags, cookiep);
	if (!error)
		intrcnt_setname(i->event->ie_fullname, i->cntidx);
	return (error);
}

int
inthand_remove(u_int irq, void *cookie)
{

	return (intr_event_remove_handler(cookie));
}

void
intr_handle(u_int irq)
{
	struct ppc_intr *i;
	struct intr_event *ie;
	struct intr_handler *ih;
	int error, sched, ret;

	i = ppc_intrs[irq];
	if (i == NULL)
		goto stray;

	atomic_add_long(i->cntp, 1);

	ie = i->event;
	KASSERT(ie != NULL, ("%s: interrupt without an event", __func__));

	if (TAILQ_EMPTY(&ie->ie_handlers))
		goto stray;

	/*
	 * Execute all fast interrupt handlers directly without Giant.  Note
	 * that this means that any fast interrupt handler must be MP safe.
	 */
	ret = 0;
	sched = 0;
	critical_enter();
	TAILQ_FOREACH(ih, &ie->ie_handlers, ih_next) {
		if (ih->ih_filter == NULL) {
			sched = 1;
			continue;
		}
		CTR4(KTR_INTR, "%s: exec %p(%p) for %s", __func__,
		    ih->ih_filter, ih->ih_argument, ih->ih_name);
		ret = ih->ih_filter(ih->ih_argument);
		/*
		 * Wrapper handler special case: see
		 * i386/intr_machdep.c::intr_execute_handlers()
		 */
		if (!sched) {
			if (ret == FILTER_SCHEDULE_THREAD)
				sched = 1;
		}
	}
	critical_exit();

	if (sched) {
		error = intr_event_schedule_thread(ie);
		KASSERT(error == 0, ("%s: impossible stray interrupt",
		    __func__));
	} else
		irq_enable(irq);
	return;

stray:
	atomic_add_long(&intrcnt[0], 1);
	if (intrcnt[0] <= MAX_STRAY_LOG) {
		printf("stray irq %d\n", irq);
		if (intrcnt[0] >= MAX_STRAY_LOG) {
			printf("got %d stray interrupts, not logging anymore\n",
			       MAX_STRAY_LOG);
		}
	}
}
