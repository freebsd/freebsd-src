/*	$NetBSD: cpufunc_asm_ixp12x0.S,v 1.2 2002/08/17 16:36:31 thorpej Exp $	*/

/*-
 * Copyright (c) 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD: src/sys/arm/arm/cpufunc_asm_ixp12x0.S,v 1.3 2005/01/05 21:58:47 imp Exp $");

/*
 * This function is the same as sa110_context_switch for now, the plan
 * is to make use of the process id register to avoid cache flushes.
 */
ENTRY(ixp12x0_context_switch)
	/*
	 * CF_CACHE_PURGE_ID will *ALWAYS* be called prior to this.
	 * Thus the data cache will contain only kernel data and the
	 * instruction cache will contain only kernel code, and all
	 * kernel mappings are shared by all processes.
	 */

	/* Write the TTB */
	mcr	p15, 0, r0, c2, c0, 0

	/* If we have updated the TTB we must flush the TLB */
	mcr	p15, 0, r0, c8, c7, 0	/* flush the I+D tlb */

	/* Make sure that pipeline is emptied */
	mov	r0, r0
	mov	r0, r0
	RET

ENTRY(ixp12x0_drain_readbuf)
	mcr	p15, 0, r0, c9, c0, 0		/* drain read buffer */
	RET

/*
 * Information for the IXP12X0 cache clean/purge functions:
 *
 *      * Virtual address of the memory region to use
 *      * Size of memory region
 */
	.data

	.global _C_LABEL(ixp12x0_cache_clean_addr)
_C_LABEL(ixp12x0_cache_clean_addr):
        .word   0xf0000000

	.global _C_LABEL(ixp12x0_cache_clean_size)
_C_LABEL(ixp12x0_cache_clean_size):
	.word   0x00008000

	.text

.Lixp12x0_cache_clean_addr:
	.word   _C_LABEL(ixp12x0_cache_clean_addr)
.Lixp12x0_cache_clean_size:
	.word   _C_LABEL(ixp12x0_cache_clean_size)
