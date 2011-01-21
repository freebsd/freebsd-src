/*-
 * Copyright (c) 2003 Peter Wemm
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/fcntl.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mman.h>
#include <sys/namei.h>
#include <sys/pioctl.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/resourcevar.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>

#include <compat/freebsd32/freebsd32_signal.h>
#include <compat/freebsd32/freebsd32_util.h>
#include <compat/freebsd32/freebsd32_proto.h>
#include <compat/ia32/ia32_signal.h>
#include <machine/psl.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/cpufunc.h>

#ifdef COMPAT_FREEBSD4
static void freebsd4_ia32_sendsig(sig_t, ksiginfo_t *, sigset_t *);
#endif
static void ia32_get_fpcontext(struct thread *td, struct ia32_mcontext *mcp);
static int ia32_set_fpcontext(struct thread *td, const struct ia32_mcontext *mcp);

extern int _ucode32sel, _udatasel;

#define	CS_SECURE(cs)		(ISPL(cs) == SEL_UPL)
#define	EFL_SECURE(ef, oef)	((((ef) ^ (oef)) & ~PSL_USERCHANGE) == 0)

static void
ia32_get_fpcontext(struct thread *td, struct ia32_mcontext *mcp)
{

	mcp->mc_ownedfp = fpugetregs(td, (struct savefpu *)&mcp->mc_fpstate);
	mcp->mc_fpformat = fpuformat();
}

static int
ia32_set_fpcontext(struct thread *td, const struct ia32_mcontext *mcp)
{

	if (mcp->mc_fpformat == _MC_FPFMT_NODEV)
		return (0);
	else if (mcp->mc_fpformat != _MC_FPFMT_XMM)
		return (EINVAL);
	else if (mcp->mc_ownedfp == _MC_FPOWNED_NONE)
		/* We don't care what state is left in the FPU or PCB. */
		fpstate_drop(td);
	else if (mcp->mc_ownedfp == _MC_FPOWNED_FPU ||
	    mcp->mc_ownedfp == _MC_FPOWNED_PCB) {
		/*
		 * XXX we violate the dubious requirement that fpusetregs()
		 * be called with interrupts disabled.
		 */
		fpusetregs(td, (struct savefpu *)&mcp->mc_fpstate);
	} else
		return (EINVAL);
	return (0);
}

/*
 * Get machine context.
 */
static int
ia32_get_mcontext(struct thread *td, struct ia32_mcontext *mcp, int flags)
{
	struct trapframe *tp;

	tp = td->td_frame;

	PROC_LOCK(curthread->td_proc);
	mcp->mc_onstack = sigonstack(tp->tf_rsp);
	PROC_UNLOCK(curthread->td_proc);
	mcp->mc_gs = td->td_pcb->pcb_gs;
	mcp->mc_fs = td->td_pcb->pcb_fs;
	mcp->mc_es = td->td_pcb->pcb_es;
	mcp->mc_ds = td->td_pcb->pcb_ds;
	mcp->mc_edi = tp->tf_rdi;
	mcp->mc_esi = tp->tf_rsi;
	mcp->mc_ebp = tp->tf_rbp;
	mcp->mc_isp = tp->tf_rsp;
	if (flags & GET_MC_CLEAR_RET) {
		mcp->mc_eax = 0;
		mcp->mc_edx = 0;
	} else {
		mcp->mc_eax = tp->tf_rax;
		mcp->mc_edx = tp->tf_rdx;
	}
	mcp->mc_ebx = tp->tf_rbx;
	mcp->mc_ecx = tp->tf_rcx;
	mcp->mc_eip = tp->tf_rip;
	mcp->mc_cs = tp->tf_cs;
	mcp->mc_eflags = tp->tf_rflags;
	mcp->mc_esp = tp->tf_rsp;
	mcp->mc_ss = tp->tf_ss;
	mcp->mc_len = sizeof(*mcp);
	ia32_get_fpcontext(td, mcp);
	return (0);
}

/*
 * Set machine context.
 *
 * However, we don't set any but the user modifiable flags, and we won't
 * touch the cs selector.
 */
