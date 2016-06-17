/*
 *  arch/ppc64/kernel/entry.S
 *
 *  PowerPC version 
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *  Rewritten by Cort Dougan (cort@cs.nmt.edu) for PReP
 *    Copyright (C) 1996 Cort Dougan <cort@cs.nmt.edu>
 *  Adapted for Power Macintosh by Paul Mackerras.
 *  Low-level exception handlers and MMU support
 *  rewritten by Paul Mackerras.
 *    Copyright (C) 1996 Paul Mackerras.
 *  MPC8xx modifications Copyright (C) 1997 Dan Malek (dmalek@jlc.net).
 *
 *  This file contains the system call entry code, context switch
 *  code, and exception/interrupt return code for PowerPC.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */

#include "ppc_asm.h"
#include <asm/processor.h>
#include <asm/page.h>
#include <asm/mmu.h>
#include <linux/errno.h>
#include <linux/sys.h>
#include <linux/config.h>
#include <asm/cputable.h>

#ifdef CONFIG_PPC_ISERIES
#define DO_SOFT_DISABLE
#endif

#undef SHOW_SYSCALLS
#undef SHOW_SYSCALLS_TASK

#ifdef SHOW_SYSCALLS_TASK
	.data
show_syscalls_task:
	.long	-1
#endif

/*
 * Handle a system call.
 */
	.text
_GLOBAL(DoSyscall)
	ld	r11,_CCR(r1)	/* Clear SO bit in CR */
	lis	r10,0x1000
	andc	r11,r11,r10
	std	r11,_CCR(r1)
	ld	r10,PACACURRENT(r13)
	std	r0,THREAD+LAST_SYSCALL(r10)
#ifdef SHOW_SYSCALLS
# ifdef SHOW_SYSCALLS_TASK
	LOADBASE(r31,show_syscalls_task)
	ld	r31,show_syscalls_task@l(r31)
	cmp	0,r10,r31
	bne	1f
# endif /* SHOW_SYSCALLS_TASK */
	LOADADDR(r3,7f)
	ld	r4,GPR0(r1)
	ld	r5,GPR3(r1)
	ld	r6,GPR4(r1)
	ld	r7,GPR5(r1)
	ld	r8,GPR6(r1)
	ld	r9,GPR7(r1)
	bl	.printk
	LOADADDR(r3,77f)
	ld	r4,GPR8(r1)
	ld	r5,GPR9(r1)
	ld	r6, PACACURRENT(r13)
	bl	.printk
	ld	r0,GPR0(r1)
	ld	r3,GPR3(r1)
	ld	r4,GPR4(r1)
	ld	r5,GPR5(r1)
	ld	r6,GPR6(r1)
	ld	r7,GPR7(r1)
	ld	r8,GPR8(r1)
1:
#endif /* SHOW_SYSCALLS */
	ld	r11,TASK_PTRACE(r10)
	andi.	r11,r11,PT_TRACESYS
	bne-	50f
	cmpli	0,r0,NR_syscalls
	bge-	66f
/* Ken Aaker: Need to vector to 32 Bit or default sys_call_table here,
 *            based on caller's run-mode / personality.
 *
 */
#ifdef CONFIG_BINFMT_ELF32
	ld	r11,THREAD+THREAD_FLAGS(r10)
	andi.	r11,r11,PPC_FLAG_32BIT
	beq-	15f
	LOADADDR(r11,.sys_call_table32)
/* Now mung the first 4 parameters into shape, by making certain that
 * the high bits (most significant 32 bits in 64 bit reg) are 0
 * for the first 4 parameter regs(3-6).
 */
	clrldi	r3,r3,32
	clrldi	r4,r4,32
	clrldi	r5,r5,32
	clrldi	r6,r6,32
	b	17f
15:
#endif
	LOADADDR(r11,.sys_call_table)
17:
	slwi	r0,r0,3
	ldx	r10,r11,r0	/* Fetch system call handler [ptr] */
	mtlr	r10
	addi	r9,r1,STACK_FRAME_OVERHEAD
	blrl			/* Call handler */
