/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
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
 *      This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Copyright (C) 2001 Benno Rice
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *	$NetBSD: machdep.c,v 1.74.2.1 2000/11/01 16:13:48 tv Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/cpu.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/signalvar.h>
#include <sys/syscallsubr.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/ucontext.h>
#include <sys/uio.h>

#include <machine/altivec.h>
#include <machine/cpu.h>
#include <machine/elf.h>
#include <machine/fpu.h>
#include <machine/pcb.h>
#include <machine/reg.h>
#include <machine/sigframe.h>
#include <machine/trap.h>
#include <machine/vmparam.h>

static int	grab_mcontext(struct thread *, mcontext_t *, int);

void
sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct trapframe *tf;
	struct sigacts *psp;
	struct sigframe sf;
	struct thread *td;
	struct proc *p;
	size_t sfpsize;
	caddr_t sfp, usfp;
	int oonstack, rndfsize;
	int sig;
	int code;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);

	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	tf = td->td_frame;
	oonstack = sigonstack(tf->fixreg[1]);

	/*
	 * Fill siginfo structure.
	 */
	ksi->ksi_info.si_signo = ksi->ksi_signo;
	#ifdef AIM
	ksi->ksi_info.si_addr = (void *)((tf->exc == EXC_DSI) ? 
	    tf->cpu.aim.dar : tf->srr0);
	#else
	ksi->ksi_info.si_addr = (void *)((tf->exc == EXC_DSI) ? 
	    tf->cpu.booke.dear : tf->srr0);
	#endif

	sig = ksi->ksi_signo;
	code = ksi->ksi_code;
	sfp = (caddr_t)&sf;
	sfpsize = sizeof(sf);
	rndfsize = ((sizeof(sf) + 15) / 16) * 16;

	/*
	 * Save user context
	 */

	memset(&sf, 0, sizeof(sf));
	grab_mcontext(td, &sf.sf_uc.uc_mcontext, 0);

	sf.sf_uc.uc_sigmask = *mask;
	sf.sf_uc.uc_stack = td->td_sigstk;
	sf.sf_uc.uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK)
	    ? ((oonstack) ? SS_ONSTACK : 0) : SS_DISABLE;

	sf.sf_uc.uc_mcontext.mc_onstack = (oonstack) ? 1 : 0;

	CTR4(KTR_SIG, "sendsig: td=%p (%s) catcher=%p sig=%d", td, p->p_comm,
	     catcher, sig);

	/*
	 * Allocate and validate space for the signal handler context.
	 */
	if ((td->td_pflags & TDP_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		usfp = (void *)(td->td_sigstk.ss_sp +
		   td->td_sigstk.ss_size - rndfsize);
	} else {
		usfp = (void *)(tf->fixreg[1] - rndfsize);
	}

	/*
	 * Translate the signal if appropriate (Linux emu ?)
	 */
	if (p->p_sysent->sv_sigtbl && sig <= p->p_sysent->sv_sigsize)
		sig = p->p_sysent->sv_sigtbl[_SIG_IDX(sig)];

	/*
	 * Save the floating-point state, if necessary, then copy it.
	 */
	/* XXX */

	/*
	 * Set up the registers to return to sigcode.
	 *
	 *   r1/sp - sigframe ptr
	 *   lr    - sig function, dispatched to by blrl in trampoline
	 *   r3    - sig number
	 *   r4    - SIGINFO ? &siginfo : exception code
	 *   r5    - user context
	 *   srr0  - trampoline function addr
	 */
	tf->lr = (register_t)catcher;
	tf->fixreg[1] = (register_t)usfp;
	tf->fixreg[FIRSTARG] = sig;
	tf->fixreg[FIRSTARG+2] = (register_t)usfp +
	    offsetof(struct sigframe, sf_uc);
	if (SIGISMEMBER(psp->ps_siginfo, sig)) {
		/*
		 * Signal handler installed with SA_SIGINFO.
		 */
			tf->fixreg[FIRSTARG+1] = (register_t)usfp +
			    offsetof(struct sigframe, sf_si);
			sf.sf_si = ksi->ksi_info;
	} else {
		/* Old FreeBSD-style arguments. */
		tf->fixreg[FIRSTARG+1] = code;
		#ifdef AIM
		tf->fixreg[FIRSTARG+3] = (tf->exc == EXC_DSI) ? 
		    tf->cpu.aim.dar : tf->srr0;
		#else
		tf->fixreg[FIRSTARG+3] = (tf->exc == EXC_DSI) ? 
		    tf->cpu.booke.dear : tf->srr0;
		#endif
	}
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(p);

	tf->srr0 = (register_t)(p->p_sysent->sv_psstrings -
	    *(p->p_sysent->sv_szsigcode));

	/*
	 * copy the frame out to userland.
	 */
	if (copyout(sfp, usfp, sfpsize) != 0) {
		/*
		 * Process has trashed its stack. Kill it.
		 */
		CTR2(KTR_SIG, "sendsig: sigexit td=%p sfp=%p", td, sfp);
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	CTR3(KTR_SIG, "sendsig: return td=%p pc=%#x sp=%#x", td,
	     tf->srr0, tf->fixreg[1]);

	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}

int
sigreturn(struct thread *td, struct sigreturn_args *uap)
{
	ucontext_t uc;
	int error;

	CTR2(KTR_SIG, "sigreturn: td=%p ucp=%p", td, uap->sigcntxp);

	if (copyin(uap->sigcntxp, &uc, sizeof(uc)) != 0) {
		CTR1(KTR_SIG, "sigreturn: efault td=%p", td);
		return (EFAULT);
	}

	error = set_mcontext(td, &uc.uc_mcontext);
	if (error != 0)
		return (error);

	kern_sigprocmask(td, SIG_SETMASK, &uc.uc_sigmask, NULL, 0);

	CTR3(KTR_SIG, "sigreturn: return td=%p pc=%#x sp=%#x",
	     td, uc.uc_mcontext.mc_srr0, uc.uc_mcontext.mc_gpr[1]);

	return (EJUSTRETURN);
}

#ifdef COMPAT_FREEBSD4
int
freebsd4_sigreturn(struct thread *td, struct freebsd4_sigreturn_args *uap)
{

	return sigreturn(td, (struct sigreturn_args *)uap);
}
#endif

/*
 * Construct a PCB from a trapframe. This is called from kdb_trap() where
 * we want to start a backtrace from the function that caused us to enter
 * the debugger. We have the context in the trapframe, but base the trace
 * on the PCB. The PCB doesn't have to be perfect, as long as it contains
 * enough for a backtrace.
 */
void
makectx(struct trapframe *tf, struct pcb *pcb)
{

	pcb->pcb_lr = tf->srr0;
	pcb->pcb_sp = tf->fixreg[1];
}

/*
 * get_mcontext/sendsig helper routine that doesn't touch the
 * proc lock
 */
static int
grab_mcontext(struct thread *td, mcontext_t *mcp, int flags)
{
	struct pcb *pcb;

	pcb = td->td_pcb;

	memset(mcp, 0, sizeof(mcontext_t));

	mcp->mc_vers = _MC_VERSION;
	mcp->mc_flags = 0;
	memcpy(&mcp->mc_frame, td->td_frame, sizeof(struct trapframe));
	if (flags & GET_MC_CLEAR_RET) {
		mcp->mc_gpr[3] = 0;
		mcp->mc_gpr[4] = 0;
	}

#ifdef AIM
	/*
	 * This assumes that floating-point context is *not* lazy,
	 * so if the thread has used FP there would have been a
	 * FP-unavailable exception that would have set things up
	 * correctly.
	 */
	if (pcb->pcb_flags & PCB_FPU) {
		KASSERT(td == curthread,
			("get_mcontext: fp save not curthread"));
		critical_enter();
		save_fpu(td);
		critical_exit();
		mcp->mc_flags |= _MC_FP_VALID;
		memcpy(&mcp->mc_fpscr, &pcb->pcb_fpu.fpscr, sizeof(double));
		memcpy(mcp->mc_fpreg, pcb->pcb_fpu.fpr, 32*sizeof(double));
	}

	/*
	 * Repeat for Altivec context
	 */

	if (pcb->pcb_flags & PCB_VEC) {
		KASSERT(td == curthread,
			("get_mcontext: fp save not curthread"));
		critical_enter();
		save_vec(td);
		critical_exit();
		mcp->mc_flags |= _MC_AV_VALID;
		mcp->mc_vscr  = pcb->pcb_vec.vscr;
		mcp->mc_vrsave =  pcb->pcb_vec.vrsave;
		memcpy(mcp->mc_avec, pcb->pcb_vec.vr, sizeof(mcp->mc_avec));
	}
#endif

	mcp->mc_len = sizeof(*mcp);

	return (0);
}

int
get_mcontext(struct thread *td, mcontext_t *mcp, int flags)
{
	int error;

	error = grab_mcontext(td, mcp, flags);
	if (error == 0) {
		PROC_LOCK(curthread->td_proc);
		mcp->mc_onstack = sigonstack(td->td_frame->fixreg[1]);
		PROC_UNLOCK(curthread->td_proc);
	}

	return (error);
}

int
set_mcontext(struct thread *td, const mcontext_t *mcp)
{
	struct pcb *pcb;
	struct trapframe *tf;

	pcb = td->td_pcb;
	tf = td->td_frame;

	if (mcp->mc_vers != _MC_VERSION || mcp->mc_len != sizeof(*mcp))
		return (EINVAL);

	#ifdef AIM
	/*
	 * Don't let the user set privileged MSR bits
	 */
	if ((mcp->mc_srr1 & PSL_USERSTATIC) != (tf->srr1 & PSL_USERSTATIC)) {
		return (EINVAL);
	}
	#endif

	memcpy(tf, mcp->mc_frame, sizeof(mcp->mc_frame));

	#ifdef AIM
	if (mcp->mc_flags & _MC_FP_VALID) {
		if ((pcb->pcb_flags & PCB_FPU) != PCB_FPU) {
			critical_enter();
			enable_fpu(td);
			critical_exit();
		}
		memcpy(&pcb->pcb_fpu.fpscr, &mcp->mc_fpscr, sizeof(double));
		memcpy(pcb->pcb_fpu.fpr, mcp->mc_fpreg, 32*sizeof(double));
	}

	if (mcp->mc_flags & _MC_AV_VALID) {
		if ((pcb->pcb_flags & PCB_VEC) != PCB_VEC) {
			critical_enter();
			enable_vec(td);
			critical_exit();
		}
		pcb->pcb_vec.vscr = mcp->mc_vscr;
		pcb->pcb_vec.vrsave = mcp->mc_vrsave;
		memcpy(pcb->pcb_vec.vr, mcp->mc_avec, sizeof(mcp->mc_avec));
	}
	#endif

	return (0);
}

/*
 * Set set up registers on exec.
 */
void
exec_setregs(struct thread *td, u_long entry, u_long stack, u_long ps_strings)
{
	struct trapframe	*tf;
	register_t		argc;

	tf = trapframe(td);
	bzero(tf, sizeof *tf);
	tf->fixreg[1] = -roundup(-stack + 8, 16);

	/*
	 * Set up arguments for _start():
	 *	_start(argc, argv, envp, obj, cleanup, ps_strings);
	 *
	 * Notes:
	 *	- obj and cleanup are the auxilliary and termination
	 *	  vectors.  They are fixed up by ld.elf_so.
	 *	- ps_strings is a NetBSD extention, and will be
	 * 	  ignored by executables which are strictly
	 *	  compliant with the SVR4 ABI.
	 *
	 * XXX We have to set both regs and retval here due to different
	 * XXX calling convention in trap.c and init_main.c.
	 */

	/* Collect argc from the user stack */
	argc = fuword((void *)stack);

        /*
         * XXX PG: these get overwritten in the syscall return code.
         * execve() should return EJUSTRETURN, like it does on NetBSD.
         * Emulate by setting the syscall return value cells. The
         * registers still have to be set for init's fork trampoline.
         */
        td->td_retval[0] = argc;
        td->td_retval[1] = stack + sizeof(register_t);
	tf->fixreg[3] = argc;
	tf->fixreg[4] = stack + sizeof(register_t);
	tf->fixreg[5] = stack + (2 + argc)*sizeof(register_t);
	tf->fixreg[6] = 0;				/* auxillary vector */
	tf->fixreg[7] = 0;				/* termination vector */
	tf->fixreg[8] = (register_t)ps_strings;		/* NetBSD extension */

	tf->srr0 = entry;
	tf->srr1 = PSL_USERSET | PSL_FE_DFLT;
	td->td_pcb->pcb_flags = 0;
}


int
fill_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *tf;

	tf = td->td_frame;
	memcpy(regs, tf, sizeof(struct reg));

	return (0);
}

