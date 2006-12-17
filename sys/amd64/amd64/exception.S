/*-
 * Copyright (c) 1989, 1990 William F. Jolitz.
 * Copyright (c) 1990 The Regents of the University of California.
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

#include "opt_atpic.h"
#include "opt_compat.h"

#include <machine/asmacros.h>
#include <machine/psl.h>
#include <machine/trap.h>

#include "assym.s"

	.text

/*****************************************************************************/
/* Trap handling                                                             */
/*****************************************************************************/
/*
 * Trap and fault vector routines.
 *
 * All traps are 'interrupt gates', SDT_SYSIGT.  An interrupt gate pushes
 * state on the stack but also disables interrupts.  This is important for
 * us for the use of the swapgs instruction.  We cannot be interrupted
 * until the GS.base value is correct.  For most traps, we automatically
 * then enable interrupts if the interrupted context had them enabled.
 * This is equivalent to the i386 port's use of SDT_SYS386TGT.
 *
 * The cpu will push a certain amount of state onto the kernel stack for
 * the current process.  See amd64/include/frame.h.  
 * This includes the current RFLAGS (status register, which includes 
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

/* Traps that we leave interrupts disabled for.. */
#define	TRAP_NOEN(a)	\
	subq $TF_RIP,%rsp; \
	movq $(a),TF_TRAPNO(%rsp) ; \
	movq $0,TF_ADDR(%rsp) ; \
	movq $0,TF_ERR(%rsp) ; \
	jmp alltraps_noen
IDTVEC(dbg)
	TRAP_NOEN(T_TRCTRAP)
IDTVEC(bpt)
	TRAP_NOEN(T_BPTFLT)

/* Regular traps; The cpu does not supply tf_err for these. */
#define	TRAP(a)	 \
	subq $TF_RIP,%rsp; \
	movq $(a),TF_TRAPNO(%rsp) ; \
	movq $0,TF_ADDR(%rsp) ; \
	movq $0,TF_ERR(%rsp) ; \
	jmp alltraps
IDTVEC(div)
	TRAP(T_DIVIDE)
IDTVEC(ofl)
	TRAP(T_OFLOW)
IDTVEC(bnd)
	TRAP(T_BOUND)
IDTVEC(ill)
	TRAP(T_PRIVINFLT)
IDTVEC(dna)
	TRAP(T_DNA)
IDTVEC(fpusegm)
	TRAP(T_FPOPFLT)
IDTVEC(mchk)
	TRAP(T_MCHK)
IDTVEC(rsvd)
	TRAP(T_RESERVED)
IDTVEC(fpu)
	TRAP(T_ARITHTRAP)
IDTVEC(xmm)
	TRAP(T_XMMFLT)

/* This group of traps have tf_err already pushed by the cpu */
#define	TRAP_ERR(a)	\
	subq $TF_ERR,%rsp; \
	movq $(a),TF_TRAPNO(%rsp) ; \
	movq $0,TF_ADDR(%rsp) ; \
	jmp alltraps
IDTVEC(tss)
	TRAP_ERR(T_TSSFLT)
IDTVEC(missing)
	TRAP_ERR(T_SEGNPFLT)
IDTVEC(stk)
	TRAP_ERR(T_STKFLT)
IDTVEC(align)
	TRAP_ERR(T_ALIGNFLT)

	/*
	 * alltraps entry point.  Use swapgs if this is the first time in the
	 * kernel from userland.  Reenable interrupts if they were enabled
	 * before the trap.  This approximates SDT_SYS386TGT on the i386 port.
	 */

	SUPERALIGN_TEXT
	.globl	alltraps
	.type	alltraps,@function
alltraps:
	testb	$SEL_RPL_MASK,TF_CS(%rsp) /* Did we come from kernel? */
	jz	alltraps_testi		/* already running with kernel GS.base */
	swapgs
alltraps_testi:
	testl	$PSL_I,TF_RFLAGS(%rsp)
	jz	alltraps_pushregs
	sti
alltraps_pushregs:
	movq	%rdi,TF_RDI(%rsp)
