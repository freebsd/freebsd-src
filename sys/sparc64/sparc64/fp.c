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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
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
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/user.h>

#include <machine/frame.h>
#include <machine/pcb.h>

void
fp_init_pcb(struct pcb *pcb)
{

	bzero(&pcb->pcb_fpstate.fp_fb, sizeof(pcb->pcb_fpstate.fp_fb));
	pcb->pcb_fpstate.fp_fsr = FSR_TEM_DZ;
	pcb->pcb_fpstate.fp_fprs = FPRS_FEF;
}

int
fp_enable_proc(struct proc *p)
{
	
	if ((p->p_frame->tf_tstate & TSTATE_PEF) != 0)
		return (0);
	mtx_lock_spin(&sched_lock);
	p->p_frame->tf_tstate |= TSTATE_PEF;
	/* Actually load the FP state into the registers. */
	restorefpctx(&p->p_addr->u_pcb.pcb_fpstate);
	mtx_unlock_spin(&sched_lock);
	return (1);
}