static int
ia32_set_mcontext(struct thread *td, const struct ia32_mcontext *mcp)
{
	struct trapframe *tp;
	long rflags;
	int ret;

	tp = td->td_frame;
	if (mcp->mc_len != sizeof(*mcp))
		return (EINVAL);
	rflags = (mcp->mc_eflags & PSL_USERCHANGE) |
	    (tp->tf_rflags & ~PSL_USERCHANGE);
	ret = ia32_set_fpcontext(td, mcp);
	if (ret != 0)
		return (ret);
#if 0	/* XXX deal with load_fs() and friends */
	tp->tf_fs = mcp->mc_fs;
	tp->tf_es = mcp->mc_es;
	tp->tf_ds = mcp->mc_ds;
#endif
	tp->tf_rdi = mcp->mc_edi;
	tp->tf_rsi = mcp->mc_esi;
	tp->tf_rbp = mcp->mc_ebp;
	tp->tf_rbx = mcp->mc_ebx;
	tp->tf_rdx = mcp->mc_edx;
	tp->tf_rcx = mcp->mc_ecx;
	tp->tf_rax = mcp->mc_eax;
	/* trapno, err */
	tp->tf_rip = mcp->mc_eip;
	tp->tf_rflags = rflags;
	tp->tf_rsp = mcp->mc_esp;
	tp->tf_ss = mcp->mc_ss;
#if 0	/* XXX deal with load_gs() and friends */
	td->td_pcb->pcb_gs = mcp->mc_gs;
#endif
	td->td_pcb->pcb_flags |= PCB_FULLCTX;
	return (0);
}

/*
 * The first two fields of a ucontext_t are the signal mask and
 * the machine context.  The next field is uc_link; we want to
 * avoid destroying the link when copying out contexts.
 */
#define	UC_COPY_SIZE	offsetof(struct ia32_ucontext, uc_link)

int
freebsd32_getcontext(struct thread *td, struct freebsd32_getcontext_args *uap)
{
	struct ia32_ucontext uc;
	int ret;

	if (uap->ucp == NULL)
		ret = EINVAL;
	else {
		ia32_get_mcontext(td, &uc.uc_mcontext, GET_MC_CLEAR_RET);
		PROC_LOCK(td->td_proc);
		uc.uc_sigmask = td->td_sigmask;
		PROC_UNLOCK(td->td_proc);
		ret = copyout(&uc, uap->ucp, UC_COPY_SIZE);
	}
	return (ret);
}

int
freebsd32_setcontext(struct thread *td, struct freebsd32_setcontext_args *uap)
{
	struct ia32_ucontext uc;
	int ret;	

	if (uap->ucp == NULL)
		ret = EINVAL;
	else {
		ret = copyin(uap->ucp, &uc, UC_COPY_SIZE);
		if (ret == 0) {
			ret = ia32_set_mcontext(td, &uc.uc_mcontext);
			if (ret == 0) {
				SIG_CANTMASK(uc.uc_sigmask);
				PROC_LOCK(td->td_proc);
				td->td_sigmask = uc.uc_sigmask;
				PROC_UNLOCK(td->td_proc);
			}
		}
	}
	return (ret == 0 ? EJUSTRETURN : ret);
}

int
freebsd32_swapcontext(struct thread *td, struct freebsd32_swapcontext_args *uap)
{
	struct ia32_ucontext uc;
	int ret;	

	if (uap->oucp == NULL || uap->ucp == NULL)
		ret = EINVAL;
	else {
		ia32_get_mcontext(td, &uc.uc_mcontext, GET_MC_CLEAR_RET);
		PROC_LOCK(td->td_proc);
		uc.uc_sigmask = td->td_sigmask;
		PROC_UNLOCK(td->td_proc);
		ret = copyout(&uc, uap->oucp, UC_COPY_SIZE);
		if (ret == 0) {
			ret = copyin(uap->ucp, &uc, UC_COPY_SIZE);
			if (ret == 0) {
				ret = ia32_set_mcontext(td, &uc.uc_mcontext);
				if (ret == 0) {
					SIG_CANTMASK(uc.uc_sigmask);
					PROC_LOCK(td->td_proc);
					td->td_sigmask = uc.uc_sigmask;
					PROC_UNLOCK(td->td_proc);
				}
			}
		}
	}
	return (ret == 0 ? EJUSTRETURN : ret);
}

