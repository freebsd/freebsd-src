/* $Id: interrupt.c,v 1.2 1998/06/10 20:13:32 dfr Exp $ */
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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

/* __KERNEL_RCSID(0, "$NetBSD: interrupt.c,v 1.23 1998/02/24 07:38:01 thorpej Exp $");*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vmmeter.h>

#include <machine/reg.h>
#include <machine/frame.h>
#include <machine/cpuconf.h>
#include <machine/bwx.h>

#if 0
#ifdef EVCNT_COUNTERS
#include <sys/device.h>
struct evcnt clock_intr_evcnt;	/* event counter for clock intrs. */
#else
#include <machine/intrcnt.h>
#endif
#endif

volatile int mc_expected, mc_received;

void
interrupt(a0, a1, a2, framep)
	unsigned long a0, a1, a2;
	struct trapframe *framep;
{

	{
		struct proc* p = curproc;
		if (!p) p = &proc0;
		if ((caddr_t) framep < (caddr_t) p->p_addr + 1024)
			printf("possible stack overflow\n");
	}

	switch (a0) {
	case ALPHA_INTR_XPROC:	/* interprocessor interrupt */
		printf("interprocessor interrupt!\n");
		break;
		
	case ALPHA_INTR_CLOCK:	/* clock interrupt */
		cnt.v_intr++;
#if 0
#ifdef EVCNT_COUNTERS
		clock_intr_evcnt.ev_count++;
#else
		intrcnt[INTRCNT_CLOCK]++;
#endif
#endif
		if (platform.clockintr)
			(*platform.clockintr)(framep);
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
		printf("performance interrupt!\n");
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
		if (alpha_implver() == ALPHA_IMPLVER_EV5
		    && alpha_amask(ALPHA_AMASK_BWX) == 0)
			rcpt = ldbu((vm_offset_t)addr);
		else
			rcpt = *(volatile u_int8_t *)addr;
		break;

	case sizeof (u_int16_t):
		if (alpha_implver() == ALPHA_IMPLVER_EV5
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
