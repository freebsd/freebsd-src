/* $FreeBSD$ */
/* $NetBSD: interrupt.c,v 1.23 1998/02/24 07:38:01 thorpej Exp $ */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Keith Bostic, Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * Additional Copyright (c) 1997 by Matthew Jacob for NASA/Ames Research Center.
 * Redistribute and modify at will, leaving only this additional copyright
 * notice.
 */

#include "opt_ddb.h"

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

/* __KERNEL_RCSID(0, "$NetBSD: interrupt.c,v 1.23 1998/02/24 07:38:01 thorpej Exp $");*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vmmeter.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/interrupt.h>
#include <sys/ipl.h>
#include <sys/kthread.h>
#include <sys/ktr.h>
#include <sys/mutex.h>
#include <sys/unistd.h>

#include <machine/reg.h>
#include <machine/frame.h>
#include <machine/cpuconf.h>
#include <machine/bwx.h>
#include <machine/intr.h>
#include <machine/rpb.h>

#ifdef EVCNT_COUNTERS
struct evcnt clock_intr_evcnt;	/* event counter for clock intrs. */
#else
#include <machine/intrcnt.h>
#endif

#ifdef DDB
#include <ddb/ddb.h>
#endif

volatile int mc_expected, mc_received;

static void 
dummy_perf(unsigned long vector, struct trapframe *framep)  
{
	printf("performance interrupt!\n");
}

void (*perf_irq)(unsigned long, struct trapframe *) = dummy_perf;


static u_int schedclk2;
static void ithd_loop(void *);
static driver_intr_t alpha_clock_interrupt;

void
interrupt(a0, a1, a2, framep)
	unsigned long a0, a1, a2;
	struct trapframe *framep;
{
	/*
	 * Find our per-cpu globals.
	 */
	globalp = (struct globaldata *) alpha_pal_rdval();

	atomic_add_int(&PCPU_GET(intr_nesting_level), 1);
	{
		struct proc* p = curproc;
		if (!p) p = &proc0;
		if ((caddr_t) framep < (caddr_t) p->p_addr + 1024) {
			mtx_enter(&Giant, MTX_DEF);
			panic("possible stack overflow\n");
		}
	}

	framep->tf_regs[FRAME_TRAPARG_A0] = a0;
	framep->tf_regs[FRAME_TRAPARG_A1] = a1;
	framep->tf_regs[FRAME_TRAPARG_A2] = a2;
	switch (a0) {
	case ALPHA_INTR_XPROC:	/* interprocessor interrupt */
		CTR0(KTR_INTR|KTR_SMP, "interprocessor interrupt");
		smp_handle_ipi(framep); /* note: lock not taken */
		break;
		
	case ALPHA_INTR_CLOCK:	/* clock interrupt */
		CTR0(KTR_INTR, "clock interrupt");
		if (PCPU_GET(cpuno) != hwrpb->rpb_primary_cpu_id) {
			CTR0(KTR_INTR, "ignoring clock on secondary");
			return;
		}
			
		mtx_enter(&Giant, MTX_DEF);
		alpha_clock_interrupt(framep);
		mtx_exit(&Giant, MTX_DEF);
		break;

	case  ALPHA_INTR_ERROR:	/* Machine Check or Correctable Error */
		mtx_enter(&Giant, MTX_DEF);
		a0 = alpha_pal_rdmces();
		if (platform.mcheck_handler)
			(*platform.mcheck_handler)(a0, framep, a1, a2);
		else
			machine_check(a0, framep, a1, a2);
		mtx_exit(&Giant, MTX_DEF);
		break;

	case ALPHA_INTR_DEVICE:	/* I/O device interrupt */
		mtx_enter(&Giant, MTX_DEF);
		cnt.v_intr++;
		if (platform.iointr)
			(*platform.iointr)(framep, a1);
		mtx_exit(&Giant, MTX_DEF);
		break;

	case ALPHA_INTR_PERF:	/* interprocessor interrupt */
		mtx_enter(&Giant, MTX_DEF);
		perf_irq(a1, framep);
		mtx_exit(&Giant, MTX_DEF);
		break;

	case ALPHA_INTR_PASSIVE:
#if	0
		printf("passive release interrupt vec 0x%lx (ignoring)\n", a1);
#endif
		break;

	default:
		mtx_enter(&Giant, MTX_DEF);
		panic("unexpected interrupt: type 0x%lx vec 0x%lx a2 0x%lx\n",
		    a0, a1, a2);
		/* NOTREACHED */
	}
	atomic_subtract_int(&PCPU_GET(intr_nesting_level), 1);
}

void
set_iointr(niointr)
	void (*niointr) __P((void *, unsigned long));
{
	if (platform.iointr)
		panic("set iointr twice");
	platform.iointr = niointr;
}


