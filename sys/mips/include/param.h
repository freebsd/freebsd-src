/*	$OpenBSD: param.h,v 1.11 1998/08/30 22:05:35 millert Exp $ */

/*-
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 *	from: Utah Hdr: machparam.h 1.11 89/08/14
 *	from: @(#)param.h	8.1 (Berkeley) 6/10/93
 *	JNPR: param.h,v 1.6.2.1 2007/09/10 07:49:36 girish
 * $FreeBSD$
 */

#ifndef _MIPS_INCLUDE_PARAM_H_
#define	_MIPS_INCLUDE_PARAM_H_

#include <machine/_align.h>

#include <sys/cdefs.h>
#ifdef _KERNEL
#ifdef _LOCORE
#include <machine/psl.h>
#else
#include <machine/cpu.h>
#endif
#endif

#define __PCI_REROUTE_INTERRUPT

#ifndef MACHINE
#define	MACHINE		"mips"
#endif
#ifndef MACHINE_ARCH
#define	MACHINE_ARCH	"mips"
#endif

/*
 * OBJFORMAT_NAMES is a comma-separated list of the object formats
 * that are supported on the architecture.
 */
#define	OBJFORMAT_NAMES		"elf"
#define	OBJFORMAT_DEFAULT	"elf"

#define	MID_MACHINE	0	/* None but has to be defined */

#ifdef SMP
#define	MAXSMPCPU	16
#define	MAXCPU		MAXSMPCPU
#else
#define	MAXSMPCPU	1
#define	MAXCPU		1
#endif

/*
 * Round p (pointer or byte index) up to a correctly-aligned value for all
 * data types (int, long, ...).	  The result is u_int and must be cast to
 * any desired pointer type.
 */

#define	ALIGNBYTES	_ALIGNBYTES
#define	ALIGN(p)	_ALIGN(p)
/*
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits). 
 */
#define	ALIGNED_POINTER(p, t)	((((unsigned long)(p)) & (sizeof (t) - 1)) == 0)

/*
 * CACHE_LINE_SIZE is the compile-time maximum cache line size for an
 * architecture.  It should be used with appropriate caution.
 */
#define	CACHE_LINE_SHIFT	6
#define	CACHE_LINE_SIZE		(1 << CACHE_LINE_SHIFT)

#define	NBPG		4096		/* bytes/page */
#define	PGOFSET		(NBPG-1)	/* byte offset into page */
#define	PGSHIFT		12		/* LOG2(NBPG) */

#define	PAGE_SHIFT	12		/* LOG2(PAGE_SIZE) */
#define	PAGE_SIZE	(1<<PAGE_SHIFT) /* bytes/page */
#define	PAGE_MASK	(PAGE_SIZE-1)
#define	NPTEPG		(PAGE_SIZE/(sizeof (pt_entry_t)))

#define	NBSEG		0x400000	/* bytes/segment */
#define	SEGOFSET	(NBSEG-1)	/* byte offset into segment */
#define	SEGSHIFT	22		/* LOG2(NBSEG) */

#define	MAXPAGESIZES	1		/* maximum number of supported page sizes */

/* XXXimp: This has moved to vmparam.h */
/* Also, this differs from the mips2 definition, but likely is better */
/* since this means the kernel won't chew up TLBs when it is executing */
/* code */
#define	KERNBASE	0x80000000	/* start of kernel virtual */
#define	BTOPKERNBASE	((u_long)KERNBASE >> PGSHIFT)

#define	BLKDEV_IOSIZE	2048		/* xxx: Why is this 1/2 page? */
#define	MAXDUMPPGS	1		/* xxx: why is this only one? */

/*
 * The kernel stack needs to be aligned on a (PAGE_SIZE * 2) boundary.
 *
 * Although we allocate 3 pages for the kernel stack we end up using
 * only the 2 pages that are aligned on a (PAGE_SIZE * 2) boundary.
 */
#define	KSTACK_PAGES		3	/* kernel stack*/
#define	KSTACK_GUARD_PAGES	1	/* pages of kstack guard; 0 disables */

#define	UPAGES			2

/* pages ("clicks") (4096 bytes) to disk blocks */
#define	ctod(x)		((x) << (PGSHIFT - DEV_BSHIFT))
#define	dtoc(x)		((x) >> (PGSHIFT - DEV_BSHIFT))

/*
 * Map a ``block device block'' to a file system block.
 * This should be device dependent, and should use the bsize
 * field from the disk label.
 * For now though just use DEV_BSIZE.
 */
#define	bdbtofsb(bn)	((bn) / (BLKDEV_IOSIZE/DEV_BSIZE))

/*
 * Conversion macros
 */
#define	mips_round_page(x)	((((unsigned long)(x)) + NBPG - 1) & ~(NBPG-1))
#define	mips_trunc_page(x)	((unsigned long)(x) & ~(NBPG-1))
#define	mips_btop(x)		((unsigned long)(x) >> PGSHIFT)
#define	mips_ptob(x)		((unsigned long)(x) << PGSHIFT)
#define	round_page		mips_round_page
#define	trunc_page		mips_trunc_page
#define	atop(x)			((unsigned long)(x) >> PAGE_SHIFT)
#define	ptoa(x)			((unsigned long)(x) << PAGE_SHIFT)

#define	pgtok(x)		((x) * (PAGE_SIZE / 1024))

#ifndef _KERNEL
#define	DELAY(n)	{ register int N = (n); while (--N > 0); }
#endif /* !_KERNEL */

#endif /* !_MIPS_INCLUDE_PARAM_H_ */
