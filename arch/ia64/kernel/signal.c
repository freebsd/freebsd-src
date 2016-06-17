/*
 * Architecture-specific signal handling support.
 *
 * Copyright (C) 1999-2002 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * Derived from i386 and Alpha versions.
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/wait.h>

#include <asm/ia32.h>
#include <asm/uaccess.h>
#include <asm/rse.h>
#include <asm/sigcontext.h>

#include "sigframe.h"

#define DEBUG_SIG	0
#define STACK_ALIGN	16		/* minimal alignment for stack pointer */
#define _BLOCKABLE	(~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

#if _NSIG_WORDS > 1
# define PUT_SIGSET(k,u)	__copy_to_user((u)->sig, (k)->sig, sizeof(sigset_t))
# define GET_SIGSET(k,u)	__copy_from_user((k)->sig, (u)->sig, sizeof(sigset_t))
#else
# define PUT_SIGSET(k,u)	__put_user((k)->sig[0], &(u)->sig[0])
# define GET_SIGSET(k,u)	__get_user((k)->sig[0], &(u)->sig[0])
#endif

extern long ia64_do_signal (sigset_t *, struct sigscratch *, long);	/* forward decl */

register double f16 asm ("f16"); register double f17 asm ("f17");
register double f18 asm ("f18"); register double f19 asm ("f19");
register double f20 asm ("f20"); register double f21 asm ("f21");
register double f22 asm ("f22"); register double f23 asm ("f23");

register double f24 asm ("f24"); register double f25 asm ("f25");
register double f26 asm ("f26"); register double f27 asm ("f27");
register double f28 asm ("f28"); register double f29 asm ("f29");
register double f30 asm ("f30"); register double f31 asm ("f31");

long
ia64_rt_sigsuspend (sigset_t *uset, size_t sigsetsize, struct sigscratch *scr)
{
	sigset_t oldset, set;

	/* XXX: Don't preclude handling different sized sigset_t's.  */
	if (sigsetsize != sizeof(sigset_t))
		return -EINVAL;

	if (!access_ok(VERIFY_READ, uset, sigsetsize))
		return -EFAULT;

	if (GET_SIGSET(&set, uset))
		return -EFAULT;

	sigdelsetmask(&set, ~_BLOCKABLE);

	spin_lock_irq(&current->sigmask_lock);
	{
		oldset = current->blocked;
		current->blocked = set;
		recalc_sigpending(current);
	}
	spin_unlock_irq(&current->sigmask_lock);

	/*
	 * The return below usually returns to the signal handler.  We need to
	 * pre-set the correct error code here to ensure that the right values
	 * get saved in sigcontext by ia64_do_signal.
	 */
	scr->pt.r8 = EINTR;
	scr->pt.r10 = -1;

	while (1) {
		current->state = TASK_INTERRUPTIBLE;
		schedule();
		if (ia64_do_signal(&oldset, scr, 1))
			return -EINTR;
	}
}

asmlinkage long
sys_sigaltstack (const stack_t *uss, stack_t *uoss, long arg2, long arg3, long arg4,
		 long arg5, long arg6, long arg7, long stack)
{
	struct pt_regs *pt = (struct pt_regs *) &stack;

	return do_sigaltstack(uss, uoss, pt->r12);
}

