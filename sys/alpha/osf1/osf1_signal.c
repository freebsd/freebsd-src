/*	$NetBSD: osf1_signal.c,v 1.4 1998/05/20 16:35:01 chs Exp $
 */

/*
 * Copyright (c) 1998-1999 Andrew Gallatin
 * 
 * Taken from NetBSD's sys/compat/osf1/osf1_signal.c, which at the
 * time *had no copyright*!
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/reboot.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/mbuf.h>
#include <sys/vmmeter.h>
#include <sys/msgbuf.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <net/netisr.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/reg.h>
#include <machine/pal.h>
#include <machine/cpuconf.h>
#include <machine/bootinfo.h>
#include <machine/rpb.h>
#include <machine/prom.h>
#include <machine/chipset.h>
#include <machine/vmparam.h>
#include <machine/elf.h>
#include <ddb/ddb.h>
#include <alpha/alpha/db_instruction.h>
#include <sys/vnode.h>

#include <alpha/osf1/osf1_signal.h>
#include <alpha/osf1/osf1_proto.h>
#include <alpha/osf1/osf1_syscall.h>
#include <alpha/osf1/osf1_util.h>
#include <alpha/osf1/osf1.h>
#include <sys/sysproto.h>

#define	DPRINTF uprintf
int osf1_sigdbg = 0;

static void bsd_to_osf1_sigaction(const struct sigaction *bsa,
					struct osf1_sigaction *osa);
static void osf1_to_bsd_sigaction(const struct osf1_sigaction *osa,
					struct sigaction *bsa);

#define	sigemptyset(s)		SIGEMPTYSET(*(s))
#define	sigismember(s, n)	SIGISMEMBER(*(s), n)
#define	sigaddset(s, n)		SIGADDSET(*(s), n)

#define	osf1_sigmask(n)		(1 << ((n) - 1))
#define	osf1_sigemptyset(s)	memset((s), 0, sizeof(*(s)))
#define	osf1_sigfillset(s)	memset((s), 0xffffffff, sizeof(*(s)))
#define	osf1_sigismember(s, n)	(*(s) & sigmask(n))
#define	osf1_sigaddset(s, n)	(*(s) |= sigmask(n))


void
osf1_to_bsd_sigset(oss, bss)
	const osf1_sigset_t *oss;
	sigset_t *bss;
{
	const u_int32_t *obits;

	SIGEMPTYSET(*bss);
	obits = (const u_int32_t *)oss;
	bss->__bits[0] = obits[0];
	bss->__bits[1] = obits[1];
}

void
bsd_to_osf1_sigset(bss, oss)
	const sigset_t *bss;
	osf1_sigset_t *oss;
{
	u_int32_t *obits;

	osf1_sigemptyset(oss);
	obits = (u_int32_t *)oss;
	obits[0] = bss->__bits[0];
	obits[1] = bss->__bits[1];
}

/*
 * XXX: Only a subset of the flags is currently implemented.
 */
void
osf1_to_bsd_sigaction(osa, bsa)
	const struct osf1_sigaction *osa;
	struct sigaction *bsa;
{

	bsa->sa_handler = osa->osa_handler;
	if (osf1_sigdbg)
		uprintf("%s(%d): handler @0x%lx \n", __FILE__, __LINE__,
			(unsigned long)osa->osa_handler);
	osf1_to_bsd_sigset(&osa->osa_mask, &bsa->sa_mask);
	bsa->sa_flags = 0;
	if ((osa->osa_flags & OSF1_SA_ONSTACK) != 0)
		bsa->sa_flags |= SA_ONSTACK;
	if ((osa->osa_flags & OSF1_SA_RESTART) != 0)
		bsa->sa_flags |= SA_RESTART;
	if ((osa->osa_flags & OSF1_SA_RESETHAND) != 0)
		bsa->sa_flags |= SA_RESETHAND;
	if ((osa->osa_flags & OSF1_SA_NOCLDSTOP) != 0)
		bsa->sa_flags |= SA_NOCLDSTOP;
	if ((osa->osa_flags & OSF1_SA_NODEFER) != 0)
		bsa->sa_flags |= SA_NODEFER;
}

void
bsd_to_osf1_sigaction(bsa, osa)
	const struct sigaction *bsa;
	struct osf1_sigaction *osa;
{

