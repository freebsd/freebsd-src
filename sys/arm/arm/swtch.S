/*	$NetBSD: cpuswitch.S,v 1.41 2003/11/15 08:44:18 scw Exp $	*/

/*-
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
/*-
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

#include "assym.s"
#include "opt_sched.h"

#include <machine/acle-compat.h>
#include <machine/asm.h>
#include <machine/asmacros.h>
#include <machine/armreg.h>
#include <machine/vfp.h>

__FBSDID("$FreeBSD$");

#if __ARM_ARCH >= 6 && defined(SMP)
#define GET_PCPU(tmp, tmp2) \
	mrc 	p15, 0, tmp, c0, c0, 5;	\
	and	tmp, tmp, #0xf;		\
	ldr 	tmp2, .Lcurpcpu+4;	\
	mul 	tmp, tmp, tmp2;		\
	ldr	tmp2, .Lcurpcpu;	\
	add	tmp, tmp, tmp2;
#else

#define GET_PCPU(tmp, tmp2) \
	ldr	tmp, .Lcurpcpu
#endif

#ifdef VFP
	.fpu vfp	/* allow VFP instructions */
#endif

.Lcurpcpu:
	.word   _C_LABEL(__pcpu)
	.word	PCPU_SIZE
.Lblocked_lock:
	.word	_C_LABEL(blocked_lock)


#ifndef ARM_NEW_PMAP

#define DOMAIN_CLIENT	0x01

.Lcpufuncs:
	.word	_C_LABEL(cpufuncs)

/*
 * cpu_throw(oldtd, newtd)
 *
 * Remove current thread state, then select the next thread to run
 * and load its state.
 * r0 = oldtd
 * r1 = newtd
 */
ENTRY(cpu_throw)
	mov	r5, r1

	/*
	 * r0 = oldtd
	 * r5 = newtd
	 */

#ifdef VFP				/* This thread is dying, disable */
	bl	_C_LABEL(vfp_discard)	/* VFP without preserving state. */
