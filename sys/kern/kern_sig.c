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
 * $FreeBSD$
 */

#include "opt_compat.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/event.h>
#include <sys/proc.h>
#include <sys/pioctl.h>
#include <sys/acct.h>
#include <sys/fcntl.h>
#include <sys/condvar.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/wait.h>
#include <sys/ktr.h>
#include <sys/ktrace.h>
#include <sys/resourcevar.h>
#include <sys/smp.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/syslog.h>
#include <sys/sysent.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/unistd.h>

#include <machine/cpu.h>

#define	ONSIG	32		/* NSIG for osig* syscalls.  XXX. */

static int coredump	__P((struct thread *));
static int do_sigaction	__P((struct proc *p, int sig, struct sigaction *act,
			     struct sigaction *oact, int old));
static int do_sigprocmask __P((struct proc *p, int how, sigset_t *set,
			       sigset_t *oset, int old));
static char *expand_name __P((const char *, uid_t, pid_t));
static int killpg1	__P((struct proc *cp, int sig, int pgid, int all));
static int sig_ffs	__P((sigset_t *set));
static int sigprop	__P((int sig));
static void stop	__P((struct proc *));

static int	filt_sigattach(struct knote *kn);
static void	filt_sigdetach(struct knote *kn);
static int	filt_signal(struct knote *kn, long hint);

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

static int sigproptbl[NSIG] = {
        SA_KILL,                /* SIGHUP */
        SA_KILL,                /* SIGINT */
        SA_KILL|SA_CORE,        /* SIGQUIT */
        SA_KILL|SA_CORE,        /* SIGILL */
        SA_KILL|SA_CORE,        /* SIGTRAP */
        SA_KILL|SA_CORE,        /* SIGABRT */
        SA_KILL|SA_CORE,        /* SIGEMT */
        SA_KILL|SA_CORE,        /* SIGFPE */
        SA_KILL,                /* SIGKILL */
        SA_KILL|SA_CORE,        /* SIGBUS */
        SA_KILL|SA_CORE,        /* SIGSEGV */
        SA_KILL|SA_CORE,        /* SIGSYS */
        SA_KILL,                /* SIGPIPE */
        SA_KILL,                /* SIGALRM */
        SA_KILL,                /* SIGTERM */
        SA_IGNORE,              /* SIGURG */
        SA_STOP,                /* SIGSTOP */
        SA_STOP|SA_TTYSTOP,     /* SIGTSTP */
        SA_IGNORE|SA_CONT,      /* SIGCONT */
        SA_IGNORE,              /* SIGCHLD */
        SA_STOP|SA_TTYSTOP,     /* SIGTTIN */
        SA_STOP|SA_TTYSTOP,     /* SIGTTOU */
        SA_IGNORE,              /* SIGIO */
        SA_KILL,                /* SIGXCPU */
        SA_KILL,                /* SIGXFSZ */
        SA_KILL,                /* SIGVTALRM */
        SA_KILL,                /* SIGPROF */
        SA_IGNORE,              /* SIGWINCH  */
        SA_IGNORE,              /* SIGINFO */
        SA_KILL,                /* SIGUSR1 */
        SA_KILL,                /* SIGUSR2 */
};

/*
 * Determine signal that should be delivered to process p, the current
 * process, 0 if none.  If there is a pending stop signal with default
 * action, the process stops in issignal().
 *
 * MP SAFE.
 */
int
CURSIG(struct proc *p)
{
	sigset_t tmpset;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (SIGISEMPTY(p->p_siglist))
		return (0);
	tmpset = p->p_siglist;
	SIGSETNAND(tmpset, p->p_sigmask);
	if (SIGISEMPTY(tmpset) && (p->p_flag & P_TRACED) == 0)
		return (0);
	return (issignal(p));
}

static __inline int
sigprop(int sig)
{

	if (sig > 0 && sig < NSIG)
		return (sigproptbl[_SIG_IDX(sig)]);
	return (0);
}

static __inline int
sig_ffs(sigset_t *set)
{
	int i;

	for (i = 0; i < _SIG_WORDS; i++)
		if (set->__bits[i])
			return (ffs(set->__bits[i]) + (i * 32));
	return (0);
}

/*
 * do_sigaction
 * sigaction
 * osigaction
 */
