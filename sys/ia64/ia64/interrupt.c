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

#include <machine/reg.h>
#include <machine/frame.h>
#include <machine/intr.h>
#include <machine/mutex.h>

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
interrupt(a0, a1, a2, framep)
	unsigned long a0, a1, a2;
	struct trapframe *framep;
{
#if 0
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
		cnt.v_intr++;
#ifdef EVCNT_COUNTERS
		clock_intr_evcnt.ev_count++;
#else
		intrcnt[INTRCNT_CLOCK]++;
#endif
		if (platform.clockintr){
			(*platform.clockintr)(framep);
			/* divide hz (1024) by 8 to get stathz (128) */
			if((++schedclk2 & 0x7) == 0)
				statclock((struct clockframe *)framep);
		}
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
#endif
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