alltraps_pushregs_no_rdi:
	movq	%rsi,TF_RSI(%rsp)
	movq	%rdx,TF_RDX(%rsp)
	movq	%rcx,TF_RCX(%rsp)
	movq	%r8,TF_R8(%rsp)
	movq	%r9,TF_R9(%rsp)
	movq	%rax,TF_RAX(%rsp)
	movq	%rbx,TF_RBX(%rsp)
	movq	%rbp,TF_RBP(%rsp)
	movq	%r10,TF_R10(%rsp)
	movq	%r11,TF_R11(%rsp)
	movq	%r12,TF_R12(%rsp)
	movq	%r13,TF_R13(%rsp)
	movq	%r14,TF_R14(%rsp)
	movq	%r15,TF_R15(%rsp)
	FAKE_MCOUNT(TF_RIP(%rsp))
	.globl	calltrap
	.type	calltrap,@function
calltrap:
	movq	%rsp, %rdi
	call	trap
	MEXITCOUNT
	jmp	doreti			/* Handle any pending ASTs */

	/*
	 * alltraps_noen entry point.  Unlike alltraps above, we want to
	 * leave the interrupts disabled.  This corresponds to
	 * SDT_SYS386IGT on the i386 port.
	 */
	SUPERALIGN_TEXT
	.globl	alltraps_noen
	.type	alltraps_noen,@function
alltraps_noen:
	testb	$SEL_RPL_MASK,TF_CS(%rsp) /* Did we come from kernel? */
	jz	alltraps_pushregs	/* already running with kernel GS.base */
	swapgs
	jmp	alltraps_pushregs

IDTVEC(dblfault)
	subq	$TF_ERR,%rsp
	movq	$T_DOUBLEFLT,TF_TRAPNO(%rsp)
	testb	$SEL_RPL_MASK,TF_CS(%rsp) /* Did we come from kernel? */
	jz	1f			/* already running with kernel GS.base */
	swapgs
1:	call	dblfault_handler
2:	hlt
	jmp	2b

IDTVEC(page)
	subq	$TF_ERR,%rsp
	movq	$T_PAGEFLT,TF_TRAPNO(%rsp)
	testb	$SEL_RPL_MASK,TF_CS(%rsp) /* Did we come from kernel? */
	jz	1f			/* already running with kernel GS.base */
	swapgs
1:
	movq	%rdi,TF_RDI(%rsp)	/* free up a GP register */
	movq	%cr2,%rdi		/* preserve %cr2 before ..  */
	movq	%rdi,TF_ADDR(%rsp)	/* enabling interrupts. */
	testl	$PSL_I,TF_RFLAGS(%rsp)
	jz	alltraps_pushregs_no_rdi
	sti
	jmp	alltraps_pushregs_no_rdi

	/*
	 * We have to special-case this one.  If we get a trap in doreti() at
	 * the iretq stage, we'll reenter with the wrong gs state.  We'll have
	 * to do a special the swapgs in this case even coming from the kernel.
	 * XXX linux has a trap handler for their equivalent of load_gs().
	 */
IDTVEC(prot)
	subq	$TF_ERR,%rsp
	movq	$T_PROTFLT,TF_TRAPNO(%rsp)
	movq	$0,TF_ADDR(%rsp)
	movq	%rdi,TF_RDI(%rsp)	/* free up a GP register */
	leaq	doreti_iret(%rip),%rdi
	cmpq	%rdi,TF_RIP(%rsp)
	je	2f			/* kernel but with user gsbase!! */
	testb	$SEL_RPL_MASK,TF_CS(%rsp) /* Did we come from kernel? */
	jz	1f			/* already running with kernel GS.base */
2:
	swapgs
1:
	testl	$PSL_I,TF_RFLAGS(%rsp)
	jz	alltraps_pushregs_no_rdi
	sti
	jmp	alltraps_pushregs_no_rdi

/*
 * Fast syscall entry point.  We enter here with just our new %cs/%ss set,
 * and the new privilige level.  We are still running on the old user stack
 * pointer.  We have to juggle a few things around to find our stack etc.
 * swapgs gives us access to our PCPU space only.
 */