_GLOBAL(ret_from_syscall_1)
20:	std	r3,RESULT(r1)	/* Save result */
#ifdef SHOW_SYSCALLS
# ifdef SHOW_SYSCALLS_TASK
	cmp	0,r13,r31
	bne	91f
# endif /* SHOW_SYSCALLS_TASK */
	mr	r4,r3
	LOADADDR(r3,79f)
	bl	.printk
	ld	r3,RESULT(r1)
91:
#endif /* SHOW_SYSCALLS */
	li	r10,-_LAST_ERRNO
	cmpl	0,r3,r10
	blt	30f
	neg	r3,r3
	cmpi	0,r3,ERESTARTNOHAND
	bne	22f
	li	r3,EINTR
22:	ld	r10,_CCR(r1)	/* Set SO bit in CR */
	oris	r10,r10,0x1000
	std	r10,_CCR(r1)
30:	std	r3,GPR3(r1)	/* Update return value */
	b	.ret_from_except
66:	li	r3,ENOSYS
	b	22b
        
/* Traced system call support */
50:	bl	.syscall_trace
	ld	r0,GPR0(r1)	/* Restore original registers */
	ld	r3,GPR3(r1)
	ld	r4,GPR4(r1)
	ld	r5,GPR5(r1)
	ld	r6,GPR6(r1)
	ld	r7,GPR7(r1)
	ld	r8,GPR8(r1)
	ld	r9,GPR9(r1)
	cmpli	0,r0,NR_syscalls
	bge-	66f
#ifdef CONFIG_BINFMT_ELF32
	ld	r10,PACACURRENT(r13)
	ld	r10,THREAD+THREAD_FLAGS(r10)
	andi.	r10,r10,PPC_FLAG_32BIT
	beq-	55f
	LOADADDR(r10,.sys_call_table32)
/* Now mung the first 4 parameters into shape, by making certain that
 * the high bits (most significant 32 bits in 64 bit reg) are 0
 * for the first 4 parameter regs(3-6).
 */
	clrldi	r3,r3,32
	clrldi	r4,r4,32
	clrldi	r5,r5,32
	clrldi	r6,r6,32
	b	57f
55:
#endif
	LOADADDR(r10,.sys_call_table)
57:
	slwi	r0,r0,3
	ldx	r10,r10,r0	/* Fetch system call handler [ptr] */
	mtlr	r10
	addi	r9,r1,STACK_FRAME_OVERHEAD
	blrl			/* Call handler */
_GLOBAL(ret_from_syscall_2)
58:	std	r3,RESULT(r1)	/* Save result */	
	li	r10,-_LAST_ERRNO
	cmpl	0,r3,r10
	blt	60f
	neg	r3,r3
	cmpi	0,r3,ERESTARTNOHAND
	bne	57f
	li	r3,EINTR
57:	ld	r10,_CCR(r1)	/* Set SO bit in CR */
	oris	r10,r10,0x1000
	std	r10,_CCR(r1)
60:	std	r3,GPR3(r1)	/* Update return value */
	bl	.syscall_trace
	b	.ret_from_except
66:	li	r3,ENOSYS
	b	57b
#ifdef SHOW_SYSCALLS
7:	.string	"syscall %d(%x, %x, %x, %x, %x, "
77:	.string	"%x, %x), current=%p\n"
79:	.string	" -> %x\n"
	.align	2,0
#endif

_GLOBAL(ppc32_sigreturn)
	bl	.sys32_sigreturn
	b	80f

_GLOBAL(ppc32_rt_sigreturn)
	bl	.sys32_rt_sigreturn
	b	80f

_GLOBAL(ppc64_sigreturn)
	bl	.sys_sigreturn
	b	80f

_GLOBAL(ppc64_rt_sigreturn)
	bl	.sys_rt_sigreturn

