/*-
 * Copyright (c) 2014-2016 Robert N. M. Watson
 * Copyright (c) 2015 SRI International
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

#include <sys/cdefs.h>

#if !__has_feature(capabilities)
#error "This code requires a CHERI-aware compiler"
#endif

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/ucontext.h>

#include <machine/cheri.h>
#include <machine/cheric.h>
#include <machine/cpuregs.h>
#include <machine/regnum.h>
#include <machine/sysarch.h>

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>

#include "cheri_stack.h"

/*
 * Return the number of frames on the trusted stack.
 */
int
cheri_stack_numframes(int *numframesp)
{
	struct cheri_stack cs;

	/*
	 * Retrieve trusted stack and validate before returning a frame count.
	 */
	if (sysarch(CHERI_GET_STACK, &cs) != 0)
		return (-1);
	if ((cs.cs_tsize % CHERI_FRAME_SIZE) != 0 ||
	    (cs.cs_tsp > cs.cs_tsize) ||
	    (cs.cs_tsp % CHERI_FRAME_SIZE) != 0) {
		errno = ERANGE;
		return (-1);
	}
	*numframesp = (cs.cs_tsize - cs.cs_tsp) / CHERI_FRAME_SIZE;
	return (0);
}

/*
 * Unwind the trusted stack by the specified number of frames (or all).
 */
int
cheri_stack_unwind(ucontext_t *uap, register_t ret, u_int op,
    u_int num_frames)
{
	struct cheri_frame *cfp;
	struct cheri_stack cs;
	struct cheri_stack_frame *csfp;
	u_int stack_size, stack_frames;
	register_t saved_mcreg0;

	if (op != CHERI_STACK_UNWIND_OP_N &&
	    op != CHERI_STACK_UNWIND_OP_ALL) {
		errno = EINVAL;
		return (-1);
	}

	/*
	 * Request to unwind zero frames is a no-op: no state transformation
	 * is needed.
	 */
	if ((op == CHERI_STACK_UNWIND_OP_N) && (num_frames == 0))
		return (0);

	/*
	 * Retrieve trusted stack and validate before attempting to unwind.
	 */
	if (sysarch(CHERI_GET_STACK, &cs) != 0)
		return (-1);
	if ((cs.cs_tsize % CHERI_FRAME_SIZE) != 0 ||
	    (cs.cs_tsp > cs.cs_tsize) ||
	    (cs.cs_tsp % CHERI_FRAME_SIZE) != 0) {
		errno = ERANGE;
		return (-1);
	}

	/*
	 * See if there is room on the stack for that much unwinding.
	 */
	stack_size = cs.cs_tsize / CHERI_FRAME_SIZE;
	stack_frames = (cs.cs_tsize - cs.cs_tsp) / CHERI_FRAME_SIZE;
	if (op == CHERI_STACK_UNWIND_OP_ALL)
		num_frames = stack_frames;
	if ((num_frames < 0) || (stack_frames < num_frames)) {
		errno = ERANGE;
		return (-1);
	}

	/*
	 * Restore state from the last frame being unwound.
	 */
	csfp = &cs.cs_frames[stack_size - (stack_frames - num_frames) - 1];
#if 0
	/* Make sure we will be returning to ambient authority. */
	if (cheri_getbase(csfp->csf_pcc) != cheri_getbase(cheri_getpcc()) ||
	    cheri_getlen(csfp->csf_pcc) != cheri_getlen(cheri_getpcc()))
		return (-1);
#endif

	/*
	 * Pop stack desired number of frames.
	 */
	cs.cs_tsp += num_frames * CHERI_FRAME_SIZE;
	assert(cs.cs_tsp <= cs.cs_tsize);

#ifdef __CHERI_SANDBOX__
	cfp = &uap->uc_mcontext.mc_cheriframe;
#else
	cfp = (struct cheri_frame *)uap->uc_mcontext.mc_cp2state;
	if (cfp == NULL || uap->uc_mcontext.mc_cp2state_len != sizeof(*cfp)) {
		errno = ERANGE;
		return (-1);
	}
#endif

	/*
	 * Zero the capability register file, explicitly restoring $pcc and
	 * $idc from the last trusted-stack frame.
	 */
	memset(cfp, 0, sizeof(*cfp));
	cfp->cf_idc =  csfp->csf_idc;
	cfp->cf_pcc = csfp->csf_pcc;

	/*
	 * Zero the general-purpose register file.  restore not only $pc, but
	 * also the slot for $zero, which will hold a magic number across
	 * sigcode and sigreturn().  Also set a return value.
	 *
	 * XXXRW: The kernel unwinder sets V1 to the signal number?
	 */
	saved_mcreg0 = uap->uc_mcontext.mc_regs[0];
	memset(uap->uc_mcontext.mc_regs, 0, sizeof(uap->uc_mcontext.mc_regs));
	uap->uc_mcontext.mc_regs[0] = saved_mcreg0;
	uap->uc_mcontext.mc_pc = cheri_getoffset(cfp->cf_pcc);
	uap->uc_mcontext.mc_regs[V0] = ret;

	/*
	 * Update kernel view of trusted stack.
	 */
	if (sysarch(CHERI_SET_STACK, &cs) != 0)
		return (-1);
	return (0);
}
