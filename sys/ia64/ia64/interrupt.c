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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vmmeter.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include <machine/intr.h>
#include <machine/pcb.h>
#include <machine/sapicvar.h>
#include <machine/smp.h>

#ifdef EVCNT_COUNTERS
struct evcnt clock_intr_evcnt;	/* event counter for clock intrs. */
#else
#include <sys/interrupt.h>
#include <machine/intrcnt.h>
#endif

#ifdef DDB
#include <ddb/ddb.h>
#endif

#ifdef SMP
extern int mp_ipi_test;
#endif

volatile int mc_expected, mc_received;

static void 
dummy_perf(unsigned long vector, struct trapframe *framep)  
{
	printf("performance interrupt!\n");
}

void (*perf_irq)(unsigned long, struct trapframe *) = dummy_perf;

static unsigned int ints[MAXCPU];
static unsigned int clks[MAXCPU];
static unsigned int asts[MAXCPU];
static unsigned int rdvs[MAXCPU];
SYSCTL_OPAQUE(_debug, OID_AUTO, ints, CTLFLAG_RW, &ints, sizeof(ints), "IU","");
SYSCTL_OPAQUE(_debug, OID_AUTO, clks, CTLFLAG_RW, &clks, sizeof(clks), "IU","");
SYSCTL_OPAQUE(_debug, OID_AUTO, asts, CTLFLAG_RW, &asts, sizeof(asts), "IU","");
SYSCTL_OPAQUE(_debug, OID_AUTO, rdvs, CTLFLAG_RW, &rdvs, sizeof(rdvs), "IU","");

static u_int schedclk2;

void
interrupt(u_int64_t vector, struct trapframe *framep)
{
	struct thread *td;
	volatile struct ia64_interrupt_block *ib = IA64_INTERRUPT_BLOCK;

	td = curthread;
	atomic_add_int(&td->td_intr_nesting_level, 1);

	/*
	 * Handle ExtINT interrupts by generating an INTA cycle to
	 * read the vector.
	 */
	if (vector == 0) {
		vector = ib->ib_inta;
		printf("ExtINT interrupt: vector=%ld\n", vector);
		goto out;	/* XXX */
	}

	if (vector == 255) {/* clock interrupt */
		/* CTR0(KTR_INTR, "clock interrupt"); */
			
		cnt.v_intr++;
#ifdef EVCNT_COUNTERS
		clock_intr_evcnt.ev_count++;
#else
		intrcnt[INTRCNT_CLOCK]++;
#endif
		critical_enter();
#ifdef SMP
		clks[PCPU_GET(cpuid)]++;
		/* Only the BSP runs the real clock */
		if (PCPU_GET(cpuid) == 0) {
#endif
			handleclock(framep);
			/* divide hz (1024) by 8 to get stathz (128) */
			if ((++schedclk2 & 0x7) == 0)
				statclock((struct clockframe *)framep);
#ifdef SMP
		} else {
			ia64_set_itm(ia64_get_itc() + itm_reload);
			mtx_lock_spin(&sched_lock);
			hardclock_process(curthread, TRAPF_USERMODE(framep));
			if ((schedclk2 & 0x7) == 0)
				statclock_process(curkse, TRAPF_PC(framep),
				    TRAPF_USERMODE(framep));
			mtx_unlock_spin(&sched_lock);
		}
#endif
		critical_exit();
#ifdef SMP
	} else if (vector == ipi_vector[IPI_AST]) {
		asts[PCPU_GET(cpuid)]++;
		CTR1(KTR_SMP, "IPI_AST, cpuid=%d", PCPU_GET(cpuid));
	} else if (vector == ipi_vector[IPI_RENDEZVOUS]) {
		rdvs[PCPU_GET(cpuid)]++;
		CTR1(KTR_SMP, "IPI_RENDEZVOUS, cpuid=%d", PCPU_GET(cpuid));
		smp_rendezvous_action();
	} else if (vector == ipi_vector[IPI_STOP]) {
		u_int32_t mybit = PCPU_GET(cpumask);

		CTR1(KTR_SMP, "IPI_STOP, cpuid=%d", PCPU_GET(cpuid));
		savectx(PCPU_GET(pcb));
		stopped_cpus |= mybit;
		while ((started_cpus & mybit) == 0)
			/* spin */;
		started_cpus &= ~mybit;
		stopped_cpus &= ~mybit;
		if (PCPU_GET(cpuid) == 0 && cpustop_restartfunc != NULL) {
			void (*f)(void) = cpustop_restartfunc;
			cpustop_restartfunc = NULL;
			(*f)();
		}
	} else if (vector == ipi_vector[IPI_TEST]) {
		CTR1(KTR_SMP, "IPI_TEST, cpuid=%d", PCPU_GET(cpuid));
		mp_ipi_test++;
#endif
	} else {
		ints[PCPU_GET(cpuid)]++;
		ia64_dispatch_intr(framep, vector);
	}

 out:
	atomic_subtract_int(&td->td_intr_nesting_level, 1);
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
	return (1);		/* XXX implement */
}

/*
 * Hardware irqs have vectors starting at this offset.
 */
#define IA64_HARDWARE_IRQ_BASE	0x20

struct ia64_intr {
    struct ithd		*ithd;  /* interrupt thread */
    volatile long	*cntp;  /* interrupt counter */
};

static struct sapic *ia64_sapics[16]; /* XXX make this resizable */
static int ia64_sapic_count;
static struct mtx ia64_intrs_lock;
static struct ia64_intr *ia64_intrs[256];

