/*-
 * Copyright (c) 2011-2014 Robert N. M. Watson
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>

#include <ddb/ddb.h>
#include <sys/kdb.h>

#include <machine/atomic.h>
#include <machine/cheri.h>
#include <machine/pcb.h>
#include <machine/sysarch.h>

/*
 * When new threads are forked, by default simply replicate the parent
 * thread's CHERI-related signal-handling state.
 *
 * XXXRW: Is this, in fact, the right thing?
 */
void
cheri_signal_copy(struct pcb *dst, struct pcb *src)
{

	cheri_memcpy(&dst->pcb_cherisignal, &src->pcb_cherisignal,
	    sizeof(dst->pcb_cherisignal));
}

/*
 * Configure CHERI register state for a thread about to resume in a signal
 * handler.  Eventually, csigp should contain configurable values, but for
 * now, this ensures handlers run with ambient authority in a useful way.
 * Note that this doesn't touch the already copied-out CHERI register frame
 * (see sendsig()), and hence when sigreturn() is called, the previous CHERI
 * state will be restored by default.
 */
void
cheri_sendsig(struct thread *td)
{
	struct cheri_frame *cfp;
	struct cheri_signal *csigp;

	cfp = &td->td_pcb->pcb_cheriframe;
	csigp = &td->td_pcb->pcb_cherisignal;
	cheri_capability_copy(&cfp->cf_c0, &csigp->csig_c0);
	cheri_capability_copy(&cfp->cf_c11, &csigp->csig_c11);
	cheri_capability_copy(&cfp->cf_idc, &csigp->csig_idc);
	cheri_capability_copy(&cfp->cf_pcc, &csigp->csig_pcc);
}

/*
 * As with system calls, handling signal delivery connotes special authority
 * in the runtime environment.  In the signal delivery code, we need to
 * determine whether to trust the executing thread to have valid stack state,
 * and use this function to query whether the execution environment is
 * suitable for direct handler execution, or if (in effect) a security-domain
 * transition is required first.
 */
int
cheri_signal_sandboxed(struct thread *td)
{
	uintmax_t c_perms;

	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC,
	    &td->td_pcb->pcb_cheriframe.cf_pcc, 0);
	CHERI_CGETPERM(c_perms, CHERI_CR_CTEMP0);
	if ((c_perms & CHERI_PERM_SYSCALL) == 0) {
		atomic_add_int(&security_cheri_sandboxed_signals, 1);
		return (ECAPMODE);
	}
	return (0);
}