	osa->osa_handler = bsa->sa_handler;
	bsd_to_osf1_sigset(&bsa->sa_mask, &osa->osa_mask);
	osa->osa_flags = 0;
	if ((bsa->sa_flags & SA_ONSTACK) != 0)
		osa->osa_flags |= SA_ONSTACK;
	if ((bsa->sa_flags & SA_RESTART) != 0)
		osa->osa_flags |= SA_RESTART;
	if ((bsa->sa_flags & SA_NOCLDSTOP) != 0)
		osa->osa_flags |= SA_NOCLDSTOP;
	if ((bsa->sa_flags & SA_NODEFER) != 0)
		osa->osa_flags |= SA_NODEFER;
	if ((bsa->sa_flags & SA_RESETHAND) != 0)
		osa->osa_flags |= SA_RESETHAND;
}

void
osf1_to_bsd_sigaltstack(oss, bss)
	const struct osf1_sigaltstack *oss;
	struct sigaltstack *bss;
{

	bss->ss_sp = oss->ss_sp;
	bss->ss_size = oss->ss_size;
	bss->ss_flags = 0;

	if ((oss->ss_flags & OSF1_SS_DISABLE) != 0)
		bss->ss_flags |= SS_DISABLE;
	if ((oss->ss_flags & OSF1_SS_ONSTACK) != 0)
		bss->ss_flags |= SS_ONSTACK;
}

void
bsd_to_osf1_sigaltstack(bss, oss)
	const struct sigaltstack *bss;
	struct osf1_sigaltstack *oss;
{

	oss->ss_sp = bss->ss_sp;
	oss->ss_size = bss->ss_size;
	oss->ss_flags = 0;

	if ((bss->ss_flags & SS_DISABLE) != 0)
		oss->ss_flags |= OSF1_SS_DISABLE;
	if ((bss->ss_flags & SS_ONSTACK) != 0)
		oss->ss_flags |= OSF1_SS_ONSTACK;
}

int
osf1_sigaction(td, uap)
	struct thread *td;
	struct osf1_sigaction_args *uap;
{
	int error;
	caddr_t sg;
	struct osf1_sigaction *nosa, *oosa, tmposa;
	struct sigaction *nbsa, *obsa, tmpbsa;
	struct sigaction_args sa;

	sg = stackgap_init();
	nosa = SCARG(uap, nsa);
	oosa = SCARG(uap, osa);
	if (osf1_sigdbg && uap->sigtramp)
		uprintf("osf1_sigaction: trampoline handler at %p\n",
		    uap->sigtramp);
		td->td_md.osf_sigtramp = uap->sigtramp;
	if (oosa != NULL)
		obsa = stackgap_alloc(&sg, sizeof(struct sigaction));
	else
		obsa = NULL;
	if (nosa != NULL) {
		nbsa = stackgap_alloc(&sg, sizeof(struct sigaction));
		if ((error = copyin(nosa, &tmposa, sizeof(tmposa))) != 0)
			return error;
		osf1_to_bsd_sigaction(&tmposa, &tmpbsa);
		if ((error = copyout(&tmpbsa, nbsa, sizeof(tmpbsa))) != 0)
			return error;
	} else
		nbsa = NULL;

	SCARG(&sa, sig) = SCARG(uap, signum);
	SCARG(&sa, act) = nbsa;
	SCARG(&sa, oact) = obsa;

	if ((error = sigaction(td, &sa)) != 0)
		return error;

	if (oosa != NULL) {
		if ((error = copyin(obsa, &tmpbsa, sizeof(tmpbsa))) != 0)
			return error;
		bsd_to_osf1_sigaction(&tmpbsa, &tmposa);
		if ((error = copyout(&tmposa, oosa, sizeof(tmposa))) != 0)
			return error;
	}

	return 0;
}

int
osf1_sigaltstack(td, uap)
	register struct thread *td;
	struct osf1_sigaltstack_args *uap;
{
	int error;
	caddr_t sg;
	struct osf1_sigaltstack *noss, *ooss, tmposs;
	struct sigaltstack *nbss, *obss, tmpbss;
	struct sigaltstack_args sa;

	sg = stackgap_init();
	noss = SCARG(uap, nss);
	ooss = SCARG(uap, oss);

	if (ooss != NULL)
		obss = stackgap_alloc(&sg, sizeof(struct sigaltstack));
	else
		obss = NULL;

