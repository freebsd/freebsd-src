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
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/clock.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include <machine/intr.h>

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

void
interrupt(u_int64_t vector, struct trapframe *framep)
{
	struct proc *p;

	p = curproc;
	atomic_add_int(&p->p_intr_nesting_level, 1);
	switch (vector) {
	case 240:	/* clock interrupt */
		CTR0(KTR_INTR, "clock interrupt");
			
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
		break;

	default:
		mtx_lock(&Giant);
		panic("unexpected interrupt: vec %ld\n", vector);
		/* NOTREACHED */
	}
	atomic_subtract_int(&p->p_intr_nesting_level, 1);
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