static int
do_sigaction(p, sig, act, oact, old)
	struct proc *p;
	register int sig;
	struct sigaction *act, *oact;
	int old;
{
	register struct sigacts *ps;

	if (!_SIG_VALID(sig))
		return (EINVAL);

	PROC_LOCK(p);
	ps = p->p_sigacts;
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
		if (sig == SIGCHLD && p->p_procsig->ps_flag & PS_NOCLDSTOP)
			oact->sa_flags |= SA_NOCLDSTOP;
		if (sig == SIGCHLD && p->p_procsig->ps_flag & PS_NOCLDWAIT)
			oact->sa_flags |= SA_NOCLDWAIT;
	}
	if (act) {
		if ((sig == SIGKILL || sig == SIGSTOP) &&
		    act->sa_handler != SIG_DFL) {
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
			SIGDELSET(ps->ps_usertramp, seg);
#endif
		if (sig == SIGCHLD) {
			if (act->sa_flags & SA_NOCLDSTOP)
				p->p_procsig->ps_flag |= PS_NOCLDSTOP;
			else
				p->p_procsig->ps_flag &= ~PS_NOCLDSTOP;
			if ((act->sa_flags & SA_NOCLDWAIT) ||
			    ps->ps_sigact[_SIG_IDX(SIGCHLD)] == SIG_IGN) {
				/*
				 * Paranoia: since SA_NOCLDWAIT is implemented
				 * by reparenting the dying child to PID 1 (and
				 * trust it to reap the zombie), PID 1 itself
				 * is forbidden to set SA_NOCLDWAIT.
				 */
				if (p->p_pid == 1)
					p->p_procsig->ps_flag &= ~PS_NOCLDWAIT;
				else
					p->p_procsig->ps_flag |= PS_NOCLDWAIT;
			} else
				p->p_procsig->ps_flag &= ~PS_NOCLDWAIT;
		}
		/*
		 * Set bit in p_sigignore for signals that are set to SIG_IGN,
		 * and for signals set to SIG_DFL where the default is to
		 * ignore. However, don't put SIGCONT in p_sigignore, as we
		 * have to restart the process.
		 */
		if (ps->ps_sigact[_SIG_IDX(sig)] == SIG_IGN ||
		    (sigprop(sig) & SA_IGNORE &&
		     ps->ps_sigact[_SIG_IDX(sig)] == SIG_DFL)) {
			/* never to be seen again */
			SIGDELSET(p->p_siglist, sig);
			if (sig != SIGCONT)
				/* easier in psignal */
				SIGADDSET(p->p_sigignore, sig);
			SIGDELSET(p->p_sigcatch, sig);
		} else {
			SIGDELSET(p->p_sigignore, sig);
			if (ps->ps_sigact[_SIG_IDX(sig)] == SIG_DFL)
				SIGDELSET(p->p_sigcatch, sig);
			else
				SIGADDSET(p->p_sigcatch, sig);
		}
#ifdef COMPAT_43
		if (ps->ps_sigact[_SIG_IDX(sig)] == SIG_IGN ||
		    ps->ps_sigact[_SIG_IDX(sig)] == SIG_DFL || !old)
			SIGDELSET(ps->ps_osigset, sig);
		else
			SIGADDSET(ps->ps_osigset, sig);
#endif
	}
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
/* ARGSUSED */
int
sigaction(td, uap)
	struct thread *td;
	register struct sigaction_args *uap;
{
	struct proc *p = td->td_proc;
	struct sigaction act, oact;
	register struct sigaction *actp, *oactp;
	int error;

	mtx_lock(&Giant);

	actp = (uap->act != NULL) ? &act : NULL;
	oactp = (uap->oact != NULL) ? &oact : NULL;
	if (actp) {
		error = copyin(uap->act, actp, sizeof(act));
		if (error)
			goto done2;
	}
	error = do_sigaction(p, uap->sig, actp, oactp, 0);
	if (oactp && !error) {
		error = copyout(oactp, uap->oact, sizeof(oact));
	}
done2:
	mtx_unlock(&Giant);
	return (error);
}

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
/* ARGSUSED */
int
osigaction(td, uap)
	struct thread *td;
	register struct osigaction_args *uap;
{
	struct proc *p = td->td_proc;
	struct osigaction sa;
	struct sigaction nsa, osa;
	register struct sigaction *nsap, *osap;
	int error;

	if (uap->signum <= 0 || uap->signum >= ONSIG)
		return (EINVAL);

	nsap = (uap->nsa != NULL) ? &nsa : NULL;
	osap = (uap->osa != NULL) ? &osa : NULL;

	mtx_lock(&Giant);

	if (nsap) {
		error = copyin(uap->nsa, &sa, sizeof(sa));
		if (error)
			goto done2;
		nsap->sa_handler = sa.sa_handler;
		nsap->sa_flags = sa.sa_flags;
		OSIG2SIG(sa.sa_mask, nsap->sa_mask);
	}
	error = do_sigaction(p, uap->signum, nsap, osap, 1);
	if (osap && !error) {
		sa.sa_handler = osap->sa_handler;
		sa.sa_flags = osap->sa_flags;
		SIG2OSIG(osap->sa_mask, sa.sa_mask);
		error = copyout(&sa, uap->osa, sizeof(sa));
	}
done2:
	mtx_unlock(&Giant);
	return (error);
}
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

	PROC_LOCK(p);
	for (i = 1; i <= NSIG; i++)
		if (sigprop(i) & SA_IGNORE && i != SIGCONT)
			SIGADDSET(p->p_sigignore, i);
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
	 * through p_sigmask (unless they were caught,
	 * and are now ignored by default).
	 */
	PROC_LOCK(p);
	ps = p->p_sigacts;
	while (SIGNOTEMPTY(p->p_sigcatch)) {
		sig = sig_ffs(&p->p_sigcatch);
		SIGDELSET(p->p_sigcatch, sig);
		if (sigprop(sig) & SA_IGNORE) {
			if (sig != SIGCONT)
				SIGADDSET(p->p_sigignore, sig);
			SIGDELSET(p->p_siglist, sig);
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
	p->p_procsig->ps_flag &= ~PS_NOCLDWAIT;
	if (ps->ps_sigact[_SIG_IDX(SIGCHLD)] == SIG_IGN)
		ps->ps_sigact[_SIG_IDX(SIGCHLD)] = SIG_DFL;
	PROC_UNLOCK(p);
}

/*
 * do_sigprocmask()
 *
 *	Manipulate signal mask.
 */
static int
do_sigprocmask(p, how, set, oset, old)
	struct proc *p;
	int how;
	sigset_t *set, *oset;
	int old;
{
	int error;

	PROC_LOCK(p);
	if (oset != NULL)
		*oset = p->p_sigmask;

	error = 0;
	if (set != NULL) {
		switch (how) {
		case SIG_BLOCK:
			SIG_CANTMASK(*set);
			SIGSETOR(p->p_sigmask, *set);
			break;
		case SIG_UNBLOCK:
			SIGSETNAND(p->p_sigmask, *set);
			break;
		case SIG_SETMASK:
			SIG_CANTMASK(*set);
			if (old)
				SIGSETLO(p->p_sigmask, *set);
			else
				p->p_sigmask = *set;
			break;
		default:
			error = EINVAL;
			break;
		}
	}
	PROC_UNLOCK(p);
	return (error);
}

/*
 * sigprocmask() - MP SAFE (XXXKSE not under KSE it isn't)
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
	struct proc *p = td->td_proc;
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
	error = do_sigprocmask(p, uap->how, setp, osetp, 0);
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
	struct proc *p = td->td_proc;
	sigset_t set, oset;
	int error;

	OSIG2SIG(uap->mask, set);
	error = do_sigprocmask(p, uap->how, &set, &oset, 1);
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
/* ARGSUSED */
int
sigpending(td, uap)
	struct thread *td;
	struct sigpending_args *uap;
{
	struct proc *p = td->td_proc;
	sigset_t siglist;
	int error;

	mtx_lock(&Giant);
	PROC_LOCK(p);
	siglist = p->p_siglist;
	PROC_UNLOCK(p);
	mtx_unlock(&Giant);
	error = copyout(&siglist, uap->set, sizeof(sigset_t));
	return(error);
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
/* ARGSUSED */
int
osigpending(td, uap)
	struct thread *td;
	struct osigpending_args *uap;
{
	struct proc *p = td->td_proc;

	mtx_lock(&Giant);
	PROC_LOCK(p);
	SIG2OSIG(p->p_siglist, td->td_retval[0]);
	PROC_UNLOCK(p);
	mtx_unlock(&Giant);
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
	struct proc *p = td->td_proc;
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
	mtx_lock(&Giant);
	error = do_sigaction(p, uap->signum, nsap, osap, 1);
	mtx_unlock(&Giant);
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
	mtx_lock(&Giant);
	PROC_LOCK(p);
	SIG2OSIG(p->p_sigmask, td->td_retval[0]);
	SIGSETOR(p->p_sigmask, set);
	PROC_UNLOCK(p);
	mtx_unlock(&Giant);
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
	mtx_lock(&Giant);
	PROC_LOCK(p);
	SIG2OSIG(p->p_sigmask, td->td_retval[0]);
	SIGSETLO(p->p_sigmask, set);
	PROC_UNLOCK(p);
	mtx_unlock(&Giant);
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
	struct proc *p = td->td_proc;
	sigset_t mask;
	register struct sigacts *ps;
	int error;

	error = copyin(uap->sigmask, &mask, sizeof(mask));
	if (error)
		return (error);

	/*
	 * When returning from sigsuspend, we want
	 * the old mask to be restored after the
	 * signal handler has finished.  Thus, we
	 * save it here and mark the sigacts structure
	 * to indicate this.
	 */
	mtx_lock(&Giant);
	PROC_LOCK(p);
	ps = p->p_sigacts;
	p->p_oldsigmask = p->p_sigmask;
	p->p_flag |= P_OLDMASK;

	SIG_CANTMASK(mask);
	p->p_sigmask = mask;
	while (msleep((caddr_t) ps, &p->p_mtx, PPAUSE|PCATCH, "pause", 0) == 0)
		/* void */;
	PROC_UNLOCK(p);
	mtx_unlock(&Giant);
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
	register struct sigacts *ps;

	mtx_lock(&Giant);
	PROC_LOCK(p);
	ps = p->p_sigacts;
	p->p_oldsigmask = p->p_sigmask;
	p->p_flag |= P_OLDMASK;
	OSIG2SIG(uap->mask, mask);
	SIG_CANTMASK(mask);
	SIGSETLO(p->p_sigmask, mask);
	while (msleep((caddr_t) ps, &p->p_mtx, PPAUSE|PCATCH, "opause", 0) == 0)
		/* void */;
	PROC_UNLOCK(p);
	mtx_unlock(&Giant);
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
	struct sigstack ss;
	int error = 0;

	mtx_lock(&Giant);

	if (uap->oss != NULL) {
		PROC_LOCK(p);
		ss.ss_sp = p->p_sigstk.ss_sp;
		ss.ss_onstack = sigonstack(cpu_getstack(td));
		PROC_UNLOCK(p);
		error = copyout(&ss, uap->oss, sizeof(struct sigstack));
		if (error)
			goto done2;
	}

	if (uap->nss != NULL) {
		if ((error = copyin(uap->nss, &ss, sizeof(ss))) != 0)
			goto done2;
		PROC_LOCK(p);
		p->p_sigstk.ss_sp = ss.ss_sp;
		p->p_sigstk.ss_size = 0;
		p->p_sigstk.ss_flags |= ss.ss_onstack & SS_ONSTACK;
		p->p_flag |= P_ALTSTACK;
		PROC_UNLOCK(p);
	}
done2:
	mtx_unlock(&Giant);
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
	struct proc *p = td->td_proc;
	stack_t ss;
	int oonstack;
	int error = 0;

	mtx_lock(&Giant);

	oonstack = sigonstack(cpu_getstack(td));

	if (uap->oss != NULL) {
		PROC_LOCK(p);
		ss = p->p_sigstk;
		ss.ss_flags = (p->p_flag & P_ALTSTACK)
		    ? ((oonstack) ? SS_ONSTACK : 0) : SS_DISABLE;
		PROC_UNLOCK(p);
		if ((error = copyout(&ss, uap->oss, sizeof(stack_t))) != 0)
			goto done2;
	}

	if (uap->ss != NULL) {
		if (oonstack) {
			error = EPERM;
			goto done2;
		}
		if ((error = copyin(uap->ss, &ss, sizeof(ss))) != 0)
			goto done2;
		if ((ss.ss_flags & ~SS_DISABLE) != 0) {
			error = EINVAL;
			goto done2;
		}
		if (!(ss.ss_flags & SS_DISABLE)) {
			if (ss.ss_size < p->p_sysent->sv_minsigstksz) {
				error = ENOMEM;
				goto done2;
			}
			PROC_LOCK(p);
			p->p_sigstk = ss;
			p->p_flag |= P_ALTSTACK;
			PROC_UNLOCK(p);
		} else {
			PROC_LOCK(p);
			p->p_flag &= ~P_ALTSTACK;
			PROC_UNLOCK(p);
		}
	}
done2:
	mtx_unlock(&Giant);
	return (error);
}

/*
 * Common code for kill process group/broadcast kill.
 * cp is calling process.
 */
int
killpg1(cp, sig, pgid, all)
	register struct proc *cp;
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
			if (p->p_pid <= 1 || p->p_flag & P_SYSTEM || p == cp) {
				PROC_UNLOCK(p);
				continue;
			}
			if (p_cansignal(cp, p, sig) == 0) {
				nfound++;
				if (sig)
					psignal(p, sig);
			}
			PROC_UNLOCK(p);
		}
		sx_sunlock(&allproc_lock);
	} else {
		if (pgid == 0)
			/*
			 * zero pgid means send to my process group.
			 */
			pgrp = cp->p_pgrp;
		else {
			pgrp = pgfind(pgid);
			if (pgrp == NULL)
				return (ESRCH);
		}
		LIST_FOREACH(p, &pgrp->pg_members, p_pglist) {
			PROC_LOCK(p);	      
			if (p->p_pid <= 1 || p->p_flag & P_SYSTEM) {
				PROC_UNLOCK(p);
				continue;
			}
			mtx_lock_spin(&sched_lock);
			if (p->p_stat == SZOMB) {
				mtx_unlock_spin(&sched_lock);
				PROC_UNLOCK(p);
				continue;
			}
			mtx_unlock_spin(&sched_lock);
			if (p_cansignal(cp, p, sig) == 0) {
				nfound++;
				if (sig)
					psignal(p, sig);
			}
			PROC_UNLOCK(p);
		}
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
	register struct proc *cp = td->td_proc;
	register struct proc *p;
	int error = 0;

	if ((u_int)uap->signum > _SIG_MAXSIG)
		return (EINVAL);

	mtx_lock(&Giant);
	if (uap->pid > 0) {
		/* kill single process */
		if ((p = pfind(uap->pid)) == NULL) {
			error = ESRCH;
		} else if (p_cansignal(cp, p, uap->signum)) {
			PROC_UNLOCK(p);
			error = EPERM;
		} else {
			if (uap->signum)
				psignal(p, uap->signum);
			PROC_UNLOCK(p);
			error = 0;
		}
	} else {
		switch (uap->pid) {
		case -1:		/* broadcast signal */
			error = killpg1(cp, uap->signum, 0, 1);
			break;
		case 0:			/* signal own process group */
			error = killpg1(cp, uap->signum, 0, 0);
			break;
		default:		/* negative explicit process group */
			error = killpg1(cp, uap->signum, -uap->pid, 0);
			break;
		}
	}
	mtx_unlock(&Giant);
	return(error);
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
	int error;

	if ((u_int)uap->signum > _SIG_MAXSIG)
		return (EINVAL);
	mtx_lock(&Giant);
	error = killpg1(td->td_proc, uap->signum, uap->pgid, 0);
	mtx_unlock(&Giant);
	return (error);
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

	if (pgid && (pgrp = pgfind(pgid)))
		pgsignal(pgrp, sig, 0);
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
		LIST_FOREACH(p, &pgrp->pg_members, p_pglist) {
			PROC_LOCK(p);
			if (checkctty == 0 || p->p_flag & P_CONTROLT)
				psignal(p, sig);
			PROC_UNLOCK(p);
		}
	}
}

