/*
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */


#include <sys/param.h>
#include <sys/bus.h>
#include <sys/rtprio.h>
#include <sys/systm.h>
#include <sys/ipl.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/ktr.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/unistd.h>
#include <sys/vmmeter.h>
#include <machine/atomic.h>
#include <machine/cpu.h>

struct swilist {
	swihand_t	*sl_handler;
	struct swilist	*sl_next;
};

static struct swilist swilists[NSWI];
u_long softintr_count[NSWI];
static struct proc *softithd;
volatile u_int sdelayed;
volatile u_int spending;

static void start_softintr(void *);
static void intr_soft(void *);

void
register_swi(intr, handler)
	int intr;
	swihand_t *handler;
{
	struct swilist *slp, *slq;
	int s;

	if (intr < 0 || intr >= NSWI)
		panic("register_swi: bad intr %d", intr);
	if (handler == swi_generic || handler == swi_null)
		panic("register_swi: bad handler %p", (void *)handler);
	slp = &swilists[intr];
	s = splhigh();
	if (shandlers[intr] == swi_null)
		shandlers[intr] = handler;
	else {
		if (slp->sl_next == NULL) {
			slp->sl_handler = shandlers[intr];
			shandlers[intr] = swi_generic;
		}
		slq = malloc(sizeof(*slq), M_DEVBUF, M_NOWAIT);
		if (slq == NULL)
			panic("register_swi: malloc failed");
		slq->sl_handler = handler;
		slq->sl_next = NULL;
		while (slp->sl_next != NULL)
			slp = slp->sl_next;
		slp->sl_next = slq;
	}
	splx(s);
}

void
swi_dispatcher(intr)
	int intr;
{
	struct swilist *slp;

	slp = &swilists[intr];
	do {
		(*slp->sl_handler)();
		slp = slp->sl_next;
	} while (slp != NULL);
}

void
unregister_swi(intr, handler)
	int intr;
	swihand_t *handler;
{
	struct swilist *slfoundpred, *slp, *slq;
	int s;

	if (intr < 0 || intr >= NSWI)
		panic("unregister_swi: bad intr %d", intr);
	if (handler == swi_generic || handler == swi_null)
		panic("unregister_swi: bad handler %p", (void *)handler);
	slp = &swilists[intr];
	s = splhigh();
	if (shandlers[intr] == handler)
		shandlers[intr] = swi_null;
	else if (slp->sl_next != NULL) {
		slfoundpred = NULL;
		for (slq = slp->sl_next; slq != NULL;
		    slp = slq, slq = slp->sl_next)
			if (slq->sl_handler == handler)
				slfoundpred = slp;
		slp = &swilists[intr];
		if (slfoundpred != NULL) {
			slq = slfoundpred->sl_next;
			slfoundpred->sl_next = slq->sl_next;
			free(slq, M_DEVBUF);
		} else if (slp->sl_handler == handler) {
			slq = slp->sl_next;
			slp->sl_next = slq->sl_next;
			slp->sl_handler = slq->sl_handler;
			free(slq, M_DEVBUF);
		}
		if (slp->sl_next == NULL)
			shandlers[intr] = slp->sl_handler;
	}
	splx(s);
}

int
ithread_priority(flags)
	int flags;
{
	int pri;

	switch (flags) {
	case INTR_TYPE_TTY:             /* keyboard or parallel port */
		pri = PI_TTYLOW;
		break;
	case (INTR_TYPE_TTY | INTR_FAST): /* sio */
		pri = PI_TTYHIGH;
		break;
	case INTR_TYPE_BIO:
		/*
		 * XXX We need to refine this.  BSD/OS distinguishes
		 * between tape and disk priorities.
		 */
		pri = PI_DISK;
		break;
	case INTR_TYPE_NET:
		pri = PI_NET;
		break;
	case INTR_TYPE_CAM:
		pri = PI_DISK;          /* XXX or PI_CAM? */
		break;
	case INTR_TYPE_MISC:
		pri = PI_DULL;          /* don't care */
		break;
	/* We didn't specify an interrupt level. */
	default:
		panic("ithread_priority: no interrupt type in flags");
	}

	return pri;
}

/*
 * Schedule the soft interrupt handler thread.
 */
void
sched_softintr(void)
{
	atomic_add_int(&cnt.v_intr, 1); /* one more global interrupt */

	/*
	 * If we don't have an interrupt resource or an interrupt thread for
	 * this IRQ, log it as a stray interrupt.
	 */
	if (softithd == NULL)
		panic("soft interrupt scheduled too early");

	CTR3(KTR_INTR, "sched_softintr pid %d(%s) spending=0x%x",
		softithd->p_pid, softithd->p_comm, spending);

	/*
	 * Get the sched lock and see if the thread is on whichkqs yet.
	 * If not, put it on there.  In any case, kick everyone so that if
	 * the new thread is higher priority than their current thread, it
	 * gets run now.
	 */
	mtx_enter(&sched_lock, MTX_SPIN);
	if (softithd->p_stat == SWAIT) { /* not on run queue */
		CTR1(KTR_INTR, "sched_softintr: setrunqueue %d",
		    softithd->p_pid);
/*		membar_lock(); */
		softithd->p_stat = SRUN;
		setrunqueue(softithd);
		aston();
	}
	mtx_exit(&sched_lock, MTX_SPIN);
#if 0	
	aston();			/* ??? check priorities first? */
#else
	need_resched();
#endif
}

SYSINIT(start_softintr, SI_SUB_SOFTINTR, SI_ORDER_FIRST, start_softintr, NULL)

