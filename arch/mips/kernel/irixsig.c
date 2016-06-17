/*
 * irixsig.c: WHEEE, IRIX signals!  YOW, am I compatable or what?!?!
 *
 * Copyright (C) 1996 David S. Miller (dm@engr.sgi.com)
 * Copyright (C) 1997 - 2000 Ralf Baechle (ralf@gnu.org)
 * Copyright (C) 2000 Silicon Graphics, Inc.
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/time.h>

#include <asm/ptrace.h>
#include <asm/uaccess.h>

extern asmlinkage void syscall_trace(void);

#undef DEBUG_SIG

#define _S(nr) (1<<((nr)-1))

#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))

typedef struct {
	unsigned long sig[4];
} irix_sigset_t;

struct sigctx_irix5 {
	u32 rmask, cp0_status;
	u64 pc;
	u64 regs[32];
	u64 fpregs[32];
	u32 usedfp, fpcsr, fpeir, sstk_flags;
	u64 hi, lo;
	u64 cp0_cause, cp0_badvaddr, _unused0;
	irix_sigset_t sigset;
	u64 weird_fpu_thing;
	u64 _unused1[31];
};

#ifdef DEBUG_SIG
/* Debugging */
static inline void dump_irix5_sigctx(struct sigctx_irix5 *c)
{
	int i;

	printk("misc: rmask[%08lx] status[%08lx] pc[%08lx]\n",
	       (unsigned long) c->rmask,
	       (unsigned long) c->cp0_status,
	       (unsigned long) c->pc);
	printk("regs: ");
	for(i = 0; i < 16; i++)
		printk("[%d]<%08lx> ", i, (unsigned long) c->regs[i]);
	printk("\nregs: ");
	for(i = 16; i < 32; i++)
		printk("[%d]<%08lx> ", i, (unsigned long) c->regs[i]);
	printk("\nfpregs: ");
	for(i = 0; i < 16; i++)
		printk("[%d]<%08lx> ", i, (unsigned long) c->fpregs[i]);
	printk("\nfpregs: ");
	for(i = 16; i < 32; i++)
		printk("[%d]<%08lx> ", i, (unsigned long) c->fpregs[i]);
	printk("misc: usedfp[%d] fpcsr[%08lx] fpeir[%08lx] stk_flgs[%08lx]\n",
	       (int) c->usedfp, (unsigned long) c->fpcsr,
	       (unsigned long) c->fpeir, (unsigned long) c->sstk_flags);
	printk("misc: hi[%08lx] lo[%08lx] cause[%08lx] badvaddr[%08lx]\n",
	       (unsigned long) c->hi, (unsigned long) c->lo,
	       (unsigned long) c->cp0_cause, (unsigned long) c->cp0_badvaddr);
	printk("misc: sigset<0>[%08lx] sigset<1>[%08lx] sigset<2>[%08lx] "
	       "sigset<3>[%08lx]\n", (unsigned long) c->sigset.sig[0],
	       (unsigned long) c->sigset.sig[1],
	       (unsigned long) c->sigset.sig[2],
	       (unsigned long) c->sigset.sig[3]);
}
#endif

static void setup_irix_frame(struct k_sigaction *ka, struct pt_regs *regs,
			     int signr, sigset_t *oldmask)
{
	unsigned long sp;
	struct sigctx_irix5 *ctx;
	int i;

	sp = regs->regs[29];
	sp -= sizeof(struct sigctx_irix5);
	sp &= ~(0xf);
	ctx = (struct sigctx_irix5 *) sp;
	if (!access_ok(VERIFY_WRITE, ctx, sizeof(*ctx)))
		goto segv_and_exit;

	__put_user(0, &ctx->weird_fpu_thing);
	__put_user(~(0x00000001), &ctx->rmask);
	__put_user(0, &ctx->regs[0]);
	for(i = 1; i < 32; i++)
		__put_user((u64) regs->regs[i], &ctx->regs[i]);

	__put_user((u64) regs->hi, &ctx->hi);
	__put_user((u64) regs->lo, &ctx->lo);
	__put_user((u64) regs->cp0_epc, &ctx->pc);
	__put_user(current->used_math, &ctx->usedfp);
	__put_user((u64) regs->cp0_cause, &ctx->cp0_cause);
	__put_user((u64) regs->cp0_badvaddr, &ctx->cp0_badvaddr);

	__put_user(0, &ctx->sstk_flags); /* XXX sigstack unimp... todo... */

	__copy_to_user(&ctx->sigset, oldmask, sizeof(irix_sigset_t));

#ifdef DEBUG_SIG
	dump_irix5_sigctx(ctx);
#endif

	regs->regs[4] = (unsigned long) signr;
	regs->regs[5] = 0; /* XXX sigcode XXX */
	regs->regs[6] = regs->regs[29] = sp;
	regs->regs[7] = (unsigned long) ka->sa.sa_handler;
	regs->regs[25] = regs->cp0_epc = (unsigned long) ka->sa_restorer;

	return;

segv_and_exit:
	if (signr == SIGSEGV)
		ka->sa.sa_handler = SIG_DFL;
	force_sig(SIGSEGV, current);
}

