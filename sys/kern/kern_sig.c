/*
 * Copyright (c) 1982, 1986, 1989, 1991 Regents of the University of California.
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
 *	from: @(#)kern_sig.c	7.35 (Berkeley) 6/28/91
 *	$Id: kern_sig.c,v 1.12 1994/05/25 19:49:38 csgr Exp $
 */

#define	SIGPROP		/* include signal properties table */
#include "param.h"
#include "signalvar.h"
#include "resourcevar.h"
#include "namei.h"
#include "vnode.h"
#include "mount.h"
#include "filedesc.h"
#include "proc.h"
#include "ucred.h"
#include "systm.h"
#include "timeb.h"
#include "times.h"
#include "buf.h"
#include "acct.h"
#include "file.h"
#include "kernel.h"
#include "wait.h"
#include "ktrace.h"
#include "syslog.h"

#include "machine/cpu.h"

#include "vm/vm.h"
#include "kinfo_proc.h"
#include "user.h"		/* for coredump */

static void setsigvec(struct proc *, int, struct sigaction *);
static void stop(struct proc *);
static void sigexit(struct proc *, int);
static int killpg1(struct proc *, int, int, int);

/*
 * Can process p, with pcred pc, send the signal signo to process q?
 */
#define CANSIGNAL(p, pc, q, signo) \
	((pc)->pc_ucred->cr_uid == 0 || \
	    (pc)->p_ruid == (q)->p_cred->p_ruid || \
	    (pc)->pc_ucred->cr_uid == (q)->p_cred->p_ruid || \
	    (pc)->p_ruid == (q)->p_ucred->cr_uid || \
	    (pc)->pc_ucred->cr_uid == (q)->p_ucred->cr_uid || \
	    ((signo) == SIGCONT && (q)->p_session == (p)->p_session))

struct sigaction_args {
	int	signo;
	struct	sigaction *nsa;
	struct	sigaction *osa;
};

/* ARGSUSED */
int
sigaction(p, uap, retval)
	struct proc *p;
	register struct sigaction_args *uap;
	int *retval;
{
	struct sigaction vec;
	register struct sigaction *sa;
	register struct sigacts *ps = p->p_sigacts;
	register int sig;
	int bit, error;

	sig = uap->signo;
	if (sig <= 0 || sig >= NSIG || sig == SIGKILL || sig == SIGSTOP)
		return (EINVAL);
	sa = &vec;
	if (uap->osa) {
		sa->sa_handler = ps->ps_sigact[sig];
		sa->sa_mask = ps->ps_catchmask[sig];
		bit = sigmask(sig);
		sa->sa_flags = 0;
		if ((ps->ps_sigonstack & bit) != 0)
			sa->sa_flags |= SA_ONSTACK;
		if ((ps->ps_sigintr & bit) == 0)
			sa->sa_flags |= SA_RESTART;
		if (p->p_flag & SNOCLDSTOP)
			sa->sa_flags |= SA_NOCLDSTOP;
		if (error = copyout((caddr_t)sa, (caddr_t)uap->osa,
		    sizeof (vec)))
			return (error);
	}
	if (uap->nsa) {
		if (error = copyin((caddr_t)uap->nsa, (caddr_t)sa,
		    sizeof (vec)))
			return (error);
		setsigvec(p, sig, sa);
	}
	return (0);
}

