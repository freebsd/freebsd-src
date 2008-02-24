/*-
 * Copyright (c) 2002 Jake Burkholder.
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
 */

#include <machine/asm.h>
__FBSDID("$FreeBSD: src/sys/sun4v/sun4v/mp_locore.S,v 1.3 2007/01/19 11:15:34 marius Exp $");

#include <machine/asi.h>
#include <machine/asmacros.h>
#include <machine/hypervisorvar.h>
#include <machine/intr_machdep.h>
#include <machine/ktr.h>
#include <machine/pstate.h>
#include <machine/smp.h>

#include "assym.s"

	.register	%g2, #ignore
	.register	%g3, #ignore

	.text
	_ALIGN_TEXT
	
1:	rd	%pc, %l0			! startpc
	mov	%o0, %g1			! save arg to to mp_tramp_func
	ldx	[%l0 + (6f-1b)], %l1		! read mp_tramp_tte_slots
	add	%l0, (7f-1b), %l2		! %l2 points to start of slot area
	clr	%l3
2:	cmp	%l3, %l1			! number of slots entered == total?
	be	%xcc, 3f
	  nop
	ldx	[%l2], %o0			! VA
	mov	0, %o1				! ctx0
	ldx	[%l2 + 8], %o2			! TTE
	mov	MAP_ITLB|MAP_DTLB, %o3		
	mov	MMU_MAP_PERM_ADDR, %o5
	ta	FAST_TRAP
#ifdef DEBUG
	brz	%o0, 9f
	  nop
	ta	0x77
	ta	0x71	
9:
#endif		
	add	%l2, 16, %l2			! point %l2 at next slot
	inc	%l3
	ba	%xcc, 2b
	  nop
3:		
	ldx	[%l0 + (4f-1b)], %o1		! read mp_tramp_tsb_ra
	mov	2, %o0
	mov	MMU_TSB_CTX0, %o5		! set ctx0 TSBs
	ta	FAST_TRAP
#ifdef DEBUG
	brz	%o0, 9f
	  nop
	ta	0x77
	ta	0x71	
9:
#endif		
	ldx	[%l0 + (5f-1b)], %l1		! fetch mp_tramp_func
	jmpl	%l1, %g0			! and off we go
	  mov	%g1, %o0
	_ALIGN_DATA
4:	.xword	0x0
5:	.xword	0x0
6:	.xword	0x0
7:	

DATA(mp_tramp_code)
	.xword	1b
DATA(mp_tramp_code_len)
	.xword	7b-1b
DATA(mp_tramp_tsb_desc_ra)
	.xword	4b-1b
DATA(mp_tramp_func)
	.xword	5b-1b
DATA(mp_tramp_tte_slots)
	.xword	6b-1b


#define PUTCHAR(x) \
	mov	x, %o0		; \
	mov	CONS_WRITE, %o5	; \
	ta	FAST_TRAP
/*
 * void mp_startup(void)
 */
ENTRY(mp_startup)
	wrpr	%g0, PSTATE_NORMAL, %pstate
	wrpr	%g0, 0, %cleanwin
	wrpr	%g0, 0, %pil
	wr	%g0, 0, %fprs

	SET(cpu_start_args, %l1, %l0)
#if KTR_COMPILE & KTR_SMP
	CATR(KTR_SMP, "mp_start: cpu %d entered kernel"
	    , %g1, %g2, %g3, 7, 8, 9)
	stx	%o0, [%g1 + KTR_PARM1]
9:
#endif

	/*
	 * Inform the boot processor we have inited.
	 */
	mov	CPU_INIT, %l1
	membar	#LoadStore
	stw	%l1, [%l0 + CSA_STATE]

#if KTR_COMPILE & KTR_SMP
	CATR(KTR_SMP, "_mp_start: cpu %d got start signal"
	    , %g1, %g2, %g3, 7, 8, 9)
	stx	%o0, [%g1 + KTR_PARM1]
9:
#endif
2:	
	 ld	[%l0 + CSA_CPUID], %l1
	cmp	%l1, %o0
	bne	%xcc, 2b
	  nop
	
	/*
	 * Get onto our per-cpu panic stack, which precedes the struct pcpu
	 * in the per-cpu page.
	 */
	ldx	[%l0 + CSA_PCPU], %l1
	set	PCPU_PAGES * PAGE_SIZE - PC_SIZEOF, %l2
	add	%l1, %l2, %l1
	sub	%l1, SPOFF + CCFSZ, %sp
	wrpr	%g0, PIL_TICK, %pil
#if KTR_COMPILE & KTR_SMP
	CATR(KTR_SMP,
	    "_mp_start: bootstrap cpuid=%d mid=%d pcpu=%#lx data=%#lx sp=%#lx"
	    , %g1, %g2, %g3, 7, 8, 9)
	lduw	[%l1 + PC_CPUID], %g2
	stx	%g2, [%g1 + KTR_PARM1]
	lduw	[%l1 + PC_MID], %g2
	stx	%g2, [%g1 + KTR_PARM2]
	stx	%l1, [%g1 + KTR_PARM3]
	stx	%sp, [%g1 + KTR_PARM5]
9:
#endif

	/*
	 * And away we go.  This doesn't return.
	 */
	call	cpu_mp_bootstrap
	 mov	%l1, %o0
	sir
	! NOTREACHED
END(mp_startup)