/*
 * Send a signal caused by a trap to the current process.
 * If it will be caught immediately, deliver it with correct code.
 * Otherwise, post it normally.
 *
 * MPSAFE
 */
void
trapsignal(p, sig, code)
	struct proc *p;
	register int sig;
	u_long code;
{
	register struct sigacts *ps = p->p_sigacts;

	mtx_lock(&Giant);
	PROC_LOCK(p);
	if ((p->p_flag & P_TRACED) == 0 && SIGISMEMBER(p->p_sigcatch, sig) &&
	    !SIGISMEMBER(p->p_sigmask, sig)) {
		p->p_stats->p_ru.ru_nsignals++;
#ifdef KTRACE
		if (KTRPOINT(p, KTR_PSIG))
			ktrpsig(p->p_tracep, sig, ps->ps_sigact[_SIG_IDX(sig)],
				&p->p_sigmask, code);
#endif
		(*p->p_sysent->sv_sendsig)(ps->ps_sigact[_SIG_IDX(sig)], sig,
						&p->p_sigmask, code);
		SIGSETOR(p->p_sigmask, ps->ps_catchmask[_SIG_IDX(sig)]);
		if (!SIGISMEMBER(ps->ps_signodefer, sig))
			SIGADDSET(p->p_sigmask, sig);
		if (SIGISMEMBER(ps->ps_sigreset, sig)) {
			/*
			 * See do_sigaction() for origin of this code.
			 */
			SIGDELSET(p->p_sigcatch, sig);
			if (sig != SIGCONT &&
			    sigprop(sig) & SA_IGNORE)
				SIGADDSET(p->p_sigignore, sig);
			ps->ps_sigact[_SIG_IDX(sig)] = SIG_DFL;
		}
	} else {
		p->p_code = code;	/* XXX for core dump/debugger */
		p->p_sig = sig;		/* XXX to verify code */
		psignal(p, sig);
	}
	PROC_UNLOCK(p);
	mtx_unlock(&Giant);
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
 */
void
psignal(p, sig)
	register struct proc *p;
	register int sig;
{
	register int prop;
	register sig_t action;
	struct thread *td;
	struct ksegrp *kg;

	KASSERT(_SIG_VALID(sig),
	    ("psignal(): invalid signal %d\n", sig));

	PROC_LOCK_ASSERT(p, MA_OWNED);
	KNOTE(&p->p_klist, NOTE_SIGNAL | sig);

	prop = sigprop(sig);

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
		 * (Note: we don't set SIGCONT in p_sigignore,
		 * and if it is set to SIG_IGN,
		 * action will be SIG_DFL here.)
		 */
		if (SIGISMEMBER(p->p_sigignore, sig) || (p->p_flag & P_WEXIT))
			return;
		if (SIGISMEMBER(p->p_sigmask, sig))
			action = SIG_HOLD;
		else if (SIGISMEMBER(p->p_sigcatch, sig))
			action = SIG_CATCH;
		else
			action = SIG_DFL;
	}

	/*
	 * bring the priority of a process up if we want it to get 
	 * killed in this lifetime.
	 * XXXKSE think if a better way to do this.
	 *
	 * What we need to do is see if there is a thread that will
	 * be able to accept the signal. e.g.
	 * FOREACH_THREAD_IN_PROC() {
	 *	if runnable, we're done
	 *	else pick one at random.
	 * }
	 */
	/* XXXKSE
	 * For now there is one thread per proc.
	 * Effectively select one sucker thread..
	 */
	td = FIRST_THREAD_IN_PROC(p);
	mtx_lock_spin(&sched_lock);
	if ((p->p_ksegrp.kg_nice > NZERO) && (action == SIG_DFL) &&
	    (prop & SA_KILL) && ((p->p_flag & P_TRACED) == 0))
		p->p_ksegrp.kg_nice = NZERO; /* XXXKSE */
	mtx_unlock_spin(&sched_lock);

	if (prop & SA_CONT)
		SIG_STOPSIGMASK(p->p_siglist);

	if (prop & SA_STOP) {
		/*
		 * If sending a tty stop signal to a member of an orphaned
		 * process group, discard the signal here if the action
		 * is default; don't stop the process below if sleeping,
		 * and don't clear any pending SIGCONT.
		 */
		if (prop & SA_TTYSTOP && p->p_pgrp->pg_jobc == 0 &&
		    action == SIG_DFL)
		        return;
		SIG_CONTSIGMASK(p->p_siglist);
	}
	SIGADDSET(p->p_siglist, sig);

	/*
	 * Defer further processing for signals which are held,
	 * except that stopped processes must be continued by SIGCONT.
	 */
	mtx_lock_spin(&sched_lock);
	if (action == SIG_HOLD && (!(prop & SA_CONT) || p->p_stat != SSTOP)) {
		mtx_unlock_spin(&sched_lock);
		return;
	}

	switch (p->p_stat) {

	case SSLEEP:
		/*
		 * If process is sleeping uninterruptibly
		 * we can't interrupt the sleep... the signal will
		 * be noticed when the process returns through
		 * trap() or syscall().
		 */
		if ((td->td_flags & TDF_SINTR) == 0) {
			mtx_unlock_spin(&sched_lock);
			goto out;
		}
		/*
		 * Process is sleeping and traced... make it runnable
		 * so it can discover the signal in issignal() and stop
		 * for the parent.
		 */
		if (p->p_flag & P_TRACED)
			goto run;
		mtx_unlock_spin(&sched_lock);
		/*
		 * If SIGCONT is default (or ignored) and process is
		 * asleep, we are finished; the process should not
		 * be awakened.
		 */
		if ((prop & SA_CONT) && action == SIG_DFL) {
			SIGDELSET(p->p_siglist, sig);
			goto out;
		}
		/*
		 * When a sleeping process receives a stop
		 * signal, process immediately if possible.
		 * All other (caught or default) signals
		 * cause the process to run.
		 */
		if (prop & SA_STOP) {
			if (action != SIG_DFL)
				goto runfast;
			/*
			 * If a child holding parent blocked,
			 * stopping could cause deadlock.
			 */
			if (p->p_flag & P_PPWAIT)
				goto out;
			SIGDELSET(p->p_siglist, sig);
			p->p_xstat = sig;
			if ((p->p_pptr->p_procsig->ps_flag & PS_NOCLDSTOP) == 0) {
				PROC_LOCK(p->p_pptr);
				psignal(p->p_pptr, SIGCHLD);
				PROC_UNLOCK(p->p_pptr);
			}
			mtx_lock_spin(&sched_lock);
			stop(p);
			mtx_unlock_spin(&sched_lock);
			goto out;
		} else
			goto runfast;
		/* NOTREACHED */

	case SSTOP:
		mtx_unlock_spin(&sched_lock);
		/*
		 * If traced process is already stopped,
		 * then no further action is necessary.
		 */
		if (p->p_flag & P_TRACED)
			goto out;

		/*
		 * Kill signal always sets processes running.
		 */
		if (sig == SIGKILL)
			goto runfast;

		if (prop & SA_CONT) {
			/*
			 * If SIGCONT is default (or ignored), we continue the
			 * process but don't leave the signal in p_siglist, as
			 * it has no further action.  If SIGCONT is held, we
			 * continue the process and leave the signal in
			 * p_siglist.  If the process catches SIGCONT, let it
			 * handle the signal itself.  If it isn't waiting on
			 * an event, then it goes back to run state.
			 * Otherwise, process goes back to sleep state.
			 */
			if (action == SIG_DFL)
				SIGDELSET(p->p_siglist, sig);
			if (action == SIG_CATCH)
				goto runfast;
			mtx_lock_spin(&sched_lock);
			/*
			 * XXXKSE
			 * do this for each thread.
			 */
			if (p->p_flag & P_KSES) {
				mtx_assert(&sched_lock,
				    MA_OWNED | MA_NOTRECURSED);
				FOREACH_THREAD_IN_PROC(p, td) {
					if (td->td_wchan == NULL) {
						setrunnable(td); /* XXXKSE */
					} else {
						/* mark it as sleeping */
					}
				}
				mtx_unlock_spin(&sched_lock);
			} else {
				if (td->td_wchan == NULL)
					goto run;
				p->p_stat = SSLEEP;
				mtx_unlock_spin(&sched_lock);
			}
			goto out;
		}

		if (prop & SA_STOP) {
			/*
			 * Already stopped, don't need to stop again.
			 * (If we did the shell could get confused.)
			 */
			SIGDELSET(p->p_siglist, sig);
			goto out;
		}

		/*
		 * If process is sleeping interruptibly, then simulate a
		 * wakeup so that when it is continued, it will be made
		 * runnable and can look at the signal.  But don't make
		 * the process runnable, leave it stopped.
		 * XXXKSE should we wake ALL blocked threads?
		 */
		mtx_lock_spin(&sched_lock);
		if (p->p_flag & P_KSES) {
			FOREACH_THREAD_IN_PROC(p, td) {
				if (td->td_wchan && (td->td_flags & TDF_SINTR)){
					if (td->td_flags & TDF_CVWAITQ)
						cv_waitq_remove(td);
					else
						unsleep(td); /* XXXKSE */
				}
			}
		} else {
			if (td->td_wchan && td->td_flags & TDF_SINTR) {
				if (td->td_flags & TDF_CVWAITQ)
					cv_waitq_remove(td);
				else
					unsleep(td); /* XXXKSE */
			}
		}
		mtx_unlock_spin(&sched_lock);
		goto out;

	default:
		/*
		 * SRUN, SIDL, SZOMB do nothing with the signal,
		 * other than kicking ourselves if we are running.
		 * It will either never be noticed, or noticed very soon.
		 */
		if (p->p_stat == SRUN) {
#ifdef SMP
			struct kse *ke;
			struct thread *td = curthread;
			signotify(&p->p_kse);  /* XXXKSE */  
/* we should only deliver to one thread.. but which one? */
			FOREACH_KSEGRP_IN_PROC(p, kg) {
				FOREACH_KSE_IN_GROUP(kg, ke) {
					if (ke->ke_thread == td) {
						continue;
					}
					forward_signal(ke->ke_thread);
				}
			}
#else
			signotify(&p->p_kse);  /* XXXKSE */  
#endif
		}
		mtx_unlock_spin(&sched_lock);
		goto out;
	}
	/*NOTREACHED*/

runfast:
	/*
	 * Raise priority to at least PUSER.
	 * XXXKSE Should we make them all run fast?
	 * Maybe just one would be enough?
	 */
	mtx_lock_spin(&sched_lock);
	FOREACH_KSEGRP_IN_PROC(p, kg) {
		if (kg->kg_pri.pri_level > PUSER) {
			kg->kg_pri.pri_level = PUSER;
		}
	}
run:
	/* If we jump here, sched_lock has to be owned. */
	mtx_assert(&sched_lock, MA_OWNED | MA_NOTRECURSED);
	setrunnable(td); /* XXXKSE */
	mtx_unlock_spin(&sched_lock);
out:
	/* If we jump here, sched_lock should not be owned. */
	mtx_assert(&sched_lock, MA_NOTOWNED);
}