	if (noss != NULL) {
		nbss = stackgap_alloc(&sg, sizeof(struct sigaltstack));
		if ((error = copyin(noss, &tmposs, sizeof(tmposs))) != 0)
			return error;
		osf1_to_bsd_sigaltstack(&tmposs, &tmpbss);
		if ((error = copyout(&tmpbss, nbss, sizeof(tmpbss))) != 0)
			return error;
	} else
		nbss = NULL;

	SCARG(&sa, ss) = nbss;
	SCARG(&sa, oss) = obss;

	if ((error = sigaltstack(td, &sa)) != 0)
		return error;

	if (obss != NULL) {
		if ((error = copyin(obss, &tmpbss, sizeof(tmpbss))) != 0)
			return error;
		bsd_to_osf1_sigaltstack(&tmpbss, &tmposs);
		if ((error = copyout(&tmposs, ooss, sizeof(tmposs))) != 0)
			return error;
	}

	return 0;
}

int
osf1_signal(td, uap)
	register struct thread *td;
	struct osf1_signal_args *uap;
{
	struct proc *p;
	int error, signum;
	caddr_t sg;

	p = td->td_proc;
	sg = stackgap_init();

	signum = OSF1_SIGNO(SCARG(uap, signum));
	if (signum <= 0 || signum > OSF1_NSIG) {
		if (OSF1_SIGCALL(SCARG(uap, signum)) == OSF1_SIGNAL_MASK ||
		    OSF1_SIGCALL(SCARG(uap, signum)) == OSF1_SIGDEFER_MASK)
			td->td_retval[0] = -1;
		return EINVAL;
	}

	switch (OSF1_SIGCALL(SCARG(uap, signum))) {
	case OSF1_SIGDEFER_MASK:
		/*
		 * sigset is identical to signal() except
		 * that SIG_HOLD is allowed as
		 * an action.
		 */
		if ((u_long)SCARG(uap, handler) ==  OSF1_SIG_HOLD) {
			sigset_t mask;
			sigset_t *bmask;
			struct sigprocmask_args sa;

			bmask = stackgap_alloc(&sg, sizeof(sigset_t));
			SIGEMPTYSET(mask);
			SIGADDSET(mask, signum);
			SCARG(&sa, how) = SIG_BLOCK;
			SCARG(&sa, set) = bmask;
			SCARG(&sa, oset) = NULL;
			if ((error = copyout(&mask, bmask, sizeof(mask))) != 0)
				return (error);
			return sigprocmask(td, &sa);
		}
		/* FALLTHROUGH */

	case OSF1_SIGNAL_MASK:
		{
			struct sigaction_args sa_args;
			struct sigaction *nbsa, *obsa, sa;

			nbsa = stackgap_alloc(&sg, sizeof(struct sigaction));
			obsa = stackgap_alloc(&sg, sizeof(struct sigaction));
			SCARG(&sa_args, sig) = signum;
			SCARG(&sa_args, act) = nbsa;
			SCARG(&sa_args, oact) = obsa;

			sa.sa_handler = SCARG(uap, handler);
			SIGEMPTYSET(sa.sa_mask);
			sa.sa_flags = 0;
#if 0
			if (signum != SIGALRM)
				sa.sa_flags = SA_RESTART;
#endif
			if ((error = copyout(&sa, nbsa, sizeof(sa))) != 0)
				return error;
			if ((error = sigaction(td, &sa_args)) != 0) {
				DPRINTF("signal: sigaction failed: %d\n",
					 error);
				td->td_retval[0] = -1;
				return error;
			}
			if ((error = copyin(obsa, &sa, sizeof(sa))) != 0)
				return error;
			td->td_retval[0] = (long)sa.sa_handler;
			return 0;
		}

	case OSF1_SIGHOLD_MASK:
		{
			struct sigprocmask_args sa;
			sigset_t set;
			sigset_t *bset;

			bset = stackgap_alloc(&sg, sizeof(sigset_t));
			SIGEMPTYSET(set);
			SIGADDSET(set, signum);
			SCARG(&sa, how) = SIG_BLOCK;
			SCARG(&sa, set) = bset;
			SCARG(&sa, oset) = NULL;
			if ((error = copyout(&set, bset, sizeof(set))) != 0)
				return (error);
			return sigprocmask(td, &sa);
		}

	case OSF1_SIGRELSE_MASK:
		{
			struct sigprocmask_args sa;
			sigset_t set;
			sigset_t *bset;

			bset = stackgap_alloc(&sg, sizeof(sigset_t));
			SIGEMPTYSET(set);
			SIGADDSET(set, signum);
			SCARG(&sa, how) = SIG_UNBLOCK;
			SCARG(&sa, set) = bset;
			SCARG(&sa, oset) = NULL;
			if ((error = copyout(&set, bset, sizeof(set))) != 0)
				return (error);
			return sigprocmask(td, &sa);

		}

	case OSF1_SIGIGNORE_MASK:
		{
			struct sigaction_args sa_args;
			struct sigaction *bsa, sa;

			bsa = stackgap_alloc(&sg, sizeof(struct sigaction));
			SCARG(&sa_args, sig) = signum;
			SCARG(&sa_args, act) = bsa;
			SCARG(&sa_args, oact) = NULL;

			sa.sa_handler = SIG_IGN;
			SIGEMPTYSET(sa.sa_mask);
			sa.sa_flags = 0;
			if ((error = copyout(&sa, bsa, sizeof(sa))) != 0)
				return error;
			if ((error = sigaction(td, &sa_args)) != 0) {
				DPRINTF(("sigignore: sigaction failed\n"));
				return error;
			}
			return 0;
		}

	case OSF1_SIGPAUSE_MASK:
		{
			struct sigsuspend_args sa;
			sigset_t set;
			sigset_t *bmask;

			bmask = stackgap_alloc(&sg, sizeof(sigset_t));
			PROC_LOCK(p);
			set = p->p_sigmask;
			PROC_UNLOCK(p);
			SIGDELSET(set, signum);
			SCARG(&sa, sigmask) = bmask;
			if ((error = copyout(&set, bmask, sizeof(set))) != 0)
				return (error);
			return sigsuspend(td, &sa);
		}

	default:
		return ENOSYS;
	}
}