static long
restore_sigcontext (struct sigcontext *sc, struct sigscratch *scr)
{
	unsigned long ip, flags, nat, um, cfm;
	long err;

	/* restore scratch that always needs gets updated during signal delivery: */
	err = __get_user(flags, &sc->sc_flags);

	err |= __get_user(nat, &sc->sc_nat);
	err |= __get_user(ip, &sc->sc_ip);			/* instruction pointer */
	err |= __get_user(cfm, &sc->sc_cfm);
	err |= __get_user(um, &sc->sc_um);			/* user mask */
	err |= __get_user(scr->pt.ar_rsc, &sc->sc_ar_rsc);
	err |= __get_user(scr->pt.ar_unat, &sc->sc_ar_unat);
	err |= __get_user(scr->pt.ar_fpsr, &sc->sc_ar_fpsr);
	err |= __get_user(scr->pt.ar_pfs, &sc->sc_ar_pfs);
	err |= __get_user(scr->pt.pr, &sc->sc_pr);		/* predicates */
	err |= __get_user(scr->pt.b0, &sc->sc_br[0]);		/* b0 (rp) */
	err |= __copy_from_user(&scr->pt.r1, &sc->sc_gr[1], 8);	/* r1 */
	err |= __copy_from_user(&scr->pt.r8, &sc->sc_gr[8], 4*8);	/* r8-r11 */
	err |= __copy_from_user(&scr->pt.r12, &sc->sc_gr[12], 2*8);	/* r12-r13 */
	err |= __copy_from_user(&scr->pt.r15, &sc->sc_gr[15], 8);	/* r15 */
	
	if ((flags & IA64_SC_FLAG_IN_SYSCALL)==0)
	{
		// Only get user sig context when not in syscall. 
		err |= __get_user(scr->pt.ar_ccv, &sc->sc_ar_ccv);
		err |= __get_user(scr->pt.ar_csd, &sc->sc_ar25);	/* ar.csd */
		err |= __get_user(scr->pt.ar_ssd, &sc->sc_ar26);	/* ar.ssd */
		err |= __get_user(scr->pt.b6, &sc->sc_br[6]);		/* b6 */
		err |= __get_user(scr->pt.b7, &sc->sc_br[7]);		/* b7 */
		err |= __copy_from_user(&scr->pt.r2, &sc->sc_gr[2], 2*8);	/* r2-r3 */
		err |= __copy_from_user(&scr->pt.r14, &sc->sc_gr[14], 8);	/* r14 */
		err |= __copy_from_user(&scr->pt.r16, &sc->sc_gr[16], 16*8);	/* r16-r31 */
	}

	scr->pt.cr_ifs = cfm | (1UL << 63);

	/* establish new instruction pointer: */
	scr->pt.cr_iip = ip & ~0x3UL;
	ia64_psr(&scr->pt)->ri = ip & 0x3;
	scr->pt.cr_ipsr = (scr->pt.cr_ipsr & ~IA64_PSR_UM) | (um & IA64_PSR_UM);

	scr->scratch_unat = ia64_put_scratch_nat_bits(&scr->pt, nat);

	if ((flags & IA64_SC_FLAG_FPH_VALID) != 0) {
		struct ia64_psr *psr = ia64_psr(&scr->pt);

		__copy_from_user(current->thread.fph, &sc->sc_fr[32], 96*16);
		psr->mfh = 0;	/* drop signal handler's fph contents... */
		if (psr->dfh)
			ia64_drop_fpu(current);
		else {
			/* We already own the local fph, otherwise psr->dfh wouldn't be 0.  */
			__ia64_load_fpu(current->thread.fph);
			ia64_set_local_fpu_owner(current);
		}
	}
	return err;
}

int
copy_siginfo_to_user (siginfo_t *to, siginfo_t *from)
{
	if (!access_ok(VERIFY_WRITE, to, sizeof(siginfo_t)))
		return -EFAULT;
	if (from->si_code < 0) {
		if (__copy_to_user(to, from, sizeof(siginfo_t)))
			return -EFAULT;
		return 0;
	} else {
		int err;

		/*
		 * If you change siginfo_t structure, please be sure
		 * this code is fixed accordingly.  It should never
		 * copy any pad contained in the structure to avoid
		 * security leaks, but must copy the generic 3 ints
		 * plus the relevant union member.
		 */
		err = __put_user(from->si_signo, &to->si_signo);
		err |= __put_user(from->si_errno, &to->si_errno);
		err |= __put_user((short)from->si_code, &to->si_code);
		switch (from->si_code >> 16) {
		      case __SI_FAULT >> 16:
			err |= __put_user(from->si_flags, &to->si_flags);
			err |= __put_user(from->si_isr, &to->si_isr);
		      case __SI_POLL >> 16:
			err |= __put_user(from->si_addr, &to->si_addr);
			err |= __put_user(from->si_imm, &to->si_imm);
			break;
		      case __SI_CHLD >> 16:
			err |= __put_user(from->si_utime, &to->si_utime);
			err |= __put_user(from->si_stime, &to->si_stime);
			err |= __put_user(from->si_status, &to->si_status);
		      case __SI_PROF >> 16:
			err |= __put_user(from->si_uid, &to->si_uid);
			err |= __put_user(from->si_pid, &to->si_pid);
			if (from->si_code == PROF_OVFL) {
				err |= __put_user(from->si_pfm_ovfl[0], &to->si_pfm_ovfl[0]);
				err |= __put_user(from->si_pfm_ovfl[1], &to->si_pfm_ovfl[1]);
				err |= __put_user(from->si_pfm_ovfl[2], &to->si_pfm_ovfl[2]);
				err |= __put_user(from->si_pfm_ovfl[3], &to->si_pfm_ovfl[3]);
			}
			break;
		      default:
			err |= __put_user(from->si_uid, &to->si_uid);
			err |= __put_user(from->si_pid, &to->si_pid);
			break;
		      /* case __SI_RT: This is not generated by the kernel as of now.  */
		}
		return err;
	}
}

