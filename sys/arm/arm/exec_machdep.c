/*	$NetBSD: arm32_machdep.c,v 1.44 2004/03/24 15:34:47 atatat Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 2004 Olivier Houchard
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/vmmeter.h>

#include <machine/asm.h>
#include <machine/machdep.h>
#include <machine/pcb.h>
#include <machine/sysarch.h>
#include <machine/vfp.h>
#include <machine/vmparam.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

_Static_assert(sizeof(mcontext_t) == 208, "mcontext_t size incorrect");
_Static_assert(sizeof(ucontext_t) == 260, "ucontext_t size incorrect");
_Static_assert(sizeof(siginfo_t) == 64, "siginfo_t size incorrect");

/*
 * Clear registers on exec
 */
void
exec_setregs(struct thread *td, struct image_params *imgp, uintptr_t stack)
{
	struct trapframe *tf = td->td_frame;

	memset(tf, 0, sizeof(*tf));
	tf->tf_usr_sp = stack;
	tf->tf_usr_lr = imgp->entry_addr;
	tf->tf_svc_lr = 0x77777777;
	tf->tf_pc = imgp->entry_addr;
	tf->tf_spsr = PSR_USR32_MODE;
	if ((register_t)imgp->entry_addr & 1)
		tf->tf_spsr |= PSR_T;
}

#ifdef VFP
/*
 * Get machine VFP context.
 */
void
get_vfpcontext(struct thread *td, mcontext_vfp_t *vfp)
{
	struct pcb *pcb;

	pcb = td->td_pcb;
	if (td == curthread) {
		critical_enter();
		vfp_store(&pcb->pcb_vfpstate, false);
		critical_exit();
	} else
		MPASS(TD_IS_SUSPENDED(td));
	memset(vfp, 0, sizeof(*vfp));
	memcpy(vfp->mcv_reg, pcb->pcb_vfpstate.reg,
	    sizeof(vfp->mcv_reg));
	vfp->mcv_fpscr = pcb->pcb_vfpstate.fpscr;
}

/*
 * Set machine VFP context.
 */
void
set_vfpcontext(struct thread *td, mcontext_vfp_t *vfp)
{
	struct pcb *pcb;

	pcb = td->td_pcb;
	if (td == curthread) {
		critical_enter();
		vfp_discard(td);
		critical_exit();
	} else
		MPASS(TD_IS_SUSPENDED(td));
	memcpy(pcb->pcb_vfpstate.reg, vfp->mcv_reg,
	    sizeof(pcb->pcb_vfpstate.reg));
	pcb->pcb_vfpstate.fpscr = vfp->mcv_fpscr;
}
#endif

int
arm_get_vfpstate(struct thread *td, void *args)
{
	int rv;
	struct arm_get_vfpstate_args ua;
	mcontext_vfp_t	mcontext_vfp;

	rv = copyin(args, &ua, sizeof(ua));
	if (rv != 0)
		return (rv);
	if (ua.mc_vfp_size != sizeof(mcontext_vfp_t))
		return (EINVAL);
#ifdef VFP
	get_vfpcontext(td, &mcontext_vfp);
#else
	bzero(&mcontext_vfp, sizeof(mcontext_vfp));
#endif

	rv = copyout(&mcontext_vfp, ua.mc_vfp,  sizeof(mcontext_vfp));
	if (rv != 0)
		return (rv);
	return (0);
}

/*
 * Get machine context.
 */
