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
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/event.h>
#include <sys/proc.h>
#include <sys/pioctl.h>
#include <sys/systm.h>
#include <sys/acct.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <sys/ktrace.h>
#include <sys/syslog.h>
#include <sys/stat.h>
#include <sys/sysent.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/unistd.h>


#include <machine/ipl.h>
#include <machine/cpu.h>
#include <machine/smp.h>

#define	ONSIG	32		/* NSIG for osig* syscalls.  XXX. */

static int coredump	__P((struct proc *));
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
 * Can process p, with pcred pc, send the signal sig to process q?
 */
#define CANSIGNAL(p, q, sig) \
	(!p_trespass(p, q) || \
	((sig) == SIGCONT && (q)->p_session == (p)->p_session))

/*
 * Policy -- Can real uid ruid with ucred uc send a signal to process q?
 */
#define CANSIGIO(ruid, uc, q) \
	((uc)->cr_uid == 0 || \
	    (ruid) == (q)->p_cred->p_ruid || \
	    (uc)->cr_uid == (q)->p_cred->p_ruid || \
	    (ruid) == (q)->p_ucred->cr_uid || \
	    (uc)->cr_uid == (q)->p_ucred->cr_uid)

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
	register struct sigacts *ps = p->p_sigacts;

	if (sig <= 0 || sig > _SIG_MAXSIG)
		return (EINVAL);

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
		    act->sa_handler != SIG_DFL)
			return (EINVAL);

		/*
		 * Change setting atomically.
		 */
		(void) splhigh();

		ps->ps_catchmask[_SIG_IDX(sig)] = act->sa_mask;
		SIG_CANTMASK(ps->ps_catchmask[_SIG_IDX(sig)]);
		if (act->sa_flags & SA_SIGINFO) {
			ps->ps_sigact[_SIG_IDX(sig)] = act->sa_handler;
			SIGADDSET(ps->ps_siginfo, sig);
		} else {
			ps->ps_sigact[_SIG_IDX(sig)] =
			    (__sighandler_t *)act->sa_sigaction;
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
			if (act->sa_flags & SA_NOCLDWAIT) {
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
		if (ps->ps_sigact[_SIG_IDX(sig)] == SIG_IGN ||
		    ps->ps_sigact[_SIG_IDX(sig)] == SIG_DFL || !old)
			SIGDELSET(ps->ps_osigset, sig);
		else
			SIGADDSET(ps->ps_osigset, sig);

		(void) spl0();
	}
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct sigaction_args {
	int	sig;
	struct	sigaction *act;
	struct	sigaction *oact;
};
#endif
/* ARGSUSED */
int
sigaction(p, uap)
	struct proc *p;
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
	error = do_sigaction(p, uap->sig, actp, oactp, 0);
	if (oactp && !error) {
		error = copyout(oactp, uap->oact, sizeof(oact));
	}
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct osigaction_args {
	int	signum;
	struct	osigaction *nsa;
	struct	osigaction *osa;
};
#endif
/* ARGSUSED */
int
osigaction(p, uap)
	struct proc *p;
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
	error = do_sigaction(p, uap->signum, nsap, osap, 1);
	if (osap && !error) {
		sa.sa_handler = osap->sa_handler;
		sa.sa_flags = osap->sa_flags;
		SIG2OSIG(osap->sa_mask, sa.sa_mask);
		error = copyout(&sa, uap->osa, sizeof(sa));
	}
	return (error);
}

/*
 * Initialize signal state for process 0;
 * set to ignore signals that are ignored by default.
 */
void
siginit(p)
	struct proc *p;
{
	register int i;

	for (i = 1; i <= NSIG; i++)
		if (sigprop(i) & SA_IGNORE && i != SIGCONT)
			SIGADDSET(p->p_sigignore, i);
}

/*
 * Reset signals for an exec of the specified process.
 */
void
execsigs(p)
	register struct proc *p;
{
	register struct sigacts *ps = p->p_sigacts;
	register int sig;

	/*
	 * Reset caught signals.  Held signals remain held
	 * through p_sigmask (unless they were caught,
	 * and are now ignored by default).
	 */
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
}

/*
 * do_sigprocmask() - MP SAFE ONLY IF p == curproc
 *
 *	Manipulate signal mask.  This routine is MP SAFE *ONLY* if
 *	p == curproc.  Also remember that in order to remain MP SAFE
 *	no spl*() calls may be made.
 */