void
machine_check(mces, framep, vector, param)
	unsigned long mces;
	struct trapframe *framep;
	unsigned long vector, param;
{
	const char *type;

	/* Make sure it's an error we know about. */
	if ((mces & (ALPHA_MCES_MIP|ALPHA_MCES_SCE|ALPHA_MCES_PCE)) == 0) {
		type = "fatal machine check or error (unknown type)";
		goto fatal;
	}

	/* Machine checks. */
	if (mces & ALPHA_MCES_MIP) {
		/* If we weren't expecting it, then we punt. */
		if (!mc_expected) {
			type = "unexpected machine check";
			goto fatal;
		}

		mc_expected = 0;
		mc_received = 1;
	}

	/* System correctable errors. */
	if (mces & ALPHA_MCES_SCE)
		printf("Warning: received system correctable error.\n");

	/* Processor correctable errors. */
	if (mces & ALPHA_MCES_PCE)
		printf("Warning: received processor correctable error.\n"); 

	/* Clear pending machine checks and correctable errors */
	alpha_pal_wrmces(mces);
	return;

fatal:
	/* Clear pending machine checks and correctable errors */
	alpha_pal_wrmces(mces);

	printf("\n");
	printf("%s:\n", type);
	printf("\n");
	printf("    mces    = 0x%lx\n", mces);
	printf("    vector  = 0x%lx\n", vector);
	printf("    param   = 0x%lx\n", param);
	printf("    pc      = 0x%lx\n", framep->tf_regs[FRAME_PC]);
	printf("    ra      = 0x%lx\n", framep->tf_regs[FRAME_RA]);
	printf("    curproc = %p\n", curproc);
	if (curproc != NULL)
		printf("        pid = %d, comm = %s\n", curproc->p_pid,
		    curproc->p_comm);
	printf("\n");
#ifdef DDB
	kdb_trap(mces, vector, param, ALPHA_KENTRY_MM, framep);
#endif
	panic("machine check");
}

int
badaddr(addr, size)
	void *addr;
	size_t size;
{
	return(badaddr_read(addr, size, NULL));
}

int
badaddr_read(addr, size, rptr)
	void *addr;
	size_t size;
	void *rptr;
{
	long rcpt;

	/* Get rid of any stale machine checks that have been waiting.  */
	alpha_pal_draina();

	/* Tell the trap code to expect a machine check. */
	mc_received = 0;
	mc_expected = 1;

	/* Read from the test address, and make sure the read happens. */
	alpha_mb();
	switch (size) {
	case sizeof (u_int8_t):
		if (alpha_implver() >= ALPHA_IMPLVER_EV5
		    && alpha_amask(ALPHA_AMASK_BWX) == 0)
			rcpt = ldbu((vm_offset_t)addr);
		else
			rcpt = *(volatile u_int8_t *)addr;
		break;

	case sizeof (u_int16_t):
		if (alpha_implver() >= ALPHA_IMPLVER_EV5
		    && alpha_amask(ALPHA_AMASK_BWX) == 0)
			rcpt = ldwu((vm_offset_t)addr);
		else
			rcpt = *(volatile u_int16_t *)addr;
		break;

	case sizeof (u_int32_t):
		rcpt = *(volatile u_int32_t *)addr;
		break;

	case sizeof (u_int64_t):
		rcpt = *(volatile u_int64_t *)addr;
		break;

	default:
		panic("badaddr: invalid size (%ld)\n", size);
	}
	alpha_mb();

	/* Make sure we took the machine check, if we caused one. */
	alpha_pal_draina();

	/* disallow further machine checks */
	mc_expected = 0;

	if (rptr) {
		switch (size) {
		case sizeof (u_int8_t):
			*(volatile u_int8_t *)rptr = rcpt;
			break;

		case sizeof (u_int16_t):
			*(volatile u_int16_t *)rptr = rcpt;
			break;

		case sizeof (u_int32_t):
			*(volatile u_int32_t *)rptr = rcpt;
			break;

		case sizeof (u_int64_t):
			*(volatile u_int64_t *)rptr = rcpt;
			break;
		}
	}
	/* Return non-zero (i.e. true) if it's a bad address. */
	return (mc_received);
}

#define HASHVEC(vector)	((vector) % 31)

LIST_HEAD(alpha_intr_list, alpha_intr);

struct alpha_intr {
    LIST_ENTRY(alpha_intr) list; /* chain handlers in this hash bucket */
    int			vector;	/* vector to match */
    struct ithd		*ithd;  /* interrupt thread */
    volatile long	*cntp;  /* interrupt counter */
    void		(*disable)(int); /* disable source */
    void		(*enable)(int);	/* enable source */
};

static struct alpha_intr_list alpha_intr_hash[31];