IDTVEC(fast_syscall)
	swapgs
	movq	%rsp,PCPU(SCRATCH_RSP)
	movq	PCPU(RSP0),%rsp
	/* Now emulate a trapframe. Make the 8 byte alignment odd for call. */
	subq	$TF_SIZE,%rsp
	/* defer TF_RSP till we have a spare register */
	movq	%r11,TF_RFLAGS(%rsp)
	movq	%rcx,TF_RIP(%rsp)	/* %rcx original value is in %r10 */
	movq	PCPU(SCRATCH_RSP),%r11	/* %r11 already saved */
	movq	%r11,TF_RSP(%rsp)	/* user stack pointer */
	sti
	movq	$KUDSEL,TF_SS(%rsp)
	movq	$KUCSEL,TF_CS(%rsp)
	movq	$2,TF_ERR(%rsp)
	movq	%rdi,TF_RDI(%rsp)	/* arg 1 */
	movq	%rsi,TF_RSI(%rsp)	/* arg 2 */
	movq	%rdx,TF_RDX(%rsp)	/* arg 3 */
	movq	%r10,TF_RCX(%rsp)	/* arg 4 */
	movq	%r8,TF_R8(%rsp)		/* arg 5 */
	movq	%r9,TF_R9(%rsp)		/* arg 6 */
	movq	%rax,TF_RAX(%rsp)	/* syscall number */
	movq	%rbx,TF_RBX(%rsp)	/* C preserved */
	movq	%rbp,TF_RBP(%rsp)	/* C preserved */
	movq	%r12,TF_R12(%rsp)	/* C preserved */
	movq	%r13,TF_R13(%rsp)	/* C preserved */
	movq	%r14,TF_R14(%rsp)	/* C preserved */
	movq	%r15,TF_R15(%rsp)	/* C preserved */
	FAKE_MCOUNT(TF_RIP(%rsp))
	movq	%rsp, %rdi
	call	syscall
	movq	PCPU(CURPCB),%rax
	testq	$PCB_FULLCTX,PCB_FLAGS(%rax)
	jne	3f
1:	/* Check for and handle AST's on return to userland */
	cli
	movq	PCPU(CURTHREAD),%rax
	testl	$TDF_ASTPENDING | TDF_NEEDRESCHED,TD_FLAGS(%rax)
	je	2f
	sti
	movq	%rsp, %rdi
	call	ast
	jmp	1b
2:	/* restore preserved registers */
	MEXITCOUNT
	movq	TF_RDI(%rsp),%rdi	/* bonus; preserve arg 1 */
	movq	TF_RSI(%rsp),%rsi	/* bonus: preserve arg 2 */
	movq	TF_RDX(%rsp),%rdx	/* return value 2 */
	movq	TF_RAX(%rsp),%rax	/* return value 1 */
	movq	TF_RBX(%rsp),%rbx	/* C preserved */
	movq	TF_RBP(%rsp),%rbp	/* C preserved */
	movq	TF_R12(%rsp),%r12	/* C preserved */
	movq	TF_R13(%rsp),%r13	/* C preserved */
	movq	TF_R14(%rsp),%r14	/* C preserved */
	movq	TF_R15(%rsp),%r15	/* C preserved */
	movq	TF_RFLAGS(%rsp),%r11	/* original %rflags */
	movq	TF_RIP(%rsp),%rcx	/* original %rip */
	movq	TF_RSP(%rsp),%r9	/* user stack pointer */
	movq	%r9,%rsp		/* original %rsp */
	swapgs
	sysretq
3:	/* Requested full context restore, use doreti for that */
	andq	$~PCB_FULLCTX,PCB_FLAGS(%rax)
	MEXITCOUNT
	jmp	doreti

/*
 * Here for CYA insurance, in case a "syscall" instruction gets
 * issued from 32 bit compatability mode. MSR_CSTAR has to point
 * to *something* if EFER_SCE is enabled.
 */
IDTVEC(fast_syscall32)
	sysret

