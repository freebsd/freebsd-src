/*-
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
 * $FreeBSD: src/sys/compat/svr4/svr4_ucontext.h,v 1.4 2005/01/05 22:34:37 imp Exp $
 */

#ifndef	_SVR4_UCONTEXT_H_
#define	_SVR4_UCONTEXT_H_

/*
 * Machine context
 */

#define SVR4_UC_SIGMASK		0x01
#define	SVR4_UC_STACK		0x02

#define SVR4_UC_CPU		0x04
#define SVR4_UC_FPU		0x08
#define SVR4_UC_WEITEK		0x10

#define SVR4_UC_MCONTEXT	(SVR4_UC_CPU|SVR4_UC_FPU|SVR4_UC_WEITEK)

#define SVR4_UC_ALL		(SVR4_UC_SIGMASK|SVR4_UC_STACK|SVR4_UC_MCONTEXT)

typedef struct svr4_ucontext {
	u_long			 uc_flags;
  /*	struct svr4_ucontext	*uc_link;*/
        void                    *uc_link;
	svr4_sigset_t		 uc_sigmask;
	struct svr4_sigaltstack	 uc_stack;
	svr4_mcontext_t		 uc_mcontext;
	long			 uc_pad[5];
} svr4_ucontext_t;

#define SVR4_UC_GETREGSET	0
#define SVR4_UC_SETREGSET	1

/*
 * Signal frame
 */
struct svr4_sigframe {
	int	sf_signum;
	union	svr4_siginfo  *sf_sip;
	struct	svr4_ucontext *sf_ucp;
	sig_t	sf_handler;
	struct	svr4_ucontext sf_uc;
	union	svr4_siginfo  sf_si;
};

#endif /* !_SVR4_UCONTEXT_H_ */
