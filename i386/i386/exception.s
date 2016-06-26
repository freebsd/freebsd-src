/*-
 * Copyright (c) 1989, 1990 William F. Jolitz.
 * Copyright (c) 1990 The Regents of the University of California.
 * Copyright (c) 2007 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by A. Joseph Koshy under
 * sponsorship from the FreeBSD Foundation and Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include "opt_apic.h"
#include "opt_atpic.h"
#include "opt_hwpmc_hooks.h"
#include "opt_npx.h"

#include <machine/asmacros.h>
#include <machine/psl.h>
#include <machine/trap.h>

#include "assym.s"

#define	SEL_RPL_MASK	0x0003
#define	GSEL_KPL	0x0020	/* GSEL(GCODE_SEL, SEL_KPL) */

#ifdef KDTRACE_HOOKS
	.bss
	.globl	dtrace_invop_jump_addr
	.align	4
	.type	dtrace_invop_jump_addr, @object
	.size	dtrace_invop_jump_addr, 4
dtrace_invop_jump_addr:
	.zero	4
	.globl	dtrace_invop_calltrap_addr
	.align	4
	.type	dtrace_invop_calltrap_addr, @object
	.size	dtrace_invop_calltrap_addr, 4
dtrace_invop_calltrap_addr:
	.zero	8
#endif
	.text
#ifdef HWPMC_HOOKS
	ENTRY(start_exceptions)
#endif
/*****************************************************************************/
/* Trap handling                                                             */
/*****************************************************************************/
/*
 * Trap and fault vector routines.
 *
 * Most traps are 'trap gates', SDT_SYS386TGT.  A trap gate pushes state on
 * the stack that mostly looks like an interrupt, but does not disable
 * interrupts.  A few of the traps we are use are interrupt gates,
 * SDT_SYS386IGT, which are nearly the same thing except interrupts are
 * disabled on entry.
 *
 * The cpu will push a certain amount of state onto the kernel stack for
 * the current process.  The amount of state depends on the type of trap
 * and whether the trap crossed rings or not.  See i386/include/frame.h.
 * At the very least the current EFLAGS (status register, which includes
 * the interrupt disable state prior to the trap), the code segment register,
 * and the return instruction pointer are pushed by the cpu.  The cpu
 * will also push an 'error' code for certain traps.  We push a dummy
 * error code for those traps where the cpu doesn't in order to maintain
 * a consistent frame.  We also push a contrived 'trap number'.
 *
 * The cpu does not push the general registers, we must do that, and we
 * must restore them prior to calling 'iret'.  The cpu adjusts the %cs and
 * %ss segment registers, but does not mess with %ds, %es, or %fs.  Thus we
 * must load them with appropriate values for supervisor mode operation.
 */

MCOUNT_LABEL(user)
MCOUNT_LABEL(btrap)

#define	TRAP(a)		pushl $(a) ; jmp alltraps

IDTVEC(div)
	pushl $0; TRAP(T_DIVIDE)
IDTVEC(dbg)
	pushl $0; TRAP(T_TRCTRAP)
IDTVEC(nmi)
	pushl $0; TRAP(T_NMI)
IDTVEC(bpt)
	pushl $0; TRAP(T_BPTFLT)
IDTVEC(dtrace_ret)
	pushl $0; TRAP(T_DTRACE_RET)
IDTVEC(ofl)
	pushl $0; TRAP(T_OFLOW)
IDTVEC(bnd)
	pushl $0; TRAP(T_BOUND)
#ifndef KDTRACE_HOOKS
IDTVEC(ill)
	pushl $0; TRAP(T_PRIVINFLT)
#endif
IDTVEC(dna)
	pushl $0; TRAP(T_DNA)
IDTVEC(fpusegm)
	pushl $0; TRAP(T_FPOPFLT)
IDTVEC(tss)
	TRAP(T_TSSFLT)
IDTVEC(missing)
	TRAP(T_SEGNPFLT)
IDTVEC(stk)
	TRAP(T_STKFLT)
IDTVEC(prot)
	TRAP(T_PROTFLT)
IDTVEC(page)
	TRAP(T_PAGEFLT)
IDTVEC(mchk)
	pushl $0; TRAP(T_MCHK)
IDTVEC(rsvd)
	pushl $0; TRAP(T_RESERVED)
IDTVEC(fpu)
	pushl $0; TRAP(T_ARITHTRAP)
IDTVEC(align)
	TRAP(T_ALIGNFLT)
IDTVEC(xmm)
	pushl $0; TRAP(T_XMMFLT)

	/*
	 * All traps except ones for syscalls jump to alltraps.  If
	 * interrupts were enabled when the trap occurred, then interrupts
	 * are enabled now if the trap was through a trap gate, else
	 * disabled if the trap was through an interrupt gate.  Note that
	 * int0x80_syscall is a trap gate.   Interrupt gates are used by
	 * page faults, non-maskable interrupts, debug and breakpoint
	 * exceptions.
	 */
	SUPERALIGN_TEXT
	.globl	alltraps
	.type	alltraps,@function