static int
do_sigprocmask(p, how, set, oset, old)
	struct proc *p;
	int how;
	sigset_t *set, *oset;
	int old;
{
	int error;

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
sigprocmask(p, uap)
	register struct proc *p;
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
	error = do_sigprocmask(p, uap->how, setp, osetp, 0);
	if (osetp && !error) {
		error = copyout(osetp, uap->oset, sizeof(oset));
	}
	return (error);
}

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
osigprocmask(p, uap)
	register struct proc *p;
	struct osigprocmask_args *uap;
{
	sigset_t set, oset;
	int error;

	OSIG2SIG(uap->mask, set);
	error = do_sigprocmask(p, uap->how, &set, &oset, 1);
	SIG2OSIG(oset, p->p_retval[0]);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct sigpending_args {
	sigset_t	*set;
};
#endif
/* ARGSUSED */
int
sigpending(p, uap)
	struct proc *p;
	struct sigpending_args *uap;
{

	return (copyout(&p->p_siglist, uap->set, sizeof(sigset_t)));
}

#ifndef _SYS_SYSPROTO_H_
struct osigpending_args {
	int	dummy;
};
#endif
/* ARGSUSED */
int
osigpending(p, uap)
	struct proc *p;
	struct osigpending_args *uap;
{

	SIG2OSIG(p->p_siglist, p->p_retval[0]);
	return (0);
}

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
/* ARGSUSED */
int
osigvec(p, uap)
	struct proc *p;
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
	error = do_sigaction(p, uap->signum, nsap, osap, 1);
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
int
osigblock(p, uap)
	register struct proc *p;
	struct osigblock_args *uap;
{
	sigset_t set;

	OSIG2SIG(uap->mask, set);
	SIG_CANTMASK(set);
	(void) splhigh();
	SIG2OSIG(p->p_sigmask, p->p_retval[0]);
	SIGSETOR(p->p_sigmask, set);
	(void) spl0();
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct osigsetmask_args {
	int	mask;
};
#endif
int
osigsetmask(p, uap)
	struct proc *p;
	struct osigsetmask_args *uap;
{
	sigset_t set;

	OSIG2SIG(uap->mask, set);
	SIG_CANTMASK(set);
	(void) splhigh();
	SIG2OSIG(p->p_sigmask, p->p_retval[0]);
	SIGSETLO(p->p_sigmask, set);
	(void) spl0();
	return (0);
}
#endif /* COMPAT_43 || COMPAT_SUNOS */

/*
 * Suspend process until signal, providing mask to be set
 * in the meantime.  Note nonstandard calling convention:
 * libc stub passes mask, not pointer, to save a copyin.
 */
#ifndef _SYS_SYSPROTO_H_
struct sigsuspend_args {
	const sigset_t *sigmask;
};
#endif
/* ARGSUSED */
int
sigsuspend(p, uap)
	register struct proc *p;
	struct sigsuspend_args *uap;
{
	sigset_t mask;
	register struct sigacts *ps = p->p_sigacts;
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
	p->p_oldsigmask = p->p_sigmask;
	p->p_flag |= P_OLDMASK;

	SIG_CANTMASK(mask);
	p->p_sigmask = mask;
	while (tsleep((caddr_t) ps, PPAUSE|PCATCH, "pause", 0) == 0)
		/* void */;
	/* always return EINTR rather than ERESTART... */
	return (EINTR);
}

#ifndef _SYS_SYSPROTO_H_
struct osigsuspend_args {
	osigset_t mask;
};
#endif
/* ARGSUSED */
int
osigsuspend(p, uap)
	register struct proc *p;
	struct osigsuspend_args *uap;
{
	sigset_t mask;
	register struct sigacts *ps = p->p_sigacts;