static void inline
setup_irix_rt_frame(struct k_sigaction * ka, struct pt_regs *regs,
               int signr, sigset_t *oldmask, siginfo_t *info)
{
	printk("Aiee: setup_tr_frame wants to be written");
	do_exit(SIGSEGV);
}

static inline void handle_signal(unsigned long sig, struct k_sigaction *ka,
        siginfo_t *info, sigset_t *oldset, struct pt_regs * regs)
{
	if (ka->sa.sa_flags & SA_SIGINFO)
		setup_irix_rt_frame(ka, regs, sig, oldset, info);
	else
		setup_irix_frame(ka, regs, sig, oldset);

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

static inline void syscall_restart(struct pt_regs *regs, struct k_sigaction *ka)
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
		regs->cp0_epc -= 8;
	}

	regs->regs[0] = 0;		/* Don't deal with this again.  */
}

asmlinkage int do_irix_signal(sigset_t *oldset, struct pt_regs *regs)
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
			case SIGCONT: case SIGCHLD: case SIGWINCH:
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
				if (do_coredump(signr, regs))
					exit_code |= 0x80;
				/* FALLTHRU */

			default:
				sigaddset(&current->pending.signal, signr);
				recalc_sigpending(current);
				current->flags |= PF_SIGNALED;
				do_exit(exit_code);
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
			regs->cp0_epc -= 8;
		}
	}
	return 0;
}

asmlinkage void
irix_sigreturn(struct pt_regs *regs)
{
	struct sigctx_irix5 *context, *magic;
	unsigned long umask, mask;
	u64 *fregs;
	int sig, i, base = 0;
	sigset_t blocked;

	if(regs->regs[2] == 1000)
		base = 1;

	context = (struct sigctx_irix5 *) regs->regs[base + 4];
	magic = (struct sigctx_irix5 *) regs->regs[base + 5];
	sig = (int) regs->regs[base + 6];
#ifdef DEBUG_SIG
	printk("[%s:%d] IRIX sigreturn(scp[%p],ucp[%p],sig[%d])\n",
	       current->comm, current->pid, context, magic, sig);
#endif
	if (!context)
		context = magic;
	if (!access_ok(VERIFY_READ, context, sizeof(struct sigctx_irix5)))
		goto badframe;

#ifdef DEBUG_SIG
	dump_irix5_sigctx(context);
#endif

	__get_user(regs->cp0_epc, &context->pc);
	umask = context->rmask; mask = 2;
	for (i = 1; i < 32; i++, mask <<= 1) {
		if(umask & mask)
			__get_user(regs->regs[i], &context->regs[i]);
	}
	__get_user(regs->hi, &context->hi);
	__get_user(regs->lo, &context->lo);

	if ((umask & 1) && context->usedfp) {
		fregs = (u64 *) &current->thread.fpu;
		for(i = 0; i < 32; i++)
			fregs[i] = (u64) context->fpregs[i];
		__get_user(current->thread.fpu.hard.control, &context->fpcsr);
	}

	/* XXX do sigstack crapola here... XXX */

	if (__copy_from_user(&blocked, &context->sigset, sizeof(blocked)))
		goto badframe;

	sigdelsetmask(&blocked, ~_BLOCKABLE);
	spin_lock_irq(&current->sigmask_lock);
	current->blocked = blocked;
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	/*
	 * Don't let your children do this ...
	 */
	if (current->ptrace & PT_TRACESYS)
		syscall_trace();
	__asm__ __volatile__(
		"move\t$29,%0\n\t"
		"j\tret_from_sys_call"
		:/* no outputs */
		:"r" (&regs));
		/* Unreached */

badframe:
	force_sig(SIGSEGV, current);
}

