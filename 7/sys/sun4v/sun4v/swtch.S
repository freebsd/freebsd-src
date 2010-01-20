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
__FBSDID("$FreeBSD$");

#include <machine/asi.h>
#include <machine/asmacros.h>
#include <machine/fsr.h>
#include <machine/hypervisorvar.h>
#include <machine/ktr.h>
#include <machine/pcb.h>
#include <machine/tstate.h>

#include "assym.s"

	.register	%g2, #ignore
	.register	%g3, #ignore

#define	PCB_REG	%g6

#define	MEMBAR_PRE	membar	#LoadStore|#StoreStore
#define MEMBAR_POST	membar	#LoadLoad

#define ATOMIC_CLEAR_INT_BIT(addr, old, bit, new, label) \
	MEMBAR_PRE		; \
	ld	[addr], old	; \
label:	andn	old, bit, new	; \
	cas	[addr], old, new ; \
	cmp	old, new	; \
	bne,a,pn	%icc, label ## b ; \
	  ld	[addr], old	; \
	MEMBAR_POST
	
#define ATOMIC_SET_INT_BIT(addr, old, bit, new, label) \
	MEMBAR_PRE		; \
	ld	[addr], old	; \
label:	or	old, bit, new	; \
	cas	[addr], old, new ; \
	cmp	old, new	; \
	bne,a,pn	%icc, label ## b ; \
	  ld	[addr], old	; \
	MEMBAR_POST	
/*
 * void cpu_throw(struct thread *old, struct thread *new)
 */
ENTRY(cpu_throw)
	save	%sp, -CCFSZ, %sp
	flushw
	ba	%xcc, .Lsw1
	 mov	%i1, %i0
END(cpu_throw)
	
/*
 * void cpu_switch(struct thread *old, struct thread *new)
 */
ENTRY(cpu_switch)
	save	%sp, -CCFSZ, %sp
	ldx	[PCPU(CURPCB)], PCB_REG
	mov	%i1, %i0

	/*
	 * If the current thread was using floating point in the kernel, save
	 * its context.  The userland floating point context has already been
	 * saved in that case.
	 */
	rd	%fprs, %l2
	andcc	%l2, FPRS_FEF, %g0
	bz,a,pt %xcc, 1f
	 nop
	call	savefpctx
	 add	PCB_REG, PCB_KFP, %o0
	ba,a	%xcc, 2f
	 nop

	/*
	 * If the current thread was using floating point in userland, save
	 * its context.
	 */
1:	sub	PCB_REG, TF_SIZEOF, %l2
	ldx	[%l2 + TF_FPRS], %l3
	andcc	%l3, FPRS_FEF, %g0
	bz,a,pt	%xcc, 2f
	 nop
	call	savefpctx
	 add	PCB_REG, PCB_UFP, %o0
	andn	%l3, FPRS_FEF, %l3
	stx	%l3, [%l2 + TF_FPRS]

	ldx	[PCB_REG + PCB_FLAGS], %l3
	or	%l3, PCB_FEF, %l3
	stx	%l3, [PCB_REG + PCB_FLAGS]

	/*
	 * Flush the windows out to the stack and save the current frame
	 * pointer and program counter.
	 */
2:	flushw
	stx	%fp, [PCB_REG + PCB_SP]
	stx	%i7, [PCB_REG + PCB_PC]


	/*
	 * Load the new thread's frame pointer and program counter, and set
	 * the current thread and pcb.
	 */
.Lsw1:
#if KTR_COMPILE & KTR_PROC & 0
	CATR(KTR_PROC, "cpu_switch: new td=%p pc=%#lx fp=%#lx"
	    , %g1, %g2, %g3, 7, 8, 9)
	stx	%i0, [%g1 + KTR_PARM1]
	ldx	[%i0 + TD_PCB], %g2
	ldx	[%g2 + PCB_PC], %g3
	stx	%g3, [%g1 + KTR_PARM2]
	ldx	[%g2 + PCB_SP], %g3
	stx	%g3, [%g1 + KTR_PARM3]
