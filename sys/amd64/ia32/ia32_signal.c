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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
static void freebsd4_ia32_sendsig(sig_t, int, sigset_t *, u_long);
#endif
static void ia32_get_fpcontext(struct thread *td, struct ia32_mcontext *mcp);
static int ia32_set_fpcontext(struct thread *td, const struct ia32_mcontext *mcp);

extern int _ucode32sel, _udatasel;

#define	CS_SECURE(cs)		(ISPL(cs) == SEL_UPL)
#define	EFL_SECURE(ef, oef)	((((ef) ^ (oef)) & ~PSL_USERCHANGE) == 0)

static void
ia32_get_fpcontext(struct thread *td, struct ia32_mcontext *mcp)
{
	struct savefpu *addr;

	/*
	 * XXX mc_fpstate might be misaligned, since its declaration is not
	 * unportabilized using __attribute__((aligned(16))) like the
	 * declaration of struct savemm, and anyway, alignment doesn't work
	 * for auto variables since we don't use gcc's pessimal stack
	 * alignment.  Work around this by abusing the spare fields after
	 * mcp->mc_fpstate.
	 *
	 * XXX unpessimize most cases by only aligning when fxsave might be
	 * called, although this requires knowing too much about
	 * npxgetregs()'s internals.
	 */
	addr = (struct savefpu *)&mcp->mc_fpstate;
	if (td == PCPU_GET(fpcurthread) && ((uintptr_t)(void *)addr & 0xF)) {
		do
			addr = (void *)((char *)addr + 4);
		while ((uintptr_t)(void *)addr & 0xF);
	}
	mcp->mc_ownedfp = npxgetregs(td, addr);
	if (addr != (struct savefpu *)&mcp->mc_fpstate) {
		bcopy(addr, &mcp->mc_fpstate, sizeof(mcp->mc_fpstate));
		bzero(&mcp->mc_spare2, sizeof(mcp->mc_spare2));
	}
	mcp->mc_fpformat = npxformat();
}

