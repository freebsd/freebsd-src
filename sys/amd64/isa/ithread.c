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

#include "opt_auto_eoi.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/rtprio.h>			/* change this name XXX */
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/ipl.h>
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

static u_int straycount[NHWI];

#define	MAX_STRAY_LOG	5

/*
 * Schedule a heavyweight interrupt process.  This function is called
 * from the interrupt handlers Xintr<num>.
 */
void
sched_ithd(void *cookie)
{
	int irq = (int) cookie;		/* IRQ we're handling */
	struct ithd *ir = ithds[irq];	/* and the process that does it */

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
	 * If this interrupt is marked as being a source of entropy, use
	 * the current timestamp to feed entropy to the PRNG.
	 */
	if (ir != NULL && (ir->it_flags & IT_ENTROPY)) {
		struct int_entropy entropy;

		entropy.irq = irq;
		entropy.p = curproc;
		random_harvest(&entropy, sizeof(entropy), 2, 0,
			       RANDOM_INTERRUPT);
	}
		
	/*
	 * If we don't have an interrupt resource or an interrupt thread for
	 * this IRQ, log it as a stray interrupt.
	 */
	if (ir == NULL || ir->it_proc == NULL) {
		if (straycount[irq] < MAX_STRAY_LOG) {
			printf("stray irq %d\n", irq);
			if (++straycount[irq] == MAX_STRAY_LOG)
				printf(
			    "got %d stray irq %d's: not logging anymore\n",
				    MAX_STRAY_LOG, irq);
		}
		return;
	}

	CTR3(KTR_INTR, "sched_ithd pid %d(%s) need=%d",
		ir->it_proc->p_pid, ir->it_proc->p_comm, ir->it_need);

	/*
	 * Set it_need so that if the thread is already running but close
	 * to done, it will do another go-round.  Then get the sched lock
	 * and see if the thread is on whichkqs yet.  If not, put it on
	 * there.  In any case, kick everyone so that if the new thread
	 * is higher priority than their current thread, it gets run now.
	 */
	ir->it_need = 1;
	mtx_lock_spin(&sched_lock);
	if (ir->it_proc->p_stat == SWAIT) { /* not on run queue */
		CTR1(KTR_INTR, "sched_ithd: setrunqueue %d",
			ir->it_proc->p_pid);
/*		membar_lock(); */
		ir->it_proc->p_stat = SRUN;
		setrunqueue(ir->it_proc);
		if (!cold) {
			if (curproc != PCPU_GET(idleproc))
				setrunqueue(curproc);
			curproc->p_stats->p_ru.ru_nvcsw++;
			mi_switch();
		} else
			need_resched();
	}
	else {
		CTR3(KTR_INTR, "sched_ithd %d: it_need %d, state %d",
			ir->it_proc->p_pid,
		        ir->it_need,
		        ir->it_proc->p_stat );
	}
	mtx_unlock_spin(&sched_lock);
}
