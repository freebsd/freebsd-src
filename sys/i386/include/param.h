/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	from: @(#)param.h	5.8 (Berkeley) 6/28/91
 *	$Id: param.h,v 1.37 1997/08/24 00:04:59 fsmp Exp $
 */

#ifndef _MACHINE_PARAM_H_
#define	_MACHINE_PARAM_H_

/*
 * Machine dependent constants for Intel 386.
 */

#define MACHINE		"i386"
#ifdef SMP
#define NCPUS		2
#else
#define NCPUS		1
#endif
#define MID_MACHINE	MID_I386

/*
 * Architecture dependent constants for i386 based machines.
 */
#ifdef PC98
/* NEC PC-9801/9821 series and compatibles. */
#define	MACHINE_ARCH	"pc-98"
#else
/* IBM-PC compatibles. */
#define	MACHINE_ARCH	"ibm-pc"
#endif

#ifndef LOCORE

/*
 * Round p (pointer or byte index) up to a correctly-aligned value
 * for all data types (int, long, ...).   The result is unsigned int
 * and must be cast to any desired pointer type.
 */
#define ALIGNBYTES	(sizeof(int) - 1)
#define ALIGN(p)	(((unsigned)(p) + ALIGNBYTES) & ~ALIGNBYTES)

#define PAGE_SHIFT	12		/* LOG2(PAGE_SIZE) */
#define PAGE_SIZE	(1<<PAGE_SHIFT)	/* bytes/page */
#define PAGE_MASK	(PAGE_SIZE-1)
#define NPTEPG		(PAGE_SIZE/(sizeof (pt_entry_t)))

#define NPDEPG		(PAGE_SIZE/(sizeof (pd_entry_t)))
#define PDRSHIFT	22		/* LOG2(NBPDR) */
#define NBPDR		(1<<PDRSHIFT)	/* bytes/page dir */
#define PDRMASK		(NBPDR-1)

#define DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define DEV_BSIZE	(1<<DEV_BSHIFT)

#define BLKDEV_IOSIZE	2048
#define MAXPHYS		(64 * 1024)	/* max raw I/O transfer size */

#define IOPAGES	2		/* pages of i/o permission bitmap */
#define UPAGES	2		/* pages of u-area */
#define UPAGES_HOLE	2	/* pages of "hole" at top of user space where */
				/* the upages used to be. DO NOT CHANGE! */

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than CLBYTES (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#ifndef	MSIZE
#define MSIZE		128		/* size of an mbuf */
#endif	/* MSIZE */

#ifndef	MCLSHIFT
#define MCLSHIFT	11		/* convert bytes to m_buf clusters */
#endif	/* MCLSHIFT */
#define MCLBYTES	(1 << MCLSHIFT)	/* size of an m_buf cluster */
#define MCLOFSET	(MCLBYTES - 1)	/* offset within an m_buf cluster */

/*
 * Some macros for units conversion
 */

/* clicks to bytes */
#define ctob(x)	((x)<<PAGE_SHIFT)

/* bytes to clicks */
#define btoc(x)	(((unsigned)(x)+PAGE_MASK)>>PAGE_SHIFT)

/*
 * btodb() is messy and perhaps slow because `bytes' may be an off_t.  We
 * want to shift an unsigned type to avoid sign extension and we don't
 * want to widen `bytes' unnecessarily.  Assume that the result fits in
 * a daddr_t.
 */
#define btodb(bytes)	 		/* calculates (bytes / DEV_BSIZE) */ \
	(sizeof (bytes) > sizeof(long) \
	 ? (daddr_t)((unsigned long long)(bytes) >> DEV_BSHIFT) \
	 : (daddr_t)((unsigned long)(bytes) >> DEV_BSHIFT))

#define dbtob(db)			/* calculates (db * DEV_BSIZE) */ \
	((off_t)(db) << DEV_BSHIFT)

/*
 * Mach derived conversion macros
 */
#define trunc_page(x)		((unsigned)(x) & ~PAGE_MASK)
#define round_page(x)		((((unsigned)(x)) + PAGE_MASK) & ~PAGE_MASK)
#define trunc_4mpage(x)		((unsigned)(x) & ~PDRMASK)
#define round_4mpage(x)		((((unsigned)(x)) + PDRMASK) & ~PDRMASK)

#define atop(x)			((unsigned)(x) >> PAGE_SHIFT)
#define ptoa(x)			((unsigned)(x) << PAGE_SHIFT)

#define i386_btop(x)		((unsigned)(x) >> PAGE_SHIFT)
#define i386_ptob(x)		((unsigned)(x) << PAGE_SHIFT)

#endif /* !LOCORE */


#ifndef _SIMPLELOCK_H_
#define _SIMPLELOCK_H_

/*
 * XXX some temp debug control of cpl locks
 */
#define REAL_ECPL	/* exception.s:		SCPL_LOCK/SCPL_UNLOCK */
#define REAL_ICPL	/* ipl.s:		CPL_LOCK/CPL_UNLOCK/FAST */
#define REAL_AICPL	/* apic_ipl.s:		SCPL_LOCK/SCPL_UNLOCK */
#define REAL_AVCPL	/* apic_vector.s:	CPL_LOCK/CPL_UNLOCK */

#define REAL_IFCPL	/* ipl_funcs.c:		SCPL_LOCK/SCPL_UNLOCK */

#define REAL_MCPL_NOT	/* microtime.s:		CPL_LOCK/movl $0,_cpl_lock */


#ifdef LOCORE

#ifdef SMP

#define	MPLOCKED	lock ;

/*
 * Some handy macros to allow logical organization and
 * convenient reassignment of various locks.
 */

#define FPU_LOCK	call	_get_fpu_lock
#define ALIGN_LOCK	call	_get_align_lock
#define SYSCALL_LOCK	call	_get_syscall_lock
#define ALTSYSCALL_LOCK	call	_get_altsyscall_lock

/*
 * Protects INTR() ISRs.
 */
#define ISR_TRYLOCK							\
	pushl	$_mp_lock ;			/* GIANT_LOCK */	\
	call	_MPtrylock ;			/* try to get lock */	\
	add	$4, %esp

#define ISR_RELLOCK							\
	pushl	$_mp_lock ;			/* GIANT_LOCK */	\
	call	_MPrellock ;						\
	add	$4, %esp

/*
 * Protects the IO APIC and apic_imen as a critical region.
 */
#define IMASK_LOCK							\
	pushl	$_imen_lock ;			/* address of lock */	\
	call	_s_lock ;			/* MP-safe */		\
	addl	$4, %esp

#define IMASK_UNLOCK							\
	pushl	$_imen_lock ;			/* address of lock */	\
	call	_s_unlock ;			/* MP-safe */		\
	addl	$4, %esp

/*
 * Variations of CPL_LOCK protect spl updates as a critical region.
 * Items within this 'region' include:
 *  cpl
 *  cil
 *  ipending
 *  ???
 */

/*
 * Botom half routines, ie. those already protected from INTs.
 *
 * Used in:
 *  sys/i386/i386/microtime.s (XXX currently NOT used, possible race?)
 *  sys/i386/isa/ipl.s:		_doreti
 *  sys/i386/isa/apic_vector.s:	_Xintr0, ..., _Xintr23
 */
#define CPL_LOCK							\
	pushl	$_cpl_lock ;			/* address of lock */	\
	call	_s_lock ;			/* MP-safe */		\
	addl	$4, %esp

#define CPL_UNLOCK							\
	pushl	$_cpl_lock ;			/* address of lock */	\
	call	_s_unlock ;			/* MP-safe */		\
	addl	$4, %esp

/*
 * INT safe version for top half of kernel.
 *
 * Used in:
 *  sys/i386/i386/exception.s:	_Xfpu, _Xalign, _Xsyscall, _Xint0x80_syscall
 *  sys/i386/isa/apic_ipl.s:	splz()
 */
#define SCPL_LOCK 							\
	pushl	$_cpl_lock ;						\
	call	_ss_lock ;						\
	addl	$4, %esp

#define SCPL_UNLOCK							\
	pushl	$_cpl_lock ;						\
	call	_ss_unlock ;						\
	addl	$4, %esp

#else  /* SMP */

#define	MPLOCKED				/* NOP */

#define FPU_LOCK				/* NOP */
#define ALIGN_LOCK				/* NOP */
#define SYSCALL_LOCK				/* NOP */
#define ALTSYSCALL_LOCK				/* NOP */

#endif /* SMP */

#else /* LOCORE */

#ifdef SMP

/*
 * Protects cpl/cil/ipending data as a critical region.
 *
 * Used in:
 *  sys/i386/isa/ipl_funcs.c:	DO_SETBITS, softclockpending(), GENSPL,
 *				spl0(), splx(), splq()
 */

/* Bottom half */
#define CPL_LOCK() 	s_lock(&cpl_lock)
#define CPL_UNLOCK() 	s_unlock(&cpl_lock)

/* INT safe version for top half of kernel */
#define SCPL_LOCK() 	ss_lock(&cpl_lock)
#define SCPL_UNLOCK() 	ss_unlock(&cpl_lock)

/*
 * Protects com/tty data as a critical region.
 */
#define COM_LOCK() 	s_lock(&com_lock)
#define COM_UNLOCK() 	s_unlock(&com_lock)

#else /* SMP */

#define CPL_LOCK()
#define CPL_UNLOCK()
#define SCPL_LOCK()
#define SCPL_UNLOCK()

#define COM_LOCK()
#define COM_UNLOCK()

#endif /* SMP */

/*
 * A simple spin lock.
 *
 * This structure only sets one bit of data, but is sized based on the
 * minimum word size that can be operated on by the hardware test-and-set
 * instruction. It is only needed for multiprocessors, as uniprocessors
 * will always run to completion or a sleep. It is an error to hold one
 * of these locks while a process is sleeping.
 */
struct simplelock {
	volatile int	lock_data;
};

/* functions in simplelock.s */
void	s_lock_init		__P((struct simplelock *));
void	s_lock			__P((struct simplelock *));
int	s_lock_try		__P((struct simplelock *));
void	s_unlock		__P((struct simplelock *));
void	ss_lock			__P((struct simplelock *));
void	ss_unlock		__P((struct simplelock *));

/* global data in mp_machdep.c */
extern struct simplelock	imen_lock;
extern struct simplelock	cpl_lock;
extern struct simplelock	fast_intr_lock;
extern struct simplelock	intr_lock;
extern struct simplelock	com_lock;

#if !defined(SIMPLELOCK_DEBUG) && NCPUS > 1
/*
 * The simple-lock routines are the primitives out of which the lock
 * package is built. The machine-dependent code must implement an
 * atomic test_and_set operation that indivisibly sets the simple lock
 * to non-zero and returns its old value. It also assumes that the
 * setting of the lock to zero below is indivisible. Simple locks may
 * only be used for exclusive locks.
 */

#ifdef the_original_code

static __inline void
simple_lock_init(struct simplelock *lkp)
{

	lkp->lock_data = 0;
}

static __inline void
simple_lock(struct simplelock *lkp)
{

	while (test_and_set(&lkp->lock_data))
		continue;
}

static __inline int
simple_lock_try(struct simplelock *lkp)
{

	return (!test_and_set(&lkp->lock_data));
}

static __inline void
simple_unlock(struct simplelock *lkp)
{

	lkp->lock_data = 0;
}

#else /* the_original_code */

/*
 * This set of defines turns on the real functions in i386/isa/apic_ipl.s.
 * It has never actually been tested.
 */
#define	simple_lock_init(alp)	s_lock_init(alp)
#define	simple_lock(alp)	s_lock(alp)
#define	simple_lock_try(alp)	s_lock_try(alp)
#define	simple_unlock(alp)	s_unlock(alp)

#endif /* the_original_code */

#endif /* NCPUS > 1 */
#endif /* LOCORE */
#endif /* !_SIMPLELOCK_H_ */

#endif /* !_MACHINE_PARAM_H_ */
