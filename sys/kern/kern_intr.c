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
#include <machine/md_var.h>

#include <net/netisr.h>		/* prototype for legacy_setsoftnet */

struct intrhand *net_ih;
struct intrhand *vm_ih;
struct intrhand *softclock_ih;
struct ithd	*clk_ithd;
struct ithd	*tty_ithd;

static void start_softintr(void *);
static void swi_net(void *);

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

void sithd_loop(void *);

struct intrhand *
sinthand_add(const char *name, struct ithd **ithdp, driver_intr_t handler, 
	    void *arg, int pri, int flags)
{
	struct proc *p;
	struct ithd *ithd;
	struct intrhand *ih;		
	struct intrhand *this_ih;

	ithd = (ithdp != NULL) ? *ithdp : NULL;


	if (ithd == NULL) {
		int error;
		ithd = malloc(sizeof (struct ithd), M_DEVBUF, M_WAITOK | M_ZERO);
		error = kthread_create(sithd_loop, NULL, &p,
			RFSTOPPED | RFHIGHPID, "swi%d: %s", pri, name);
		if (error)
			panic("inthand_add: Can't create interrupt thread");
		ithd->it_proc = p;
		p->p_ithd = ithd;
		p->p_rtprio.type = RTP_PRIO_ITHREAD;
		p->p_rtprio.prio = pri + PI_SOFT;	/* soft interrupt */
		p->p_stat = SWAIT;			/* we're idle */
		/* XXX - some hacks are _really_ gross */
		if (pri == SWI_CLOCK)
			p->p_flag |= P_NOLOAD;
		if (ithdp != NULL)
			*ithdp = ithd;
	}
	this_ih = malloc(sizeof (struct intrhand), M_DEVBUF, M_WAITOK | M_ZERO);
	this_ih->ih_handler = handler;
	this_ih->ih_argument = arg;
	this_ih->ih_flags = flags;
	this_ih->ih_ithd = ithd;
	this_ih->ih_name = malloc(strlen(name) + 1, M_DEVBUF, M_WAITOK);
	if ((ih = ithd->it_ih)) {
		while (ih->ih_next != NULL)
			ih = ih->ih_next;
		ih->ih_next = this_ih;
	} else
		ithd->it_ih = this_ih;
	strcpy(this_ih->ih_name, name);
	return (this_ih);
}


/*
 * Schedule a heavyweight software interrupt process. 
 */
void
sched_swi(struct intrhand *ih, int flag)
{
	struct ithd *it = ih->ih_ithd;	/* and the process that does it */
	struct proc *p = it->it_proc;

	atomic_add_int(&cnt.v_intr, 1); /* one more global interrupt */
		
	CTR3(KTR_INTR, "sched_sihand pid %d(%s) need=%d",
		p->p_pid, p->p_comm, it->it_need);

	/*
	 * Set it_need so that if the thread is already running but close
	 * to done, it will do another go-round.  Then get the sched lock
	 * and see if the thread is on whichkqs yet.  If not, put it on
	 * there.  In any case, kick everyone so that if the new thread
	 * is higher priority than their current thread, it gets run now.
	 */
	ih->ih_need = 1;
	if (!(flag & SWI_DELAY)) {
		it->it_need = 1;
		mtx_enter(&sched_lock, MTX_SPIN);
		if (p->p_stat == SWAIT) { /* not on run queue */
			CTR1(KTR_INTR, "sched_ithd: setrunqueue %d", p->p_pid);
/*			membar_lock(); */
			p->p_stat = SRUN;
			setrunqueue(p);
			aston();
		}
		else {
			CTR3(KTR_INTR, "sched_ithd %d: it_need %d, state %d",
				p->p_pid, it->it_need, p->p_stat );
		}
		mtx_exit(&sched_lock, MTX_SPIN);
		need_resched();
	}
}

/*
 * This is the main code for soft interrupt threads.
 */
void
sithd_loop(void *dummy)
{
	struct ithd *it;		/* our thread context */
	struct intrhand *ih;		/* and our interrupt handler chain */
	
	struct proc *p = curproc;
	it = p->p_ithd;			/* point to myself */

	/*
	 * As long as we have interrupts outstanding, go through the
	 * list of handlers, giving each one a go at it.
	 */
	for (;;) {
		CTR3(KTR_INTR, "sithd_loop pid %d(%s) need=%d",
		     p->p_pid, p->p_comm, it->it_need);
		while (it->it_need) {
			/*
			 * Service interrupts.  If another interrupt
			 * arrives while we are running, they will set
			 * it_need to denote that we should make
			 * another pass.
			 */
			it->it_need = 0;
			for (ih = it->it_ih; ih != NULL; ih = ih->ih_next) {
				if (!ih->ih_need)
					continue;
				ih->ih_need = 0;
				CTR5(KTR_INTR,
				    "sithd_loop pid %d ih=%p: %p(%p) flg=%x",
				    p->p_pid, (void *)ih,
				    (void *)ih->ih_handler, ih->ih_argument,
				    ih->ih_flags);

				if ((ih->ih_flags & INTR_MPSAFE) == 0)
					mtx_enter(&Giant, MTX_DEF);
				ih->ih_handler(ih->ih_argument);
				if ((ih->ih_flags & INTR_MPSAFE) == 0)
					mtx_exit(&Giant, MTX_DEF);
			}
		}

		/*
		 * Processed all our interrupts.  Now get the sched
		 * lock.  This may take a while and it_need may get
		 * set again, so we have to check it again.
		 */
		mtx_enter(&sched_lock, MTX_SPIN);
		if (!it->it_need) {
			p->p_stat = SWAIT; /* we're idle */
			CTR1(KTR_INTR, "sithd_loop pid %d: done", p->p_pid);
			mi_switch();
			CTR1(KTR_INTR, "sithd_loop pid %d: resumed", p->p_pid);
		}
		mtx_exit(&sched_lock, MTX_SPIN);
	}
}

SYSINIT(start_softintr, SI_SUB_SOFTINTR, SI_ORDER_FIRST, start_softintr, NULL)

/*
 * Start standard software interrupt threads
 */
static void
start_softintr(dummy)
	void *dummy;
{
	net_ih = sinthand_add("net", NULL, swi_net, NULL, SWI_NET, 0);
	softclock_ih = 
	    sinthand_add("clock", &clk_ithd, softclock, NULL, SWI_CLOCK, 0);
	vm_ih = sinthand_add("vm", NULL, swi_vm, NULL, SWI_VM, 0);
}

void
legacy_setsoftnet()
{
	sched_swi(net_ih, SWI_NOSWITCH);
}

/*
 * XXX: This should really be in the network code somewhere and installed
 * via a SI_SUB_SOFINTR, SI_ORDER_MIDDLE sysinit.
 */
void	(*netisrs[32]) __P((void));
u_int	netisr;

static void
swi_net(void *dummy)
{
	u_int bits;
	int i;

	bits = atomic_readandclear_int(&netisr);
	while ((i = ffs(bits)) != 0) {
		i--;
		netisrs[i]();
		bits &= ~(1 << i);
	}
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
