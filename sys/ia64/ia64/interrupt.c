/* $FreeBSD: src/sys/ia64/ia64/interrupt.c,v 1.61.2.2.2.1 2008/11/25 02:59:29 kensmith Exp $ */
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
#include <sys/syslog.h>

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

static void ia64_dispatch_intr(void *, u_int);

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

void
interrupt(struct trapframe *tf)
{
	struct thread *td;
	volatile struct ia64_interrupt_block *ib = IA64_INTERRUPT_BLOCK;
	uint64_t adj, clk, itc;
	int64_t delta;
	u_int vector;
	int count;
	uint8_t inta;
	ia64_set_fpsr(IA64_FPSR_DEFAULT);

	td = curthread;
	atomic_add_int(&td->td_intr_nesting_level, 1);

	vector = tf->tf_special.ifa;

 next:
	/*
	 * Handle ExtINT interrupts by generating an INTA cycle to
	 * read the vector.
	 */
	if (vector == 0) {
		inta = ib->ib_inta;
		printf("ExtINT interrupt: vector=%u\n", (int)inta);
		if (inta == 15) {
			__asm __volatile("mov cr.eoi = r0;; srlz.d");
			goto stray;
		}
		vector = (int)inta;
	} else if (vector == 15)
		goto stray;

	if (vector == CLOCK_VECTOR) {/* clock interrupt */
		/* CTR0(KTR_INTR, "clock interrupt"); */

		itc = ia64_get_itc();

		PCPU_INC(cnt.v_intr);
#ifdef EVCNT_COUNTERS
		clock_intr_evcnt.ev_count++;
#else
		intrcnt[INTRCNT_CLOCK]++;
#endif
		clks[PCPU_GET(cpuid)]++;

		critical_enter();

		adj = PCPU_GET(clockadj);
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
		ia64_set_itm(itc + ia64_clock_reload - adj);
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
		ia64_srlz_d();

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
		cpumask_t mybit = PCPU_GET(cpumask);

		savectx(PCPU_PTR(pcb));
		atomic_set_int(&stopped_cpus, mybit);
		while ((started_cpus & mybit) == 0)
			cpu_spinwait();
		atomic_clear_int(&started_cpus, mybit);
		atomic_clear_int(&stopped_cpus, mybit);
	} else if (vector == ipi_vector[IPI_TEST]) {
		CTR1(KTR_SMP, "IPI_TEST, cpuid=%d", PCPU_GET(cpuid));
		mp_ipi_test++;
#endif
	} else {
		ints[PCPU_GET(cpuid)]++;
		ia64_dispatch_intr(tf, vector);
	}

	__asm __volatile("mov cr.eoi = r0;; srlz.d");
	vector = ia64_get_ivr();
	if (vector != 15)
		goto next;

stray:
	atomic_subtract_int(&td->td_intr_nesting_level, 1);

	if (TRAPF_USERMODE(tf)) {
		enable_intr();
		userret(td, tf);
		mtx_assert(&Giant, MA_NOTOWNED);
		do_ast(tf);
	}
}

/*
 * Hardware irqs have vectors starting at this offset.
 */
#define IA64_HARDWARE_IRQ_BASE	0x20

struct ia64_intr {
	struct intr_event *event;	/* interrupt event */
	volatile long *cntp;		/* interrupt counter */
	struct sapic *sapic;
	u_int	irq;
};

static struct ia64_intr *ia64_intrs[256];

static void
ia64_intr_eoi(void *arg)
{
	u_int vector = (uintptr_t)arg;
	struct ia64_intr *i;

	i = ia64_intrs[vector];
	if (i != NULL)
		sapic_eoi(i->sapic, vector);
}

static void
ia64_intr_mask(void *arg)
{
	u_int vector = (uintptr_t)arg;
	struct ia64_intr *i;

	i = ia64_intrs[vector];
	if (i != NULL) {
		sapic_mask(i->sapic, i->irq);
		sapic_eoi(i->sapic, vector);
	}
}

static void
ia64_intr_unmask(void *arg)
{
	u_int vector = (uintptr_t)arg;
	struct ia64_intr *i;

	i = ia64_intrs[vector];
	if (i != NULL)
		sapic_unmask(i->sapic, i->irq);
}