struct sigact_irix5 {
	int flags;
	void (*handler)(int);
	u32 sigset[4];
	int _unused0[2];
};

#ifdef DEBUG_SIG
static inline void dump_sigact_irix5(struct sigact_irix5 *p)
{
	printk("<f[%d] hndlr[%08lx] msk[%08lx]>", p->flags,
	       (unsigned long) p->handler,
	       (unsigned long) p->sigset[0]);
}
#endif

asmlinkage int
irix_sigaction(int sig, const struct sigaction *act,
	      struct sigaction *oact, void *trampoline)
{
	struct k_sigaction new_ka, old_ka;
	int ret;

#ifdef DEBUG_SIG
	printk(" (%d,%s,%s,%08lx) ", sig, (!new ? "0" : "NEW"),
	       (!old ? "0" : "OLD"), trampoline);
	if(new) {
		dump_sigact_irix5(new); printk(" ");
	}
#endif
	if (act) {
		sigset_t mask;
		if (verify_area(VERIFY_READ, act, sizeof(*act)) ||
		    __get_user(new_ka.sa.sa_handler, &act->sa_handler) ||
		    __get_user(new_ka.sa.sa_flags, &act->sa_flags))
			return -EFAULT;

		__copy_from_user(&mask, &act->sa_mask, sizeof(sigset_t));

		/*
		 * Hmmm... methinks IRIX libc always passes a valid trampoline
		 * value for all invocations of sigaction.  Will have to
		 * investigate.  POSIX POSIX, die die die...
		 */
		new_ka.sa_restorer = trampoline;
	}

/* XXX Implement SIG_SETMASK32 for IRIX compatibility */
	ret = do_sigaction(sig, act ? &new_ka : NULL, oact ? &old_ka : NULL);

	if (!ret && oact) {
		if (verify_area(VERIFY_WRITE, oact, sizeof(*oact)) ||
		    __put_user(old_ka.sa.sa_handler, &oact->sa_handler) ||
		    __put_user(old_ka.sa.sa_flags, &oact->sa_flags))
			return -EFAULT;
		__copy_to_user(&old_ka.sa.sa_mask, &oact->sa_mask,
		               sizeof(sigset_t));
	}

	return ret;
}

asmlinkage int irix_sigpending(irix_sigset_t *set)
{
	return do_sigpending(set, sizeof(*set));
}

asmlinkage int irix_sigprocmask(int how, irix_sigset_t *new, irix_sigset_t *old)
{
	sigset_t oldbits, newbits;
	int error;

	if (new) {
		error = verify_area(VERIFY_READ, new, sizeof(*new));
		if (error)
			return error;
		__copy_from_user(&newbits, new, sizeof(unsigned long)*4);
		sigdelsetmask(&newbits, ~_BLOCKABLE);

		spin_lock_irq(&current->sigmask_lock);
		oldbits = current->blocked;

		switch(how) {
		case 1:
			sigorsets(&newbits, &oldbits, &newbits);
			break;

		case 2:
			sigandsets(&newbits, &oldbits, &newbits);
			break;

		case 3:
			break;

		case 256:
			siginitset(&newbits, newbits.sig[0]);
			break;

		default:
			return -EINVAL;
		}
		recalc_sigpending(current);
		spin_unlock_irq(&current->sigmask_lock);
	}
	if(old) {
		error = verify_area(VERIFY_WRITE, old, sizeof(*old));
		if(error)
			return error;
		__copy_to_user(old, &current->blocked, sizeof(unsigned long)*4);
	}

	return 0;
}

asmlinkage int irix_sigsuspend(struct pt_regs *regs)
{
	sigset_t *uset, saveset, newset;

	uset = (sigset_t *) regs->regs[4];
	if (copy_from_user(&newset, uset, sizeof(sigset_t)))
		return -EFAULT;
	sigdelsetmask(&newset, ~_BLOCKABLE);

	spin_lock_irq(&current->sigmask_lock);
	saveset = current->blocked;
	current->blocked = newset;
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	regs->regs[2] = -EINTR;
	while (1) {
		current->state = TASK_INTERRUPTIBLE;
		schedule();
		if (do_irix_signal(&saveset, regs))
			return -EINTR;
	}
}

