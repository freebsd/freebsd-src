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

#include "isa.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/rtprio.h>			/* change this name XXX */
#ifndef SMP
#include <machine/lock.h>
#endif
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/unistd.h>
#include <sys/errno.h>
#include <sys/interrupt.h>
#include <machine/ipl.h>
#include <machine/md_var.h>
#include <machine/segments.h>

#if defined(APIC_IO)
#include <machine/smp.h>
#include <machine/smptests.h>			/** FAST_HI */
#include <machine/resource.h>
#endif /* APIC_IO */
#ifdef PC98
#include <pc98/pc98/pc98.h>
#include <pc98/pc98/pc98_machdep.h>
#include <pc98/pc98/epsonio.h>
#else
#include <i386/isa/isa.h>
#endif
#include <i386/isa/icu.h>

#if NISA > 0
#include <isa/isavar.h>
#endif
#include <i386/isa/intr_machdep.h>
#include <sys/interrupt.h>
#ifdef APIC_IO
#include <machine/clock.h>
#endif

#include "mca.h"
#if NMCA > 0
#include <i386/isa/mca_machdep.h>
#endif

#include <sys/vmmeter.h>
#include <machine/mutex.h>
#include <sys/ktr.h>
#include <machine/cpu.h>

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
	mtx_enter(&sched_lock, MTX_SPIN);
	if (ir->it_proc->p_stat == SWAIT) { /* not on run queue */
		CTR1(KTR_INTR, "sched_ithd: setrunqueue %d",
			ir->it_proc->p_pid);
/*		membar_lock(); */
		ir->it_proc->p_stat = SRUN;
		setrunqueue(ir->it_proc);
		aston();
	}
	else {
		CTR3(KTR_INTR, "sched_ithd %d: it_need %d, state %d",
			ir->it_proc->p_pid,
		        ir->it_need,
		        ir->it_proc->p_stat );
	}
	mtx_exit(&sched_lock, MTX_SPIN);
	need_resched();
}

/*
 * This is the main code for all interrupt threads.  It gets put on
 * whichkqs by setrunqueue above.
 */
void
ithd_loop(void *dummy)
{
	struct ithd *me;		/* our thread context */
	struct intrec *ih;		/* and our interrupt handler chain */

	me = curproc->p_ithd;		/* point to myself */

	/*
	 * As long as we have interrupts outstanding, go through the
	 * list of handlers, giving each one a go at it.
	 */
	for (;;) {
		CTR3(KTR_INTR, "ithd_loop pid %d(%s) need=%d",
		     me->it_proc->p_pid, me->it_proc->p_comm, me->it_need);
		while (me->it_need) {
			/*
			 * Service interrupts.  If another interrupt
			 * arrives while we are running, they will set
			 * it_need to denote that we should make
			 * another pass.
			 */
			me->it_need = 0;
#if 0
			membar_unlock(); /* push out "it_need=0" */
#endif
			for (ih = me->it_ih; ih != NULL; ih = ih->next) {
				CTR5(KTR_INTR,
				    "ithd_loop pid %d ih=%p: %p(%p) flg=%x",
				    me->it_proc->p_pid, (void *)ih,
				    (void *)ih->handler, ih->argument,
				    ih->flags);

				if ((ih->flags & INTR_MPSAFE) == 0)
					mtx_enter(&Giant, MTX_DEF);
				ih->handler(ih->argument);
				if ((ih->flags & INTR_MPSAFE) == 0)
					mtx_exit(&Giant, MTX_DEF);
			}
		}

		/*
		 * Processed all our interrupts.  Now get the sched
		 * lock.  This may take a while and it_need may get
		 * set again, so we have to check it again.
		 */
		mtx_enter(&sched_lock, MTX_SPIN);
		if (!me->it_need) {

			INTREN (1 << me->irq); /* reset the mask bit */
			me->it_proc->p_stat = SWAIT; /* we're idle */
#ifdef APIC_IO
			CTR1(KTR_INTR, "ithd_loop pid %d: done",
				me->it_proc->p_pid);
#else
			CTR2(KTR_INTR, "ithd_loop pid %d: done, imen=%x",
				me->it_proc->p_pid, imen);
#endif
			mi_switch();
			CTR1(KTR_INTR, "ithd_loop pid %d: resumed",
				me->it_proc->p_pid);
		}
		mtx_exit(&sched_lock, MTX_SPIN);
	}
}
