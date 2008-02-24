/*	$NetBSD: intr.c,v 1.12 2003/07/15 00:24:41 lukem Exp $	*/

/*-
 * Copyright (c) 2004 Olivier Houchard.
 * Copyright (c) 1994-1998 Mark Brinicombe.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Soft interrupt and other generic interrupt functions.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/arm/arm/intr.c,v 1.17 2007/07/27 14:26:42 cognet Exp $");
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslog.h> 
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/conf.h>
#include <machine/atomic.h>
#include <machine/intr.h>
#include <machine/cpu.h>

static struct intr_event *intr_events[NIRQ];
static int intrcnt_tab[NIRQ];
static int intrcnt_index = 0;
static int last_printed = 0;

void	arm_handler_execute(struct trapframe *, int);

#ifdef INTR_FILTER
static void
intr_disab_eoi_src(void *arg)
{
	uintptr_t nb;

	nb = (uintptr_t)arg;
	arm_mask_irq(nb);
}

static void
intr_eoi_src(void *arg)
{
	uintptr_t nb;

	nb = (uintptr_t)arg;
	arm_unmask_irq(nb);
}

#endif

void
arm_setup_irqhandler(const char *name, driver_filter_t *filt, 
    void (*hand)(void*), void *arg, int irq, int flags, void **cookiep)
{
	struct intr_event *event;
	int error;

	if (irq < 0 || irq >= NIRQ)
		return;
	event = intr_events[irq];
	if (event == NULL) {
#ifdef INTR_FILTER
		error = intr_event_create(&event, (void *)irq, 0,
		    (void (*)(void *))arm_unmask_irq, intr_eoi_src,
		    intr_disab_eoi_src, "intr%d:", irq);
#else
		error = intr_event_create(&event, (void *)irq, 0,
		    (void (*)(void *))arm_unmask_irq, "intr%d:", irq);
#endif
		if (error)
			return;
		intr_events[irq] = event;
		last_printed += 
		    snprintf(intrnames + last_printed,
		    MAXCOMLEN + 1,
		    "irq%d: %s", irq, name);
		last_printed++;
		intrcnt_tab[irq] = intrcnt_index;
		intrcnt_index++;
		
	}
	intr_event_add_handler(event, name, filt, hand, arg,
	    intr_priority(flags), flags, cookiep);
}

int
arm_remove_irqhandler(void *cookie)
{
	return (intr_event_remove_handler(cookie));
}

void dosoftints(void);
void
dosoftints(void)
{
}

void
arm_handler_execute(struct trapframe *frame, int irqnb)
{
	struct intr_event *event;
	struct thread *td = curthread;
#ifdef INTR_FILTER
	int i;
#else
	int i, thread, ret;
	struct intr_handler *ih;
#endif

	PCPU_INC(cnt.v_intr);
	td->td_intr_nesting_level++;
	while ((i = arm_get_next_irq()) != -1) {
#ifndef INTR_FILTER
		arm_mask_irq(i);
#endif
		intrcnt[intrcnt_tab[i]]++;
		event = intr_events[i];
		if (!event || TAILQ_EMPTY(&event->ie_handlers)) {
#ifdef INTR_FILTER
			arm_mask_irq(i);
#endif
			continue;
		}

#ifdef INTR_FILTER
		intr_event_handle(event, frame);
		/* XXX: Log stray IRQs */
#else
		/* Execute fast handlers. */
		ret = 0;
		thread = 0;
		TAILQ_FOREACH(ih, &event->ie_handlers, ih_next) {
			if (ih->ih_filter == NULL)
				thread = 1;
			else
				ret = ih->ih_filter(ih->ih_argument ?
				    ih->ih_argument : frame);
			/*
			 * Wrapper handler special case: see
			 * i386/intr_machdep.c::intr_execute_handlers()
			 */
			if (!thread) {
				if (ret == FILTER_SCHEDULE_THREAD)
					thread = 1;
			}
		}

		/* Schedule thread if needed. */
		if (thread)
			intr_event_schedule_thread(event);
		else
			arm_unmask_irq(i);
#endif
	}
	td->td_intr_nesting_level--;
}
