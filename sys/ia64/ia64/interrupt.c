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
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vmmeter.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/clock.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include <machine/intr.h>
#include <machine/sapicvar.h>

#ifdef EVCNT_COUNTERS
struct evcnt clock_intr_evcnt;	/* event counter for clock intrs. */
#else
#include <sys/interrupt.h>
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
		handleclock(framep);

		/* divide hz (1024) by 8 to get stathz (128) */
		if((++schedclk2 & 0x7) == 0)
			statclock((struct clockframe *)framep);
	} else {
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

static void	ithds_init(void *dummy);

static void
ithds_init(void *dummy)
{

	mtx_init(&ia64_intrs_lock, "ithread table lock", MTX_SPIN);
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
	 * As an optomization, if an ithread has no handlers, don't
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
		ih->ih_handler(ih->ih_argument);
		ia64_send_eoi(vector);
		return;
	}

	error = ithread_schedule(ithd, 0);	/* XXX:no preemption for now */
	KASSERT(error == 0, ("got an impossible stray interrupt"));
}

