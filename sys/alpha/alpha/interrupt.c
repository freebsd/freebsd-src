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
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/ktr.h>
#include <sys/mutex.h>
#include <sys/smp.h>
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
static void alpha_clock_interrupt(struct trapframe *framep);

void
interrupt(a0, a1, a2, framep)
	unsigned long a0, a1, a2;
	struct trapframe *framep;
{
	struct proc *p;
#ifdef SMP
	critical_t s;
#endif

	/*
	 * Find our per-cpu globals.
	 */
#ifdef SMP
	s = critical_enter();
#endif
	globalp = (struct globaldata *) alpha_pal_rdval();
	p = curproc;
#ifdef SMP
	p->p_md.md_kernnest++;
	critical_exit(s);
#endif
	atomic_add_int(&p->p_intr_nesting_level, 1);
#ifndef SMP
	{
		if ((caddr_t) framep < (caddr_t) p->p_addr + 1024) {
			panic("possible stack overflow\n");
		}
	}
#endif

	framep->tf_regs[FRAME_TRAPARG_A0] = a0;
	framep->tf_regs[FRAME_TRAPARG_A1] = a1;
	framep->tf_regs[FRAME_TRAPARG_A2] = a2;
	switch (a0) {
#ifdef SMP
	case ALPHA_INTR_XPROC:	/* interprocessor interrupt */
		CTR0(KTR_INTR|KTR_SMP, "interprocessor interrupt");
		smp_handle_ipi(framep); /* note: lock not taken */
		break;
#endif
		
	case ALPHA_INTR_CLOCK:	/* clock interrupt */
		CTR0(KTR_INTR, "clock interrupt");
		alpha_clock_interrupt(framep);
		break;

	case  ALPHA_INTR_ERROR:	/* Machine Check or Correctable Error */
		a0 = alpha_pal_rdmces();
		if (platform.mcheck_handler)
			(*platform.mcheck_handler)(a0, framep, a1, a2);
		else
			machine_check(a0, framep, a1, a2);
		break;

	case ALPHA_INTR_DEVICE:	/* I/O device interrupt */
		cnt.v_intr++;
		if (platform.iointr)
			(*platform.iointr)(framep, a1);
		break;

	case ALPHA_INTR_PERF:	/* interprocessor interrupt */
		perf_irq(a1, framep);
		break;

	case ALPHA_INTR_PASSIVE:
#if	0
		printf("passive release interrupt vec 0x%lx (ignoring)\n", a1);
#endif
		break;

	default:
		panic("unexpected interrupt: type 0x%lx vec 0x%lx a2 0x%lx\n",
		    a0, a1, a2);
		/* NOTREACHED */
	}
	atomic_subtract_int(&p->p_intr_nesting_level, 1);
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
	alpha_mb(); /* magic for ev5 2100A  & maybe more */

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
};

static struct mtx alpha_intr_hash_lock;
static struct alpha_intr_list alpha_intr_hash[31];

static void	ithds_init(void *dummy);

static void
ithds_init(void *dummy)
{

	mtx_init(&alpha_intr_hash_lock, "ithread table lock", MTX_SPIN);
}
SYSINIT(ithds_init, SI_SUB_INTR, SI_ORDER_SECOND, ithds_init, NULL);

int
alpha_setup_intr(const char *name, int vector, driver_intr_t handler, void *arg,
		 enum intr_type flags, void **cookiep, volatile long *cntp,
    		 void (*disable)(int), void (*enable)(int))
{
	int h = HASHVEC(vector);
	struct alpha_intr *i;
	int errcode;

	/*
	 * XXX - Can we have more than one device on a vector?  If so, we have
	 * a race condition here that needs to be worked around similar to
	 * the fashion done in the i386 inthand_add() function.
	 */
	
	/* First, check for an existing hash table entry for this vector. */
	mtx_lock_spin(&alpha_intr_hash_lock);
	for (i = LIST_FIRST(&alpha_intr_hash[h]); i && i->vector != vector;
	    i = LIST_NEXT(i, list))
		;	/* nothing */
	mtx_unlock_spin(&alpha_intr_hash_lock);

	if (i == NULL) {
		/* None was found, so create an entry. */
		i = malloc(sizeof(struct alpha_intr), M_DEVBUF, M_NOWAIT);
		if (i == NULL)
			return ENOMEM;
		i->vector = vector;
		i->cntp = cntp;
		errcode = ithread_create(&i->ithd, vector, 0, disable, enable,
		    "intr:");
		if (errcode) {
			free(i, M_DEVBUF);
			return errcode;
		}

		mtx_lock_spin(&alpha_intr_hash_lock);
		LIST_INSERT_HEAD(&alpha_intr_hash[h], i, list);
		mtx_unlock_spin(&alpha_intr_hash_lock);
	}

	/* Second, add this handler. */
	return (ithread_add_handler(i->ithd, name, handler, arg,
	    ithread_priority(flags), flags, cookiep));
}

int
alpha_teardown_intr(void *cookie)
{

	return (ithread_remove_handler(cookie));
}

void
alpha_dispatch_intr(void *frame, unsigned long vector)
{
	int h = HASHVEC(vector);
	struct alpha_intr *i;
	struct ithd *ithd;			/* our interrupt thread */
	struct intrhand *ih;
	int error;

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
	 * As an optomization, if an ithread has no handlers, don't
	 * schedule it to run.
	 */
	if (TAILQ_EMPTY(&ithd->it_handlers))
		return;

	atomic_add_long(i->cntp, 1);

	/*
	 * Handle a fast interrupt if there is no actual thread for this
	 * interrupt by calling the handler directly without Giant.  Note
	 * that this means that any fast interrupt handler must be MP safe.
	 */
	ih = TAILQ_FIRST(&ithd->it_handlers);
	if ((ih->ih_flags & INTR_FAST) != 0) {
		ih->ih_handler(ih->ih_argument);
		return;
	}

	if (ithd->it_disable) {
		CTR1(KTR_INTR,
		    "alpha_dispatch_intr: disabling vector 0x%x", i->vector);
		ithd->it_disable(ithd->it_vector);
	}
	error = ithread_schedule(ithd, !cold);
	KASSERT(error == 0, ("got an impossible stray interrupt"));
}

static void
alpha_clock_interrupt(struct trapframe *framep)
{

	cnt.v_intr++;
#ifdef EVCNT_COUNTERS
	clock_intr_evcnt.ev_count++;
#else
	intrcnt[INTRCNT_CLOCK]++;
#endif
	if (platform.clockintr) {
#ifdef SMP
		/*
		 * Only one processor drives the actual timer.
		 */
		if (PCPU_GET(cpuid) == boot_cpu_id) {
#endif
			(*platform.clockintr)(framep);
			/* divide hz (1024) by 8 to get stathz (128) */
			if ((++schedclk2 & 0x7) == 0)
				statclock((struct clockframe *)framep);
#ifdef SMP
		} else {
			mtx_lock_spin(&sched_lock);
			hardclock_process(curproc, TRAPF_USERMODE(framep));
			if ((schedclk2 & 0x7) == 0)
				statclock_process(curproc, TRAPF_PC(framep),
				    TRAPF_USERMODE(framep));
			mtx_unlock_spin(&sched_lock);
		}
#endif
	}
}