void
setsigvec(p, sig, sa)
	register struct proc *p;
	int sig;
	register struct sigaction *sa;
{
	register struct sigacts *ps = p->p_sigacts;
	register int bit;

	bit = sigmask(sig);
	/*
	 * Change setting atomically.
	 */
	(void) splhigh();
	ps->ps_sigact[sig] = sa->sa_handler;
	ps->ps_catchmask[sig] = sa->sa_mask &~ sigcantmask;
	if ((sa->sa_flags & SA_RESTART) == 0)
		ps->ps_sigintr |= bit;
	else
		ps->ps_sigintr &= ~bit;
	if (sa->sa_flags & SA_ONSTACK)
		ps->ps_sigonstack |= bit;
	else
		ps->ps_sigonstack &= ~bit;
	if (sig == SIGCHLD) {
		if (sa->sa_flags & SA_NOCLDSTOP)
			p->p_flag |= SNOCLDSTOP;
		else
			p->p_flag &= ~SNOCLDSTOP;
	}
	/*
	 * Set bit in p_sigignore for signals that are set to SIG_IGN,
	 * and for signals set to SIG_DFL where the default is to ignore.
	 * However, don't put SIGCONT in p_sigignore,
	 * as we have to restart the process.
	 */
	if (sa->sa_handler == SIG_IGN ||
	    (sigprop[sig] & SA_IGNORE && sa->sa_handler == SIG_DFL)) {
		p->p_sig &= ~bit;		/* never to be seen again */
		if (sig != SIGCONT)
			p->p_sigignore |= bit;	/* easier in psignal */
		p->p_sigcatch &= ~bit;
	} else {
		p->p_sigignore &= ~bit;
		if (sa->sa_handler == SIG_DFL)
			p->p_sigcatch &= ~bit;
		else
			p->p_sigcatch |= bit;
	}
	(void) spl0();
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

	for (i = 0; i < NSIG; i++)
		if (sigprop[i] & SA_IGNORE && i != SIGCONT)
			p->p_sigignore |= sigmask(i);
}

/*
 * Reset signals for an exec of the specified process.
 */
void
execsigs(p)
	register struct proc *p;
{
	register struct sigacts *ps = p->p_sigacts;
	register int nc, mask;

	/*
	 * Reset caught signals.  Held signals remain held
	 * through p_sigmask (unless they were caught,
	 * and are now ignored by default).
	 */
	while (p->p_sigcatch) {
		nc = ffs((long)p->p_sigcatch);
		mask = sigmask(nc);
		p->p_sigcatch &= ~mask;
		if (sigprop[nc] & SA_IGNORE) {
			if (nc != SIGCONT)
				p->p_sigignore |= mask;
			p->p_sig &= ~mask;
		}
		ps->ps_sigact[nc] = SIG_DFL;
	}
	/*
	 * Reset stack state to the user stack.
	 * Clear set of signals caught on the signal stack.
	 */
	ps->ps_onstack = 0;
	ps->ps_sigsp = 0;
	ps->ps_sigonstack = 0;
}

/*
 * Manipulate signal mask.
 * Note that we receive new mask, not pointer,
 * and return old mask as return value;
 * the library stub does the rest.
 */

struct sigprocmask_args {
	int	how;
	sigset_t mask;
};

int
sigprocmask(p, uap, retval)
	register struct proc *p;
	struct sigprocmask_args *uap;
	int *retval;
{
	int error = 0;

	*retval = p->p_sigmask;
	(void) splhigh();

	switch (uap->how) {
	case SIG_BLOCK:
		p->p_sigmask |= uap->mask &~ sigcantmask;
		break;

	case SIG_UNBLOCK:
		p->p_sigmask &= ~uap->mask;
		break;

	case SIG_SETMASK:
		p->p_sigmask = uap->mask &~ sigcantmask;
		break;
	
	default:
		error = EINVAL;
		break;
	}
	(void) spl0();
	return (error);
}

/* ARGSUSED */
int
sigpending(p, uap, retval)
	struct proc *p;
	void *uap;
	int *retval;
{

	*retval = p->p_sig;
	return (0);
}

#ifdef COMPAT_43
/*
 * Generalized interface signal handler, 4.3-compatible.
 */

struct osigvec_args {
	int	signo;
	struct	sigvec *nsv;
	struct	sigvec *osv;
};