/*
 * If the current process has received a signal (should be caught or cause
 * termination, should interrupt current syscall), return the signal number.
 * Stop signals with default action are processed immediately, then cleared;
 * they aren't returned.  This is checked after each entry to the system for
 * a syscall or trap (though this can usually be done without calling issignal
 * by checking the pending signal masks in the CURSIG macro.) The normal call
 * sequence is
 *
 *	while (sig = CURSIG(curproc))
 *		postsig(sig);
 */
int
issignal(p)
	register struct proc *p;
{
	sigset_t mask;
	register int sig, prop;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	for (;;) {
		int traced = (p->p_flag & P_TRACED) || (p->p_stops & S_SIG);

		mask = p->p_siglist;
		SIGSETNAND(mask, p->p_sigmask);
		if (p->p_flag & P_PPWAIT)
			SIG_STOPSIGMASK(mask);
		if (!SIGNOTEMPTY(mask))		/* no signal to send */
			return (0);
		sig = sig_ffs(&mask);
		prop = sigprop(sig);

		_STOPEVENT(p, S_SIG, sig);

		/*
		 * We should see pending but ignored signals
		 * only if P_TRACED was on when they were posted.
		 */
		if (SIGISMEMBER(p->p_sigignore, sig) && (traced == 0)) {
			SIGDELSET(p->p_siglist, sig);
			continue;
		}
		if (p->p_flag & P_TRACED && (p->p_flag & P_PPWAIT) == 0) {
			/*
			 * If traced, always stop, and stay
			 * stopped until released by the parent.
			 */
			p->p_xstat = sig;
			PROC_LOCK(p->p_pptr);
			psignal(p->p_pptr, SIGCHLD);
			PROC_UNLOCK(p->p_pptr);
			do {
				mtx_lock_spin(&sched_lock);
				stop(p);
				PROC_UNLOCK(p);
				DROP_GIANT();
				p->p_stats->p_ru.ru_nivcsw++;
				mi_switch();
				mtx_unlock_spin(&sched_lock);
				PICKUP_GIANT();
				PROC_LOCK(p);
			} while (!trace_req(p)
				 && p->p_flag & P_TRACED);

			/*
			 * If the traced bit got turned off, go back up
			 * to the top to rescan signals.  This ensures
			 * that p_sig* and ps_sigact are consistent.
			 */
			if ((p->p_flag & P_TRACED) == 0)
				continue;

			/*
			 * If parent wants us to take the signal,
			 * then it will leave it in p->p_xstat;
			 * otherwise we just look for signals again.
			 */
			SIGDELSET(p->p_siglist, sig);	/* clear old signal */
			sig = p->p_xstat;
			if (sig == 0)
				continue;

			/*
			 * Put the new signal into p_siglist.  If the
			 * signal is being masked, look for other signals.
			 */
			SIGADDSET(p->p_siglist, sig);
			if (SIGISMEMBER(p->p_sigmask, sig))
				continue;
		}

		/*
		 * Decide whether the signal should be returned.
		 * Return the signal's number, or fall through
		 * to clear it from the pending mask.
		 */
		switch ((int)(intptr_t)p->p_sigacts->ps_sigact[_SIG_IDX(sig)]) {

		case (int)SIG_DFL:
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
				p->p_xstat = sig;
				if ((p->p_pptr->p_procsig->ps_flag & PS_NOCLDSTOP) == 0) {
					PROC_LOCK(p->p_pptr);
					psignal(p->p_pptr, SIGCHLD);
					PROC_UNLOCK(p->p_pptr);
				}
				mtx_lock_spin(&sched_lock);
				stop(p);
				PROC_UNLOCK(p);
				DROP_GIANT();
				p->p_stats->p_ru.ru_nivcsw++;
				mi_switch();
				mtx_unlock_spin(&sched_lock);
				PICKUP_GIANT();
				PROC_LOCK(p);
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

		case (int)SIG_IGN:
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
		SIGDELSET(p->p_siglist, sig);		/* take the signal! */
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
stop(p)
	register struct proc *p;
{

	PROC_LOCK_ASSERT(p, MA_OWNED);
	mtx_assert(&sched_lock, MA_OWNED);
	p->p_stat = SSTOP;
	p->p_flag &= ~P_WAITED;
	wakeup((caddr_t)p->p_pptr);
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
	SIGDELSET(p->p_siglist, sig);
	action = ps->ps_sigact[_SIG_IDX(sig)];
#ifdef KTRACE
	if (KTRPOINT(p, KTR_PSIG))
		ktrpsig(p->p_tracep, sig, action, p->p_flag & P_OLDMASK ?
		    &p->p_oldsigmask : &p->p_sigmask, 0);
#endif
	_STOPEVENT(p, S_SIG, sig);

	if (action == SIG_DFL) {
		/*
		 * Default action, where the default is to kill
		 * the process.  (Other cases were ignored above.)
		 */
		sigexit(td, sig);
		/* NOTREACHED */
	} else {
		/*
		 * If we get here, the signal must be caught.
		 */
		KASSERT(action != SIG_IGN && !SIGISMEMBER(p->p_sigmask, sig),
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
		if (p->p_flag & P_OLDMASK) {
			returnmask = p->p_oldsigmask;
			p->p_flag &= ~P_OLDMASK;
		} else
			returnmask = p->p_sigmask;

		SIGSETOR(p->p_sigmask, ps->ps_catchmask[_SIG_IDX(sig)]);
		if (!SIGISMEMBER(ps->ps_signodefer, sig))
			SIGADDSET(p->p_sigmask, sig);

		if (SIGISMEMBER(ps->ps_sigreset, sig)) {
			/*
			 * See do_sigaction() for origin of this code.
			 */
			SIGDELSET(p->p_sigcatch, sig);
			if (sig != SIGCONT &&
			    sigprop(sig) & SA_IGNORE)
				SIGADDSET(p->p_sigignore, sig);
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
		(*p->p_sysent->sv_sendsig)(action, sig, &returnmask, code);
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
			    p->p_ucred ? p->p_ucred->cr_uid : -1,
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
const char *name; uid_t uid; pid_t pid; {
	char *temp;
	char buf[11];		/* Buffer for pid/uid -- max 4B */
	int i, n;
	char *format = corefilename;
	size_t namelen;

	temp = malloc(MAXPATHLEN + 1, M_TEMP, M_NOWAIT);
	if (temp == NULL)
		return NULL;
	namelen = strlen(name);
	for (i = 0, n = 0; n < MAXPATHLEN && format[i]; i++) {
		int l;
		switch (format[i]) {
		case '%':	/* Format character */
			i++;
			switch (format[i]) {
			case '%':
				temp[n++] = '%';
				break;
			case 'N':	/* process name */
				if ((n + namelen) > MAXPATHLEN) {
					log(LOG_ERR, "pid %d (%s), uid (%u):  Path `%s%s' is too long\n",
					    pid, name, uid, temp, name);
					free(temp, M_TEMP);
					return NULL;
				}
				memcpy(temp+n, name, namelen);
				n += namelen;
				break;
			case 'P':	/* process id */
				l = sprintf(buf, "%u", pid);
				if ((n + l) > MAXPATHLEN) {
					log(LOG_ERR, "pid %d (%s), uid (%u):  Path `%s%s' is too long\n",
					    pid, name, uid, temp, name);
					free(temp, M_TEMP);
					return NULL;
				}
				memcpy(temp+n, buf, l);
				n += l;
				break;
			case 'U':	/* user id */
				l = sprintf(buf, "%u", uid);
				if ((n + l) > MAXPATHLEN) {
					log(LOG_ERR, "pid %d (%s), uid (%u):  Path `%s%s' is too long\n",
					    pid, name, uid, temp, name);
					free(temp, M_TEMP);
					return NULL;
				}
				memcpy(temp+n, buf, l);
				n += l;
				break;
			default:
			  	log(LOG_ERR, "Unknown format character %c in `%s'\n", format[i], format);
			}
			break;
		default:
			temp[n++] = format[i];
		}
	}
	temp[n] = '\0';
	return temp;
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
	register struct ucred *cred = p->p_ucred;
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
	name = expand_name(p->p_comm, p->p_ucred->cr_uid, p->p_pid);
	if (name == NULL)
		return (EINVAL);
	NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_SYSSPACE, name, td); /* XXXKSE */
	flags = O_CREAT | FWRITE | O_NOFOLLOW;
	error = vn_open(&nd, &flags, S_IRUSR | S_IWUSR);
	free(name, M_TEMP);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;

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

	/* Don't dump to non-regular files or files with links. */
	if (vp->v_type != VREG ||
	    VOP_GETATTR(vp, &vattr, cred, td) || vattr.va_nlink != 1) {
		error = EFAULT;
		goto out1;
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

out1:
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

	mtx_lock(&Giant);
	PROC_LOCK(p);
	psignal(p, SIGSYS);
	PROC_UNLOCK(p);
	mtx_unlock(&Giant);
	return (EINVAL);
}

/*
 * Send a SIGIO or SIGURG signal to a process or process group using
 * stored credentials rather than those of the current process.
 */
void
pgsigio(sigio, sig, checkctty)
	struct sigio *sigio;
	int sig, checkctty;
{
	if (sigio == NULL)
		return;
		
	if (sigio->sio_pgid > 0) {
		PROC_LOCK(sigio->sio_proc);
		if (CANSIGIO(sigio->sio_ucred, sigio->sio_proc->p_ucred))
			psignal(sigio->sio_proc, sig);
		PROC_UNLOCK(sigio->sio_proc);
	} else if (sigio->sio_pgid < 0) {
		struct proc *p;

		LIST_FOREACH(p, &sigio->sio_pgrp->pg_members, p_pglist) {
			PROC_LOCK(p);
			if (CANSIGIO(sigio->sio_ucred, p->p_ucred) &&
			    (checkctty == 0 || (p->p_flag & P_CONTROLT)))
				psignal(p, sig);
			PROC_UNLOCK(p);
		}
	}
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