alltraps:
	pushal
	pushl	$0
	movw	%ds,(%esp)
	pushl	$0
	movw	%es,(%esp)
	pushl	$0
	movw	%fs,(%esp)
alltraps_with_regs_pushed:
	SET_KERNEL_SREGS
	cld
	FAKE_MCOUNT(TF_EIP(%esp))
calltrap:
	pushl	%esp
	call	trap
	add	$4, %esp

	/*
	 * Return via doreti to handle ASTs.
	 */
	MEXITCOUNT
	jmp	doreti

/*
 * Privileged instruction fault.
 */
#ifdef KDTRACE_HOOKS
	SUPERALIGN_TEXT
IDTVEC(ill)
	/* Check if there is no DTrace hook registered. */
	cmpl	$0,dtrace_invop_jump_addr
	je	norm_ill

	/* Check if this is a user fault. */
	cmpl	$GSEL_KPL, 4(%esp)	/* Check the code segment. */

	/* If so, just handle it as a normal trap. */
	jne	norm_ill

	/*
	 * This is a kernel instruction fault that might have been caused
	 * by a DTrace provider.
	 */
	pushal				/* Push all registers onto the stack. */

	/*
	 * Set our jump address for the jump back in the event that
	 * the exception wasn't caused by DTrace at all.
	 */
	movl	$norm_ill, dtrace_invop_calltrap_addr

	/* Jump to the code hooked in by DTrace. */
	jmpl	*dtrace_invop_jump_addr

	/*
	 * Process the instruction fault in the normal way.
	 */
norm_ill:
	pushl $0
	TRAP(T_PRIVINFLT)
#endif

/*
 * Call gate entry for syscalls (lcall 7,0).
 * This is used by FreeBSD 1.x a.out executables and "old" NetBSD executables.
 *
 * The intersegment call has been set up to specify one dummy parameter.
 * This leaves a place to put eflags so that the call frame can be
 * converted to a trap frame. Note that the eflags is (semi-)bogusly
 * pushed into (what will be) tf_err and then copied later into the
 * final spot. It has to be done this way because esp can't be just
 * temporarily altered for the pushfl - an interrupt might come in
 * and clobber the saved cs/eip.
 */
	SUPERALIGN_TEXT
IDTVEC(lcall_syscall)
	pushfl				/* save eflags */
	popl	8(%esp)			/* shuffle into tf_eflags */
	pushl	$7			/* sizeof "lcall 7,0" */
	subl	$4,%esp			/* skip over tf_trapno */
	pushal
	pushl	$0
	movw	%ds,(%esp)
	pushl	$0
	movw	%es,(%esp)
	pushl	$0
	movw	%fs,(%esp)
	SET_KERNEL_SREGS
	cld
	FAKE_MCOUNT(TF_EIP(%esp))
	pushl	%esp
	call	syscall
	add	$4, %esp
	MEXITCOUNT
	jmp	doreti

/*
 * Trap gate entry for syscalls (int 0x80).
 * This is used by FreeBSD ELF executables, "new" NetBSD executables, and all
 * Linux executables.
 *
 * Even though the name says 'int0x80', this is actually a trap gate, not an
 * interrupt gate.  Thus interrupts are enabled on entry just as they are for
 * a normal syscall.
 */
	SUPERALIGN_TEXT
IDTVEC(int0x80_syscall)
	pushl	$2			/* sizeof "int 0x80" */
	subl	$4,%esp			/* skip over tf_trapno */
	pushal
	pushl	$0
	movw	%ds,(%esp)
	pushl	$0
	movw	%es,(%esp)
	pushl	$0
	movw	%fs,(%esp)
	SET_KERNEL_SREGS
	cld
	FAKE_MCOUNT(TF_EIP(%esp))
	pushl	%esp
	call	syscall
	add	$4, %esp
	MEXITCOUNT
	jmp	doreti

ENTRY(fork_trampoline)
	pushl	%esp			/* trapframe pointer */
	pushl	%ebx			/* arg1 */
	pushl	%esi			/* function */
	call	fork_exit
	addl	$12,%esp
	/* cut from syscall */

	/*
	 * Return via doreti to handle ASTs.
	 */
	MEXITCOUNT
	jmp	doreti


/*
 * To efficiently implement classification of trap and interrupt handlers
 * for profiling, there must be only trap handlers between the labels btrap
 * and bintr, and only interrupt handlers between the labels bintr and
 * eintr.  This is implemented (partly) by including files that contain
 * some of the handlers.  Before including the files, set up a normal asm
 * environment so that the included files doen't need to know that they are
 * included.
 */

	.data
	.p2align 4
	.text
	SUPERALIGN_TEXT
MCOUNT_LABEL(bintr)

#ifdef DEV_ATPIC
#include <i386/i386/atpic_vector.s>
#endif

#if defined(DEV_APIC) && defined(DEV_ATPIC)
	.data
	.p2align 4
	.text
	SUPERALIGN_TEXT
#endif