/* ARGSUSED */
int
osigvec(p, uap, retval)
	struct proc *p;
	register struct osigvec_args *uap;
	int *retval;
{
	struct sigvec vec;
	register struct sigacts *ps = p->p_sigacts;
	register struct sigvec *sv;
	register int sig;
	int bit, error;

	sig = uap->signo;
	if (sig <= 0 || sig >= NSIG || sig == SIGKILL || sig == SIGSTOP)
		return (EINVAL);
	sv = &vec;
	if (uap->osv) {
		*(sig_t *)&sv->sv_handler = ps->ps_sigact[sig];
		sv->sv_mask = ps->ps_catchmask[sig];
		bit = sigmask(sig);
		sv->sv_flags = 0;
		if ((ps->ps_sigonstack & bit) != 0)
			sv->sv_flags |= SV_ONSTACK;
		if ((ps->ps_sigintr & bit) != 0)
			sv->sv_flags |= SV_INTERRUPT;
		if (p->p_flag & SNOCLDSTOP)
			sv->sv_flags |= SA_NOCLDSTOP;
		if (error = copyout((caddr_t)sv, (caddr_t)uap->osv,
		    sizeof (vec)))
			return (error);
	}
	if (uap->nsv) {
		if (error = copyin((caddr_t)uap->nsv, (caddr_t)sv,
		    sizeof (vec)))
			return (error);
		sv->sv_flags ^= SA_RESTART;	/* opposite of SV_INTERRUPT */
		setsigvec(p, sig, (struct sigaction *)sv);
	}
	return (0);
}

struct osigblock_args {
	int	mask;
};

int
osigblock(p, uap, retval)
	register struct proc *p;
	struct osigblock_args *uap;
	int *retval;
{

	(void) splhigh();
	*retval = p->p_sigmask;
	p->p_sigmask |= uap->mask &~ sigcantmask;
	(void) spl0();
	return (0);
}

struct osigsetmask_args {
	int	mask;
};

int
osigsetmask(p, uap, retval)
	struct proc *p;
	struct osigsetmask_args *uap;
	int *retval;
{

	(void) splhigh();
	*retval = p->p_sigmask;
	p->p_sigmask = uap->mask &~ sigcantmask;
	(void) spl0();
	return (0);
}
#endif

/*
 * Suspend process until signal, providing mask to be set
 * in the meantime.  Note nonstandard calling convention:
 * libc stub passes mask, not pointer, to save a copyin.
 */

struct sigsuspend_args {
	sigset_t mask;
};

/* ARGSUSED */
int
sigsuspend(p, uap, retval)
	register struct proc *p;
	struct sigsuspend_args *uap;
	int *retval;
{
	register struct sigacts *ps = p->p_sigacts;

	/*
	 * When returning from sigpause, we want
	 * the old mask to be restored after the
	 * signal handler has finished.  Thus, we
	 * save it here and mark the proc structure
	 * to indicate this (should be in sigacts).
	 */
	ps->ps_oldmask = p->p_sigmask;
	ps->ps_flags |= SA_OLDMASK;
	p->p_sigmask = uap->mask &~ sigcantmask;
	(void) tsleep((caddr_t) ps, PPAUSE|PCATCH, "pause", 0);
	/* always return EINTR rather than ERESTART... */
	return (EINTR);
}

struct sigstack_args {
	struct	sigstack *nss;
	struct	sigstack *oss;
};

/* ARGSUSED */
int
sigstack(p, uap, retval)
	struct proc *p;
	register struct sigstack_args *uap;
	int *retval;
{
	struct sigstack ss;
	int error = 0;

	if (uap->oss && (error = copyout((caddr_t)&p->p_sigacts->ps_sigstack,
	    (caddr_t)uap->oss, sizeof (struct sigstack))))
		return (error);
	if (uap->nss && (error = copyin((caddr_t)uap->nss, (caddr_t)&ss,
	    sizeof (ss))) == 0)
		p->p_sigacts->ps_sigstack = ss;
	return (error);
}

struct kill_args {
	int	pid;
	int	signo;
};

