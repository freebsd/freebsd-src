/*-
 * Copyright (c) 1997 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	From BSDI: intr.c,v 1.6.2.5 1999/07/06 19:16:52 cp Exp
 * $FreeBSD$
 */

/* Interrupt thread code. */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/rtprio.h>			/* change this name XXX */
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/unistd.h>
#include <sys/errno.h>
#include <sys/interrupt.h>
#include <sys/random.h>
#include <sys/resourcevar.h>
#include <sys/time.h>
#include <machine/md_var.h>
#include <machine/segments.h>

#include <i386/isa/icu.h>

#include <isa/isavar.h>
#include <i386/isa/intr_machdep.h>
#include <sys/interrupt.h>

#include <sys/vmmeter.h>
#include <sys/ktr.h>
#include <machine/cpu.h>

struct int_entropy {
	struct proc *p;
	int irq;
};

static u_int straycount[ICU_LEN];

#define	MAX_STRAY_LOG	5

/*
 * Schedule a heavyweight interrupt process.  This function is called
 * from the interrupt handlers Xintr<num>.
 */
void
sched_ithd(void *cookie)
{
	int irq = (int) cookie;		/* IRQ we're handling */
	struct ithd *ithd = ithds[irq];	/* and the process that does it */
	int error;

	/* This used to be in icu_vector.s */
	/*
	 * We count software interrupts when we process them.  The
	 * code here follows previous practice, but there's an
	 * argument for counting hardware interrupts when they're
	 * processed too.
	 */
	atomic_add_long(intr_countp[irq], 1); /* one more for this IRQ */
	atomic_add_int(&cnt.v_intr, 1); /* one more global interrupt */

	/*
	 * Schedule the interrupt thread to run if needed and switch to it
	 * if we schedule it if !cold.
	 */
	error = ithread_schedule(ithd, !cold);

	/*
	 * Log stray interrupts.
	 */
	if (error == EINVAL)
		if (straycount[irq] < MAX_STRAY_LOG) {
			printf("stray irq %d\n", irq);
			if (++straycount[irq] == MAX_STRAY_LOG)
				printf(
			    "got %d stray irq %d's: not logging anymore\n",
				    MAX_STRAY_LOG, irq);
		}
}