/* hate hate hate... */
struct irix5_siginfo {
	int sig, code, error;
	union {
		char unused[128 - (3 * 4)]; /* Safety net. */
		struct {
			int pid;
			union {
				int uid;
				struct {
					int utime, status, stime;
				} child;
			} procdata;
		} procinfo;

		unsigned long fault_addr;

		struct {
			int fd;
			long band;
		} fileinfo;

		unsigned long sigval;
	} stuff;
};

static inline unsigned long timespectojiffies(struct timespec *value)
{
	unsigned long sec = (unsigned) value->tv_sec;
	long nsec = value->tv_nsec;

	if (sec > (LONG_MAX / HZ))
		return LONG_MAX;
	nsec += 1000000000L / HZ - 1;
	nsec /= 1000000000L / HZ;
	return HZ * sec + nsec;
}

asmlinkage int irix_sigpoll_sys(unsigned long *set, struct irix5_siginfo *info,
				struct timespec *tp)
{
	long expire = MAX_SCHEDULE_TIMEOUT;
	sigset_t kset;
	int i, sig, error, timeo = 0;

#ifdef DEBUG_SIG
	printk("[%s:%d] irix_sigpoll_sys(%p,%p,%p)\n",
	       current->comm, current->pid, set, info, tp);
#endif

	/* Must always specify the signal set. */
	if(!set)
		return -EINVAL;

	error = verify_area(VERIFY_READ, set, sizeof(kset));
	if (error)
		goto out;

	__copy_from_user(&kset, set, sizeof(set));
	if (error)
		goto out;

	if (info && clear_user(info, sizeof(*info))) {
		error = -EFAULT;
		goto out;
	}

	if(tp) {
		error = verify_area(VERIFY_READ, tp, sizeof(*tp));
		if(error)
			return error;
		if(!tp->tv_sec && !tp->tv_nsec) {
			error = -EINVAL;
			goto out;
		}
		expire = timespectojiffies(tp)+(tp->tv_sec||tp->tv_nsec);
	}

	while(1) {
		long tmp = 0;

		current->state = TASK_INTERRUPTIBLE;
		expire = schedule_timeout(expire);

		for (i=0; i<=4; i++)
			tmp |= (current->pending.signal.sig[i] & kset.sig[i]);

		if (tmp)
			break;
		if (!expire) {
			timeo = 1;
			break;
		}
		if (signal_pending(current))
			return -EINTR;
	}
	if (timeo)
		return -EAGAIN;

	for(sig = 1; i <= 65 /* IRIX_NSIG */; sig++) {
		if (sigismember (&kset, sig))
			continue;
		if (sigismember (&current->pending.signal, sig)) {
			/* XXX need more than this... */
			if (info)
				info->sig = sig;
			error = 0;
			goto out;
		}
	}

	/* Should not get here, but do something sane if we do. */
	error = -EINTR;

out:
	return error;
}

/* This is here because of irix5_siginfo definition. */
#define P_PID    0
#define P_PGID   2
#define P_ALL    7

extern int getrusage(struct task_struct *, int, struct rusage *);

#define W_EXITED     1
#define W_TRAPPED    2
#define W_STOPPED    4
#define W_CONT       8
#define W_NOHANG    64

#define W_MASK      (W_EXITED | W_TRAPPED | W_STOPPED | W_CONT | W_NOHANG)