/* ARGSUSED */
int
kill(cp, uap, retval)
	register struct proc *cp;
	register struct kill_args *uap;
	int *retval;
{
	register struct proc *p;
	register struct pcred *pc = cp->p_cred;

	if ((unsigned) uap->signo >= NSIG)
		return (EINVAL);
	if (uap->pid > 0) {
		/* kill single process */
		p = pfind(uap->pid);
		if (p == 0)
			return (ESRCH);
		if (!CANSIGNAL(cp, pc, p, uap->signo))
			return (EPERM);
		if (uap->signo)
			psignal(p, uap->signo);
		return (0);
	}
	switch (uap->pid) {
	case -1:		/* broadcast signal */
		return (killpg1(cp, uap->signo, 0, 1));
	case 0:			/* signal own process group */
		return (killpg1(cp, uap->signo, 0, 0));
	default:		/* negative explicit process group */
		return (killpg1(cp, uap->signo, -uap->pid, 0));
	}
	/* NOTREACHED */
}

#ifdef COMPAT_43

struct okillpg_args {
	int	pgid;
	int	signo;
};

/* ARGSUSED */
int
okillpg(p, uap, retval)
	struct proc *p;
	register struct okillpg_args *uap;
	int *retval;
{

	if ((unsigned) uap->signo >= NSIG)
		return (EINVAL);
	return (killpg1(p, uap->signo, uap->pgid, 0));
}
#endif

/*
 * Common code for kill process group/broadcast kill.
 * cp is calling process.
 */
static int
killpg1(cp, signo, pgid, all)
	register struct proc *cp;
	int signo, pgid, all;
{
	register struct proc *p;
	register struct pcred *pc = cp->p_cred;
	struct pgrp *pgrp;
	int nfound = 0;
	
	if (all)	
		/* 
		 * broadcast 
		 */
		for (p = allproc; p != NULL; p = p->p_nxt) {
			if (p->p_pid <= 1 || p->p_flag&SSYS || 
			    p == cp || !CANSIGNAL(cp, pc, p, signo))
				continue;
			nfound++;
			if (signo)
				psignal(p, signo);
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
		for (p = pgrp->pg_mem; p != NULL; p = p->p_pgrpnxt) {
			if (p->p_pid <= 1 || p->p_flag&SSYS ||
			    p->p_stat == SZOMB || !CANSIGNAL(cp, pc, p, signo))
				continue;
			nfound++;
			if (signo)
				psignal(p, signo);
		}
	}
	return (nfound ? 0 : ESRCH);
}

/*
 * Send the specified signal to
 * all processes with 'pgid' as
 * process group.
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
 * Send sig to every member of a process group.
 * If checktty is 1, limit to members which have a controlling
 * terminal.
 */