int
osf1_sigprocmask(td, uap)
	register struct thread *td;
	struct osf1_sigprocmask_args /* {
		syscallarg(int) how;
		syscallarg(osf1_sigset_t *) set;
	} */ *uap;
{
	struct proc *p;
	int error;
	osf1_sigset_t oss;
	sigset_t bss;

	p = td->td_proc;
	error = 0;
		/* Fix the return value first if needed */
	bsd_to_osf1_sigset(&p->p_sigmask, &oss);
	td->td_retval[0] = oss;

	osf1_to_bsd_sigset(&uap->mask, &bss);

	PROC_LOCK(p);

	switch (SCARG(uap, how)) {
	case OSF1_SIG_BLOCK:
		SIGSETOR(p->p_sigmask, bss);
		SIG_CANTMASK(p->p_sigmask);
		break;

	case OSF1_SIG_UNBLOCK:
		SIGSETNAND(p->p_sigmask, bss);
		break;

	case OSF1_SIG_SETMASK:
		p->p_sigmask = bss;
		SIG_CANTMASK(p->p_sigmask);
		break;

	default:
		error = EINVAL;
		break;
	}

	PROC_UNLOCK(p);

	return error;
}

int
osf1_sigpending(td, uap)
	register struct thread *td;
	struct osf1_sigpending_args /* {
		syscallarg(osf1_sigset_t *) mask;
	} */ *uap;
{
	struct proc *p;
	osf1_sigset_t oss;
	sigset_t bss;

	p = td->td_proc;
	PROC_LOCK(p);
	bss = p->p_siglist;
	SIGSETAND(bss, p->p_sigmask);
	PROC_UNLOCK(p);
	bsd_to_osf1_sigset(&bss, &oss);

	return copyout(&oss, SCARG(uap, mask), sizeof(oss));
}

int
osf1_sigsuspend(td, uap)
	register struct thread *td;
	struct osf1_sigsuspend_args /* {
		syscallarg(osf1_sigset_t *) ss;
	} */ *uap;
{
	int error;
	caddr_t sg;
	osf1_sigset_t oss;
	sigset_t bss;
	sigset_t *bmask;
	struct sigsuspend_args sa;

	sg = stackgap_init();

	bmask = stackgap_alloc(&sg, sizeof(sigset_t));
	oss = SCARG(uap, ss);
	osf1_to_bsd_sigset(&oss, &bss);
	SCARG(&sa, sigmask) = bmask;
	if ((error = copyout(&bss, bmask, sizeof(bss))) != 0)
		return (error);
	return sigsuspend(td, &sa);
}

