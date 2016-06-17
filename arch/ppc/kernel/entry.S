/*
 *  PowerPC version
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *  Rewritten by Cort Dougan (cort@fsmlabs.com) for PReP
 *    Copyright (C) 1996 Cort Dougan <cort@fsmlabs.com>
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
 *
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sys.h>
#include <linux/threads.h>
#include <asm/processor.h>
#include <asm/page.h>
#include <asm/mmu.h>
#include <asm/cputable.h>
#include <asm/ppc_asm.h>
#include "ppc_defs.h"

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
	.stabs	"arch/ppc/kernel/",N_SO,0,0,0f
	.stabs	"entry.S",N_SO,0,0,0f
0:

_GLOBAL(DoSyscall)
	stw	r0,THREAD+LAST_SYSCALL(r2)
	lwz	r11,_CCR(r1)	/* Clear SO bit in CR */
	lis	r10,0x1000
	andc	r11,r11,r10
	stw	r11,_CCR(r1)
#ifdef SHOW_SYSCALLS
#ifdef SHOW_SYSCALLS_TASK
	lis	r31,show_syscalls_task@ha
	lwz	r31,show_syscalls_task@l(r31)
	cmp	0,r2,r31
	bne	1f
#endif
	lis	r3,7f@ha
	addi	r3,r3,7f@l
	lwz	r4,GPR0(r1)
	lwz	r5,GPR3(r1)
	lwz	r6,GPR4(r1)
	lwz	r7,GPR5(r1)
	lwz	r8,GPR6(r1)
	lwz	r9,GPR7(r1)
	bl	printk
	lis	r3,77f@ha
	addi	r3,r3,77f@l
	lwz	r4,GPR8(r1)
	lwz	r5,GPR9(r1)
	mr	r6,r2
	bl	printk
	lwz	r0,GPR0(r1)
	lwz	r3,GPR3(r1)
	lwz	r4,GPR4(r1)
	lwz	r5,GPR5(r1)
	lwz	r6,GPR6(r1)
	lwz	r7,GPR7(r1)
	lwz	r8,GPR8(r1)
1:
#endif /* SHOW_SYSCALLS */
	cmpi	0,r0,0x7777	/* Special case for 'sys_sigreturn' */
	beq-	10f
	cmpi    0,r0,0x6666     /* Special case for 'sys_rt_sigreturn' */
	beq-    16f
	lwz	r10,TASK_PTRACE(r2)
	andi.	r10,r10,PT_TRACESYS
	bne-	50f
	cmpli	0,r0,NR_syscalls
	bge-	66f
	lis	r10,sys_call_table@h
	ori	r10,r10,sys_call_table@l
	slwi	r0,r0,2
	lwzx	r10,r10,r0	/* Fetch system call handler [ptr] */
	cmpi	0,r10,0
	beq-	66f
	mtlr	r10
	addi	r9,r1,STACK_FRAME_OVERHEAD
	blrl			/* Call handler */
	.globl	ret_from_syscall_1
ret_from_syscall_1:
20:	stw	r3,RESULT(r1)	/* Save result */
#ifdef SHOW_SYSCALLS
#ifdef SHOW_SYSCALLS_TASK
	cmp	0,r2,r31
	bne	91f
#endif
	mr	r4,r3
	lis	r3,79f@ha
	addi	r3,r3,79f@l
	bl	printk
	lwz	r3,RESULT(r1)
91:
#endif
	li	r10,-_LAST_ERRNO
	cmpl	0,r3,r10
	blt	30f
	neg	r3,r3
	cmpi	0,r3,ERESTARTNOHAND
	bne	22f
	li	r3,EINTR
22:	lwz	r10,_CCR(r1)	/* Set SO bit in CR */
	oris	r10,r10,0x1000
	stw	r10,_CCR(r1)
30:	stw	r3,GPR3(r1)	/* Update return value */
	b	ret_from_except
66:	li	r3,ENOSYS
	b	22b
/* sys_sigreturn */
10:	addi	r3,r1,STACK_FRAME_OVERHEAD
	bl	sys_sigreturn
	cmpi    0,r3,0          /* Check for restarted system call */
	bge     ret_from_except
	b       20b
