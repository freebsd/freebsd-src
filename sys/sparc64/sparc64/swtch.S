/*-
 * Copyright (c) 2001 Jake Burkholder.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <machine/asmacros.h>

#include "assym.s"

/*
 * Save and restore FPU state. This is done at switch time.
 * We could use FPRS_DL and FPRS_DU; however, it is accessible to non-privileged
 * software, so it is avoided for compatabilities sake.
 * savefp clobbers %fprs.
 */
	.macro	savefp	state, tmp
	rd	%fprs, \tmp
	stx	\tmp, [\state + FP_FPRS]
	or	\tmp, FPRS_FEF, \tmp
	wr	\tmp, 0, %fprs
	stx	%fsr, [\state + FP_FSR]
	rd	%asi, \tmp
	wr	%g0, ASI_BLK_S, %asi
	stda	%f0, [\state + FP_FB0] %asi
	stda	%f16, [\state + FP_FB1] %asi
	stda	%f32, [\state + FP_FB2] %asi
	stda	%f48, [\state + FP_FB3] %asi
	wr	\tmp, 0, %asi
	membar	#Sync
	.endm
	
	.macro	restrfp	state, tmp
	rd	%fprs, \tmp
	or	\tmp, FPRS_FEF, \tmp
	wr	\tmp, 0, %fprs
	rd	%asi, \tmp
	wr	%g0, ASI_BLK_S, %asi
	ldda	[\state + FP_FB0] %asi, %f0
	ldda	[\state + FP_FB1] %asi, %f16
	ldda	[\state + FP_FB2] %asi, %f32
	ldda	[\state + FP_FB3] %asi, %f48
	wr	\tmp, 0, %asi
	membar	#Sync
	ldx	[\state + FP_FSR], %fsr
	ldx	[\state + FP_FPRS], \tmp
	wr	\tmp, 0, %fprs
	.endm

ENTRY(cpu_switch)
	save	%sp, -CCFSZ, %sp
	call	chooseproc
	 ldx	[PCPU(CURPROC)], %l0
	cmp	%l0, %o0
	be,pn	%xcc, 3f
	 ldx	[%l0 + P_FRAME], %l2
	ldx	[%l2 + TF_TSTATE], %l2
	andcc	%l2, TSTATE_PEF, %l2
	be,pt	%xcc, 1f
	 ldx	[PCPU(CURPCB)], %l1
	savefp	%l1 + PCB_FPSTATE, %l3
1:	flushw
	wrpr	%g0, 0, %cleanwin
	stx	%fp, [%l1 + PCB_FP]
	stx	%i7, [%l1 + PCB_PC]
	ldx	[%o0 + P_ADDR], %o1
	ldx	[%o1 + U_PCB + PCB_FP], %fp
	ldx	[%o1 + U_PCB + PCB_PC], %i7
	ldx	[%o0 + P_FRAME], %l2
	ldx	[%l2 + TF_TSTATE], %l2
	andcc	%l2, TSTATE_PEF, %l2
	be,pt	%xcc, 2f
	 stx	%o0, [PCPU(CURPROC)]
	restrfp	%o1 + U_PCB + PCB_FPSTATE, %l4
2:	stx	%o1, [PCPU(CURPCB)]
	sub     %fp, CCFSZ, %sp
3:	ret
	 restore
END(cpu_switch)

ENTRY(savectx)
	save	%sp, -CCFSZ, %sp
	flushw
	ldx	[PCPU(CURPROC)], %l0
	ldx	[%l0 + P_FRAME], %l0
	ldx	[%l0 + TF_TSTATE], %l0
	andcc	%l0, TSTATE_PEF, %l0
	be,pt	%xcc, 1f
	 stx	%fp, [%i0 + PCB_FP]
	add	%i0, PCB_FPSTATE, %o0
	call	savefpctx
1:	 stx	%i7, [%i0 + PCB_PC]
	ret
	 restore %g0, 0, %o0
END(savectx)

ENTRY(savefpctx)
	rd	%fprs, %o2
	savefp	%o0, %o1
	retl
	 wr	%o2, 0, %fprs
END(savefpctx)

ENTRY(restorefpctx)
	restrfp	%o0, %o1
	retl
	 nop
END(restorefpctx)
