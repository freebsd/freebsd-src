/*-
 * Copyright (c) 1995 Scott Bartram
 * Copyright (c) 1995 Steven Wallace
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/signalvar.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>

#include <i386/ibcs2/ibcs2_types.h>
#include <i386/ibcs2/ibcs2_signal.h>
#include <i386/ibcs2/ibcs2_proto.h>
#include <i386/ibcs2/ibcs2_xenix.h>
#include <i386/ibcs2/ibcs2_util.h>

#define sigemptyset(s)		SIGEMPTYSET(*(s))
#define sigismember(s, n)	SIGISMEMBER(*(s), n)
#define sigaddset(s, n)		SIGADDSET(*(s), n)

#define	ibcs2_sigmask(n)	(1 << ((n) - 1))
#define ibcs2_sigemptyset(s)	bzero((s), sizeof(*(s)))
#define ibcs2_sigismember(s, n)	(*(s) & ibcs2_sigmask(n))
#define ibcs2_sigaddset(s, n)	(*(s) |= ibcs2_sigmask(n))

static void ibcs2_to_bsd_sigset(const ibcs2_sigset_t *, sigset_t *);
static void bsd_to_ibcs2_sigset(const sigset_t *, ibcs2_sigset_t *);
static void ibcs2_to_bsd_sigaction(struct ibcs2_sigaction *,
					struct sigaction *);
static void bsd_to_ibcs2_sigaction(struct sigaction *,
					struct ibcs2_sigaction *);

int bsd_to_ibcs2_sig[IBCS2_SIGTBLSZ] = {
	IBCS2_SIGHUP,		/* 1 */
	IBCS2_SIGINT,		/* 2 */
	IBCS2_SIGQUIT,		/* 3 */
	IBCS2_SIGILL,		/* 4 */
	IBCS2_SIGTRAP,		/* 5 */
	IBCS2_SIGABRT,		/* 6 */
	IBCS2_SIGEMT,		/* 7 */
	IBCS2_SIGFPE,		/* 8 */
	IBCS2_SIGKILL,		/* 9 */
	IBCS2_SIGBUS,		/* 10 */
	IBCS2_SIGSEGV,		/* 11 */
	IBCS2_SIGSYS,		/* 12 */
	IBCS2_SIGPIPE,		/* 13 */
	IBCS2_SIGALRM,		/* 14 */
	IBCS2_SIGTERM,		/* 15 */
	0,			/* 16 - SIGURG */
	IBCS2_SIGSTOP,		/* 17 */
	IBCS2_SIGTSTP,		/* 18 */
	IBCS2_SIGCONT,		/* 19 */
	IBCS2_SIGCLD,		/* 20 */
	IBCS2_SIGTTIN,		/* 21 */
	IBCS2_SIGTTOU,		/* 22 */
	IBCS2_SIGPOLL,		/* 23 */
	0,			/* 24 - SIGXCPU */
	0,			/* 25 - SIGXFSZ */
	IBCS2_SIGVTALRM,	/* 26 */
	IBCS2_SIGPROF,		/* 27 */
	IBCS2_SIGWINCH,		/* 28 */
	0,			/* 29 */
	IBCS2_SIGUSR1,		/* 30 */
	IBCS2_SIGUSR2,		/* 31 */
	0			/* 32 */
};

static int ibcs2_to_bsd_sig[IBCS2_SIGTBLSZ] = {
	SIGHUP,			/* 1 */
	SIGINT,			/* 2 */
	SIGQUIT,		/* 3 */
	SIGILL,			/* 4 */
	SIGTRAP,		/* 5 */
	SIGABRT,		/* 6 */
	SIGEMT,			/* 7 */
	SIGFPE,			/* 8 */
	SIGKILL,		/* 9 */
	SIGBUS,			/* 10 */
	SIGSEGV,		/* 11 */
	SIGSYS,			/* 12 */
	SIGPIPE,		/* 13 */
	SIGALRM,		/* 14 */
	SIGTERM,		/* 15 */
	SIGUSR1,		/* 16 */
	SIGUSR2,		/* 17 */
	SIGCHLD,		/* 18 */
	0,			/* 19 - SIGPWR */
	SIGWINCH,		/* 20 */
	0,			/* 21 */
	SIGIO,			/* 22 */
	SIGSTOP,		/* 23 */
	SIGTSTP,		/* 24 */
	SIGCONT,		/* 25 */
	SIGTTIN,		/* 26 */
	SIGTTOU,		/* 27 */
	SIGVTALRM,		/* 28 */
	SIGPROF,		/* 29 */
	0,			/* 30 */
	0,			/* 31 */
	0			/* 32 */
};

void
ibcs2_to_bsd_sigset(iss, bss)
	const ibcs2_sigset_t *iss;
	sigset_t *bss;
{
	int i, newsig;

	sigemptyset(bss);
	for (i = 1; i <= IBCS2_SIGTBLSZ; i++) {
		if (ibcs2_sigismember(iss, i)) {
			newsig = ibcs2_to_bsd_sig[_SIG_IDX(i)];
			if (newsig)
				sigaddset(bss, newsig);
		}
	}
}

