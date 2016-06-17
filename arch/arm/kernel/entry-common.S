/*
 *  linux/arch/arm/kernel/entry-common.S
 *
 *  Copyright (C) 2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/config.h>
#include "entry-header.S"

/* 
 * We rely on the fact that R0 is at the bottom of the stack (due to
 * slow/fast restore user regs).
 */
#if S_R0 != 0
#error "Please fix"
#endif

/*
 * Our do_softirq out of line code.  See include/asm-arm/softirq.h for
 * the calling assembly.
 */
ENTRY(__do_softirq)
	stmfd	sp!, {r0 - r3, ip, lr}
	bl	do_softirq
	ldmfd	sp!, {r0 - r3, ip, pc}

	.align	5
/*
 * This is the fast syscall return path.  We do as little as
 * possible here, and this includes saving r0 back into the SVC
 * stack.
 */
ret_fast_syscall:
	disable_irq r1				@ ensure IRQs are disabled
	ldr	r1, [tsk, #TSK_NEED_RESCHED]
	ldr	r2, [tsk, #TSK_SIGPENDING]
	teq	r1, #0				@ need_resched || sigpending
	teqeq	r2, #0
	bne	slow
	fast_restore_user_regs

/*
 * Ok, we need to do extra processing, enter the slow path.
 */
slow:	str	r0, [sp, #S_R0+S_OFF]!	@ returned r0
	teq	r1, #0
	beq	1f

/*
 * "slow" syscall return path.  "why" tells us if this was a real syscall.
 */
reschedule:
	bl	SYMBOL_NAME(schedule)
ret_disable_irq:
	disable_irq r1				@ ensure IRQs are disabled
ENTRY(ret_to_user)
ret_slow_syscall:
	ldr	r1, [tsk, #TSK_NEED_RESCHED]
	ldr	r2, [tsk, #TSK_SIGPENDING]
	teq	r1, #0				@ need_resched => schedule()
	bne	reschedule
1:	teq	r2, #0				@ sigpending => do_signal()
	bne	__do_signal
restore:
	restore_user_regs

__do_signal:
	enable_irq r1
	mov	r0, #0				@ NULL 'oldset'
	mov	r1, sp				@ 'regs'
	mov	r2, why				@ 'syscall'
	bl	SYMBOL_NAME(do_signal)		@ note the bl above sets lr
	disable_irq r1				@ ensure IRQs are disabled
	b	restore

/*
 * This is how we return from a fork.  __switch_to will be calling us
 * with r0 pointing at the previous task that was running (ready for
 * calling schedule_tail).
 */
ENTRY(ret_from_fork)
	bl	SYMBOL_NAME(schedule_tail)
	get_current_task tsk
	ldr	ip, [tsk, #TSK_PTRACE]		@ check for syscall tracing
	mov	why, #1
	tst	ip, #PT_TRACESYS		@ are we tracing syscalls?
	beq	ret_disable_irq
	mov	r1, sp
	mov	r0, #1				@ trace exit [IP = 1]
	bl	SYMBOL_NAME(syscall_trace)
	b	ret_disable_irq
	

#include "calls.S"

/*=============================================================================
 * SWI handler
 *-----------------------------------------------------------------------------
 */

	/* If we're optimising for StrongARM the resulting code won't 
	   run on an ARM7 and we can save a couple of instructions.  
								--pb */
#ifdef CONFIG_CPU_ARM710
	.macro	arm710_bug_check, instr, temp
	and	\temp, \instr, #0x0f000000	@ check for SWI
	teq	\temp, #0x0f000000
	bne	.Larm700bug
	.endm

.Larm700bug:
	ldr	r0, [sp, #S_PSR]		@ Get calling cpsr
	sub	lr, lr, #4
	str	lr, [r8]
	msr	spsr, r0
	ldmia	sp, {r0 - lr}^			@ Get calling r0 - lr
	mov	r0, r0
	ldr	lr, [sp, #S_PC]			@ Get PC
	add	sp, sp, #S_FRAME_SIZE
	movs	pc, lr
#else
	.macro	arm710_bug_check, instr, temp
	.endm
#endif

	.align	5
ENTRY(vector_swi)
	save_user_regs
	zero_fp
	get_scno
	arm710_bug_check scno, ip

#ifdef CONFIG_ALIGNMENT_TRAP
	ldr	ip, __cr_alignment
	ldr	ip, [ip]
	mcr	p15, 0, ip, c1, c0		@ update control register
#endif
	enable_irq ip

	str	r4, [sp, #-S_OFF]!		@ push fifth arg

	get_current_task tsk
	ldr	ip, [tsk, #TSK_PTRACE]		@ check for syscall tracing
	bic	scno, scno, #0xff000000		@ mask off SWI op-code
	eor	scno, scno, #OS_NUMBER << 20	@ check OS number
	adr	tbl, sys_call_table		@ load syscall table pointer
	tst	ip, #PT_TRACESYS		@ are we tracing syscalls?
	bne	__sys_trace

	adrsvc	al, lr, ret_fast_syscall	@ return address
	cmp	scno, #NR_syscalls		@ check upper syscall limit
	ldrcc	pc, [tbl, scno, lsl #2]		@ call sys_* routine

	add	r1, sp, #S_OFF
2:	mov	why, #0				@ no longer a real syscall
	cmp	scno, #ARMSWI_OFFSET
	eor	r0, scno, #OS_NUMBER << 20	@ put OS number back
	bcs	SYMBOL_NAME(arm_syscall)	
	b	SYMBOL_NAME(sys_ni_syscall)	@ not private func

	/*
	 * This is the really slow path.  We're going to be doing
	 * context switches, and waiting for our parent to respond.
	 */
__sys_trace:
	add	r1, sp, #S_OFF
	mov	r0, #0				@ trace entry [IP = 0]
	bl	SYMBOL_NAME(syscall_trace)

	adrsvc	al, lr, __sys_trace_return	@ return address
	add	r1, sp, #S_R0 + S_OFF		@ pointer to regs
	cmp	scno, #NR_syscalls		@ check upper syscall limit
	ldmccia	r1, {r0 - r3}			@ have to reload r0 - r3
	ldrcc	pc, [tbl, scno, lsl #2]		@ call sys_* routine
	b	2b

__sys_trace_return:
	str	r0, [sp, #S_R0 + S_OFF]!	@ save returned r0
	mov	r1, sp
	mov	r0, #1				@ trace exit [IP = 1]
	bl	SYMBOL_NAME(syscall_trace)
	b	ret_disable_irq

	.align	5
#ifdef CONFIG_ALIGNMENT_TRAP
	.type	__cr_alignment, #object
__cr_alignment:
	.word	SYMBOL_NAME(cr_alignment)
#endif

	.type	sys_call_table, #object
ENTRY(sys_call_table)
#include "calls.S"

/*============================================================================
 * Special system call wrappers
 */
@ r0 = syscall number
@ r5 = syscall table
		.type	sys_syscall, #function
SYMBOL_NAME(sys_syscall):
		eor	scno, r0, #OS_NUMBER << 20
		cmp	scno, #NR_syscalls	@ check range
		stmleia	sp, {r5, r6}		@ shuffle args
		movle	r0, r1
		movle	r1, r2
		movle	r2, r3
		movle	r3, r4
		ldrle	pc, [tbl, scno, lsl #2]
		b	sys_ni_syscall

sys_fork_wrapper:
		add	r0, sp, #S_OFF
		b	SYMBOL_NAME(sys_fork)

sys_vfork_wrapper:
		add	r0, sp, #S_OFF
		b	SYMBOL_NAME(sys_vfork)

sys_execve_wrapper:
		add	r3, sp, #S_OFF
		b	SYMBOL_NAME(sys_execve)

sys_clone_wapper:
		add	r2, sp, #S_OFF
		b	SYMBOL_NAME(sys_clone)

sys_sigsuspend_wrapper:
		add	r3, sp, #S_OFF
		b	SYMBOL_NAME(sys_sigsuspend)

sys_rt_sigsuspend_wrapper:
		add	r2, sp, #S_OFF
		b	SYMBOL_NAME(sys_rt_sigsuspend)

sys_sigreturn_wrapper:
		add	r0, sp, #S_OFF
		b	SYMBOL_NAME(sys_sigreturn)

sys_rt_sigreturn_wrapper:
		add	r0, sp, #S_OFF
		b	SYMBOL_NAME(sys_rt_sigreturn)

sys_sigaltstack_wrapper:
		ldr	r2, [sp, #S_OFF + S_SP]
		b	do_sigaltstack

/*
 * Note: off_4k (r5) is always units of 4K.  If we can't do the requested
 * offset, we return EINVAL.
 */
sys_mmap2:
#if PAGE_SHIFT > 12
		tst	r5, #PGOFF_MASK
		moveq	r5, r5, lsr #PAGE_SHIFT - 12
		streq	r5, [sp, #4]
		beq	do_mmap2
		mov	r0, #-EINVAL
		RETINSTR(mov,pc, lr)
#else
		str	r5, [sp, #4]
		b	do_mmap2
#endif