void
pgsignal(pgrp, sig, checkctty)
	struct pgrp *pgrp;
	int sig, checkctty;
{
	register struct proc *p;

	if (pgrp)
		for (p = pgrp->pg_mem; p != NULL; p = p->p_pgrpnxt)
			if (checkctty == 0 || p->p_flag&SCTTY)
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
	unsigned code;
{
	register struct sigacts *ps = p->p_sigacts;
	int mask;

	mask = sigmask(sig);
	if (p == curproc && (p->p_flag & STRC) == 0 &&
	    (p->p_sigcatch & mask) != 0 && (p->p_sigmask & mask) == 0) {
		p->p_stats->p_ru.ru_nsignals++;
#ifdef KTRACE
		if (KTRPOINT(p, KTR_PSIG))
			ktrpsig(p->p_tracep, sig, ps->ps_sigact[sig], 
				p->p_sigmask, code);
#endif
		sendsig(ps->ps_sigact[sig], sig, p->p_sigmask, code);
		p->p_sigmask |= ps->ps_catchmask[sig] | mask;
	} else {
		ps->ps_code = code;	/* XXX for core dump/debugger */
		psignal(p, sig);
	}
}

/*
 * Send the specified signal to the specified process.
 * If the signal has an action, the action is usually performed
 * by the target process rather than the caller; we simply add
 * the signal to the set of pending signals for the process.
 * Exceptions:
 *   o When a stop signal is sent to a sleeping process that takes the default
 *     action, the process is stopped without awakening it.
 *   o SIGCONT restarts stopped processes (or puts them back to sleep)
 *     regardless of the signal action (eg, blocked or ignored).
 * Other ignored signals are discarded immediately.
 */
void
psignal(p, sig)
	register struct proc *p;
	register int sig;
{
	register int s, prop;
	register sig_t action;
	int mask;

	/* Ignore signals to system (internal) daemons */
	if (p->p_flag & SSYS)
		return;

	if ((unsigned)sig >= NSIG || sig == 0)
		panic("psignal sig");
	mask = sigmask(sig);
	prop = sigprop[sig];

	/*
	 * If proc is traced, always give parent a chance.
	 */
	if (p->p_flag & STRC)
		action = SIG_DFL;
	else {
		/*
		 * If the signal is being ignored,
		 * then we forget about it immediately.
		 * (Note: we don't set SIGCONT in p_sigignore,
		 * and if it is set to SIG_IGN,
		 * action will be SIG_DFL here.)
		 */
		if (p->p_sigignore & mask)
			return;
		if (p->p_sigmask & mask)
			action = SIG_HOLD;
		else if (p->p_sigcatch & mask)
			action = SIG_CATCH;
		else
			action = SIG_DFL;
	}

	if (p->p_nice > NZERO && (sig == SIGKILL ||
	    sig == SIGTERM && (p->p_flag&STRC || action != SIG_DFL)))
		p->p_nice = NZERO;

	if (prop & SA_CONT)
		p->p_sig &= ~stopsigmask;

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
		p->p_sig &= ~contsigmask;
	}
	p->p_sig |= mask;

	/*
	 * Defer further processing for signals which are held,
	 * except that stopped processes must be continued by SIGCONT.
	 */
	if (action == SIG_HOLD && ((prop & SA_CONT) == 0 || p->p_stat != SSTOP))
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
		if ((p->p_flag & SSINTR) == 0)
			goto out;
		/*
		 * Process is sleeping and traced... make it runnable
		 * so it can discover the signal in issig() and stop
		 * for the parent.
		 */
		if (p->p_flag&STRC)
			goto run;
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
			if (p->p_flag&SPPWAIT)
				goto out;
			p->p_sig &= ~mask;
			p->p_xstat = sig;
			if ((p->p_pptr->p_flag & SNOCLDSTOP) == 0)
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
		if (p->p_flag&STRC)
			goto out;

		/*
		 * Kill signal always sets processes running.
		 */
		if (sig == SIGKILL)
			goto runfast;

		if (prop & SA_CONT) {
			/*
			 * If SIGCONT is default (or ignored), we continue
			 * the process but don't leave the signal in p_sig,
			 * as it has no further action.  If SIGCONT is held,
			 * continue the process and leave the signal in p_sig.
			 * If the process catches SIGCONT, let it handle
			 * the signal itself.  If it isn't waiting on
			 * an event, then it goes back to run state.
			 * Otherwise, process goes back to sleep state.
			 */
			if (action == SIG_DFL)
				p->p_sig &= ~mask;
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
			p->p_sig &= ~mask;		/* take it away */
			goto out;
		}

		/*
		 * If process is sleeping interruptibly, then
		 * simulate a wakeup so that when it is continued,
		 * it will be made runnable and can look at the signal.
		 * But don't setrun the process, leave it stopped.
		 */
		if (p->p_wchan && p->p_flag & SSINTR)
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
		goto out;
	}
	/*NOTREACHED*/

runfast:
	/*
	 * Raise priority to at least PUSER.
	 */
	if (p->p_pri > PUSER)
		p->p_pri = PUSER;
run:
	setrun(p);