#endif

	GET_PCPU(r7, r9)
	ldr	r7, [r5, #(TD_PCB)]		/* r7 = new thread's PCB */
  
	/* Switch to lwp0 context */

	ldr	r9, .Lcpufuncs
#if !defined(CPU_ARM11) && !defined(CPU_CORTEXA) && !defined(CPU_MV_PJ4B) && !defined(CPU_KRAIT)
	mov	lr, pc
	ldr	pc, [r9, #CF_IDCACHE_WBINV_ALL]
#endif
	ldr	r0, [r7, #(PCB_PL1VEC)]
	ldr	r1, [r7, #(PCB_DACR)]
	/*
	 * r0 = Pointer to L1 slot for vector_page (or NULL)
	 * r1 = lwp0's DACR
	 * r5 = lwp0
	 * r7 = lwp0's PCB
	 * r9 = cpufuncs
	 */

	/*
	 * Ensure the vector table is accessible by fixing up lwp0's L1
	 */
	cmp	r0, #0			/* No need to fixup vector table? */
	ldrne	r3, [r0]		/* But if yes, fetch current value */
	ldrne	r2, [r7, #(PCB_L1VEC)]	/* Fetch new vector_page value */
	mcr	p15, 0, r1, c3, c0, 0	/* Update DACR for lwp0's context */
	cmpne	r3, r2			/* Stuffing the same value? */
	strne	r2, [r0]		/* Store if not. */

#ifdef PMAP_INCLUDE_PTE_SYNC
	/*
	 * Need to sync the cache to make sure that last store is
	 * visible to the MMU.
	 */
	movne	r1, #4
	movne	lr, pc
	ldrne	pc, [r9, #CF_DCACHE_WB_RANGE]
#endif /* PMAP_INCLUDE_PTE_SYNC */

	/*
	 * Note: We don't do the same optimisation as cpu_switch() with
	 * respect to avoiding flushing the TLB if we're switching to
	 * the same L1 since this process' VM space may be about to go
	 * away, so we don't want *any* turds left in the TLB.
	 */

	/* Switch the memory to the new process */
	ldr	r0, [r7, #(PCB_PAGEDIR)]
	mov	lr, pc
	ldr	pc, [r9, #CF_CONTEXT_SWITCH]

	GET_PCPU(r6, r4)
	/* Hook in a new pcb */
	str	r7, [r6, #PC_CURPCB]
	/* We have a new curthread now so make a note it */
	str	r5, [r6, #PC_CURTHREAD]
#ifndef ARM_TP_ADDRESS
	mcr	p15, 0, r5, c13, c0, 4
#endif
	/* Set the new tp */
	ldr	r6, [r5, #(TD_MD + MD_TP)]
#ifdef ARM_TP_ADDRESS
	ldr	r4, =ARM_TP_ADDRESS
	str	r6, [r4]
	ldr	r6, [r5, #(TD_MD + MD_RAS_START)]
	str	r6, [r4, #4] /* ARM_RAS_START */
	ldr	r6, [r5, #(TD_MD + MD_RAS_END)]
	str	r6, [r4, #8] /* ARM_RAS_END */
#else
	mcr p15, 0, r6, c13, c0, 3
#endif
	/* Restore all the saved registers and exit */
	add	r3, r7, #PCB_R4
	ldmia	r3, {r4-r12, sp, pc}
END(cpu_throw)

/*
 * cpu_switch(oldtd, newtd, lock)
 *
 * Save the current thread state, then select the next thread to run
 * and load its state.
 * r0 = oldtd
 * r1 = newtd
 * r2 = lock (new lock for old thread)
 */
ENTRY(cpu_switch)
	/* Interrupts are disabled. */
	/* Save all the registers in the old thread's pcb. */
	ldr	r3, [r0, #(TD_PCB)]

	/* Restore all the saved registers and exit */
	add	r3, #(PCB_R4)
	stmia	r3, {r4-r12, sp, lr, pc}

	mov	r6, r2 /* Save the mutex */

	/* rem: r0 = old lwp */
	/* rem: interrupts are disabled */

	/* Process is now on a processor. */
	/* We have a new curthread now so make a note it */
	GET_PCPU(r7, r2)
	str	r1, [r7, #PC_CURTHREAD]
#ifndef ARM_TP_ADDRESS
	mcr	p15, 0, r1, c13, c0, 4
#endif

	/* Hook in a new pcb */
	ldr	r2, [r1, #TD_PCB]
	str	r2, [r7, #PC_CURPCB]

	/* Stage two : Save old context */

	/* Get the user structure for the old thread. */
	ldr	r2, [r0, #(TD_PCB)]
	mov	r4, r0 /* Save the old thread. */

#ifdef ARM_TP_ADDRESS
	/* Store the old tp; userland can change it on armv4. */
	ldr	r3, =ARM_TP_ADDRESS
	ldr	r9, [r3]
	str	r9, [r0, #(TD_MD + MD_TP)]
	ldr	r9, [r3, #4]
	str	r9, [r0, #(TD_MD + MD_RAS_START)]
	ldr	r9, [r3, #8]
	str	r9, [r0, #(TD_MD + MD_RAS_END)]

	/* Set the new tp */
	ldr	r9, [r1, #(TD_MD + MD_TP)]
	str	r9, [r3]
	ldr	r9, [r1, #(TD_MD + MD_RAS_START)]
	str	r9, [r3, #4]
	ldr	r9, [r1, #(TD_MD + MD_RAS_END)]
	str	r9, [r3, #8]
#else
	/* 
	 * Set new tp.  No need to store the old one first, userland can't 
	 * change it directly on armv6.
	 */
	ldr	r9, [r1, #(TD_MD + MD_TP)]
	mcr p15, 0, r9, c13, c0, 3
#endif
	
	/* Get the user structure for the new process in r9 */
	ldr	r9, [r1, #(TD_PCB)]

	/* rem: r2 = old PCB */
	/* rem: r9 = new PCB */
	/* rem: interrupts are enabled */

#ifdef VFP
	fmrx	r0, fpexc		/* If the VFP is enabled */
	tst	r0, #(VFPEXC_EN)	/* the current thread has */
	movne	r1, #1			/* used it, so go save */
	addne	r0, r2, #(PCB_VFPSTATE)	/* the state into the PCB */
	blne	_C_LABEL(vfp_store)	/* and disable the VFP. */
#endif

	/* r0-r3 now free! */

	/* Third phase : restore saved context */

	/* rem: r2 = old PCB */
	/* rem: r9 = new PCB */

	ldr	r5, [r9, #(PCB_DACR)]		/* r5 = new DACR */
	mov	r2, #DOMAIN_CLIENT
	cmp     r5, r2, lsl #(PMAP_DOMAIN_KERNEL * 2) /* Sw to kernel thread? */
	beq     .Lcs_context_switched        /* Yup. Don't flush cache */
	mrc	p15, 0, r0, c3, c0, 0		/* r0 = old DACR */
	/*
	 * Get the new L1 table pointer into r11.  If we're switching to
	 * an LWP with the same address space as the outgoing one, we can
	 * skip the cache purge and the TTB load.
	 *
	 * To avoid data dep stalls that would happen anyway, we try
	 * and get some useful work done in the mean time.
	 */
	mrc	p15, 0, r10, c2, c0, 0		/* r10 = old L1 */
	ldr	r11, [r9, #(PCB_PAGEDIR)]	/* r11 = new L1 */

	teq	r10, r11			/* Same L1? */
	cmpeq	r0, r5				/* Same DACR? */
	beq	.Lcs_context_switched		/* yes! */

#if !defined(CPU_ARM11) && !defined(CPU_CORTEXA) && !defined(CPU_MV_PJ4B) && !defined(CPU_KRAIT)
	/*
	 * Definately need to flush the cache.
	 */

	ldr	r1, .Lcpufuncs
	mov	lr, pc
	ldr	pc, [r1, #CF_IDCACHE_WBINV_ALL]
#endif
.Lcs_cache_purge_skipped:
	/* rem: r6 = lock */
	/* rem: r9 = new PCB */
	/* rem: r10 = old L1 */
	/* rem: r11 = new L1 */

	mov	r2, #0x00000000
	ldr	r7, [r9, #(PCB_PL1VEC)]

	/*
	 * Ensure the vector table is accessible by fixing up the L1
	 */
	cmp	r7, #0			/* No need to fixup vector table? */
	ldrne	r2, [r7]		/* But if yes, fetch current value */
	ldrne	r0, [r9, #(PCB_L1VEC)]	/* Fetch new vector_page value */
	mcr	p15, 0, r5, c3, c0, 0	/* Update DACR for new context */
	cmpne	r2, r0			/* Stuffing the same value? */
#ifndef PMAP_INCLUDE_PTE_SYNC
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

	/* Release the old thread */
	str	r6, [r4, #TD_LOCK]
#if defined(SCHED_ULE) && defined(SMP)
	ldr	r6, .Lblocked_lock
	GET_CURTHREAD_PTR(r3)
1:
	ldr	r4, [r3, #TD_LOCK]
	cmp	r4, r6
	beq	1b
#endif
	
	/* XXXSCW: Safe to re-enable FIQs here */

	/* rem: r9 = new PCB */

	/* Restore all the saved registers and exit */
	add	r3, r9, #PCB_R4
	ldmia	r3, {r4-r12, sp, pc}
END(cpu_switch)


#else /* !ARM_NEW_PMAP */
#include <machine/sysreg.h>

ENTRY(cpu_context_switch) /* QQQ: What about macro instead of function?	*/
	DSB
	mcr	CP15_TTBR0(r0)		/* set the new TTB */
	ISB
	mov	r0, #(CPU_ASID_KERNEL)
	mcr	CP15_TLBIASID(r0)	/* flush not global TLBs */
	/*
	* Flush entire Branch Target Cache because of the branch predictor
	* is not architecturally invisible. See ARM Architecture Reference
	* Manual ARMv7-A and ARMv7-R edition, page B2-1264(65), Branch
	* predictors and Requirements for branch predictor maintenance
	* operations sections.
	*
	* QQQ: The predictor is virtually addressed and holds virtual target
	*      addresses. Therefore, if mapping is changed, the predictor cache
	*      must be flushed.The flush is part of entire i-cache invalidation
	*      what is always called when code mapping is changed. So herein,
	*      it's the only place where standalone predictor flush must be
	*      executed in kernel (except self modifying code case).
	*/
	mcr   CP15_BPIALL       /* and flush entire Branch Target Cache */
	DSB
	mov	pc, lr
END(cpu_context_switch)

/*
 * cpu_throw(oldtd, newtd)
 *
 * Remove current thread state,	then select the	next thread to run
 * and load its	state.
 * r0 =	oldtd
 * r1 =	newtd
 */
ENTRY(cpu_throw)
	mov	r10, r0			/* r10 = oldtd */
	mov	r11, r1			/* r11 = newtd */

#ifdef VFP				/* This thread is dying, disable */
	bl	_C_LABEL(vfp_discard)	/* VFP without preserving state. */
#endif
	GET_PCPU(r8, r9)		/* r8 = current pcpu */
	ldr     r4, [r8, #PC_CPUID]	/* r4 = current cpu id */

	cmp	r10, #0	/* old thread? */
	beq	2f			/* no, skip */

	/* Remove this CPU from the active list. */
	ldr	r5, [r8, #PC_CURPMAP]
	mov	r0, #(PM_ACTIVE)
	add	r5, r0			/* r5 = old pm_active */

	/* Compute position and mask. */
#if _NCPUWORDS > 1
	lsr	r0, r4, #3
	bic	r0, #3
	add	r5, r0			/* r5 = position in old pm_active */
	mov	r2, #1
	and	r0, r4, #31
	lsl	r2, r0			/* r2 = mask */
#else
	mov	r2, #1
	lsl	r2, r4			/* r2 = mask */
#endif
	/* Clear cpu from old active list. */
#ifdef SMP
1:	ldrex	r0, [r5]
	bic	r0, r2
	strex	r1, r0, [r5]
	teq	r1, #0
	bne	1b
#else
	ldr	r0, [r5]
	bic	r0, r2
	str	r0, [r5]
#endif

2:
#ifdef INVARIANTS
	cmp	r11, #0			/* new thread? */
	beq	badsw1			/* no, panic */
#endif
	ldr	r7, [r11, #(TD_PCB)]	/* r7 = new PCB */

	/*
	 * Registers at this point
	 *   r4  = current cpu id
	 *   r7  = new PCB
	 *   r8  = current pcpu
	 *   r11 = newtd
	 */

	/* MMU switch to new thread. */
	ldr     r0, [r7, #(PCB_PAGEDIR)]
#ifdef INVARIANTS
	cmp	r0, #0			/* new thread? */
	beq	badsw4			/* no, panic */
#endif
	bl	_C_LABEL(cpu_context_switch)

	/*
	 * Set new PMAP as current one.
	 * Insert cpu to new active list.
	 */

	ldr	r6, [r11, #(TD_PROC)]	/* newtd->proc */
	ldr	r6, [r6, #(P_VMSPACE)]	/* newtd->proc->vmspace */
	add	r6, #VM_PMAP		/* newtd->proc->vmspace->pmap */
	str	r6, [r8, #PC_CURPMAP]	/* store to curpmap */

	mov	r0, #PM_ACTIVE
	add	r6, r0			/* r6 = new pm_active */

	/* compute position and mask */
#if _NCPUWORDS > 1
	lsr	r0, r4, #3
	bic	r0, #3
	add	r6, r0			/* r6 = position in new pm_active */
	mov	r2, #1
	and	r0, r4, #31
	lsl	r2, r0			/* r2 = mask */
#else
	mov	r2, #1
	lsl	r2, r4 			/* r2 = mask */
#endif
	/* Set cpu to new active list. */
#ifdef SMP
1:	ldrex	r0, [r6]
	orr	r0, r2
	strex	r1, r0, [r6]
	teq	r1, #0
	bne	1b
#else
	ldr	r0, [r6]
	orr	r0, r2
	str	r0, [r6]
#endif
	/*
	 * Registers at this point.
	 *   r7  = new PCB
	 *   r8  = current pcpu
	 *   r11 = newtd
	 * They must match the ones in sw1 position !!!
	 */
	DMB
	b	sw1	/* share new thread init with cpu_switch() */
END(cpu_throw)

/*
 * cpu_switch(oldtd, newtd, lock)
 *
 * Save the current thread state, then select the next thread to run
 * and load its state.
 * r0 = oldtd
 * r1 = newtd
 * r2 = lock (new lock for old thread)
 */
ENTRY(cpu_switch)
	/* Interrupts are disabled. */
#ifdef INVARIANTS
	cmp	r0, #0			/* old thread? */
	beq	badsw2			/* no, panic */
#endif
	/* Save all the registers in the old thread's pcb. */
	ldr	r3, [r0, #(TD_PCB)]
	add	r3, #(PCB_R4)
	stmia	r3, {r4-r12, sp, lr, pc}

#ifdef INVARIANTS
	cmp	r1, #0			/* new thread? */
	beq	badsw3			/* no, panic */
#endif
	/*
	 * Save arguments. Note that we can now use r0-r14 until
	 * it is time to restore them for the new thread. However,
	 * some registers are not safe over function call.
	 */
	mov	r9, r2			/* r9  = lock */
	mov	r10, r0			/* r10 = oldtd */
	mov	r11, r1			/* r11 = newtd */

	GET_PCPU(r8, r3)		/* r8  = current PCPU */
	ldr	r7, [r11, #(TD_PCB)]	/* r7 = newtd->td_pcb */



#ifdef VFP
	ldr	r3, [r10, #(TD_PCB)]
	fmrx	r0, fpexc		/* If the VFP is enabled */
	tst	r0, #(VFPEXC_EN)	/* the current thread has */
	movne	r1, #1			/* used it, so go save */
	addne	r0, r3, #(PCB_VFPSTATE)	/* the state into the PCB */
	blne	_C_LABEL(vfp_store)	/* and disable the VFP. */
#endif

	/*
	 * MMU switch. If we're switching to a thread with the same
	 * address space as the outgoing one, we can skip the MMU switch.
	 */
	mrc	CP15_TTBR0(r1)		/* r1 = old TTB */
	ldr	r0, [r7, #(PCB_PAGEDIR)] /* r0 = new TTB */
	cmp	r0, r1			/* Switching to the TTB? */
	beq	sw0			/* same TTB, skip */

#ifdef INVARIANTS
	cmp	r0, #0			/* new thread? */
	beq	badsw4			/* no, panic */
#endif

	bl	cpu_context_switch	/* new TTB as argument */

	/*
	 * Registers at this point
	 *   r7  = new PCB
	 *   r8  = current pcpu
	 *   r9  = lock
	 *   r10 = oldtd
	 *   r11 = newtd
	 */

	/*
	 * Set new PMAP as current one.
	 * Update active list on PMAPs.
	 */
	ldr	r6, [r11, #TD_PROC]	/* newtd->proc */
	ldr	r6, [r6, #P_VMSPACE]	/* newtd->proc->vmspace */
	add	r6, #VM_PMAP		/* newtd->proc->vmspace->pmap */

	ldr	r5, [r8, #PC_CURPMAP]   /* get old curpmap */
	str	r6, [r8, #PC_CURPMAP]   /* and save new one */

	mov	r0, #PM_ACTIVE
	add	r5, r0			/* r5 = old pm_active */
	add	r6, r0			/* r6 = new pm_active */

	/* Compute position and mask. */
	ldr	r4, [r8, #PC_CPUID]
#if _NCPUWORDS > 1
	lsr	r0, r4, #3
	bic	r0, #3
	add	r5, r0			/* r5 = position in old pm_active */
	add	r6, r0			/* r6 = position in new pm_active */
	mov	r2, #1
	and	r0, r4, #31
	lsl	r2, r0			/* r2 = mask */
#else
	mov	r2, #1
	lsl	r2, r4			/* r2 = mask */
#endif
	/* Clear cpu from old active list. */
#ifdef SMP
1:	ldrex	r0, [r5]
	bic	r0, r2
	strex	r1, r0, [r5]
	teq	r1, #0
	bne	1b
#else
	ldr	r0, [r5]
	bic	r0, r2
	str	r0, [r5]
#endif
	/* Set cpu to new active list. */
#ifdef SMP
1:      ldrex	r0, [r6]
	orr	r0, r2
	strex	r1, r0, [r6]
	teq	r1, #0
	bne	1b
#else
	ldr	r0, [r6]
	orr	r0, r2
	str	r0, [r6]
#endif

sw0:
	/*
	 * Registers at this point
	 *   r7  = new PCB
	 *   r8  = current pcpu
	 *   r9  = lock
	 *   r10 = oldtd
	 *   r11 = newtd
	 */

	/* Change the old thread lock. */
	add	r5, r10, #TD_LOCK
	DMB
1:	ldrex	r0, [r5]
	strex	r1, r9, [r5]
	teq	r1, #0
	bne	1b
	DMB

sw1:
	clrex
	/*
	 * Registers at this point
	 *   r7  = new PCB
	 *   r8  = current pcpu
	 *   r11 = newtd
	 */

#if defined(SMP) && defined(SCHED_ULE)
	/*
	 * 386 and amd64 do the blocked lock test only for SMP and SCHED_ULE
	 * QQQ: What does it mean in reality and why is it done?
	 */
	ldr	r6, =blocked_lock
1:
	ldr	r3, [r11, #TD_LOCK]	/* atomic write regular read */
	cmp	r3, r6
	beq	1b
#endif
	/* Set the new tls */
	ldr     r0, [r11, #(TD_MD + MD_TP)]
	mcr	CP15_TPIDRURO(r0)	/* write tls thread reg 2 */

	/* We have a new curthread now so make a note it */
	str	r11, [r8, #PC_CURTHREAD]
	mcr	CP15_TPIDRPRW(r11)

	/* store pcb in per cpu structure */
	str	r7, [r8, #PC_CURPCB]

	/*
	 * Restore all saved registers and return. Note that some saved
	 * registers can be changed when either cpu_fork(), cpu_set_upcall(),
	 * cpu_set_fork_handler(), or makectx() was called.
	 */
	add	r3, r7, #PCB_R4
	ldmia	r3, {r4-r12, sp, pc}

#ifdef INVARIANTS
badsw1:
	ldr	r0, =sw1_panic_str
	bl	_C_LABEL(panic)
1:	nop
	b       1b

badsw2:
	ldr	r0, =sw2_panic_str
	bl	_C_LABEL(panic)
1:	nop
	b	1b

badsw3:
	ldr	r0, =sw3_panic_str
	bl	_C_LABEL(panic)
1:	nop
	b	1b

badsw4:
	ldr	r0, =sw4_panic_str
	bl	_C_LABEL(panic)
1:	nop
	b	1b

sw1_panic_str:
	.asciz	"cpu_throw: no newthread supplied.\n"
sw2_panic_str:
	.asciz	"cpu_switch: no curthread supplied.\n"
sw3_panic_str:
	.asciz	"cpu_switch: no newthread supplied.\n"
sw4_panic_str:
	.asciz	"cpu_switch: new pagedir is NULL.\n"
#endif
END(cpu_switch)


#endif /* !ARM_NEW_PMAP */

ENTRY(savectx)
	stmfd	sp!, {lr}
	sub	sp, sp, #4
	
	/* Store all the registers in the thread's pcb */
	add	r3, r0, #(PCB_R4)
	stmia	r3, {r4-r12, sp, lr, pc}
#ifdef VFP
	fmrx	r2, fpexc		/* If the VFP is enabled */
	tst	r2, #(VFPEXC_EN)	/* the current thread has */
	movne	r1, #1			/* used it, so go save */
	addne	r0, r0, #(PCB_VFPSTATE)	/* the state into the PCB */
	blne	_C_LABEL(vfp_store)	/* and disable the VFP. */
#endif
	add	sp, sp, #4;
	ldmfd	sp!, {pc}
END(savectx)

ENTRY(fork_trampoline)
	STOP_UNWINDING	/* EABI: Don't unwind beyond the thread enty point. */
	mov	fp, #0	/* OABI: Stack traceback via fp stops here. */
	mov	r2, sp
	mov	r1, r5
	mov	r0, r4
	ldr	lr, =swi_exit		/* Go finish forking, then return */
	b	_C_LABEL(fork_exit)	/* to userland via swi_exit code. */
END(fork_trampoline)