int
ia64_setup_intr(const char *name, int irq, driver_filter_t filter,
    driver_intr_t handler, void *arg, enum intr_type flags, void **cookiep)
{
	struct ia64_intr *i;
	struct sapic *sa;
	char *intrname;
	u_int vector;
	int error;

	/* Get the I/O SAPIC that corresponds to the IRQ. */
	sa = sapic_lookup(irq);
	if (sa == NULL)
		return (EINVAL);

	/*
	 * XXX - There's a priority implied by the choice of vector.
	 * We should therefore relate the vector to the interrupt type.
	 */
	vector = irq + IA64_HARDWARE_IRQ_BASE;

	i = ia64_intrs[vector];
	if (i == NULL) {
		i = malloc(sizeof(struct ia64_intr), M_DEVBUF, M_NOWAIT);
		if (i == NULL)
			return (ENOMEM);

		error = intr_event_create(&i->event, (void *)(uintptr_t)vector,
		    0, ia64_intr_unmask,
#ifdef INTR_FILTER
		    ia64_intr_eoi, ia64_intr_mask,
#endif
		    NULL, "irq%u:", irq);
		if (error) {
			free(i, M_DEVBUF);
			return (error);
		}

		if (!atomic_cmpset_ptr(&ia64_intrs[vector], NULL, i)) {
			intr_event_destroy(i->event);
			free(i, M_DEVBUF);
			i = ia64_intrs[vector];
		} else {
			i->sapic = sa;
			i->irq = irq;

			i->cntp = intrcnt + irq + INTRCNT_ISA_IRQ;
			if (name != NULL && *name != '\0') {
				/* XXX needs abstraction. Too error prone. */
				intrname = intrnames +
				    (irq + INTRCNT_ISA_IRQ) * INTRNAME_LEN;
				memset(intrname, ' ', INTRNAME_LEN - 1);
				bcopy(name, intrname, strlen(name));
			}

			sapic_enable(i->sapic, irq, vector);
		}
	}

	error = intr_event_add_handler(i->event, name, filter, handler, arg,
	    intr_priority(flags), flags, cookiep);
	return (error);
}

int
ia64_teardown_intr(void *cookie)
{

	return (intr_event_remove_handler(cookie));
}

static void
ia64_dispatch_intr(void *frame, u_int vector)
{
	struct ia64_intr *i;
	struct intr_event *ie;			/* our interrupt event */
#ifndef INTR_FILTER
	struct intr_handler *ih;
	int error, thread, ret;
#endif

	/*
	 * Find the interrupt thread for this vector.
	 */
	i = ia64_intrs[vector];
	KASSERT(i != NULL, ("%s: unassigned vector", __func__));

	(*i->cntp)++;

	ie = i->event;
	KASSERT(ie != NULL, ("%s: interrupt without event", __func__));

#ifdef INTR_FILTER
	if (intr_event_handle(ie, frame) != 0) {
		ia64_intr_mask((void *)(uintptr_t)vector);
		log(LOG_ERR, "stray irq%u\n", i->irq);
	}
#else
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

	if (thread) {
		ia64_intr_mask((void *)(uintptr_t)vector);
		error = intr_event_schedule_thread(ie);
		KASSERT(error == 0, ("%s: impossible stray", __func__));
	} else
		ia64_intr_eoi((void *)(uintptr_t)vector);
	critical_exit();
#endif
}

#ifdef DDB

static void
db_print_vector(u_int vector, int always)
{
	struct ia64_intr *i;

	i = ia64_intrs[vector];
	if (i != NULL) {
		db_printf("vector %u (%p): ", vector, i);
		sapic_print(i->sapic, i->irq);
	} else if (always)
		db_printf("vector %u: unassigned\n", vector);
}

DB_SHOW_COMMAND(vector, db_show_vector)
{
	u_int vector;

	if (have_addr) {
		vector = ((addr >> 4) % 16) * 10 + (addr % 16);
		if (vector >= 256)
			db_printf("error: vector %u not in range [0..255]\n",
			    vector);
		else
			db_print_vector(vector, 1);
	} else {
		for (vector = 0; vector < 256; vector++)
			db_print_vector(vector, 0);
	}
}

#endif
