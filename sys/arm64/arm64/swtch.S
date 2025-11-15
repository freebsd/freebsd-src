/*-
 * Copyright (c) 2014 Andrew Turner
 * Copyright (c) 2014 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under sponsorship from
 * the FreeBSD Foundation.
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
 */

#include "assym.inc"
#include "opt_kstack_pages.h"
#include "opt_sched.h"

#include <sys/elf_common.h>

#include <machine/asm.h>
#include <machine/armreg.h>
#include <machine/proc.h>

.macro clear_step_flag pcbflags, tmp
	tbz	\pcbflags, #PCB_SINGLE_STEP_SHIFT, 999f
	mrs	\tmp, mdscr_el1
	bic	\tmp, \tmp, #MDSCR_SS
	msr	mdscr_el1, \tmp
	isb
999:
.endm

.macro set_step_flag pcbflags, tmp
	tbz	\pcbflags, #PCB_SINGLE_STEP_SHIFT, 999f
	mrs	\tmp, mdscr_el1
	orr	\tmp, \tmp, #MDSCR_SS
	msr	mdscr_el1, \tmp
	isb
999:
.endm

/*
 * Lower 32 bits of CONTEXTIDR_EL1 are PID
 * Upper 32 bits are reserved for future use e.g. TID
 */
.macro pid_in_context_idr
	adrp	x9, arm64_pid_in_contextidr
	ldrb	w10, [x9, :lo12:arm64_pid_in_contextidr]
	cbz	w10, 998f
	ldr	x9, [x1, #TD_PROC]
	/* PID is always 0 or positive, do not sign extend */
	ldr	w10, [x9, #P_PID]
	msr	contextidr_el1, x10
998:
.endm

/*
 * void cpu_throw(struct thread *old, struct thread *new)
 */
ENTRY(cpu_throw)
	/* Of old == NULL skip disabling stepping */
	cbz	x0, 1f

	/* If we were single stepping, disable it */
	ldr	x4, [x0, #TD_PCB]
	ldr	w5, [x4, #PCB_FLAGS]
	clear_step_flag w5, x6

1:
	/* debug/trace: set CONTEXTIDR_EL1 to current PID, if enabled */
	pid_in_context_idr

#ifdef VFP
	/* Backup the new thread pointer around a call to C code */
	mov	x19, x1
	bl	vfp_discard
	mov	x0, x19
#else
	mov	x0, x1
#endif

	/* This returns the thread pointer so no need to save it */
	bl	ptrauth_switch
#ifdef PERTHREAD_SSP
	mov	x19, x0
#endif
	/* This returns the thread pcb */
	bl	pmap_switch
	mov	x4, x0
#ifdef PERTHREAD_SSP
	/* Update the per-thread stack canary pointer. */
	add	x19, x19, #(TD_MD_CANARY)
	msr	sp_el0, x19
#endif

	/* If we are single stepping, enable it */
	ldr	w5, [x4, #PCB_FLAGS]
	set_step_flag w5, x6

	/* Restore the registers */
	ldp	x5, x6, [x4, #PCB_SP]
	mov	sp, x5
	msr	tpidr_el0, x6
	ldr	x6, [x4, #PCB_TPIDRRO]
	msr	tpidrro_el0, x6
	ldp	x19, x20, [x4, #PCB_REGS + (PCB_X19 + 0) * 8]
	ldp	x21, x22, [x4, #PCB_REGS + (PCB_X19 + 2) * 8]
	ldp	x23, x24, [x4, #PCB_REGS + (PCB_X19 + 4) * 8]
	ldp	x25, x26, [x4, #PCB_REGS + (PCB_X19 + 6) * 8]
	ldp	x27, x28, [x4, #PCB_REGS + (PCB_X19 + 8) * 8]
	ldp	x29, lr, [x4, #PCB_REGS + (PCB_X19 + 10) * 8]

	ret
END(cpu_throw)

/*
 * void cpu_switch(struct thread *old, struct thread *new, struct mtx *mtx)
 *
 * x0 = old
 * x1 = new
 * x2 = mtx
 * x3 to x7, x16 and x17 are caller saved
 */
ENTRY(cpu_switch)
	/*
	 * Save the old context.
	 */
	ldr	x4, [x0, #TD_PCB]

	/* Store the callee-saved registers */
	stp	x19, x20, [x4, #PCB_REGS + (PCB_X19 + 0) * 8]
	stp	x21, x22, [x4, #PCB_REGS + (PCB_X19 + 2) * 8]
	stp	x23, x24, [x4, #PCB_REGS + (PCB_X19 + 4) * 8]
	stp	x25, x26, [x4, #PCB_REGS + (PCB_X19 + 6) * 8]
	stp	x27, x28, [x4, #PCB_REGS + (PCB_X19 + 8) * 8]
	stp	x29, lr, [x4, #PCB_REGS + (PCB_X19 + 10) * 8]
	/* And the old stack pointer */
	mov	x5, sp
	mrs	x6, tpidrro_el0
	str	x6, [x4, #PCB_TPIDRRO]
	mrs	x6, tpidr_el0
	stp	x5, x6, [x4, #PCB_SP]

	/* If we were single stepping, disable it */
	ldr	w5, [x4, #PCB_FLAGS]
	clear_step_flag w5, x6

	mov	x19, x0
	mov	x20, x1
	mov	x21, x2

	/* debug/trace: set CONTEXTIDR_EL1 to current PID, if enabled */
	pid_in_context_idr

#ifdef VFP
	bl	vfp_save_state_switch
	mov	x0, x20
#else
	mov	x0, x1
#endif

	/* This returns the thread pointer so no need to save it */
	bl	ptrauth_switch
	/* This returns the thread pcb */
	bl	pmap_switch
	/* Move the new pcb out of the way */
	mov	x4, x0

	mov	x2, x21
	mov	x1, x20
	mov	x0, x19
#ifdef PERTHREAD_SSP
	/* Update the per-thread stack canary pointer. */
	add	x20, x20, #(TD_MD_CANARY)
	msr	sp_el0, x20
#endif

	/*
	 * Release the old thread.
	 */
	stlr	x2, [x0, #TD_LOCK]
#if defined(SCHED_ULE) && defined(SMP)
	/* Spin if TD_LOCK points to a blocked_lock */
	ldr	x2, =_C_LABEL(blocked_lock)
1:
	ldar	x3, [x1, #TD_LOCK]
	cmp	x3, x2
	b.eq	1b
#endif

	/* If we are single stepping, enable it */
	ldr	w5, [x4, #PCB_FLAGS]
	set_step_flag w5, x6

	/* Restore the registers */
	ldp	x5, x6, [x4, #PCB_SP]
	mov	sp, x5
	msr	tpidr_el0, x6
	ldr	x6, [x4, #PCB_TPIDRRO]
	msr	tpidrro_el0, x6
	ldp	x19, x20, [x4, #PCB_REGS + (PCB_X19 + 0) * 8]
	ldp	x21, x22, [x4, #PCB_REGS + (PCB_X19 + 2) * 8]
	ldp	x23, x24, [x4, #PCB_REGS + (PCB_X19 + 4) * 8]
	ldp	x25, x26, [x4, #PCB_REGS + (PCB_X19 + 6) * 8]
	ldp	x27, x28, [x4, #PCB_REGS + (PCB_X19 + 8) * 8]
	ldp	x29, lr, [x4, #PCB_REGS + (PCB_X19 + 10) * 8]

	ret
END(cpu_switch)

ENTRY(fork_trampoline)
	mov	x0, x19
	mov	x1, x20
	mov	x2, sp
	mov	fp, #0	/* Stack traceback stops here. */
	bl	_C_LABEL(fork_exit)

	/*
	 * Disable interrupts as we are setting userspace specific
	 * state that we won't handle correctly in an interrupt while
	 * in the kernel.
	 */
	msr	daifset, #(DAIF_D | DAIF_INTR)

	ldr	x0, [x18, #PC_CURTHREAD]

	/* Set the per-process tcr_el1 fields */
	ldr	x10, [x0, #TD_PROC]
	ldr	x10, [x10, #P_MD_TCR]
	mrs	x11, tcr_el1
	and	x11, x11, #(~MD_TCR_FIELDS)
	orr	x11, x11, x10
	msr	tcr_el1, x11
	/* No isb as the eret below is the context-synchronising event */

	bl	ptrauth_enter_el0

	/* Restore sp, lr, elr, and spsr */
	ldp	x18, lr, [sp, #TF_SP]
	ldp	x10, x11, [sp, #TF_ELR]
	msr	sp_el0, x18
	msr	spsr_el1, x11
	msr	elr_el1, x10

	/* Restore the CPU registers */
	ldp	x0, x1, [sp, #TF_X + 0 * 8]
	ldp	x2, x3, [sp, #TF_X + 2 * 8]
	ldp	x4, x5, [sp, #TF_X + 4 * 8]
	ldp	x6, x7, [sp, #TF_X + 6 * 8]
	ldp	x8, x9, [sp, #TF_X + 8 * 8]
	ldp	x10, x11, [sp, #TF_X + 10 * 8]
	ldp	x12, x13, [sp, #TF_X + 12 * 8]
	ldp	x14, x15, [sp, #TF_X + 14 * 8]
	ldp	x16, x17, [sp, #TF_X + 16 * 8]
	ldp	x18, x19, [sp, #TF_X + 18 * 8]
	ldp	x20, x21, [sp, #TF_X + 20 * 8]
	ldp	x22, x23, [sp, #TF_X + 22 * 8]
	ldp	x24, x25, [sp, #TF_X + 24 * 8]
	ldp	x26, x27, [sp, #TF_X + 26 * 8]
	ldp	x28, x29, [sp, #TF_X + 28 * 8]

	/*
	 * No need for interrupts reenabling since PSR
	 * will be set to the desired value anyway.
	 */
	ERET
	
END(fork_trampoline)

ENTRY(savectx)
	/* Store the callee-saved registers */
	stp	x19, x20, [x0, #PCB_REGS + (PCB_X19 + 0) * 8]
	stp	x21, x22, [x0, #PCB_REGS + (PCB_X19 + 2) * 8]
	stp	x23, x24, [x0, #PCB_REGS + (PCB_X19 + 4) * 8]
	stp	x25, x26, [x0, #PCB_REGS + (PCB_X19 + 6) * 8]
	stp	x27, x28, [x0, #PCB_REGS + (PCB_X19 + 8) * 8]
	stp	x29, lr, [x0, #PCB_REGS + (PCB_X19 + 10) * 8]
	/* And the old stack pointer */
	mov	x5, sp
	mrs	x6, tpidrro_el0
	str	x6, [x0, #PCB_TPIDRRO]
	mrs	x6, tpidr_el0
	stp	x5, x6, [x0, #PCB_SP]

#ifdef VFP
	/* Store the VFP registers */
	b	vfp_save_state_savectx
#else
	ret
#endif
END(savectx)

GNU_PROPERTY_AARCH64_FEATURE_1_NOTE(GNU_PROPERTY_AARCH64_FEATURE_1_VAL)
