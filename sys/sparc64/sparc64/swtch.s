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

ENTRY(cpu_switch)
	save	%sp, -CCFSZ, %sp
	call	chooseproc
	 ldx	[PCPU(CURPROC)], %l0
	cmp	%l0, %o0
	be,pn	%xcc, 2f
	 ldx	[PCPU(FPCURPROC)], %l2
	cmp	%l0, %l2
	bne,pt	%xcc, 1f
	 ldx	[PCPU(CURPCB)], %l1
	PANIC("cpu_switch: fpcurproc", %i0)
1:	flushw
	wrpr	%g0, 0, %cleanwin
	stx	%fp, [%l1 + PCB_FP]
	stx	%i7, [%l1 + PCB_PC]
	ldx	[%o0 + P_ADDR], %o1
	ldx	[%o1 + U_PCB + PCB_FP], %fp
	ldx	[%o1 + U_PCB + PCB_PC], %i7
	stx	%o0, [PCPU(CURPROC)]
	stx	%o1, [PCPU(CURPCB)]
	sub     %fp, CCFSZ, %sp
2:	ret
	 restore
END(cpu_switch)

ENTRY(savectx)
	save	%sp, -CCFSZ, %sp
	flushw
	ldx	[PCPU(FPCURPROC)], %l0
	brz,pt	%l0, 1f
	 nop
	illtrap
1:	stx	%fp, [%i0 + PCB_FP]
	stx	%i7, [%i0 + PCB_PC]
	ret
	 restore %g0, 0, %o0
END(savectx)
