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
 *	$Id: exception.s,v 1.3 1994/04/02 07:00:23 davidg Exp $
 */

#include "npx.h"				/* NNPX */

#include "assym.s"				/* system defines */

#include "errno.h"				/* error return codes */

#include "machine/spl.h"			/* SWI_AST_MASK ... */

#include "machine/psl.h"			/* PSL_I */

#include "machine/trap.h"			/* trap codes */
#include "syscall.h"				/* syscall numbers */

#include "machine/asmacros.h"			/* miscellaneous macros */

#define	KDSEL		0x10			/* kernel data selector */
#define	SEL_RPL_MASK	0x0003
#define	TRAPF_CS_OFF	(13 * 4)

	.text

/*****************************************************************************/
/* Trap handling                                                             */
/*****************************************************************************/
/*
 * Trap and fault vector routines
 */
#define	IDTVEC(name)	ALIGN_TEXT ; .globl _X/**/name ; _X/**/name:
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
	.globl	bdb_/**/name/**/_ljmp ; \
bdb_/**/name/**/_ljmp: ; \
	ljmp	$0,$0 ; \
1:
#else
#define BDBTRAP(name)
#endif

#ifdef KGDB
#  define BPTTRAP(a)	testl $PSL_I,4+8(%esp) ; je 1f ; sti ; 1: ; \
			pushl $(a) ; jmp _bpttraps
#else
#  define BPTTRAP(a)	testl $PSL_I,4+8(%esp) ; je 1f ; sti ; 1: ; TRAP(a)
#endif

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
IDTVEC(dble)
	TRAP(T_DOUBLEFLT)
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
IDTVEC(rsvd)
	pushl $0; TRAP(T_RESERVED)
IDTVEC(fpu)
#if NNPX > 0
	/*
	 * Handle like an interrupt so that we can call npxintr to clear the
	 * error.  It would be better to handle npx interrupts as traps but
	 * this is difficult for nested interrupts.
	 */
	pushl	$0				/* dumby error code */
	pushl	$0				/* dumby trap type */
	pushal
	pushl	%ds
	pushl	%es				/* now the stack frame is a trap frame */
	movl	$KDSEL,%eax
	movl	%ax,%ds
	movl	%ax,%es
	FAKE_MCOUNT(12*4(%esp))
	movl	_cpl,%eax
	pushl	%eax
	pushl	$0				/* dummy unit to finish building intr frame */
	incl	_cnt+V_TRAP
	orl	$SWI_AST_MASK,%eax
	movl	%eax,_cpl
	call	_npxintr
	MEXITCOUNT
	jmp	_doreti
#else	/* NNPX > 0 */
	pushl $0; TRAP(T_ARITHTRAP)
#endif	/* NNPX > 0 */
	/* 17 - 31 reserved for future exp */
IDTVEC(rsvd0)
	pushl $0; TRAP(17)
IDTVEC(rsvd1)
	pushl $0; TRAP(18)
IDTVEC(rsvd2)
	pushl $0; TRAP(19)
IDTVEC(rsvd3)
	pushl $0; TRAP(20)
IDTVEC(rsvd4)
	pushl $0; TRAP(21)
IDTVEC(rsvd5)
	pushl $0; TRAP(22)
IDTVEC(rsvd6)
	pushl $0; TRAP(23)
IDTVEC(rsvd7)
	pushl $0; TRAP(24)
IDTVEC(rsvd8)
	pushl $0; TRAP(25)
IDTVEC(rsvd9)
	pushl $0; TRAP(26)
IDTVEC(rsvd10)
	pushl $0; TRAP(27)
IDTVEC(rsvd11)
	pushl $0; TRAP(28)
IDTVEC(rsvd12)
	pushl $0; TRAP(29)
IDTVEC(rsvd13)
	pushl $0; TRAP(30)
IDTVEC(rsvd14)
	pushl $0; TRAP(31)

	SUPERALIGN_TEXT
_alltraps:
	pushal
	pushl	%ds
	pushl	%es
	movl	$KDSEL,%eax
	movl	%ax,%ds
	movl	%ax,%es
	FAKE_MCOUNT(12*4(%esp))
calltrap:
	FAKE_MCOUNT(_btrap)			/* init "from" _btrap -> calltrap */
	incl	_cnt+V_TRAP
	orl	$SWI_AST_MASK,_cpl
	call	_trap
	/*
	 * There was no place to save the cpl so we have to recover it
	 * indirectly.  For traps from user mode it was 0, and for traps
	 * from kernel mode Oring SWI_AST_MASK into it didn't change it.
	 */
	subl	%eax,%eax
	testb	$SEL_RPL_MASK,TRAPF_CS_OFF(%esp)
	jne	1f
	movl	_cpl,%eax
1:
	/*
	 * Return via _doreti to handle ASTs.  Have to change trap frame
	 * to interrupt frame.
	 */
	pushl	%eax
	subl	$4,%esp
	MEXITCOUNT
	jmp	_doreti

#ifdef KGDB
/*
 * This code checks for a kgdb trap, then falls through
 * to the regular trap code.
 */
	SUPERALIGN_TEXT
_bpttraps:
	pushal
	pushl	%ds
	pushl	%es
	movl	$KDSEL,%eax
	movl	%ax,%ds
	movl	%ax,%es
	FAKE_MCOUNT(12*4(%esp))
	testb	$SEL_RPL_MASK,TRAPF_CS_OFF(%esp) /* non-kernel mode? */
	jne	calltrap			/* yes */
	call	_kgdb_trap_glue
	MEXITCOUNT
	jmp	calltrap
#endif

/*
 * Call gate entry for syscall
 */
	SUPERALIGN_TEXT
IDTVEC(syscall)
	pushfl					/* Room for tf_err */
	pushfl					/* Room for tf_trapno */
	pushal
	pushl	%ds
	pushl	%es
	movl	$KDSEL,%eax			/* switch to kernel segments */
	movl	%ax,%ds
	movl	%ax,%es
	movl	TF_ERR(%esp),%eax		/* copy eflags from tf_err to fs_eflags */
	movl	%eax,TF_EFLAGS(%esp)
	FAKE_MCOUNT(12*4(%esp))
	incl	_cnt+V_SYSCALL
	movl	$SWI_AST_MASK,_cpl
	call	_syscall
	/*
	 * Return via _doreti to handle ASTs.
	 */
	pushl	$0				/* cpl to restore */
	subl	$4,%esp
	MEXITCOUNT
	jmp	_doreti

/*
 * include generated interrupt vectors and ISA intr code
 */
#include "i386/isa/vector.s"
#include "i386/isa/icu.s"
