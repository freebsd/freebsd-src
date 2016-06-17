/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1991, 1992  Linus Torvalds
 * Copyright (C) 1994 - 2000  Ralf Baechle
 * Copyright (C) 1999, 2000 Silicon Graphics, Inc.
 */
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/ptrace.h>
#include <linux/unistd.h>

#include <asm/asm.h>
#include <asm/bitops.h>
#include <asm/pgalloc.h>
#include <asm/stackframe.h>
#include <asm/uaccess.h>
#include <asm/ucontext.h>
#include <asm/system.h>
#include <asm/fpu.h>

/*
 * Including <asm/unistd.h> would give use the 64-bit syscall numbers ...
 */
#define __NR_O32_sigreturn		4119
#define __NR_O32_rt_sigreturn		4193
#define __NR_O32_restart_syscall	4253

#define DEBUG_SIG 0

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

extern asmlinkage int do_signal32(sigset_t *oldset, struct pt_regs *regs);

extern asmlinkage void syscall_trace(void);

/* 32-bit compatibility types */

#define _NSIG32_BPW	32
#define _NSIG32_WORDS	(_NSIG / _NSIG32_BPW)

typedef struct {
	unsigned int sig[_NSIG32_WORDS];
} sigset32_t;

typedef unsigned int __sighandler32_t;
typedef void (*vfptr_t)(void);

struct sigaction32 {
	unsigned int		sa_flags;
	__sighandler32_t	sa_handler;
	sigset32_t		sa_mask;
};

/* IRIX compatible stack_t  */
typedef struct sigaltstack32 {
	s32 ss_sp;
	__kernel_size_t32 ss_size;
	int ss_flags;
} stack32_t;

struct ucontext32 {
	u32                 uc_flags;
	s32                 uc_link;
	stack32_t           uc_stack;
	struct sigcontext32 uc_mcontext;
	sigset_t32          uc_sigmask;   /* mask last for extensibility */
};

extern void __put_sigset_unknown_nsig(void);
extern void __get_sigset_unknown_nsig(void);

static inline int put_sigset(const sigset_t *kbuf, sigset32_t *ubuf)
{
	int err = 0;

	if (!access_ok(VERIFY_WRITE, ubuf, sizeof(*ubuf)))
		return -EFAULT;

	switch (_NSIG_WORDS) {
	default:
		__put_sigset_unknown_nsig();
	case 2:
		err |= __put_user (kbuf->sig[1] >> 32, &ubuf->sig[3]);
		err |= __put_user (kbuf->sig[1] & 0xffffffff, &ubuf->sig[2]);
	case 1:
		err |= __put_user (kbuf->sig[0] >> 32, &ubuf->sig[1]);
		err |= __put_user (kbuf->sig[0] & 0xffffffff, &ubuf->sig[0]);
	}

	return err;
}

static inline int get_sigset(sigset_t *kbuf, const sigset32_t *ubuf)
{
	int err = 0;
	unsigned long sig[4];

	if (!access_ok(VERIFY_READ, ubuf, sizeof(*ubuf)))
		return -EFAULT;

	switch (_NSIG_WORDS) {
	default:
		__get_sigset_unknown_nsig();
	case 2:
		err |= __get_user (sig[3], &ubuf->sig[3]);
		err |= __get_user (sig[2], &ubuf->sig[2]);
		kbuf->sig[1] = sig[2] | (sig[3] << 32);
	case 1:
		err |= __get_user (sig[1], &ubuf->sig[1]);
		err |= __get_user (sig[0], &ubuf->sig[0]);
		kbuf->sig[0] = sig[0] | (sig[1] << 32);
	}

	return err;
}

/*
 * Atomically swap in the new signal mask, and wait for a signal.
 */
save_static_function(sys32_sigsuspend);
static_unused int _sys32_sigsuspend(abi64_no_regargs, struct pt_regs regs)
{
	sigset32_t *uset;
	sigset_t newset, saveset;

	uset = (sigset32_t *) regs.regs[4];
	if (get_sigset(&newset, uset))
		return -EFAULT;
	sigdelsetmask(&newset, ~_BLOCKABLE);

	spin_lock_irq(&current->sigmask_lock);
	saveset = current->blocked;
	current->blocked = newset;
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	regs.regs[2] = EINTR;
	regs.regs[7] = 1;
	while (1) {
		current->state = TASK_INTERRUPTIBLE;
		schedule();
		if (do_signal32(&saveset, &regs))
			return -EINTR;
	}
}