	p->p_oldsigmask = p->p_sigmask;
	p->p_flag |= P_OLDMASK;
	OSIG2SIG(uap->mask, mask);
	SIG_CANTMASK(mask);
	SIGSETLO(p->p_sigmask, mask);
	while (tsleep((caddr_t) ps, PPAUSE|PCATCH, "opause", 0) == 0)
		/* void */;
	/* always return EINTR rather than ERESTART... */
	return (EINTR);
}

#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
#ifndef _SYS_SYSPROTO_H_
struct osigstack_args {
	struct	sigstack *nss;
	struct	sigstack *oss;
};
#endif
/* ARGSUSED */
int
osigstack(p, uap)
	struct proc *p;
	register struct osigstack_args *uap;
{
	struct sigstack ss;
	int error = 0;

	ss.ss_sp = p->p_sigstk.ss_sp;
	ss.ss_onstack = p->p_sigstk.ss_flags & SS_ONSTACK;
	if (uap->oss && (error = copyout(&ss, uap->oss,
	    sizeof(struct sigstack))))
		return (error);
	if (uap->nss && (error = copyin(uap->nss, &ss, sizeof(ss))) == 0) {
		p->p_sigstk.ss_sp = ss.ss_sp;
		p->p_sigstk.ss_size = 0;
		p->p_sigstk.ss_flags |= ss.ss_onstack & SS_ONSTACK;
		p->p_flag |= P_ALTSTACK;
	}
	return (error);
}
#endif /* COMPAT_43 || COMPAT_SUNOS */

#ifndef _SYS_SYSPROTO_H_
struct sigaltstack_args {
	stack_t	*ss;
	stack_t	*oss;
};
#endif
/* ARGSUSED */
int
sigaltstack(p, uap)
	struct proc *p;
	register struct sigaltstack_args *uap;
{
	stack_t ss;
	int error;

	if ((p->p_flag & P_ALTSTACK) == 0)
		p->p_sigstk.ss_flags |= SS_DISABLE;
	if (uap->oss && (error = copyout(&p->p_sigstk, uap->oss,
	    sizeof(stack_t))))
		return (error);
	if (uap->ss == 0)
		return (0);
	if ((error = copyin(uap->ss, &ss, sizeof(ss))))
		return (error);
	if (ss.ss_flags & SS_DISABLE) {
		if (p->p_sigstk.ss_flags & SS_ONSTACK)
			return (EINVAL);
		p->p_flag &= ~P_ALTSTACK;
		p->p_sigstk.ss_flags = ss.ss_flags;
		return (0);
	}
	if (ss.ss_size < p->p_sysent->sv_minsigstksz)
		return (ENOMEM);
	p->p_flag |= P_ALTSTACK;
	p->p_sigstk = ss;
	return (0);
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

	if (all)
		/*
		 * broadcast
		 */
		LIST_FOREACH(p, &allproc, p_list) {
			if (p->p_pid <= 1 || p->p_flag & P_SYSTEM ||
			    p == cp || !CANSIGNAL(cp, p, sig))
				continue;
			nfound++;
			if (sig)
				psignal(p, sig);
		}
	else {
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
			if (p->p_pid <= 1 || p->p_flag & P_SYSTEM ||
			    p->p_stat == SZOMB ||
			    !CANSIGNAL(cp, p, sig))
				continue;
			nfound++;
			if (sig)
				psignal(p, sig);
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
/* ARGSUSED */
int
kill(cp, uap)
	register struct proc *cp;
	register struct kill_args *uap;
{
	register struct proc *p;

	if ((u_int)uap->signum > _SIG_MAXSIG)
		return (EINVAL);
	if (uap->pid > 0) {
		/* kill single process */
		if ((p = pfind(uap->pid)) == NULL)
			return (ESRCH);
		if (!CANSIGNAL(cp, p, uap->signum))
			return (EPERM);
		if (uap->signum)
			psignal(p, uap->signum);
		return (0);
	}
	switch (uap->pid) {
	case -1:		/* broadcast signal */
		return (killpg1(cp, uap->signum, 0, 1));
	case 0:			/* signal own process group */
		return (killpg1(cp, uap->signum, 0, 0));
	default:		/* negative explicit process group */
		return (killpg1(cp, uap->signum, -uap->pid, 0));
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
/* ARGSUSED */
int
okillpg(p, uap)
	struct proc *p;
	register struct okillpg_args *uap;
{

	if ((u_int)uap->signum > _SIG_MAXSIG)
		return (EINVAL);
	return (killpg1(p, uap->signum, uap->pgid, 0));
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

	if (pgrp)
		LIST_FOREACH(p, &pgrp->pg_members, p_pglist)
			if (checkctty == 0 || p->p_flag & P_CONTROLT)
				psignal(p, sig);
}

/*
 * Send a signal caused by a trap to the current process.
 * If it will be caught immediately, deliver it with correct code.
 * Otherwise, post it normally.
 */
void
trapsignal(p, sig, code)
	struct proc *p;
	register int sig;
	u_long code;
{
	register struct sigacts *ps = p->p_sigacts;

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
	register int s, prop;
	register sig_t action;

	if (sig > _SIG_MAXSIG || sig <= 0) {
		printf("psignal: signal %d\n", sig);
		panic("psignal signal number");
	}

	KNOTE(&p->p_klist, NOTE_SIGNAL | sig);

	prop = sigprop(sig);

	/*
	 * If proc is traced, always give parent a chance;
	 * if signal event is tracked by procfs, give *that*
	 * a chance, as well.
	 */
	if ((p->p_flag & P_TRACED) || (p->p_stops & S_SIG))
		action = SIG_DFL;
	else {
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

	if (p->p_nice > NZERO && action == SIG_DFL && (prop & SA_KILL) &&
	    (p->p_flag & P_TRACED) == 0)
		p->p_nice = NZERO;

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
	if (action == SIG_HOLD && (!(prop & SA_CONT) || p->p_stat != SSTOP))
		return;
	s = splhigh();
	switch (p->p_stat) {

	case SSLEEP:
		/*
		 * If process is sleeping uninterruptibly
		 * we can't interrupt the sleep... the signal will
		 * be noticed when the process returns through
		 * trap() or syscall().
		 */
		if ((p->p_flag & P_SINTR) == 0)
			goto out;
		/*
		 * Process is sleeping and traced... make it runnable
		 * so it can discover the signal in issignal() and stop
		 * for the parent.
		 */
		if (p->p_flag & P_TRACED)
			goto run;
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
			if ((p->p_pptr->p_procsig->ps_flag & PS_NOCLDSTOP) == 0)
				psignal(p->p_pptr, SIGCHLD);
			stop(p);
			goto out;
		} else
			goto runfast;
		/*NOTREACHED*/

	case SSTOP:
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
			if (p->p_wchan == 0)
				goto run;
			p->p_stat = SSLEEP;
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
		 */
		if (p->p_wchan && p->p_flag & P_SINTR)
			unsleep(p);
		goto out;

	default:
		/*
		 * SRUN, SIDL, SZOMB do nothing with the signal,
		 * other than kicking ourselves if we are running.
		 * It will either never be noticed, or noticed very soon.
		 */
		if (p == curproc)
			signotify(p);
#ifdef SMP
		else if (p->p_stat == SRUN)
			forward_signal(p);
#endif
		goto out;
	}
	/*NOTREACHED*/

runfast:
	/*
	 * Raise priority to at least PUSER.
	 */
	if (p->p_priority > PUSER)
		p->p_priority = PUSER;
run:
	setrunnable(p);
out:
	splx(s);
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

	for (;;) {
		int traced = (p->p_flag & P_TRACED) || (p->p_stops & S_SIG);

		mask = p->p_siglist;
		SIGSETNAND(mask, p->p_sigmask);
		if (p->p_flag & P_PPWAIT)
			SIG_STOPSIGMASK(mask);
		if (!SIGNOTEMPTY(mask))	 	/* no signal to send */
			return (0);
		sig = sig_ffs(&mask);
		prop = sigprop(sig);

		STOPEVENT(p, S_SIG, sig);

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
			psignal(p->p_pptr, SIGCHLD);
			do {
				stop(p);
				mi_switch();
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
				stop(p);
				if ((p->p_pptr->p_procsig->ps_flag & PS_NOCLDSTOP) == 0)
					psignal(p->p_pptr, SIGCHLD);
				mi_switch();
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
 * on the run queue.
 */
void
stop(p)
	register struct proc *p;
{

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
	register struct proc *p = curproc;
	struct sigacts *ps = p->p_sigacts;
	sig_t action;
	sigset_t returnmask;
	int code;

	KASSERT(sig != 0, ("postsig"));

	SIGDELSET(p->p_siglist, sig);
	action = ps->ps_sigact[_SIG_IDX(sig)];
#ifdef KTRACE
	if (KTRPOINT(p, KTR_PSIG))
		ktrpsig(p->p_tracep, sig, action, p->p_flag & P_OLDMASK ?
		    &p->p_oldsigmask : &p->p_sigmask, 0);
#endif
	STOPEVENT(p, S_SIG, sig);

	if (action == SIG_DFL) {
		/*
		 * Default action, where the default is to kill
		 * the process.  (Other cases were ignored above.)
		 */
		sigexit(p, sig);
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
		(void) splhigh();
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
		(void) spl0();
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
	log(LOG_ERR, "pid %d (%s), uid %d, was killed: %s\n", p->p_pid, p->p_comm,
		p->p_cred && p->p_ucred ? p->p_ucred->cr_uid : -1, why);
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
sigexit(p, sig)
	register struct proc *p;
	int sig;
{

	p->p_acflag |= AXSIG;
	if (sigprop(sig) & SA_CORE) {
		p->p_sig = sig;
		/*
		 * Log signals which would cause core dumps
		 * (Log as LOG_INFO to appease those who don't want
		 * these messages.)
		 * XXX : Todo, as well as euid, write out ruid too
		 */
		if (coredump(p) == 0)
			sig |= WCOREFLAG;
		if (kern_logsigexit)
			log(LOG_INFO,
			    "pid %d (%s), uid %d: exited on signal %d%s\n",
			    p->p_pid, p->p_comm,
			    p->p_cred && p->p_ucred ? p->p_ucred->cr_uid : -1,
			    sig &~ WCOREFLAG,
			    sig & WCOREFLAG ? " (core dumped)" : "");
	}
	exit1(p, W_EXITCODE(0, sig));
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
coredump(p)
	register struct proc *p;
{
	register struct vnode *vp;
	register struct ucred *cred = p->p_ucred;
	struct flock lf;
	struct nameidata nd;
	struct vattr vattr;
	int error, error1;
	char *name;			/* name of corefile */
	off_t limit;
	
	STOPEVENT(p, S_CORE, 0);

	if (((sugid_coredump == 0) && p->p_flag & P_SUGID) || do_coredump == 0)
		return (EFAULT);
	
	/*
	 * Note that the bulk of limit checking is done after
	 * the corefile is created.  The exception is if the limit
	 * for corefiles is 0, in which case we don't bother
	 * creating the corefile at all.  This layout means that
	 * a corefile is truncated instead of not being created,
	 * if it is larger than the limit.
	 */
	limit = p->p_rlimit[RLIMIT_CORE].rlim_cur;
	if (limit == 0)
		return 0;

	name = expand_name(p->p_comm, p->p_ucred->cr_uid, p->p_pid);
	if (name == NULL)
		return (EINVAL);
	NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_SYSSPACE, name, p);
	error = vn_open(&nd, O_CREAT | FWRITE | O_NOFOLLOW, S_IRUSR | S_IWUSR);
	free(name, M_TEMP);
	if (error)
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp = nd.ni_vp;

	VOP_UNLOCK(vp, 0, p);
	lf.l_whence = SEEK_SET;
	lf.l_start = 0;
	lf.l_len = 0;
	lf.l_type = F_WRLCK;
	error = VOP_ADVLOCK(vp, (caddr_t)p, F_SETLK, &lf, F_FLOCK);
	if (error)
		goto out2;

	/* Don't dump to non-regular files or files with links. */
	if (vp->v_type != VREG ||
	    VOP_GETATTR(vp, &vattr, cred, p) || vattr.va_nlink != 1) {
		error = EFAULT;
		goto out1;
	}
	VATTR_NULL(&vattr);
	vattr.va_size = 0;
	VOP_LEASE(vp, p, cred, LEASE_WRITE);
	VOP_SETATTR(vp, &vattr, cred, p);
	p->p_acflag |= ACORE;

	error = p->p_sysent->sv_coredump ?
	  p->p_sysent->sv_coredump(p, vp, limit) :
	  ENOSYS;

out1:
	lf.l_type = F_UNLCK;
	VOP_ADVLOCK(vp, (caddr_t)p, F_UNLCK, &lf, F_FLOCK);
out2:
	error1 = vn_close(vp, FWRITE, cred, p);
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
/* ARGSUSED */
int
nosys(p, args)
	struct proc *p;
	struct nosys_args *args;
{

	psignal(p, SIGSYS);
	return (EINVAL);
}

/*
 * Send a signal to a SIGIO or SIGURG to a process or process group using
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
		if (CANSIGIO(sigio->sio_ruid, sigio->sio_ucred,
		             sigio->sio_proc))
			psignal(sigio->sio_proc, sig);
	} else if (sigio->sio_pgid < 0) {
		struct proc *p;

		LIST_FOREACH(p, &sigio->sio_pgrp->pg_members, p_pglist)
			if (CANSIGIO(sigio->sio_ruid, sigio->sio_ucred, p) &&
			    (checkctty == 0 || (p->p_flag & P_CONTROLT)))
				psignal(p, sig);
	}
}

static int
filt_sigattach(struct knote *kn)
{
	struct proc *p = curproc;

	kn->kn_ptr.p_proc = p;
	kn->kn_flags |= EV_CLEAR;		/* automatically set */

	/* XXX lock the proc here while adding to the list? */
	SLIST_INSERT_HEAD(&p->p_klist, kn, kn_selnext);

	return (0);
}

static void
filt_sigdetach(struct knote *kn)
{
	struct proc *p = kn->kn_ptr.p_proc;

	SLIST_REMOVE(&p->p_klist, kn, knote, kn_selnext);
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