static int
ia32_set_fpcontext(struct thread *td, const struct ia32_mcontext *mcp)
{
	struct savefpu *addr;

	if (mcp->mc_fpformat == _MC_FPFMT_NODEV)
		return (0);
	else if (mcp->mc_fpformat != _MC_FPFMT_XMM)
		return (EINVAL);
	else if (mcp->mc_ownedfp == _MC_FPOWNED_NONE)
		/* We don't care what state is left in the FPU or PCB. */
		fpstate_drop(td);
	else if (mcp->mc_ownedfp == _MC_FPOWNED_FPU ||
	    mcp->mc_ownedfp == _MC_FPOWNED_PCB) {
		/* XXX align as above. */
		addr = (struct savefpu *)&mcp->mc_fpstate;
		if (td == PCPU_GET(fpcurthread) &&
		    ((uintptr_t)(void *)addr & 0xF)) {
			do
				addr = (void *)((char *)addr + 4);
			while ((uintptr_t)(void *)addr & 0xF);
			bcopy(&mcp->mc_fpstate, addr, sizeof(mcp->mc_fpstate));
		}
		/*
		 * XXX we violate the dubious requirement that npxsetregs()
		 * be called with interrupts disabled.
		 */
		npxsetregs(td, addr);
		/*
		 * Don't bother putting things back where they were in the
		 * misaligned case, since we know that the caller won't use
		 * them again.
		 */
	} else
		return (EINVAL);
	return (0);
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
freebsd4_ia32_sendsig(sig_t catcher, int sig, sigset_t *mask, u_long code)
{
	struct ia32_sigframe4 sf, *sfp;
	struct proc *p;
	struct thread *td;
	struct sigacts *psp;
	struct trapframe *regs;
	int oonstack;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	psp = p->p_sigacts;
	regs = td->td_frame;
	oonstack = sigonstack(regs->tf_rsp);

	/* Save user context. */
	bzero(&sf, sizeof(sf));
	sf.sf_uc.uc_sigmask = *mask;
	sf.sf_uc.uc_stack.ss_sp = (uintptr_t)p->p_sigstk.ss_sp;
	sf.sf_uc.uc_stack.ss_size = p->p_sigstk.ss_size;
	sf.sf_uc.uc_stack.ss_flags = (p->p_flag & P_ALTSTACK)
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
	if ((p->p_flag & P_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sfp = (struct ia32_sigframe4 *)(p->p_sigstk.ss_sp +
		    p->p_sigstk.ss_size - sizeof(sf));
	} else
		sfp = (struct ia32_sigframe4 *)regs->tf_rsp - 1;
	PROC_UNLOCK(p);

	/* Translate the signal if appropriate. */
	if (p->p_sysent->sv_sigtbl && sig <= p->p_sysent->sv_sigsize)
		sig = p->p_sysent->sv_sigtbl[_SIG_IDX(sig)];

	/* Build the argument list for the signal handler. */
	sf.sf_signum = sig;
	sf.sf_ucontext = (register_t)&sfp->sf_uc;
	PROC_LOCK(p);
	if (SIGISMEMBER(p->p_sigacts->ps_siginfo, sig)) {
		/* Signal handler installed with SA_SIGINFO. */
		sf.sf_siginfo = (u_int32_t)(uintptr_t)&sfp->sf_si;
		sf.sf_ah = (u_int32_t)(uintptr_t)catcher;

		/* Fill in POSIX parts */
		sf.sf_si.si_signo = sig;
		sf.sf_si.si_code = code;
		sf.sf_si.si_addr = regs->tf_addr;
	} else {
		/* Old FreeBSD-style arguments. */
		sf.sf_siginfo = code;
		sf.sf_addr = regs->tf_addr;
		sf.sf_ah = (u_int32_t)(uintptr_t)catcher;
	}
	PROC_UNLOCK(p);

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
	regs->tf_rflags &= ~PSL_T;
	regs->tf_cs = _ucode32sel;
	regs->tf_ss = _udatasel;
	load_ds(_udatasel);
	td->td_pcb->pcb_ds = _udatasel;
	load_es(_udatasel);
	td->td_pcb->pcb_es = _udatasel;
	/* leave user %fs and %gs untouched */
	PROC_LOCK(p);
}
#endif	/* COMPAT_FREEBSD4 */

void
ia32_sendsig(sig_t catcher, int sig, sigset_t *mask, u_long code)
{
	struct ia32_sigframe sf, *sfp;
	struct proc *p;
	struct thread *td;
	struct sigacts *psp;
	char *sp;
	struct trapframe *regs;
	int oonstack;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	psp = p->p_sigacts;
#ifdef COMPAT_FREEBSD4
	if (SIGISMEMBER(psp->ps_freebsd4, sig)) {
		freebsd4_ia32_sendsig(catcher, sig, mask, code);
		return;
	}
#endif
	regs = td->td_frame;
	oonstack = sigonstack(regs->tf_rsp);

	/* Save user context. */
	bzero(&sf, sizeof(sf));
	sf.sf_uc.uc_sigmask = *mask;
	sf.sf_uc.uc_stack.ss_sp = (uintptr_t)p->p_sigstk.ss_sp;
	sf.sf_uc.uc_stack.ss_size = p->p_sigstk.ss_size;
	sf.sf_uc.uc_stack.ss_flags = (p->p_flag & P_ALTSTACK)
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
	if ((p->p_flag & P_ALTSTACK) != 0 && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sp = p->p_sigstk.ss_sp +
		    p->p_sigstk.ss_size - sizeof(sf);
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
	PROC_LOCK(p);
	if (SIGISMEMBER(p->p_sigacts->ps_siginfo, sig)) {
		/* Signal handler installed with SA_SIGINFO. */
		sf.sf_siginfo = (u_int32_t)(uintptr_t)&sfp->sf_si;
		sf.sf_ah = (u_int32_t)(uintptr_t)catcher;

		/* Fill in POSIX parts */
		sf.sf_si.si_signo = sig;
		sf.sf_si.si_code = code;
		sf.sf_si.si_addr = regs->tf_addr;
	} else {
		/* Old FreeBSD-style arguments. */
		sf.sf_siginfo = code;
		sf.sf_addr = regs->tf_addr;
		sf.sf_ah = (u_int32_t)(uintptr_t)catcher;
	}
	PROC_UNLOCK(p);

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
	regs->tf_rflags &= ~PSL_T;
	regs->tf_cs = _ucode32sel;
	regs->tf_ss = _udatasel;
	load_ds(_udatasel);
	td->td_pcb->pcb_ds = _udatasel;
	load_es(_udatasel);
	td->td_pcb->pcb_es = _udatasel;
	/* leave user %fs and %gs untouched */
	PROC_LOCK(p);
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
		trapsignal(td, SIGBUS, T_PROTFLT);
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
		trapsignal(td, SIGBUS, T_PROTFLT);
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