asmlinkage int irix_waitsys(int type, int pid, struct irix5_siginfo *info,
			    int options, struct rusage *ru)
{
	int flag, retval;
	DECLARE_WAITQUEUE(wait, current);
	struct task_struct *p;

	if (!info) {
		retval = -EINVAL;
		goto out;
	}
	retval = verify_area(VERIFY_WRITE, info, sizeof(*info));
	if(retval)
		goto out;
	if (ru) {
		retval = verify_area(VERIFY_WRITE, ru, sizeof(*ru));
		if(retval)
			goto out;
	}
	if (options & ~(W_MASK)) {
		retval = -EINVAL;
		goto out;
	}
	if (type != P_PID && type != P_PGID && type != P_ALL) {
		retval = -EINVAL;
		goto out;
	}
	add_wait_queue(&current->wait_chldexit, &wait);
repeat:
	flag = 0;
	current->state = TASK_INTERRUPTIBLE;
	read_lock(&tasklist_lock);
	for (p = current->p_cptr; p; p = p->p_osptr) {
		if ((type == P_PID) && p->pid != pid)
			continue;
		if ((type == P_PGID) && p->pgrp != pid)
			continue;
		if ((p->exit_signal != SIGCHLD))
			continue;
		flag = 1;
		switch (p->state) {
		case TASK_STOPPED:
			if (!p->exit_code)
				continue;
			if (!(options & (W_TRAPPED|W_STOPPED)) &&
			    !(p->ptrace & PT_PTRACED))
				continue;
			read_unlock(&tasklist_lock);
			if (ru != NULL)
				getrusage(p, RUSAGE_BOTH, ru);
			__put_user(SIGCHLD, &info->sig);
			__put_user(0, &info->code);
			__put_user(p->pid, &info->stuff.procinfo.pid);
			__put_user((p->exit_code >> 8) & 0xff,
			           &info->stuff.procinfo.procdata.child.status);
			__put_user(p->times.tms_utime, &info->stuff.procinfo.procdata.child.utime);
			__put_user(p->times.tms_stime, &info->stuff.procinfo.procdata.child.stime);
			p->exit_code = 0;
			retval = 0;
			goto end_waitsys;
		case TASK_ZOMBIE:
			current->times.tms_cutime += p->times.tms_utime + p->times.tms_cutime;
			current->times.tms_cstime += p->times.tms_stime + p->times.tms_cstime;
			read_unlock(&tasklist_lock);
			if (ru != NULL)
				getrusage(p, RUSAGE_BOTH, ru);
			__put_user(SIGCHLD, &info->sig);
			__put_user(1, &info->code);      /* CLD_EXITED */
			__put_user(p->pid, &info->stuff.procinfo.pid);
			__put_user((p->exit_code >> 8) & 0xff,
			           &info->stuff.procinfo.procdata.child.status);
			__put_user(p->times.tms_utime,
			           &info->stuff.procinfo.procdata.child.utime);
			__put_user(p->times.tms_stime,
			           &info->stuff.procinfo.procdata.child.stime);
			retval = 0;
			if (p->p_opptr != p->p_pptr) {
				write_lock_irq(&tasklist_lock);
				REMOVE_LINKS(p);
				p->p_pptr = p->p_opptr;
				SET_LINKS(p);
				notify_parent(p, SIGCHLD);
				write_unlock_irq(&tasklist_lock);
			} else
				release_task(p);
			goto end_waitsys;
		default:
			continue;
		}
	}
	read_unlock(&tasklist_lock);
	if (flag) {
		retval = 0;
		if (options & W_NOHANG)
			goto end_waitsys;
		retval = -ERESTARTSYS;
		if (signal_pending(current))
			goto end_waitsys;
		current->state = TASK_INTERRUPTIBLE;
		schedule();
		goto repeat;
	}
	retval = -ECHILD;
end_waitsys:
	current->state = TASK_RUNNING;
	remove_wait_queue(&current->wait_chldexit, &wait);

out:
	return retval;
}

struct irix5_context {
	u32 flags;
	u32 link;
	u32 sigmask[4];
	struct { u32 sp, size, flags; } stack;
	int regs[36];
	u32 fpregs[32];
	u32 fpcsr;
	u32 _unused0;
	u32 _unused1[47];
	u32 weird_graphics_thing;
};

