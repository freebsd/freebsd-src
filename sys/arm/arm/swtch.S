/*	$NetBSD: cpuswitch.S,v 1.41 2003/11/15 08:44:18 scw Exp $	*/

/*
 * Copyright 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
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
 */
/*
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * cpuswitch.S
 *
 * cpu switching functions
 *
 * Created      : 15/10/94
 *
 */

#include <machine/asm.h>
#include <machine/asmacros.h>
#include <machine/armreg.h>
__FBSDID("$FreeBSD$");

#include "assym.s"

/*
 * New experimental definitions of IRQdisable and IRQenable
 * These keep FIQ's enabled since FIQ's are special.
 */

#define DOMAIN_CLIENT	0x01
#define IRQdisable \
	mrs	r14, cpsr ; \
	orr	r14, r14, #(I32_bit) ; \
	msr	cpsr_c, r14 ; \

#define IRQenable \
	mrs	r14, cpsr ; \
	bic	r14, r14, #(I32_bit) ; \
	msr	cpsr_c, r14 ; \

/*
 * These are used for switching the translation table/DACR.
 * Since the vector page can be invalid for a short time, we must
 * disable both regular IRQs *and* FIQs.
 *
 * XXX: This is not necessary if the vector table is relocated.
 */
#define IRQdisableALL \
	mrs	r14, cpsr ; \
	orr	r14, r14, #(I32_bit | F32_bit) ; \
	msr	cpsr_c, r14

#define IRQenableALL \
	mrs	r14, cpsr ; \
	bic	r14, r14, #(I32_bit | F32_bit) ; \
	msr	cpsr_c, r14
	
.Lpcpu:
	.word	_C_LABEL(__pcpu)
.Lcurthread:
	.word 	_C_LABEL(__pcpu) + PC_CURTHREAD
.Lcurpcb:
	.word	_C_LABEL(__pcpu) + PC_CURPCB
.Lcpufuncs:	
	.word	_C_LABEL(cpufuncs)
.Lblock_userspace_access:
	.word	_C_LABEL(block_userspace_access)

.Lcpu_do_powersave:
	.word	_C_LABEL(cpu_do_powersave)

.Lpmap_kernel_cstate:
	.word	(kernel_pmap_store + PMAP_CSTATE)

.Llast_cache_state_ptr:
	.word	_C_LABEL(pmap_cache_state)

/* XXX: wow */
ENTRY(cpu_throw)
ENTRY(cpu_switch)
	stmfd	sp!, {r4-r7, lr}
	mov	r6, r1
	mov	r1, r0

	.Lswitch_resume:
	/* rem: r1 = old lwp */
	/* rem: r4 = return value [not used if came from cpu_switchto()] */
	/* rem: r6 = new process */
	/* rem: interrupts are disabled */