int
alpha_setup_intr(const char *name, int vector, driver_intr_t *handler,
		 void *arg, int pri, void **cookiep, volatile long *cntp,
		 void (*disable)(int), void (*enable)(int))
{
	int h = HASHVEC(vector);
	struct alpha_intr *i;
	struct intrhand *head, *idesc;
	struct ithd *ithd;
	struct proc *p;
	int s, errcode;

	/* First, check for an existing hash table entry for this vector. */
	for (i = LIST_FIRST(&alpha_intr_hash[h]); i && i->vector != vector;
	    i = LIST_NEXT(i, list))
		;	/* nothing */
	if (i == NULL) {
		/* None was found, so create an entry. */
		i = malloc(sizeof(struct alpha_intr), M_DEVBUF, M_NOWAIT);
		if (i == NULL)
			return ENOMEM;
		i->vector = vector;
		i->ithd = NULL;
		i->cntp = cntp;
		i->disable = disable;
		i->enable = enable;

		s = splhigh();
		LIST_INSERT_HEAD(&alpha_intr_hash[h], i, list);
		splx(s);
	}

	/* Second, create the interrupt thread if needed. */
	ithd = i->ithd;
	if (ithd == NULL || ithd->it_ih == NULL) {
		/* first handler for this vector */
		if (ithd == NULL) {
			ithd = malloc(sizeof(struct ithd), M_DEVBUF,
			    M_WAITOK | M_ZERO);
			if (ithd == NULL)
				return ENOMEM;

			ithd->irq = vector;
			ithd->it_md = i;
			i->ithd = ithd;
		}

		/* Create a kernel thread if needed. */
		if (ithd->it_proc == NULL) {
			errcode = kthread_create(ithd_loop, NULL, &p,
			    RFSTOPPED | RFHIGHPID, "intr: %s", name);
			if (errcode)
				panic(
			    "alpha_setup_intr: Can't create interrupt thread");
			p->p_rtprio.type = RTP_PRIO_ITHREAD;
			p->p_stat = SWAIT;	/* we're idle */

			/* Put in linkages. */
			ithd->it_proc = p;
			p->p_ithd = ithd;
		} else
			snprintf(ithd->it_proc->p_comm, MAXCOMLEN, "intr%03x: %s",
			    vector, name);
		p->p_rtprio.prio = pri;
	} else {
		p = ithd->it_proc;
		if (strlen(p->p_comm) + strlen(name) < MAXCOMLEN) {
			strcat(p->p_comm, " ");
			strcat(p->p_comm, name);
		} else if (strlen(p->p_comm) == MAXCOMLEN)
			p->p_comm[MAXCOMLEN - 1] = '+';
		else
			strcat(p->p_comm, "+");
	}

	/* Third, setup the interrupt descriptor for this handler. */
	idesc = malloc(sizeof (struct intrhand), M_DEVBUF, M_WAITOK | M_ZERO);
	if (idesc == NULL)
		return ENOMEM;

	idesc->ih_handler = handler;
	idesc->ih_argument = arg;
	idesc->ih_name = malloc(strlen(name) + 1, M_DEVBUF, M_WAITOK);
	if (idesc->ih_name == NULL) {
		free(idesc, M_DEVBUF);
		return(NULL);
	}
	strcpy(idesc->ih_name, name);

	/* Fourth, add our handler to the end of the ithread's handler list. */
	head = ithd->it_ih;
	if (head) {
		while (head->ih_next != NULL)
			head = head->ih_next;
		head->ih_next = idesc;
	} else
		ithd->it_ih = idesc;

	*cookiep = idesc;
	return 0;
}

int
alpha_teardown_intr(void *cookie)
{
	struct intrhand *idesc = cookie;
	struct ithd *ithd;
	struct intrhand *head;
#if 0
	struct alpha_intr *i;
	int s;
#endif

	/* First, detach ourself from our interrupt thread. */
	ithd = idesc->ih_ithd;
	KASSERT(ithd != NULL, ("idesc without an interrupt thread"));

	head = ithd->it_ih;
	if (head == idesc)
		ithd->it_ih = idesc->ih_next;
	else {
		while (head != NULL && head->ih_next != idesc)
			head = head->ih_next;
		if (head == NULL)
			return (-1);	/* couldn't find ourself */
		head->ih_next = idesc->ih_next;
	}
	free(idesc, M_DEVBUF);

	/* XXX - if the ithd has no handlers left, we should remove it */

#if 0
	s = splhigh();
	LIST_REMOVE(i, list);
	splx(s);

	free(i, M_DEVBUF);
#endif
	return 0;
}

