/*-
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
 *	$Id: exception.s,v 1.45 1997/10/10 09:43:57 peter Exp $
 */

#include "npx.h"
#include "opt_vm86.h"

#include <machine/asmacros.h>
#include <machine/ipl.h>
#include <machine/lock.h>
#ifdef VM86
#include <machine/psl.h>
#endif
#include <machine/trap.h>
#ifdef SMP
#include <machine/smptests.h>		/** CPL_AND_CML, REAL_ */
#endif

#include "assym.s"

#ifndef SMP
#define ECPL_LOCK			/* make these nops */
#define ECPL_UNLOCK
#define ICPL_LOCK
#define ICPL_UNLOCK
#define FAST_ICPL_UNLOCK
#define AICPL_LOCK
#define AICPL_UNLOCK
#define AVCPL_LOCK
#define AVCPL_UNLOCK
#endif /* SMP */

#define	KCSEL		0x08		/* kernel code selector */
#define	KDSEL		0x10		/* kernel data selector */
#define	SEL_RPL_MASK	0x0003
#define	TRAPF_CS_OFF	(13 * 4)

	.text

/*****************************************************************************/
/* Trap handling                                                             */
/*****************************************************************************/
/*
 * Trap and fault vector routines
 */
#define	IDTVEC(name)	ALIGN_TEXT; .globl __CONCAT(_X,name); __CONCAT(_X,name):
#define	TRAP(a)		pushl $(a) ; jmp _alltraps

/*
 * XXX - debugger traps are now interrupt gates so at least bdb doesn't lose
 * control.  The sti's give the standard losing behaviour for ddb and kgdb.
 */
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

#define BPTTRAP(a)	testl $PSL_I,4+8(%esp) ; je 1f ; sti ; 1: ; TRAP(a)

MCOUNT_LABEL(user)
MCOUNT_LABEL(btrap)

IDTVEC(div)
	pushl $0; TRAP(T_DIVIDE)
IDTVEC(dbg)
	BDBTRAP(dbg)
	pushl $0; BPTTRAP(T_TRCTRAP)
IDTVEC(nmi)
	pushl $0; TRAP(T_NMI)
IDTVEC(bpt)
	BDBTRAP(bpt)
	pushl $0; BPTTRAP(T_BPTFLT)
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
#if NNPX > 0
	/*
	 * Handle like an interrupt (except for accounting) so that we can
	 * call npxintr to clear the error.  It would be better to handle
	 * npx interrupts as traps.  This used to be difficult for nested
	 * interrupts, but now it is fairly easy - mask nested ones the
	 * same as SWI_AST's.
	 */
	pushl	$0			/* dummy error code */
	pushl	$0			/* dummy trap type */
	pushal
	pushl	%ds
	pushl	%es			/* now stack frame is a trap frame */
	movl	$KDSEL,%eax
	movl	%ax,%ds
	movl	%ax,%es
	FAKE_MCOUNT(12*4(%esp))

#ifdef SMP
	MPLOCKED incl _cnt+V_TRAP
	FPU_LOCK
	ECPL_LOCK
#ifdef CPL_AND_CML
	movl	_cml,%eax
	pushl	%eax			/* save original cml */
	orl	$SWI_AST_MASK,%eax
	movl	%eax,_cml
#else
	movl	_cpl,%eax
	pushl	%eax			/* save original cpl */
	orl	$SWI_AST_MASK,%eax
	movl	%eax,_cpl
#endif /* CPL_AND_CML */
	ECPL_UNLOCK
	pushl	$0			/* dummy unit to finish intr frame */
#else /* SMP */
	movl	_cpl,%eax
	pushl	%eax
	pushl	$0			/* dummy unit to finish intr frame */
	incl	_cnt+V_TRAP
	orl	$SWI_AST_MASK,%eax
	movl	%eax,_cpl
#endif /* SMP */

	call	_npxintr

	incb	_intr_nesting_level
	MEXITCOUNT
	jmp	_doreti
#else	/* NNPX > 0 */
	pushl $0; TRAP(T_ARITHTRAP)
#endif	/* NNPX > 0 */

IDTVEC(align)
	TRAP(T_ALIGNFLT)

	SUPERALIGN_TEXT
	.globl	_alltraps
_alltraps:
	pushal
	pushl	%ds
	pushl	%es
alltraps_with_regs_pushed:
	movl	$KDSEL,%eax
	movl	%ax,%ds
	movl	%ax,%es
	FAKE_MCOUNT(12*4(%esp))
calltrap:
	FAKE_MCOUNT(_btrap)		/* init "from" _btrap -> calltrap */
	MPLOCKED incl _cnt+V_TRAP
	ALIGN_LOCK
	ECPL_LOCK
#ifdef CPL_AND_CML
	orl	$SWI_AST_MASK,_cml
#else
	orl	$SWI_AST_MASK,_cpl
#endif
	ECPL_UNLOCK
	call	_trap

	/*
	 * There was no place to save the cpl so we have to recover it
	 * indirectly.  For traps from user mode it was 0, and for traps
	 * from kernel mode Oring SWI_AST_MASK into it didn't change it.
	 */
#ifndef SMP
	subl	%eax,%eax