/*
 * NMI handling is special.
 *
 * First, NMIs do not respect the state of the processor's RFLAGS.IF
 * bit and the NMI handler may be invoked at any time, including when
 * the processor is in a critical section with RFLAGS.IF == 0.  In
 * particular, this means that the processor's GS.base values could be
 * inconsistent on entry to the handler, and so we need to read
 * MSR_GSBASE to determine if a 'swapgs' is needed.  We use '%ebx', a
 * C-preserved register, to remember whether to swap GS back on the
 * exit path.
 *
 * Second, the processor treats NMIs specially, blocking further NMIs
 * until an 'iretq' instruction is executed.  We therefore need to
 * execute the NMI handler with interrupts disabled to prevent a
 * nested interrupt from executing an 'iretq' instruction and
 * inadvertently taking the processor out of NMI mode.
 */

IDTVEC(nmi)
	subq	$TF_RIP,%rsp
	movq	$(T_NMI),TF_TRAPNO(%rsp)
	movq	$0,TF_ADDR(%rsp)
	movq	$0,TF_ERR(%rsp)
	movq	%rdi,TF_RDI(%rsp)
	movq	%rsi,TF_RSI(%rsp)
	movq	%rdx,TF_RDX(%rsp)
	movq	%rcx,TF_RCX(%rsp)
	movq	%r8,TF_R8(%rsp)
	movq	%r9,TF_R9(%rsp)
	movq	%rax,TF_RAX(%rsp)
	movq	%rbx,TF_RBX(%rsp)
	movq	%rbp,TF_RBP(%rsp)
	movq	%r10,TF_R10(%rsp)
	movq	%r11,TF_R11(%rsp)
	movq	%r12,TF_R12(%rsp)
	movq	%r13,TF_R13(%rsp)
	movq	%r14,TF_R14(%rsp)
	movq	%r15,TF_R15(%rsp)
	xorl	%ebx,%ebx
	testb	$SEL_RPL_MASK,TF_CS(%rsp)
	jnz	nmi_needswapgs		/* we came from userland */
	movl	$MSR_GSBASE,%ecx
	rdmsr
	cmpl	$VM_MAXUSER_ADDRESS >> 32,%edx
	jae	nmi_calltrap		/* GS.base holds a kernel VA */
nmi_needswapgs:
	incl	%ebx
	swapgs
/* Note: this label is also used by ddb and gdb: */
nmi_calltrap:
	FAKE_MCOUNT(TF_RIP(%rsp))
	call	trap
	MEXITCOUNT
	testl	%ebx,%ebx
	jz	nmi_restoreregs
	swapgs
nmi_restoreregs:
	movq	TF_RDI(%rsp),%rdi
	movq	TF_RSI(%rsp),%rsi
	movq	TF_RDX(%rsp),%rdx
	movq	TF_RCX(%rsp),%rcx
	movq	TF_R8(%rsp),%r8
	movq	TF_R9(%rsp),%r9
	movq	TF_RAX(%rsp),%rax
	movq	TF_RBX(%rsp),%rbx
	movq	TF_RBP(%rsp),%rbp
	movq	TF_R10(%rsp),%r10
	movq	TF_R11(%rsp),%r11
	movq	TF_R12(%rsp),%r12
	movq	TF_R13(%rsp),%r13
	movq	TF_R14(%rsp),%r14
	movq	TF_R15(%rsp),%r15
	addq	$TF_RIP,%rsp
	iretq

ENTRY(fork_trampoline)
	movq	%r12, %rdi		/* function */
	movq	%rbx, %rsi		/* arg1 */
	movq	%rsp, %rdx		/* trapframe pointer */
	call	fork_exit
	MEXITCOUNT
	jmp	doreti			/* Handle any ASTs */

/*
 * To efficiently implement classification of trap and interrupt handlers
 * for profiling, there must be only trap handlers between the labels btrap
 * and bintr, and only interrupt handlers between the labels bintr and
 * eintr.  This is implemented (partly) by including files that contain
 * some of the handlers.  Before including the files, set up a normal asm
 * environment so that the included files doen't need to know that they are
 * included.
 */

#ifdef COMPAT_IA32
	.data
	.p2align 4
	.text
	SUPERALIGN_TEXT

#include <amd64/ia32/ia32_exception.S>
#endif

	.data
	.p2align 4
	.text
	SUPERALIGN_TEXT
