/*-
 * Copyright (c) 2000 Doug Rabson
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

BSS(ia64_pal_entry, 8)

/*
 * struct ia64_pal_result ia64_call_pal_static(u_int64_t proc,
	u_int64_t arg1, u_int64_t arg2, u_int64_t arg3)
 */
	NESTED(ia64_call_pal_static, 4, 5, 0, r39, r40)
	
palret	=	loc0
entry	=	loc1
rpsave	=	loc2
pfssave =	loc3
psrsave	=	loc4

	movl	entry=ia64_pal_entry
1:	mov	palret=ip		// for return address
	mov	rpsave=rp
	;; 
	mov	psrsave=psr
	mov	r28=in0			// procedure number
	;;
	ld8	entry=[entry]		// read entry point
	mov	r29=in1			// copy arguments
	mov	r30=in2
	mov	r31=in3
	;;
	mov	b1=entry
	add	palret=2f-1b,palret	// calculate return address
	;;
	mov	b0=palret
	rsm	psr.i			// disable interrupts
	;;
	br.cond.sptk b1			// call into firmware
2:	mov	psr.l=psrsave
	mov	b0=rpsave
	mov	ar.pfs=pfssave
	;;
	srlz.d
	br.ret.sptk b0		

	END(ia64_call_pal_static)