static void
bsd_to_ibcs2_sigset(bss, iss)
	const sigset_t *bss;
	ibcs2_sigset_t *iss;
{
	int i, newsig;

	ibcs2_sigemptyset(iss);
	for (i = 1; i <= IBCS2_SIGTBLSZ; i++) {
		if (sigismember(bss, i)) {
			newsig = bsd_to_ibcs2_sig[_SIG_IDX(i)];
			if (newsig)
				ibcs2_sigaddset(iss, newsig);
		}
	}
}

static void
ibcs2_to_bsd_sigaction(isa, bsa)
	struct ibcs2_sigaction *isa;
	struct sigaction *bsa;
{

	bsa->sa_handler = isa->isa_handler;
	ibcs2_to_bsd_sigset(&isa->isa_mask, &bsa->sa_mask);
	bsa->sa_flags = 0;	/* ??? SA_NODEFER */
	if ((isa->isa_flags & IBCS2_SA_NOCLDSTOP) != 0)
		bsa->sa_flags |= SA_NOCLDSTOP;
}

static void
bsd_to_ibcs2_sigaction(bsa, isa)
	struct sigaction *bsa;
	struct ibcs2_sigaction *isa;
{

	isa->isa_handler = bsa->sa_handler;
	bsd_to_ibcs2_sigset(&bsa->sa_mask, &isa->isa_mask);
	isa->isa_flags = 0;
	if ((bsa->sa_flags & SA_NOCLDSTOP) != 0)
		isa->isa_flags |= IBCS2_SA_NOCLDSTOP;
}

int
ibcs2_sigaction(td, uap)
	register struct thread *td;
	struct ibcs2_sigaction_args *uap;
{
	struct ibcs2_sigaction isa;
	struct sigaction nbsa, obsa;
	struct sigaction *nbsap;
 	int error;

	if (uap->act != NULL) {
		if ((error = copyin(uap->act, &isa, sizeof(isa))) != 0)
			return (error);
		ibcs2_to_bsd_sigaction(&isa, &nbsa);
		nbsap = &nbsa;
	} else
		nbsap = NULL;
	if (uap->sig <= 0 || uap->sig > IBCS2_NSIG)
		return (EINVAL);
	error = kern_sigaction(td, ibcs2_to_bsd_sig[_SIG_IDX(uap->sig)], &nbsa,
	    &obsa, 0);
	if (error == 0 && uap->oact != NULL) {
		bsd_to_ibcs2_sigaction(&obsa, &isa);
		error = copyout(&isa, uap->oact, sizeof(isa));
	}
	return (error);
}

int
ibcs2_sigsys(td, uap)
	register struct thread *td;
	struct ibcs2_sigsys_args *uap;
{
	struct proc *p = td->td_proc;
	struct sigaction sa;
	int signum = IBCS2_SIGNO(uap->sig);
	int error;

	if (signum <= 0 || signum > IBCS2_NSIG) {
		if (IBCS2_SIGCALL(uap->sig) == IBCS2_SIGNAL_MASK ||
		    IBCS2_SIGCALL(uap->sig) == IBCS2_SIGSET_MASK)
			td->td_retval[0] = (int)IBCS2_SIG_ERR;
		return EINVAL;
	}
	signum = ibcs2_to_bsd_sig[_SIG_IDX(signum)];
	
