/* $FreeBSD$ */
/*	From: NetBSD: SYS.h,v 1.5 1997/05/02 18:15:15 kleink Exp */

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
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
 */

#include <machine/asm.h>
#ifdef __NETBSD_SYSCALLS
#include <sys/netbsd_syscall.h>
#else
#include <sys/syscall.h>
#endif

#define	CALLSYS_ERROR(name)					\
	CALLSYS_NOERROR(name);					\
	br	gp, LLABEL(name,0);				\
LLABEL(name,0):							\
	LDGP(gp);						\
	beq	a3, LLABEL(name,1);				\
	jmp	zero, .cerror;					\
LLABEL(name,1):


#define	SYSCALL(name)						\
LEAF(__CONCAT(_,name),0);		/* XXX # of args? */	\
	WEAK_ALIAS(name, __CONCAT(_,name));			\
	CALLSYS_ERROR(name)

#define	SYSCALL_NOERROR(name)					\
LEAF(name,0);				/* XXX # of args? */	\
	CALLSYS_NOERROR(name)


#define RSYSCALL(name)						\
	SYSCALL(name);						\
	RET;							\
END(__CONCAT(_,name))

#define RSYSCALL_NOERROR(name)					\
	SYSCALL_NOERROR(name);					\
	RET;							\
END(name)


#define	PSEUDO(label,name)					\
LEAF(__CONCAT(_,label),0);		/* XXX # of args? */	\
	WEAK_ALIAS(label, __CONCAT(_,label));			\
	CALLSYS_ERROR(name);					\
	RET;							\
END(__CONCAT(_,label));

#define	PSEUDO_NOERROR(label,name)				\
LEAF(label,0);				/* XXX # of args? */	\
	CALLSYS_NOERROR(name);					\
	RET;							\
END(label);

/*
 * Design note:
 *
 * The macros PSYSCALL() and PRSYSCALL() are intended for use where a
 * syscall needs to be renamed in the threaded library. When building
 * a normal library, they default to the traditional SYSCALL() and
 * RSYSCALL(). This avoids the need to #ifdef _THREAD_SAFE everywhere
 * that the renamed function needs to be called.
 */
#ifdef _THREAD_SAFE
/*
 * For the thread_safe versions, we prepend _thread_sys_ to the function
 * name so that the 'C' wrapper can go around the real name.
 */
#define	PCALL(name)						\
	CALL(___CONCAT(_thread_sys_,name))

#define	PLEAF(name, args)					\
LEAF(___CONCAT(_thread_sys_,name),args)

#define	PEND(name)						\
END(___CONCAT(_thread_sys_,name))

#define	PSYSCALL(name)						\
PLEAF(name,0);				/* XXX # of args? */	\
	CALLSYS_ERROR(name)

#define	PRSYSCALL(name)						\
PLEAF(name,0);				/* XXX # of args? */	\
	CALLSYS_ERROR(name)					\
	RET;							\
PEND(name)

#define	PPSEUDO(label,name)					\
PLEAF(label,0);				/* XXX # of args? */	\
	CALLSYS_ERROR(name);					\
	RET;							\
PEND(label)

#else
/*
 * The non-threaded library defaults to traditional syscalls where
 * the function name matches the syscall name.
 */
#define	PSYSCALL(x)	SYSCALL(x)
#define	PRSYSCALL(x)	RSYSCALL(x)
#define	PPSEUDO(x,y)	PSEUDO(x,y)
#define	PLEAF(x,y)	LEAF(x,y)
#define	PEND(x)		END(x)
#define	PCALL(x)	CALL(x)
#endif
