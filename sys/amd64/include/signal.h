/*
 * Copyright (c) 1986, 1989, 1991, 1993
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
 *	@(#)signal.h	8.1 (Berkeley) 6/11/93
 * $FreeBSD$
 */

#ifndef _MACHINE_SIGNAL_H_
#define	_MACHINE_SIGNAL_H_

#include <sys/cdefs.h>
#include <sys/_sigset.h>

/*
 * Machine-dependent signal definitions
 */

typedef long sig_atomic_t;

#if __XSI_VISIBLE
/*
 * Minimum signal stack size. The current signal frame
 * for i386 is 408 bytes large.
 */
#define	MINSIGSTKSZ	(512 * 4)
#endif

#if __BSD_VISIBLE
#include <machine/trap.h>	/* codes for SIGILL, SIGFPE */

/*
 * Only the kernel should need these old type definitions.
 */
/*
 * Information pushed on stack when a signal is delivered.
 * This is used by the kernel to restore state following
 * execution of the signal handler.  It is also made available
 * to the handler to allow it to restore state properly if
 * a non-standard exit is performed.
 */
/*
 * The sequence of the fields/registers in struct sigcontext should match
 * those in mcontext_t.
 */
struct sigcontext {
	struct __sigset sc_mask;	/* signal mask to restore */
	long	sc_onstack;		/* sigstack state to restore */
	long	sc_rdi;		/* machine state (struct trapframe) */
	long	sc_rsi;
	long	sc_rdx;
	long	sc_rcx;
	long	sc_r8;
	long	sc_r9;
	long	sc_rax;
	long	sc_rbx;
	long	sc_rbp;
	long	sc_r10;
	long	sc_r11;
	long	sc_r12;
	long	sc_r13;
	long	sc_r14;
	long	sc_r15;
	long	sc_trapno;
	long	sc_addr;
	long	sc_err;
	long	sc_rip;
	long	sc_cs;
	long	sc_rflags;
	long	sc_rsp;
	long	sc_ss;
	long	sc_len;			/* sizeof(mcontext_t) */
	/*
	 * XXX - See <machine/ucontext.h> and <machine/npx.h> for
	 *       the following fields.
	 */
	long	sc_fpformat;
	long	sc_ownedfp;
	long	sc_spare1[1];
	long	sc_fpstate[64] __aligned(16);
	long	sc_spare2[8];
};
#endif /* __BSD_VISIBLE */

#endif /* !_MACHINE_SIGNAL_H_ */
