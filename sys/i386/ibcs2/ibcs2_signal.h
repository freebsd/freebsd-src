/*	$NetBSD: ibcs2_signal.h,v 1.7 1995/08/14 02:26:01 mycroft Exp $	*/

/*
 * Copyright (c) 1994, 1995 Scott Bartram
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
 *      This product includes software developed by Scott Bartram.
 * 4. The name of the author may not be used to endorse or promote products
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
 * $FreeBSD: src/sys/i386/ibcs2/ibcs2_signal.h,v 1.5 1999/09/29 15:12:10 marcel Exp $
 */

#ifndef	_IBCS2_SIGNAL_H
#define	_IBCS2_SIGNAL_H

#define IBCS2_SIGHUP		1
#define IBCS2_SIGINT		2
#define IBCS2_SIGQUIT		3
#define IBCS2_SIGILL		4
#define IBCS2_SIGTRAP		5
#define IBCS2_SIGIOT		6
#define IBCS2_SIGABRT		6
#define IBCS2_SIGEMT		7
#define IBCS2_SIGFPE		8
#define IBCS2_SIGKILL		9
#define IBCS2_SIGBUS		10
#define IBCS2_SIGSEGV		11
#define IBCS2_SIGSYS		12
#define IBCS2_SIGPIPE		13
#define IBCS2_SIGALRM		14
#define IBCS2_SIGTERM		15
#define IBCS2_SIGUSR1		16
#define IBCS2_SIGUSR2		17
#define IBCS2_SIGCLD		18
#define IBCS2_SIGPWR		19
#define IBCS2_SIGWINCH		20
#define IBCS2_SIGPOLL		22
#define IBCS2_NSIG		32
#define IBCS2_SIGTBLSZ		32

/*
 * SCO-specific
 */
#define IBCS2_SIGSTOP		23
#define IBCS2_SIGTSTP		24
#define IBCS2_SIGCONT		25
#define IBCS2_SIGTTIN		26
#define IBCS2_SIGTTOU		27
#define IBCS2_SIGVTALRM		28
#define IBCS2_SIGPROF		29

#define IBCS2_SIGNO_MASK	0x00FF
#define IBCS2_SIGNAL_MASK	0x0000
#define IBCS2_SIGSET_MASK	0x0100
#define IBCS2_SIGHOLD_MASK	0x0200
#define IBCS2_SIGRELSE_MASK	0x0400
#define IBCS2_SIGIGNORE_MASK	0x0800
#define IBCS2_SIGPAUSE_MASK	0x1000

#define IBCS2_SIGNO(x)		((x) & IBCS2_SIGNO_MASK)
#define IBCS2_SIGCALL(x)	((x) & ~IBCS2_SIGNO_MASK)

typedef long	ibcs2_sigset_t;
typedef void	(*ibcs2_sig_t) __P((int));

struct ibcs2_sigaction {
	ibcs2_sig_t	isa_handler;
	ibcs2_sigset_t	isa_mask;
	int		isa_flags;
};

#define IBCS2_SIG_DFL		((ibcs2_sig_t)0)
#define IBCS2_SIG_ERR		((ibcs2_sig_t)-1)
#define IBCS2_SIG_IGN		((ibcs2_sig_t)1)
#define IBCS2_SIG_HOLD		((ibcs2_sig_t)2)

#define IBCS2_SIG_SETMASK	0
#define IBCS2_SIG_BLOCK		1
#define IBCS2_SIG_UNBLOCK	2

/* sa_flags */
#define IBCS2_SA_NOCLDSTOP	1

extern int bsd_to_ibcs2_sig[];

#endif /* _IBCS2_SIGNAL_H */
