/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)signalvar.h	8.6 (Berkeley) 2/19/95
 * $FreeBSD: src/sys/sys/signalvar.h,v 1.62 2003/05/14 15:00:24 jhb Exp $
 */

#ifndef _SYS_SIGNALVAR_H_
#define	_SYS_SIGNALVAR_H_

#include <sys/queue.h>
#include <sys/_lock.h>
#include <sys/_mutex.h>
#include <sys/signal.h>

/*
 * Kernel signal definitions and data structures,
 * not exported to user programs.
 */

/*
 * Logical process signal actions and state, needed only within the process
 * The mapping between sigacts and proc structures is 1:1 except for rfork()
 * processes masquerading as threads which use one structure for the whole
 * group.  All members are locked by the included mutex.  The reference count
 * and mutex must be last for the bcopy in sigacts_copy() to work.
 */
struct sigacts {
	sig_t	ps_sigact[_SIG_MAXSIG];	/* Disposition of signals. */
	sigset_t ps_catchmask[_SIG_MAXSIG];	/* Signals to be blocked. */
	sigset_t ps_sigonstack;		/* Signals to take on sigstack. */
	sigset_t ps_sigintr;		/* Signals that interrupt syscalls. */
	sigset_t ps_sigreset;		/* Signals that reset when caught. */
	sigset_t ps_signodefer;		/* Signals not masked while handled. */
	sigset_t ps_siginfo;		/* Signals that want SA_SIGINFO args. */
	sigset_t ps_sigignore;		/* Signals being ignored. */
	sigset_t ps_sigcatch;		/* Signals being caught by user. */
	sigset_t ps_freebsd4;		/* signals using freebsd4 ucontext. */
	sigset_t ps_osigset;		/* Signals using <= 3.x osigset_t. */
	sigset_t ps_usertramp;		/* SunOS compat; libc sigtramp. XXX */
	int	ps_flag;
	int	ps_refcnt;
	struct mtx ps_mtx;
};

#define	PS_NOCLDWAIT	0x0001	/* No zombies if child dies */
#define	PS_NOCLDSTOP	0x0002	/* No SIGCHLD when children stop. */
#define	PS_CLDSIGIGN	0x0004	/* The SIGCHLD handler is SIG_IGN. */

#if defined(_KERNEL) && defined(COMPAT_43)
/*
 * Compatibility.
 */
typedef struct {
	struct osigcontext si_sc;
	int		si_signo;
	int		si_code;
	union sigval	si_value;
} osiginfo_t;

struct osigaction {
	union {
		void    (*__sa_handler)(int);
		void    (*__sa_sigaction)(int, osiginfo_t *, void *);
	} __sigaction_u;		/* signal handler */
	osigset_t	sa_mask;	/* signal mask to apply */
	int		sa_flags;	/* see signal options below */
};

typedef void __osiginfohandler_t(int, osiginfo_t *, void *);
#endif /* _KERNEL && COMPAT_43 */

/* additional signal action values, used only temporarily/internally */
#define	SIG_CATCH	((__sighandler_t *)2)
#define SIG_HOLD        ((__sighandler_t *)3)

/*
 * get signal action for process and signal; currently only for current process
 */
#define SIGACTION(p, sig)	(p->p_sigacts->ps_sigact[_SIG_IDX(sig)])

/*
 * sigset_t manipulation macros
 */
#define SIGADDSET(set, signo)						\
	((set).__bits[_SIG_WORD(signo)] |= _SIG_BIT(signo))

#define SIGDELSET(set, signo)						\
	((set).__bits[_SIG_WORD(signo)] &= ~_SIG_BIT(signo))

#define SIGEMPTYSET(set)						\
	do {								\
		int __i;						\
		for (__i = 0; __i < _SIG_WORDS; __i++)			\
			(set).__bits[__i] = 0;				\
	} while (0)

#define SIGFILLSET(set)							\
	do {								\
		int __i;						\
		for (__i = 0; __i < _SIG_WORDS; __i++)			\
			(set).__bits[__i] = ~0U;			\
	} while (0)

#define SIGISMEMBER(set, signo)						\
	((set).__bits[_SIG_WORD(signo)] & _SIG_BIT(signo))

#define SIGISEMPTY(set)		(__sigisempty(&(set)))
#define SIGNOTEMPTY(set)	(!__sigisempty(&(set)))

#define SIGSETEQ(set1, set2)	(__sigseteq(&(set1), &(set2)))
#define SIGSETNEQ(set1, set2)	(!__sigseteq(&(set1), &(set2)))

#define SIGSETOR(set1, set2)						\
	do {								\
		int __i;						\
		for (__i = 0; __i < _SIG_WORDS; __i++)			\
			(set1).__bits[__i] |= (set2).__bits[__i];	\
	} while (0)

