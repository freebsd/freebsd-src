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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#include "opt_npx.h"

#include <machine/asmacros.h>
#include <sys/mutex.h>
#include <machine/psl.h>
#include <machine/trap.h>
#ifdef SMP
#include <machine/smptests.h>		/** various SMP options */
#endif

#include "assym.s"

#define	SEL_RPL_MASK	0x0003

	.text

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
#define	IDTVEC(name)	ALIGN_TEXT; .globl __CONCAT(X,name); \
			.type __CONCAT(X,name),@function; __CONCAT(X,name):
#define	TRAP(a)		pushl $(a) ; jmp alltraps

#ifdef BDE_DEBUGGER
#define	BDBTRAP(name) \
	ss ; \
	cmpb	$0,_bdb_exists ; \
	je	1f ; \
	testb	$SEL_RPL_MASK,4(%esp) ; \
	jne	1f ; \
	ss ; \
	.globl	__CONCAT(__CONCAT(bdb_,name),_ljmp); \
__CONCAT(__CONCAT(bdb_,name),_ljmp): \
	ljmp	$0,$0 ; \
1:
#else
#define BDBTRAP(name)
#endif

MCOUNT_LABEL(user)
MCOUNT_LABEL(btrap)

IDTVEC(div)
	pushl $0; TRAP(T_DIVIDE)
IDTVEC(dbg)
	BDBTRAP(dbg)
	pushl $0; TRAP(T_TRCTRAP)
IDTVEC(nmi)
	pushl $0; TRAP(T_NMI)
IDTVEC(bpt)
	BDBTRAP(bpt)
	pushl $0; TRAP(T_BPTFLT)
IDTVEC(ofl)
	pushl $0; TRAP(T_OFLOW)
IDTVEC(bnd)
	pushl $0; TRAP(T_BOUND)
IDTVEC(ill)
	pushl $0; TRAP(T_PRIVINFLT)
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
	 * alltraps entry point.  Interrupts are enabled if this was a trap
	 * gate (TGT), else disabled if this was an interrupt gate (IGT).
	 * Note that int0x80_syscall is a trap gate.  Only page faults
	 * use an interrupt gate.
	 */

	SUPERALIGN_TEXT
	.globl	alltraps
	.type	alltraps,@function
alltraps:
	pushal
	pushl	%ds
	pushl	%es
	pushl	%fs
alltraps_with_regs_pushed:
	mov	$KDSEL,%ax
	mov	%ax,%ds
	mov	%ax,%es
	mov	$KPSEL,%ax
	mov	%ax,%fs
	FAKE_MCOUNT(13*4(%esp))
calltrap:
	FAKE_MCOUNT(btrap)		/* init "from" btrap -> calltrap */
	call	trap

	/*
	 * Return via doreti to handle ASTs.
	 */
	MEXITCOUNT
	jmp	doreti

/*
 * SYSCALL CALL GATE (old entry point for a.out binaries)
 *
 * The intersegment call has been set up to specify one dummy parameter.
 *
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
	pushl	%ds
	pushl	%es
	pushl	%fs
	mov	$KDSEL,%ax		/* switch to kernel segments */
	mov	%ax,%ds
	mov	%ax,%es
	mov	$KPSEL,%ax
	mov	%ax,%fs
	FAKE_MCOUNT(13*4(%esp))
	call	syscall
	MEXITCOUNT
	jmp	doreti

/*
 * Call gate entry for FreeBSD ELF and Linux/NetBSD syscall (int 0x80)
 *
 * Even though the name says 'int0x80', this is actually a TGT (trap gate)
 * rather then an IGT (interrupt gate).  Thus interrupts are enabled on
 * entry just as they are for a normal syscall.
 */
	SUPERALIGN_TEXT
IDTVEC(int0x80_syscall)
	pushl	$2			/* sizeof "int 0x80" */
	subl	$4,%esp			/* skip over tf_trapno */
	pushal
	pushl	%ds
	pushl	%es
	pushl	%fs
	mov	$KDSEL,%ax		/* switch to kernel segments */
	mov	%ax,%ds
	mov	%ax,%es
	mov	$KPSEL,%ax
	mov	%ax,%fs
	FAKE_MCOUNT(13*4(%esp))
	call	syscall
	MEXITCOUNT
	jmp	doreti

ENTRY(fork_trampoline)
	pushl	%esp			/* trapframe pointer */
	pushl	%ebx			/* arg1 */
	pushl	%esi			/* function */
	movl    PCPU(CURTHREAD),%ebx	/* setup critnest */
	movl	$1,TD_CRITNEST(%ebx)
	sti				/* enable interrupts */
	call	fork_exit
	addl	$12,%esp
	/* cut from syscall */

	/*
	 * Return via doreti to handle ASTs.
	 */
	MEXITCOUNT
	jmp	doreti


/*
 * Include vm86 call routines, which want to call doreti.
 */
#include "i386/i386/vm86bios.s"

/*
 * Include what was once config+isa-dependent code.
 * XXX it should be in a stand-alone file.  It's still icu-dependent and
 * belongs in i386/isa.
 */
#include "i386/isa/vector.s"

	.data
	ALIGN_DATA

/*
 * void doreti(struct trapframe)
 *
 * Handle return from interrupts, traps and syscalls.
 */
	.text
	SUPERALIGN_TEXT
	.type	doreti,@function
doreti:
	FAKE_MCOUNT(bintr)		/* init "from" bintr -> doreti */
doreti_next:
	/*
	 * Check if ASTs can be handled now.  PSL_VM must be checked first
	 * since segment registers only have an RPL in non-VM86 mode.
	 */
	testl	$PSL_VM,TF_EFLAGS(%esp)	/* are we in vm86 mode? */
	jz	doreti_notvm86
	cmpl	$1,in_vm86call		/* are we in a vm86 call? XXXSMP */
	jne	doreti_ast		/* can handle ASTs now if not */
  	jmp	doreti_exit

doreti_notvm86:
	testb	$SEL_RPL_MASK,TF_CS(%esp) /* are we returning to user mode? */
	jz	doreti_exit		/* can't handle ASTs now if not */

doreti_ast:
	/*
	 * Check for ASTs atomically with returning.  Disabling CPU
	 * interrupts provides sufficient locking evein the SMP case,
	 * since we will be informed of any new ASTs by an IPI.
	 */
	cli
	movl	PCPU(CURTHREAD),%eax
	movl	TD_KSE(%eax), %eax
	testl	$KEF_ASTPENDING | KEF_NEEDRESCHED,KE_FLAGS(%eax)
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
	pushl	%ds
	.globl	doreti_popl_ds_fault
doreti_popl_ds_fault:
	pushl	%es
	.globl	doreti_popl_es_fault
doreti_popl_es_fault:
	pushl	%fs
	.globl	doreti_popl_fs_fault
doreti_popl_fs_fault:
	movl	$0,TF_ERR(%esp)	/* XXX should be the error code */
	movl	$T_PROTFLT,TF_TRAPNO(%esp)
	jmp	alltraps_with_regs_pushed

#ifdef APIC_IO
#include "i386/isa/apic_ipl.s"
#else
#include "i386/isa/icu_ipl.s"
#endif /* APIC_IO */
