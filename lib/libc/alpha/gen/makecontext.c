/*
 * Copyright (c) 2001 Daniel M. Eischen <deischen@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/signal.h>

#include <errno.h>
#include <stdarg.h>
#include <ucontext.h>
#include <unistd.h>


/* Prototypes */
extern void _ctx_start(int argc, ...);


__weak_reference(__makecontext, makecontext);

void
_ctx_done (ucontext_t *ucp)
{
	if (ucp->uc_link == NULL)
		exit(0);
	else {
		/*
		 * Since this context has finished, don't allow it
		 * to be restarted without being reinitialized (via
		 * setcontext or swapcontext).
		 */
		ucp->uc_mcontext.mc_format = 0;

		/* Set context to next one in link */
		/* XXX - what to do for error, abort? */
		setcontext((const ucontext_t *)ucp->uc_link);
		abort();	/* should never get here */
	}
}

void
__makecontext(ucontext_t *ucp, void (*start)(void), int argc, ...)
{
	va_list 	ap;
	char		*stack_top;
	intptr_t	*argp;
	int		i;

	if (ucp == NULL)
		return;
	else if ((ucp->uc_stack.ss_sp == NULL) ||
	    (ucp->uc_stack.ss_size < MINSIGSTKSZ)) {
		/*
		 * This should really return -1 with errno set to ENOMEM
		 * or something, but the spec says that makecontext is
		 * a void function.   At least make sure that the context
		 * isn't valid so it can't be used without an error.
		 */
		ucp->uc_mcontext.mc_format = 0;
	}
	/* XXX - Do we want to sanity check argc? */
	else if ((argc < 0) || (argc > NCARGS)) {
		ucp->uc_mcontext.mc_format = 0;
	}
	/*
	 * Make sure the context is valid.  For now, we only allow
	 * trapframe format contexts to be used for makecontext.
	 */
	else if (ucp->uc_mcontext.mc_format == __UC_REV0_SIGFRAME) {
		/*
		 * Alpha passes the first 6 parameters in registers and
		 * remaining parameters on the stack.  Set up the context
		 * accordingly, with the user start routine in register
		 * S0, and the context start wrapper (_ctx_start) in the
		 * program counter and return address.  The context must
		 * be in trapframe format.
		 *
		 * Note: The context start wrapper needs to retrieve the
		 *       ucontext pointer.  Place this in register S1
		 *       which must be saved by the callee.
		 */
		stack_top = (char *)(ucp->uc_stack.ss_sp +
		    ucp->uc_stack.ss_size - sizeof(double));
		stack_top = (char *)ALIGN(stack_top);

		/*
		 * Adjust top of stack to allow for any additional integer
		 * arguments beyond 6.
		 */
		if (argc > 6)
			stack_top = stack_top - (sizeof(intptr_t) * (argc - 6));

		argp = (intptr_t *)stack_top;

		va_start(ap, argc);
		for (i = 0; i < argc; i++) {
			switch (i) {
			case 0:	ucp->uc_mcontext.mc_regs[FRAME_TRAPARG_A0] =
				    (unsigned long)va_arg(ap, intptr_t);
				break;

			case 1: ucp->uc_mcontext.mc_regs[FRAME_TRAPARG_A1] =
				    (unsigned long)va_arg(ap, intptr_t);
				break;

			case 2:	ucp->uc_mcontext.mc_regs[FRAME_TRAPARG_A2] =
				    (unsigned long)va_arg(ap, intptr_t);
				break;

			case 3:	ucp->uc_mcontext.mc_regs[FRAME_A3] =
				    (unsigned long)va_arg(ap, intptr_t);
				break;

			case 4:	ucp->uc_mcontext.mc_regs[FRAME_A4] =
				    (unsigned long)va_arg(ap, intptr_t);
				break;

			case 5:	ucp->uc_mcontext.mc_regs[FRAME_A5] =
				    (unsigned long)va_arg(ap, intptr_t);
				break;

			default:
				*argp = va_arg(ap, intptr_t);
				argp++;
				break;
			}
		}
		va_end(ap);

		/*
		 * The start routine and ucontext are placed in registers
		 * S0 and S1 respectively.
		 */
		ucp->uc_mcontext.mc_regs[FRAME_S0] = (unsigned long)start;
		ucp->uc_mcontext.mc_regs[FRAME_S1] = (unsigned long)ucp;

		/*
		 * Set the machine context to point to the top of the stack,
		 * and the program counter and return address to the context
		 * start wrapper.
		 */
		ucp->uc_mcontext.mc_regs[FRAME_SP] = (unsigned long)stack_top;
		ucp->uc_mcontext.mc_regs[FRAME_PC] = (unsigned long)_ctx_start;
		ucp->uc_mcontext.mc_regs[FRAME_RA] = (unsigned long)_ctx_start;
		ucp->uc_mcontext.mc_regs[FRAME_T12] = (unsigned long)_ctx_start;
	}
}
