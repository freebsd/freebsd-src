/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	@(#)profile.h	8.1 (Berkeley) 6/11/93
 * $FreeBSD: src/sys/i386/include/profile.h,v 1.42 2006/10/28 11:03:03 bde Exp $
 */

#ifndef _MACHINE_PROFILE_H_
#define	_MACHINE_PROFILE_H_

#ifndef _SYS_CDEFS_H_
#error this file needs sys/cdefs.h as a prerequisite
#endif

#ifdef _KERNEL

/*
 * Config generates something to tell the compiler to align functions on 16
 * byte boundaries.  A strict alignment is good for keeping the tables small.
 */
#define	FUNCTION_ALIGNMENT	16

/*
 * The kernel uses assembler stubs instead of unportable inlines.
 * This is mainly to save a little time when profiling is not enabled,
 * which is the usual case for the kernel.
 */
#define	_MCOUNT_DECL void mcount
#define	MCOUNT

#ifdef GUPROF
#define	MCOUNT_DECL(s)
#define	MCOUNT_ENTER(s)
#define	MCOUNT_EXIT(s)
#ifdef __GNUCLIKE_ASM
#define	MCOUNT_OVERHEAD(label)						\
	__asm __volatile("pushl %0; call __mcount; popl %%ecx"		\
			 :						\
			 : "i" (label)					\
			 : "ax", "dx", "cx", "memory")
#define	MEXITCOUNT_OVERHEAD()						\
	__asm __volatile("call .mexitcount; 1:"				\
			 : :						\
			 : "ax", "dx", "cx", "memory")
#define	MEXITCOUNT_OVERHEAD_GETLABEL(labelp)				\
	__asm __volatile("movl $1b,%0" : "=rm" (labelp))
#elif defined(lint)
#define	MCOUNT_OVERHEAD(label)
#define	MEXITCOUNT_OVERHEAD()
#define	MEXITCOUNT_OVERHEAD_GETLABEL()
#else
#error
#endif /* !__GNUCLIKE_ASM */
#else /* !GUPROF */
#define	MCOUNT_DECL(s)	u_long s;
#ifdef SMP
extern int	mcount_lock;
#define	MCOUNT_ENTER(s)	{ s = read_eflags(); disable_intr(); \
 			  while (!atomic_cmpset_acq_int(&mcount_lock, 0, 1)) \
			  	/* nothing */ ; }
#define	MCOUNT_EXIT(s)	{ atomic_store_rel_int(&mcount_lock, 0); \
			  write_eflags(s); }
#else
#define	MCOUNT_ENTER(s)	{ s = read_eflags(); disable_intr(); }
#define	MCOUNT_EXIT(s)	(write_eflags(s))
#endif
#endif /* GUPROF */

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

#else /* !_KERNEL */

#define	FUNCTION_ALIGNMENT	4

#define	_MCOUNT_DECL static __inline void _mcount

#ifdef __GNUCLIKE_ASM
#define	MCOUNT								\
void									\
mcount()								\
{									\
	uintfptr_t selfpc, frompc;					\
	/*								\
	 * Find the return address for mcount,				\
	 * and the return address for mcount's caller.			\
	 *								\
	 * selfpc = pc pushed by call to mcount				\
	 */								\
	__asm("movl 4(%%ebp),%0" : "=r" (selfpc));			\
	/*								\
	 * frompc = pc pushed by call to mcount's caller.		\
	 * The caller's stack frame has already been built, so %ebp is	\
	 * the caller's frame pointer.  The caller's raddr is in the	\
	 * caller's frame following the caller's caller's frame pointer.\
	 */								\
	__asm("movl (%%ebp),%0" : "=r" (frompc));			\
	frompc = ((uintfptr_t *)frompc)[1];				\
	_mcount(frompc, selfpc);					\
}
#else /* !__GNUCLIKE_ASM */
#define	MCOUNT
#endif /* __GNUCLIKE_ASM */

typedef	u_int	uintfptr_t;

#endif /* _KERNEL */

/*
 * An unsigned integral type that can hold non-negative difference between
 * function pointers.
 */
typedef	u_int	fptrdiff_t;

#ifdef _KERNEL

void	mcount(uintfptr_t frompc, uintfptr_t selfpc);

#else /* !_KERNEL */

#include <sys/cdefs.h>

__BEGIN_DECLS
#ifdef __GNUCLIKE_ASM
void	mcount(void) __asm(".mcount");
#endif
__END_DECLS

#endif /* _KERNEL */

#endif /* !_MACHINE_PROFILE_H_ */
