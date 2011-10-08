/*-
 * Copyright (c) 2000-2001 Doug Rabson
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

	.data
	.global ia64_pal_entry
ia64_pal_entry:	.quad 0
	.text

/*
 * struct ia64_pal_result ia64_call_pal_static(u_int64_t proc,
 *	u_int64_t arg1, u_int64_t arg2, u_int64_t arg3)
 */
ENTRY(ia64_call_pal_static, 4)

	.regstk	4,4,0,0
palret	=	loc0
entry	=	loc1
rpsave	=	loc2
pfssave =	loc3

	alloc	pfssave=ar.pfs,4,4,0,0
	;; 
	mov	rpsave=rp
	movl	entry=@gprel(ia64_pal_entry)

1:	mov	palret=ip		// for return address
	;;
	add	entry=entry,gp
	add	palret=2f-1b,palret	// calculate return address
	mov	r28=in0			// procedure number
	mov	r29=in1			// copy arguments
	mov	r30=in2
	mov	r31=in3
	;;
	ld8	entry=[entry]		// read entry point
	mov	b0=palret
	;;
	mov	b6=entry
	;;
	br.cond.sptk b6			// call into firmware
	;;
2:
	mov	rp=rpsave
	mov	ar.pfs=pfssave
	;;
	br.ret.sptk rp
	;;
END(ia64_call_pal_static)

/*
 * struct ia64_pal_result ia64_call_pal_stacked(u_int64_t proc,
 *	u_int64_t arg1, u_int64_t arg2, u_int64_t arg3)
 */
ENTRY(ia64_call_pal_stacked, 4)
	
	.regstk	4,4,4,0
entry	=	loc0
rpsave	=	loc1
pfssave =	loc2
psrsave	=	loc3

	alloc	pfssave=ar.pfs,4,4,4,0
	;; 
	mov	rpsave=rp
	movl	entry=@gprel(ia64_pal_entry)
	;;
	add	entry=entry,gp
	mov	psrsave=psr
	mov	r28=in0			// procedure number
	mov	out0=in0
	;;
	ld8	entry=[entry]		// read entry point
	mov	out1=in1		// copy arguments
	mov	out2=in2
	mov	out3=in3
	;;
	mov	b6=entry
	;;
	rsm	psr.i			// disable interrupts
	;;
	br.call.sptk.many rp=b6		// call into firmware
	mov	psr.l=psrsave
	mov	rp=rpsave
	mov	ar.pfs=pfssave
	;;
	srlz.d
	br.ret.sptk rp

END(ia64_call_pal_stacked)