out:
	splx(s);
}

/*
 * If the current process has a signal to process (should be caught
 * or cause termination, should interrupt current syscall),
 * return the signal number.  Stop signals with default action
 * are processed immediately, then cleared; they aren't returned.
 * This is checked after each entry to the system for a syscall
 * or trap (though this can usually be done without actually calling
 * issig by checking the pending signal masks in the CURSIG macro.)
 * The normal call sequence is
 *
 *	while (sig = CURSIG(curproc))
 *		psig(sig);
 */
int
issig(p)
	register struct proc *p;
{
	register int sig, mask, prop;

	for (;;) {
		mask = p->p_sig &~ p->p_sigmask;
		if (p->p_flag&SPPWAIT)
			mask &= ~stopsigmask;
		if (mask == 0)	 	/* no signal to send */
			return (0);
		sig = ffs((long)mask);
		mask = sigmask(sig);
		prop = sigprop[sig];
		/*
		 * We should see pending but ignored signals
		 * only if STRC was on when they were posted.
		 */
		if (mask & p->p_sigignore && (p->p_flag&STRC) == 0) {
			p->p_sig &= ~mask;
			continue;
		}
		if (p->p_flag&STRC && (p->p_flag&SPPWAIT) == 0) {
			/*
			 * If traced, always stop, and stay
			 * stopped until released by the parent.
			 */
			p->p_xstat = sig;
			psignal(p->p_pptr, SIGCHLD);
			do {
				stop(p);
				(void) splclock();
				swtch();
				(void) splnone();
			} while (!procxmt(p) && p->p_flag&STRC);

			/*
			 * If the traced bit got turned off,
			 * go back up to the top to rescan signals.
			 * This ensures that p_sig* and ps_sigact
			 * are consistent.
			 */
			if ((p->p_flag&STRC) == 0)
				continue;

			/*
			 * If parent wants us to take the signal,
			 * then it will leave it in p->p_xstat;
			 * otherwise we just look for signals again.
			 */
			p->p_sig &= ~mask;	/* clear the old signal */
			sig = p->p_xstat;
			if (sig == 0)
				continue;

			/*
			 * Put the new signal into p_sig.
			 * If signal is being masked,
			 * look for other signals.
			 */
			mask = sigmask(sig);
			p->p_sig |= mask;
			if (p->p_sigmask & mask)
				continue;
		}

		/*
		 * Decide whether the signal should be returned.
		 * Return the signal's number, or fall through
		 * to clear it from the pending mask.
		 */
		switch ((int)p->p_sigacts->ps_sigact[sig]) {

		case SIG_DFL:
			/*
			 * Don't take default actions on system processes.
			 */
			if (p->p_pid <= 1)
				break;		/* == ignore */
			/*
			 * If there is a pending stop signal to process
			 * with default action, stop here,
			 * then clear the signal.  However,
			 * if process is member of an orphaned
			 * process group, ignore tty stop signals.
			 */
			if (prop & SA_STOP) {
				if (p->p_flag&STRC ||
		    		    (p->p_pgrp->pg_jobc == 0 &&
				    prop & SA_TTYSTOP))
					break;	/* == ignore */
				p->p_xstat = sig;
				stop(p);
				if ((p->p_pptr->p_flag & SNOCLDSTOP) == 0)
					psignal(p->p_pptr, SIGCHLD);
				(void) splclock();
				swtch();
				(void) splnone();
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

		case SIG_IGN:
			/*
			 * Masking above should prevent us ever trying
			 * to take action on an ignored signal other
			 * than SIGCONT, unless process is traced.
			 */
			if ((prop & SA_CONT) == 0 && (p->p_flag&STRC) == 0)
				printf("issig\n");
			break;		/* == ignore */

		default:
			/*
			 * This signal has an action, let
			 * psig process it.
			 */
			return (sig);
		}
		p->p_sig &= ~mask;		/* take the signal! */
	}
	/* NOTREACHED */
}

/*
 * Put the argument process into the stopped
 * state and notify the parent via wakeup.
 * Signals are handled elsewhere.
 * The process must not be on the run queue.
 */
static void
stop(p)
	register struct proc *p;
{

	p->p_stat = SSTOP;
	p->p_flag &= ~SWTED;
	wakeup((caddr_t)p->p_pptr);
}

/*
 * Take the action for the specified signal
 * from the current set of pending signals.
 */
void
psig(sig)
	register int sig;
{
	register struct proc *p = curproc;
	register struct sigacts *ps = p->p_sigacts;
	register sig_t action;
	int mask, returnmask;

#ifdef DIAGNOSTIC
	if (sig == 0)
		panic("psig");
#endif
	mask = sigmask(sig);
	p->p_sig &= ~mask;
	action = ps->ps_sigact[sig];
#ifdef KTRACE
	if (KTRPOINT(p, KTR_PSIG))
		ktrpsig(p->p_tracep, sig, action, ps->ps_flags & SA_OLDMASK ?
		    ps->ps_oldmask : p->p_sigmask, 0);
#endif
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
#ifdef DIAGNOSTIC
		if (action == SIG_IGN || (p->p_sigmask & mask))
			panic("psig action");
#endif
		/*
		 * Set the new mask value and also defer further
		 * occurences of this signal.
		 *
		 * Special case: user has done a sigpause.  Here the
		 * current mask is not of interest, but rather the
		 * mask from before the sigpause is what we want
		 * restored after the signal processing is completed.
		 */
		(void) splhigh();
		if (ps->ps_flags & SA_OLDMASK) {
			returnmask = ps->ps_oldmask;
			ps->ps_flags &= ~SA_OLDMASK;
		} else
			returnmask = p->p_sigmask;
		p->p_sigmask |= ps->ps_catchmask[sig] | mask;
		(void) spl0();
		p->p_stats->p_ru.ru_nsignals++;
		sendsig(action, sig, returnmask, 0);
	}
}

