/*	$NetBSD: intr.c,v 1.12 2003/07/15 00:24:41 lukem Exp $	*/

/*
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
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/conf.h>
#include <machine/atomic.h>
#include <machine/intr.h>
#include <machine/cpu.h>

int current_spl_level = _SPL_SERIAL;

u_int spl_masks[_SPL_LEVELS + 1];
u_int spl_smasks[_SPL_LEVELS];
extern u_int irqmasks[];

#define NIRQ 0x20 /* XXX */
struct ithd *ithreads[NIRQ];
void
set_splmasks()
{
		int loop;

	for (loop = 0; loop < _SPL_LEVELS; ++loop) {
		spl_masks[loop] = 0xffffffff;
		spl_smasks[loop] = 1;
	}

	spl_masks[_SPL_NET]        = irqmasks[IPL_NET];
	spl_masks[_SPL_SOFTSERIAL] = irqmasks[IPL_TTY];
	spl_masks[_SPL_TTY]        = irqmasks[IPL_TTY];
	spl_masks[_SPL_VM]         = irqmasks[IPL_VM];
	spl_masks[_SPL_AUDIO]      = irqmasks[IPL_AUDIO];
	spl_masks[_SPL_CLOCK]      = irqmasks[IPL_CLOCK];
#ifdef IPL_STATCLOCK
	spl_masks[_SPL_STATCLOCK]  = irqmasks[IPL_STATCLOCK];
#else
	spl_masks[_SPL_STATCLOCK]  = irqmasks[IPL_CLOCK];
#endif
	spl_masks[_SPL_HIGH]       = irqmasks[IPL_HIGH];
	spl_masks[_SPL_SERIAL]     = irqmasks[IPL_SERIAL];
	spl_masks[_SPL_LEVELS]     = 0;

	spl_smasks[_SPL_0] = 0xffffffff;
	for (loop = 0; loop < _SPL_SOFTSERIAL; ++loop)
		spl_smasks[loop] |= SOFTIRQ_BIT(SOFTIRQ_SERIAL);
	for (loop = 0; loop < _SPL_SOFTNET; ++loop)
		spl_smasks[loop] |= SOFTIRQ_BIT(SOFTIRQ_NET);
	for (loop = 0; loop < _SPL_SOFTCLOCK; ++loop)
		spl_smasks[loop] |= SOFTIRQ_BIT(SOFTIRQ_CLOCK);
}

void arm_setup_irqhandler(const char *name, void (*hand)(void*), void *arg, 
    int irq, int flags, void **cookiep)
{
	struct ithd *cur_ith;
	int error;

	if (irq < 0 || irq >= NIRQ)
		return;
	cur_ith = ithreads[irq];
	if (cur_ith == NULL) {
		error = ithread_create(&cur_ith, irq, 0, NULL, NULL, "intr%d:",
		    irq);
		if (error)
			return;
		ithreads[irq] = cur_ith;
	}
	ithread_add_handler(cur_ith, name, hand, arg, ithread_priority(flags),
	    flags, cookiep);
}

void dosoftints(void);
void
dosoftints(void)
{
}

void
arm_handler_execute(void *);
void
arm_handler_execute(void *irq)
{
	struct ithd *ithd;
	int i;
	int irqnb = (int)irq;
	struct intrhand *ih;

	for (i = 0; i < NIRQ; i++) {
		if (1 << i & irqnb) {
			ithd = ithreads[i];
			if (!ithd) /* FUCK */
				return;
			ih = TAILQ_FIRST(&ithd->it_handlers);
			if (ih && ih->ih_flags & IH_FAST) {
				TAILQ_FOREACH(ih, &ithd->it_handlers,
				    ih_next) {
					ih->ih_handler(ih->ih_argument);
					/* 
					 * XXX: what about the irq frame if
					 * the arg is NULL ?
					 */
				}
			} else if (ih) {
				ithread_schedule(ithd, !cold);
			}
		}
	}	
}