int
fill_dbregs(struct thread *td, struct dbreg *dbregs)
{
	/* No debug registers on PowerPC */
	return (ENOSYS);
}

int
fill_fpregs(struct thread *td, struct fpreg *fpregs)
{
	struct pcb *pcb;

	pcb = td->td_pcb;

	if ((pcb->pcb_flags & PCB_FPU) == 0)
		memset(fpregs, 0, sizeof(struct fpreg));
	else
		memcpy(fpregs, &pcb->pcb_fpu, sizeof(struct fpreg));

	return (0);
}

int
set_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *tf;

	tf = td->td_frame;
	memcpy(tf, regs, sizeof(struct reg));
	
	return (0);
}

int
set_dbregs(struct thread *td, struct dbreg *dbregs)
{
	/* No debug registers on PowerPC */
	return (ENOSYS);
}

int
set_fpregs(struct thread *td, struct fpreg *fpregs)
{
#ifdef AIM
	struct pcb *pcb;

	pcb = td->td_pcb;
	if ((pcb->pcb_flags & PCB_FPU) == 0)
		enable_fpu(td);
	memcpy(&pcb->pcb_fpu, fpregs, sizeof(struct fpreg));
#endif

	return (0);
}



void
cpu_set_syscall_retval(struct thread *td, int error)
{
	struct proc *p;
	struct trapframe *tf;
	int fixup;

	if (error == EJUSTRETURN)
		return;

	p = td->td_proc;
	tf = td->td_frame;

	if (tf->fixreg[0] == SYS___syscall) {
		int code = tf->fixreg[FIRSTARG + 1];
		if (p->p_sysent->sv_mask)
			code &= p->p_sysent->sv_mask;
		fixup = (code != SYS_freebsd6_lseek && code != SYS_lseek) ?
		    1 : 0;
	} else
		fixup = 0;

	switch (error) {
	case 0:
		if (fixup) {
			/*
			 * 64-bit return, 32-bit syscall. Fixup byte order
			 */
			tf->fixreg[FIRSTARG] = 0;
			tf->fixreg[FIRSTARG + 1] = td->td_retval[0];
		} else {
			tf->fixreg[FIRSTARG] = td->td_retval[0];
			tf->fixreg[FIRSTARG + 1] = td->td_retval[1];
		}
		tf->cr &= ~0x10000000;		/* Unset summary overflow */
		break;
	case ERESTART:
		/*
		 * Set user's pc back to redo the system call.
		 */
		tf->srr0 -= 4;
		break;
	default:
		if (p->p_sysent->sv_errsize) {
			error = (error < p->p_sysent->sv_errsize) ?
			    p->p_sysent->sv_errtbl[error] : -1;
		}
		tf->fixreg[FIRSTARG] = error;
		tf->cr |= 0x10000000;		/* Set summary overflow */
		break;
	}
}