/*
 * Start soft interrupt thread.
 */
static void
start_softintr(dummy)
	void *dummy;
{
	int error;

	if (softithd != NULL) {		/* we already have a thread */
		printf("start_softintr: already running");
		return;
	}

	error = kthread_create(intr_soft, NULL, &softithd,
		RFSTOPPED | RFHIGHPID, "softinterrupt");
	if (error)
		panic("start_softintr: kthread_create error %d\n", error);

	softithd->p_rtprio.type = RTP_PRIO_ITHREAD;
	softithd->p_rtprio.prio = PI_SOFT;	/* soft interrupt */
	softithd->p_stat = SWAIT;		/* we're idle */
	softithd->p_flag |= P_NOLOAD;
}

/*
 * Software interrupt process code.
 */
static void
intr_soft(dummy)
	void *dummy;
{
	int i;
	u_int pend;

	/* Main loop */
	for (;;) {
		CTR3(KTR_INTR, "intr_soft pid %d(%s) spending=0x%x",
		    curproc->p_pid, curproc->p_comm, spending);

		/*
		 * Service interrupts.  If another interrupt arrives
		 * while we are running, they will set spending to
		 * denote that we should make another pass.
		 */
		pend = atomic_readandclear_int(&spending);
		while ((i = ffs(pend))) {
			i--;
			atomic_add_long(&softintr_count[i], 1);
			pend &= ~ (1 << i);
			mtx_enter(&Giant, MTX_DEF);
			if (shandlers[i] == swi_generic)
				swi_dispatcher(i);
			else
				(shandlers[i])();
			mtx_exit(&Giant, MTX_DEF);
		}
		/*
		 * Processed all our interrupts.  Now get the sched
		 * lock.  This may take a while and spending may get
		 * set again, so we have to check it again.
		 */
		mtx_enter(&sched_lock, MTX_SPIN);
		if (spending == 0) {
			CTR1(KTR_INTR, "intr_soft pid %d: done",
			    curproc->p_pid);
			curproc->p_stat = SWAIT; /* we're idle */
			mi_switch();
			CTR1(KTR_INTR, "intr_soft pid %d: resumed",
			    curproc->p_pid);
		}
		mtx_exit(&sched_lock, MTX_SPIN);
	}
}

/*
 * Bits in the spending bitmap variable must be set atomically because
 * spending may be manipulated by interrupts or other cpu's without holding 
 * any locks.
 *
 * Note: setbits uses a locked or, making simple cases MP safe.
 */
#define DO_SETBITS(name, var, bits) \
void name(void)					\
{						\
	atomic_set_int(var, bits);		\
	sched_softintr();			\
}

#define DO_SETBITS_AND_NO_MORE(name, var, bits)	\
void name(void)					\
{						\
	atomic_set_int(var, bits);		\
}

DO_SETBITS(setsoftcamnet,&spending, SWI_CAMNET_PENDING)
DO_SETBITS(setsoftcambio,&spending, SWI_CAMBIO_PENDING)
DO_SETBITS(setsoftclock, &spending, SWI_CLOCK_PENDING)
DO_SETBITS(setsoftnet,   &spending, SWI_NET_PENDING)
DO_SETBITS(setsofttty,   &spending, SWI_TTY_PENDING)
DO_SETBITS(setsoftvm,	 &spending, SWI_VM_PENDING)
DO_SETBITS(setsofttq,	 &spending, SWI_TQ_PENDING)

DO_SETBITS_AND_NO_MORE(schedsoftcamnet, &sdelayed, SWI_CAMNET_PENDING)
DO_SETBITS_AND_NO_MORE(schedsoftcambio, &sdelayed, SWI_CAMBIO_PENDING)
DO_SETBITS_AND_NO_MORE(schedsoftnet, &sdelayed, SWI_NET_PENDING)
DO_SETBITS_AND_NO_MORE(schedsofttty, &sdelayed, SWI_TTY_PENDING)
DO_SETBITS_AND_NO_MORE(schedsoftvm, &sdelayed, SWI_VM_PENDING)
DO_SETBITS_AND_NO_MORE(schedsofttq, &sdelayed, SWI_TQ_PENDING)

void
setdelayed(void)
{
	int pend;

	pend = atomic_readandclear_int(&sdelayed);
	if (pend != 0) {
		atomic_set_int(&spending, pend);
		sched_softintr();
	}
}

intrmask_t
softclockpending(void)
{
	return (spending & SWI_CLOCK_PENDING);
}

/*
 * Dummy spl calls.  The only reason for these is to not break
 * all the code which expects to call them.
 */
void spl0 (void) {}
void splx (intrmask_t x) {}
intrmask_t  splq(intrmask_t mask) { return 0; }
intrmask_t  splbio(void) { return 0; }
intrmask_t  splcam(void) { return 0; }
intrmask_t  splclock(void) { return 0; }
intrmask_t  splhigh(void) { return 0; }
intrmask_t  splimp(void) { return 0; }
intrmask_t  splnet(void) { return 0; }
intrmask_t  splsoftcam(void) { return 0; }
intrmask_t  splsoftcambio(void) { return 0; }
intrmask_t  splsoftcamnet(void) { return 0; }
intrmask_t  splsoftclock(void) { return 0; }
intrmask_t  splsofttty(void) { return 0; }
intrmask_t  splsoftvm(void) { return 0; }
intrmask_t  splsofttq(void) { return 0; }
intrmask_t  splstatclock(void) { return 0; }
intrmask_t  spltty(void) { return 0; }
intrmask_t  splvm(void) { return 0; }