#ifdef DEV_APIC
#include <i386/i386/apic_vector.s>
#endif

	.data
	.p2align 4
	.text
	SUPERALIGN_TEXT
#include <i386/i386/vm86bios.s>

	.text
MCOUNT_LABEL(eintr)

/*
 * void doreti(struct trapframe)
 *
 * Handle return from interrupts, traps and syscalls.
 */
	.text
	SUPERALIGN_TEXT
	.type	doreti,@function
	.globl	doreti
doreti:
	FAKE_MCOUNT($bintr)		/* init "from" bintr -> doreti */
doreti_next:
	/*
	 * Check if ASTs can be handled now.  ASTs cannot be safely
	 * processed when returning from an NMI.
	 */
	cmpb	$T_NMI,TF_TRAPNO(%esp)
#ifdef HWPMC_HOOKS
	je	doreti_nmi
#else
	je	doreti_exit
#endif
	/*
	 * PSL_VM must be checked first since segment registers only
	 * have an RPL in non-VM86 mode.
	 * ASTs can not be handled now if we are in a vm86 call.
	 */
	testl	$PSL_VM,TF_EFLAGS(%esp)
	jz	doreti_notvm86
	movl	PCPU(CURPCB),%ecx
	testl	$PCB_VM86CALL,PCB_FLAGS(%ecx)
	jz	doreti_ast
	jmp	doreti_exit

doreti_notvm86:
	testb	$SEL_RPL_MASK,TF_CS(%esp) /* are we returning to user mode? */
	jz	doreti_exit		/* can't handle ASTs now if not */

doreti_ast:
	/*
	 * Check for ASTs atomically with returning.  Disabling CPU
	 * interrupts provides sufficient locking even in the SMP case,
	 * since we will be informed of any new ASTs by an IPI.
	 */
	cli
	movl	PCPU(CURTHREAD),%eax
	testl	$TDF_ASTPENDING | TDF_NEEDRESCHED,TD_FLAGS(%eax)
	je	doreti_exit
	sti
	pushl	%esp			/* pass a pointer to the trapframe */
	call	ast
	add	$4,%esp
	jmp	doreti_ast

	/*
	 * doreti_exit:	pop registers, iret.
	 *
	 *	The segment register pop is a special case, since it may
	 *	fault if (for example) a sigreturn specifies bad segment
	 *	registers.  The fault is handled in trap.c.
	 */
doreti_exit:
	MEXITCOUNT

	.globl	doreti_popl_fs
doreti_popl_fs:
	popl	%fs
	.globl	doreti_popl_es
doreti_popl_es:
	popl	%es
	.globl	doreti_popl_ds
doreti_popl_ds:
	popl	%ds
	popal
	addl	$8,%esp
	.globl	doreti_iret
doreti_iret:
	iret

	/*
	 * doreti_iret_fault and friends.  Alternative return code for
	 * the case where we get a fault in the doreti_exit code
	 * above.  trap() (i386/i386/trap.c) catches this specific
	 * case, sends the process a signal and continues in the
	 * corresponding place in the code below.
	 */
	ALIGN_TEXT
	.globl	doreti_iret_fault
doreti_iret_fault:
	subl	$8,%esp
	pushal
	pushl	$0
	movw	%ds,(%esp)
	.globl	doreti_popl_ds_fault
doreti_popl_ds_fault:
	pushl	$0
	movw	%es,(%esp)
	.globl	doreti_popl_es_fault
doreti_popl_es_fault:
	pushl	$0
	movw	%fs,(%esp)
	.globl	doreti_popl_fs_fault
doreti_popl_fs_fault:
	sti
	movl	$0,TF_ERR(%esp)	/* XXX should be the error code */
	movl	$T_PROTFLT,TF_TRAPNO(%esp)
	jmp	alltraps_with_regs_pushed
#ifdef HWPMC_HOOKS
doreti_nmi:
	/*
	 * Since we are returning from an NMI, check if the current trap
	 * was from user mode and if so whether the current thread
	 * needs a user call chain capture.
	 */
	testb	$SEL_RPL_MASK,TF_CS(%esp)
	jz	doreti_exit
	movl	PCPU(CURTHREAD),%eax	/* curthread present? */
	orl	%eax,%eax
	jz	doreti_exit
	testl	$TDP_CALLCHAIN,TD_PFLAGS(%eax) /* flagged for capture? */
	jz	doreti_exit
	/*
	 * Take the processor out of NMI mode by executing a fake "iret".
	 */
	pushfl
	pushl	%cs
	pushl	$outofnmi
	iret
outofnmi:
	/*
	 * Call the callchain capture hook after turning interrupts back on.
	 */
	movl	pmc_hook,%ecx
	orl	%ecx,%ecx
	jz	doreti_exit
	pushl	%esp			/* frame pointer */
	pushl	$PMC_FN_USER_CALLCHAIN	/* command */
	movl	PCPU(CURTHREAD),%eax
	pushl	%eax			/* curthread */
	sti
	call	*%ecx
	addl	$12,%esp
	jmp	doreti_ast
	ENTRY(end_exceptions)
#endif