static void
ithds_init(void *dummy)
{

	mtx_init(&ia64_intrs_lock, "ithread table lock", NULL, MTX_SPIN);
}
SYSINIT(ithds_init, SI_SUB_INTR, SI_ORDER_SECOND, ithds_init, NULL);

void
ia64_add_sapic(struct sapic *sa)
{

	ia64_sapics[ia64_sapic_count++] = sa;
}

static void
ia64_enable(int vector)
{
	int irq, i;

	irq = vector - IA64_HARDWARE_IRQ_BASE;
	for (i = 0; i < ia64_sapic_count; i++) {
		struct sapic *sa = ia64_sapics[i];
		if (irq >= sa->sa_base && irq <= sa->sa_limit)
			sapic_enable(sa, irq - sa->sa_base, vector,
				     (irq < 16
				      ? SAPIC_TRIGGER_EDGE
				      : SAPIC_TRIGGER_LEVEL),
				     (irq < 16
				      ? SAPIC_POLARITY_HIGH
				      : SAPIC_POLARITY_LOW));
	}
}

static void
ia64_send_eoi(int vector)
{
	int irq, i;

	irq = vector - IA64_HARDWARE_IRQ_BASE;
	for (i = 0; i < ia64_sapic_count; i++) {
		struct sapic *sa = ia64_sapics[i];
		if (irq >= sa->sa_base && irq <= sa->sa_limit)
			sapic_eoi(sa, vector);
	}
}

int
ia64_setup_intr(const char *name, int irq, driver_intr_t handler, void *arg,
		enum intr_type flags, void **cookiep, volatile long *cntp)
{
	struct ia64_intr *i;
	int errcode;
	int vector = irq + IA64_HARDWARE_IRQ_BASE;

	/*
	 * XXX - Can we have more than one device on a vector?  If so, we have
	 * a race condition here that needs to be worked around similar to
	 * the fashion done in the i386 inthand_add() function.
	 */
	
	/* First, check for an existing hash table entry for this vector. */
	mtx_lock_spin(&ia64_intrs_lock);
	i = ia64_intrs[vector];
	mtx_unlock_spin(&ia64_intrs_lock);

	if (i == NULL) {
		/* None was found, so create an entry. */
		i = malloc(sizeof(struct ia64_intr), M_DEVBUF, M_NOWAIT);
		if (i == NULL)
			return ENOMEM;
		i->cntp = cntp;
		errcode = ithread_create(&i->ithd, vector, 0, 0,
					 ia64_send_eoi, "intr:");
		if (errcode) {
			free(i, M_DEVBUF);
			return errcode;
		}

		mtx_lock_spin(&ia64_intrs_lock);
		ia64_intrs[vector] = i;
		mtx_unlock_spin(&ia64_intrs_lock);
	}

	/* Second, add this handler. */
	errcode = ithread_add_handler(i->ithd, name, handler, arg,
	    ithread_priority(flags), flags, cookiep);
	if (errcode)
		return errcode;

	ia64_enable(vector);
	return 0;
}

int
ia64_teardown_intr(void *cookie)
{

	return (ithread_remove_handler(cookie));
}

void
ia64_dispatch_intr(void *frame, unsigned long vector)
{
	struct ia64_intr *i;
	struct ithd *ithd;			/* our interrupt thread */
	struct intrhand *ih;
	int error;

	/*
	 * Find the interrupt thread for this vector.
	 */
	i = ia64_intrs[vector];
	if (i == NULL)
		return;			/* no ithread for this vector */

	ithd = i->ithd;
	KASSERT(ithd != NULL, ("interrupt vector without a thread"));

	/*
	 * As an optimization, if an ithread has no handlers, don't
	 * schedule it to run.
	 */
	if (TAILQ_EMPTY(&ithd->it_handlers))
		return;

	if (i->cntp)
		atomic_add_long(i->cntp, 1);

	/*
	 * Handle a fast interrupt if there is no actual thread for this
	 * interrupt by calling the handler directly without Giant.  Note
	 * that this means that any fast interrupt handler must be MP safe.
	 */
	ih = TAILQ_FIRST(&ithd->it_handlers);
	if ((ih->ih_flags & IH_FAST) != 0) {
		critical_enter();
		ih->ih_handler(ih->ih_argument);
		ia64_send_eoi(vector);
		critical_exit();
		return;
	}

	error = ithread_schedule(ithd, 0);	/* XXX:no preemption for now */
	KASSERT(error == 0, ("got an impossible stray interrupt"));
}

#ifdef DDB

static void
db_show_vector(int vector)
{
	int irq, i;

	irq = vector - IA64_HARDWARE_IRQ_BASE;
	for (i = 0; i < ia64_sapic_count; i++) {
		struct sapic *sa = ia64_sapics[i];
		if (irq >= sa->sa_base && irq <= sa->sa_limit)
			sapic_print(sa, irq - sa->sa_base);
	}
}

DB_SHOW_COMMAND(irq, db_show_irq)
{
	int vector;

	if (have_addr) {
		vector = ((addr >> 4) % 16) * 10 + (addr % 16);
		db_show_vector(vector);
	} else {
		for (vector = IA64_HARDWARE_IRQ_BASE;
		     vector < IA64_HARDWARE_IRQ_BASE + 64; vector++)
			db_show_vector(vector);
	}
}

#endif