/*
 * Threading functions
 */
void
cpu_thread_exit(struct thread *td)
{
}

void
cpu_thread_clean(struct thread *td)
{
}

void
cpu_thread_alloc(struct thread *td)
{
	struct pcb *pcb;

	pcb = (struct pcb *)((td->td_kstack + td->td_kstack_pages * PAGE_SIZE -
	    sizeof(struct pcb)) & ~0x2fUL);
	td->td_pcb = pcb;
	td->td_frame = (struct trapframe *)pcb - 1;
}

void
cpu_thread_free(struct thread *td)
{
}

int
cpu_set_user_tls(struct thread *td, void *tls_base)
{

	td->td_frame->fixreg[2] = (register_t)tls_base + 0x7008;
	return (0);
}

void
cpu_set_upcall(struct thread *td, struct thread *td0)
{
	struct pcb *pcb2;
	struct trapframe *tf;
	struct callframe *cf;

	pcb2 = td->td_pcb;

	/* Copy the upcall pcb */
	bcopy(td0->td_pcb, pcb2, sizeof(*pcb2));

	/* Create a stack for the new thread */
	tf = td->td_frame;
	bcopy(td0->td_frame, tf, sizeof(struct trapframe));
	tf->fixreg[FIRSTARG] = 0;
	tf->fixreg[FIRSTARG + 1] = 0;
	tf->cr &= ~0x10000000;

	/* Set registers for trampoline to user mode. */
	cf = (struct callframe *)tf - 1;
	memset(cf, 0, sizeof(struct callframe));
	cf->cf_func = (register_t)fork_return;
	cf->cf_arg0 = (register_t)td;
	cf->cf_arg1 = (register_t)tf;

	pcb2->pcb_sp = (register_t)cf;
	pcb2->pcb_lr = (register_t)fork_trampoline;
	pcb2->pcb_cpu.aim.usr = 0;

	/* Setup to release spin count in fork_exit(). */
	td->td_md.md_spinlock_count = 1;
	td->td_md.md_saved_msr = PSL_KERNSET;
}

void
cpu_set_upcall_kse(struct thread *td, void (*entry)(void *), void *arg,
	stack_t *stack)
{
	struct trapframe *tf;
	uintptr_t sp;

	tf = td->td_frame;
	/* align stack and alloc space for frame ptr and saved LR */
	sp = ((uintptr_t)stack->ss_sp + stack->ss_size - 8) &
	    ~0x1f;
	bzero(tf, sizeof(struct trapframe));

	tf->fixreg[1] = (register_t)sp;
	tf->fixreg[3] = (register_t)arg;
	tf->srr0 = (register_t)entry;
    #ifdef AIM
	tf->srr1 = PSL_MBO | PSL_USERSET | PSL_FE_DFLT;
    #else
	tf->srr1 = PSL_USERSET;
    #endif

	td->td_pcb->pcb_flags = 0;

	td->td_retval[0] = (register_t)entry;
	td->td_retval[1] = 0;
}

