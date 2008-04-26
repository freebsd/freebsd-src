/*	$NetBSD: makecontext.c,v 1.3 2003/01/19 08:53:36 matt Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: makecontext.c,v 1.3 2003/01/19 08:53:36 matt Exp $");
#endif

#include <sys/types.h>
#include <ucontext.h>
#include <stdarg.h>

void
makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
{
	/* XXXMIPS: Implement me */
#if 0
	__greg_t *gr = ucp->uc_mcontext.__gregs;
	uintptr_t *sp;
	int i;
	va_list ap;

	void __resumecontext(void);

	/* LINTED uintptr_t is safe */
	sp  = (uintptr_t *)
	    ((uintptr_t)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size);
	/* LINTED uintptr_t is safe */
	sp -= (argc >= 4 ? argc : 4);	/* Make room for >=4 arguments. */
	sp  = (uintptr_t *)
	      ((uintptr_t)sp & ~0x7);	/* Align on double-word boundary. */

	gr[_REG_SP]  = (__greg_t)sp;
	gr[_REG_RA]  = (__greg_t)__resumecontext;
	gr[_REG_T9]  = (__greg_t)func;	/* required for .abicalls */
	gr[_REG_EPC] = (__greg_t)func;

	/* Construct argument list. */
	va_start(ap, argc);
	/* Up to the first four arguments are passed in $a0-3. */
	for (i = 0; i < argc && i < 4; i++)
		/* LINTED uintptr_t is safe */
		gr[_REG_A0 + i] = va_arg(ap, uintptr_t);
	/* Pass remaining arguments on the stack above the $a0-3 gap. */
	for (sp += 4; i < argc; i++)
		/* LINTED uintptr_t is safe */
		*sp++ = va_arg(ap, uintptr_t);
	va_end(ap);
#endif
}
