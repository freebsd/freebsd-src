/* $FreeBSD$ */
/* $NetBSD: interrupt.c,v 1.23 1998/02/24 07:38:01 thorpej Exp $ */

/*-
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
/*-
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
#include <machine/fpu.h>
#include <machine/frame.h>
#include <machine/intr.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/reg.h>
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
dummy_perf(unsigned long vector, struct trapframe *tf)  
{
	printf("performance interrupt!\n");
}

void (*perf_irq)(unsigned long, struct trapframe *) = dummy_perf;

static unsigned int ints[MAXCPU];
SYSCTL_OPAQUE(_debug, OID_AUTO, ints, CTLFLAG_RW, &ints, sizeof(ints), "IU",
    "");

static unsigned int clks[MAXCPU];
#ifdef SMP
SYSCTL_OPAQUE(_debug, OID_AUTO, clks, CTLFLAG_RW, &clks, sizeof(clks), "IU",
    "");
#else
SYSCTL_INT(_debug, OID_AUTO, clks, CTLFLAG_RW, clks, 0, "");
#endif

#ifdef SMP
static unsigned int asts[MAXCPU];
SYSCTL_OPAQUE(_debug, OID_AUTO, asts, CTLFLAG_RW, &asts, sizeof(asts), "IU",
    "");

static unsigned int rdvs[MAXCPU];
SYSCTL_OPAQUE(_debug, OID_AUTO, rdvs, CTLFLAG_RW, &rdvs, sizeof(rdvs), "IU",
    "");
#endif

SYSCTL_NODE(_debug, OID_AUTO, clock, CTLFLAG_RW, 0, "clock statistics");

static int adjust_edges = 0;
SYSCTL_INT(_debug_clock, OID_AUTO, adjust_edges, CTLFLAG_RD,
    &adjust_edges, 0, "Number of times ITC got more than 12.5% behind");

static int adjust_excess = 0;
SYSCTL_INT(_debug_clock, OID_AUTO, adjust_excess, CTLFLAG_RD,
    &adjust_excess, 0, "Total number of ignored ITC interrupts");

static int adjust_lost = 0;
SYSCTL_INT(_debug_clock, OID_AUTO, adjust_lost, CTLFLAG_RD,
    &adjust_lost, 0, "Total number of lost ITC interrupts");

static int adjust_ticks = 0;
SYSCTL_INT(_debug_clock, OID_AUTO, adjust_ticks, CTLFLAG_RD,
    &adjust_ticks, 0, "Total number of ITC interrupts with adjustment");

int
interrupt(u_int64_t vector, struct trapframe *tf)
{
	struct thread *td;
	volatile struct ia64_interrupt_block *ib = IA64_INTERRUPT_BLOCK;
	uint64_t adj, clk, itc;
	int64_t delta;
	int count;

	ia64_set_fpsr(IA64_FPSR_DEFAULT);

	td = curthread;
	atomic_add_int(&td->td_intr_nesting_level, 1);

	/*
	 * Handle ExtINT interrupts by generating an INTA cycle to
	 * read the vector.
	 */
	if (vector == 0) {
		vector = ib->ib_inta;
		printf("ExtINT interrupt: vector=%ld\n", vector);
		if (vector == 15)
			goto stray;
	}

	if (vector == CLOCK_VECTOR) {/* clock interrupt */
		/* CTR0(KTR_INTR, "clock interrupt"); */

		PCPU_LAZY_INC(cnt.v_intr);
#ifdef EVCNT_COUNTERS
		clock_intr_evcnt.ev_count++;
#else
		intrcnt[INTRCNT_CLOCK]++;
#endif
		clks[PCPU_GET(cpuid)]++;

		critical_enter();

		adj = PCPU_GET(clockadj);
		itc = ia64_get_itc();
		ia64_set_itm(itc + ia64_clock_reload - adj);
		clk = PCPU_GET(clock);
		delta = itc - clk;
		count = 0;
		while (delta >= ia64_clock_reload) {
			/* Only the BSP runs the real clock */
			if (PCPU_GET(cpuid) == 0)
				hardclock(TRAPF_USERMODE(tf), TRAPF_PC(tf));
			else
				hardclock_cpu(TRAPF_USERMODE(tf));
			if (profprocs != 0)
				profclock(TRAPF_USERMODE(tf), TRAPF_PC(tf));
			statclock(TRAPF_USERMODE(tf));
			delta -= ia64_clock_reload;
			clk += ia64_clock_reload;
			if (adj != 0)
				adjust_ticks++;
			count++;
		}
		if (count > 0) {
			adjust_lost += count - 1;
			if (delta > (ia64_clock_reload >> 3)) {
				if (adj == 0)
					adjust_edges++;
				adj = ia64_clock_reload >> 4;
			} else
				adj = 0;
		} else {
			adj = 0;
			adjust_excess++;
		}
		PCPU_SET(clock, clk);
		PCPU_SET(clockadj, adj);

		critical_exit();

#ifdef SMP
	} else if (vector == ipi_vector[IPI_AST]) {
		asts[PCPU_GET(cpuid)]++;
		CTR1(KTR_SMP, "IPI_AST, cpuid=%d", PCPU_GET(cpuid));
	} else if (vector == ipi_vector[IPI_HIGH_FP]) {
		struct thread *thr = PCPU_GET(fpcurthread);
		if (thr != NULL) {
			mtx_lock_spin(&thr->td_md.md_highfp_mtx);
			save_high_fp(&thr->td_pcb->pcb_high_fp);
			thr->td_pcb->pcb_fpcpu = NULL;
			PCPU_SET(fpcurthread, NULL);
			mtx_unlock_spin(&thr->td_md.md_highfp_mtx);
		}
	} else if (vector == ipi_vector[IPI_RENDEZVOUS]) {
		rdvs[PCPU_GET(cpuid)]++;
		CTR1(KTR_SMP, "IPI_RENDEZVOUS, cpuid=%d", PCPU_GET(cpuid));
		smp_rendezvous_action();
	} else if (vector == ipi_vector[IPI_STOP]) {
		register_t intr;
		cpumask_t mybit = PCPU_GET(cpumask);

		intr = intr_disable();
		savectx(PCPU_PTR(pcb));
		atomic_set_int(&stopped_cpus, mybit);
		while ((started_cpus & mybit) == 0)
			/* spin */;
		atomic_clear_int(&started_cpus, mybit);
		atomic_clear_int(&stopped_cpus, mybit);
		intr_restore(intr);
	} else if (vector == ipi_vector[IPI_TEST]) {
		CTR1(KTR_SMP, "IPI_TEST, cpuid=%d", PCPU_GET(cpuid));
		mp_ipi_test++;
#endif
	} else {
		ints[PCPU_GET(cpuid)]++;
		ia64_dispatch_intr(tf, vector);
	}

