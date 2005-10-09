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
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/vmmeter.h>
#include <sys/proc.h>

#include <machine/frame.h>
#include <machine/interruptvar.h>
#include <machine/intr_machdep.h>
#include <machine/trap.h>

#define	MAX_STRAY_LOG	5

MALLOC_DEFINE(M_INTR, "intr", "interrupt handler data");

static int	intr_initialized = 0;

static u_int		intr_nirq;
static struct		intr_handler *intr_handlers;

static struct		mtx intr_table_lock;

extern int	extint, extsize;
extern u_long	extint_call;

static int 		intrcnt_index;
static ih_func_t	intr_stray_handler;
static ih_func_t	sched_ithd;

static void		(*irq_enable)(uintptr_t);
static void		(*irq_disable)(uintptr_t);

static void intrcnt_setname(const char *name, int index);
static void intrcnt_updatename(struct intr_handler *ih);

static void
intrcnt_setname(const char *name, int index)
{
	snprintf(intrnames + (MAXCOMLEN + 1) * index, MAXCOMLEN + 1, "%-*s",
	    MAXCOMLEN, name);
}

static void
intrcnt_updatename(struct intr_handler *ih)
{
	intrcnt_setname(ih->ih_ithd->it_td->td_proc->p_comm, ih->ih_index);
}

static void
intrcnt_register(struct intr_handler *ih)
{
	char straystr[MAXCOMLEN + 1];

	KASSERT(ih->ih_ithd != NULL,
		("%s: intr_handler with no ithread", __func__));

	ih->ih_index = intrcnt_index;
	intrcnt_index += 2;
	snprintf(straystr, MAXCOMLEN + 1, "stray irq%d", ih->ih_irq);
	intrcnt_updatename(ih);
	ih->ih_count = &intrcnt[ih->ih_index];
	intrcnt_setname(straystr, ih->ih_index + 1);
	ih->ih_straycount = &intrcnt[ih->ih_index + 1];
}

void
intr_init(void (*handler)(void), int nirq, void (*irq_e)(uintptr_t),
    void (*irq_d)(uintptr_t))
{
	int		i;
	u_int32_t	msr;

	if (intr_initialized != 0)
		panic("intr_init: interrupts intialized twice\n");

	intr_initialized++;

	intr_nirq = nirq;
	intr_handlers = malloc(nirq * sizeof(struct intr_handler), M_INTR,
	    M_NOWAIT|M_ZERO);
	if (intr_handlers == NULL)
		panic("intr_init: unable to allocate interrupt handler array");

	for (i = 0; i < nirq; i++) {
		intr_handlers[i].ih_func = intr_stray_handler;
		intr_handlers[i].ih_arg = &intr_handlers[i];
		intr_handlers[i].ih_irq = i;
		intr_handlers[i].ih_flags = 0;
		/* mux all initial stray irqs onto same count... */
		intr_handlers[i].ih_straycount = &intrcnt[0];
	}

	intrcnt_setname("???", 0);
	intrcnt_index = 1;

	msr = mfmsr();
	mtmsr(msr & ~PSL_EE);

	ext_intr_install(handler);

	mtmsr(msr);

	irq_enable = irq_e;
	irq_disable = irq_d;

	mtx_init(&intr_table_lock, "ithread table lock", NULL, MTX_SPIN);
}

void
intr_setup(u_int irq, ih_func_t *ihf, void *iha, u_int flags)
{
	u_int32_t	msr;

	msr = mfmsr();
	mtmsr(msr & ~PSL_EE);

	intr_handlers[irq].ih_func = ihf;
	intr_handlers[irq].ih_arg = iha;
	intr_handlers[irq].ih_irq = irq;
	intr_handlers[irq].ih_flags = flags;

	mtmsr(msr);
}

int
inthand_add(const char *name, u_int irq, void (*handler)(void *), void *arg,
    int flags, void **cookiep)
{
	struct	intr_handler *ih;
	struct	ithd *ithd, *orphan;
	int	error = 0;
	int	created_ithd = 0;

	/*
	 * Work around a race where more than one CPU may be registering
	 * handlers on the same IRQ at the same time.
	 */
	ih = &intr_handlers[irq];
	mtx_lock_spin(&intr_table_lock);
	ithd = ih->ih_ithd;
	mtx_unlock_spin(&intr_table_lock);
	if (ithd == NULL) {
		error = ithread_create(&ithd, irq, 0, irq_disable,
		    irq_enable, "irq%d:", irq);
		if (error)
			return (error);

		mtx_lock_spin(&intr_table_lock);

		if (ih->ih_ithd == NULL) {
			ih->ih_ithd = ithd;
			created_ithd++;
			mtx_unlock_spin(&intr_table_lock);
		} else {
			orphan = ithd;
			ithd = ih->ih_ithd;
			mtx_unlock_spin(&intr_table_lock);
			ithread_destroy(orphan);
		}
	}

	error = ithread_add_handler(ithd, name, handler, arg,
	    ithread_priority(flags), flags, cookiep);

	if ((flags & INTR_FAST) == 0 || error) {
		intr_setup(irq, sched_ithd, ih, flags);
		error = 0;
	}

	if (error)
		return (error);

	if (flags & INTR_FAST)
		intr_setup(irq, handler, arg, flags);

	intrcnt_register(ih);

	return (0);
}

int
inthand_remove(u_int irq, void *cookie)
{
	struct	intr_handler *ih;
	int	error;

	error = ithread_remove_handler(cookie);

	if (error == 0) {
		ih = &intr_handlers[irq];

		mtx_lock_spin(&intr_table_lock);

		if (ih->ih_ithd == NULL) {
			intr_setup(irq, intr_stray_handler, ih, 0);
		} else {
			intr_setup(irq, sched_ithd, ih, 0);
		}

		mtx_unlock_spin(&intr_table_lock);
	}

	return (error);
}

void
intr_handle(u_int irq)
{
	atomic_add_long(intr_handlers[irq].ih_count, 1);
	intr_handlers[irq].ih_func(intr_handlers[irq].ih_arg);

	/* XXX wrong thing when using pre-emption ? */
	if ((intr_handlers[irq].ih_flags & INTR_FAST) != 0)
		irq_enable(irq);
}

static void
intr_stray_handler(void *cookie)
{
	struct	intr_handler *ih;

	ih = (struct intr_handler *)cookie;

	if (*intr_handlers[ih->ih_irq].ih_straycount < MAX_STRAY_LOG) {
		printf("stray irq %d\n", ih->ih_irq);

		atomic_add_long(intr_handlers[ih->ih_irq].ih_straycount, 1);
		if (*intr_handlers[ih->ih_irq].ih_straycount >= MAX_STRAY_LOG)
			printf("got %d stray irq %d's: not logging anymore\n",
			       MAX_STRAY_LOG, ih->ih_irq);
	}
}

static void
sched_ithd(void *cookie)
{
	struct	intr_handler *ih;
	int	error;

	ih = (struct intr_handler *)cookie;

	error = ithread_schedule(ih->ih_ithd);

	if (error == EINVAL)
		intr_stray_handler(ih);
}