/* sys_rt_sigreturn */
16:	addi    r3,r1,STACK_FRAME_OVERHEAD
	bl      sys_rt_sigreturn
	cmpi	0,r3,0		/* Check for restarted system call */
	bge	ret_from_except
	b	20b
/* Traced system call support */
50:	bl	syscall_trace
	lwz	r0,GPR0(r1)	/* Restore original registers */
	lwz	r3,GPR3(r1)
	lwz	r4,GPR4(r1)
	lwz	r5,GPR5(r1)
	lwz	r6,GPR6(r1)
	lwz	r7,GPR7(r1)
	lwz	r8,GPR8(r1)
	lwz	r9,GPR9(r1)
	cmpli	0,r0,NR_syscalls
	bge-	66f
	lis	r10,sys_call_table@h
	ori	r10,r10,sys_call_table@l
	slwi	r0,r0,2
	lwzx	r10,r10,r0	/* Fetch system call handler [ptr] */
	cmpi	0,r10,0
	beq-	66f
	mtlr	r10
	addi	r9,r1,STACK_FRAME_OVERHEAD
	blrl			/* Call handler */
	.globl	ret_from_syscall_2
ret_from_syscall_2:
	stw	r3,RESULT(r1)	/* Save result */
	li	r10,-_LAST_ERRNO
	cmpl	0,r3,r10
	blt	60f
	neg	r3,r3
	cmpi	0,r3,ERESTARTNOHAND
	bne	52f
	li	r3,EINTR
52:	lwz	r10,_CCR(r1)	/* Set SO bit in CR */
	oris	r10,r10,0x1000
	stw	r10,_CCR(r1)
60:	stw	r3,GPR3(r1)	/* Update return value */
	bl	syscall_trace
	b	ret_from_except
66:	li	r3,ENOSYS
	b	52b
#ifdef SHOW_SYSCALLS
7:	.string	"syscall %d(%x, %x, %x, %x, %x, "
77:	.string	"%x, %x), current=%p\n"
79:	.string	" -> %x\n"
	.align	2,0
#endif

/*
 * This routine switches between two different tasks.  The process
 * state of one is saved on its kernel stack.  Then the state
 * of the other is restored from its kernel stack.  The memory
 * management hardware is updated to the second process's state.
 * Finally, we can return to the second process.
 * On entry, r3 points to the THREAD for the current task, r4
 * points to the THREAD for the new task.
 *
 * Note: there are two ways to get to the "going out" portion
 * of this code; either by coming in via the entry (_switch)
 * or via "fork" which must set up an environment equivalent
 * to the "_switch" path.  If you change this , you'll have to
 * change the fork code also.
 *
 * The code which creates the new task context is in 'copy_thread'
 * in arch/ppc/kernel/process.c
 */
_GLOBAL(_switch)
	stwu	r1,-INT_FRAME_SIZE(r1)
	stw	r0,GPR0(r1)
	lwz	r0,0(r1)
	stw	r0,GPR1(r1)
	/* r3-r13 are caller saved -- Cort */
	SAVE_GPR(2, r1)
	SAVE_8GPRS(14, r1)
	SAVE_10GPRS(22, r1)
	mflr	r20		/* Return to switch caller */
	stw	r20,INT_FRAME_SIZE+4(r1)
	mfmsr	r22
	li	r0,MSR_FP	/* Disable floating-point */
#ifdef CONFIG_ALTIVEC
BEGIN_FTR_SECTION
	oris	r0,r0,MSR_VEC@h	/* Disable altivec */
END_FTR_SECTION_IFSET(CPU_FTR_ALTIVEC)
#endif /* CONFIG_ALTIVEC */
	and.	r0,r0,r22	/* FP or altivec enabled? */
	beq+	1f
	andc	r22,r22,r0
	mtmsr	r22
	isync