9:
#endif
	
	ldx	[%i0 + TD_PCB], %i1

	stx	%i0, [PCPU(CURTHREAD)]
	stx	%i1, [PCPU(CURPCB)]

	mov	%i1, PCB_REG				! load in new PCB

	ldx	[PCB_REG + PCB_SP], %fp
	ldx	[PCB_REG + PCB_PC], %i7
	sub	%fp, CCFSZ, %sp

 	/*
	 * Point to the pmaps of the new process, and of the last non-kernel
	 * process to run.
	 */
	ldx	[%i0 + TD_PROC], %i2
	ldx	[PCPU(PMAP)], %l2
	ldx	[%i2 + P_VMSPACE], %i5
	add	%i5, VM_PMAP, %i2

#if KTR_COMPILE & KTR_PROC & 0
	CATR(KTR_PROC, "cpu_switch: new pmap=%p old pmap=%p"
	    , %g1, %g2, %g3, 7, 8, 9)
	stx	%i2, [%g1 + KTR_PARM1]
	stx	%l2, [%g1 + KTR_PARM2]
9:
#endif
	/*
	 * If they are the same we are done.
	 */
	cmp	%l2, %i2			! current pmap == new pmap?
	be,a,pn %xcc, 5f
	  nop
	ldx	[%i2 + PM_CONTEXT], %l5		! new context
	ldx	[%l2 + PM_CONTEXT], %l3		! old context
	cmp	%g0, %l5
	be,pn	%xcc, 5f			! new context == kernel?
	  ld	[PCPU(CPUMASK)], %l4

	brz	%l3, 10f			! old context == kernel?
	  nop
	
	/*
	 * Mark the old pmap as no longer active on this CPU
	 */
	add	%l2, PM_ACTIVE, %l2
	ATOMIC_CLEAR_INT_BIT(%l2, %l3, %l4, %l6, 8)	
	
10:
	add	%i2,  PM_ACTIVE, %i3
	ATOMIC_SET_INT_BIT(%i3, %l3, %l4, %l6, 11)

	add	%i2, PM_TLBACTIVE, %i3
	ATOMIC_SET_INT_BIT(%i3, %l3, %l4, %l6, 12)
	
	mov	SCRATCH_REG_HASH_USER, %l6
	mov	SCRATCH_REG_TSB_USER, %l7
	ldx	[%i2 + PM_HASHSCRATCH], %l3	
	ldx	[%i2 + PM_TSBSCRATCH], %l4
	stx	%i2, [PCPU(PMAP)]

	SET_SCRATCH(%l6,%l3)			! XXX we're assuming the 
	SET_SCRATCH(%l7,%l4)			! scratch values <= 32 bits


	ldx	[%i2 + PM_TSB_RA], %l3

	mov	MMU_CID_S, %l6

	mov	1, %o0
	mov	%l3, %o1
	mov	MMU_TSB_CTXNON0, %o5
	ta	FAST_TRAP
	cmp	%g0, %o0
	be	%xcc, 4f
	  nop
	MAGIC_TRAP_ON
	MAGIC_EXIT
	
4:	/*
	 * install the new secondary context number in the cpu.
	 */
	
	SET_MMU_CONTEXT(%l6, %l5)
	membar #Sync
	/*
	 * Done.  Return and load the new process's window from the stack.
	 */
5:	ret
	 restore
END(cpu_switch)

ENTRY(savectx)
	save	%sp, -CCFSZ, %sp
	flushw
	call	savefpctx
	 add	%i0, PCB_UFP, %o0
	stx	%fp, [%i0 + PCB_SP]
	stx	%i7, [%i0 + PCB_PC]
	ret
	 restore %g0, 0, %o0
END(savectx)

/*
 * void savefpctx(uint32_t *);
 */
ENTRY(savefpctx)
	wr	%g0, FPRS_FEF, %fprs
	std	%f0, [%o0 + (0 * 64)] 
	std	%f16, [%o0 + (1 * 64)] 
	std	%f32, [%o0 + (2 * 64)] 
	std	%f48, [%o0 + (3 * 64)] 
	retl
	 wr	%g0, 0, %fprs
END(savefpctx)
