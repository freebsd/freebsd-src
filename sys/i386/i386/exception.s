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
 * $FreeBSD: src/sys/i386/i386/exception.s,v 1.65.2.2 2000/07/07 00:38:46 obrien Exp $
 */

#include "npx.h"

#include <machine/asmacros.h>
#include <machine/ipl.h>
#include <machine/lock.h>
#include <machine/psl.h>
#include <machine/trap.h>
#ifdef SMP
#include <machine/smptests.h>		/** various SMP options */
#endif

#include "assym.s"

#ifdef SMP
#define	MOVL_KPSEL_EAX	movl	$KPSEL,%eax
#else
#define	MOVL_KPSEL_EAX
#endif
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
 *
 * On entry to a trap or interrupt WE DO NOT OWN THE MP LOCK.  This means
 * that we must be careful in regards to accessing global variables.  We
 * save (push) the current cpl (our software interrupt disable mask), call
 * the trap function, then call _doreti to restore the cpl and deal with
 * ASTs (software interrupts).  _doreti will determine if the restoration
 * of the cpl unmasked any pending interrupts and will issue those interrupts
 * synchronously prior to doing the iret.
 *
 * At the moment we must own the MP lock to do any cpl manipulation, which
 * means we must own it prior to  calling _doreti.  The syscall case attempts
 * to avoid this by handling a reduced set of cases itself and iret'ing.
 */
#define	IDTVEC(name)	ALIGN_TEXT; .globl __CONCAT(_X,name); \
			.type __CONCAT(_X,name),@function; __CONCAT(_X,name):
#define	TRAP(a)		pushl $(a) ; jmp _alltraps

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
	 * call npx_intr to clear the error.  It would be better to handle
	 * npx interrupts as traps.  Nested interrupts would probably have
	 * to be converted to ASTs.
	 */
	pushl	$0			/* dummy error code */
	pushl	$0			/* dummy trap type */
	pushal
	pushl	%ds
	pushl	%es			/* now stack frame is a trap frame */
	pushl	%fs
	mov	$KDSEL,%ax
	mov	%ax,%ds
	mov	%ax,%es
	MOVL_KPSEL_EAX
	mov	%ax,%fs
	FAKE_MCOUNT(13*4(%esp))

#ifdef SMP
	MPLOCKED incl _cnt+V_TRAP
	MP_LOCK
	movl	_cpl,%eax
	pushl	%eax			/* save original cpl */
	pushl	$0			/* dummy unit to finish intr frame */
#else /* SMP */
	movl	_cpl,%eax
	pushl	%eax
	pushl	$0			/* dummy unit to finish intr frame */
	incl	_cnt+V_TRAP
#endif /* SMP */

	call	_npx_intr

	incb	_intr_nesting_level
	MEXITCOUNT
	jmp	_doreti
#else	/* NNPX > 0 */
	pushl $0; TRAP(T_ARITHTRAP)
#endif	/* NNPX > 0 */

IDTVEC(align)
	TRAP(T_ALIGNFLT)

	/*
	 * _alltraps entry point.  Interrupts are enabled if this was a trap
	 * gate (TGT), else disabled if this was an interrupt gate (IGT).
	 * Note that int0x80_syscall is a trap gate.  Only page faults
	 * use an interrupt gate.
	 *
	 * Note that all calls to MP_LOCK must occur with interrupts enabled
	 * in order to be able to take IPI's while waiting for the lock.
	 */

	SUPERALIGN_TEXT
	.globl	_alltraps
	.type	_alltraps,@function
_alltraps:
	pushal
	pushl	%ds
	pushl	%es
	pushl	%fs
alltraps_with_regs_pushed:
	mov	$KDSEL,%ax
	mov	%ax,%ds
	mov	%ax,%es
	MOVL_KPSEL_EAX
	mov	%ax,%fs
	FAKE_MCOUNT(13*4(%esp))
