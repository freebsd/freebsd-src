/*
 * Copyright (c) 2003 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/ucontext.h>
#include <machine/fpu.h>
#include <stdarg.h>
#include <stdio.h>

struct fdesc {
	uint64_t ip;
	uint64_t gp;
};

typedef void (*func_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
    uint64_t, uint64_t, uint64_t);

static __inline uint64_t *
spill(uint64_t *bsp, uint64_t arg)
{
	*bsp++ = arg;
	if (((intptr_t)bsp & 0x1ff) == 0x1f8)
		*bsp++ = 0;
	return (bsp);
}

static void
ctx_wrapper(ucontext_t *ucp, func_t func, uint64_t *args)
{

	(*func)(args[0], args[1], args[2], args[3], args[4], args[5], args[6],
	    args[7]);
	if (ucp->uc_link == NULL)
		exit(0);
	setcontext((const ucontext_t *)ucp->uc_link);
	/* should never get here */
	abort();
	/* NOTREACHED */
}

__weak_reference(__makecontext, makecontext);

void
__makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...)
{
	uint64_t *args, *bsp;
	va_list ap;
	int i;

	/*
	 * Drop the ball completely if something's not right. We only
	 * support general registers as arguments and not more than 8
	 * of them. Things get hairy if we need to support FP registers
	 * (alignment issues) or more than 8 arguments (stack based).
	 */
	if (argc < 0 || argc > 8 || ucp == NULL ||
	    ucp->uc_stack.ss_sp == NULL || (ucp->uc_stack.ss_size & 15) ||
	    ((intptr_t)ucp->uc_stack.ss_sp & 15) ||
	    ucp->uc_stack.ss_size < MINSIGSTKSZ)
		abort();

	/*
	 * Copy the arguments of function 'func' onto the (memory) stack.
	 * Always take up space for 8 arguments.
	 */
	va_start(ap, argc);
	args = (uint64_t*)(ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size) - 8;
	i = 0;
	while (i < argc)
		args[i++] = va_arg(ap, uint64_t);
	while (i < 8)
		args[i++] = 0;
	va_end(ap);

	/*
	 * Push (spill) the arguments of the context wrapper onto the register
	 * stack. They get loaded by the RSE on a context switch.
	 */
	bsp = (uint64_t*)ucp->uc_stack.ss_sp;
	bsp = spill(bsp, (intptr_t)ucp);
	bsp = spill(bsp, (intptr_t)func);
	bsp = spill(bsp, (intptr_t)args);

	/*
	 * Setup the MD portion of the context.
	 */
	memset(&ucp->uc_mcontext, 0, sizeof(ucp->uc_mcontext));
	ucp->uc_mcontext.mc_special.sp = (intptr_t)args - 16;
	ucp->uc_mcontext.mc_special.bspstore = (intptr_t)bsp;
	ucp->uc_mcontext.mc_special.pfs = (3 << 7) | 3;
	ucp->uc_mcontext.mc_special.rsc = 0xf;
	ucp->uc_mcontext.mc_special.rp = ((struct fdesc*)ctx_wrapper)->ip;
	ucp->uc_mcontext.mc_special.gp = ((struct fdesc*)ctx_wrapper)->gp;
	ucp->uc_mcontext.mc_special.fpsr = IA64_FPSR_DEFAULT;
}