#ifdef MULTIPROCESSOR
	/* XXX use curcpu() */
	ldr	r0, .Lcpu_info_store
	str	r0, [r6, #(L_CPU)]
#else
	/* l->l_cpu initialized in fork1() for single-processor */
#endif

	/* Process is now on a processor. */

	/* We have a new curlwp now so make a note it */
	ldr	r7, .Lcurthread
	str	r6, [r7]

	/* Hook in a new pcb */
	ldr	r7, .Lcurpcb
	ldr	r0, [r6, #(TD_PCB)]
	str	r0, [r7]

	/* At this point we can allow IRQ's again. */
	/* rem: r1 = old lwp */
	/* rem: r4 = return value */
	/* rem: r6 = new process */
	/* rem: interrupts are enabled */

	/* Remember the old lwp in r0 */
	mov	r0, r1

	/*
	 * If the old lwp on entry to cpu_switch was zero then the
	 * process that called it was exiting. This means that we do
	 * not need to save the current context. Instead we can jump
	 * straight to restoring the context for the new process.
	 */
	teq	r0, #0x00000000
	beq	.Lswitch_exited

	/* rem: r0 = old lwp */
	/* rem: r4 = return value */
	/* rem: r6 = new process */
	/* rem: interrupts are enabled */

	/* Stage two : Save old context */

	/* Get the user structure for the old lwp. */
	ldr	r1, [r0, #(TD_PCB)]

	/* Save all the registers in the old lwp's pcb */
#ifndef __XSCALE__
	add	r7, r1, #(PCB_R8)
	stmia	r7, {r8-r13}
#else
	strd	r8, [r1, #(PCB_R8)]
	strd	r10, [r1, #(PCB_R10)]
	strd	r12, [r1, #(PCB_R12)]
#endif

	/*
	 * NOTE: We can now use r8-r13 until it is time to restore
	 * them for the new process.
	 */

	/* Remember the old PCB. */
	mov	r8, r1

	/* r1 now free! */

	/* Get the user structure for the new process in r9 */
	ldr	r9, [r6, #(TD_PCB)]

	/*
	 * This can be optimised... We know we want to go from SVC32
	 * mode to UND32 mode
	 */
        mrs	r3, cpsr
	bic	r2, r3, #(PSR_MODE)
	orr	r2, r2, #(PSR_UND32_MODE | I32_bit)
        msr	cpsr_c, r2

	str	sp, [r8, #(PCB_UND_SP)]

        msr	cpsr_c, r3		/* Restore the old mode */

	/* rem: r0 = old lwp */
	/* rem: r4 = return value */
	/* rem: r6 = new process */
	/* rem: r8 = old PCB */
	/* rem: r9 = new PCB */
	/* rem: interrupts are enabled */

	/* What else needs to be saved  Only FPA stuff when that is supported */

	/* Third phase : restore saved context */

	/* rem: r0 = old lwp */
	/* rem: r4 = return value */
	/* rem: r6 = new lwp */
	/* rem: r8 = old PCB */
	/* rem: r9 = new PCB */
	/* rem: interrupts are enabled */

	/*
	 * Get the new L1 table pointer into r11.  If we're switching to
	 * an LWP with the same address space as the outgoing one, we can
	 * skip the cache purge and the TTB load.
	 *
	 * To avoid data dep stalls that would happen anyway, we try
	 * and get some useful work done in the mean time.
	 */
	ldr	r10, [r8, #(PCB_PAGEDIR)]	/* r10 = old L1 */
	ldr	r11, [r9, #(PCB_PAGEDIR)]	/* r11 = new L1 */



	ldr	r0, [r8, #(PCB_DACR)]		/* r0 = old DACR */
	ldr	r1, [r9, #(PCB_DACR)]		/* r1 = new DACR */
	ldr	r8, [r9, #(PCB_CSTATE)]		/* r8 = &new_pmap->pm_cstate */
	ldr	r5, .Llast_cache_state_ptr	/* Previous thread's cstate */

	teq	r10, r11			/* Same L1? */
	ldr	r5, [r5]
	cmpeq	r0, r1				/* Same DACR? */
	beq	.Lcs_context_switched		/* yes! */
	ldr	r3, .Lblock_userspace_access
	mov	r12, #0
	cmp	r5, #0				/* No last vm? (switch_exit) */
	beq	.Lcs_cache_purge_skipped	/* No, we can skip cache flsh */

	mov	r2, #DOMAIN_CLIENT
	cmp	r1, r2, lsl #(PMAP_DOMAIN_KERNEL * 2) /* Sw to kernel thread? */
	beq	.Lcs_cache_purge_skipped	/* Yup. Don't flush cache */

	cmp	r5, r8				/* Same userland VM space? */
	ldrneb	r12, [r5, #(CS_CACHE_ID)]	/* Last VM space cache state */

	/*
	 * We're definately switching to a new userland VM space,
	 * and the previous userland VM space has yet to be flushed
	 * from the cache/tlb.
	 *
	 * r12 holds the previous VM space's cs_cache_id state
	 */
	tst	r12, #0xff			/* Test cs_cache_id */
	beq	.Lcs_cache_purge_skipped	/* VM space is not in cache */

	/*
	 * Definately need to flush the cache.
	 * Mark the old VM space as NOT being resident in the cache.
	 */
	mov	r2, #0x00000000
	strb	r2, [r5, #(CS_CACHE_ID)]
	strb	r2, [r5, #(CS_CACHE_D)]

	/*
	 * Don't allow user space access between the purge and the switch.
	 */
	mov	r2, #0x00000001
	str	r2, [r3]

	stmfd	sp!, {r0-r3}
	ldr	r1, .Lcpufuncs
	mov	lr, pc
	ldr	pc, [r1, #CF_IDCACHE_WBINV_ALL]
	ldmfd	sp!, {r0-r3}

.Lcs_cache_purge_skipped:
	/* rem: r1 = new DACR */
	/* rem: r3 = &block_userspace_access */
	/* rem: r4 = return value */
	/* rem: r5 = &old_pmap->pm_cstate (or NULL) */
	/* rem: r6 = new lwp */
	/* rem: r8 = &new_pmap->pm_cstate */
	/* rem: r9 = new PCB */
	/* rem: r10 = old L1 */
	/* rem: r11 = new L1 */

	mov	r2, #0x00000000
	ldr	r7, [r9, #(PCB_PL1VEC)]

	/*
	 * At this point we need to kill IRQ's again.
	 *
	 * XXXSCW: Don't need to block FIQs if vectors have been relocated
	 */
#if 0
	IRQdisableALL
#endif

	/*
	 * Interrupts are disabled so we can allow user space accesses again
	 * as none will occur until interrupts are re-enabled after the
	 * switch.
	 */
	str	r2, [r3]

	/*
	 * Ensure the vector table is accessible by fixing up the L1
	 */
	cmp	r7, #0			/* No need to fixup vector table? */
	ldrne	r2, [r7]		/* But if yes, fetch current value */
	ldrne	r0, [r9, #(PCB_L1VEC)]	/* Fetch new vector_page value */
	mcr	p15, 0, r1, c3, c0, 0	/* Update DACR for new context */
	cmpne	r2, r0			/* Stuffing the same value? */
#if 0
	strne	r0, [r7]		/* Nope, update it */
#else
	beq	.Lcs_same_vector
	str	r0, [r7]		/* Otherwise, update it */

	/*
	 * Need to sync the cache to make sure that last store is
	 * visible to the MMU.
	 */
	ldr	r2, .Lcpufuncs
	mov	r0, r7
	mov	r1, #4
	mov	lr, pc
	ldr	pc, [r2, #CF_DCACHE_WB_RANGE]

.Lcs_same_vector:
#endif /* PMAP_INCLUDE_PTE_SYNC */

	cmp	r10, r11		/* Switching to the same L1? */
	ldr	r10, .Lcpufuncs
	beq	.Lcs_same_l1		/* Yup. */
	/*
	 * Do a full context switch, including full TLB flush.
	 */
	mov	r0, r11
	mov	lr, pc
	ldr	pc, [r10, #CF_CONTEXT_SWITCH]

	/*
	 * Mark the old VM space as NOT being resident in the TLB
	 */
	mov	r2, #0x00000000
	cmp	r5, #0
	strneh	r2, [r5, #(CS_TLB_ID)]
	b	.Lcs_context_switched

	/*
	 * We're switching to a different process in the same L1.
	 * In this situation, we only need to flush the TLB for the
	 * vector_page mapping, and even then only if r7 is non-NULL.
	 */
.Lcs_same_l1:
	cmp	r7, #0
	movne	r0, #0			/* We *know* vector_page's VA is 0x0 */
	movne	lr, pc
	ldrne	pc, [r10, #CF_TLB_FLUSHID_SE]

.Lcs_context_switched:
	/* rem: r8 = &new_pmap->pm_cstate */

	/* XXXSCW: Safe to re-enable FIQs here */

	/*
	 * The new VM space is live in the cache and TLB.
	 * Update its cache/tlb state, and if it's not the kernel
	 * pmap, update the 'last cache state' pointer.
	 */
	mov	r2, #-1
	ldr	r5, .Lpmap_kernel_cstate
	ldr	r0, .Llast_cache_state_ptr
	str	r2, [r8, #(CS_ALL)]
	cmp	r5, r8
	strne	r8, [r0]

	/* rem: r4 = return value */
	/* rem: r6 = new lwp */
	/* rem: r9 = new PCB */

	/*
	 * This can be optimised... We know we want to go from SVC32
	 * mode to UND32 mode
	 */
        mrs	r3, cpsr
	bic	r2, r3, #(PSR_MODE)
	orr	r2, r2, #(PSR_UND32_MODE)
        msr	cpsr_c, r2

	ldr	sp, [r9, #(PCB_UND_SP)]

        msr	cpsr_c, r3		/* Restore the old mode */

	/* Restore all the save registers */
#ifndef __XSCALE__
	add	r7, r9, #PCB_R8
	ldmia	r7, {r8-r13}
	sub	r7, r7, #PCB_R8		/* restore PCB pointer */
#else
	mov	r7, r9
	ldr	r8, [r7, #(PCB_R8)]
	ldr	r9, [r7, #(PCB_R9)]
	ldr	r10, [r7, #(PCB_R10)]
	ldr	r11, [r7, #(PCB_R11)]
	ldr	r12, [r7, #(PCB_R12)]
	ldr	r13, [r7, #(PCB_SP)]
#endif

	ldr	r5, [r6, #(TD_PROC)]	/* fetch the proc for below */

	/* rem: r4 = return value */
	/* rem: r5 = new lwp's proc */
	/* rem: r6 = new lwp */
	/* rem: r7 = new pcb */

#ifdef ARMFPE
	add	r0, r7, #(USER_SIZE) & 0x00ff
	add	r0, r0, #(USER_SIZE) & 0xff00 
	bl	_C_LABEL(arm_fpe_core_changecontext)
#endif

	/* We can enable interrupts again */
#if 0
	IRQenableALL
#endif
	/* rem: r4 = return value */
	/* rem: r5 = new lwp's proc */
	/* rem: r6 = new lwp */
	/* rem: r7 = new PCB */

.Lswitch_return:

	/*
	 * Pull the registers that got pushed when either savectx() or
	 * cpu_switch() was called and return.
	 */
	ldmfd	sp!, {r4-r7, pc}
.Lswitch_exited:
	/*
	 * We skip the cache purge because switch_exit() already did it.
	 * Load up registers the way .Lcs_cache_purge_skipped expects.
	 * Userpsace access already blocked by switch_exit().
	 */
	ldr	r9, [r6, #(TD_PCB)]		/* r9 = new PCB */
	ldr	r3, .Lblock_userspace_access
	mrc	p15, 0, r10, c2, c0, 0		/* r10 = old L1 */
	mov	r5, #0				/* No previous cache state */
	ldr	r1, [r9, #(PCB_DACR)]		/* r1 = new DACR */
	ldr	r8, [r9, #(PCB_CSTATE)]		/* r8 = new cache state */
	ldr	r11, [r9, #(PCB_PAGEDIR)]	/* r11 = new L1 */
	b	.Lcs_cache_purge_skipped
#ifdef DIAGNOSTIC
.Lswitch_bogons:
	adr	r0, .Lswitch_panic_str
	bl	_C_LABEL(panic)
1:	nop
	b	1b

.Lswitch_panic_str:
	.asciz	"cpu_switch: sched_qs empty with non-zero sched_whichqs!\n"
#endif
ENTRY(savectx)
	mov	pc, lr
ENTRY(fork_trampoline)
	mov	r1, r5
	mov	r2, sp
	mov	r0, r4
	mov	lr, pc
	#if 0
	mov	r2, sp
	#endif
	#if 0
	mov	pc, r4
	#endif
	bl	_C_LABEL(fork_exit)
	/* Kill irq's */
        mrs     r0, cpsr
        orr     r0, r0, #(I32_bit)
        msr     cpsr_c, r0

	PULLFRAME

	movs	pc, lr			/* Exit */

#ifndef __XSCALE__
	.type .Lcpu_switch_ffs_table, _ASM_TYPE_OBJECT;
.Lcpu_switch_ffs_table:
/* same as ffs table but all nums are -1 from that */
/*               0   1   2   3   4   5   6   7           */
	.byte	 0,  0,  1, 12,  2,  6,  0, 13  /*  0- 7 */
	.byte	 3,  0,  7,  0,  0,  0,  0, 14  /*  8-15 */
	.byte	10,  4,  0,  0,  8,  0,  0, 25  /* 16-23 */
	.byte	 0,  0,  0,  0,  0, 21, 27, 15  /* 24-31 */
	.byte	31, 11,  5,  0,  0,  0,  0,  0	/* 32-39 */
	.byte	 9,  0,  0, 24,  0,  0, 20, 26  /* 40-47 */
	.byte	30,  0,  0,  0,  0, 23,  0, 19  /* 48-55 */
	.byte   29,  0, 22, 18, 28, 17, 16,  0  /* 56-63 */
#endif	/* !__XSCALE_ */