#define SIGSETAND(set1, set2)						\
	do {								\
		int __i;						\
		for (__i = 0; __i < _SIG_WORDS; __i++)			\
			(set1).__bits[__i] &= (set2).__bits[__i];	\
	} while (0)

#define SIGSETNAND(set1, set2)						\
	do {								\
		int __i;						\
		for (__i = 0; __i < _SIG_WORDS; __i++)			\
			(set1).__bits[__i] &= ~(set2).__bits[__i];	\
	} while (0)

#define SIGSETLO(set1, set2)	((set1).__bits[0] = (set2).__bits[0])
#define SIGSETOLD(set, oset)	((set).__bits[0] = (oset))

#define SIG_CANTMASK(set)						\
	SIGDELSET(set, SIGKILL), SIGDELSET(set, SIGSTOP)

#define SIG_STOPSIGMASK(set)						\
	SIGDELSET(set, SIGSTOP), SIGDELSET(set, SIGTSTP),		\
	SIGDELSET(set, SIGTTIN), SIGDELSET(set, SIGTTOU)

#define SIG_CONTSIGMASK(set)						\
	SIGDELSET(set, SIGCONT)

#define sigcantmask	(sigmask(SIGKILL) | sigmask(SIGSTOP))

#define SIG2OSIG(sig, osig)	(osig = (sig).__bits[0])
#define OSIG2SIG(osig, sig)	SIGEMPTYSET(sig); (sig).__bits[0] = osig

static __inline int
__sigisempty(sigset_t *set)
{
	int i;

	for (i = 0; i < _SIG_WORDS; i++) {
		if (set->__bits[i])
			return (0);
	}
	return (1);
}

static __inline int
__sigseteq(sigset_t *set1, sigset_t *set2)
{
	int i;

	for (i = 0; i < _SIG_WORDS; i++) {
		if (set1->__bits[i] != set2->__bits[i])
			return (0);
	}
	return (1);
}

#ifdef _KERNEL

/* Return nonzero if process p has an unmasked pending signal. */
#define	SIGPENDING(td)							\
	(!SIGISEMPTY((td)->td_siglist) &&				\
	    (!sigsetmasked(&(td)->td_siglist, &(td)->td_sigmask) ||	\
	    (td)->td_proc->p_flag & P_TRACED))

/*
 * Return the value of the pseudo-expression ((*set & ~*mask) != 0).  This
 * is an optimized version of SIGISEMPTY() on a temporary variable
 * containing SIGSETNAND(*set, *mask).
 */
static __inline int
sigsetmasked(sigset_t *set, sigset_t *mask)
{
	int i;

	for (i = 0; i < _SIG_WORDS; i++) {
		if (set->__bits[i] & ~mask->__bits[i])
			return (0);
	}
	return (1);
}

struct pgrp;
struct thread;
struct proc;
struct sigio;
struct mtx;

extern int sugid_coredump;	/* Sysctl variable kern.sugid_coredump */
extern struct mtx	sigio_lock;

/*
 * Lock the pointers for a sigio object in the underlying objects of
 * a file descriptor.
 */
#define SIGIO_LOCK()	mtx_lock(&sigio_lock)
#define SIGIO_TRYLOCK()	mtx_trylock(&sigio_lock)
#define SIGIO_UNLOCK()	mtx_unlock(&sigio_lock)
#define SIGIO_LOCKED()	mtx_owned(&sigio_lock)
#define SIGIO_ASSERT(type)	mtx_assert(&sigio_lock, type)

/*
 * Machine-independent functions:
 */
int	cursig(struct thread *td);
void	execsigs(struct proc *p);
void	gsignal(int pgid, int sig);
void	killproc(struct proc *p, char *why);
void	pgsigio(struct sigio **, int signum, int checkctty);
void	pgsignal(struct pgrp *pgrp, int sig, int checkctty);
void	postsig(int sig);
void	psignal(struct proc *p, int sig);
struct sigacts *sigacts_alloc(void);
void	sigacts_copy(struct sigacts *dest, struct sigacts *src);
void	sigacts_free(struct sigacts *ps);
struct sigacts *sigacts_hold(struct sigacts *ps);
int	sigacts_shared(struct sigacts *ps);
void	sigexit(struct thread *td, int signum) __dead2;
int	sig_ffs(sigset_t *set);
void	siginit(struct proc *p);
void	signotify(struct thread *td);
void	tdsignal(struct thread *td, int sig);
void	trapsignal(struct thread *td, int sig, u_long code);

/*
 * Machine-dependent functions:
 */
void	sendsig(sig_t action, int sig, sigset_t *retmask, u_long code);

#endif /* _KERNEL */

#endif /* !_SYS_SIGNALVAR_H_ */
