/*-
 * Copyright (c) 2013-2014 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>

#include <ddb/ddb.h>
#include <sys/kdb.h>

#include <machine/atomic.h>
#include <machine/cheri.h>
#include <machine/pcb.h>
#include <machine/sysarch.h>

/*-
 * Some user-level security models rely on strict call-return semantics, which
 * is implemented via a trusted stack in the object-capability invocation
 * path.  This file contains a simple implementation of a CHERI trusted stack
 * for the software exception path.
 *
 * XXXRW: Lots to think about here.
 *
 * 1. How do we want to handle user software models that aren't just
 *    call-return -- e.g., a closure-passing model.  Should the language
 *    runtime be able to rewrite return paths?
 * 2. Do we want some sort of kernel-implemented timeout/resource model, or
 *    just let userspace do it with signals?
 * 3. More generally, how do we want to deal with signals?  (a) switching to
 *    a suitable signal processing context; (b) extending sigcontext_t for
 *    capability state?
 */

/*
 * Initialise the trusted stack of a process (thread) control block.
 *
 * XXXRW: Someday, depth should perhaps be configurable.
 *
 * XXXRW: It makes sense to me that the stack starts empty, and the first
 * CCall populates it with a default return context (e.g., the language
 * runtime).  But does that make sense to anyone else?
 *
 * XXXRW: A fixed-size stack here may or may not be the right thing.
 */
void
cheri_stack_init(struct pcb *pcb)
{

	bzero(&pcb->pcb_cheristack, sizeof(pcb->pcb_cheristack));
	pcb->pcb_cheristack.cs_tsize = CHERI_STACK_SIZE;
	pcb->pcb_cheristack.cs_tsp = CHERI_STACK_SIZE;
}

/*
 * On fork(), we exactly reproduce the current thread's CHERI call stack in
 * the child, as the address space is exactly reproduced.
 *
 * XXXRW: Is this the right thing?
 */
void
cheri_stack_copy(struct pcb *pcb2, struct pcb *pcb1)
{

	cheri_memcpy(&pcb2->pcb_cheristack, &pcb1->pcb_cheristack,
	    sizeof(pcb2->pcb_cheristack));
}

/*
 * Normally, we expect that sandbox libraries will register signal handlers
 * for any traps that might arise within a sandbox, and handle them
 * explicitly.  However, in prototyping, it proves useful if the kernel can
 * 'assist' with unwinding when the signal is not caught: effectively, a
 * forced CReturn.  Unlike normal CReturn, because the caller has not
 * performed a controlled, compiler-directed return, we scrub the user
 * register files to avoid information and rights leakage.  The semantics are
 * a bit poor since we have no way to usefully report it other than frobbing
 * return-register state.  It's not clear that this is really avoidable.
 *
 * XXXRW: Forced return may leave callee invariants violated -- the price for
 * throwing an uncaught signal.  However, it's possible to imagine fail-open
 * scenarios in which a callee with privilege is somehow tricked into
 * throwing an uncaught exception during a window where its invariants are
 * violated, leading to a security vulnerability.  Userspace signal handlers
 * may have more information about how to avoid this and so are preferred; in
 * particular, a userspace runtime might provide a mechanism to allow catching
 * exceptions explicitly rather than always unconditionally unwinding.
 *
 * XXXRW: There's a possible argument for unwinding multiple frames up to the
 * last frame with ambient authority.
 */
