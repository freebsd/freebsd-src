/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *
 *	@(#)kern_sig.c	8.7 (Berkeley) 4/18/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/acct.h>
#include <sys/condvar.h>
#include <sys/event.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/kse.h>
#include <sys/ktr.h>
#include <sys/ktrace.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/pioctl.h>
#include <sys/resourcevar.h>
#include <sys/smp.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/syslog.h>
#include <sys/sysproto.h>
#include <sys/unistd.h>
#include <sys/wait.h>

#include <machine/cpu.h>

#if defined (__alpha__) && !defined(COMPAT_43)
#error "You *really* need COMPAT_43 on the alpha for longjmp(3)"
#endif

#define	ONSIG	32		/* NSIG for osig* syscalls.  XXX. */

static int	coredump(struct thread *);
static char	*expand_name(const char *, uid_t, pid_t);
static int	killpg1(struct thread *td, int sig, int pgid, int all);
static int	issignal(struct thread *p);
static int	sigprop(int sig);
static void	stop(struct proc *);
static void	tdsigwakeup(struct thread *td, int sig, sig_t action);
static int	filt_sigattach(struct knote *kn);
static void	filt_sigdetach(struct knote *kn);
static int	filt_signal(struct knote *kn, long hint);
static struct thread *sigtd(struct proc *p, int sig, int prop);
static int	kern_sigtimedwait(struct thread *td, sigset_t set,
				siginfo_t *info, struct timespec *timeout);
static void	do_tdsignal(struct thread *td, int sig, sigtarget_t target);

struct filterops sig_filtops =
	{ 0, filt_sigattach, filt_sigdetach, filt_signal };

static int	kern_logsigexit = 1;
SYSCTL_INT(_kern, KERN_LOGSIGEXIT, logsigexit, CTLFLAG_RW, 
    &kern_logsigexit, 0, 
    "Log processes quitting on abnormal signals to syslog(3)");

/*
 * Policy -- Can ucred cr1 send SIGIO to process cr2?
 * Should use cr_cansignal() once cr_cansignal() allows SIGIO and SIGURG
 * in the right situations.
 */
#define CANSIGIO(cr1, cr2) \
	((cr1)->cr_uid == 0 || \
	    (cr1)->cr_ruid == (cr2)->cr_ruid || \
	    (cr1)->cr_uid == (cr2)->cr_ruid || \
	    (cr1)->cr_ruid == (cr2)->cr_uid || \
	    (cr1)->cr_uid == (cr2)->cr_uid)

int sugid_coredump;
SYSCTL_INT(_kern, OID_AUTO, sugid_coredump, CTLFLAG_RW, 
    &sugid_coredump, 0, "Enable coredumping set user/group ID processes");

static int	do_coredump = 1;
SYSCTL_INT(_kern, OID_AUTO, coredump, CTLFLAG_RW,
	&do_coredump, 0, "Enable/Disable coredumps");

/*
 * Signal properties and actions.
 * The array below categorizes the signals and their default actions
 * according to the following properties:
 */
#define	SA_KILL		0x01		/* terminates process by default */
#define	SA_CORE		0x02		/* ditto and coredumps */
#define	SA_STOP		0x04		/* suspend process */
#define	SA_TTYSTOP	0x08		/* ditto, from tty */
#define	SA_IGNORE	0x10		/* ignore by default */
#define	SA_CONT		0x20		/* continue if suspended */
#define	SA_CANTMASK	0x40		/* non-maskable, catchable */
#define	SA_PROC		0x80		/* deliverable to any thread */

static int sigproptbl[NSIG] = {
        SA_KILL|SA_PROC,		/* SIGHUP */
        SA_KILL|SA_PROC,		/* SIGINT */
        SA_KILL|SA_CORE|SA_PROC,	/* SIGQUIT */
        SA_KILL|SA_CORE,		/* SIGILL */
        SA_KILL|SA_CORE,		/* SIGTRAP */
        SA_KILL|SA_CORE,		/* SIGABRT */
        SA_KILL|SA_CORE|SA_PROC,	/* SIGEMT */
        SA_KILL|SA_CORE,		/* SIGFPE */
        SA_KILL|SA_PROC,		/* SIGKILL */
        SA_KILL|SA_CORE,		/* SIGBUS */
        SA_KILL|SA_CORE,		/* SIGSEGV */
        SA_KILL|SA_CORE,		/* SIGSYS */
        SA_KILL|SA_PROC,		/* SIGPIPE */
        SA_KILL|SA_PROC,		/* SIGALRM */
        SA_KILL|SA_PROC,		/* SIGTERM */
        SA_IGNORE|SA_PROC,		/* SIGURG */
        SA_STOP|SA_PROC,		/* SIGSTOP */
        SA_STOP|SA_TTYSTOP|SA_PROC,	/* SIGTSTP */
        SA_IGNORE|SA_CONT|SA_PROC,	/* SIGCONT */
        SA_IGNORE|SA_PROC,		/* SIGCHLD */
        SA_STOP|SA_TTYSTOP|SA_PROC,	/* SIGTTIN */
        SA_STOP|SA_TTYSTOP|SA_PROC,	/* SIGTTOU */
        SA_IGNORE|SA_PROC,		/* SIGIO */
        SA_KILL,			/* SIGXCPU */
        SA_KILL,			/* SIGXFSZ */
        SA_KILL|SA_PROC,		/* SIGVTALRM */
        SA_KILL|SA_PROC,		/* SIGPROF */
        SA_IGNORE|SA_PROC,		/* SIGWINCH  */
        SA_IGNORE|SA_PROC,		/* SIGINFO */
        SA_KILL|SA_PROC,		/* SIGUSR1 */
        SA_KILL|SA_PROC,		/* SIGUSR2 */
};

/*
 * Determine signal that should be delivered to process p, the current
 * process, 0 if none.  If there is a pending stop signal with default
 * action, the process stops in issignal().
 * XXXKSE   the check for a pending stop is not done under KSE
 *
 * MP SAFE.
 */
int
cursig(struct thread *td)
{
	PROC_LOCK_ASSERT(td->td_proc, MA_OWNED);
	mtx_assert(&td->td_proc->p_sigacts->ps_mtx, MA_OWNED);
	mtx_assert(&sched_lock, MA_NOTOWNED);
	return (SIGPENDING(td) ? issignal(td) : 0);
}

/*
 * Arrange for ast() to handle unmasked pending signals on return to user
 * mode.  This must be called whenever a signal is added to td_siglist or
 * unmasked in td_sigmask.
 */
void
signotify(struct thread *td)
{
	struct proc *p;
	sigset_t set, saved;

	p = td->td_proc;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	/*
	 * If our mask changed we may have to move signal that were
	 * previously masked by all threads to our siglist.
	 */
	set = p->p_siglist;
	if (p->p_flag & P_SA)
		saved = p->p_siglist;
	SIGSETNAND(set, td->td_sigmask);
	SIGSETNAND(p->p_siglist, set);
	SIGSETOR(td->td_siglist, set);

	if (SIGPENDING(td)) {
		mtx_lock_spin(&sched_lock);
		td->td_flags |= TDF_NEEDSIGCHK | TDF_ASTPENDING;
		mtx_unlock_spin(&sched_lock);
	}
	if ((p->p_flag & P_SA) && !(p->p_flag & P_SIGEVENT)) {
		if (SIGSETEQ(saved, p->p_siglist))
			return;
		else {
			/* pending set changed */
			p->p_flag |= P_SIGEVENT;
			wakeup(&p->p_siglist);
		}
	}
}

int
sigonstack(size_t sp)
{
	struct proc *p = curthread->td_proc;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	return ((p->p_flag & P_ALTSTACK) ?
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	    ((p->p_sigstk.ss_size == 0) ? (p->p_sigstk.ss_flags & SS_ONSTACK) :
		((sp - (size_t)p->p_sigstk.ss_sp) < p->p_sigstk.ss_size))
#else
	    ((sp - (size_t)p->p_sigstk.ss_sp) < p->p_sigstk.ss_size)
#endif
	    : 0);
}

static __inline int
sigprop(int sig)
{

	if (sig > 0 && sig < NSIG)
		return (sigproptbl[_SIG_IDX(sig)]);
	return (0);
}

int
sig_ffs(sigset_t *set)
{
	int i;

	for (i = 0; i < _SIG_WORDS; i++)
		if (set->__bits[i])
			return (ffs(set->__bits[i]) + (i * 32));
	return (0);
}

/*
 * kern_sigaction
 * sigaction
 * freebsd4_sigaction
 * osigaction
 *
 * MPSAFE
 */
int
kern_sigaction(td, sig, act, oact, flags)
	struct thread *td;
	register int sig;
	struct sigaction *act, *oact;
	int flags;
{
	struct sigacts *ps;
	struct thread *td0;
	struct proc *p = td->td_proc;

	if (!_SIG_VALID(sig))
		return (EINVAL);

	PROC_LOCK(p);
	ps = p->p_sigacts;
	mtx_lock(&ps->ps_mtx);
	if (oact) {
		oact->sa_handler = ps->ps_sigact[_SIG_IDX(sig)];
		oact->sa_mask = ps->ps_catchmask[_SIG_IDX(sig)];
		oact->sa_flags = 0;
		if (SIGISMEMBER(ps->ps_sigonstack, sig))
			oact->sa_flags |= SA_ONSTACK;
		if (!SIGISMEMBER(ps->ps_sigintr, sig))
			oact->sa_flags |= SA_RESTART;
		if (SIGISMEMBER(ps->ps_sigreset, sig))
			oact->sa_flags |= SA_RESETHAND;
		if (SIGISMEMBER(ps->ps_signodefer, sig))
			oact->sa_flags |= SA_NODEFER;
		if (SIGISMEMBER(ps->ps_siginfo, sig))
			oact->sa_flags |= SA_SIGINFO;
		if (sig == SIGCHLD && ps->ps_flag & PS_NOCLDSTOP)
			oact->sa_flags |= SA_NOCLDSTOP;
		if (sig == SIGCHLD && ps->ps_flag & PS_NOCLDWAIT)
			oact->sa_flags |= SA_NOCLDWAIT;
	}
	if (act) {
		if ((sig == SIGKILL || sig == SIGSTOP) &&
		    act->sa_handler != SIG_DFL) {
			mtx_unlock(&ps->ps_mtx);
			PROC_UNLOCK(p);
			return (EINVAL);
		}

		/*
		 * Change setting atomically.
		 */

		ps->ps_catchmask[_SIG_IDX(sig)] = act->sa_mask;
		SIG_CANTMASK(ps->ps_catchmask[_SIG_IDX(sig)]);
		if (act->sa_flags & SA_SIGINFO) {
			ps->ps_sigact[_SIG_IDX(sig)] =
			    (__sighandler_t *)act->sa_sigaction;
			SIGADDSET(ps->ps_siginfo, sig);
		} else {
			ps->ps_sigact[_SIG_IDX(sig)] = act->sa_handler;
			SIGDELSET(ps->ps_siginfo, sig);
		}
		if (!(act->sa_flags & SA_RESTART))
			SIGADDSET(ps->ps_sigintr, sig);
		else
			SIGDELSET(ps->ps_sigintr, sig);
		if (act->sa_flags & SA_ONSTACK)
			SIGADDSET(ps->ps_sigonstack, sig);
		else
			SIGDELSET(ps->ps_sigonstack, sig);
		if (act->sa_flags & SA_RESETHAND)
			SIGADDSET(ps->ps_sigreset, sig);
		else
			SIGDELSET(ps->ps_sigreset, sig);
		if (act->sa_flags & SA_NODEFER)
			SIGADDSET(ps->ps_signodefer, sig);
		else
			SIGDELSET(ps->ps_signodefer, sig);
#ifdef COMPAT_SUNOS
		if (act->sa_flags & SA_USERTRAMP)
			SIGADDSET(ps->ps_usertramp, sig);
		else
			SIGDELSET(ps->ps_usertramp, sig);
#endif
		if (sig == SIGCHLD) {
			if (act->sa_flags & SA_NOCLDSTOP)
				ps->ps_flag |= PS_NOCLDSTOP;
			else
				ps->ps_flag &= ~PS_NOCLDSTOP;
			if (act->sa_flags & SA_NOCLDWAIT) {
				/*
				 * Paranoia: since SA_NOCLDWAIT is implemented
				 * by reparenting the dying child to PID 1 (and
				 * trust it to reap the zombie), PID 1 itself
				 * is forbidden to set SA_NOCLDWAIT.
				 */
				if (p->p_pid == 1)
					ps->ps_flag &= ~PS_NOCLDWAIT;
				else
					ps->ps_flag |= PS_NOCLDWAIT;
			} else
				ps->ps_flag &= ~PS_NOCLDWAIT;
			if (ps->ps_sigact[_SIG_IDX(SIGCHLD)] == SIG_IGN)
				ps->ps_flag |= PS_CLDSIGIGN;
			else
				ps->ps_flag &= ~PS_CLDSIGIGN;
		}
		/*
		 * Set bit in ps_sigignore for signals that are set to SIG_IGN,
		 * and for signals set to SIG_DFL where the default is to
		 * ignore. However, don't put SIGCONT in ps_sigignore, as we
		 * have to restart the process.
		 */
		if (ps->ps_sigact[_SIG_IDX(sig)] == SIG_IGN ||
		    (sigprop(sig) & SA_IGNORE &&
		     ps->ps_sigact[_SIG_IDX(sig)] == SIG_DFL)) {
			if ((p->p_flag & P_SA) &&
			     SIGISMEMBER(p->p_siglist, sig)) {
				p->p_flag |= P_SIGEVENT;
				wakeup(&p->p_siglist);
			}
			/* never to be seen again */
			SIGDELSET(p->p_siglist, sig);
			FOREACH_THREAD_IN_PROC(p, td0)
				SIGDELSET(td0->td_siglist, sig);
			if (sig != SIGCONT)
				/* easier in psignal */
				SIGADDSET(ps->ps_sigignore, sig);
			SIGDELSET(ps->ps_sigcatch, sig);
		} else {
			SIGDELSET(ps->ps_sigignore, sig);
			if (ps->ps_sigact[_SIG_IDX(sig)] == SIG_DFL)
				SIGDELSET(ps->ps_sigcatch, sig);
			else
				SIGADDSET(ps->ps_sigcatch, sig);
		}
#ifdef COMPAT_FREEBSD4
		if (ps->ps_sigact[_SIG_IDX(sig)] == SIG_IGN ||
		    ps->ps_sigact[_SIG_IDX(sig)] == SIG_DFL ||
		    (flags & KSA_FREEBSD4) == 0)
			SIGDELSET(ps->ps_freebsd4, sig);
		else
			SIGADDSET(ps->ps_freebsd4, sig);
#endif
#ifdef COMPAT_43
		if (ps->ps_sigact[_SIG_IDX(sig)] == SIG_IGN ||
		    ps->ps_sigact[_SIG_IDX(sig)] == SIG_DFL ||
		    (flags & KSA_OSIGSET) == 0)
			SIGDELSET(ps->ps_osigset, sig);
		else
			SIGADDSET(ps->ps_osigset, sig);
#endif
	}
	mtx_unlock(&ps->ps_mtx);
	PROC_UNLOCK(p);
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct sigaction_args {
	int	sig;
	struct	sigaction *act;
	struct	sigaction *oact;
};
#endif
/*
 * MPSAFE
 */
int
sigaction(td, uap)
	struct thread *td;
	register struct sigaction_args *uap;
{
	struct sigaction act, oact;
	register struct sigaction *actp, *oactp;
	int error;

	actp = (uap->act != NULL) ? &act : NULL;
	oactp = (uap->oact != NULL) ? &oact : NULL;
	if (actp) {
		error = copyin(uap->act, actp, sizeof(act));
		if (error)
			return (error);
	}
	error = kern_sigaction(td, uap->sig, actp, oactp, 0);
	if (oactp && !error)
		error = copyout(oactp, uap->oact, sizeof(oact));
	return (error);
}

#ifdef COMPAT_FREEBSD4
#ifndef _SYS_SYSPROTO_H_
struct freebsd4_sigaction_args {
	int	sig;
	struct	sigaction *act;
	struct	sigaction *oact;
};
#endif
/*
 * MPSAFE
 */
int
freebsd4_sigaction(td, uap)
	struct thread *td;
	register struct freebsd4_sigaction_args *uap;
{
	struct sigaction act, oact;
	register struct sigaction *actp, *oactp;
	int error;


	actp = (uap->act != NULL) ? &act : NULL;
	oactp = (uap->oact != NULL) ? &oact : NULL;
	if (actp) {
		error = copyin(uap->act, actp, sizeof(act));
		if (error)
			return (error);
	}
	error = kern_sigaction(td, uap->sig, actp, oactp, KSA_FREEBSD4);
	if (oactp && !error)
		error = copyout(oactp, uap->oact, sizeof(oact));
	return (error);
}
#endif	/* COMAPT_FREEBSD4 */

#ifdef COMPAT_43	/* XXX - COMPAT_FBSD3 */
#ifndef _SYS_SYSPROTO_H_
struct osigaction_args {
	int	signum;
	struct	osigaction *nsa;
	struct	osigaction *osa;
};
#endif
/*
 * MPSAFE
 */
int
osigaction(td, uap)
	struct thread *td;
	register struct osigaction_args *uap;
{
	struct osigaction sa;
	struct sigaction nsa, osa;
	register struct sigaction *nsap, *osap;
	int error;

	if (uap->signum <= 0 || uap->signum >= ONSIG)
		return (EINVAL);

	nsap = (uap->nsa != NULL) ? &nsa : NULL;
	osap = (uap->osa != NULL) ? &osa : NULL;

	if (nsap) {
		error = copyin(uap->nsa, &sa, sizeof(sa));
		if (error)
			return (error);
		nsap->sa_handler = sa.sa_handler;
		nsap->sa_flags = sa.sa_flags;
		OSIG2SIG(sa.sa_mask, nsap->sa_mask);
	}
	error = kern_sigaction(td, uap->signum, nsap, osap, KSA_OSIGSET);
	if (osap && !error) {
		sa.sa_handler = osap->sa_handler;
		sa.sa_flags = osap->sa_flags;
		SIG2OSIG(osap->sa_mask, sa.sa_mask);
		error = copyout(&sa, uap->osa, sizeof(sa));
	}
	return (error);
}

#if !defined(__i386__) && !defined(__alpha__)
/* Avoid replicating the same stub everywhere */
int
osigreturn(td, uap)
	struct thread *td;
	struct osigreturn_args *uap;
{

	return (nosys(td, (struct nosys_args *)uap));
}
#endif
#endif /* COMPAT_43 */

/*
 * Initialize signal state for process 0;
 * set to ignore signals that are ignored by default.
 */
void
siginit(p)
	struct proc *p;
{
	register int i;
	struct sigacts *ps;

	PROC_LOCK(p);
	ps = p->p_sigacts;
	mtx_lock(&ps->ps_mtx);
	for (i = 1; i <= NSIG; i++)
		if (sigprop(i) & SA_IGNORE && i != SIGCONT)
			SIGADDSET(ps->ps_sigignore, i);
	mtx_unlock(&ps->ps_mtx);
	PROC_UNLOCK(p);
}

/*
 * Reset signals for an exec of the specified process.
 */
void
execsigs(p)
	register struct proc *p;
{
	register struct sigacts *ps;
	register int sig;

	/*
	 * Reset caught signals.  Held signals remain held
	 * through td_sigmask (unless they were caught,
	 * and are now ignored by default).
	 */
	PROC_LOCK_ASSERT(p, MA_OWNED);
	ps = p->p_sigacts;
	mtx_lock(&ps->ps_mtx);
	while (SIGNOTEMPTY(ps->ps_sigcatch)) {
		sig = sig_ffs(&ps->ps_sigcatch);
		SIGDELSET(ps->ps_sigcatch, sig);
		if (sigprop(sig) & SA_IGNORE) {
			if (sig != SIGCONT)
				SIGADDSET(ps->ps_sigignore, sig);
			SIGDELSET(p->p_siglist, sig);
			/*
			 * There is only one thread at this point.
			 */
			SIGDELSET(FIRST_THREAD_IN_PROC(p)->td_siglist, sig);
		}
		ps->ps_sigact[_SIG_IDX(sig)] = SIG_DFL;
	}
	/*
	 * Reset stack state to the user stack.
	 * Clear set of signals caught on the signal stack.
	 */
	p->p_sigstk.ss_flags = SS_DISABLE;
	p->p_sigstk.ss_size = 0;
	p->p_sigstk.ss_sp = 0;
	p->p_flag &= ~P_ALTSTACK;
	/*
	 * Reset no zombies if child dies flag as Solaris does.
	 */
	ps->ps_flag &= ~(PS_NOCLDWAIT | PS_CLDSIGIGN);
	if (ps->ps_sigact[_SIG_IDX(SIGCHLD)] == SIG_IGN)
		ps->ps_sigact[_SIG_IDX(SIGCHLD)] = SIG_DFL;
	mtx_unlock(&ps->ps_mtx);
}

/*
 * kern_sigprocmask()
 *
 *	Manipulate signal mask.
 */
int
kern_sigprocmask(td, how, set, oset, old)
	struct thread *td;
	int how;
	sigset_t *set, *oset;
	int old;
{
	int error;

	PROC_LOCK(td->td_proc);
	if (oset != NULL)
		*oset = td->td_sigmask;

	error = 0;
	if (set != NULL) {
		switch (how) {
		case SIG_BLOCK:
			SIG_CANTMASK(*set);
			SIGSETOR(td->td_sigmask, *set);
			break;
		case SIG_UNBLOCK:
			SIGSETNAND(td->td_sigmask, *set);
			signotify(td);
			break;
		case SIG_SETMASK:
			SIG_CANTMASK(*set);
			if (old)
				SIGSETLO(td->td_sigmask, *set);
			else
				td->td_sigmask = *set;
			signotify(td);
			break;
		default:
			error = EINVAL;
			break;
		}
	}
	PROC_UNLOCK(td->td_proc);
	return (error);
}

/*
 * sigprocmask() - MP SAFE
 */

#ifndef _SYS_SYSPROTO_H_
struct sigprocmask_args {
	int	how;
	const sigset_t *set;
	sigset_t *oset;
};
#endif
int
sigprocmask(td, uap)
	register struct thread *td;
	struct sigprocmask_args *uap;
{
	sigset_t set, oset;
	sigset_t *setp, *osetp;
	int error;

	setp = (uap->set != NULL) ? &set : NULL;
	osetp = (uap->oset != NULL) ? &oset : NULL;
	if (setp) {
		error = copyin(uap->set, setp, sizeof(set));
		if (error)
			return (error);
	}
	error = kern_sigprocmask(td, uap->how, setp, osetp, 0);
	if (osetp && !error) {
		error = copyout(osetp, uap->oset, sizeof(oset));
	}
	return (error);
}

#ifdef COMPAT_43	/* XXX - COMPAT_FBSD3 */
/*
 * osigprocmask() - MP SAFE
 */
#ifndef _SYS_SYSPROTO_H_
struct osigprocmask_args {
	int	how;
	osigset_t mask;
};
#endif
int
osigprocmask(td, uap)
	register struct thread *td;
	struct osigprocmask_args *uap;
{
	sigset_t set, oset;
	int error;

	OSIG2SIG(uap->mask, set);
	error = kern_sigprocmask(td, uap->how, &set, &oset, 1);
	SIG2OSIG(oset, td->td_retval[0]);
	return (error);
}
#endif /* COMPAT_43 */

#ifndef _SYS_SYSPROTO_H_
struct sigpending_args {
	sigset_t	*set;
};
#endif
/*
 * MPSAFE
 */
int
sigwait(struct thread *td, struct sigwait_args *uap)
{
	siginfo_t info;
	sigset_t set;
	int error;

	error = copyin(uap->set, &set, sizeof(set));
	if (error)
		return (error);

	error = kern_sigtimedwait(td, set, &info, NULL);
	if (error)
		return (error);

	error = copyout(&info.si_signo, uap->sig, sizeof(info.si_signo));
	/* Repost if we got an error. */
	if (error && info.si_signo) {
		PROC_LOCK(td->td_proc);
		tdsignal(td, info.si_signo, SIGTARGET_TD);
		PROC_UNLOCK(td->td_proc);
	}
	return (error);
}
/*
 * MPSAFE
 */
int
sigtimedwait(struct thread *td, struct sigtimedwait_args *uap)
{
	struct timespec ts;
	struct timespec *timeout;
	sigset_t set;
	siginfo_t info;
	int error;

	if (uap->timeout) {
		error = copyin(uap->timeout, &ts, sizeof(ts));
		if (error)
			return (error);

		timeout = &ts;
	} else
		timeout = NULL;

	error = copyin(uap->set, &set, sizeof(set));
	if (error)
		return (error);

	error = kern_sigtimedwait(td, set, &info, timeout);
	if (error)
		return (error);

	if (uap->info)
		error = copyout(&info, uap->info, sizeof(info));
	/* Repost if we got an error. */
	if (error && info.si_signo) {
		PROC_LOCK(td->td_proc);
		tdsignal(td, info.si_signo, SIGTARGET_TD);
		PROC_UNLOCK(td->td_proc);
	} else {
		td->td_retval[0] = info.si_signo; 
	}
	return (error);
}

/*
 * MPSAFE
 */
int
sigwaitinfo(struct thread *td, struct sigwaitinfo_args *uap)
{
	siginfo_t info;
	sigset_t set;
	int error;

	error = copyin(uap->set, &set, sizeof(set));
	if (error)
		return (error);

	error = kern_sigtimedwait(td, set, &info, NULL);
	if (error)
		return (error);

	if (uap->info)
		error = copyout(&info, uap->info, sizeof(info));
	/* Repost if we got an error. */
	if (error && info.si_signo) {
		PROC_LOCK(td->td_proc);
		tdsignal(td, info.si_signo, SIGTARGET_TD);
		PROC_UNLOCK(td->td_proc);
	} else {
		td->td_retval[0] = info.si_signo;
	}
	return (error);
}

static int
kern_sigtimedwait(struct thread *td, sigset_t waitset, siginfo_t *info,
    struct timespec *timeout)
{
	struct sigacts *ps;
	sigset_t savedmask, sigset;
	struct proc *p;
	int error;
	int sig;
	int hz;
	int i;

	p = td->td_proc;
	error = 0;
	sig = 0;
	SIG_CANTMASK(waitset);

	PROC_LOCK(p);
	ps = p->p_sigacts;
	savedmask = td->td_sigmask;

again:
	for (i = 1; i <= _SIG_MAXSIG; ++i) {
		if (!SIGISMEMBER(waitset, i))
			continue;
		if (SIGISMEMBER(td->td_siglist, i)) {
			SIGFILLSET(td->td_sigmask);
			SIG_CANTMASK(td->td_sigmask);
			SIGDELSET(td->td_sigmask, i);
			mtx_lock(&ps->ps_mtx);
			sig = cursig(td);
			i = 0;
			mtx_unlock(&ps->ps_mtx);
		} else if (SIGISMEMBER(p->p_siglist, i)) {
			if (p->p_flag & P_SA) {
				p->p_flag |= P_SIGEVENT;
				wakeup(&p->p_siglist);
			}
			SIGDELSET(p->p_siglist, i);
			SIGADDSET(td->td_siglist, i);
			SIGFILLSET(td->td_sigmask);
			SIG_CANTMASK(td->td_sigmask);
			SIGDELSET(td->td_sigmask, i);
			mtx_lock(&ps->ps_mtx);
			sig = cursig(td);
			i = 0;
			mtx_unlock(&ps->ps_mtx);
		}
		if (sig) {
			td->td_sigmask = savedmask;
			signotify(td);
			goto out;
		}
	}
	if (error)
		goto out;

	td->td_sigmask = savedmask;
	signotify(td);
	sigset = td->td_siglist;
	SIGSETOR(sigset, p->p_siglist);
	SIGSETAND(sigset, waitset);
	if (!SIGISEMPTY(sigset))
		goto again;

	/*
	 * POSIX says this must be checked after looking for pending
	 * signals.
	 */
	if (timeout) {
		struct timeval tv;

		if (timeout->tv_nsec < 0 || timeout->tv_nsec > 1000000000) {
			error = EINVAL;
			goto out;
		}
		if (timeout->tv_sec == 0 && timeout->tv_nsec == 0) {
			error = EAGAIN;
			goto out;
		}
		TIMESPEC_TO_TIMEVAL(&tv, timeout);
		hz = tvtohz(&tv);
	} else
		hz = 0;

	td->td_waitset = &waitset;
	error = msleep(ps, &p->p_mtx, PPAUSE|PCATCH, "sigwait", hz);
	td->td_waitset = NULL;
	if (error == 0) /* surplus wakeup ? */
		error = EINTR;
	goto again;

out:
	if (sig) {
		sig_t action;

		error = 0;
		mtx_lock(&ps->ps_mtx);
		action = ps->ps_sigact[_SIG_IDX(sig)];
		mtx_unlock(&ps->ps_mtx);
#ifdef KTRACE
		if (KTRPOINT(td, KTR_PSIG))
			ktrpsig(sig, action, &td->td_sigmask, 0);
#endif
		_STOPEVENT(p, S_SIG, sig);

		SIGDELSET(td->td_siglist, sig);
		info->si_signo = sig;
		info->si_code = 0;
	}
	PROC_UNLOCK(p);
	return (error);
}

/*
 * MPSAFE
 */
int
sigpending(td, uap)
	struct thread *td;
	struct sigpending_args *uap;
{
	struct proc *p = td->td_proc;
	sigset_t siglist;

	PROC_LOCK(p);
	siglist = p->p_siglist;
	SIGSETOR(siglist, td->td_siglist);
	PROC_UNLOCK(p);
	return (copyout(&siglist, uap->set, sizeof(sigset_t)));
}

#ifdef COMPAT_43	/* XXX - COMPAT_FBSD3 */
#ifndef _SYS_SYSPROTO_H_
struct osigpending_args {
	int	dummy;
};
#endif
/*
 * MPSAFE
 */
int
osigpending(td, uap)
	struct thread *td;
	struct osigpending_args *uap;
{
	struct proc *p = td->td_proc;
	sigset_t siglist;

	PROC_LOCK(p);
	siglist = p->p_siglist;
	SIGSETOR(siglist, td->td_siglist);
	PROC_UNLOCK(p);
	SIG2OSIG(siglist, td->td_retval[0]);
	return (0);
}
#endif /* COMPAT_43 */

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
/*
 * Generalized interface signal handler, 4.3-compatible.
 */
#ifndef _SYS_SYSPROTO_H_
struct osigvec_args {
	int	signum;
	struct	sigvec *nsv;
	struct	sigvec *osv;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
osigvec(td, uap)
	struct thread *td;
	register struct osigvec_args *uap;
{
	struct sigvec vec;
	struct sigaction nsa, osa;
	register struct sigaction *nsap, *osap;
	int error;

	if (uap->signum <= 0 || uap->signum >= ONSIG)
		return (EINVAL);
	nsap = (uap->nsv != NULL) ? &nsa : NULL;
	osap = (uap->osv != NULL) ? &osa : NULL;
	if (nsap) {
		error = copyin(uap->nsv, &vec, sizeof(vec));
		if (error)
			return (error);
		nsap->sa_handler = vec.sv_handler;
		OSIG2SIG(vec.sv_mask, nsap->sa_mask);
		nsap->sa_flags = vec.sv_flags;
		nsap->sa_flags ^= SA_RESTART;	/* opposite of SV_INTERRUPT */
#ifdef COMPAT_SUNOS
		nsap->sa_flags |= SA_USERTRAMP;
#endif
	}
	error = kern_sigaction(td, uap->signum, nsap, osap, KSA_OSIGSET);
	if (osap && !error) {
		vec.sv_handler = osap->sa_handler;
		SIG2OSIG(osap->sa_mask, vec.sv_mask);
		vec.sv_flags = osap->sa_flags;
		vec.sv_flags &= ~SA_NOCLDWAIT;
		vec.sv_flags ^= SA_RESTART;
#ifdef COMPAT_SUNOS
		vec.sv_flags &= ~SA_NOCLDSTOP;
#endif
		error = copyout(&vec, uap->osv, sizeof(vec));
	}
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct osigblock_args {
	int	mask;
};
#endif
/*
 * MPSAFE
 */
int
osigblock(td, uap)
	register struct thread *td;
	struct osigblock_args *uap;
{
	struct proc *p = td->td_proc;
	sigset_t set;

	OSIG2SIG(uap->mask, set);
	SIG_CANTMASK(set);
	PROC_LOCK(p);
	SIG2OSIG(td->td_sigmask, td->td_retval[0]);
	SIGSETOR(td->td_sigmask, set);
	PROC_UNLOCK(p);
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct osigsetmask_args {
	int	mask;
};
#endif
/*
 * MPSAFE
 */
int
osigsetmask(td, uap)
	struct thread *td;
	struct osigsetmask_args *uap;
{
	struct proc *p = td->td_proc;
	sigset_t set;

	OSIG2SIG(uap->mask, set);
	SIG_CANTMASK(set);
	PROC_LOCK(p);
	SIG2OSIG(td->td_sigmask, td->td_retval[0]);
	SIGSETLO(td->td_sigmask, set);
	signotify(td);
	PROC_UNLOCK(p);
	return (0);
}
#endif /* COMPAT_43 || COMPAT_SUNOS */

/*
 * Suspend process until signal, providing mask to be set
 * in the meantime.  Note nonstandard calling convention:
 * libc stub passes mask, not pointer, to save a copyin.
 ***** XXXKSE this doesn't make sense under KSE.
 ***** Do we suspend the thread or all threads in the process?
 ***** How do we suspend threads running NOW on another processor?
 */
#ifndef _SYS_SYSPROTO_H_
struct sigsuspend_args {
	const sigset_t *sigmask;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
sigsuspend(td, uap)
	struct thread *td;
	struct sigsuspend_args *uap;
{
	sigset_t mask;
	int error;

	error = copyin(uap->sigmask, &mask, sizeof(mask));
	if (error)
		return (error);
	return (kern_sigsuspend(td, mask));
}

int
kern_sigsuspend(struct thread *td, sigset_t mask)
{
	struct proc *p = td->td_proc;

	/*
	 * When returning from sigsuspend, we want
	 * the old mask to be restored after the
	 * signal handler has finished.  Thus, we
	 * save it here and mark the sigacts structure
	 * to indicate this.
	 */
	PROC_LOCK(p);
	td->td_oldsigmask = td->td_sigmask;
	td->td_pflags |= TDP_OLDMASK;
	SIG_CANTMASK(mask);
	td->td_sigmask = mask;
	signotify(td);
	while (msleep(p->p_sigacts, &p->p_mtx, PPAUSE|PCATCH, "pause", 0) == 0)
		/* void */;
	PROC_UNLOCK(p);
	/* always return EINTR rather than ERESTART... */
	return (EINTR);
}

#ifdef COMPAT_43	/* XXX - COMPAT_FBSD3 */
#ifndef _SYS_SYSPROTO_H_
struct osigsuspend_args {
	osigset_t mask;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
osigsuspend(td, uap)
	struct thread *td;
	struct osigsuspend_args *uap;
{
	struct proc *p = td->td_proc;
	sigset_t mask;

	PROC_LOCK(p);
	td->td_oldsigmask = td->td_sigmask;
	td->td_pflags |= TDP_OLDMASK;
	OSIG2SIG(uap->mask, mask);
	SIG_CANTMASK(mask);
	SIGSETLO(td->td_sigmask, mask);
	signotify(td);
	while (msleep(p->p_sigacts, &p->p_mtx, PPAUSE|PCATCH, "opause", 0) == 0)
		/* void */;
	PROC_UNLOCK(p);
	/* always return EINTR rather than ERESTART... */
	return (EINTR);
}
#endif /* COMPAT_43 */

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
#ifndef _SYS_SYSPROTO_H_
struct osigstack_args {
	struct	sigstack *nss;
	struct	sigstack *oss;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
osigstack(td, uap)
	struct thread *td;
	register struct osigstack_args *uap;
{
	struct proc *p = td->td_proc;
	struct sigstack nss, oss;
	int error = 0;

	if (uap->nss != NULL) {
		error = copyin(uap->nss, &nss, sizeof(nss));
		if (error)
			return (error);
	}
	PROC_LOCK(p);
	oss.ss_sp = p->p_sigstk.ss_sp;
	oss.ss_onstack = sigonstack(cpu_getstack(td));
	if (uap->nss != NULL) {
		p->p_sigstk.ss_sp = nss.ss_sp;
		p->p_sigstk.ss_size = 0;
		p->p_sigstk.ss_flags |= nss.ss_onstack & SS_ONSTACK;
		p->p_flag |= P_ALTSTACK;
	}
	PROC_UNLOCK(p);
	if (uap->oss != NULL)
		error = copyout(&oss, uap->oss, sizeof(oss));

	return (error);
}
#endif /* COMPAT_43 || COMPAT_SUNOS */

#ifndef _SYS_SYSPROTO_H_
struct sigaltstack_args {
	stack_t	*ss;
	stack_t	*oss;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
sigaltstack(td, uap)
	struct thread *td;
	register struct sigaltstack_args *uap;
{
	stack_t ss, oss;
	int error;

	if (uap->ss != NULL) {
		error = copyin(uap->ss, &ss, sizeof(ss));
		if (error)
			return (error);
	}
	error = kern_sigaltstack(td, (uap->ss != NULL) ? &ss : NULL,
	    (uap->oss != NULL) ? &oss : NULL);
	if (error)
		return (error);
	if (uap->oss != NULL)
		error = copyout(&oss, uap->oss, sizeof(stack_t));
	return (error);
}

int
kern_sigaltstack(struct thread *td, stack_t *ss, stack_t *oss)
{
	struct proc *p = td->td_proc;
	int oonstack;

	PROC_LOCK(p);
	oonstack = sigonstack(cpu_getstack(td));

	if (oss != NULL) {
		*oss = p->p_sigstk;
		oss->ss_flags = (p->p_flag & P_ALTSTACK)
		    ? ((oonstack) ? SS_ONSTACK : 0) : SS_DISABLE;
	}

	if (ss != NULL) {
		if (oonstack) {
			PROC_UNLOCK(p);
			return (EPERM);
		}
		if ((ss->ss_flags & ~SS_DISABLE) != 0) {
			PROC_UNLOCK(p);
			return (EINVAL);
		}
		if (!(ss->ss_flags & SS_DISABLE)) {
			if (ss->ss_size < p->p_sysent->sv_minsigstksz) {
				PROC_UNLOCK(p);
				return (ENOMEM);
			}
			p->p_sigstk = *ss;
			p->p_flag |= P_ALTSTACK;
		} else {
			p->p_flag &= ~P_ALTSTACK;
		}
	}
	PROC_UNLOCK(p);
	return (0);
}

/*
 * Common code for kill process group/broadcast kill.
 * cp is calling process.
 */
static int
killpg1(td, sig, pgid, all)
	register struct thread *td;
	int sig, pgid, all;
{
	register struct proc *p;
	struct pgrp *pgrp;
	int nfound = 0;

	if (all) {
		/*
		 * broadcast
		 */
		sx_slock(&allproc_lock);
		LIST_FOREACH(p, &allproc, p_list) {
			PROC_LOCK(p);
			if (p->p_pid <= 1 || p->p_flag & P_SYSTEM ||
			    p == td->td_proc) {
				PROC_UNLOCK(p);
				continue;
			}
			if (p_cansignal(td, p, sig) == 0) {
				nfound++;
				if (sig)
					psignal(p, sig);
			}
			PROC_UNLOCK(p);
		}
		sx_sunlock(&allproc_lock);
	} else {
		sx_slock(&proctree_lock);
		if (pgid == 0) {
			/*
			 * zero pgid means send to my process group.
			 */
			pgrp = td->td_proc->p_pgrp;
			PGRP_LOCK(pgrp);
		} else {
			pgrp = pgfind(pgid);
			if (pgrp == NULL) {
				sx_sunlock(&proctree_lock);
				return (ESRCH);
			}
		}
		sx_sunlock(&proctree_lock);
		LIST_FOREACH(p, &pgrp->pg_members, p_pglist) {
			PROC_LOCK(p);	      
			if (p->p_pid <= 1 || p->p_flag & P_SYSTEM) {
				PROC_UNLOCK(p);
				continue;
			}
			if (p->p_state == PRS_ZOMBIE) {
				PROC_UNLOCK(p);
				continue;
			}
			if (p_cansignal(td, p, sig) == 0) {
				nfound++;
				if (sig)
					psignal(p, sig);
			}
			PROC_UNLOCK(p);
		}
		PGRP_UNLOCK(pgrp);
	}
	return (nfound ? 0 : ESRCH);
}

#ifndef _SYS_SYSPROTO_H_
struct kill_args {
	int	pid;
	int	signum;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
kill(td, uap)
	register struct thread *td;
	register struct kill_args *uap;
{
	register struct proc *p;
	int error;

	if ((u_int)uap->signum > _SIG_MAXSIG)
		return (EINVAL);

	if (uap->pid > 0) {
		/* kill single process */
		if ((p = pfind(uap->pid)) == NULL)
			return (ESRCH);
		error = p_cansignal(td, p, uap->signum);
		if (error == 0 && uap->signum)
			psignal(p, uap->signum);
		PROC_UNLOCK(p);
		return (error);
	}
	switch (uap->pid) {
	case -1:		/* broadcast signal */
		return (killpg1(td, uap->signum, 0, 1));
	case 0:			/* signal own process group */
		return (killpg1(td, uap->signum, 0, 0));
	default:		/* negative explicit process group */
		return (killpg1(td, uap->signum, -uap->pid, 0));
	}
	/* NOTREACHED */
}

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
#ifndef _SYS_SYSPROTO_H_
struct okillpg_args {
	int	pgid;
	int	signum;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
okillpg(td, uap)
	struct thread *td;
	register struct okillpg_args *uap;
{

	if ((u_int)uap->signum > _SIG_MAXSIG)
		return (EINVAL);
	return (killpg1(td, uap->signum, uap->pgid, 0));
}
#endif /* COMPAT_43 || COMPAT_SUNOS */

/*
 * Send a signal to a process group.
 */
void
gsignal(pgid, sig)
	int pgid, sig;
{
	struct pgrp *pgrp;

	if (pgid != 0) {
		sx_slock(&proctree_lock);
		pgrp = pgfind(pgid);
		sx_sunlock(&proctree_lock);
		if (pgrp != NULL) {
			pgsignal(pgrp, sig, 0);
			PGRP_UNLOCK(pgrp);
		}
	}
}

/*
 * Send a signal to a process group.  If checktty is 1,
 * limit to members which have a controlling terminal.
 */
void
pgsignal(pgrp, sig, checkctty)
	struct pgrp *pgrp;
	int sig, checkctty;
{
	register struct proc *p;

	if (pgrp) {
		PGRP_LOCK_ASSERT(pgrp, MA_OWNED);
		LIST_FOREACH(p, &pgrp->pg_members, p_pglist) {
			PROC_LOCK(p);
			if (checkctty == 0 || p->p_flag & P_CONTROLT)
				psignal(p, sig);
			PROC_UNLOCK(p);
		}
	}
}

/*
 * Send a signal caused by a trap to the current thread.
 * If it will be caught immediately, deliver it with correct code.
 * Otherwise, post it normally.
 *
 * MPSAFE
 */
void
trapsignal(struct thread *td, int sig, u_long code)
{
	struct sigacts *ps;
	struct proc *p;
	siginfo_t siginfo;
	int error;

	p = td->td_proc;
	if (td->td_flags & TDF_SA) {
		thread_user_enter(p, td);
		PROC_LOCK(p);
		if (td->td_mailbox) {
			SIGDELSET(td->td_sigmask, sig);
			mtx_lock_spin(&sched_lock);
			/*
			 * Force scheduling an upcall, so UTS has chance to
			 * process the signal before thread runs again in
			 * userland.
			 */
			if (td->td_upcall)
				td->td_upcall->ku_flags |= KUF_DOUPCALL;
			mtx_unlock_spin(&sched_lock);
		} else {
			/* UTS caused a sync signal */
			p->p_code = code;	/* XXX for core dump/debugger */
			p->p_sig = sig;		/* XXX to verify code */
			sigexit(td, sig);
		}
	} else {
		PROC_LOCK(p);
	}
	ps = p->p_sigacts;
	mtx_lock(&ps->ps_mtx);
	if ((p->p_flag & P_TRACED) == 0 && SIGISMEMBER(ps->ps_sigcatch, sig) &&
	    !SIGISMEMBER(td->td_sigmask, sig)) {
		p->p_stats->p_ru.ru_nsignals++;
#ifdef KTRACE
		if (KTRPOINT(curthread, KTR_PSIG))
			ktrpsig(sig, ps->ps_sigact[_SIG_IDX(sig)],
			    &td->td_sigmask, code);
#endif
		if (!(td->td_flags & TDF_SA))
			(*p->p_sysent->sv_sendsig)(
				ps->ps_sigact[_SIG_IDX(sig)], sig,
				&td->td_sigmask, code);
		else {
			cpu_thread_siginfo(sig, code, &siginfo);
			mtx_unlock(&ps->ps_mtx);
			PROC_UNLOCK(p);
			error = copyout(&siginfo, &td->td_mailbox->tm_syncsig,
			    sizeof(siginfo));
			PROC_LOCK(p);
			/* UTS memory corrupted */
			if (error)
				sigexit(td, SIGILL);
			SIGADDSET(td->td_sigmask, sig);
			mtx_lock(&ps->ps_mtx);
		}
		SIGSETOR(td->td_sigmask, ps->ps_catchmask[_SIG_IDX(sig)]);
		if (!SIGISMEMBER(ps->ps_signodefer, sig))
			SIGADDSET(td->td_sigmask, sig);
		if (SIGISMEMBER(ps->ps_sigreset, sig)) {
			/*
			 * See kern_sigaction() for origin of this code.
			 */
			SIGDELSET(ps->ps_sigcatch, sig);
			if (sig != SIGCONT &&
			    sigprop(sig) & SA_IGNORE)
				SIGADDSET(ps->ps_sigignore, sig);
			ps->ps_sigact[_SIG_IDX(sig)] = SIG_DFL;
		}
		mtx_unlock(&ps->ps_mtx);
	} else {
		mtx_unlock(&ps->ps_mtx);
		p->p_code = code;	/* XXX for core dump/debugger */
		p->p_sig = sig;		/* XXX to verify code */
		tdsignal(td, sig, SIGTARGET_TD);
	}
	PROC_UNLOCK(p);
}

static struct thread *
sigtd(struct proc *p, int sig, int prop)
{
	struct thread *td, *signal_td;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	/*
	 * First find a thread in sigwait state and signal belongs to
	 * its wait set. POSIX's arguments is that speed of delivering signal
	 * to sigwait thread is faster than delivering signal to user stack.
	 * If we can not find sigwait thread, then find the first thread in
	 * the proc that doesn't have this signal masked, an exception is
	 * if current thread is sending signal to its process, and it does not
	 * mask the signal, it should get the signal, this is another fast
	 * way to deliver signal.
	 */
	signal_td = NULL;
	FOREACH_THREAD_IN_PROC(p, td) {
		if (td->td_waitset != NULL &&
		    SIGISMEMBER(*(td->td_waitset), sig))
				return (td);
		if (!SIGISMEMBER(td->td_sigmask, sig)) {
			if (td == curthread)
				signal_td = curthread;
			else if (signal_td == NULL)
				signal_td = td;
		}
	}
	if (signal_td == NULL)
		signal_td = FIRST_THREAD_IN_PROC(p);
	return (signal_td);
}

/*
 * Send the signal to the process.  If the signal has an action, the action
 * is usually performed by the target process rather than the caller; we add
 * the signal to the set of pending signals for the process.
 *
 * Exceptions:
 *   o When a stop signal is sent to a sleeping process that takes the
 *     default action, the process is stopped without awakening it.
 *   o SIGCONT restarts stopped processes (or puts them back to sleep)
 *     regardless of the signal action (eg, blocked or ignored).
 *
 * Other ignored signals are discarded immediately.
 *
 * MPSAFE
 */
void
psignal(struct proc *p, int sig)
{
	struct thread *td;
	int prop;

	if (!_SIG_VALID(sig))
		panic("psignal(): invalid signal");

	PROC_LOCK_ASSERT(p, MA_OWNED);
	prop = sigprop(sig);

	/*
	 * Find a thread to deliver the signal to.
	 */
	td = sigtd(p, sig, prop);

	tdsignal(td, sig, SIGTARGET_P);
}

/*
 * MPSAFE
 */
void
tdsignal(struct thread *td, int sig, sigtarget_t target)
{
	sigset_t saved;
	struct proc *p = td->td_proc;

	if (p->p_flag & P_SA)
		saved = p->p_siglist;
	do_tdsignal(td, sig, target);
	if ((p->p_flag & P_SA) && !(p->p_flag & P_SIGEVENT)) {
		if (SIGSETEQ(saved, p->p_siglist))
			return;
		else {
			/* pending set changed */
			p->p_flag |= P_SIGEVENT;
			wakeup(&p->p_siglist);
		}
	}
}

static void
do_tdsignal(struct thread *td, int sig, sigtarget_t target)
{
	struct proc *p;
	register sig_t action;
	sigset_t *siglist;
	struct thread *td0;
	register int prop;
	struct sigacts *ps;

	if (!_SIG_VALID(sig))
		panic("do_tdsignal(): invalid signal");

	p = td->td_proc;
	ps = p->p_sigacts;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	KNOTE(&p->p_klist, NOTE_SIGNAL | sig);

	prop = sigprop(sig);

	/*
	 * If the signal is blocked and not destined for this thread, then
	 * assign it to the process so that we can find it later in the first
	 * thread that unblocks it.  Otherwise, assign it to this thread now.
	 */
	if (target == SIGTARGET_TD) {
		siglist = &td->td_siglist;
	} else {
		if (!SIGISMEMBER(td->td_sigmask, sig))
			siglist = &td->td_siglist;
		else if (td->td_waitset != NULL &&
			SIGISMEMBER(*(td->td_waitset), sig))
			siglist = &td->td_siglist;
		else
			siglist = &p->p_siglist;
	}

	/*
	 * If proc is traced, always give parent a chance;
	 * if signal event is tracked by procfs, give *that*
	 * a chance, as well.
	 */
	if ((p->p_flag & P_TRACED) || (p->p_stops & S_SIG)) {
		action = SIG_DFL;
	} else {
		/*
		 * If the signal is being ignored,
		 * then we forget about it immediately.
		 * (Note: we don't set SIGCONT in ps_sigignore,
		 * and if it is set to SIG_IGN,
		 * action will be SIG_DFL here.)
		 */
		mtx_lock(&ps->ps_mtx);
		if (SIGISMEMBER(ps->ps_sigignore, sig) ||
		    (p->p_flag & P_WEXIT)) {
			mtx_unlock(&ps->ps_mtx);
			return;
		}
		if (((td->td_waitset == NULL) &&
		     SIGISMEMBER(td->td_sigmask, sig)) ||
		    ((td->td_waitset != NULL) &&
		     SIGISMEMBER(td->td_sigmask, sig) &&
		     !SIGISMEMBER(*(td->td_waitset), sig)))
			action = SIG_HOLD;
		else if (SIGISMEMBER(ps->ps_sigcatch, sig))
			action = SIG_CATCH;
		else
			action = SIG_DFL;
		mtx_unlock(&ps->ps_mtx);
	}

	if (prop & SA_CONT) {
		SIG_STOPSIGMASK(p->p_siglist);
		/*
		 * XXX Should investigate leaving STOP and CONT sigs only in
		 * the proc's siglist.
		 */
		FOREACH_THREAD_IN_PROC(p, td0)
			SIG_STOPSIGMASK(td0->td_siglist);
	}

	if (prop & SA_STOP) {
		/*
		 * If sending a tty stop signal to a member of an orphaned
		 * process group, discard the signal here if the action
		 * is default; don't stop the process below if sleeping,
		 * and don't clear any pending SIGCONT.
		 */
		if ((prop & SA_TTYSTOP) &&
		    (p->p_pgrp->pg_jobc == 0) &&
		    (action == SIG_DFL))
		        return;
		SIG_CONTSIGMASK(p->p_siglist);
		FOREACH_THREAD_IN_PROC(p, td0)
			SIG_CONTSIGMASK(td0->td_siglist);
		p->p_flag &= ~P_CONTINUED;
	}

	SIGADDSET(*siglist, sig);
	signotify(td);			/* uses schedlock */
	if (siglist == &td->td_siglist && (td->td_waitset != NULL) &&
	    action != SIG_HOLD) {
		td->td_waitset = NULL;
	}

	/*
	 * Defer further processing for signals which are held,
	 * except that stopped processes must be continued by SIGCONT.
	 */
	if (action == SIG_HOLD &&
	    !((prop & SA_CONT) && (p->p_flag & P_STOPPED_SIG)))
		return;
	/*
	 * Some signals have a process-wide effect and a per-thread
	 * component.  Most processing occurs when the process next
	 * tries to cross the user boundary, however there are some
	 * times when processing needs to be done immediatly, such as
	 * waking up threads so that they can cross the user boundary.
	 * We try do the per-process part here.
	 */
	if (P_SHOULDSTOP(p)) {
		/*
		 * The process is in stopped mode. All the threads should be
		 * either winding down or already on the suspended queue.
		 */
		if (p->p_flag & P_TRACED) {
			/*
			 * The traced process is already stopped,
			 * so no further action is necessary.
			 * No signal can restart us.
			 */
			goto out;
		}

		if (sig == SIGKILL) {
			/*
			 * SIGKILL sets process running.
			 * It will die elsewhere.
			 * All threads must be restarted.
			 */
			p->p_flag &= ~P_STOPPED;
			goto runfast;
		}

		if (prop & SA_CONT) {
			/*
			 * If SIGCONT is default (or ignored), we continue the
			 * process but don't leave the signal in siglist as
			 * it has no further action.  If SIGCONT is held, we
			 * continue the process and leave the signal in
			 * siglist.  If the process catches SIGCONT, let it
			 * handle the signal itself.  If it isn't waiting on
			 * an event, it goes back to run state.
			 * Otherwise, process goes back to sleep state.
			 */
			p->p_flag &= ~P_STOPPED_SIG;
			p->p_flag |= P_CONTINUED;
			if (action == SIG_DFL) {
				SIGDELSET(*siglist, sig);
			} else if (action == SIG_CATCH) {
				/*
				 * The process wants to catch it so it needs
				 * to run at least one thread, but which one?
				 * It would seem that the answer would be to
				 * run an upcall in the next KSE to run, and
				 * deliver the signal that way. In a NON KSE
				 * process, we need to make sure that the
				 * single thread is runnable asap.
				 * XXXKSE for now however, make them all run.
				 */
				goto runfast;
			}
			/*
			 * The signal is not ignored or caught.
			 */
			mtx_lock_spin(&sched_lock);
			thread_unsuspend(p);
			mtx_unlock_spin(&sched_lock);
			goto out;
		}

		if (prop & SA_STOP) {
			/*
			 * Already stopped, don't need to stop again
			 * (If we did the shell could get confused).
			 * Just make sure the signal STOP bit set.
			 */
			p->p_flag |= P_STOPPED_SIG;
			SIGDELSET(*siglist, sig);
			goto out;
		}

		/*
		 * All other kinds of signals:
		 * If a thread is sleeping interruptibly, simulate a
		 * wakeup so that when it is continued it will be made
		 * runnable and can look at the signal.  However, don't make
		 * the PROCESS runnable, leave it stopped.
		 * It may run a bit until it hits a thread_suspend_check().
		 */
		mtx_lock_spin(&sched_lock);
		if (TD_ON_SLEEPQ(td) && (td->td_flags & TDF_SINTR)) {
			if (td->td_flags & TDF_CVWAITQ)
				cv_abort(td);
			else
				abortsleep(td);
		}
		mtx_unlock_spin(&sched_lock);
		goto out;
		/*
		 * XXXKSE  What about threads that are waiting on mutexes?
		 * Shouldn't they abort too?
		 * No, hopefully mutexes are short lived.. They'll
		 * eventually hit thread_suspend_check().
		 */
	}  else if (p->p_state == PRS_NORMAL) {
		if ((p->p_flag & P_TRACED) || (action != SIG_DFL) ||
			!(prop & SA_STOP)) {
			mtx_lock_spin(&sched_lock);
			tdsigwakeup(td, sig, action);
			mtx_unlock_spin(&sched_lock);
			goto out;
		}
		if (prop & SA_STOP) {
			if (p->p_flag & P_PPWAIT)
				goto out;
			p->p_flag |= P_STOPPED_SIG;
			p->p_xstat = sig;
			mtx_lock_spin(&sched_lock);
			FOREACH_THREAD_IN_PROC(p, td0) {
				if (TD_IS_SLEEPING(td0) &&
				    (td0->td_flags & TDF_SINTR) &&
				    !TD_IS_SUSPENDED(td0)) {
					thread_suspend_one(td0);
				} else if (td != td0) {
					td0->td_flags |= TDF_ASTPENDING;
				}
			}
			thread_stopped(p);
			if (p->p_numthreads == p->p_suspcount) {
				SIGDELSET(p->p_siglist, p->p_xstat);
				FOREACH_THREAD_IN_PROC(p, td0)
					SIGDELSET(td0->td_siglist, p->p_xstat);
			}
			mtx_unlock_spin(&sched_lock);
			goto out;
		} 
		else
			goto runfast;
		/* NOTREACHED */
	} else {
		/* Not in "NORMAL" state. discard the signal. */
		SIGDELSET(*siglist, sig);
		goto out;
	}

	/*
	 * The process is not stopped so we need to apply the signal to all the
	 * running threads.
	 */

runfast:
	mtx_lock_spin(&sched_lock);
	tdsigwakeup(td, sig, action);
	thread_unsuspend(p);
	mtx_unlock_spin(&sched_lock);
out:
	/* If we jump here, sched_lock should not be owned. */
	mtx_assert(&sched_lock, MA_NOTOWNED);
}

/*
 * The force of a signal has been directed against a single
 * thread. We need to see what we can do about knocking it
 * out of any sleep it may be in etc.
 */
static void
tdsigwakeup(struct thread *td, int sig, sig_t action)
{
	struct proc *p = td->td_proc;
	register int prop;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	mtx_assert(&sched_lock, MA_OWNED);
	prop = sigprop(sig);
	/*
	 * Bring the priority of a thread up if we want it to get
	 * killed in this lifetime.
	 */
	if ((action == SIG_DFL) && (prop & SA_KILL)) {
		if (td->td_priority > PUSER) {
			td->td_priority = PUSER;
		}
	}
	if (TD_IS_SLEEPING(td)) {
		/*
		 * If thread is sleeping uninterruptibly
		 * we can't interrupt the sleep... the signal will
		 * be noticed when the process returns through
		 * trap() or syscall().
		 */
		if ((td->td_flags & TDF_SINTR) == 0) {
			return;
		}
		/*
		 * Process is sleeping and traced.  Make it runnable
		 * so it can discover the signal in issignal() and stop
		 * for its parent.
		 */
		if (p->p_flag & P_TRACED) {
			p->p_flag &= ~P_STOPPED_TRACE;
		} else {

			/*
			 * If SIGCONT is default (or ignored) and process is
			 * asleep, we are finished; the process should not
			 * be awakened.
			 */
			if ((prop & SA_CONT) && action == SIG_DFL) {
				SIGDELSET(p->p_siglist, sig);
				/*
				 * It may be on either list in this state.
				 * Remove from both for now.
				 */
				SIGDELSET(td->td_siglist, sig);
				return;
			}

			/*
			 * Raise priority to at least PUSER.
			 */
			if (td->td_priority > PUSER) {
				td->td_priority = PUSER;
			}
		}
		if (td->td_flags & TDF_CVWAITQ) 
			cv_abort(td);
		else
			abortsleep(td);
	}
#ifdef SMP
	  else {
		/*
		 * Other states do nothing with the signal immediatly,
		 * other than kicking ourselves if we are running.
		 * It will either never be noticed, or noticed very soon.
		 */
		if (TD_IS_RUNNING(td) && td != curthread) {
			forward_signal(td);
		}
	  }
#endif
}

void
ptracestop(struct thread *td, int sig)
{
	struct proc *p = td->td_proc;

	WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK,
	    &p->p_mtx.mtx_object, "Stopping for traced signal");

	p->p_xstat = sig;
	PROC_LOCK(p->p_pptr);
	psignal(p->p_pptr, SIGCHLD);
	PROC_UNLOCK(p->p_pptr);
	mtx_lock_spin(&sched_lock);
	stop(p);	/* uses schedlock too eventually */
	thread_suspend_one(td);
	PROC_UNLOCK(p);
	DROP_GIANT();
	p->p_stats->p_ru.ru_nivcsw++;
	mi_switch();
	mtx_unlock_spin(&sched_lock);
	PICKUP_GIANT();
}

/*
 * If the current process has received a signal (should be caught or cause
 * termination, should interrupt current syscall), return the signal number.
 * Stop signals with default action are processed immediately, then cleared;
 * they aren't returned.  This is checked after each entry to the system for
 * a syscall or trap (though this can usually be done without calling issignal
 * by checking the pending signal masks in cursig.) The normal call
 * sequence is
 *
 *	while (sig = cursig(curthread))
 *		postsig(sig);
 */
static int
issignal(td)
	struct thread *td;
{
	struct proc *p;
	struct sigacts *ps;
	sigset_t sigpending;
	int sig, prop;
	struct thread *td0;

	p = td->td_proc;
	ps = p->p_sigacts;
	mtx_assert(&ps->ps_mtx, MA_OWNED);
	PROC_LOCK_ASSERT(p, MA_OWNED);
	for (;;) {
		int traced = (p->p_flag & P_TRACED) || (p->p_stops & S_SIG);

		sigpending = td->td_siglist;
		SIGSETNAND(sigpending, td->td_sigmask);

		if (p->p_flag & P_PPWAIT)
			SIG_STOPSIGMASK(sigpending);
		if (SIGISEMPTY(sigpending))	/* no signal to send */
			return (0);
		sig = sig_ffs(&sigpending);

		_STOPEVENT(p, S_SIG, sig);

		/*
		 * We should see pending but ignored signals
		 * only if P_TRACED was on when they were posted.
		 */
		if (SIGISMEMBER(ps->ps_sigignore, sig) && (traced == 0)) {
			SIGDELSET(td->td_siglist, sig);
			continue;
		}
		if (p->p_flag & P_TRACED && (p->p_flag & P_PPWAIT) == 0) {
			/*
			 * If traced, always stop.
			 */
			mtx_unlock(&ps->ps_mtx);
			ptracestop(td, sig);
			PROC_LOCK(p);
			mtx_lock(&ps->ps_mtx);

			/*
			 * If parent wants us to take the signal,
			 * then it will leave it in p->p_xstat;
			 * otherwise we just look for signals again.
			 */
			SIGDELSET(td->td_siglist, sig);	/* clear old signal */
			sig = p->p_xstat;
			if (sig == 0)
				continue;

			/*
			 * If the traced bit got turned off, go back up
			 * to the top to rescan signals.  This ensures
			 * that p_sig* and p_sigact are consistent.
			 */
			if ((p->p_flag & P_TRACED) == 0)
				continue;

			/*
			 * Put the new signal into td_siglist.  If the
			 * signal is being masked, look for other signals.
			 */
			SIGADDSET(td->td_siglist, sig);
			if (SIGISMEMBER(td->td_sigmask, sig))
				continue;
			signotify(td);
		}

		prop = sigprop(sig);

		/*
		 * Decide whether the signal should be returned.
		 * Return the signal's number, or fall through
		 * to clear it from the pending mask.
		 */
		switch ((intptr_t)p->p_sigacts->ps_sigact[_SIG_IDX(sig)]) {

		case (intptr_t)SIG_DFL:
			/*
			 * Don't take default actions on system processes.
			 */
			if (p->p_pid <= 1) {
#ifdef DIAGNOSTIC
				/*
				 * Are you sure you want to ignore SIGSEGV
				 * in init? XXX
				 */
				printf("Process (pid %lu) got signal %d\n",
					(u_long)p->p_pid, sig);
#endif
				break;		/* == ignore */
			}
			/*
			 * If there is a pending stop signal to process
			 * with default action, stop here,
			 * then clear the signal.  However,
			 * if process is member of an orphaned
			 * process group, ignore tty stop signals.
			 */
			if (prop & SA_STOP) {
				if (p->p_flag & P_TRACED ||
		    		    (p->p_pgrp->pg_jobc == 0 &&
				     prop & SA_TTYSTOP))
					break;	/* == ignore */
				mtx_unlock(&ps->ps_mtx);
				WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK,
				    &p->p_mtx.mtx_object, "Catching SIGSTOP");
				p->p_flag |= P_STOPPED_SIG;
				p->p_xstat = sig;
				mtx_lock_spin(&sched_lock);
				FOREACH_THREAD_IN_PROC(p, td0) {
					if (TD_IS_SLEEPING(td0) &&
					    (td0->td_flags & TDF_SINTR) &&
					    !TD_IS_SUSPENDED(td0)) {
						thread_suspend_one(td0);
					} else if (td != td0) {
						td0->td_flags |= TDF_ASTPENDING;
					}
				}
				thread_stopped(p);
				thread_suspend_one(td);
				PROC_UNLOCK(p);
				DROP_GIANT();
				p->p_stats->p_ru.ru_nivcsw++;
				mi_switch();
				mtx_unlock_spin(&sched_lock);
				PICKUP_GIANT();
				PROC_LOCK(p);
				mtx_lock(&ps->ps_mtx);
				break;
			} else if (prop & SA_IGNORE) {
				/*
				 * Except for SIGCONT, shouldn't get here.
				 * Default action is to ignore; drop it.
				 */
				break;		/* == ignore */
			} else
				return (sig);
			/*NOTREACHED*/

		case (intptr_t)SIG_IGN:
			/*
			 * Masking above should prevent us ever trying
			 * to take action on an ignored signal other
			 * than SIGCONT, unless process is traced.
			 */
			if ((prop & SA_CONT) == 0 &&
			    (p->p_flag & P_TRACED) == 0)
				printf("issignal\n");
			break;		/* == ignore */

		default:
			/*
			 * This signal has an action, let
			 * postsig() process it.
			 */
			return (sig);
		}
		SIGDELSET(td->td_siglist, sig);		/* take the signal! */
	}
	/* NOTREACHED */
}

/*
 * Put the argument process into the stopped state and notify the parent
 * via wakeup.  Signals are handled elsewhere.  The process must not be
 * on the run queue.  Must be called with the proc p locked and the scheduler
 * lock held.
 */
static void
stop(struct proc *p)
{

	PROC_LOCK_ASSERT(p, MA_OWNED);
	p->p_flag |= P_STOPPED_SIG;
	p->p_flag &= ~P_WAITED;
	wakeup(p->p_pptr);
}

/*
 * MPSAFE
 */
void
thread_stopped(struct proc *p)
{
	struct proc *p1 = curthread->td_proc;
	struct sigacts *ps;
	int n;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	mtx_assert(&sched_lock, MA_OWNED);
	n = p->p_suspcount;
	if (p == p1)
		n++;
	if ((p->p_flag & P_STOPPED_SIG) && (n == p->p_numthreads)) {
		mtx_unlock_spin(&sched_lock);
		stop(p);
		PROC_LOCK(p->p_pptr);
		ps = p->p_pptr->p_sigacts;
		mtx_lock(&ps->ps_mtx);
		if ((ps->ps_flag & PS_NOCLDSTOP) == 0) {
			mtx_unlock(&ps->ps_mtx);
			psignal(p->p_pptr, SIGCHLD);
		} else
			mtx_unlock(&ps->ps_mtx);
		PROC_UNLOCK(p->p_pptr);
		mtx_lock_spin(&sched_lock);
	}
}
 
/*
 * Take the action for the specified signal
 * from the current set of pending signals.
 */
void
postsig(sig)
	register int sig;
{
	struct thread *td = curthread;
	register struct proc *p = td->td_proc;
	struct sigacts *ps;
	sig_t action;
	sigset_t returnmask;
	int code;

	KASSERT(sig != 0, ("postsig"));

	PROC_LOCK_ASSERT(p, MA_OWNED);
	ps = p->p_sigacts;
	mtx_assert(&ps->ps_mtx, MA_OWNED);
	SIGDELSET(td->td_siglist, sig);
	action = ps->ps_sigact[_SIG_IDX(sig)];
#ifdef KTRACE
	if (KTRPOINT(td, KTR_PSIG))
		ktrpsig(sig, action, td->td_pflags & TDP_OLDMASK ?
		    &td->td_oldsigmask : &td->td_sigmask, 0);
#endif
	_STOPEVENT(p, S_SIG, sig);

	if (!(td->td_flags & TDF_SA && td->td_mailbox) &&
	    action == SIG_DFL) {
		/*
		 * Default action, where the default is to kill
		 * the process.  (Other cases were ignored above.)
		 */
		mtx_unlock(&ps->ps_mtx);
		sigexit(td, sig);
		/* NOTREACHED */
	} else {
		if (td->td_flags & TDF_SA && td->td_mailbox) {
			if (sig == SIGKILL) {
				mtx_unlock(&ps->ps_mtx);
				sigexit(td, sig);
			}
		}

		/*
		 * If we get here, the signal must be caught.
		 */
		KASSERT(action != SIG_IGN && !SIGISMEMBER(td->td_sigmask, sig),
		    ("postsig action"));
		/*
		 * Set the new mask value and also defer further
		 * occurrences of this signal.
		 *
		 * Special case: user has done a sigsuspend.  Here the
		 * current mask is not of interest, but rather the
		 * mask from before the sigsuspend is what we want
		 * restored after the signal processing is completed.
		 */
		if (td->td_pflags & TDP_OLDMASK) {
			returnmask = td->td_oldsigmask;
			td->td_pflags &= ~TDP_OLDMASK;
		} else
			returnmask = td->td_sigmask;

		SIGSETOR(td->td_sigmask, ps->ps_catchmask[_SIG_IDX(sig)]);
		if (!SIGISMEMBER(ps->ps_signodefer, sig))
			SIGADDSET(td->td_sigmask, sig);

		if (SIGISMEMBER(ps->ps_sigreset, sig)) {
			/*
			 * See kern_sigaction() for origin of this code.
			 */
			SIGDELSET(ps->ps_sigcatch, sig);
			if (sig != SIGCONT &&
			    sigprop(sig) & SA_IGNORE)
				SIGADDSET(ps->ps_sigignore, sig);
			ps->ps_sigact[_SIG_IDX(sig)] = SIG_DFL;
		}
		p->p_stats->p_ru.ru_nsignals++;
		if (p->p_sig != sig) {
			code = 0;
		} else {
			code = p->p_code;
			p->p_code = 0;
			p->p_sig = 0;
		}
		if (td->td_flags & TDF_SA && td->td_mailbox)
			thread_signal_add(curthread, sig);
		else
			(*p->p_sysent->sv_sendsig)(action, sig,
			    &returnmask, code);
	}
}

/*
 * Kill the current process for stated reason.
 */
void
killproc(p, why)
	struct proc *p;
	char *why;
{

	PROC_LOCK_ASSERT(p, MA_OWNED);
	CTR3(KTR_PROC, "killproc: proc %p (pid %d, %s)",
		p, p->p_pid, p->p_comm);
	log(LOG_ERR, "pid %d (%s), uid %d, was killed: %s\n", p->p_pid, p->p_comm,
		p->p_ucred ? p->p_ucred->cr_uid : -1, why);
	psignal(p, SIGKILL);
}

/*
 * Force the current process to exit with the specified signal, dumping core
 * if appropriate.  We bypass the normal tests for masked and caught signals,
 * allowing unrecoverable failures to terminate the process without changing
 * signal state.  Mark the accounting record with the signal termination.
 * If dumping core, save the signal number for the debugger.  Calls exit and
 * does not return.
 *
 * MPSAFE
 */
void
sigexit(td, sig)
	struct thread *td;
	int sig;
{
	struct proc *p = td->td_proc;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	p->p_acflag |= AXSIG;
	if (sigprop(sig) & SA_CORE) {
		p->p_sig = sig;
		/*
		 * Log signals which would cause core dumps
		 * (Log as LOG_INFO to appease those who don't want
		 * these messages.)
		 * XXX : Todo, as well as euid, write out ruid too
		 */
		PROC_UNLOCK(p);
		if (!mtx_owned(&Giant))
			mtx_lock(&Giant);
		if (coredump(td) == 0)
			sig |= WCOREFLAG;
		if (kern_logsigexit)
			log(LOG_INFO,
			    "pid %d (%s), uid %d: exited on signal %d%s\n",
			    p->p_pid, p->p_comm,
			    td->td_ucred ? td->td_ucred->cr_uid : -1,
			    sig &~ WCOREFLAG,
			    sig & WCOREFLAG ? " (core dumped)" : "");
	} else {
		PROC_UNLOCK(p);
		if (!mtx_owned(&Giant))
			mtx_lock(&Giant);
	}
	exit1(td, W_EXITCODE(0, sig));
	/* NOTREACHED */
}

static char corefilename[MAXPATHLEN+1] = {"%N.core"};
SYSCTL_STRING(_kern, OID_AUTO, corefile, CTLFLAG_RW, corefilename,
	      sizeof(corefilename), "process corefile name format string");

/*
 * expand_name(name, uid, pid)
 * Expand the name described in corefilename, using name, uid, and pid.
 * corefilename is a printf-like string, with three format specifiers:
 *	%N	name of process ("name")
 *	%P	process id (pid)
 *	%U	user id (uid)
 * For example, "%N.core" is the default; they can be disabled completely
 * by using "/dev/null", or all core files can be stored in "/cores/%U/%N-%P".
 * This is controlled by the sysctl variable kern.corefile (see above).
 */

static char *
expand_name(name, uid, pid)
	const char *name;
	uid_t uid;
	pid_t pid;
{
	const char *format, *appendstr;
	char *temp;
	char buf[11];		/* Buffer for pid/uid -- max 4B */
	size_t i, l, n;

	format = corefilename;
	temp = malloc(MAXPATHLEN, M_TEMP, M_NOWAIT | M_ZERO);
	if (temp == NULL)
		return (NULL);
	for (i = 0, n = 0; n < MAXPATHLEN && format[i]; i++) {
		switch (format[i]) {
		case '%':	/* Format character */
			i++;
			switch (format[i]) {
			case '%':
				appendstr = "%";
				break;
			case 'N':	/* process name */
				appendstr = name;
				break;
			case 'P':	/* process id */
				sprintf(buf, "%u", pid);
				appendstr = buf;
				break;
			case 'U':	/* user id */
				sprintf(buf, "%u", uid);
				appendstr = buf;
				break;
			default:
				appendstr = "";
			  	log(LOG_ERR,
				    "Unknown format character %c in `%s'\n",
				    format[i], format);
			}
			l = strlen(appendstr);
			if ((n + l) >= MAXPATHLEN)
				goto toolong;
			memcpy(temp + n, appendstr, l);
			n += l;
			break;
		default:
			temp[n++] = format[i];
		}
	}
	if (format[i] != '\0')
		goto toolong;
	return (temp);
toolong:
	log(LOG_ERR, "pid %ld (%s), uid (%lu): corename is too long\n",
	    (long)pid, name, (u_long)uid);
	free(temp, M_TEMP);
	return (NULL);
}

/*
 * Dump a process' core.  The main routine does some
 * policy checking, and creates the name of the coredump;
 * then it passes on a vnode and a size limit to the process-specific
 * coredump routine if there is one; if there _is not_ one, it returns
 * ENOSYS; otherwise it returns the error from the process-specific routine.
 */

static int
coredump(struct thread *td)
{
	struct proc *p = td->td_proc;
	register struct vnode *vp;
	register struct ucred *cred = td->td_ucred;
	struct flock lf;
	struct nameidata nd;
	struct vattr vattr;
	int error, error1, flags;
	struct mount *mp;
	char *name;			/* name of corefile */
	off_t limit;

	PROC_LOCK(p);
	_STOPEVENT(p, S_CORE, 0);

	if (((sugid_coredump == 0) && p->p_flag & P_SUGID) || do_coredump == 0) {
		PROC_UNLOCK(p);
		return (EFAULT);
	}
	
	/*
	 * Note that the bulk of limit checking is done after
	 * the corefile is created.  The exception is if the limit
	 * for corefiles is 0, in which case we don't bother
	 * creating the corefile at all.  This layout means that
	 * a corefile is truncated instead of not being created,
	 * if it is larger than the limit.
	 */
	limit = p->p_rlimit[RLIMIT_CORE].rlim_cur;
	if (limit == 0) {
		PROC_UNLOCK(p);
		return 0;
	}
	PROC_UNLOCK(p);

restart:
	name = expand_name(p->p_comm, td->td_ucred->cr_uid, p->p_pid);
	if (name == NULL)
		return (EINVAL);
	NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_SYSSPACE, name, td); /* XXXKSE */
	flags = O_CREAT | FWRITE | O_NOFOLLOW;
	error = vn_open(&nd, &flags, S_IRUSR | S_IWUSR, -1);
	free(name, M_TEMP);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;

	/* Don't dump to non-regular files or files with links. */
	if (vp->v_type != VREG ||
	    VOP_GETATTR(vp, &vattr, cred, td) || vattr.va_nlink != 1) {
		VOP_UNLOCK(vp, 0, td);
		error = EFAULT;
		goto out2;
	}

	VOP_UNLOCK(vp, 0, td);
	lf.l_whence = SEEK_SET;
	lf.l_start = 0;
	lf.l_len = 0;
	lf.l_type = F_WRLCK;
	error = VOP_ADVLOCK(vp, (caddr_t)p, F_SETLK, &lf, F_FLOCK);
	if (error)
		goto out2;

	if (vn_start_write(vp, &mp, V_NOWAIT) != 0) {
		lf.l_type = F_UNLCK;
		VOP_ADVLOCK(vp, (caddr_t)p, F_UNLCK, &lf, F_FLOCK);
		if ((error = vn_close(vp, FWRITE, cred, td)) != 0)
			return (error);
		if ((error = vn_start_write(NULL, &mp, V_XSLEEP | PCATCH)) != 0)
			return (error);
		goto restart;
	}

	VATTR_NULL(&vattr);
	vattr.va_size = 0;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	VOP_LEASE(vp, td, cred, LEASE_WRITE);
	VOP_SETATTR(vp, &vattr, cred, td);
	VOP_UNLOCK(vp, 0, td);
	PROC_LOCK(p);
	p->p_acflag |= ACORE;
	PROC_UNLOCK(p);

	error = p->p_sysent->sv_coredump ?
	  p->p_sysent->sv_coredump(td, vp, limit) :
	  ENOSYS;

	lf.l_type = F_UNLCK;
	VOP_ADVLOCK(vp, (caddr_t)p, F_UNLCK, &lf, F_FLOCK);
	vn_finished_write(mp);
out2:
	error1 = vn_close(vp, FWRITE, cred, td);
	if (error == 0)
		error = error1;
	return (error);
}

/*
 * Nonexistent system call-- signal process (may want to handle it).
 * Flag error in case process won't see signal immediately (blocked or ignored).
 */
#ifndef _SYS_SYSPROTO_H_
struct nosys_args {
	int	dummy;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
nosys(td, args)
	struct thread *td;
	struct nosys_args *args;
{
	struct proc *p = td->td_proc;

	PROC_LOCK(p);
	psignal(p, SIGSYS);
	PROC_UNLOCK(p);
	return (ENOSYS);
}

/*
 * Send a SIGIO or SIGURG signal to a process or process group using
 * stored credentials rather than those of the current process.
 */
void
pgsigio(sigiop, sig, checkctty)
	struct sigio **sigiop;
	int sig, checkctty;
{
	struct sigio *sigio;

	SIGIO_LOCK();
	sigio = *sigiop;
	if (sigio == NULL) {
		SIGIO_UNLOCK();
		return;
	}
	if (sigio->sio_pgid > 0) {
		PROC_LOCK(sigio->sio_proc);
		if (CANSIGIO(sigio->sio_ucred, sigio->sio_proc->p_ucred))
			psignal(sigio->sio_proc, sig);
		PROC_UNLOCK(sigio->sio_proc);
	} else if (sigio->sio_pgid < 0) {
		struct proc *p;

		PGRP_LOCK(sigio->sio_pgrp);
		LIST_FOREACH(p, &sigio->sio_pgrp->pg_members, p_pglist) {
			PROC_LOCK(p);
			if (CANSIGIO(sigio->sio_ucred, p->p_ucred) &&
			    (checkctty == 0 || (p->p_flag & P_CONTROLT)))
				psignal(p, sig);
			PROC_UNLOCK(p);
		}
		PGRP_UNLOCK(sigio->sio_pgrp);
	}
	SIGIO_UNLOCK();
}

static int
filt_sigattach(struct knote *kn)
{
	struct proc *p = curproc;

	kn->kn_ptr.p_proc = p;
	kn->kn_flags |= EV_CLEAR;		/* automatically set */

	PROC_LOCK(p);
	SLIST_INSERT_HEAD(&p->p_klist, kn, kn_selnext);
	PROC_UNLOCK(p);

	return (0);
}

static void
filt_sigdetach(struct knote *kn)
{
	struct proc *p = kn->kn_ptr.p_proc;

	PROC_LOCK(p);
	SLIST_REMOVE(&p->p_klist, kn, knote, kn_selnext);
	PROC_UNLOCK(p);
}

/*
 * signal knotes are shared with proc knotes, so we apply a mask to 
 * the hint in order to differentiate them from process hints.  This
 * could be avoided by using a signal-specific knote list, but probably
 * isn't worth the trouble.
 */
static int
filt_signal(struct knote *kn, long hint)
{

	if (hint & NOTE_SIGNAL) {
		hint &= ~NOTE_SIGNAL;

		if (kn->kn_id == hint)
			kn->kn_data++;
	}
	return (kn->kn_data != 0);
}

struct sigacts *
sigacts_alloc(void)
{
	struct sigacts *ps;

	ps = malloc(sizeof(struct sigacts), M_SUBPROC, M_WAITOK | M_ZERO);
	ps->ps_refcnt = 1;
	mtx_init(&ps->ps_mtx, "sigacts", NULL, MTX_DEF);
	return (ps);
}

void
sigacts_free(struct sigacts *ps)
{

	mtx_lock(&ps->ps_mtx);
	ps->ps_refcnt--;
	if (ps->ps_refcnt == 0) {
		mtx_destroy(&ps->ps_mtx);
		free(ps, M_SUBPROC);
	} else
		mtx_unlock(&ps->ps_mtx);
}

struct sigacts *
sigacts_hold(struct sigacts *ps)
{
	mtx_lock(&ps->ps_mtx);
	ps->ps_refcnt++;
	mtx_unlock(&ps->ps_mtx);
	return (ps);
}

void
sigacts_copy(struct sigacts *dest, struct sigacts *src)
{

	KASSERT(dest->ps_refcnt == 1, ("sigacts_copy to shared dest"));
	mtx_lock(&src->ps_mtx);
	bcopy(src, dest, offsetof(struct sigacts, ps_refcnt));
	mtx_unlock(&src->ps_mtx);
}

int
sigacts_shared(struct sigacts *ps)
{
	int shared;

	mtx_lock(&ps->ps_mtx);
	shared = ps->ps_refcnt > 1;
	mtx_unlock(&ps->ps_mtx);
	return (shared);
}