	switch (IBCS2_SIGCALL(uap->sig)) {
	case IBCS2_SIGSET_MASK:
		/*
		 * Check for SIG_HOLD action.
		 * Otherwise, perform signal() except with different sa_flags.
		 */
		if (uap->fp != IBCS2_SIG_HOLD) {
			/* add sig to mask before exececuting signal handler */
			sa.sa_flags = 0;
			goto ibcs2_sigset;
		}
		/* else FALLTHROUGH to sighold */

	case IBCS2_SIGHOLD_MASK:
		{
			sigset_t mask;

			SIGEMPTYSET(mask);
			SIGADDSET(mask, signum);
			return (kern_sigprocmask(td, SIG_BLOCK, &mask, NULL,
				    0));
		}
		
	case IBCS2_SIGNAL_MASK:
		{
			struct sigaction osa;

			/* do not automatically block signal */
			sa.sa_flags = SA_NODEFER;
#ifdef SA_RESETHAND
			if((signum != IBCS2_SIGILL) &&
			   (signum != IBCS2_SIGTRAP) &&
			   (signum != IBCS2_SIGPWR))
				/* set to SIG_DFL before executing handler */
				sa.sa_flags |= SA_RESETHAND;
#endif
		ibcs2_sigset:
			sa.sa_handler = uap->fp;
			sigemptyset(&sa.sa_mask);
#if 0
			if (signum != SIGALRM)
				sa.sa_flags |= SA_RESTART;
#endif
			error = kern_sigaction(td, signum, &sa, &osa, 0);
			if (error != 0) {
				DPRINTF(("signal: sigaction failed: %d\n",
					 error));
				td->td_retval[0] = (int)IBCS2_SIG_ERR;
				return (error);
			}
			td->td_retval[0] = (int)osa.sa_handler;

			/* special sigset() check */
                        if(IBCS2_SIGCALL(uap->sig) == IBCS2_SIGSET_MASK) {
				PROC_LOCK(p);
			        /* check to make sure signal is not blocked */
                                if(sigismember(&td->td_sigmask, signum)) {
				        /* return SIG_HOLD and unblock signal*/
                                        td->td_retval[0] = (int)IBCS2_SIG_HOLD;
					SIGDELSET(td->td_sigmask, signum);
					signotify(td);
				}
				PROC_UNLOCK(p);
			}
				
			return 0;
		}
		
	case IBCS2_SIGRELSE_MASK:
		{
			sigset_t mask;

			SIGEMPTYSET(mask);
			SIGADDSET(mask, signum);
			return (kern_sigprocmask(td, SIG_UNBLOCK, &mask, NULL,
				    0));
		}
		
	case IBCS2_SIGIGNORE_MASK:
		{
			sa.sa_handler = SIG_IGN;
			sigemptyset(&sa.sa_mask);
			sa.sa_flags = 0;
			error = kern_sigaction(td, signum, &sa, NULL, 0);
			if (error != 0)
				DPRINTF(("sigignore: sigaction failed\n"));
			return (error);
		}
		
	case IBCS2_SIGPAUSE_MASK:
		{
			sigset_t mask;

			PROC_LOCK(p);
			mask = td->td_sigmask;
			PROC_UNLOCK(p);
			SIGDELSET(mask, signum);
			return kern_sigsuspend(td, mask);
		}
		
	default:
		return ENOSYS;
	}
}

int
ibcs2_sigprocmask(td, uap)
	register struct thread *td;
	struct ibcs2_sigprocmask_args *uap;
{
	ibcs2_sigset_t iss;
	sigset_t oss, nss;
	sigset_t *nssp;
	int error, how;

	switch (uap->how) {
	case IBCS2_SIG_BLOCK:
		how = SIG_BLOCK;
		break;
	case IBCS2_SIG_UNBLOCK:
		how = SIG_UNBLOCK;
		break;
	case IBCS2_SIG_SETMASK:
		how = SIG_SETMASK;
		break;
	default:
		return (EINVAL);
	}
	if (uap->set != NULL) {
		if ((error = copyin(uap->set, &iss, sizeof(iss))) != 0)
			return error;
		ibcs2_to_bsd_sigset(&iss, &nss);
		nssp = &nss;
	} else
		nssp = NULL;
	error = kern_sigprocmask(td, how, nssp, &oss, 0);
	if (error == 0 && uap->oset != NULL) {
		bsd_to_ibcs2_sigset(&oss, &iss);
		error = copyout(&iss, uap->oset, sizeof(iss));
	}
	return (error);
}

int
ibcs2_sigpending(td, uap)
	register struct thread *td;
	struct ibcs2_sigpending_args *uap;
{
	struct proc *p = td->td_proc;
	sigset_t bss;
	ibcs2_sigset_t iss;

	PROC_LOCK(p);
	bss = td->td_siglist;
	SIGSETOR(bss, p->p_siglist);
	SIGSETAND(bss, td->td_sigmask);
	PROC_UNLOCK(p);
	bsd_to_ibcs2_sigset(&bss, &iss);

	return copyout(&iss, uap->mask, sizeof(iss));
}

int
ibcs2_sigsuspend(td, uap)
	register struct thread *td;
	struct ibcs2_sigsuspend_args *uap;
{
	ibcs2_sigset_t sss;
	sigset_t bss;
	int error;

	if ((error = copyin(uap->mask, &sss, sizeof(sss))) != 0)
		return error;

	ibcs2_to_bsd_sigset(&sss, &bss);
	return kern_sigsuspend(td, bss);
}

int
ibcs2_pause(td, uap)
	register struct thread *td;
	struct ibcs2_pause_args *uap;
{
	sigset_t mask;

	PROC_LOCK(td->td_proc);
	mask = td->td_sigmask;
	PROC_UNLOCK(td->td_proc);
	return kern_sigsuspend(td, mask);
}

int
ibcs2_kill(td, uap)
	register struct thread *td;
	struct ibcs2_kill_args *uap;
{
	struct kill_args ka;

	if (uap->signo <= 0 || uap->signo > IBCS2_NSIG)
		return (EINVAL);
	ka.pid = uap->pid;
	ka.signum = ibcs2_to_bsd_sig[_SIG_IDX(uap->signo)];
	return sys_kill(td, &ka);
}