save_static_function(sys32_rt_sigsuspend);
static_unused int _sys32_rt_sigsuspend(abi64_no_regargs, struct pt_regs regs)
{
	sigset32_t *uset;
	sigset_t newset, saveset;
        size_t sigsetsize;

	/* XXX Don't preclude handling different sized sigset_t's.  */
	sigsetsize = regs.regs[5];
	if (sigsetsize != sizeof(sigset32_t))
		return -EINVAL;

	uset = (sigset32_t *) regs.regs[4];
	if (get_sigset(&newset, uset))
		return -EFAULT;
	sigdelsetmask(&newset, ~_BLOCKABLE);

	spin_lock_irq(&current->sigmask_lock);
	saveset = current->blocked;
	current->blocked = newset;
        recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	regs.regs[2] = EINTR;
	regs.regs[7] = 1;
	while (1) {
		current->state = TASK_INTERRUPTIBLE;
		schedule();
		if (do_signal32(&saveset, &regs))
			return -EINTR;
	}
}

asmlinkage int sys32_sigaction(int sig, const struct sigaction32 *act,
                               struct sigaction32 *oact)
{
	struct k_sigaction new_ka, old_ka;
	int ret;
	int err = 0;

	if (act) {
		old_sigset_t mask;

		if (!access_ok(VERIFY_READ, act, sizeof(*act)))
			return -EFAULT;
		err |= __get_user((u32)(u64)new_ka.sa.sa_handler,
		                  &act->sa_handler);
		err |= __get_user(new_ka.sa.sa_flags, &act->sa_flags);
		err |= __get_user(mask, &act->sa_mask.sig[0]);
		if (err)
			return -EFAULT;

		siginitset(&new_ka.sa.sa_mask, mask);
	}

	ret = do_sigaction(sig, act ? &new_ka : NULL, oact ? &old_ka : NULL);

	if (!ret && oact) {
		if (!access_ok(VERIFY_WRITE, oact, sizeof(*oact)))
                        return -EFAULT;
		err |= __put_user(old_ka.sa.sa_flags, &oact->sa_flags);
		err |= __put_user((u32)(u64)old_ka.sa.sa_handler,
		                  &oact->sa_handler);
		err |= __put_user(old_ka.sa.sa_mask.sig[0], oact->sa_mask.sig);
                err |= __put_user(0, &oact->sa_mask.sig[1]);
                err |= __put_user(0, &oact->sa_mask.sig[2]);
                err |= __put_user(0, &oact->sa_mask.sig[3]);
                if (err)
			return -EFAULT;
	}

	return ret;
}

asmlinkage int sys32_sigaltstack(abi64_no_regargs, struct pt_regs regs)
{
	const stack32_t *uss = (const stack32_t *) regs.regs[4];
	stack32_t *uoss = (stack32_t *) regs.regs[5];
	unsigned long usp = regs.regs[29];
	stack_t kss, koss;
	int ret, err = 0;
	mm_segment_t old_fs = get_fs();
	s32 sp;

	if (uss) {
		if (!access_ok(VERIFY_READ, uss, sizeof(*uss)))
			return -EFAULT;
		err |= __get_user(sp, &uss->ss_sp);
		kss.ss_sp = (void *) (long) sp;
		err |= __get_user(kss.ss_size, &uss->ss_size);
		err |= __get_user(kss.ss_flags, &uss->ss_flags);
		if (err)
			return -EFAULT;
	}

	set_fs (KERNEL_DS);
	ret = do_sigaltstack(uss ? &kss : NULL , uoss ? &koss : NULL, usp);
	set_fs (old_fs);

	if (!ret && uoss) {
		if (!access_ok(VERIFY_WRITE, uoss, sizeof(*uoss)))
			return -EFAULT;
		sp = (int) (long) koss.ss_sp;
		err |= __put_user(sp, &uoss->ss_sp);
		err |= __put_user(koss.ss_size, &uoss->ss_size);
		err |= __put_user(koss.ss_flags, &uoss->ss_flags);
		if (err)
			return -EFAULT;
	}
	return ret;
}