/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored
 * at top to call routine, followed by kcall
 * to sigreturn routine below.  After sigreturn
 * resets the signal mask, the stack, and the
 * frame pointer, it returns to the user
 * specified pc, psl.
 */
#ifdef COMPAT_FREEBSD4
static void
freebsd4_ia32_sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct ia32_sigframe4 sf, *sfp;
	struct siginfo32 siginfo;
	struct proc *p;
	struct thread *td;
	struct sigacts *psp;
	struct trapframe *regs;
	int oonstack;
	int sig;

	td = curthread;
	p = td->td_proc;
	siginfo_to_siginfo32(&ksi->ksi_info, &siginfo);

	PROC_LOCK_ASSERT(p, MA_OWNED);
	sig = siginfo.si_signo;
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	regs = td->td_frame;
	oonstack = sigonstack(regs->tf_rsp);

	/* Save user context. */
	bzero(&sf, sizeof(sf));
	sf.sf_uc.uc_sigmask = *mask;
	sf.sf_uc.uc_stack.ss_sp = (uintptr_t)td->td_sigstk.ss_sp;
	sf.sf_uc.uc_stack.ss_size = td->td_sigstk.ss_size;
	sf.sf_uc.uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK)
	    ? ((oonstack) ? SS_ONSTACK : 0) : SS_DISABLE;
	sf.sf_uc.uc_mcontext.mc_onstack = (oonstack) ? 1 : 0;
	sf.sf_uc.uc_mcontext.mc_gs = rgs();
	sf.sf_uc.uc_mcontext.mc_fs = rfs();
	__asm __volatile("movl %%es,%0" : "=rm" (sf.sf_uc.uc_mcontext.mc_es));
	__asm __volatile("movl %%ds,%0" : "=rm" (sf.sf_uc.uc_mcontext.mc_ds));
	sf.sf_uc.uc_mcontext.mc_edi = regs->tf_rdi;
	sf.sf_uc.uc_mcontext.mc_esi = regs->tf_rsi;
	sf.sf_uc.uc_mcontext.mc_ebp = regs->tf_rbp;
	sf.sf_uc.uc_mcontext.mc_isp = regs->tf_rsp; /* XXX */
	sf.sf_uc.uc_mcontext.mc_ebx = regs->tf_rbx;
	sf.sf_uc.uc_mcontext.mc_edx = regs->tf_rdx;
	sf.sf_uc.uc_mcontext.mc_ecx = regs->tf_rcx;
	sf.sf_uc.uc_mcontext.mc_eax = regs->tf_rax;
	sf.sf_uc.uc_mcontext.mc_trapno = regs->tf_trapno;
	sf.sf_uc.uc_mcontext.mc_err = regs->tf_err;
	sf.sf_uc.uc_mcontext.mc_eip = regs->tf_rip;
	sf.sf_uc.uc_mcontext.mc_cs = regs->tf_cs;
	sf.sf_uc.uc_mcontext.mc_eflags = regs->tf_rflags;
	sf.sf_uc.uc_mcontext.mc_esp = regs->tf_rsp;
	sf.sf_uc.uc_mcontext.mc_ss = regs->tf_ss;

	/* Allocate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sfp = (struct ia32_sigframe4 *)(td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size - sizeof(sf));
	} else
		sfp = (struct ia32_sigframe4 *)regs->tf_rsp - 1;
	PROC_UNLOCK(p);

	/* Translate the signal if appropriate. */
	if (p->p_sysent->sv_sigtbl && sig <= p->p_sysent->sv_sigsize)
		sig = p->p_sysent->sv_sigtbl[_SIG_IDX(sig)];

	/* Build the argument list for the signal handler. */
	sf.sf_signum = sig;
	sf.sf_ucontext = (register_t)&sfp->sf_uc;
	if (SIGISMEMBER(psp->ps_siginfo, sig)) {
		/* Signal handler installed with SA_SIGINFO. */
		sf.sf_siginfo = (u_int32_t)(uintptr_t)&sfp->sf_si;
		sf.sf_ah = (u_int32_t)(uintptr_t)catcher;

		/* Fill in POSIX parts */
		sf.sf_si = siginfo;
		sf.sf_si.si_signo = sig;
	} else {
		/* Old FreeBSD-style arguments. */
		sf.sf_siginfo = siginfo.si_code;
		sf.sf_addr = (u_int32_t)siginfo.si_addr;
		sf.sf_ah = (u_int32_t)(uintptr_t)catcher;
	}
	mtx_unlock(&psp->ps_mtx);

	/*
	 * Copy the sigframe out to the user's stack.
	 */
	if (copyout(&sf, sfp, sizeof(*sfp)) != 0) {
#ifdef DEBUG
		printf("process %ld has trashed its stack\n", (long)p->p_pid);
#endif
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	regs->tf_rsp = (uintptr_t)sfp;
	regs->tf_rip = FREEBSD32_PS_STRINGS - sz_freebsd4_ia32_sigcode;
	regs->tf_rflags &= ~(PSL_T | PSL_D);
	regs->tf_cs = _ucode32sel;
	regs->tf_ss = _udatasel;
	load_ds(_udatasel);
	td->td_pcb->pcb_ds = _udatasel;
	load_es(_udatasel);
	td->td_pcb->pcb_es = _udatasel;
	/* leave user %fs and %gs untouched */
	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}