int
osf1_kill(td, uap)
	register struct thread *td;
	struct osf1_kill_args /* {
		syscallarg(int) pid;
		syscallarg(int) signum;
	} */ *uap;
{
	struct kill_args ka;

	SCARG(&ka, pid) = SCARG(uap, pid);
	SCARG(&ka, signum) = SCARG(uap, signum);
	return kill(td, &ka);
}


/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored at top to call routine,
 * followed by kcall to sigreturn routine below.  After sigreturn resets
 * the signal mask, the stack, and the frame pointer, it returns to the
 * user specified pc, psl.
 */

void
osf1_sendsig(sig_t catcher, int sig, sigset_t *mask, u_long code)
{
	int fsize, oonstack, rndfsize;
	struct thread *td;
	struct proc *p;
	osiginfo_t *sip, ksi;
	struct trapframe *frame;
	struct sigacts *psp;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	psp = p->p_sigacts;

	frame = td->td_frame;
	oonstack = sigonstack(alpha_pal_rdusp());
	fsize = sizeof ksi;
	rndfsize = ((fsize + 15) / 16) * 16;

	/*
	 * Allocate and validate space for the signal handler context.
	 * Note that if the stack is in P0 space, the call to grow() is a nop,
	 * and the useracc() check will fail if the process has not already
	 * allocated the space with a `brk'.
	 */
	if ((p->p_flag & P_ALTSTACK) && !oonstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		sip = (osiginfo_t *)((caddr_t)p->p_sigstk.ss_sp +
		    p->p_sigstk.ss_size - rndfsize);
		p->p_sigstk.ss_flags |= SS_ONSTACK;
	} else
		sip = (osiginfo_t *)(alpha_pal_rdusp() - rndfsize);
	PROC_UNLOCK(p);

	(void)grow_stack(p, (u_long)sip);
	if (useracc((caddr_t)sip, fsize, VM_PROT_WRITE) == 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		PROC_LOCK(p);
		SIGACTION(p, SIGILL) = SIG_DFL;
		SIGDELSET(p->p_sigignore, SIGILL);
		SIGDELSET(p->p_sigcatch, SIGILL);
		SIGDELSET(p->p_sigmask, SIGILL);
		psignal(p, SIGILL);
		return;
	}

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	ksi.si_sc.sc_onstack = (oonstack) ? 1 : 0;
	bsd_to_osf1_sigset(mask, &ksi.si_sc.sc_mask);
	ksi.si_sc.sc_pc = frame->tf_regs[FRAME_PC];
	ksi.si_sc.sc_ps = frame->tf_regs[FRAME_PS];

	/* copy the registers. */
	fill_regs(td, (struct reg *)ksi.si_sc.sc_regs);
	ksi.si_sc.sc_regs[R_ZERO] = 0xACEDBADE;		/* magic number */
	ksi.si_sc.sc_regs[R_SP] = alpha_pal_rdusp();

	/* save the floating-point state, if necessary, then copy it. */
	alpha_fpstate_save(td, 1);		/* XXX maybe write=0 */
	ksi.si_sc.sc_ownedfp = td->td_md.md_flags & MDP_FPUSED;
	bcopy(&td->td_pcb->pcb_fp, (struct fpreg *)ksi.si_sc.sc_fpregs,
	    sizeof(struct fpreg));
	ksi.si_sc.sc_fp_control = td->td_pcb->pcb_fp_control;
	bzero(ksi.si_sc.sc_reserved, sizeof ksi.si_sc.sc_reserved); /* XXX */
	ksi.si_sc.sc_xxx1[0] = 0;				/* XXX */
	ksi.si_sc.sc_xxx1[1] = 0;				/* XXX */
	ksi.si_sc.sc_traparg_a0 = frame->tf_regs[FRAME_TRAPARG_A0];
	ksi.si_sc.sc_traparg_a1 = frame->tf_regs[FRAME_TRAPARG_A1];
	ksi.si_sc.sc_traparg_a2 = frame->tf_regs[FRAME_TRAPARG_A2];
	ksi.si_sc.sc_xxx2[0] = 0;				/* XXX */
	ksi.si_sc.sc_xxx2[1] = 0;				/* XXX */
	ksi.si_sc.sc_xxx2[2] = 0;				/* XXX */
	/* Fill in POSIX parts */
	ksi.si_signo = sig;
	ksi.si_code = code;
	ksi.si_value.sigval_ptr = NULL;				/* XXX */

	/*
	 * copy the frame out to userland.
	 */
	(void) copyout((caddr_t)&ksi, (caddr_t)sip, fsize);

	/*
	 * Set up the registers to return to sigcode.
	 */
	if (osf1_sigdbg)
		uprintf("attempting to call osf1 sigtramp\n");
	frame->tf_regs[FRAME_PC] = (u_int64_t)td->td_md.osf_sigtramp;
	frame->tf_regs[FRAME_A0] = sig;
	frame->tf_regs[FRAME_A1] = code;
	frame->tf_regs[FRAME_A2] = (u_int64_t)sip;
	frame->tf_regs[FRAME_A3]  = (u_int64_t)catcher;		/* a3 is pv */
	frame->tf_regs[FRAME_FLAGS] = 0;   	/* full restore */
	alpha_pal_wrusp((unsigned long)sip);
	PROC_LOCK(p);
}