80:	ld	r10,PACACURRENT(r13)
	ld	r10,TASK_PTRACE(r10)
	andi.	r10,r10,PT_TRACESYS
	bne-	81f
	cmpi	0,r3,0
	bge	.ret_from_except
	b	20b
81:	cmpi	0,r3,0
	blt	58b
	bl	.syscall_trace
	b	.ret_from_except

/*
 * This routine switches between two different tasks.  The process
 * state of one is saved on its kernel stack.  Then the state
 * of the other is restored from its kernel stack.  The memory
 * management hardware is updated to the second process's state.
 * Finally, we can return to the second process, via ret_from_except.
 * On entry, r3 points to the THREAD for the current task, r4
 * points to the THREAD for the new task.
 *
 * Note: there are two ways to get to the "going out" portion
 * of this code; either by coming in via the entry (_switch)
 * or via "fork" which must set up an environment equivalent
 * to the "_switch" path.  If you change this (or in particular, the
 * SAVE_REGS macro), you'll have to change the fork code also.
 *
 * The code which creates the new task context is in 'copy_thread'
 * in arch/ppc64/kernel/process.c
 */
_GLOBAL(_switch)
	stdu	r1,-INT_FRAME_SIZE(r1)
	ld	r6,0(r1)
	std	r6,GPR1(r1)
	/* r3-r13 are caller saved -- Cort */
	SAVE_GPR(2, r1)
	SAVE_8GPRS(14, r1)
	SAVE_10GPRS(22, r1)
	mflr	r20		/* Return to switch caller */
	mfmsr	r22
	li	r6,MSR_FP	/* Disable floating-point */
#ifdef CONFIG_ALTIVEC
BEGIN_FTR_SECTION
	oris	r6,r6,MSR_VEC@h	/* Disable altivec */
END_FTR_SECTION_IFSET(CPU_FTR_ALTIVEC)
#endif /* CONFIG_ALTIVEC */
	andc	r22,r22,r6
	mtmsrd	r22
	isync
	std	r20,_NIP(r1)
	std	r22,_MSR(r1)
	std	r20,_LINK(r1)
	mfcr	r20
	std	r20,_CCR(r1)
	li	r6,0x0ff0
	std	r6,TRAP(r1)
	std	r1,KSP(r3)	/* Set old stack pointer */

	addi	r3,r3,-THREAD	/* old 'current' for return value */
	addi	r7,r4,-THREAD	/* Convert THREAD to 'current' */
	std	r7,PACACURRENT(r13)	/* Set new 'current' */

#ifdef CONFIG_PPC_ISERIES
	ld	r7,THREAD_FLAGS(r4)	/* Get run light flag */
	mfspr	r9,CTRLF
	srdi	r7,r7,1		/* Align to run light bit in CTRL reg */
	insrdi	r9,r7,1,63	/* Insert run light into CTRL */
	mtspr	CTRLT,r9
#endif
	ld	r1,KSP(r4)	/* Load new stack pointer */
	ld	r6,_CCR(r1)
	mtcrf	0xFF,r6
	/* r3-r13 are destroyed -- Cort */
	REST_8GPRS(14, r1)
	REST_10GPRS(22, r1)

	ld	r7,_NIP(r1)	/* Return to _switch caller in new task */
	ld	r1,GPR1(r1)
	mtlr	r7
	blr

_GLOBAL(ret_from_fork)
	bl	.schedule_tail
	ld	r4,PACACURRENT(r13)
	ld	r0,TASK_PTRACE(r4)
	andi.	r0,r0,PT_TRACESYS
	beq+	.ret_from_except
	bl	.syscall_trace
	b	.ret_from_except

_GLOBAL(ret_from_except)
#ifdef CONFIG_PPC_ISERIES
	ld	r5,SOFTE(r1)
	cmpdi	0,r5,0
	beq	4f
irq_recheck:
	/* Check for pending interrupts (iSeries) */
	CHECKANYINT(r3,r4)
	beq+	4f	/* skip do_IRQ if no interrupts */

	li	r3,0
	stb	r3,PACAPROCENABLED(r13)	/* ensure we are disabled */
	addi	r3,r1,STACK_FRAME_OVERHEAD
	bl	.do_IRQ
	b	irq_recheck	/* loop back and handle more */