static asmlinkage int restore_sigcontext32(struct pt_regs *regs,
					   struct sigcontext32 *sc)
{
	int err = 0;

	err |= __get_user(regs->cp0_epc, &sc->sc_pc);
	err |= __get_user(regs->hi, &sc->sc_mdhi);
	err |= __get_user(regs->lo, &sc->sc_mdlo);

#define restore_gp_reg(i) do {						\
	err |= __get_user(regs->regs[i], &sc->sc_regs[i]);		\
} while(0)
	restore_gp_reg( 1); restore_gp_reg( 2); restore_gp_reg( 3);
	restore_gp_reg( 4); restore_gp_reg( 5); restore_gp_reg( 6);
	restore_gp_reg( 7); restore_gp_reg( 8); restore_gp_reg( 9);
	restore_gp_reg(10); restore_gp_reg(11); restore_gp_reg(12);
	restore_gp_reg(13); restore_gp_reg(14); restore_gp_reg(15);
	restore_gp_reg(16); restore_gp_reg(17); restore_gp_reg(18);
	restore_gp_reg(19); restore_gp_reg(20); restore_gp_reg(21);
	restore_gp_reg(22); restore_gp_reg(23); restore_gp_reg(24);
	restore_gp_reg(25); restore_gp_reg(26); restore_gp_reg(27);
	restore_gp_reg(28); restore_gp_reg(29); restore_gp_reg(30);
	restore_gp_reg(31);
#undef restore_gp_reg

	err |= __get_user(current->used_math, &sc->sc_used_math);

	if (current->used_math) {
		/* restore fpu context if we have used it before */
		own_fpu();
		err |= restore_fp_context32(sc);
	} else {
		/* signal handler may have used FPU.  Give it up. */
		lose_fpu();
	}

	return err;
}

struct sigframe {
	u32 sf_ass[4];			/* argument save space for o32 */
	u32 sf_code[2];			/* signal trampoline */
	struct sigcontext32 sf_sc;
	sigset_t sf_mask;
};

struct rt_sigframe32 {
	u32 rs_ass[4];			/* argument save space for o32 */
	u32 rs_code[2];			/* signal trampoline */
	struct siginfo32 rs_info;
	struct ucontext32 rs_uc;
};

static int copy_siginfo_to_user32(siginfo_t32 *to, siginfo_t *from)
{
	int err;

	if (!access_ok (VERIFY_WRITE, to, sizeof(siginfo_t32)))
		return -EFAULT;

	/* If you change siginfo_t structure, please be sure
	   this code is fixed accordingly.
	   It should never copy any pad contained in the structure
	   to avoid security leaks, but must copy the generic
	   3 ints plus the relevant union member.
	   This routine must convert siginfo from 64bit to 32bit as well
	   at the same time.  */
	err = __put_user(from->si_signo, &to->si_signo);
	err |= __put_user(from->si_errno, &to->si_errno);
	err |= __put_user((short)from->si_code, &to->si_code);
	if (from->si_code < 0)
		err |= __copy_to_user(&to->_sifields._pad, &from->_sifields._pad, SI_PAD_SIZE);
	else {
		switch (from->si_code >> 16) {
		case __SI_CHLD >> 16:
			err |= __put_user(from->si_utime, &to->si_utime);
			err |= __put_user(from->si_stime, &to->si_stime);
			err |= __put_user(from->si_status, &to->si_status);
		default:
			err |= __put_user(from->si_pid, &to->si_pid);
			err |= __put_user(from->si_uid, &to->si_uid);
			break;
		case __SI_FAULT >> 16:
			err |= __put_user((long)from->si_addr, &to->si_addr);
			break;
		case __SI_POLL >> 16:
			err |= __put_user(from->si_band, &to->si_band);
			err |= __put_user(from->si_fd, &to->si_fd);
			break;
		/* case __SI_RT: This is not generated by the kernel as of now.  */
		}
	}
	return err;
}