/*
 * Force the current process to exit with the specified
 * signal, dumping core if appropriate.  We bypass the normal
 * tests for masked and caught signals, allowing unrecoverable
 * failures to terminate the process without changing signal state.
 * Mark the accounting record with the signal termination.
 * If dumping core, save the signal number for the debugger.
 * Calls exit and does not return.
 */
static void
sigexit(p, sig)
	register struct proc *p;
	int sig;
{

	p->p_acflag |= AXSIG;
	if (sigprop[sig] & SA_CORE) {
		p->p_sigacts->ps_sig = sig;
		/*
		 * Log signals which would cause core dumps
		 * (Log as LOG_INFO to appease those who don't want 
		 * these messages.)
		 * XXX : Todo, as well as euid, write out ruid too
		 */
		log(LOG_INFO, "pid %d: %s: uid %d: exited on signal %d\n",
			p->p_pid, p->p_comm, p->p_ucred->cr_uid, sig);
		if (coredump(p) == 0)
			sig |= WCOREFLAG;
	}
	kexit(p, W_EXITCODE(0, sig));
	/* NOTREACHED */
}

/*
 * Create a core dump.
 * The file name is "progname.core".
 * Core dumps are not created if:
 *	the process is setuid,
 *	we are on a filesystem mounted with MNT_NOCORE,
 *	a file already exists and is not a core file,
 *		or was not produced from the same program,
 *	the link count to the corefile is > 1.
 */
