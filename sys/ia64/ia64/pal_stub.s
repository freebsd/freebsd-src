/*-
 * Copyright (c) 2001 Doug Rabson
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
 *
 *	$FreeBSD$
 */

#include <machine/asm.h>
#include <assym.s>

/*
 * Stub for running in simulation. Fakes the values from an SDV.
 */
ENTRY(ski_fake_pal, 0)

	mov	r8=-3			// default to return error

	cmp.eq	p6,p0=PAL_PTCE_INFO,r28
	;;
(p6)	mov	r8=0
(p6)	mov	r9=0
(p6)	movl	r10=0x100000001
(p6)	mov	r11=0
	;;
	cmp.eq	p6,p0=PAL_FREQ_RATIOS,r28
	;;
(p6)	mov	r8=0
(p6)	movl	r9=0xb00000002		// proc 11/1
(p6)	movl	r10=0x100000001		// bus 1/1
(p6)	movl	r11=0xb00000002		// itc 11/1
	mov	r14=PAL_VM_SUMMARY
	;;
	cmp.eq	p6,p0=r14,r28
	;; 
(p6)	mov	r8=0
(p6)	movl	r9=(8<<40)|(8<<32)
(p6)	movl	r10=(18<<8)|(41<<0)
(p6)	mov	r11=0
	;;
	tbit.nz	p6,p7=r28,8		// static or stacked?
	;;
(p6)	br.ret.sptk.few rp
(p7)	br.cond.sptk.few rp

END(ski_fake_pal)