int
copy_siginfo_from_user (siginfo_t *to, siginfo_t *from)
{
	if (!access_ok(VERIFY_READ, from, sizeof(siginfo_t)))
		return -EFAULT;
	if (__copy_from_user(to, from, sizeof(siginfo_t)) != 0)
		return -EFAULT;

	if (SI_FROMUSER(to))
		return 0;

	to->si_code &= ~__SI_MASK;
	if (to->si_code != 0) {
		switch (to->si_signo) {
		      case SIGILL: case SIGFPE: case SIGSEGV: case SIGBUS: case SIGTRAP:
			to->si_code |= __SI_FAULT;
			break;

		      case SIGCHLD:
			to->si_code |= __SI_CHLD;
			break;

		      case SIGPOLL:
			to->si_code |= __SI_POLL;
			break;

		      case SIGPROF:
			to->si_code |= __SI_PROF;
			break;

		      default:
			break;
		}
	}
	return 0;
}

long
ia64_rt_sigreturn (struct sigscratch *scr)
{
	extern char ia64_strace_leave_kernel, ia64_leave_kernel;
	struct sigcontext *sc;
	struct siginfo si;
	sigset_t set;
	long retval;

	sc = &((struct sigframe *) (scr->pt.r12 + 16))->sc;

	/*
	 * When we return to the previously executing context, r8 and r10 have already
	 * been setup the way we want them.  Indeed, if the signal wasn't delivered while
	 * in a system call, we must not touch r8 or r10 as otherwise user-level state
	 * could be corrupted.
	 */
	retval = (long) &ia64_leave_kernel;
	if (current->ptrace & PT_TRACESYS)
		/*
		 * strace expects to be notified after sigreturn returns even though the
		 * context to which we return may not be in the middle of a syscall.
		 * Thus, the return-value that strace displays for sigreturn is
		 * meaningless.
		 */
		retval = (long) &ia64_strace_leave_kernel;

	if (!access_ok(VERIFY_READ, sc, sizeof(*sc)))
		goto give_sigsegv;

	if (GET_SIGSET(&set, &sc->sc_mask))
		goto give_sigsegv;

	sigdelsetmask(&set, ~_BLOCKABLE);

	spin_lock_irq(&current->sigmask_lock);
	{
		current->blocked = set;
		recalc_sigpending(current);
	}
	spin_unlock_irq(&current->sigmask_lock);

	if (restore_sigcontext(sc, scr))
		goto give_sigsegv;

#if DEBUG_SIG
	printk("SIG return (%s:%d): sp=%lx ip=%lx\n",
	       current->comm, current->pid, scr->pt.r12, scr->pt.cr_iip);
#endif
	/*
	 * It is more difficult to avoid calling this function than to
	 * call it and ignore errors.
	 */
	do_sigaltstack(&sc->sc_stack, 0, scr->pt.r12);
	return retval;

  give_sigsegv:
	si.si_signo = SIGSEGV;
	si.si_errno = 0;
	si.si_code = SI_KERNEL;
	si.si_pid = current->pid;
	si.si_uid = current->uid;
	si.si_addr = sc;
	force_sig_info(SIGSEGV, &si, current);
	return retval;
}

