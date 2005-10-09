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
__FBSDID("$FreeBSD$");
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

static struct ithd *ithreads[NIRQ];
static int intrcnt_tab[NIRQ];
static int intrcnt_index = 0;
static int last_printed = 0;

void	arm_handler_execute(void *, int);

void
arm_setup_irqhandler(const char *name, void (*hand)(void*), void *arg, 
    int irq, int flags, void **cookiep)
{
	struct ithd *cur_ith;
	int error;

	if (irq < 0 || irq >= NIRQ)
		return;
	cur_ith = ithreads[irq];
	if (cur_ith == NULL) {
		error = ithread_create(&cur_ith, irq, 0, arm_mask_irq,
		    arm_unmask_irq, "intr%d:", irq);
		if (error)
			return;
		ithreads[irq] = cur_ith;
		last_printed += 
		    snprintf(intrnames + last_printed,
		    MAXCOMLEN + 1,
		    "irq%d: %s", irq, name);
		last_printed++;
		intrcnt_tab[irq] = intrcnt_index;
		intrcnt_index++;
		
	}
	ithread_add_handler(cur_ith, name, hand, arg, 
	    ithread_priority(flags), flags, cookiep);
}

int
arm_remove_irqhandler(void *cookie)
{
	return (ithread_remove_handler(cookie));
}

void dosoftints(void);
void
dosoftints(void)
{
}

void
arm_handler_execute(void *frame, int irqnb)
{
	struct ithd *ithd;
	int i;
	struct intrhand *ih;
	struct thread *td = curthread;

	td->td_intr_nesting_level++;
	while ((i = arm_get_next_irq()) != -1) {
		intrcnt[intrcnt_tab[i]]++;
		ithd = ithreads[i];
		if (!ithd)
			continue;
		ih = TAILQ_FIRST(&ithd->it_handlers);
		if (ih && ih->ih_flags & IH_FAST) {
			TAILQ_FOREACH(ih, &ithd->it_handlers,
			    ih_next) {
				ih->ih_handler(ih->ih_argument ?
				    ih->ih_argument : frame);
			}
		} else if (ih) {
			arm_mask_irq(i);
			ithread_schedule(ithd);
		}
	}
	td->td_intr_nesting_level--;
}