4:
#endif
_GLOBAL(do_bottom_half_ret)
	ld	r3,_MSR(r1)	/* Returning to user mode? */
	andi.	r3,r3,MSR_PR
	beq+	restore		/* if so, check need_resched and signals */
_GLOBAL(ret_to_user_hook)
	nop
	/* NEED_RESCHED is a volatile long (64-bits) */
	ld	r3,PACACURRENT(r13)
	ld	r3,NEED_RESCHED(r3)
	cmpi	0,r3,0		/* check need_resched flag */
	beq+	7f
	bl	.schedule
	/* SIGPENDING is an int (32-bits) */
7:
	ld	r5,PACACURRENT(r13)
	lwz	r5,SIGPENDING(r5) /* Check for pending unblocked signals */
	cmpwi	0,r5,0
	beq+	restore
	li	r3,0
	addi	r4,r1,STACK_FRAME_OVERHEAD
	bl	.do_signal
_GLOBAL(do_signal_ret)
restore:	
	ld	r3,_CTR(r1)
	ld	r0,_LINK(r1)
	mtctr	r3
	mtlr	r0
	ld	r3,_XER(r1)
	mtspr	XER,r3

	REST_8GPRS(5, r1)
	REST_10GPRS(14, r1)
	REST_8GPRS(24, r1)
	
	/* make sure we hard disable here, even if rtl is active, to protect
	 * SRR[01] and SPRG2 -- Cort 
	 */
	mfmsr	r0		/* Get current interrupt state */
	li	r4,0
	ori	r4,r4,MSR_EE|MSR_RI
	andc	r0,r0,r4	/* clear MSR_EE and MSR_RI */
	mtmsrd	r0		/* Update machine state */
#ifdef CONFIG_PPC_ISERIES
	ld	r0,SOFTE(r1)
	cmpi	0,r0,0
	beq+	1f

	CHECKANYINT(r4,r3)
	beq+	1f
	mfmsr	r0
	ori	r0,r0,MSR_EE|MSR_RI
	mtmsrd	r0
	b	irq_recheck
	
1:	
#endif
	stdcx.	r0,0,r1		/* to clear the reservation */

#ifdef DO_SOFT_DISABLE
	ld	r0,SOFTE(r1)
	stb	r0,PACAPROCENABLED(r13)
#endif
	/* if returning to user mode, save kernel SP */
	ld	r0,_MSR(r1)
	andi.	r0,r0,MSR_PR
	beq+	1f
	ld	r4,PACACURRENT(r13)
#ifdef CONFIG_ALTIVEC
BEGIN_FTR_SECTION
	ld	r0,THREAD+THREAD_VRSAVE(r4)
	mtspr	SPRN_VRSAVE,r0		/* if GPUL, restore VRSAVE reg */
END_FTR_SECTION_IFSET(CPU_FTR_ALTIVEC)
#endif /* CONFIG_ALTIVEC */
	addi	r0,r1,INT_FRAME_SIZE	/* size of frame */
	std	r0,THREAD+KSP(r4)	/* save kernel stack pointer */
	std	r1,PACAKSAVE(r13)	/* save exception stack pointer */
	REST_GPR(13,r1)
1:
	mfmsr	r0
	li	r2, MSR_RI
	andc	r0,r0,r2
	mtmsrd	r0,1

	ld	r0,_MSR(r1)
	mtspr	SRR1,r0
	ld	r2,_CCR(r1)
	mtcrf	0xFF,r2
	ld	r2,_NIP(r1)
	mtspr	SRR0,r2
	ld	r0,GPR0(r1)
	ld	r2,GPR2(r1)
	ld	r3,GPR3(r1)
	ld	r4,GPR4(r1)
	ld	r1,GPR1(r1)

	rfid