/*
 * This does just the minimum required setup of sigcontext.
 * Specifically, it only installs data that is either not knowable at
 * the user-level or that gets modified before execution in the
 * trampoline starts.  Everything else is done at the user-level.
 */
static long
setup_sigcontext (struct sigcontext *sc, sigset_t *mask, struct sigscratch *scr)
{
	unsigned long flags = 0, ifs, cfm, nat;
	long err;

	ifs = scr->pt.cr_ifs;

	if (on_sig_stack((unsigned long) sc))
		flags |= IA64_SC_FLAG_ONSTACK;
	if ((ifs & (1UL << 63)) == 0) {
		/* if cr_ifs isn't valid, we got here through a syscall */
		flags |= IA64_SC_FLAG_IN_SYSCALL;
		cfm = scr->ar_pfs & ((1UL << 38) - 1);
	} else
		cfm = ifs & ((1UL << 38) - 1);
	ia64_flush_fph(current);
	if ((current->thread.flags & IA64_THREAD_FPH_VALID)) {
		flags |= IA64_SC_FLAG_FPH_VALID;
		__copy_to_user(&sc->sc_fr[32], current->thread.fph, 96*16);
	}

	nat = ia64_get_scratch_nat_bits(&scr->pt, scr->scratch_unat);

	err  = __put_user(flags, &sc->sc_flags);

	err |= __put_user(nat, &sc->sc_nat);
	err |= PUT_SIGSET(mask, &sc->sc_mask);
	err |= __put_user(cfm, &sc->sc_cfm);
	err |= __put_user(scr->pt.cr_ipsr & IA64_PSR_UM, &sc->sc_um);
	err |= __put_user(scr->pt.ar_rsc, &sc->sc_ar_rsc);
	err |= __put_user(scr->pt.ar_unat, &sc->sc_ar_unat);		/* ar.unat */
	err |= __put_user(scr->pt.ar_fpsr, &sc->sc_ar_fpsr);		/* ar.fpsr */
	err |= __put_user(scr->pt.ar_pfs, &sc->sc_ar_pfs);
	err |= __put_user(scr->pt.pr, &sc->sc_pr);			/* predicates */
	err |= __put_user(scr->pt.b0, &sc->sc_br[0]);			/* b0 (rp) */
	err |= __copy_to_user(&sc->sc_gr[1], &scr->pt.r1, 8);		/* r1 */
	err |= __copy_to_user(&sc->sc_gr[8], &scr->pt.r8, 4*8);		/* r8-r11 */
	err |= __copy_to_user(&sc->sc_gr[12], &scr->pt.r12, 2*8);	/* r12-r13 */
	err |= __copy_to_user(&sc->sc_gr[15], &scr->pt.r15, 8);		/* r15 */
	err |= __put_user(scr->pt.cr_iip + ia64_psr(&scr->pt)->ri, &sc->sc_ip);

	if (flags & IA64_SC_FLAG_IN_SYSCALL)
	{
		// Clear scratch registers in sig context for asynchronized signal.
		err |= __clear_user(&sc->sc_ar_ccv, 8);
		err |= __clear_user(&sc->sc_ar25,8);				/* ar.csd */
		err |= __clear_user(&sc->sc_ar26,8);				/* ar.ssd */
		err |= __clear_user(&sc->sc_br[6],8);				/* b6 */
		err |= __clear_user(&sc->sc_br[7],8);				/* b7 */

		err |= __clear_user(&sc->sc_gr[2], 2*8);			/* r2-r3 */
		err |= __clear_user(&sc->sc_gr[14],8);				/* r14 */
		err |= __clear_user(&sc->sc_gr[16],16*8);			/* r16-r31 */
	} else
	{
		// Copy scratch registers in sig context from pt_regs for synchronized signal.
		err |= __put_user(scr->pt.ar_ccv, &sc->sc_ar_ccv);
		err |= __put_user(scr->pt.ar_csd, &sc->sc_ar25);		/* ar.csd */
		err |= __put_user(scr->pt.ar_ssd, &sc->sc_ar26);		/* ar.ssd */
		err |= __put_user(scr->pt.b6, &sc->sc_br[6]);			/* b6 */
		err |= __put_user(scr->pt.b7, &sc->sc_br[7]);			/* b7 */

		err |= __copy_to_user(&sc->sc_gr[2], &scr->pt.r2, 2*8);		/* r2-r3 */
		err |= __copy_to_user(&sc->sc_gr[14], &scr->pt.r14, 8);		/* r14 */
		err |= __copy_to_user(&sc->sc_gr[16], &scr->pt.r16, 16*8);	/* r16-r31 */
	}
	return err;
}