/*
 * System call to cleanup state after a signal has been taken.  Reset signal
 * mask and stack state from context left by sendsig (above).  Return to
 * previous pc and psl as specified by context left by sendsig. Check
 * carefully to make sure that the user has not modified the state to gain
 * improper privileges.
 */
int
osf1_sigreturn(struct thread *td,
	struct osf1_sigreturn_args /* {
		struct osigcontext *sigcntxp;
	} */ *uap)
{
	struct osigcontext ksc, *scp;
	struct proc *p;

	p = td->td_proc;
	scp = uap->sigcntxp;
	if (useracc((caddr_t)scp, sizeof (*scp), VM_PROT_READ) == 0 ) {
		uprintf("uac fails\n");
		uprintf("scp: %p\n", scp);
	}
	/*
	 * Test and fetch the context structure.
	 * We grab it all at once for speed.
	 */
	if (useracc((caddr_t)scp, sizeof (*scp), VM_PROT_READ) == 0 ||
	    copyin((caddr_t)scp, (caddr_t)&ksc, sizeof ksc))
		return (EFAULT);

	/*
	 * Restore the user-supplied information.
	 */
	PROC_LOCK(p);
	if (ksc.sc_onstack)
		p->p_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigstk.ss_flags &= ~SS_ONSTACK;

	/*
	 * longjmp is still implemented by calling osigreturn. The new
	 * sigmask is stored in sc_reserved, sc_mask is only used for
	 * backward compatibility.
	 */
	osf1_to_bsd_sigset(&ksc.sc_mask, &p->p_sigmask);
	SIG_CANTMASK(p->p_sigmask);
	PROC_UNLOCK(p);

	set_regs(td, (struct reg *)ksc.sc_regs);
	td->td_frame->tf_regs[FRAME_PC] = ksc.sc_pc;
	td->td_frame->tf_regs[FRAME_PS] =
	    (ksc.sc_ps | ALPHA_PSL_USERSET) & ~ALPHA_PSL_USERCLR;
	td->td_frame->tf_regs[FRAME_FLAGS] = 0; /* full restore */

	alpha_pal_wrusp(ksc.sc_regs[R_SP]);

	/* XXX ksc.sc_ownedfp ? */
	alpha_fpstate_drop(td);
	bcopy((struct fpreg *)ksc.sc_fpregs, &td->td_pcb->pcb_fp,
	    sizeof(struct fpreg));
	td->td_pcb->pcb_fp_control = ksc.sc_fp_control;
	return (EJUSTRETURN);
}

extern int
osigstack(struct thread *td, struct osf1_osigstack_args *uap);

int
osf1_osigstack(td, uap)
	register struct thread *td;
	struct osf1_osigstack_args /* {
					struct sigstack *nss;
					struct sigstack *oss;
	} */ *uap;
{

/*	uprintf("osf1_osigstack: oss = %p, nss = %p",uap->oss, uap->nss);
	uprintf(" stack ptr = %p\n",p->p_sigacts->ps_sigstk.ss_sp);*/
	return(osigstack(td, uap));
}