asmlinkage void sys32_sigreturn(abi64_no_regargs, struct pt_regs regs)
{
	struct sigframe *frame;
	sigset_t blocked;

	frame = (struct sigframe *) regs.regs[29];
	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;
	if (__copy_from_user(&blocked, &frame->sf_mask, sizeof(blocked)))
		goto badframe;

	sigdelsetmask(&blocked, ~_BLOCKABLE);
	spin_lock_irq(&current->sigmask_lock);
	current->blocked = blocked;
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	if (restore_sigcontext32(&regs, &frame->sf_sc))
		goto badframe;

	/*
	 * Don't let your children do this ...
	 */
	if (current->ptrace & PT_TRACESYS)
		syscall_trace();
	__asm__ __volatile__(
		"move\t$29, %0\n\t"
		"j\tret_from_sys_call"
		:/* no outputs */
		:"r" (&regs));
	/* Unreached */

badframe:
	force_sig(SIGSEGV, current);
}

asmlinkage void sys32_rt_sigreturn(abi64_no_regargs, struct pt_regs regs)
{
	struct rt_sigframe32 *frame;
	sigset_t set;
	stack_t st;
	s32 sp;

	frame = (struct rt_sigframe32 *) regs.regs[29];
	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;
	if (__copy_from_user(&set, &frame->rs_uc.uc_sigmask, sizeof(set)))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	spin_lock_irq(&current->sigmask_lock);
	current->blocked = set;
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	if (restore_sigcontext32(&regs, &frame->rs_uc.uc_mcontext))
		goto badframe;

	/* The ucontext contains a stack32_t, so we must convert!  */
	if (__get_user(sp, &frame->rs_uc.uc_stack.ss_sp))
		goto badframe;
	st.ss_size = (long) sp;
	if (__get_user(st.ss_size, &frame->rs_uc.uc_stack.ss_size))
		goto badframe;
	if (__get_user(st.ss_flags, &frame->rs_uc.uc_stack.ss_flags))
		goto badframe;

	/* It is more difficult to avoid calling this function than to
	   call it and ignore errors.  */
	do_sigaltstack(&st, NULL, regs.regs[29]);

	/*
	 * Don't let your children do this ...
	 */
	__asm__ __volatile__(
		"move\t$29, %0\n\t"
		"j\tret_from_sys_call"
		:/* no outputs */
		:"r" (&regs));
	/* Unreached */

badframe:
	force_sig(SIGSEGV, current);
}

static int inline setup_sigcontext32(struct pt_regs *regs,
				     struct sigcontext32 *sc)
{
	int err = 0;

	err |= __put_user(regs->cp0_epc, &sc->sc_pc);
	err |= __put_user(regs->cp0_status, &sc->sc_status);

#define save_gp_reg(i) {						\
	err |= __put_user(regs->regs[i], &sc->sc_regs[i]);		\
} while(0)
	__put_user(0, &sc->sc_regs[0]); save_gp_reg(1); save_gp_reg(2);
	save_gp_reg(3); save_gp_reg(4); save_gp_reg(5); save_gp_reg(6);
	save_gp_reg(7); save_gp_reg(8); save_gp_reg(9); save_gp_reg(10);
	save_gp_reg(11); save_gp_reg(12); save_gp_reg(13); save_gp_reg(14);
	save_gp_reg(15); save_gp_reg(16); save_gp_reg(17); save_gp_reg(18);
	save_gp_reg(19); save_gp_reg(20); save_gp_reg(21); save_gp_reg(22);
	save_gp_reg(23); save_gp_reg(24); save_gp_reg(25); save_gp_reg(26);
	save_gp_reg(27); save_gp_reg(28); save_gp_reg(29); save_gp_reg(30);
	save_gp_reg(31);
#undef save_gp_reg

	err |= __put_user(regs->hi, &sc->sc_mdhi);
	err |= __put_user(regs->lo, &sc->sc_mdlo);
	err |= __put_user(regs->cp0_cause, &sc->sc_cause);
	err |= __put_user(regs->cp0_badvaddr, &sc->sc_badvaddr);

	err |= __put_user(current->used_math, &sc->sc_used_math);

	if (!current->used_math)
		goto out;

	/* 
	 * Save FPU state to signal context.  Signal handler will "inherit"
	 * current FPU state.
	 */
	if (!is_fpu_owner()) {
		own_fpu();
		restore_fp(current);
	}
	err |= save_fp_context32(sc);

out:
	return err;
}

/*
 * Determine which stack to use..
 */