/*
 * Check whether the register-backing store is already on the signal stack.
 */
static inline int
rbs_on_sig_stack (unsigned long bsp)
{
	return (bsp - current->sas_ss_sp < current->sas_ss_size);
}

static long
setup_frame (int sig, struct k_sigaction *ka, siginfo_t *info, sigset_t *set,
	     struct sigscratch *scr)
{
	extern char ia64_sigtramp[], __start_gate_section[];
	unsigned long tramp_addr, new_rbs = 0;
	struct sigframe *frame;
	struct siginfo si;
	long err;

	frame = (void *) scr->pt.r12;
	tramp_addr = GATE_ADDR + (ia64_sigtramp - __start_gate_section);
	if ((ka->sa.sa_flags & SA_ONSTACK) && sas_ss_flags((unsigned long) frame) == 0) {
		frame = (void *) ((current->sas_ss_sp + current->sas_ss_size)
				  & ~(STACK_ALIGN - 1));
  		/*
		 * We need to check for the register stack being on the signal stack
		 * separately, because it's switched separately (memory stack is switched
		 * in the kernel, register stack is switched in the signal trampoline).
  		 */
		if (!rbs_on_sig_stack(scr->pt.ar_bspstore))
			new_rbs  = (current->sas_ss_sp + sizeof(long) - 1) & ~(sizeof(long) - 1);
	}
	frame = (void *) frame - ((sizeof(*frame) + STACK_ALIGN - 1) & ~(STACK_ALIGN - 1));

	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		goto give_sigsegv;

	err  = __put_user(sig, &frame->arg0);
	err |= __put_user(&frame->info, &frame->arg1);
	err |= __put_user(&frame->sc, &frame->arg2);
	err |= __put_user(new_rbs, &frame->sc.sc_rbs_base);
	err |= __put_user(0, &frame->sc.sc_loadrs);	/* initialize to zero */
	err |= __put_user(ka->sa.sa_handler, &frame->handler);

	err |= copy_siginfo_to_user(&frame->info, info);

	err |= __put_user(current->sas_ss_sp, &frame->sc.sc_stack.ss_sp);
	err |= __put_user(current->sas_ss_size, &frame->sc.sc_stack.ss_size);
	err |= __put_user(sas_ss_flags(scr->pt.r12), &frame->sc.sc_stack.ss_flags);
	err |= setup_sigcontext(&frame->sc, set, scr);

	if (err)
		goto give_sigsegv;

	scr->pt.r12 = (unsigned long) frame - 16;	/* new stack pointer */
	scr->pt.ar_fpsr = FPSR_DEFAULT;			/* reset fpsr for signal handler */
	scr->pt.cr_iip = tramp_addr;
	ia64_psr(&scr->pt)->ri = 0;			/* start executing in first slot */
	/*
	 * Force the interruption function mask to zero.  This has no effect when a
	 * system-call got interrupted by a signal (since, in that case, scr->pt_cr_ifs is
	 * ignored), but it has the desirable effect of making it possible to deliver a
	 * signal with an incomplete register frame (which happens when a mandatory RSE
	 * load faults).  Furthermore, it has no negative effect on the getting the user's
	 * dirty partition preserved, because that's governed by scr->pt.loadrs.
	 */
	scr->pt.cr_ifs = (1UL << 63);

