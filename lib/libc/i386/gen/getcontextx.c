/*
 * Copyright (c) 2011 Konstantin Belousov <kib@FreeBSD.org>
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
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <machine/npx.h>
#include <machine/specialreg.h>
#include <machine/sysarch.h>

static int xstate_sz = -1;

int
__getcontextx_size(void)
{
	u_int p[4];
	int cpuid_supported;

	if (xstate_sz == -1) {
		__asm __volatile(
		    "	pushfl\n"
		    "	popl	%%eax\n"
		    "	movl    %%eax,%%ecx\n"
		    "	xorl    $0x200000,%%eax\n"
		    "	pushl	%%eax\n"
		    "	popfl\n"
		    "	pushfl\n"
		    "	popl    %%eax\n"
		    "	xorl    %%eax,%%ecx\n"
		    "	je	1f\n"
		    "	movl	$1,%0\n"
		    "	jmp	2f\n"
		    "1:	movl	$0,%0\n"
		    "2:\n"
		    : "=r" (cpuid_supported) : : "eax", "ecx");
		if (cpuid_supported) {
			__asm __volatile(
			    "	pushl	%%ebx\n"
			    "	cpuid\n"
			    "	movl	%%ebx,%1\n"
			    "	popl	%%ebx\n"
			    : "=a" (p[0]), "=r" (p[1]), "=c" (p[2]), "=d" (p[3])
			    :  "0" (0x1));
			if ((p[2] & CPUID2_OSXSAVE) != 0) {
				__asm __volatile(
				    "	pushl	%%ebx\n"
				    "	cpuid\n"
				    "	movl	%%ebx,%1\n"
				    "	popl	%%ebx\n"
				    : "=a" (p[0]), "=r" (p[1]), "=c" (p[2]),
					"=d" (p[3])
				    :  "0" (0xd), "2" (0x0));
				xstate_sz = p[1] - sizeof(struct savexmm);
			} else
				xstate_sz = 0;
		} else
			xstate_sz = 0;
	}

	return (sizeof(ucontext_t) + xstate_sz);
}

int
__fillcontextx2(char *ctx)
{
	struct i386_get_xfpustate xfpu;
	ucontext_t *ucp;

	ucp = (ucontext_t *)ctx;
	if (xstate_sz != 0) {
		xfpu.addr = (char *)(ucp + 1);
		xfpu.len = xstate_sz;
		if (sysarch(I386_GET_XFPUSTATE, &xfpu) == -1)
			return (-1);
		ucp->uc_mcontext.mc_xfpustate = (__register_t)xfpu.addr;
		ucp->uc_mcontext.mc_xfpustate_len = xstate_sz;
		ucp->uc_mcontext.mc_flags |= _MC_HASFPXSTATE;
	} else {
		ucp->uc_mcontext.mc_xfpustate = 0;
		ucp->uc_mcontext.mc_xfpustate_len = 0;
	}
	return (0);
}

int
__fillcontextx(char *ctx)
{
	ucontext_t *ucp;

	ucp = (ucontext_t *)ctx;
	if (getcontext(ucp) == -1)
		return (-1);
	__fillcontextx2(ctx);
	return (0);
}

__weak_reference(__getcontextx, getcontextx);

ucontext_t *
__getcontextx(void)
{
	char *ctx;
	int error;

	ctx = malloc(__getcontextx_size());
	if (ctx == NULL)
		return (NULL);
	if (__fillcontextx(ctx) == -1) {
		error = errno;
		free(ctx);
		errno = error;
		return (NULL);
	}
	return ((ucontext_t *)ctx);
}
