/*-
 * Copyright (c) 2004 Olivier Houchard
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
#include <sys/ucontext.h>

#include <machine/frame.h>
#include <machine/sigframe.h>

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <signal.h>

__weak_reference(__signalcontext, signalcontext);

extern void _ctx_start(void);

int
__signalcontext(ucontext_t *ucp, int sig, __sighandler_t *func)
{
	struct sigframe *sfp;
	__greg_t *gr = ucp->uc_mcontext.__gregs;
	unsigned int *sp;
	mcontext_t *mc;

	mc = &ucp->uc_mcontext;
	sp = (unsigned int *)
	    (((uintptr_t)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size) & ~7);

	sfp = (struct sigframe *)sp - 1;
	
	bzero(sfp, sizeof(*sfp));
	bcopy(ucp, &sfp->sf_uc, sizeof(*ucp));
	sfp->sf_si.si_signo = sig;

	gr[_REG_SP] = (__greg_t)sp;
	/* Wipe out frame pointer. */
	gr[_REG_FP] = 0;
	/* Arrange for return via the trampoline code. */
	gr[_REG_LR] = (__greg_t)_ctx_start;
	gr[_REG_PC] = (__greg_t)func;
	gr[_REG_R0] = (__greg_t)ucp;

	ucp->uc_link = &sfp->sf_uc;
	sigdelset(&ucp->uc_sigmask, sig);

	return (0);
}
