/*-
 * Copyright (c) 1994
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
 *	@(#)interrupt.h	8.1 (Berkeley) 1/9/94
 */

/*
 * Macros to declare the variables and then turn on and off interrupts.
 */
#define	DECLARE_INTERRUPTS						\
	struct sigaction __act, __oact;					\
	struct termios __nterm, __term;					\
	u_int __istate;							\
	int __isig, __termreset

/*
 * Search, global, and substitute interruptions.
 *
 * ISIG turns on VINTR, VQUIT and VSUSP.  We want VINTR to interrupt the
 * search, so we install a handler.  VQUIT is ignored by main() because
 * nvi never wants to catch it.  A handler for VSUSP should have been
 * installed by the screen code.
 */
#define	SET_UP_INTERRUPTS(handler) {					\
	if (F_ISSET(sp->gp, G_ISFROMTTY)) {				\
		__act.sa_handler = handler;				\
		sigemptyset(&__act.sa_mask);				\
		__act.sa_flags = 0;					\
		if (__isig = !sigaction(SIGINT, &__act, &__oact)) {	\
			__termreset = 0;				\
			__istate = F_ISSET(sp, S_INTERRUPTIBLE);	\
			F_CLR(sp, S_INTERRUPTED);			\
			F_SET(sp, S_INTERRUPTIBLE);			\
			if (tcgetattr(STDIN_FILENO, &__term)) {		\
				rval = 1;				\
				msgq(sp, M_SYSERR, "tcgetattr");	\
				goto interrupt_err;			\
			}						\
			__nterm = __term;				\
			__nterm.c_lflag |= ISIG;			\
			if (tcsetattr(STDIN_FILENO,			\
			    TCSANOW | TCSASOFT, &__nterm)) {		\
				rval = 1;				\
				msgq(sp, M_SYSERR, "tcsetattr");	\
				goto interrupt_err;			\
			}						\
			__termreset = 1;				\
		}							\
	}								\
}

#define	TEAR_DOWN_INTERRUPTS {						\
	if (F_ISSET(sp->gp, G_ISFROMTTY) && __isig) {			\
		if (sigaction(SIGINT, &__oact, NULL)) {			\
			rval = 1;					\
			msgq(sp, M_SYSERR, "signal");			\
		}							\
		if (__termreset && tcsetattr(STDIN_FILENO,		\
		    TCSANOW | TCSASOFT, &__term)) {			\
			rval = 1;					\
			msgq(sp, M_SYSERR, "tcsetattr");		\
		}							\
		F_CLR(sp, S_INTERRUPTED);				\
		if (!__istate)						\
			F_CLR(sp, S_INTERRUPTIBLE);			\
	}								\
}