#endif
	testb	$SEL_RPL_MASK,TRAPF_CS_OFF(%esp)
	jne	1f
#ifdef VM86
	testl	$PSL_VM,TF_EFLAGS(%esp)
	jne	1f
#endif /* VM86 */

#ifdef SMP
	ECPL_LOCK
#ifdef CPL_AND_CML
	pushl	_cml			/* XXX will this work??? */
#else
	pushl	_cpl
#endif
	ECPL_UNLOCK
	jmp	2f
1:
	pushl	$0			/* cpl to restore */
2:
#else /* SMP */
	movl	_cpl,%eax
1:
	pushl	%eax
#endif /* SMP */

	/*
	 * Return via _doreti to handle ASTs.  Have to change trap frame
	 * to interrupt frame.
	 */
	subl	$4,%esp			/* dummy unit to finish intr frame */
	MPLOCKED incb _intr_nesting_level
	MEXITCOUNT
	jmp	_doreti

/*
 * Call gate entry for syscall.
 * The intersegment call has been set up to specify one dummy parameter.
 * This leaves a place to put eflags so that the call frame can be
 * converted to a trap frame. Note that the eflags is (semi-)bogusly
 * pushed into (what will be) tf_err and then copied later into the
 * final spot. It has to be done this way because esp can't be just
 * temporarily altered for the pushfl - an interrupt might come in
 * and clobber the saved cs/eip.
 */
	SUPERALIGN_TEXT
IDTVEC(syscall)
	pushfl				/* save eflags in tf_err for now */
	subl	$4,%esp			/* skip over tf_trapno */
	pushal
	pushl	%ds
	pushl	%es
	movl	$KDSEL,%eax		/* switch to kernel segments */
	movl	%ax,%ds
	movl	%ax,%es
	movl	TF_ERR(%esp),%eax	/* copy saved eflags to final spot */
	movl	%eax,TF_EFLAGS(%esp)
	movl	$7,TF_ERR(%esp) 	/* sizeof "lcall 7,0" */
	FAKE_MCOUNT(12*4(%esp))
	MPLOCKED incl _cnt+V_SYSCALL
	SYSCALL_LOCK
	ECPL_LOCK
#ifdef CPL_AND_CML
	movl	$SWI_AST_MASK,_cml
#else
	movl	$SWI_AST_MASK,_cpl
#endif
	ECPL_UNLOCK
	call	_syscall

	/*
	 * Return via _doreti to handle ASTs.
	 */
	pushl	$0			/* cpl to restore */
	subl	$4,%esp			/* dummy unit to finish intr frame */
	movb	$1,_intr_nesting_level
	MEXITCOUNT
	jmp	_doreti

/*
 * Call gate entry for Linux/NetBSD syscall (int 0x80)
 */
	SUPERALIGN_TEXT
IDTVEC(int0x80_syscall)
	subl	$8,%esp			/* skip over tf_trapno and tf_err */
	pushal
	pushl	%ds
	pushl	%es
	movl	$KDSEL,%eax		/* switch to kernel segments */
	movl	%ax,%ds
	movl	%ax,%es
	movl	$2,TF_ERR(%esp)		/* sizeof "int 0x80" */
	FAKE_MCOUNT(12*4(%esp))
	MPLOCKED incl _cnt+V_SYSCALL
	ALTSYSCALL_LOCK
	ECPL_LOCK
#ifdef CPL_AND_CML
	movl	$SWI_AST_MASK,_cml
#else
	movl	$SWI_AST_MASK,_cpl
#endif
	ECPL_UNLOCK
	call	_syscall

	/*
	 * Return via _doreti to handle ASTs.
	 */
	pushl	$0			/* cpl to restore */
	subl	$4,%esp			/* dummy unit to finish intr frame */
	movb	$1,_intr_nesting_level
	MEXITCOUNT
	jmp	_doreti

ENTRY(fork_trampoline)
	call	_spl0
	pushl	$_runtime
	call	_microtime
	popl	%eax

	/*
	 * cpu_set_fork_handler intercepts this function call to
	 * have this call a non-return function to stay in kernel mode.
	 * initproc has it's own fork handler, but it does return.
	 */
	pushl	%ebx			/* arg1 */
	call	%esi			/* function */
	addl	$4,%esp
	/* cut from syscall */

	/*
	 * Return via _doreti to handle ASTs.
	 */
	pushl	$0			/* cpl to restore */
	subl	$4,%esp			/* dummy unit to finish intr frame */
	movb	$1,_intr_nesting_level
	MEXITCOUNT
	jmp	_doreti


/*
 * Include what was once config+isa-dependent code.
 * XXX it should be in a stand-alone file.  It's still icu-dependent and
 * belongs in i386/isa.
 */
#include "i386/isa/vector.s"

/*
 * Include what was once icu-dependent code.
 * XXX it should be merged into this file (also move the definition of
 * imen to vector.s or isa.c).
 * Before including it, set up a normal asm environment so that vector.s
 * doesn't have to know that stuff is included after it.
 */
	.data
	ALIGN_DATA
	.text
	SUPERALIGN_TEXT
#include "i386/isa/ipl.s"