	/*
	 * Note: this affects only the NaT bits of the scratch regs (the ones saved in
	 * pt_regs), which is exactly what we want.
	 */
	scr->scratch_unat = 0; /* ensure NaT bits of r12 is clear */

#if DEBUG_SIG
	printk("SIG deliver (%s:%d): sig=%d sp=%lx ip=%lx handler=%lx\n",
	       current->comm, current->pid, sig, scr->pt.r12, scr->pt.cr_iip, scr->pt.r3);
#endif
	return 1;

  give_sigsegv:
	if (sig == SIGSEGV)
		ka->sa.sa_handler = SIG_DFL;
	si.si_signo = SIGSEGV;
	si.si_errno = 0;
	si.si_code = SI_KERNEL;
	si.si_pid = current->pid;
	si.si_uid = current->uid;
	si.si_addr = frame;
	force_sig_info(SIGSEGV, &si, current);
	return 0;
}

static long
handle_signal (unsigned long sig, struct k_sigaction *ka, siginfo_t *info, sigset_t *oldset,
	       struct sigscratch *scr)
{
#ifdef CONFIG_IA32_SUPPORT
	if (IS_IA32_PROCESS(&scr->pt)) {
		/* send signal to IA-32 process */
		if (!ia32_setup_frame1(sig, ka, info, oldset, &scr->pt))
			return 0;
	} else
#endif
	/* send signal to IA-64 process */
	if (!setup_frame(sig, ka, info, oldset, scr))
		return 0;

	if (ka->sa.sa_flags & SA_ONESHOT)
		ka->sa.sa_handler = SIG_DFL;

	if (!(ka->sa.sa_flags & SA_NODEFER)) {
		spin_lock_irq(&current->sigmask_lock);
		{
			sigorsets(&current->blocked, &current->blocked, &ka->sa.sa_mask);
			sigaddset(&current->blocked, sig);
			recalc_sigpending(current);
		}
		spin_unlock_irq(&current->sigmask_lock);
	}
	return 1;
}

/*
 * Note that `init' is a special process: it doesn't get signals it doesn't want to
 * handle.  Thus you cannot kill init even with a SIGKILL even by mistake.
 */