int
get_mcontext(struct thread *td, mcontext_t *mcp, int clear_ret)
{
	struct trapframe *tf = td->td_frame;
	__greg_t *gr = mcp->__gregs;

	if (clear_ret & GET_MC_CLEAR_RET) {
		gr[_REG_R0] = 0;
		gr[_REG_CPSR] = tf->tf_spsr & ~PSR_C;
	} else {
		gr[_REG_R0]   = tf->tf_r0;
		gr[_REG_CPSR] = tf->tf_spsr;
	}
	gr[_REG_R1]   = tf->tf_r1;
	gr[_REG_R2]   = tf->tf_r2;
	gr[_REG_R3]   = tf->tf_r3;
	gr[_REG_R4]   = tf->tf_r4;
	gr[_REG_R5]   = tf->tf_r5;
	gr[_REG_R6]   = tf->tf_r6;
	gr[_REG_R7]   = tf->tf_r7;
	gr[_REG_R8]   = tf->tf_r8;
	gr[_REG_R9]   = tf->tf_r9;
	gr[_REG_R10]  = tf->tf_r10;
	gr[_REG_R11]  = tf->tf_r11;
	gr[_REG_R12]  = tf->tf_r12;
	gr[_REG_SP]   = tf->tf_usr_sp;
	gr[_REG_LR]   = tf->tf_usr_lr;
	gr[_REG_PC]   = tf->tf_pc;

	mcp->mc_vfp_size = 0;
	mcp->mc_vfp_ptr = NULL;
	memset(&mcp->mc_spare, 0, sizeof(mcp->mc_spare));

	return (0);
}

/*
 * Set machine context.
 *
 * However, we don't set any but the user modifiable flags, and we won't
 * touch the cs selector.
 */
int
set_mcontext(struct thread *td, mcontext_t *mcp)
{
	mcontext_vfp_t mc_vfp, *vfp;
	struct trapframe *tf = td->td_frame;
	const __greg_t *gr = mcp->__gregs;
	int spsr;

	/*
	 * Make sure the processor mode has not been tampered with and
	 * interrupts have not been disabled.
	 */
	spsr = gr[_REG_CPSR];
	if ((spsr & PSR_MODE) != PSR_USR32_MODE ||
	    (spsr & (PSR_I | PSR_F)) != 0)
		return (EINVAL);

#ifdef WITNESS
	if (mcp->mc_vfp_size != 0 && mcp->mc_vfp_size != sizeof(mc_vfp)) {
		printf("%s: %s: Malformed mc_vfp_size: %d (0x%08X)\n",
		    td->td_proc->p_comm, __func__,
		    mcp->mc_vfp_size, mcp->mc_vfp_size);
	} else if (mcp->mc_vfp_size != 0 && mcp->mc_vfp_ptr == NULL) {
		printf("%s: %s: c_vfp_size != 0 but mc_vfp_ptr == NULL\n",
		    td->td_proc->p_comm, __func__);
	}
#endif

	if (mcp->mc_vfp_size == sizeof(mc_vfp) && mcp->mc_vfp_ptr != NULL) {
		if (copyin(mcp->mc_vfp_ptr, &mc_vfp, sizeof(mc_vfp)) != 0)
			return (EFAULT);
		vfp = &mc_vfp;
	} else {
		vfp = NULL;
	}

	tf->tf_r0 = gr[_REG_R0];
	tf->tf_r1 = gr[_REG_R1];
	tf->tf_r2 = gr[_REG_R2];
	tf->tf_r3 = gr[_REG_R3];
	tf->tf_r4 = gr[_REG_R4];
	tf->tf_r5 = gr[_REG_R5];
	tf->tf_r6 = gr[_REG_R6];
	tf->tf_r7 = gr[_REG_R7];
	tf->tf_r8 = gr[_REG_R8];
	tf->tf_r9 = gr[_REG_R9];
	tf->tf_r10 = gr[_REG_R10];
	tf->tf_r11 = gr[_REG_R11];
	tf->tf_r12 = gr[_REG_R12];
	tf->tf_usr_sp = gr[_REG_SP];
	tf->tf_usr_lr = gr[_REG_LR];
	tf->tf_pc = gr[_REG_PC];
	tf->tf_spsr = gr[_REG_CPSR];
#ifdef VFP
	if (vfp != NULL)
		set_vfpcontext(td, vfp);
#endif
	return (0);
}

