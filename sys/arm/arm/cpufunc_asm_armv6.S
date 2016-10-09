/*	$NetBSD: cpufunc_asm_armv6.S,v 1.4 2010/12/10 02:06:22 bsh Exp $	*/

/*
 * Copyright (c) 2002, 2005 ARM Limited
 * Portions Copyright (c) 2007 Microsoft
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
 * 3. The name of the company may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * ARMv6 assembly functions for manipulating caches.
 * These routines can be used by any core that supports the mcrr address
 * range operations.
 */

/*
 * $FreeBSD$
 */

#include <machine/asm.h>

	.arch	armv6

/*
 * Cache operations.
 */

#ifdef ELF_TRAMPOLINE
/* LINTSTUB: void armv6_idcache_wbinv_all(void); */
ENTRY_NP(armv6_idcache_wbinv_all)
	/*
	 * We assume that the code here can never be out of sync with the
	 * dcache, so that we can safely flush the Icache and fall through
	 * into the Dcache purging code.
	 */
	mcr	p15, 0, r0, c7, c5, 0	/* Flush I cache */

	/* Purge Dcache. */
	mcr	p15, 0, r0, c7, c14, 0	/* clean & invalidate D cache */
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	RET
END(armv6_idcache_wbinv_all)
#endif /* ELF_TRAMPOLINE */