/*
 * On CHRP, the Run-Time Abstraction Services (RTAS) have to be
 * called with the MMU off.
 *
 * In addition, we need to be in 32b mode, at least for now.
 * 
 * Note: r3 is an input parameter to rtas, so don't trash it...
 */
_GLOBAL(enter_rtas)
	mflr	r0
	std	r0,16(r1)
        stdu	r1,-RTAS_FRAME_SIZE(r1)	/* Save SP and create stack space. */

	/* Because RTAS is running in 32b mode, it clobbers the high order half
	 * of all registers that it saves.  We therefore save those registers
	 * RTAS might touch to the stack.  (r0, r3-r13 are caller saved)
   	 */
	SAVE_GPR(2, r1)			/* Save the TOC */
	SAVE_GPR(13, r1)		/* Save current */
	SAVE_8GPRS(14, r1)		/* Save the non-volatiles */
	SAVE_10GPRS(22, r1)		/* ditto */

	mfcr	r4
	std	r4,_CCR(r1)
	mfctr	r5
	std	r5,_CTR(r1)
	mfspr	r6,XER
	std	r6,_XER(r1)
	mfdar	r7
	std	r7,_DAR(r1)
	mfdsisr	r8
	std	r8,_DSISR(r1)
	mfsrr0	r9
	std	r9,_SRR0(r1)
	mfsrr1	r10
	std	r10,_SRR1(r1)

	/* Unfortunately, the stack pointer and the MSR are also clobbered,
	 * so they are saved in the PACA (SPRG3) which allows us to restore
	 * our original state after RTAS returns.
	 */
	std	r1,PACAR1(r13)
	mfmsr	r6
	std	r6,PACASAVEDMSR(r13)

	/* Setup our real return addr */	
	SET_REG_TO_LABEL(r4,.rtas_return_loc)
	SET_REG_TO_CONST(r9,KERNELBASE)
	sub	r4,r4,r9
       	mtlr	r4

	li	r0,0
	ori	r0,r0,MSR_EE|MSR_SE|MSR_BE|MSR_RI
	andc	r0,r6,r0
	
        li      r9,1
        rldicr  r9,r9,MSR_SF_LG,(63-MSR_SF_LG)
	ori	r9,r9,MSR_IR|MSR_DR|MSR_FE0|MSR_FE1|MSR_FP|MSR_RI
	andc	r6,r0,r9
	sync				/* disable interrupts so SRR0/1 */
	mtmsrd	r0			/* don't get trashed */

	SET_REG_TO_LABEL(r4,rtas)
	ld	r5,RTASENTRY(r4)	/* get the rtas->entry value */
	ld	r4,RTASBASE(r4)		/* get the rtas->base value */
	
	mtspr	SRR0,r5
	mtspr	SRR1,r6
	rfid

_STATIC(rtas_return_loc)
	/* relocation is off at this point */
	mfspr	r13,SPRG3	        /* Get PACA */
	SET_REG_TO_CONST(r5, KERNELBASE)
	sub     r13,r13,r5              /* RELOC the PACA base pointer */

	mfmsr	r6			/* Clear RI while SRR0/1 are live */
	li	r0,MSR_RI
	andc	r6,r6,r0
	sync
	mtmsrd	r6

	ld	r1,PACAR1(r13)           /* Restore our SP */
	LOADADDR(r3,.rtas_restore_regs)
	ld	r4,PACASAVEDMSR(r13)     /* Restore our MSR */

	mtspr	SRR0,r3
	mtspr	SRR1,r4
	rfid

