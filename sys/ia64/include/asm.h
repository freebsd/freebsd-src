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
 *
 *	The document
 *
 *		"ALPHA Calling Standard", DEC 27-Apr-90
 *
 *	defines (a superset of) the rules and conventions
 *	we use.  While we make no promise of adhering to
 *	such standard and its evolution (esp where we
 *	can get faster code paths) it is certainly intended
 *	that we be interoperable with such standard.
 *
 *	In this sense, this file is a proper part of the
 *	definition of the (software) Alpha architecture.
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
	.set noat;				\
	jsr	at_reg,_mcount;			\
	.set at
#endif

/*
 * LEAF
 *	Declare a global leaf function.
 *	A leaf function does not call other functions.
 */
#define	LEAF(_name_, _n_args_)			\
	.global	_name_;				\
	.proc	_name_;				\
_name_:;					\
	.regstk	_n_args_, 0, 0, 0		\
	MCOUNT

#define	LEAF_NOPROFILE(_name_, _n_args_)	\
	.global	_name_;				\
	.proc	_name_;				\
_name_:;					\
	.regstk	_n_args_, 0, 0, 0

/*
 * STATIC_LEAF
 *	Declare a local leaf function.
 */
#define STATIC_LEAF(_name_, _n_args_)		\
	.proc	_name_;				\
_name_:;					\
	.regstk	_n_args_, 0, 0, 0		\
	MCOUNT
/*
 * XLEAF
 *	Global alias for a leaf function, or alternate entry point
 */
#define	XLEAF(_name_)				\
	.globl	_name_;				\
_name_:

/*
 * STATIC_XLEAF
 *	Local alias for a leaf function, or alternate entry point
 */
#define	STATIC_XLEAF(_name_)			\
_name_:

/*
 * NESTED
 *	Declare a (global) nested function
 *	A nested function calls other functions and needs
 *	to use alloc to save registers.
 */
#define	NESTED(_name_,_n_args_,_n_locals_,_n_outputs_,			\
	       _pfs_reg_,_rp_reg_)					\
	.globl	_name_;							\
	.proc	_name_;							\
_name_:;								\
	alloc	_pfs_reg_=ar.pfs,_n_args_,_n_locals_,_n_outputs_,0;;	\
	mov	_rp_reg_=rp						\
	MCOUNT

#define	NESTED_NOPROFILE(_name_,_n_args_,_n_locals_,_n_outputs_,	\
			 _pfs_reg_,_rp_reg_)				\
	.globl	_name_;							\
	.proc	_name_;							\
_name_:;								\
	alloc	_pfs_reg_=ar.pfs,_n_args_,_n_locals_,_n_outputs_,0;;	\
	mov	_rp_reg_=rp

/*
 * STATIC_NESTED
 *	Declare a local nested function.
 */
#define	STATIC_NESTED(_name_,_n_args_,_n_locals_,_n_outputs_,		\
		      _pfs_reg_,_rp_reg_)				\
	.proc	_name_;							\
_name_:;								\
	alloc	_pfs_reg_=ar.pfs,_n_args_,_n_locals_,_n_outputs_,0;;	\
	mov	_rp_reg_=rp;;						\
	MCOUNT

/*
 * XNESTED
 *	Same as XLEAF, for a nested function.
 */
#define	XNESTED(_name_)				\
	.globl	_name_;				\
_name_:


/*
 * STATIC_XNESTED
 *	Same as STATIC_XLEAF, for a nested function.
 */
#define	STATIC_XNESTED(_name_)			\
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
#define	MSG(msg,reg,label)					\
	lda reg, label;						\
	.data;							\
label:	ASCIZ msg;						\
	.text;

/*
 * System call glue.
 */
#define	SYSCALLNUM(name)			\
	SYS_ ## name

#define	CALLSYS_NOERROR(name)			\
	mov	r15=SYSCALLNUM(name);		\
	break	0x100000 ;;

/*
 * WEAK_ALIAS: create a weak alias (ELF only).
 */
#ifdef __ELF__
#define WEAK_ALIAS(alias,sym)					\
	.weak alias;						\
	alias = sym
#endif

/*
 * Kernel RCS ID tag and copyright macros
 */

#ifdef _KERNEL

#ifdef __ELF__
#define	__KERNEL_SECTIONSTRING(_sec, _str)				\
	.section _sec ; .asciz _str ; .text
#else /* __ELF__ */
#define	__KERNEL_SECTIONSTRING(_sec, _str)				\
	.data ; .asciz _str ; .align 3 ; .text
#endif /* __ELF__ */

#define	__KERNEL_RCSID(_n, _s)		__KERNEL_SECTIONSTRING(.ident, _s)
#define	__KERNEL_COPYRIGHT(_n, _s)	__KERNEL_SECTIONSTRING(.copyright, _s)

#ifdef NO_KERNEL_RCSIDS
#undef __KERNEL_RCSID
#define	__KERNEL_RCSID(_n, _s)		/* nothing */
#endif

#endif /* _KERNEL */
