/*-
 * Copyright 2001 by Thomas Moestl <tmm@FreeBSD.org>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/ktr.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/user.h>

#include <machine/fp.h>
#include <machine/frame.h>
#include <machine/fsr.h>
#include <machine/pcb.h>
#include <machine/tstate.h>

void
fp_init_thread(struct pcb *pcb)
{

	bzero(&pcb->pcb_fpstate.fp_fb, sizeof(pcb->pcb_fpstate.fp_fb));
#if 0
	pcb->pcb_fpstate.fp_fsr = 0;
#else
	/*
	 * This causes subnormal operands to be treated as zeroes, which
	 * prevents unfinished_FPop traps in this cases, which would require
	 * emultation that is not yet implemented.
	 * This does of course reduce the accuracy, so it should be removed
	 * as soon as possible.
	 */
	pcb->pcb_fpstate.fp_fsr = FSR_NS;
#endif
	wr(fprs, 0, 0);
}

int
fp_enable_thread(struct thread *td, struct trapframe *tf)
{
	struct pcb *pcb;

	pcb = td->td_pcb;
	CTR5(KTR_TRAP, "fp_enable_thread: tpc=%#lx, tnpc=%#lx, tstate=%#lx, "
	    "fprs=%#lx, fsr=%#lx", tf->tf_tpc, tf->tf_tnpc, tf->tf_tstate,
	    pcb->pcb_fpstate.fp_fprs, pcb->pcb_fpstate.fp_fsr);
	if ((tf->tf_tstate & TSTATE_PEF) != 0 &&
	    (pcb->pcb_fpstate.fp_fprs & FPRS_FEF) == 0) {
		/*
		 * Enable FEF for now. The SCD mandates that this should be
		 * done when no user trap is set. User traps are not currently
		 * supported...
		 */
		wr(fprs, rd(fprs), FPRS_FEF);
		return (1);
	}
	
	if ((tf->tf_tstate & TSTATE_PEF) != 0)
		return (0);
	mtx_lock_spin(&sched_lock);
	tf->tf_tstate |= TSTATE_PEF;
	/* Actually load the FP state into the registers. */
	restorefpctx(&pcb->pcb_fpstate);
	mtx_unlock_spin(&sched_lock);
	return (1);
}

int
fp_exception_other(struct thread *td, struct trapframe *tf)
{
	u_long fsr;

	__asm __volatile("stx %%fsr, %0" : "=m" (fsr));
	CTR4(KTR_TRAP, "fp_exception_other: tpc=%#lx, tnpc=%#lx, ftt=%lx "
	    "fsr=%lx", tf->tf_tpc, tf->tf_tnpc, FSR_FTT(fsr), fsr);
	/* XXX: emulate unimplemented and unfinished instructions. */
	return (SIGFPE);
}