1:	stw	r20,_NIP(r1)
	stw	r22,_MSR(r1)
	stw	r20,_LINK(r1)
	mfcr	r20
	mfctr	r22
	mfspr	r23,XER
	stw	r20,_CCR(r1)
	stw	r22,_CTR(r1)
	stw	r23,_XER(r1)
	li	r0,0x0ff0
	stw	r0,TRAP(r1)
	stw	r1,KSP(r3)	/* Set old stack pointer */

	tophys(r0,r4)
	CLR_TOP32(r0)
	mtspr	SPRG3,r0	/* Update current THREAD phys addr */
	lwz	r1,KSP(r4)	/* Load new stack pointer */
	/* save the old current 'last' for return value */
	mr	r3,r2
	addi	r2,r4,-THREAD	/* Update current */
	lwz	r0,_CCR(r1)
	mtcrf	0xFF,r0
	/* r3-r13 are destroyed -- Cort */
	REST_2GPRS(14, r1)
	REST_8GPRS(16, r1)
	REST_8GPRS(24, r1)

	lwz	r4,_NIP(r1)	/* Return to _switch caller in new task */
	mtlr	r4
	addi	r1,r1,INT_FRAME_SIZE
	blr

	.globl	syscall_direct_return
syscall_direct_return:
	addi	r1,r3,-STACK_FRAME_OVERHEAD
	lwz	r10,TASK_PTRACE(r2)
	andi.	r10,r10,PT_TRACESYS
	beq+	ret_from_except
	bl	syscall_trace
	b	ret_from_except

	.globl	ret_from_fork
ret_from_fork:
	bl	schedule_tail
	lwz	r0,TASK_PTRACE(r2)
	andi.	r0,r0,PT_TRACESYS
	bnel-	syscall_trace
	b	ret_from_except

	.globl	ret_from_intercept
ret_from_intercept:
	/*
	 * We may be returning from RTL and cannot do the normal checks
	 * -- Cort
	 */
	cmpi	0,r3,0
	beq	restore
	.globl	ret_from_except
ret_from_except:
	lwz	r3,_MSR(r1)	/* Returning to user mode? */
	andi.	r3,r3,MSR_PR
	beq+	do_signal_ret	/* if so, check need_resched and signals */
	lwz	r3,NEED_RESCHED(r2)
	cmpi	0,r3,0		/* check need_resched flag */
	beq+	7f
	bl	schedule
7:	lwz	r5,SIGPENDING(r2) /* Check for pending unblocked signals */
	cmpwi	0,r5,0
	beq+	do_signal_ret
	li	r3,0
	addi	r4,r1,STACK_FRAME_OVERHEAD
	bl	do_signal
	.globl	do_signal_ret
do_signal_ret:
	.globl ret_to_user_hook
ret_to_user_hook:
	nop
restore:
	lwz	r3,_XER(r1)
	mtspr	XER,r3
	REST_10GPRS(9,r1)
	REST_10GPRS(19,r1)
	REST_2GPRS(29,r1)
	REST_GPR(31,r1)

	/* make sure we hard disable here, even if rtl is active, to protect
	 * SRR[01] and SPRG2 -- Cort
	 */
	mfmsr	r0		/* Get current interrupt state */
	rlwinm	r0,r0,0,17,15	/* clear MSR_EE in r0 */
#ifdef CONFIG_4xx
 	rlwinm	r0,r0,0,23,21	/* clear MSR_DE in r0 */
#endif
	SYNC			/* Some chip revs have problems here... */
	mtmsr	r0		/* Update machine state */

	PPC405_ERR77(0,r1)
	stwcx.	r0,0,r1		/* to clear the reservation */

	/* if returning to user mode, set new sprg2 and save kernel SP */
	lwz	r0,_MSR(r1)
	andi.	r0,r0,MSR_PR
	beq+	1f
#ifdef CONFIG_ALTIVEC
BEGIN_FTR_SECTION
	lwz	r0,THREAD+THREAD_VRSAVE(r2)
	mtspr	SPRN_VRSAVE,r0		/* if G4, restore VRSAVE reg */
END_FTR_SECTION_IFSET(CPU_FTR_ALTIVEC)
#endif /* CONFIG_ALTIVEC */
#if defined(CONFIG_4xx) && !defined(CONFIG_BDI_SWITCH)
	/* Restore the processor debugging state of the thread.  Only do
	 * this if we aren't using an Abatron BDI JTAG debugger.  It doesn't
	 * tolerate others mucking with the debug registers.
	 */
	lwz	r0,THREAD+THREAD_DBCR0(r2)
	mtspr	SPRN_DBCR0,r0
