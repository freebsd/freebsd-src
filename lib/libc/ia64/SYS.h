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
#include <sys/syscall.h>

#define	CALLSYS_ERROR(name)					\
	CALLSYS_NOERROR(name);					\
	cmp.ne	p6,p0=r0,r10;					\
(p6)	br.cond.sptk.few .cerror


#define	SYSCALL(name)						\
ENTRY(_ ## name,0);			/* XXX # of args? */	\
	WEAK_ALIAS(name, _ ## name);				\
	CALLSYS_ERROR(name)

#define	SYSCALL_NOERROR(name)					\
ENTRY(name,0);				/* XXX # of args? */	\
	CALLSYS_NOERROR(name)


#define RSYSCALL(name)						\
	SYSCALL(name);						\
	br.ret.sptk.few rp;					\
END(_ ## name)

#define RSYSCALL_NOERROR(name)					\
	SYSCALL_NOERROR(name);					\
	br.ret.sptk.few rp;					\
END(name)


#define	PSEUDO(label,name)					\
ENTRY(_ ## label,0);		/* XXX # of args? */		\
	WEAK_ALIAS(label, _ ## label);				\
	CALLSYS_ERROR(name);					\
	br.ret.sptk.few rp;					\
END(_ ## label);

#define	PSEUDO_NOERROR(label,name)				\
ENTRY(label,0);				/* XXX # of args? */	\
	CALLSYS_NOERROR(name);					\
	br.ret.sptk.few rp;					\
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
	CALL(_thread_sys_ ## name)

#define	PENTRY(name, args)					\
ENTRY(_thread_sys_ ## name,args)

#define	PEND(name)						\
END(_thread_sys_ ## name)

#define	PSYSCALL(name)						\
PENTRY(name,0);				/* XXX # of args? */	\
	CALLSYS_ERROR(name)

#define	PRSYSCALL(name)						\
PENTRY(name,0);				/* XXX # of args? */	\
	CALLSYS_ERROR(name)					\
	br.ret.sptk.few rp;					\
PEND(name)

#define	PPSEUDO(label,name)					\
PENTRY(label,0);				/* XXX # of args? */	\
	CALLSYS_ERROR(name);					\
	br.ret.sptk.few rp;					\
PEND(label)

#else
/*
 * The non-threaded library defaults to traditional syscalls where
 * the function name matches the syscall name.
 */
#define	PSYSCALL(x)	SYSCALL(x)
#define	PRSYSCALL(x)	RSYSCALL(x)
#define	PPSEUDO(x,y)	PSEUDO(x,y)
#define	PENTRY(x,y)	ENTRY(x,y)
#define	PEND(x)		END(x)
#define	PCALL(x)	CALL(x)
#endif