static inline void *get_sigframe(struct k_sigaction *ka, struct pt_regs *regs,
				 size_t frame_size)
{
	unsigned long sp;

	/* Default to using normal stack */
	sp = regs->regs[29];

	/*
 	 * FPU emulator may have it's own trampoline active just
 	 * above the user stack, 16-bytes before the next lowest
 	 * 16 byte boundary.  Try to avoid trashing it.
 	 */
 	sp -= 32;

	/* This is the X/Open sanctioned signal stack switching.  */
	if ((ka->sa.sa_flags & SA_ONSTACK) && ! on_sig_stack(sp))
		sp = current->sas_ss_sp + current->sas_ss_size;

	return (void *)((sp - frame_size) & ALMASK);
}

static void inline setup_frame(struct k_sigaction * ka, struct pt_regs *regs,
			       int signr, sigset_t *set)
{
	struct sigframe *frame;
	int err = 0;

	frame = get_sigframe(ka, regs, sizeof(*frame));
	if (!access_ok(VERIFY_WRITE, frame, sizeof (*frame)))
		goto give_sigsegv;

	/*
	 * Set up the return code ...
	 *
	 *         li      v0, __NR_O32_sigreturn
	 *         syscall
	 */
	err |= __put_user(0x24020000 + __NR_O32_sigreturn, frame->sf_code + 0);
	err |= __put_user(0x0000000c                     , frame->sf_code + 1);
	flush_cache_sigtramp((unsigned long) frame->sf_code);

	err |= setup_sigcontext32(regs, &frame->sf_sc);
	err |= __copy_to_user(&frame->sf_mask, set, sizeof(*set));
	if (err)
		goto give_sigsegv;

	/*
	 * Arguments to signal handler:
	 *
	 *   a0 = signal number
	 *   a1 = 0 (should be cause)
	 *   a2 = pointer to struct sigcontext
	 *
	 * $25 and c0_epc point to the signal handler, $29 points to the
	 * struct sigframe.
	 */
	regs->regs[ 4] = signr;
	regs->regs[ 5] = 0;
	regs->regs[ 6] = (unsigned long) &frame->sf_sc;
	regs->regs[29] = (unsigned long) frame;
	regs->regs[31] = (unsigned long) frame->sf_code;
	regs->cp0_epc = regs->regs[25] = (unsigned long) ka->sa.sa_handler;

#if DEBUG_SIG
	printk("SIG deliver (%s:%d): sp=0x%p pc=0x%lx ra=0x%p\n",
	       current->comm, current->pid,
	       frame, regs->cp0_epc, frame->sf_code);
#endif
        return;

give_sigsegv:
	if (signr == SIGSEGV)
		ka->sa.sa_handler = SIG_DFL;
	force_sig(SIGSEGV, current);
}

static void inline setup_rt_frame(struct k_sigaction * ka,
				  struct pt_regs *regs, int signr,
				  sigset_t *set, siginfo_t *info)
{
	struct rt_sigframe32 *frame;
	int err = 0;
	s32 sp;

	frame = get_sigframe(ka, regs, sizeof(*frame));
	if (!access_ok(VERIFY_WRITE, frame, sizeof (*frame)))
		goto give_sigsegv;

	/* Set up to return from userspace.  If provided, use a stub already
	   in userspace.  */
	/*
	 * Set up the return code ...
	 *
	 *         li      v0, __NR_O32_rt_sigreturn
	 *         syscall
	 */
	err |= __put_user(0x24020000 + __NR_O32_rt_sigreturn, frame->rs_code + 0);
	err |= __put_user(0x0000000c                      , frame->rs_code + 1);
	flush_cache_sigtramp((unsigned long) frame->rs_code);

	/* Convert (siginfo_t -> siginfo_t32) and copy to user. */
	err |= copy_siginfo_to_user32(&frame->rs_info, info);

	/* Create the ucontext.  */
	err |= __put_user(0, &frame->rs_uc.uc_flags);
	err |= __put_user(0, &frame->rs_uc.uc_link);
	sp = (int) (long) current->sas_ss_sp;
	err |= __put_user(sp,
	                  &frame->rs_uc.uc_stack.ss_sp);
	err |= __put_user(sas_ss_flags(regs->regs[29]),
	                  &frame->rs_uc.uc_stack.ss_flags);
	err |= __put_user(current->sas_ss_size,
	                  &frame->rs_uc.uc_stack.ss_size);
	err |= setup_sigcontext32(regs, &frame->rs_uc.uc_mcontext);
	err |= __copy_to_user(&frame->rs_uc.uc_sigmask, set, sizeof(*set));

	if (err)
		goto give_sigsegv;

	/*
	 * Arguments to signal handler:
	 *
	 *   a0 = signal number
	 *   a1 = 0 (should be cause)
	 *   a2 = pointer to ucontext
	 *
	 * $25 and c0_epc point to the signal handler, $29 points to
	 * the struct rt_sigframe32.
	 */
	regs->regs[ 4] = signr;
	regs->regs[ 5] = (unsigned long) &frame->rs_info;
	regs->regs[ 6] = (unsigned long) &frame->rs_uc;
	regs->regs[29] = (unsigned long) frame;
	regs->regs[31] = (unsigned long) frame->rs_code;
	regs->cp0_epc = regs->regs[25] = (unsigned long) ka->sa.sa_handler;

#if DEBUG_SIG
	printk("SIG deliver (%s:%d): sp=0x%p pc=0x%lx ra=0x%p\n",
	       current->comm, current->pid,
	       frame, regs->cp0_epc, frame->rs_code);
#endif
	return;

give_sigsegv:
	if (signr == SIGSEGV)
		ka->sa.sa_handler = SIG_DFL;
	force_sig(SIGSEGV, current);
}

