/*-
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
 *	@(#)signal.h	8.3 (Berkeley) 3/30/94
 *
 * $FreeBSD: src/include/signal.h,v 1.14 1999/10/02 19:33:23 marcel Exp $
 */

#ifndef _SIGNAL_H_
#define	_SIGNAL_H_

#include <sys/cdefs.h>
#include <sys/_posix.h>
#include <machine/ansi.h>
#include <sys/signal.h>
#include <sys/time.h>

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
extern __const char *__const sys_signame[NSIG];
extern __const char *__const sys_siglist[NSIG];
extern __const int sys_nsig;
#endif

__BEGIN_DECLS
int	raise __P((int));
#ifndef	_ANSI_SOURCE
int	kill __P((_BSD_PID_T_, int));
int	sigaction __P((int, const struct sigaction *, struct sigaction *));
int	sigaddset __P((sigset_t *, int));
int	sigdelset __P((sigset_t *, int));
int	sigemptyset __P((sigset_t *));
int	sigfillset __P((sigset_t *));
int	sigismember __P((const sigset_t *, int));
int	sigpending __P((sigset_t *));
int	sigprocmask __P((int, const sigset_t *, sigset_t *));
int	sigsuspend __P((const sigset_t *));
int	sigwait __P((const sigset_t *, int *));


#ifdef _P1003_1B_VISIBLE

__BEGIN_DECLS
int sigqueue __P((_BSD_PID_T_, int, const union sigval));
int sigtimedwait __P((const sigset_t *, siginfo_t *, const struct timespec *));
int sigwaitinfo __P((const sigset_t *, siginfo_t *));
__END_DECLS

#endif
#ifndef _POSIX_SOURCE
int	killpg __P((_BSD_PID_T_, int));
int	sigaltstack __P((const stack_t *, stack_t *)); 
int	sigblock __P((int));
int	siginterrupt __P((int, int));
int	sigpause __P((int));
int	sigreturn __P((ucontext_t *));
int	sigsetmask __P((int));
int	sigstack __P((const struct sigstack *, struct sigstack *));
int	sigvec __P((int, struct sigvec *, struct sigvec *));
void	psignal __P((unsigned int, const char *));
#endif /* !_POSIX_SOURCE */
#endif /* !_ANSI_SOURCE */
__END_DECLS

#endif /* !_SIGNAL_H_ */
