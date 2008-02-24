/*-
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 *	from: NetBSD: profile.h,v 1.9 1997/04/06 08:47:37 cgd Exp
 *	from: FreeBSD: src/sys/alpha/include/profile.h,v 1.4 1999/12/29
 * $FreeBSD: src/sys/powerpc/include/profile.h,v 1.6 2005/12/29 04:07:36 grehan Exp $
 */

#ifndef _MACHINE_PROFILE_H_
#define	_MACHINE_PROFILE_H_

#define	_MCOUNT_DECL	void __mcount

#define FUNCTION_ALIGNMENT 16

typedef u_int	fptrdiff_t;

/*
 * The mcount trampoline macro, expanded in libc/gmon/mcount.c
 *
 * For PowerPC SVR4 ABI profiling, the compiler will insert
 * a data declaration and code sequence at the start of a routine of the form
 *
 * .function_mc:       	.data
 *			.align	2
 *			.long	0
 *			.text
 *
 * function:		mflr	%r0
 *			addis	%r11,%r0, .function_mc@ha
 *			stw	%r0,4(%r1)
 *			addi	%r0,%r11, .function_mc@l
 *			bl	_mcount
 *
 * The link register is saved in the LR save word in the caller's
 * stack frame, r0 is set up to point to the allocated longword,
 * and control is transferred to _mcount.
 *
 * On return from _mcount, the routine should function as it would
 * with no profiling so _mcount must restore register state to that upon
 * entry. Any routine called by the _mcount trampoline will save
 * callee-save registers, so _mcount must make sure it saves volatile
 * registers that may have state after it returns i.e. parameter registers.
 *
 * The FreeBSD libc mcount routine ignores the r0 longword pointer, but
 * instead requires as parameters the current PC and called PC. The current
 * PC is obtained from the link register, as a result of "bl _mcount" in
 * the stub, while the caller's PC is obtained from the LR save word.
 *
 * On return from libc mcount, the return is done indirectly with the
 * ctr register rather than the link register, to allow the link register
 * to be restored to what it was on entry to the profiled routine.
 */

#ifdef PIC
#define _PLT "@plt"
#else
#define _PLT
#endif

#define	MCOUNT \
__asm("	.globl	_mcount						\n" \
"	.type	_mcount,@function				\n" \
"_mcount:							\n" \
"	stwu	%r1,-64(%r1)	/* alloca for reg save space */	\n" \
"	stw	%r3,16(%r1)	/* save parameter registers, */	\n" \
"	stw	%r4,20(%r1)    	/*  r3-10		     */	\n" \
"	stw	%r5,24(%r1)	       				\n" \
"	stw	%r6,28(%r1)	       				\n" \
"	stw	%r7,32(%r1)	       				\n" \
"	stw	%r8,36(%r1)	       				\n" \
"	stw	%r9,40(%r1)	       				\n" \
"	stw	%r10,44(%r1)					\n" \
"								\n" \
"	mflr	%r4		/* link register is 'selfpc' */	\n" \
"	stw	%r4,48(%r1)    	/* save since bl will scrub  */	\n" \
"	lwz	%r3,68(%r1)    	/* get 'frompc' from LR-save */	\n" \
"	bl	__mcount" _PLT "  /* __mcount(frompc, selfpc)*/	\n" \
"	lwz	%r3,68(%r1)					\n" \
"	mtlr	%r3		/* restore caller's lr	     */	\n" \
"	lwz	%r4,48(%r1)     			       	\n" \
"	mtctr	%r4		/* set up ctr for call back  */	\n" \
"				/* note that blr is not used!*/	\n" \
"	lwz	%r3,16(%r1)	/* restore r3-10 parameters  */	\n" \
"	lwz	%r4,20(%r1)	       				\n" \
"	lwz	%r5,24(%r1)	       				\n" \
"	lwz	%r6,28(%r1)	       				\n" \
"	lwz	%r7,32(%r1)	       				\n" \
"	lwz	%r8,36(%r1)	       				\n" \
"	lwz	%r9,40(%r1)	       				\n" \
"	lwz	%r10,44(%r1)					\n" \
"	addi	%r1,%r1,64	/* blow away alloca save area */ \n" \
"	bctr			/* return with indirect call */	\n" \
"_mcount_end:				\n" \
"	.size	_mcount,_mcount_end-_mcount");


#ifdef _KERNEL
#define MCOUNT_ENTER(s)		s = intr_disable();
#define MCOUNT_EXIT(s)		intr_restore(s);
#define	MCOUNT_DECL(s)		register_t s

void bintr(void);
void btrap(void);
void eintr(void);
void user(void);

#define	MCOUNT_FROMPC_USER(pc)					\
	((pc < (uintfptr_t)VM_MAXUSER_ADDRESS) ? (uintfptr_t)user : pc)

#define	MCOUNT_FROMPC_INTR(pc)					\
	((pc >= (uintfptr_t)btrap && pc < (uintfptr_t)eintr) ?	\
	    ((pc >= (uintfptr_t)bintr) ? (uintfptr_t)bintr :	\
		(uintfptr_t)btrap) : ~0U)


#else	/* !_KERNEL */

typedef u_int	uintfptr_t;

#endif	/* _KERNEL */

#endif /* !_MACHINE_PROFILE_H_ */