MCOUNT_LABEL(bintr)

#include <amd64/amd64/apic_vector.S>

#ifdef DEV_ATPIC
	.data
	.p2align 4
	.text
	SUPERALIGN_TEXT

#include <amd64/isa/atpic_vector.S>
#endif

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
doreti:
	FAKE_MCOUNT($bintr)		/* init "from" bintr -> doreti */
	/*
	 * Check if ASTs can be handled now.
	 */
	testb	$SEL_RPL_MASK,TF_CS(%rsp) /* are we returning to user mode? */
	jz	doreti_exit		/* can't handle ASTs now if not */

doreti_ast:
	/*
	 * Check for ASTs atomically with returning.  Disabling CPU
	 * interrupts provides sufficient locking eve in the SMP case,
	 * since we will be informed of any new ASTs by an IPI.
	 */
	cli
	movq	PCPU(CURTHREAD),%rax
	testl	$TDF_ASTPENDING | TDF_NEEDRESCHED,TD_FLAGS(%rax)
	je	doreti_exit
	sti
	movq	%rsp, %rdi			/* pass a pointer to the trapframe */
	call	ast
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
	movq	TF_RDI(%rsp),%rdi
	movq	TF_RSI(%rsp),%rsi
	movq	TF_RDX(%rsp),%rdx
	movq	TF_RCX(%rsp),%rcx
	movq	TF_R8(%rsp),%r8
	movq	TF_R9(%rsp),%r9
	movq	TF_RAX(%rsp),%rax
	movq	TF_RBX(%rsp),%rbx
	movq	TF_RBP(%rsp),%rbp
	movq	TF_R10(%rsp),%r10
	movq	TF_R11(%rsp),%r11
	movq	TF_R12(%rsp),%r12
	movq	TF_R13(%rsp),%r13
	movq	TF_R14(%rsp),%r14
	movq	TF_R15(%rsp),%r15
	testb	$SEL_RPL_MASK,TF_CS(%rsp) /* Did we come from kernel? */
	jz	1f			/* keep running with kernel GS.base */
	cli
	swapgs
1:	addq	$TF_RIP,%rsp		/* skip over tf_err, tf_trapno */
	.globl	doreti_iret
doreti_iret:
	iretq

	/*
	 * doreti_iret_fault.  Alternative return code for
	 * the case where we get a fault in the doreti_exit code
	 * above.  trap() (amd64/amd64/trap.c) catches this specific
	 * case, sends the process a signal and continues in the
	 * corresponding place in the code below.
	 */
	ALIGN_TEXT
	.globl	doreti_iret_fault
doreti_iret_fault:
	subq	$TF_RIP,%rsp		/* space including tf_err, tf_trapno */
	testb	$SEL_RPL_MASK,TF_CS(%rsp) /* Did we come from kernel? */
	jz	1f			/* already running with kernel GS.base */
	swapgs
1:	testl	$PSL_I,TF_RFLAGS(%rsp)
	jz	2f
	sti
2:	movq	%rdi,TF_RDI(%rsp)
	movq	%rsi,TF_RSI(%rsp)
	movq	%rdx,TF_RDX(%rsp)
	movq	%rcx,TF_RCX(%rsp)
	movq	%r8,TF_R8(%rsp)
	movq	%r9,TF_R9(%rsp)
	movq	%rax,TF_RAX(%rsp)
	movq	%rbx,TF_RBX(%rsp)
	movq	%rbp,TF_RBP(%rsp)
	movq	%r10,TF_R10(%rsp)
	movq	%r11,TF_R11(%rsp)
	movq	%r12,TF_R12(%rsp)
	movq	%r13,TF_R13(%rsp)
	movq	%r14,TF_R14(%rsp)
	movq	%r15,TF_R15(%rsp)
	movq	$T_PROTFLT,TF_TRAPNO(%rsp)
	movq	$0,TF_ERR(%rsp)	/* XXX should be the error code */
	movq	$0,TF_ADDR(%rsp)
	FAKE_MCOUNT(TF_RIP(%rsp))
	jmp	calltrap