static inline void handle_signal(unsigned long sig, struct k_sigaction *ka,
				 siginfo_t *info, sigset_t *oldset,
				 struct pt_regs * regs)
{
	if (ka->sa.sa_flags & SA_SIGINFO)
		setup_rt_frame(ka, regs, sig, oldset, info);
	else
		setup_frame(ka, regs, sig, oldset);

	if (ka->sa.sa_flags & SA_ONESHOT)
		ka->sa.sa_handler = SIG_DFL;
	if (!(ka->sa.sa_flags & SA_NODEFER)) {
		spin_lock_irq(&current->sigmask_lock);
		sigorsets(&current->blocked,&current->blocked,&ka->sa.sa_mask);
		sigaddset(&current->blocked,sig);
		recalc_sigpending(current);
		spin_unlock_irq(&current->sigmask_lock);
	}
}

static inline void syscall_restart(struct pt_regs *regs,
				   struct k_sigaction *ka)
{
	switch(regs->regs[0]) {
	case ERESTARTNOHAND:
		regs->regs[2] = EINTR;
		break;
	case ERESTARTSYS:
		if(!(ka->sa.sa_flags & SA_RESTART)) {
			regs->regs[2] = EINTR;
			break;
		}
	/* fallthrough */
	case ERESTARTNOINTR:		/* Userland will reload $v0.  */
		regs->regs[7] = regs->regs[26];
		regs->cp0_epc -= 8;
	}

	regs->regs[0] = 0;		/* Don't deal with this again.  */
}

asmlinkage int do_signal32(sigset_t *oldset, struct pt_regs *regs)
{
	struct k_sigaction *ka;
	siginfo_t info;

	if (!oldset)
		oldset = &current->blocked;

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
			current->state = TASK_STOPPED;
			notify_parent(current, SIGCHLD);
			schedule();

			/* We're back.  Did the debugger cancel the sig?  */
			if (!(signr = current->exit_code))
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

		ka = &current->sig->action[signr-1];
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
				if (!(current->p_pptr->sig->action[SIGCHLD-1].sa.sa_flags & SA_NOCLDSTOP))
					notify_parent(current, SIGCHLD);
				schedule();
				continue;

			case SIGQUIT: case SIGILL: case SIGTRAP:
			case SIGABRT: case SIGFPE: case SIGSEGV:
			case SIGBUS: case SIGSYS: case SIGXCPU: case SIGXFSZ:
				if (do_coredump(signr, regs))
					exit_code |= 0x80;
				/* FALLTHRU */

			default:
				sig_exit(signr, exit_code, &info);
				/* NOTREACHED */
			}
		}

		if (regs->regs[0])
			syscall_restart(regs, ka);
		/* Whee!  Actually deliver the signal.  */
		handle_signal(signr, ka, &info, oldset, regs);
		return 1;
	}

	/*
	 * Who's code doesn't conform to the restartable syscall convention
	 * dies here!!!  The li instruction, a single machine instruction,
	 * must directly be followed by the syscall instruction.
	 */
	if (regs->regs[0]) {
		if (regs->regs[2] == ERESTARTNOHAND ||
		    regs->regs[2] == ERESTARTSYS ||
		    regs->regs[2] == ERESTARTNOINTR) {
			regs->regs[7] = regs->regs[26];
			regs->cp0_epc -= 8;
		}
	}
	return 0;
}