int
coredump(p)
	register struct proc *p;
{
	register struct vnode *vp;
	register struct pcred *pcred = p->p_cred;
	register struct ucred *cred = pcred->pc_ucred;
	register struct vmspace *vm = p->p_vmspace;
	struct vattr vattr;
	int error, error1, exists;
	struct nameidata nd;
	char name[MAXCOMLEN+6];	/* progname.core */

	if (pcred->p_svuid != pcred->p_ruid ||
	    pcred->p_svgid != pcred->p_rgid)
		return (EFAULT);
	if (ctob(UPAGES + vm->vm_dsize + vm->vm_ssize) >=
	    p->p_rlimit[RLIMIT_CORE].rlim_cur)
		return (EFAULT);
	if (p->p_fd->fd_cdir->v_mount->mnt_flag & MNT_NOCORE)
		return (EFAULT);

	sprintf(name, "%s.core", p->p_comm);
	nd.ni_dirp = name;
	nd.ni_segflg = UIO_SYSSPACE;
	if ((error = vn_open(&nd, p, FWRITE, 0600)) == 0)
		exists = 1;
	else
		exists = 0;
	if (error == ENOENT)
		error = vn_open(&nd, p, O_CREAT | FWRITE, 0600);
	if (error)
		return (error);
	vp = nd.ni_vp;
	if (vp->v_type != VREG || VOP_GETATTR(vp, &vattr, cred, p) ||
	    vattr.va_nlink != 1) {
		error = EFAULT;
		goto out;
	}
	if (exists) {	/* if file already exists, look if it's a coredump */
	    struct user	userbuf;	/* XXX */
	    error = vn_rdwr(UIO_READ, vp, (caddr_t)&userbuf, sizeof(userbuf),
		(off_t)0, UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred,
		(int *)NULL, p);
	    if (error || (vattr.va_size != ctob(UPAGES + 
			userbuf.u_kproc.kp_eproc.e_vm.vm_dsize +
			userbuf.u_kproc.kp_eproc.e_vm.vm_ssize)) ||
			strcmp(p->p_comm, userbuf.u_kproc.kp_proc.p_comm)) {
		error = EFAULT;
		goto out;
		}
	}
	VATTR_NULL(&vattr);
	vattr.va_size = 0;
	VOP_SETATTR(vp, &vattr, cred, p);
	p->p_acflag |= ACORE;
	bcopy(p, &p->p_addr->u_kproc.kp_proc, sizeof(struct proc));
	fill_eproc(p, &p->p_addr->u_kproc.kp_eproc);
#ifdef HPUXCOMPAT
	/*
	 * BLETCH!  If we loaded from an HPUX format binary file
	 * we have to dump an HPUX style user struct so that the
	 * HPUX debuggers can grok it.
	 */
	if (p->p_addr->u_pcb.pcb_flags & PCB_HPUXBIN)
		error = hpuxdumpu(vp, cred);
	else
#endif
	error = vn_rdwr(UIO_WRITE, vp, (caddr_t) p->p_addr, ctob(UPAGES),
	    (off_t)0, UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, (int *) NULL,
	    p);
	if (error == 0)
		error = vn_rdwr(UIO_WRITE, vp, vm->vm_daddr,
		    (int)ctob(vm->vm_dsize), (off_t)ctob(UPAGES), UIO_USERSPACE,
		    IO_NODELOCKED|IO_UNIT, cred, (int *) NULL, p);
	if (error == 0)
		error = vn_rdwr(UIO_WRITE, vp,
		    (caddr_t) trunc_page(vm->vm_maxsaddr + MAXSSIZ
			- ctob(vm->vm_ssize)),
		    round_page(ctob(vm->vm_ssize)),
		    (off_t)ctob(UPAGES) + ctob(vm->vm_dsize), UIO_USERSPACE,
		    IO_NODELOCKED|IO_UNIT, cred, (int *) NULL, p);
out:
	VOP_UNLOCK(vp);
	error1 = vn_close(vp, FWRITE, cred, p);
	if (error == 0)
		error = error1;
	return (error);
}

/*
 * Nonexistent system call-- signal process (may want to handle it).
 * Flag error in case process won't see signal immediately (blocked or ignored).
 */
/* ARGSUSED */
int
nosys(p, args, retval)
	struct proc *p;
	void *args;
	int *retval;
{

	psignal(p, SIGSYS);
	return (EINVAL);
}
