/*
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1994 Christos Zoulas
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
 * 
 * $FreeBSD$
 */

#ifndef	_SVR4_SIGNAL_H_
#define	_SVR4_SIGNAL_H_

#include <i386/svr4/svr4_machdep.h>
#include <compat/svr4/svr4_siginfo.h>

#define	SVR4_SIGHUP	 1
#define	SVR4_SIGINT	 2
#define	SVR4_SIGQUIT	 3
#define	SVR4_SIGILL	 4
#define	SVR4_SIGTRAP	 5
#define	SVR4_SIGIOT	 6
#define	SVR4_SIGABRT	 6
#define	SVR4_SIGEMT	 7
#define	SVR4_SIGFPE	 8
#define	SVR4_SIGKILL	 9
#define	SVR4_SIGBUS	10
#define	SVR4_SIGSEGV	11
#define	SVR4_SIGSYS	12
#define	SVR4_SIGPIPE	13
#define	SVR4_SIGALRM	14
#define	SVR4_SIGTERM	15
#define	SVR4_SIGUSR1	16
#define	SVR4_SIGUSR2	17
#define	SVR4_SIGCLD	18
#define	SVR4_SIGCHLD	18
#define	SVR4_SIGPWR	19
#define	SVR4_SIGWINCH	20
#define	SVR4_SIGURG	21
#define	SVR4_SIGPOLL	22
#define	SVR4_SIGIO	22
#define	SVR4_SIGSTOP	23
#define	SVR4_SIGTSTP	24
#define	SVR4_SIGCONT	25
#define	SVR4_SIGTTIN	26
#define	SVR4_SIGTTOU	27
#define	SVR4_SIGVTALRM	28
#define	SVR4_SIGPROF	29
#define	SVR4_SIGXCPU	30
#define	SVR4_SIGXFSZ	31
#define SVR4_NSIG	32
#define SVR4_SIGTBLSZ	31

#define	SVR4_SIGNO_MASK		0x00FF
#define	SVR4_SIGNAL_MASK	0x0000
#define	SVR4_SIGDEFER_MASK	0x0100
#define	SVR4_SIGHOLD_MASK	0x0200
#define	SVR4_SIGRELSE_MASK	0x0400
#define	SVR4_SIGIGNORE_MASK	0x0800
#define	SVR4_SIGPAUSE_MASK	0x1000

typedef void (*svr4_sig_t) __P((int, svr4_siginfo_t *, void *));
#define	SVR4_SIG_DFL	(svr4_sig_t)	 0
#define	SVR4_SIG_ERR	(svr4_sig_t)	-1
#define	SVR4_SIG_IGN	(svr4_sig_t)	 1
#define	SVR4_SIG_HOLD	(svr4_sig_t)	 2

#define SVR4_SIGNO(a)	((a) & SVR4_SIGNO_MASK)
#define SVR4_SIGCALL(a) ((a) & ~SVR4_SIGNO_MASK)

#define SVR4_SIG_BLOCK		1
#define SVR4_SIG_UNBLOCK	2
#define SVR4_SIG_SETMASK	3

extern int bsd_to_svr4_sig[];
extern int svr4_to_bsd_sig[];

#define SVR4_BSD2SVR4_SIG(sig)	\
	(((sig) <= SVR4_SIGTBLSZ) ? bsd_to_svr4_sig[_SIG_IDX(sig)] : sig)
#define SVR4_SVR42BSD_SIG(sig)	\
	(((sig) <= SVR4_SIGTBLSZ) ? svr4_to_bsd_sig[_SIG_IDX(sig)] : sig)

typedef struct {
        u_long bits[4];
} svr4_sigset_t;

struct svr4_sigaction {
	int		ssa_flags;
	svr4_sig_t	ssa_handler;
	svr4_sigset_t	ssa_mask;
	int 		ssa_reserved[2];
};

struct svr4_sigaltstack {
	char		*ss_sp;
	int		ss_size;
	int		ss_flags;
};

/* sa_flags */
#define SVR4_SA_ONSTACK		0x00000001
#define SVR4_SA_RESETHAND	0x00000002
#define SVR4_SA_RESTART		0x00000004
#define SVR4_SA_SIGINFO		0x00000008
#define SVR4_SA_NODEFER		0x00000010
#define SVR4_SA_NOCLDWAIT	0x00010000	/* No zombies 	*/
#define SVR4_SA_NOCLDSTOP	0x00020000	/* No jcl	*/
#define	SVR4_SA_ALLBITS		0x0003001f

/* ss_flags */
#define SVR4_SS_ONSTACK		0x00000001
#define SVR4_SS_DISABLE		0x00000002
#define	SVR4_SS_ALLBITS		0x00000003

void bsd_to_svr4_sigaltstack __P((const struct sigaltstack *, struct svr4_sigaltstack *));
void bsd_to_svr4_sigset __P((const sigset_t *, svr4_sigset_t *));
void svr4_to_bsd_sigaltstack __P((const struct svr4_sigaltstack *, struct sigaltstack *));
void svr4_to_bsd_sigset __P((const svr4_sigset_t *, sigset_t *));
void svr4_sendsig(sig_t, int, sigset_t  *, u_long);

#endif /* !_SVR4_SIGNAL_H_ */