#endif	/* COMPAT_FREEBSD4 */

void
ia32_sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct ia32_sigframe sf, *sfp;
	struct siginfo32 siginfo;
	struct proc *p;
	struct thread *td;
	struct sigacts *psp;
	char *sp;
	struct trapframe *regs;
	int oonstack;
	int sig;

	siginfo_to_siginfo32(&ksi->ksi_info, &siginfo);
	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	sig = siginfo.si_signo;
	psp = p->p_sigacts;
#ifdef COMPAT_FREEBSD4
	if (SIGISMEMBER(psp->ps_freebsd4, sig)) {
		freebsd4_ia32_sendsig(catcher, ksi, mask);
		return;
	}
#endif
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	regs = td->td_frame;
	oonstack = sigonstack(regs->tf_rsp);

	/* Save user context. */
	bzero(&sf, sizeof(sf));
	sf.sf_uc.uc_sigmask = *mask;
	sf.sf_uc.uc_stack.ss_sp = (uintptr_t)td->td_sigstk.ss_sp;
	sf.sf_uc.uc_stack.ss_size = td->td_sigstk.ss_size;
	sf.sf_uc.uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK)
	    ? ((oonstack) ? SS_ONSTACK : 0) : SS_DISABLE;
	sf.sf_uc.uc_mcontext.mc_onstack = (oonstack) ? 1 : 0;
	sf.sf_uc.uc_mcontext.mc_gs = rgs();
	sf.sf_uc.uc_mcontext.mc_fs = rfs();
	__asm __volatile("movl %%es,%0" : "=rm" (sf.sf_uc.uc_mcontext.mc_es));
	__asm __volatile("movl %%ds,%0" : "=rm" (sf.sf_uc.uc_mcontext.mc_ds));
	sf.sf_uc.uc_mcontext.mc_edi = regs->tf_rdi;
	sf.sf_uc.uc_mcontext.mc_esi = regs->tf_rsi;
	sf.sf_uc.uc_mcontext.mc_ebp = regs->tf_rbp;
	sf.sf_uc.uc_mcontext.mc_isp = regs->tf_rsp; /* XXX */
	sf.sf_uc.uc_mcontext.mc_ebx = regs->tf_rbx;
	sf.sf_uc.uc_mcontext.mc_edx = regs->tf_rdx;
	sf.sf_uc.uc_mcontext.mc_ecx = regs->tf_rcx;
	sf.sf_uc.uc_mcontext.mc_eax = regs->tf_rax;
	sf.sf_uc.uc_mcontext.mc_trapno = regs->tf_trapno;
	sf.sf_uc.uc_mcontext.mc_err = regs->tf_err;
	sf.sf_uc.uc_mcontext.mc_eip = regs->tf_rip;
	sf.sf_uc.uc_mcontext.mc_cs = regs->tf_cs;
	sf.sf_uc.uc_mcontext.mc_eflags = regs->tf_rflags;
	sf.sf_uc.uc_mcontext.mc_esp = regs->tf_rsp;
	sf.sf_uc.uc_mcontext.mc_ss = regs->tf_ss;
	sf.sf_uc.uc_mcontext.mc_len = sizeof(sf.sf_uc.uc_mcontext); /* magic */
	ia32_get_fpcontext(td, &sf.sf_uc.uc_mcontext);
	fpstate_drop(td);

	/* Allocate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sp = td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size - sizeof(sf);
	} else
		sp = (char *)regs->tf_rsp - sizeof(sf);
	/* Align to 16 bytes. */
	sfp = (struct ia32_sigframe *)((uintptr_t)sp & ~0xF);
	PROC_UNLOCK(p);

	/* Translate the signal if appropriate. */
	if (p->p_sysent->sv_sigtbl && sig <= p->p_sysent->sv_sigsize)
		sig = p->p_sysent->sv_sigtbl[_SIG_IDX(sig)];

	/* Build the argument list for the signal handler. */
	sf.sf_signum = sig;
	sf.sf_ucontext = (register_t)&sfp->sf_uc;
	if (SIGISMEMBER(psp->ps_siginfo, sig)) {
		/* Signal handler installed with SA_SIGINFO. */
		sf.sf_siginfo = (u_int32_t)(uintptr_t)&sfp->sf_si;
		sf.sf_ah = (u_int32_t)(uintptr_t)catcher;

		/* Fill in POSIX parts */
		sf.sf_si = siginfo;
		sf.sf_si.si_signo = sig;
	} else {
		/* Old FreeBSD-style arguments. */
		sf.sf_siginfo = siginfo.si_code;
		sf.sf_addr = (u_int32_t)siginfo.si_addr;
		sf.sf_ah = (u_int32_t)(uintptr_t)catcher;
	}
	mtx_unlock(&psp->ps_mtx);

	/*
	 * Copy the sigframe out to the user's stack.
	 */
	if (copyout(&sf, sfp, sizeof(*sfp)) != 0) {
#ifdef DEBUG
		printf("process %ld has trashed its stack\n", (long)p->p_pid);
#endif
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	regs->tf_rsp = (uintptr_t)sfp;
	regs->tf_rip = FREEBSD32_PS_STRINGS - *(p->p_sysent->sv_szsigcode);
	regs->tf_rflags &= ~(PSL_T | PSL_D);
	regs->tf_cs = _ucode32sel;
	regs->tf_ss = _udatasel;
	load_ds(_udatasel);
	td->td_pcb->pcb_ds = _udatasel;
	load_es(_udatasel);
	td->td_pcb->pcb_es = _udatasel;
	/* leave user %fs and %gs untouched */
	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * state to gain improper privileges.
 */
#ifdef COMPAT_FREEBSD4
/*
 * MPSAFE
 */
int
freebsd4_freebsd32_sigreturn(td, uap)
	struct thread *td;
	struct freebsd4_freebsd32_sigreturn_args /* {
		const struct freebsd4_freebsd32_ucontext *sigcntxp;
	} */ *uap;
{
	struct ia32_ucontext4 uc;
	struct proc *p = td->td_proc;
	struct trapframe *regs;
	const struct ia32_ucontext4 *ucp;
	int cs, eflags, error;
	ksiginfo_t ksi;

	error = copyin(uap->sigcntxp, &uc, sizeof(uc));
	if (error != 0)
		return (error);
	ucp = &uc;
	regs = td->td_frame;
	eflags = ucp->uc_mcontext.mc_eflags;
	/*
	 * Don't allow users to change privileged or reserved flags.
	 */
	/*
	 * XXX do allow users to change the privileged flag PSL_RF.
	 * The cpu sets PSL_RF in tf_eflags for faults.  Debuggers
	 * should sometimes set it there too.  tf_eflags is kept in
	 * the signal context during signal handling and there is no
	 * other place to remember it, so the PSL_RF bit may be
	 * corrupted by the signal handler without us knowing.
	 * Corruption of the PSL_RF bit at worst causes one more or
	 * one less debugger trap, so allowing it is fairly harmless.
	 */
	if (!EFL_SECURE(eflags & ~PSL_RF, regs->tf_rflags & ~PSL_RF)) {
		printf("freebsd4_freebsd32_sigreturn: eflags = 0x%x\n", eflags);
		return (EINVAL);
	}

	/*
	 * Don't allow users to load a valid privileged %cs.  Let the
	 * hardware check for invalid selectors, excess privilege in
	 * other selectors, invalid %eip's and invalid %esp's.
	 */
	cs = ucp->uc_mcontext.mc_cs;
	if (!CS_SECURE(cs)) {
		printf("freebsd4_sigreturn: cs = 0x%x\n", cs);
		ksiginfo_init_trap(&ksi);
		ksi.ksi_signo = SIGBUS;
		ksi.ksi_code = BUS_OBJERR;
		ksi.ksi_trapno = T_PROTFLT;
		ksi.ksi_addr = (void *)regs->tf_rip;
		trapsignal(td, &ksi);
		return (EINVAL);
	}

	/* Segment selectors restored by sigtramp.S */
	regs->tf_rdi = ucp->uc_mcontext.mc_edi;
	regs->tf_rsi = ucp->uc_mcontext.mc_esi;
	regs->tf_rbp = ucp->uc_mcontext.mc_ebp;
	regs->tf_rbx = ucp->uc_mcontext.mc_ebx;
	regs->tf_rdx = ucp->uc_mcontext.mc_edx;
	regs->tf_rcx = ucp->uc_mcontext.mc_ecx;
	regs->tf_rax = ucp->uc_mcontext.mc_eax;
	regs->tf_trapno = ucp->uc_mcontext.mc_trapno;
	regs->tf_err = ucp->uc_mcontext.mc_err;
	regs->tf_rip = ucp->uc_mcontext.mc_eip;
	regs->tf_cs = cs;
	regs->tf_rflags = ucp->uc_mcontext.mc_eflags;
	regs->tf_rsp = ucp->uc_mcontext.mc_esp;
	regs->tf_ss = ucp->uc_mcontext.mc_ss;

	PROC_LOCK(p);
	td->td_sigmask = ucp->uc_sigmask;
	SIG_CANTMASK(td->td_sigmask);
	signotify(td);
	PROC_UNLOCK(p);
	return (EJUSTRETURN);
}
#endif	/* COMPAT_FREEBSD4 */

/*
 * MPSAFE
 */
int
freebsd32_sigreturn(td, uap)
	struct thread *td;
	struct freebsd32_sigreturn_args /* {
		const struct freebsd32_ucontext *sigcntxp;
	} */ *uap;
{
	struct ia32_ucontext uc;
	struct proc *p = td->td_proc;
	struct trapframe *regs;
	const struct ia32_ucontext *ucp;
	int cs, eflags, error, ret;
	ksiginfo_t ksi;

	error = copyin(uap->sigcntxp, &uc, sizeof(uc));
	if (error != 0)
		return (error);
	ucp = &uc;
	regs = td->td_frame;
	eflags = ucp->uc_mcontext.mc_eflags;
	/*
	 * Don't allow users to change privileged or reserved flags.
	 */
	/*
	 * XXX do allow users to change the privileged flag PSL_RF.
	 * The cpu sets PSL_RF in tf_eflags for faults.  Debuggers
	 * should sometimes set it there too.  tf_eflags is kept in
	 * the signal context during signal handling and there is no
	 * other place to remember it, so the PSL_RF bit may be
	 * corrupted by the signal handler without us knowing.
	 * Corruption of the PSL_RF bit at worst causes one more or
	 * one less debugger trap, so allowing it is fairly harmless.
	 */
	if (!EFL_SECURE(eflags & ~PSL_RF, regs->tf_rflags & ~PSL_RF)) {
		printf("freebsd32_sigreturn: eflags = 0x%x\n", eflags);
		return (EINVAL);
	}

	/*
	 * Don't allow users to load a valid privileged %cs.  Let the
	 * hardware check for invalid selectors, excess privilege in
	 * other selectors, invalid %eip's and invalid %esp's.
	 */
	cs = ucp->uc_mcontext.mc_cs;
	if (!CS_SECURE(cs)) {
		printf("sigreturn: cs = 0x%x\n", cs);
		ksiginfo_init_trap(&ksi);
		ksi.ksi_signo = SIGBUS;
		ksi.ksi_code = BUS_OBJERR;
		ksi.ksi_trapno = T_PROTFLT;
		ksi.ksi_addr = (void *)regs->tf_rip;
		trapsignal(td, &ksi);
		return (EINVAL);
	}

	ret = ia32_set_fpcontext(td, &ucp->uc_mcontext);
	if (ret != 0)
		return (ret);

	/* Segment selectors restored by sigtramp.S */
	regs->tf_rdi = ucp->uc_mcontext.mc_edi;
	regs->tf_rsi = ucp->uc_mcontext.mc_esi;
	regs->tf_rbp = ucp->uc_mcontext.mc_ebp;
	regs->tf_rbx = ucp->uc_mcontext.mc_ebx;
	regs->tf_rdx = ucp->uc_mcontext.mc_edx;
	regs->tf_rcx = ucp->uc_mcontext.mc_ecx;
	regs->tf_rax = ucp->uc_mcontext.mc_eax;
	regs->tf_trapno = ucp->uc_mcontext.mc_trapno;
	regs->tf_err = ucp->uc_mcontext.mc_err;
	regs->tf_rip = ucp->uc_mcontext.mc_eip;
	regs->tf_cs = cs;
	regs->tf_rflags = ucp->uc_mcontext.mc_eflags;
	regs->tf_rsp = ucp->uc_mcontext.mc_esp;
	regs->tf_ss = ucp->uc_mcontext.mc_ss;

	PROC_LOCK(p);
	td->td_sigmask = ucp->uc_sigmask;
	SIG_CANTMASK(td->td_sigmask);
	signotify(td);
	PROC_UNLOCK(p);
	return (EJUSTRETURN);
}

/*
 * Clear registers on exec
 */
void
ia32_setregs(td, entry, stack, ps_strings)
	struct thread *td;
	u_long entry;
	u_long stack;
	u_long ps_strings;
{
	struct trapframe *regs = td->td_frame;
	struct pcb *pcb = td->td_pcb;
	
	wrmsr(MSR_FSBASE, 0);
	wrmsr(MSR_KGSBASE, 0);	/* User value while we're in the kernel */
	pcb->pcb_fsbase = 0;
	pcb->pcb_gsbase = 0;
	load_ds(_udatasel);
	load_es(_udatasel);
	load_fs(_udatasel);
	load_gs(_udatasel);
	pcb->pcb_ds = _udatasel;
	pcb->pcb_es = _udatasel;
	pcb->pcb_fs = _udatasel;
	pcb->pcb_gs = _udatasel;
	pcb->pcb_initial_fpucw = __INITIAL_FPUCW_I386__;

	bzero((char *)regs, sizeof(struct trapframe));
	regs->tf_rip = entry;
	regs->tf_rsp = stack;
	regs->tf_rflags = PSL_USER | (regs->tf_rflags & PSL_T);
	regs->tf_ss = _udatasel;
	regs->tf_cs = _ucode32sel;
	regs->tf_rbx = ps_strings;
	fpstate_drop(td);

	/* Return via doreti so that we can change to a different %cs */
	pcb->pcb_flags |= PCB_FULLCTX | PCB_32BIT;
	pcb->pcb_flags &= ~PCB_GS32BIT;
	td->td_retval[1] = 0;
}
