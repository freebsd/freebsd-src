/* $FreeBSD$ */
/* From: NetBSD: asm.h,v 1.18 1997/11/03 04:22:06 ross Exp */

/* 
 * Copyright (c) 1991,1990,1989,1994,1995,1996 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
 *	Assembly coding style
 *
 *	This file contains macros and register defines to
 *	aid in writing more readable assembly code.
 *	Some rules to make assembly code understandable by
 *	a debugger are also noted.
 */

/*
 * Macro to make a local label name.
 */
#define	LLABEL(name,num)	L ## name ## num

/*
 * MCOUNT
 */

#if !defined(GPROF) && !defined(PROF)
#define MCOUNT	/* nothing */
#else
#define MCOUNT					\
	br.call.sptk.many b7=_mcount
#endif

/*
 * ENTRY
 *	Declare a global leaf function.
 *	A leaf function does not call other functions.
 */
#define	ENTRY(_name_, _n_args_)			\
	.global	_name_;				\
	.align	16;				\
	.proc	_name_;				\
_name_:;					\
	.regstk	_n_args_, 0, 0, 0		\
	MCOUNT

#define	ENTRY_NOPROFILE(_name_, _n_args_)	\
	.global	_name_;				\
	.align	16;				\
	.proc	_name_;				\
_name_:;					\
	.regstk	_n_args_, 0, 0, 0

/*
 * STATIC_ENTRY
 *	Declare a local leaf function.
 */
#define STATIC_ENTRY(_name_, _n_args_)		\
	.align	16;				\
	.proc	_name_;				\
_name_:;					\
	.regstk	_n_args_, 0, 0, 0		\
	MCOUNT
/*
 * XENTRY
 *	Global alias for a leaf function, or alternate entry point
 */
#define	XENTRY(_name_)				\
	.globl	_name_;				\
_name_:

/*
 * STATIC_XENTRY
 *	Local alias for a leaf function, or alternate entry point
 */
#define	STATIC_XENTRY(_name_)			\
_name_:


/*
 * END
 *	Function delimiter
 */
#define	END(_name_)						\
	.endp	_name_


/*
 * EXPORT
 *	Export a symbol
 */
#define	EXPORT(_name_)						\
	.global	_name_;						\
_name_:


/*
 * IMPORT
 *	Make an external name visible, typecheck the size
 */
#define	IMPORT(_name_, _size_)					\
	/* .extern	_name_,_size_ */


/*
 * ABS
 *	Define an absolute symbol
 */
#define	ABS(_name_, _value_)					\
	.globl	_name_;						\
_name_	=	_value_


/*
 * BSS
 *	Allocate un-initialized space for a global symbol
 */
#define	BSS(_name_,_numbytes_)					\
	.comm	_name_,_numbytes_


/*
 * MSG
 *	Allocate space for a message (a read-only ascii string)
 */
#define	ASCIZ	.asciz
#define	MSG(msg,reg,label)			\
	addl reg,@ltoff(label),gp;;		\
	ld8 reg=[reg];;				\
	.data;					\
label:	ASCIZ msg;				\
	.text;


/*
 * System call glue.
 */
#define	SYSCALLNUM(name)	SYS_ ## name

#define	CALLSYS_NOERROR(name)					\
{	.mmi ;							\
	alloc		r9 = ar.pfs, 0, 0, 8, 0 ;		\
	mov		r31 = ar.k5 ;				\
	mov		r10 = b0 ;; }				\
{	.mib ;							\
	mov		r8 = SYSCALLNUM(name) ;			\
	mov		b7 = r31 ; 				\
	br.call.sptk	b0 = b7 ;; }


/*
 * WEAK_ALIAS: create a weak alias (ELF only).
 */
#define WEAK_ALIAS(alias,sym)					\
	.weak alias;						\
	alias = sym

/*
 * ID tag macros
 */
#if !defined(lint) && !defined(STRIP_FBSDID)
#define __FBSDID(s)	.ident s
#else
#define __FBSDID(s)	/* nothing */
#endif /* not lint and not STRIP_FBSDID */