#endif
	addi	r0,r1,INT_FRAME_SIZE	/* size of frame */
	stw	r0,THREAD+KSP(r2)	/* save kernel stack pointer */
	tophys(r8,r1)
	CLR_TOP32(r8)
	mtspr	SPRG2,r8		/* phys exception stack pointer */
1:
	lwz	r3,_CTR(r1)
	lwz	r0,_LINK(r1)
	mtctr	r3
	mtlr	r0
	REST_4GPRS(3, r1)
	REST_2GPRS(7, r1)

#ifndef CONFIG_SMP
	/* We have to "dummy" load from the context save area in case
	 * these instructions cause an MMU fault.  If this happens
	 * after we load SRR0/SRR1, our return context is hosed.  -- Dan
	 *
	 * This workaround is not enough, we must also make sure the
	 * actual code for this routine is in the TLB or BAT mapped.
	 * For 6xx/Power3, we know the code is in a BAT, so this should
	 * be enough in UP. In SMP, I limit lowmem to the amount of
	 * RAM that can be BAT mapped. Other CPUs may need additional
	 * tweaks, especially if used SMP or if the code for this routine
	 * crosses page boundaries. The TLB pin down for 4xx should help
	 * for example. --BenH.
	 */
	lwz	r0,GPR0(r1)
	lwz	r0,GPR2(r1)
	lwz	r0,GPR1(r1)
#endif /* ndef CONFIG_SMP */

	/* We re-use r3,r4 here (the load above was to cause the MMU
	 * fault if necessary).  Using r3,r4 removes the need to "dummy"
	 * load the CCR and NIP.  Since we load them we may as well
	 * use them.
	 */
	lwz	r3,_CCR(r1)
	lwz	r4,_NIP(r1)

	lwz	r0,_MSR(r1)
	FIX_SRR1(r0,r2)
	mtspr	SRR1,r0
	mtcrf	0xFF,r3
	mtspr	SRR0,r4
	lwz	r0,GPR0(r1)
	lwz	r2,GPR2(r1)
	lwz	r3,GPR3(r1)
	lwz	r4,GPR4(r1)
	lwz	r1,GPR1(r1)
	SYNC
	PPC405_ERR77_SYNC
	RFI


/*
 * PROM code for specific machines follows.  Put it
 * here so it's easy to add arch-specific sections later.
 * -- Cort
 */
#if defined(CONFIG_ALL_PPC)
/*
 * On CHRP, the Run-Time Abstraction Services (RTAS) have to be
 * called with the MMU off.
 */
	.globl	enter_rtas
enter_rtas:
	mflr	r0
	stw	r0,20(r1)
	lis	r4,rtas_data@ha
	lwz	r4,rtas_data@l(r4)
	lis	r6,1f@ha	/* physical return address for rtas */
	addi	r6,r6,1f@l
	addis	r6,r6,-KERNELBASE@h
	subi	r7,r1,INT_FRAME_SIZE
	addis	r7,r7,-KERNELBASE@h
	lis	r8,rtas_entry@ha
	lwz	r8,rtas_entry@l(r8)
	mfmsr	r9
	stw	r9,8(r1)
	li	r0,0
	ori	r0,r0,MSR_EE|MSR_SE|MSR_BE|MSR_FE0|MSR_FE1
	andc	r0,r9,r0
	li	r10,MSR_IR|MSR_DR|MSR_FP
	andc	r9,r0,r10
	SYNC			/* disable interrupts so SRR0/1 */
	mtmsr	r0		/* don't get trashed */
	mtlr	r6
	CLR_TOP32(r7)
	mtspr	SPRG2,r7
	mtspr	SRR0,r8
	mtspr	SRR1,r9
	RFI
1:	addis	r9,r1,-KERNELBASE@h
	lwz	r8,20(r9)	/* get return address */
	lwz	r9,8(r9)	/* original msr value */
	FIX_SRR1(r9,r0)
	li	r0,0
	mtspr	SPRG2,r0
	mtspr	SRR0,r8
	mtspr	SRR1,r9
	RFI			/* return to caller */
#endif /* CONFIG_ALL_PPC */
