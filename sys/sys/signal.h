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
 *	from: @(#)signal.h	7.16 (Berkeley) 3/17/91
 *	$Id: signal.h,v 1.3 1993/10/16 17:17:39 rgrimes Exp $
 */

#ifndef	_SIGNAL_H_
#define	_SIGNAL_H_

#define NSIG	32		/* counting 0; could be 33 (mask is 1-32) */

#ifndef _POSIX_SOURCE
#include <machine/trap.h>	/* codes for SIGILL, SIGFPE */
#endif /* _POSIX_SOURCE */

#define	SIGHUP	1	/* hangup */
#define	SIGINT	2	/* interrupt */
#define	SIGQUIT	3	/* quit */
#define	SIGILL	4	/* illegal instruction (not reset when caught) */
#ifndef _POSIX_SOURCE
#define	SIGTRAP	5	/* trace trap (not reset when caught) */
#endif
#define	SIGABRT	6	/* abort() */
#ifndef _POSIX_SOURCE
#define	SIGIOT	SIGABRT	/* compatibility */
#define	SIGEMT	7	/* EMT instruction */
#endif
#define	SIGFPE	8	/* floating point exception */
#define	SIGKILL	9	/* kill (cannot be caught or ignored) */
#ifndef _POSIX_SOURCE
#define	SIGBUS	10	/* bus error */
#endif
#define	SIGSEGV	11	/* segmentation violation */
#ifndef _POSIX_SOURCE
#define	SIGSYS	12	/* bad argument to system call */
#endif
#define	SIGPIPE	13	/* write on a pipe with no one to read it */
#define	SIGALRM	14	/* alarm clock */
#define	SIGTERM	15	/* software termination signal from kill */
#ifndef _POSIX_SOURCE
#define	SIGURG	16	/* urgent condition on IO channel */
#endif
#define	SIGSTOP	17	/* sendable stop signal not from tty */
#define	SIGTSTP	18	/* stop signal from tty */
#define	SIGCONT	19	/* continue a stopped process */
#define	SIGCHLD	20	/* to parent on child stop or exit */
#define	SIGTTIN	21	/* to readers pgrp upon background tty read */
#define	SIGTTOU	22	/* like TTIN for output if (tp->t_local&LTOSTOP) */
#ifndef _POSIX_SOURCE
#define	SIGIO	23	/* input/output possible signal */
#define	SIGXCPU	24	/* exceeded CPU time limit */
#define	SIGXFSZ	25	/* exceeded file size limit */
#define	SIGVTALRM 26	/* virtual time alarm */
#define	SIGPROF	27	/* profiling time alarm */
#define SIGWINCH 28	/* window size changes */
#define SIGINFO	29	/* information request */
#endif
#define SIGUSR1 30	/* user defined signal 1 */
#define SIGUSR2 31	/* user defined signal 2 */

#include <sys/cdefs.h>

#ifndef _POSIX_SOURCE
typedef	void (*sig_t) __P((int));
#endif

typedef void (*__sighandler_t) __P((int));
typedef unsigned int sigset_t;

__BEGIN_DECLS
int	sigaddset __P((sigset_t *, int));
int	sigdelset __P((sigset_t *, int));
int	sigemptyset __P((sigset_t *));
int	sigfillset __P((sigset_t *));
int	sigismember __P((const sigset_t *, int));
__END_DECLS

#define sigemptyset(set)	( *(set) = 0 )
#define sigfillset(set)		( *(set) = ~(sigset_t)0, 0 )
#define sigaddset(set, signo)	( *(set) |= 1 << ((signo) - 1), 0)
#define sigdelset(set, signo)	( *(set) &= ~(1 << ((signo) - 1)), 0)
#define sigismember(set, signo)	( (*(set) & (1 << ((signo) - 1))) != 0)

/*
 * Signal vector "template" used in sigaction call.
 */
struct	sigaction {
	__sighandler_t  sa_handler;     /* signal handler */
	sigset_t sa_mask;		/* signal mask to apply */
	int	sa_flags;		/* see signal options below */
};
#ifndef _POSIX_SOURCE
#define SA_ONSTACK	0x0001	/* take signal on signal stack */
#define SA_RESTART	0x0002	/* do not restart system on signal return */
#endif
#define SA_NOCLDSTOP	0x0004	/* do not generate SIGCHLD on child stop */

/*
 * Flags for sigprocmask:
 */
#define	SIG_BLOCK	1	/* block specified signal set */
#define	SIG_UNBLOCK	2	/* unblock specified signal set */
#define	SIG_SETMASK	3	/* set specified signal set */

#ifndef _POSIX_SOURCE
/*
 * 4.3 compatibility:
 * Signal vector "template" used in sigvec call.
 */
struct	sigvec {
	void	(*sv_handler)();	/* signal handler */
	int	sv_mask;		/* signal mask to apply */
	int	sv_flags;		/* see signal options below */
};
#define SV_ONSTACK	SA_ONSTACK
#define SV_INTERRUPT	SA_RESTART	/* same bit, opposite sense */
#define sv_onstack sv_flags	/* isn't compatibility wonderful! */

/*
 * Structure used in sigaltstack call.
 */
struct	sigaltstack {
	char	*ss_base;		/* signal stack base */
	int	ss_len;			/* signal stack length */
	int	ss_onstack;		/* current status */
};

/*
 * Structure used in sigstack call.
 */
struct	sigstack {
	char	*ss_sp;			/* signal stack pointer */
	int	ss_onstack;		/* current status */
};

/*
 * Information pushed on stack when a signal is delivered.
 * This is used by the kernel to restore state following
 * execution of the signal handler.  It is also made available
 * to the handler to allow it to restore state properly if
 * a non-standard exit is performed.
 */
struct	sigcontext {
	int	sc_onstack;		/* sigstack state to restore */
	int	sc_mask;		/* signal mask to restore */
	int	sc_sp;			/* sp to restore */
	int	sc_fp;			/* fp to restore */
	int	sc_ap;			/* ap to restore */
	int	sc_pc;			/* pc to restore */
	int	sc_ps;			/* psl to restore */
};

/*
 * Macro for converting signal number to a mask suitable for
 * sigblock().
 */
#define sigmask(m)	(1 << ((m)-1))

#define	SIG_ERR		((__sighandler_t) -1)
#endif	/* _POSIX_SOURCE */
  
#define	SIG_DFL		((__sighandler_t) 0)
#define	SIG_IGN		((__sighandler_t) 1)

#ifndef KERNEL
#include <sys/types.h>

__BEGIN_DECLS
/*void	(*signal __P((int, void (*) __P((int))))) __P((int));*/
__sighandler_t signal __P((int, __sighandler_t));
int	raise __P((int));
#ifndef	_ANSI_SOURCE
int	kill __P((pid_t, int));
int	sigaction __P((int, const struct sigaction *, struct sigaction *));
int	sigpending __P((sigset_t *));
int	sigprocmask __P((int, const sigset_t *, sigset_t *));
int	sigsuspend __P((const sigset_t *));
#endif	/* !_ANSI_SOURCE */
#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
int	killpg __P((pid_t, int));
void	psignal __P((unsigned, const char *));
int	sigblock __P((int));
int	siginterrupt __P((int, int));
int	sigpause __P((int));
int	sigreturn __P((struct sigcontext *));
int	sigsetmask __P((int));
int	sigstack __P((const struct sigstack *, struct sigstack *));
int	sigvec __P((int, struct sigvec *, struct sigvec *));
#endif /* !_ANSI_SOURCE && !_POSIX_SOURCE */
__END_DECLS

#endif	/* !KERNEL */
#endif	/* !_SIGNAL_H_ */