asmlinkage int irix_getcontext(struct pt_regs *regs)
{
	int error, i, base = 0;
	struct irix5_context *ctx;
	unsigned long flags;

	if (regs->regs[2] == 1000)
		base = 1;
	ctx = (struct irix5_context *) regs->regs[base + 4];

#ifdef DEBUG_SIG
	printk("[%s:%d] irix_getcontext(%p)\n",
	       current->comm, current->pid, ctx);
#endif

	error = verify_area(VERIFY_WRITE, ctx, sizeof(*ctx));
	if(error)
		goto out;
	__put_user(current->thread.irix_oldctx, &ctx->link);

	__copy_to_user(&ctx->sigmask, &current->blocked, sizeof(irix_sigset_t));

	/* XXX Do sigstack stuff someday... */
	__put_user(0, &ctx->stack.sp);
	__put_user(0, &ctx->stack.size);
	__put_user(0, &ctx->stack.flags);

	__put_user(0, &ctx->weird_graphics_thing);
	__put_user(0, &ctx->regs[0]);
	for (i = 1; i < 32; i++)
		__put_user(regs->regs[i], &ctx->regs[i]);
	__put_user(regs->lo, &ctx->regs[32]);
	__put_user(regs->hi, &ctx->regs[33]);
	__put_user(regs->cp0_cause, &ctx->regs[34]);
	__put_user(regs->cp0_epc, &ctx->regs[35]);

	flags = 0x0f;
	if(!current->used_math) {
		flags &= ~(0x08);
	} else {
		/* XXX wheee... */
		printk("Wheee, no code for saving IRIX FPU context yet.\n");
	}
	__put_user(flags, &ctx->flags);
	error = 0;

out:
	return error;
}

asmlinkage unsigned long irix_setcontext(struct pt_regs *regs)
{
	int error, base = 0;
	struct irix5_context *ctx;

	if(regs->regs[2] == 1000)
		base = 1;
	ctx = (struct irix5_context *) regs->regs[base + 4];

#ifdef DEBUG_SIG
	printk("[%s:%d] irix_setcontext(%p)\n",
	       current->comm, current->pid, ctx);
#endif

	error = verify_area(VERIFY_READ, ctx, sizeof(*ctx));
	if (error)
		goto out;

	if (ctx->flags & 0x02) {
		/* XXX sigstack garbage, todo... */
		printk("Wheee, cannot do sigstack stuff in setcontext\n");
	}

	if (ctx->flags & 0x04) {
		int i;

		/* XXX extra control block stuff... todo... */
		for(i = 1; i < 32; i++)
			regs->regs[i] = ctx->regs[i];
		regs->lo = ctx->regs[32];
		regs->hi = ctx->regs[33];
		regs->cp0_epc = ctx->regs[35];
	}

	if (ctx->flags & 0x08) {
		/* XXX fpu context, blah... */
		printk("Wheee, cannot restore FPU context yet...\n");
	}
	current->thread.irix_oldctx = ctx->link;
	error = regs->regs[2];

out:
	return error;
}

struct irix_sigstack { unsigned long sp; int status; };

asmlinkage int irix_sigstack(struct irix_sigstack *new, struct irix_sigstack *old)
{
	int error;

#ifdef DEBUG_SIG
	printk("[%s:%d] irix_sigstack(%p,%p)\n",
	       current->comm, current->pid, new, old);
#endif
	if(new) {
		error = verify_area(VERIFY_READ, new, sizeof(*new));
		if(error)
			goto out;
	}

	if(old) {
		error = verify_area(VERIFY_WRITE, old, sizeof(*old));
		if(error)
			goto out;
	}
	error = 0;

out:
	return error;
}

struct irix_sigaltstack { unsigned long sp; int size; int status; };

asmlinkage int irix_sigaltstack(struct irix_sigaltstack *new,
				struct irix_sigaltstack *old)
{
	int error;

#ifdef DEBUG_SIG
	printk("[%s:%d] irix_sigaltstack(%p,%p)\n",
	       current->comm, current->pid, new, old);
#endif
	if (new) {
		error = verify_area(VERIFY_READ, new, sizeof(*new));
		if(error)
			goto out;
	}

	if (old) {
		error = verify_area(VERIFY_WRITE, old, sizeof(*old));
		if(error)
			goto out;
	}
	error = 0;

out:
	error = 0;

	return error;
}

struct irix_procset {
	int cmd, ltype, lid, rtype, rid;
};

asmlinkage int irix_sigsendset(struct irix_procset *pset, int sig)
{
	int error;

	error = verify_area(VERIFY_READ, pset, sizeof(*pset));
	if(error)
		goto out;
#ifdef DEBUG_SIG
	printk("[%s:%d] irix_sigsendset([%d,%d,%d,%d,%d],%d)\n",
	       current->comm, current->pid,
	       pset->cmd, pset->ltype, pset->lid, pset->rtype, pset->rid,
	       sig);
#endif
	error = -EINVAL;

out:
	return error;
}