extern asmlinkage int sys_sigprocmask(int how, old_sigset_t *set,
						old_sigset_t *oset);

asmlinkage int sys32_sigprocmask(int how, old_sigset_t32 *set,
				 old_sigset_t32 *oset)
{
	old_sigset_t s;
	int ret;
	mm_segment_t old_fs = get_fs();

	if (set && get_user (s, set))
		return -EFAULT;

	set_fs (KERNEL_DS);
	ret = sys_sigprocmask(how, set ? &s : NULL, oset ? &s : NULL);
	set_fs (old_fs);

	if (!ret && oset && put_user (s, oset))
		return -EFAULT;

	return ret;
}

asmlinkage long sys_sigpending(old_sigset_t *set);

asmlinkage int sys32_sigpending(old_sigset_t32 *set)
{
	old_sigset_t pending;
	int ret;
	mm_segment_t old_fs = get_fs();

	set_fs (KERNEL_DS);
	ret = sys_sigpending(&pending);
	set_fs (old_fs);

	if (put_user(pending, set))
		return -EFAULT;

	return ret;
}

asmlinkage int sys32_rt_sigaction(int sig, const struct sigaction32 *act,
				  struct sigaction32 *oact,
				  unsigned int sigsetsize)
{
	struct k_sigaction new_sa, old_sa;
	int ret = -EINVAL;

	/* XXX: Don't preclude handling different sized sigset_t's.  */
	if (sigsetsize != sizeof(sigset_t))
		goto out;

	if (act) {
		int err = 0;

		if (!access_ok(VERIFY_READ, act, sizeof(*act)))
			return -EFAULT;
		err |= __get_user((u32)(u64)new_sa.sa.sa_handler,
		                  &act->sa_handler);
		err |= __get_user(new_sa.sa.sa_flags, &act->sa_flags);
		err |= get_sigset(&new_sa.sa.sa_mask, &act->sa_mask);
		if (err)
			return -EFAULT;
	}

	ret = do_sigaction(sig, act ? &new_sa : NULL, oact ? &old_sa : NULL);

	if (!ret && oact) {
		int err = 0;

		if (!access_ok(VERIFY_WRITE, oact, sizeof(*oact)))
			return -EFAULT;

		err |= __put_user((u32)(u64)old_sa.sa.sa_handler,
		                   &oact->sa_handler);
		err |= __put_user(old_sa.sa.sa_flags, &oact->sa_flags);
		err |= put_sigset(&old_sa.sa.sa_mask, &oact->sa_mask);
		if (err)
			return -EFAULT;
	}
out:
	return ret;
}

asmlinkage long sys_rt_sigprocmask(int how, sigset_t *set, sigset_t *oset,
				   size_t sigsetsize);

asmlinkage int sys32_rt_sigprocmask(int how, sigset32_t *set, sigset32_t *oset,
				    unsigned int sigsetsize)
{
	sigset_t old_set, new_set;
	int ret;
	mm_segment_t old_fs = get_fs();

	if (set && get_sigset(&new_set, set))
		return -EFAULT;

	set_fs (KERNEL_DS);
	ret = sys_rt_sigprocmask(how, set ? &new_set : NULL,
				 oset ? &old_set : NULL, sigsetsize);
	set_fs (old_fs);

	if (!ret && oset && put_sigset(&old_set, oset))
		return -EFAULT;

	return ret;
}

asmlinkage long sys_rt_sigpending(sigset_t *set, size_t sigsetsize);

asmlinkage int sys32_rt_sigpending(sigset32_t *uset, unsigned int sigsetsize)
{
	int ret;
	sigset_t set;
	mm_segment_t old_fs = get_fs();

	set_fs (KERNEL_DS);
	ret = sys_rt_sigpending(&set, sigsetsize);
	set_fs (old_fs);

	if (!ret && put_sigset(&set, uset))
		return -EFAULT;

	return ret;
}

