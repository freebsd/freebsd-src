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
 *	$Id$
 */

#include "npx.h"				/* NNPX */

#include "assym.s"				/* system defines */

#include "errno.h"				/* error return codes */

#include "i386/isa/debug.h"			/* BDE debugging macros */

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
 *
 * XXX - debugger traps are now interrupt gates so at least bdb doesn't lose
 * control.  The sti's give the standard losing behaviour for ddb and kgdb.
 */
#define	IDTVEC(name)	ALIGN_TEXT; .globl _X/**/name; _X/**/name:
#define	TRAP(a)		pushl $(a) ; jmp alltraps
#ifdef KGDB
#  define BPTTRAP(a)	sti; pushl $(a) ; jmp bpttraps
#else
#  define BPTTRAP(a)	sti; TRAP(a)
#endif

IDTVEC(div)
	pushl $0; TRAP(T_DIVIDE)
IDTVEC(dbg)
#if defined(BDE_DEBUGGER) && defined(BDBTRAP)
	BDBTRAP(dbg)
#endif
	pushl $0; BPTTRAP(T_TRCTRAP)
IDTVEC(nmi)
	pushl $0; TRAP(T_NMI)
IDTVEC(bpt)
#if defined(BDE_DEBUGGER) && defined(BDBTRAP)
	BDBTRAP(bpt)
#endif
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
	pushl	$0				/* dummy error code */
	pushl	$T_ASTFLT
	pushal
	nop					/* silly, the bug is for popal and it only
						 * bites when the next instruction has a
						 * complicated address mode */
	pushl	%ds
	pushl	%es				/* now the stack frame is a trap frame */
	movl	$KDSEL,%eax
	movl	%ax,%ds
	movl	%ax,%es
	pushl	_cpl
	pushl	$0				/* dummy unit to finish building intr frame */
	incl	_cnt+V_TRAP
	call	_npxintr
	jmp	doreti
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
alltraps:
	pushal
	nop
	pushl	%ds
	pushl	%es
	movl	$KDSEL,%eax
	movl	%ax,%ds
	movl	%ax,%es
calltrap:
	incl	_cnt+V_TRAP
	call	_trap
	/*
	 * Return through doreti to handle ASTs.  Have to change trap frame
	 * to interrupt frame.
	 */
	movl	$T_ASTFLT,4+4+32(%esp)		/* new trap type (err code not used) */
	pushl	_cpl
	pushl	$0				/* dummy unit */
	jmp	doreti

#ifdef KGDB
/*
 * This code checks for a kgdb trap, then falls through
 * to the regular trap code.
 */
	SUPERALIGN_TEXT
bpttraps:
	pushal
	nop
	pushl	%es
	pushl	%ds
	movl	$KDSEL,%eax
	movl	%ax,%ds
	movl	%ax,%es
	testb	$SEL_RPL_MASK,TRAPF_CS_OFF(%esp) /* non-kernel mode? */
	jne	calltrap			/* yes */
	call	_kgdb_trap_glue
	jmp	calltrap
#endif

/*
 * Call gate entry for syscall
 */
	SUPERALIGN_TEXT
IDTVEC(syscall)
	pushfl	/* only for stupid carry bit and more stupid wait3 cc kludge */
		/* XXX - also for direction flag (bzero, etc. clear it) */
	pushal	/* only need eax,ecx,edx - trap resaves others */
	nop
	movl	$KDSEL,%eax			/* switch to kernel segments */
	movl	%ax,%ds
	movl	%ax,%es
	incl	_cnt+V_SYSCALL
	call	_syscall
	/*
	 * Return through doreti to handle ASTs.  Have to change syscall frame
	 * to interrupt frame.
	 *
	 * XXX - we should have set up the frame earlier to avoid the
	 * following popal/pushal (not much can be done to avoid shuffling
	 * the flags).  Consistent frames would simplify things all over.
	 */
	movl	32+0(%esp),%eax			/* old flags, shuffle to above cs:eip */
	movl	32+4(%esp),%ebx			/* `int' frame should have been ef, eip, cs */
	movl	32+8(%esp),%ecx
	movl	%ebx,32+0(%esp)
	movl	%ecx,32+4(%esp)
	movl	%eax,32+8(%esp)
	popal
	nop
	pushl	$0				/* dummy error code */
	pushl	$T_ASTFLT
	pushal
	nop
	movl	__udatasel,%eax			/* switch back to user segments */
	pushl	%eax				/* XXX - better to preserve originals? */
	pushl	%eax
	pushl	_cpl
	pushl	$0
	jmp	doreti

#ifdef SHOW_A_LOT
/*
 * 'show_bits' was too big when defined as a macro.  The line length for some
 * enclosing macro was too big for gas.  Perhaps the code would have blown
 * the cache anyway.
 */
	ALIGN_TEXT
show_bits:
	pushl   %eax
	SHOW_BIT(0)
	SHOW_BIT(1)
	SHOW_BIT(2)
	SHOW_BIT(3)
	SHOW_BIT(4)
	SHOW_BIT(5)
	SHOW_BIT(6)
	SHOW_BIT(7)
	SHOW_BIT(8)
	SHOW_BIT(9)
	SHOW_BIT(10)
	SHOW_BIT(11)
	SHOW_BIT(12)
	SHOW_BIT(13)
	SHOW_BIT(14)
	SHOW_BIT(15)
	popl    %eax
	ret

	.data
bit_colors:
	.byte   GREEN,RED,0,0
	.text

#endif /* SHOW_A_LOT */

/*
 * include generated interrupt vectors and ISA intr code
 */
#include "i386/isa/vector.s"
#include "i386/isa/icu.s"