int
cheri_stack_unwind(struct thread *td, struct trapframe *tf, int signum)
{
	struct cheri_stack_frame *csfp;
	struct pcb *pcb = td->td_pcb;
	register_t sr, badvaddr, cause;
	f_register_t fsr;

	PROC_LOCK_ASSERT(td->td_proc, MA_OWNED);

	if (pcb->pcb_cheristack.cs_tsp == CHERI_STACK_SIZE)
		return (0);

#if DDB
	if (security_cheri_debugger_on_sandbox_unwind)
		kdb_enter(KDB_WHY_CHERI, "CHERI sandbox unwind on signal");
#endif

	/*
	 * XXXRW: It is my belief that the trap frame in a thread is always a
	 * pointer to the PCB.  Check this is true, however, because I rely on
	 * it.
	 */
	KASSERT(td->td_frame == &pcb->pcb_regs,
	    ("%s: td_frame != pcb_regs", __func__));
	KASSERT(td->td_frame == tf, ("%s: td_frame != tf", __func__));

	/*
	 * Clear the regular and capability register files to ensure no state
	 * (information, rights) is returned to the caller that shouldn't be
	 * when the callee exits unexpectedly.  Save and restore kernel-side
	 * registers, however.
	 */
	sr = pcb->pcb_regs.sr;
	badvaddr = pcb->pcb_regs.badvaddr;
	cause = pcb->pcb_regs.cause;
	fsr = pcb->pcb_regs.fsr;
	bzero(&pcb->pcb_regs, sizeof(pcb->pcb_regs));
	bzero(&pcb->pcb_cheriframe, sizeof(pcb->pcb_cheriframe));
	pcb->pcb_regs.sr = sr;
	pcb->pcb_regs.badvaddr = badvaddr;
	pcb->pcb_regs.cause = cause;
	pcb->pcb_regs.fsr = fsr;

	/*
	 * Reproduce CReturn.
	 */
	csfp = &pcb->pcb_cheristack.cs_frames[pcb->pcb_cheristack.cs_tsp /
	    CHERI_FRAME_SIZE];
	pcb->pcb_cheristack.cs_tsp += CHERI_FRAME_SIZE;

	/*
	 * Pop IDC, PCC.
	 */
	cheri_capability_load(CHERI_CR_CTEMP0, &csfp->csf_idc);
	cheri_capability_store(CHERI_CR_CTEMP0, &pcb->pcb_cheriframe.cf_idc);
	cheri_capability_load(CHERI_CR_CTEMP0, &csfp->csf_pcc);
	cheri_capability_store(CHERI_CR_CTEMP0, &pcb->pcb_cheriframe.cf_pcc);

	/*
	 * Extract PCC.offset into PC.
	 */
	CHERI_CGETOFFSET(pcb->pcb_regs.pc, CHERI_CR_CTEMP0);

	/*
	 * Set 'v0' to -1, and 'v1' to the signal number so that the consumer
	 * of CCall can handle the error.
	 *
	 * XXXRW: That isn't really quite what we want, however.  What about
	 * CCall failures themselves, and what if CReturn returns a -1 -- how
	 * should the caller interpret that?
	 */
	pcb->pcb_regs.v0 = -1;
	pcb->pcb_regs.v1 = signum;
	return (1);
}

int
cheri_sysarch_getstack(struct thread *td, struct sysarch_args *uap)
{

	KASSERT(uap->op == CHERI_GET_STACK, ("%s: invalid opcode %d",
	    __func__, uap->op));

	return (copyoutcap(&td->td_pcb->pcb_cheristack, uap->parms,
	    sizeof(td->td_pcb->pcb_cheristack)));
}

int
cheri_sysarch_setstack(struct thread *td, struct sysarch_args *uap)
{
	struct cheri_stack cs;
	int error;

	KASSERT(uap->op == CHERI_SET_STACK, ("%s: invalid opcode %d",
	    __func__, uap->op));

	error = copyincap(uap->parms, &cs, sizeof(cs));
	if (error)
		return (error);

	/*
	 * Validate trusted-stack fields to prevent, for example, setting an
	 * improper stack pointer or change in stack size.
	 */
	if (cs.cs_tsize != td->td_pcb->pcb_cheristack.cs_tsize)
		return (EINVAL);
	if (cs.cs_tsp < 0 || cs.cs_tsp > CHERI_STACK_SIZE)
		return (EINVAL);
	cheri_bcopy(&cs, &td->td_pcb->pcb_cheristack, sizeof(cs));
	return (0);
}

#ifdef DDB
/*
 * Print out the trusted stack for the current thread, starting at the top.
 *
 * XXXRW: Would be nice to take a tid/pid argument rather than always use
 * curthread.
 */
DB_SHOW_COMMAND(cheristack, ddb_dump_cheristack)
{
	struct cheri_stack_frame *csfp;
	struct pcb *pcb = curthread->td_pcb;
	int i;

	db_printf("Trusted stack for TID %d; TSP 0x%016jx\n",
	    curthread->td_tid, (uintmax_t)pcb->pcb_cheristack.cs_tsp);
	for (i = CHERI_STACK_DEPTH - 1; i >= 0; i--) {
	    /* i > (pcb->pcb_cheristack.cs_tsp / CHERI_FRAME_SIZE); i--) { */
		csfp = &pcb->pcb_cheristack.cs_frames[i];

		db_printf("Frame %d%c\n", i,
		    (i >= (pcb->pcb_cheristack.cs_tsp / CHERI_FRAME_SIZE)) ?
		    '*' : ' ');

		CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &csfp->csf_idc, 0);
		db_printf("  IDC ");
		DB_CHERI_CAP_PRINT(CHERI_CR_CTEMP0);

		CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &csfp->csf_pcc, 0);
		db_printf("  PCC ");
		DB_CHERI_CAP_PRINT(CHERI_CR_CTEMP0);
	}
}
#endif