void
sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct thread *td;
	struct proc *p;
	struct trapframe *tf;
	struct sigframe *fp, frame;
	struct sigacts *psp;
	struct sysentvec *sysent;
	int onstack;
	int sig;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	sig = ksi->ksi_signo;
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	tf = td->td_frame;
	onstack = sigonstack(tf->tf_usr_sp);

	CTR4(KTR_SIG, "sendsig: td=%p (%s) catcher=%p sig=%d", td, p->p_comm,
	    catcher, sig);

	/* Allocate and validate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) != 0 && !(onstack) &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		fp = (struct sigframe *)((uintptr_t)td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size);
#if defined(COMPAT_43)
		td->td_sigstk.ss_flags |= SS_ONSTACK;
#endif
	} else
		fp = (struct sigframe *)td->td_frame->tf_usr_sp;

	/* make room on the stack */
	fp--;

	/* make the stack aligned */
	fp = (struct sigframe *)STACKALIGN(fp);
	/* Populate the siginfo frame. */
	bzero(&frame, sizeof(frame));
	get_mcontext(td, &frame.sf_uc.uc_mcontext, 0);
#ifdef VFP
	get_vfpcontext(td, &frame.sf_vfp);
	frame.sf_uc.uc_mcontext.mc_vfp_size = sizeof(fp->sf_vfp);
	frame.sf_uc.uc_mcontext.mc_vfp_ptr = &fp->sf_vfp;
#else
	frame.sf_uc.uc_mcontext.mc_vfp_size = 0;
	frame.sf_uc.uc_mcontext.mc_vfp_ptr = NULL;
#endif
	frame.sf_si = ksi->ksi_info;
	frame.sf_uc.uc_sigmask = *mask;
	frame.sf_uc.uc_stack = td->td_sigstk;
	frame.sf_uc.uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK) != 0 ?
	    (onstack ? SS_ONSTACK : 0) : SS_DISABLE;
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(td->td_proc);

	/* Copy the sigframe out to the user's stack. */
	if (copyout(&frame, fp, sizeof(*fp)) != 0) {
		/* Process has trashed its stack. Kill it. */
		CTR2(KTR_SIG, "sendsig: sigexit td=%p fp=%p", td, fp);
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	/*
	 * Build context to run handler in.  We invoke the handler
	 * directly, only returning via the trampoline.  Note the
	 * trampoline version numbers are coordinated with machine-
	 * dependent code in libc.
	 */

	tf->tf_r0 = sig;
	tf->tf_r1 = (register_t)&fp->sf_si;
	tf->tf_r2 = (register_t)&fp->sf_uc;

	/* the trampoline uses r5 as the uc address */
	tf->tf_r5 = (register_t)&fp->sf_uc;
	tf->tf_pc = (register_t)catcher;
	tf->tf_usr_sp = (register_t)fp;
	sysent = p->p_sysent;
	if (PROC_HAS_SHP(p))
		tf->tf_usr_lr = (register_t)PROC_SIGCODE(p);
	else
		tf->tf_usr_lr = (register_t)(PROC_PS_STRINGS(p) -
		    *(sysent->sv_szsigcode));
	/* Set the mode to enter in the signal handler */
#if __ARM_ARCH >= 7
	if ((register_t)catcher & 1)
		tf->tf_spsr |= PSR_T;
	else
		tf->tf_spsr &= ~PSR_T;
#endif

	CTR3(KTR_SIG, "sendsig: return td=%p pc=%#x sp=%#x", td, tf->tf_usr_lr,
	    tf->tf_usr_sp);

	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}

int
sys_sigreturn(struct thread *td, struct sigreturn_args *uap)
{
	ucontext_t uc;
	int error;

	if (uap == NULL)
		return (EFAULT);
	if (copyin(uap->sigcntxp, &uc, sizeof(uc)))
		return (EFAULT);
	/* Restore register context. */
	error = set_mcontext(td, &uc.uc_mcontext);
	if (error != 0)
		return (error);

	/* Restore signal mask. */
	kern_sigprocmask(td, SIG_SETMASK, &uc.uc_sigmask, NULL, 0);

	return (EJUSTRETURN);
}