long
ia64_do_signal (sigset_t *oldset, struct sigscratch *scr, long in_syscall)
{
	struct signal_struct *sig;
	struct k_sigaction *ka;
	siginfo_t info;
	long restart = in_syscall;
	long errno = scr->pt.r8;

	/*
	 * In the ia64_leave_kernel code path, we want the common case to go fast, which
	 * is why we may in certain cases get here from kernel mode. Just return without
	 * doing anything if so.
	 */
	if (!user_mode(&scr->pt))
		return 0;

	if (!oldset)
		oldset = &current->blocked;

#ifdef CONFIG_IA32_SUPPORT
	if (IS_IA32_PROCESS(&scr->pt)) {
		if (in_syscall) {
			if (errno >= 0)
				restart = 0;
			else
				errno = -errno;
		}
	} else
#endif
	if (scr->pt.r10 != -1) {
		/*
		 * A system calls has to be restarted only if one of the error codes
		 * ERESTARTNOHAND, ERESTARTSYS, or ERESTARTNOINTR is returned.  If r10
		 * isn't -1 then r8 doesn't hold an error code and we don't need to
		 * restart the syscall, so we can clear the "restart" flag here.
		 */
		restart = 0;
	}

	for (;;) {
		unsigned long signr;

		spin_lock_irq(&current->sigmask_lock);
		signr = dequeue_signal(&current->blocked, &info);
		spin_unlock_irq(&current->sigmask_lock);

		if (!signr)
			break;

		if ((current->ptrace & PT_PTRACED) && signr != SIGKILL) {
			/* Let the debugger run.  */
			current->exit_code = signr;
			current->thread.siginfo = &info;
			current->state = TASK_STOPPED;
			notify_parent(current, SIGCHLD);
			schedule();

			signr = current->exit_code;
			current->thread.siginfo = 0;

			/* We're back.  Did the debugger cancel the sig?  */
			if (!signr)
				continue;
			current->exit_code = 0;

			/* The debugger continued.  Ignore SIGSTOP.  */
			if (signr == SIGSTOP)
				continue;

			/* Update the siginfo structure.  Is this good?  */
			if (signr != info.si_signo) {
				info.si_signo = signr;
				info.si_errno = 0;
				info.si_code = SI_USER;
				info.si_pid = current->p_pptr->pid;
				info.si_uid = current->p_pptr->uid;
			}

			/* If the (new) signal is now blocked, requeue it.  */
			if (sigismember(&current->blocked, signr)) {
				send_sig_info(signr, &info, current);
				continue;
			}
		}

		ka = &current->sig->action[signr - 1];
		if (ka->sa.sa_handler == SIG_IGN) {
			if (signr != SIGCHLD)
				continue;
			/* Check for SIGCHLD: it's special.  */
			while (sys_wait4(-1, NULL, WNOHANG, NULL) > 0)
				/* nothing */;
			continue;
		}

		if (ka->sa.sa_handler == SIG_DFL) {
			int exit_code = signr;

			/* Init gets no signals it doesn't want.  */
			if (current->pid == 1)
				continue;

			switch (signr) {
			      case SIGCONT: case SIGCHLD: case SIGWINCH: case SIGURG:
				continue;

			      case SIGTSTP: case SIGTTIN: case SIGTTOU:
				if (is_orphaned_pgrp(current->pgrp))
					continue;
				/* FALLTHRU */

			      case SIGSTOP:
				current->state = TASK_STOPPED;
				current->exit_code = signr;
				sig = current->p_pptr->sig;
				if (sig && !(sig->action[SIGCHLD-1].sa.sa_flags & SA_NOCLDSTOP))
					notify_parent(current, SIGCHLD);
				schedule();
				continue;

			      case SIGQUIT: case SIGILL: case SIGTRAP:
			      case SIGABRT: case SIGFPE: case SIGSEGV:
			      case SIGBUS: case SIGSYS: case SIGXCPU: case SIGXFSZ:
				if (do_coredump(signr, &scr->pt))
					exit_code |= 0x80;
				/* FALLTHRU */

			      default:
				sig_exit(signr, exit_code, &info);
				/* NOTREACHED */
			}
		}

		if (restart) {
			switch (errno) {
			      case ERESTARTSYS:
				if ((ka->sa.sa_flags & SA_RESTART) == 0) {
			      case ERESTARTNOHAND:
#ifdef CONFIG_IA32_SUPPORT
					if (IS_IA32_PROCESS(&scr->pt))
						scr->pt.r8 = -EINTR;
					else
#endif
					scr->pt.r8 = EINTR;
					/* note: scr->pt.r10 is already -1 */
					break;
				}
			      case ERESTARTNOINTR:
#ifdef CONFIG_IA32_SUPPORT
				if (IS_IA32_PROCESS(&scr->pt)) {
					scr->pt.r8 = scr->pt.r1;
					scr->pt.cr_iip -= 2;
				} else
#endif
				ia64_decrement_ip(&scr->pt);
			}
		}

		/* Whee!  Actually deliver the signal.  If the
		   delivery failed, we need to continue to iterate in
		   this loop so we can deliver the SIGSEGV... */
		if (handle_signal(signr, ka, &info, oldset, scr))
			return 1;
	}

	/* Did we come from a system call? */
	if (restart) {
		/* Restart the system call - no handlers present */
		if (errno == ERESTARTNOHAND || errno == ERESTARTSYS || errno == ERESTARTNOINTR) {
#ifdef CONFIG_IA32_SUPPORT
			if (IS_IA32_PROCESS(&scr->pt)) {
				scr->pt.r8 = scr->pt.r1;
				scr->pt.cr_iip -= 2;
			} else
#endif
			/*
			 * Note: the syscall number is in r15 which is
			 * saved in pt_regs so all we need to do here
			 * is adjust ip so that the "break"
			 * instruction gets re-executed.
			 */
			ia64_decrement_ip(&scr->pt);
		}
	}
	return 0;
}
