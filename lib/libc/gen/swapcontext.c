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
#include <sys/ucontext.h>

#include <errno.h>
#include <stddef.h>

__weak_reference(__swapcontext, swapcontext);

#warning "FIX __swapcontext on non i386 please!"

int
__swapcontext(ucontext_t *oucp, const ucontext_t *ucp)
{
	int ret;

#ifdef __i386__
	/* XXX PLEASE FIX! XXX */
	if ((oucp == NULL) ||
	    (oucp->uc_mcontext.mc_len != sizeof(mcontext_t)) ||
	    (ucp == NULL) ||
	    (ucp->uc_mcontext.mc_len != sizeof(mcontext_t))) {
		errno = EINVAL;
		return (-1);
	}
#endif
	oucp->uc_flags &= ~UCF_SWAPPED;
	ret = getcontext(oucp);
	if ((ret == 0) && !(oucp->uc_flags & UCF_SWAPPED)) {
		oucp->uc_flags |= UCF_SWAPPED;
		ret = setcontext(ucp);
	}
	return (ret);
}