calltrap:
	FAKE_MCOUNT(_btrap)		/* init "from" _btrap -> calltrap */
	MPLOCKED incl _cnt+V_TRAP
	MP_LOCK
	movl	_cpl,%ebx		/* keep orig. cpl here during trap() */
	call	_trap

	/*
	 * Return via _doreti to handle ASTs.  Have to change trap frame
	 * to interrupt frame.
	 */
	pushl	%ebx			/* cpl to restore */
	subl	$4,%esp			/* dummy unit to finish intr frame */
	incb	_intr_nesting_level
	MEXITCOUNT
	jmp	_doreti

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
 *
 * We do not obtain the MP lock, but the call to syscall2 might.  If it
 * does it will release the lock prior to returning.
 */
	SUPERALIGN_TEXT
IDTVEC(syscall)
	pushfl				/* save eflags in tf_err for now */
	subl	$4,%esp			/* skip over tf_trapno */
	pushal
	pushl	%ds
	pushl	%es
	pushl	%fs
	mov	$KDSEL,%ax		/* switch to kernel segments */
	mov	%ax,%ds
	mov	%ax,%es
	MOVL_KPSEL_EAX
	mov	%ax,%fs
	movl	TF_ERR(%esp),%eax	/* copy saved eflags to final spot */
	movl	%eax,TF_EFLAGS(%esp)
	movl	$7,TF_ERR(%esp) 	/* sizeof "lcall 7,0" */
	FAKE_MCOUNT(13*4(%esp))
	MPLOCKED incl _cnt+V_SYSCALL
	call	_syscall2
	MEXITCOUNT
	cli				/* atomic astpending access */
	cmpl    $0,_astpending
	je	doreti_syscall_ret
#ifdef SMP
	MP_LOCK
#endif
	pushl	$0			/* cpl to restore */
	subl	$4,%esp			/* dummy unit for interrupt frame */
	movb	$1,_intr_nesting_level
	jmp	_doreti

/*
 * Call gate entry for FreeBSD ELF and Linux/NetBSD syscall (int 0x80)
 *
 * Even though the name says 'int0x80', this is actually a TGT (trap gate)
 * rather then an IGT (interrupt gate).  Thus interrupts are enabled on
 * entry just as they are for a normal syscall.
 *
 * We do not obtain the MP lock, but the call to syscall2 might.  If it
 * does it will release the lock prior to returning.
 */
	SUPERALIGN_TEXT
IDTVEC(int0x80_syscall)
	subl	$8,%esp			/* skip over tf_trapno and tf_err */
	pushal
	pushl	%ds
	pushl	%es
	pushl	%fs
	mov	$KDSEL,%ax		/* switch to kernel segments */
	mov	%ax,%ds
	mov	%ax,%es
	MOVL_KPSEL_EAX
	mov	%ax,%fs
	movl	$2,TF_ERR(%esp)		/* sizeof "int 0x80" */
	FAKE_MCOUNT(13*4(%esp))
	MPLOCKED incl _cnt+V_SYSCALL
	call	_syscall2
	MEXITCOUNT
	cli				/* atomic astpending access */
	cmpl    $0,_astpending
	je	doreti_syscall_ret
#ifdef SMP
	MP_LOCK
#endif
	pushl	$0			/* cpl to restore */
	subl	$4,%esp			/* dummy unit for interrupt frame */
	movb	$1,_intr_nesting_level
	jmp	_doreti

ENTRY(fork_trampoline)
	call	_spl0

#ifdef SMP
	cmpl	$0,_switchtime
	jne	1f
	movl	$gd_switchtime,%eax
	addl	%fs:0,%eax
	pushl	%eax
	call	_microuptime
	popl	%edx
	movl	_ticks,%eax
	movl	%eax,_switchticks
1:
#endif

	/*
	 * cpu_set_fork_handler intercepts this function call to
	 * have this call a non-return function to stay in kernel mode.
	 * initproc has its own fork handler, but it does return.
	 */
	pushl	%ebx			/* arg1 */
	call	*%esi			/* function */
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
 * Include vm86 call routines, which want to call _doreti.
 */
#include "i386/i386/vm86bios.s"

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