stray:
	atomic_subtract_int(&td->td_intr_nesting_level, 1);
	return (TRAPF_USERMODE(tf));
}

/*
 * Hardware irqs have vectors starting at this offset.
 */
#define IA64_HARDWARE_IRQ_BASE	0x20

struct ia64_intr {
    struct intr_event	*event; /* interrupt event */
    volatile long	*cntp;  /* interrupt counter */
};

static struct mtx ia64_intrs_lock;
static struct ia64_intr *ia64_intrs[256];

extern struct sapic *ia64_sapics[];
extern int ia64_sapic_count;

static void
ithds_init(void *dummy)
{

	mtx_init(&ia64_intrs_lock, "intr table", NULL, MTX_SPIN);
}
SYSINIT(ithds_init, SI_SUB_INTR, SI_ORDER_SECOND, ithds_init, NULL);

static void
ia64_send_eoi(uintptr_t vector)
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
ia64_setup_intr(const char *name, int irq, driver_filter_t filter, 
		driver_intr_t handler, void *arg, enum intr_type flags, 
		void **cookiep, volatile long *cntp)		
{
	struct ia64_intr *i;
	int errcode;
	intptr_t vector = irq + IA64_HARDWARE_IRQ_BASE;
	char *intrname;

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
		if (cntp == NULL)
			i->cntp = intrcnt + irq + INTRCNT_ISA_IRQ;
		else
			i->cntp = cntp;
		if (name != NULL && *name != '\0') {
			/* XXX needs abstraction. Too error phrone. */
			intrname = intrnames + (irq + INTRCNT_ISA_IRQ) *
			    INTRNAME_LEN;
			memset(intrname, ' ', INTRNAME_LEN - 1);
			bcopy(name, intrname, strlen(name));
		}
		errcode = intr_event_create(&i->event, (void *)vector, 0,
		    (void (*)(void *))ia64_send_eoi, "intr:");
		if (errcode) {
			free(i, M_DEVBUF);
			return errcode;
		}

		mtx_lock_spin(&ia64_intrs_lock);
		ia64_intrs[vector] = i;
		mtx_unlock_spin(&ia64_intrs_lock);
	}

	/* Second, add this handler. */
	errcode = intr_event_add_handler(i->event, name, filter, handler, arg,
	    intr_priority(flags), flags, cookiep);
	if (errcode)
		return errcode;

	return (sapic_enable(irq, vector));
}

int
ia64_teardown_intr(void *cookie)
{

	return (intr_event_remove_handler(cookie));
}

void
ia64_dispatch_intr(void *frame, unsigned long vector)
{
	struct ia64_intr *i;
	struct intr_event *ie;			/* our interrupt event */
	struct intr_handler *ih;
	int error, thread, ret;

	/*
	 * Find the interrupt thread for this vector.
	 */
	i = ia64_intrs[vector];
	if (i == NULL)
		return;			/* no event for this vector */

	if (i->cntp)
		atomic_add_long(i->cntp, 1);

	ie = i->event;
	KASSERT(ie != NULL, ("interrupt vector without an event"));

	/*
	 * As an optimization, if an event has no handlers, don't
	 * schedule it to run.
	 */
	if (TAILQ_EMPTY(&ie->ie_handlers))
		return;

	/*
	 * Execute all fast interrupt handlers directly without Giant.  Note
	 * that this means that any fast interrupt handler must be MP safe.
	 */
	ret = 0;
	thread = 0;
	critical_enter();
	TAILQ_FOREACH(ih, &ie->ie_handlers, ih_next) {
		if (ih->ih_filter == NULL) {
			thread = 1;
			continue;
		}
		CTR4(KTR_INTR, "%s: exec %p(%p) for %s", __func__,
		    ih->ih_filter, ih->ih_argument, ih->ih_name);
		ret = ih->ih_filter(ih->ih_argument);
		/*
		 * Wrapper handler special case: see
		 * i386/intr_machdep.c::intr_execute_handlers()
		 */
		if (!thread) {
			if (ret == FILTER_SCHEDULE_THREAD)
				thread = 1;
		}
	}
	critical_exit();

	if (thread) {
		error = intr_event_schedule_thread(ie);
		KASSERT(error == 0, ("got an impossible stray interrupt"));
	} else
		ia64_send_eoi(vector);
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