struct timespec32 {
	int	tv_sec;
	int	tv_nsec;
};

asmlinkage int sys32_rt_sigtimedwait(sigset_t32 *uthese, siginfo_t32 *uinfo,
	struct timespec32 *uts, __kernel_size_t32 sigsetsize)
{
	int ret, sig;
	sigset_t these;
	sigset_t32 these32;
	struct timespec ts;
	siginfo_t info;
	long timeout = 0;

	/*
	 * As the result of a brainfarting competition a few years ago the
	 * size of sigset_t for the 32-bit kernel was choosen to be 128 bits
	 * but nothing so far is actually using that many, 64 are enough.  So
	 * for now we just drop the high bits.
	 */
	if (copy_from_user (&these32, uthese, sizeof(old_sigset_t32)))
		return -EFAULT;

	switch (_NSIG_WORDS) {
#ifdef __MIPSEB__
	case 4: these.sig[3] = these32.sig[6] | (((long)these32.sig[7]) << 32);
	case 3: these.sig[2] = these32.sig[4] | (((long)these32.sig[5]) << 32);
	case 2: these.sig[1] = these32.sig[2] | (((long)these32.sig[3]) << 32);
	case 1: these.sig[0] = these32.sig[0] | (((long)these32.sig[1]) << 32);
#endif
#ifdef __MIPSEL__
	case 4: these.sig[3] = these32.sig[7] | (((long)these32.sig[6]) << 32);
	case 3: these.sig[2] = these32.sig[5] | (((long)these32.sig[4]) << 32);
	case 2: these.sig[1] = these32.sig[3] | (((long)these32.sig[2]) << 32);
	case 1: these.sig[0] = these32.sig[1] | (((long)these32.sig[0]) << 32);
#endif
	}

	/*
	 * Invert the set of allowed signals to get those we
	 * want to block.
	 */
	sigdelsetmask(&these, sigmask(SIGKILL)|sigmask(SIGSTOP));
	signotset(&these);

	if (uts) {
		if (get_user (ts.tv_sec, &uts->tv_sec) ||
		    get_user (ts.tv_nsec, &uts->tv_nsec))
			return -EINVAL;
		if (ts.tv_nsec >= 1000000000L || ts.tv_nsec < 0
		    || ts.tv_sec < 0)
			return -EINVAL;
	}

	spin_lock_irq(&current->sigmask_lock);
	sig = dequeue_signal(&these, &info);
	if (!sig) {
		/* None ready -- temporarily unblock those we're interested
		   in so that we'll be awakened when they arrive.  */
		sigset_t oldblocked = current->blocked;
		sigandsets(&current->blocked, &current->blocked, &these);
		recalc_sigpending(current);
		spin_unlock_irq(&current->sigmask_lock);

		timeout = MAX_SCHEDULE_TIMEOUT;
		if (uts)
			timeout = (timespec_to_jiffies(&ts)
				   + (ts.tv_sec || ts.tv_nsec));

		current->state = TASK_INTERRUPTIBLE;
		timeout = schedule_timeout(timeout);

		spin_lock_irq(&current->sigmask_lock);
		sig = dequeue_signal(&these, &info);
		current->blocked = oldblocked;
		recalc_sigpending(current);
	}
	spin_unlock_irq(&current->sigmask_lock);

	if (sig) {
		ret = sig;
		if (uinfo) {
			if (copy_siginfo_to_user32(uinfo, &info))
				ret = -EFAULT;
		}
	} else {
		ret = -EAGAIN;
		if (timeout)
			ret = -EINTR;
	}

	return ret;
}

extern asmlinkage int sys_rt_sigqueueinfo(int pid, int sig, siginfo_t *uinfo);

asmlinkage int sys32_rt_sigqueueinfo(int pid, int sig, siginfo_t32 *uinfo)
{
	siginfo_t info;
	int ret;
	mm_segment_t old_fs = get_fs();

	if (copy_from_user (&info, uinfo, 3*sizeof(int)) ||
	    copy_from_user (info._sifields._pad, uinfo->_sifields._pad, SI_PAD_SIZE))
		return -EFAULT;
	set_fs (KERNEL_DS);
	ret = sys_rt_sigqueueinfo(pid, sig, &info);
	set_fs (old_fs);
	return ret;
}