_STATIC(rtas_restore_regs)
	/* relocation is on at this point */
	REST_GPR(2, r1)			/* Restore the TOC */
	REST_GPR(13, r1)		/* Restore current */
	REST_8GPRS(14, r1)		/* Restore the non-volatiles */
	REST_10GPRS(22, r1)		/* ditto */

	/* put back paca in r13 */
	mfspr	r13,SPRG3

	ld	r4,_CCR(r1)
	mtcr	r4
	ld	r5,_CTR(r1)
	mtctr	r5
	ld	r6,_XER(r1)
	mtspr	XER,r6
	ld	r7,_DAR(r1)
	mtdar	r7
	ld	r8,_DSISR(r1)
	mtdsisr	r8
	ld	r9,_SRR0(r1)
	mtsrr0	r9
	ld	r10,_SRR1(r1)
	mtsrr1	r10

        addi	r1,r1,RTAS_FRAME_SIZE	/* Unstack our frame */
	ld	r0,16(r1)		/* get return address */

	mtlr    r0
        blr				/* return to caller */

_GLOBAL(enter_prom)
	mflr	r0
	std	r0,16(r1)
        stdu	r1,-PROM_FRAME_SIZE(r1)	/* Save SP and create stack space */

	/* Because PROM is running in 32b mode, it clobbers the high order half
	 * of all registers that it saves.  We therefore save those registers
	 * PROM might touch to the stack.  (r0, r3-r13 are caller saved)
   	 */
	SAVE_8GPRS(2, r1)		/* Save the TOC & incoming param(s) */
	SAVE_GPR(13, r1)		/* Save current */
	SAVE_8GPRS(14, r1)		/* Save the non-volatiles */
	SAVE_10GPRS(22, r1)		/* ditto */

	mfcr	r4
	std	r4,_CCR(r1)
	mfctr	r5
	std	r5,_CTR(r1)
	mfspr	r6,XER
	std	r6,_XER(r1)
	mfdar	r7
	std	r7,_DAR(r1)
	mfdsisr	r8
	std	r8,_DSISR(r1)
	mfsrr0	r9
	std	r9,_SRR0(r1)
	mfsrr1	r10
	std	r10,_SRR1(r1)
	mfmsr	r11
	std	r11,_MSR(r1)

	/* Unfortunately, the stack pointer is also clobbered, so it is saved
	 * in the SPRG2 which allows us to restore our original state after
	 * PROM returns.
         */
	mtspr	SPRG2,r1

        /* put a relocation offset into r3 */
        bl      .reloc_offset
	LOADADDR(r12,prom)
	sub	r12,r12,r3
	ld	r12,PROMENTRY(r12)	/* get the prom->entry value */
	mtlr	r12

        mfmsr   r11			/* grab the current MSR */
        li      r12,1
        rldicr  r12,r12,MSR_SF_LG,(63-MSR_SF_LG)
        andc    r11,r11,r12
        li      r12,1
        rldicr  r12,r12,MSR_ISF_LG,(63-MSR_ISF_LG)
        andc    r11,r11,r12
        mtmsrd  r11
        isync

	REST_8GPRS(2, r1)		/* Restore the TOC & param(s) */
	REST_GPR(13, r1)		/* Restore current */
	REST_8GPRS(14, r1)		/* Restore the non-volatiles */
	REST_10GPRS(22, r1)		/* ditto */
	blrl				/* Entering PROM here... */

	mfspr	r1,SPRG2		/* Restore the stack pointer */
	ld	r6,_MSR(r1)		/* Restore the MSR */
	mtmsrd	r6
        isync

	REST_GPR(2, r1)			/* Restore the TOC */
	REST_GPR(13, r1)		/* Restore current */
	REST_8GPRS(14, r1)		/* Restore the non-volatiles */
	REST_10GPRS(22, r1)		/* ditto */

	ld	r4,_CCR(r1)
	mtcr	r4
	ld	r5,_CTR(r1)
	mtctr	r5
	ld	r6,_XER(r1)
	mtspr	XER,r6
	ld	r7,_DAR(r1)
	mtdar	r7
	ld	r8,_DSISR(r1)
	mtdsisr	r8
	ld	r9,_SRR0(r1)
	mtsrr0	r9
	ld	r10,_SRR1(r1)
	mtsrr1	r10
        addi	r1,r1,PROM_FRAME_SIZE
	ld	r0,16(r1)		/* get return address */

	mtlr    r0
        blr				/* return to caller */