void
alpha_dispatch_intr(void *frame, unsigned long vector)
{
	int h = HASHVEC(vector);
	struct alpha_intr *i;
	struct ithd *ithd;			/* our interrupt thread */

	/*
	 * Walk the hash bucket for this vector looking for this vector's
	 * interrupt thread.
	 */
	for (i = LIST_FIRST(&alpha_intr_hash[h]); i && i->vector != vector;
	    i = LIST_NEXT(i, list))
		;	/* nothing */
	if (i == NULL)
		return;			/* no ithread for this vector */

	ithd = i->ithd;
	KASSERT(ithd != NULL, ("interrupt vector without a thread"));

	/*
	 * As an optomization until we have kthread_cancel(), if an ithread
	 * has no handlers, don't schedule it to run.
	 */
	if (ithd->it_ih == NULL)
		return;

	atomic_add_long(i->cntp, 1);

	CTR3(KTR_INTR, "sched_ithd pid %d(%s) need=%d",
		ithd->it_proc->p_pid, ithd->it_proc->p_comm, ithd->it_need);

	/*
	 * Set it_need so that if the thread is already running but close
	 * to done, it will do another go-round.  Then get the sched lock
	 * and see if the thread is on whichkqs yet.  If not, put it on
	 * there.  In any case, kick everyone so that if the new thread
	 * is higher priority than their current thread, it gets run now.
	 */
	ithd->it_need = 1;
	mtx_enter(&sched_lock, MTX_SPIN);
	if (ithd->it_proc->p_stat == SWAIT) {
		/* not on the run queue and not running */
		CTR1(KTR_INTR, "alpha_dispatch_intr: setrunqueue %d",
		    ithd->it_proc->p_pid);

		alpha_mb();	/* XXX - ??? */
		ithd->it_proc->p_stat = SRUN;
		setrunqueue(ithd->it_proc);
		aston();
	} else {
		CTR3(KTR_INTR, "alpha_dispatch_intr: %d: it_need %d, state %d",
		    ithd->it_proc->p_pid, ithd->it_need, ithd->it_proc->p_stat);
	}
	if (i->disable)
		i->disable(i->vector);
	mtx_exit(&sched_lock, MTX_SPIN);

	need_resched();
}
 
void
ithd_loop(void *dummy)
{
	struct ithd *ithd;		/* our thread context */
	struct intrhand *ih;		/* list of handlers */
	struct alpha_intr *i;		/* interrupt source */

	ithd = curproc->p_ithd;
	i = ithd->it_md;

	/*
	 * As long as we have interrupts outstanding, go through the
	 * list of handlers, giving each one a go at it.
	 */
	for (;;) {
		CTR3(KTR_INTR, "ithd_loop pid %d(%s) need=%d",
		    ithd->it_proc->p_pid, ithd->it_proc->p_comm, ithd->it_need);
                while (ithd->it_need) {
                        /*
                         * Service interrupts.  If another interrupt
                         * arrives while we are running, they will set
                         * it_need to denote that we should make
                         * another pass.
                         */
                        ithd->it_need = 0;

                        alpha_wmb(); /* push out "it_need=0" */

			for (ih = ithd->it_ih; ih != NULL; ih = ih->ih_next) {
				CTR5(KTR_INTR,
				    "ithd_loop pid %d ih=%p: %p(%p) flg=%x",
				    ithd->it_proc->p_pid, (void *)ih,
				    (void *)ih->ih_handler, ih->ih_argument,
				    ih->ih_flags);

				if ((ih->ih_flags & INTR_MPSAFE) == 0)
					mtx_enter(&Giant, MTX_DEF);
				ih->ih_handler(ih->ih_argument);
				if ((ih->ih_flags & INTR_MPSAFE) == 0)
					mtx_exit(&Giant, MTX_DEF);
			}

			/*
			 * Reenable the source to give it a chance to
			 * set it_need again. 
			 */
			if (i->enable)
				i->enable(i->vector);
		}

		/*
		 * Processed all our interrupts.  Now get the sched
		 * lock.  This may take a while and it_need may get
		 * set again, so we have to check it again.
		 */
		mtx_enter(&sched_lock, MTX_SPIN);
		if (!ithd->it_need) {
			ithd->it_proc->p_stat = SWAIT; /* we're idle */
			CTR1(KTR_INTR, "ithd_loop pid %d: done",
			    ithd->it_proc->p_pid);
			mi_switch();
			CTR1(KTR_INTR, "ithd_loop pid %d: resumed",
			    ithd->it_proc->p_pid);
		}
		mtx_exit(&sched_lock, MTX_SPIN);
	}
}

static void
alpha_clock_interrupt(void *framep)
{

	cnt.v_intr++;
#ifdef EVCNT_COUNTERS
	clock_intr_evcnt.ev_count++;
#else
	intrcnt[INTRCNT_CLOCK]++;
#endif
	if (platform.clockintr){
		(*platform.clockintr)((struct trapframe *)framep);
		/* divide hz (1024) by 8 to get stathz (128) */
		if((++schedclk2 & 0x7) == 0)
			statclock((struct clockframe *)framep);
	}
}
