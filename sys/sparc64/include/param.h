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
 * $FreeBSD$
 */

/*
 * Machine dependent constants for sparc64.
 */

#define	TODO							\
	panic("implement %s", __func__)

/*
 * Round p (pointer or byte index) up to a correctly-aligned value
 * for all data types (int, long, ...).   The result is unsigned int
 * and must be cast to any desired pointer type.
 */
#ifndef _ALIGNBYTES
#define _ALIGNBYTES	0xf
#endif
#ifndef _ALIGN
#define _ALIGN(p)	(((u_long)(p) + _ALIGNBYTES) & ~_ALIGNBYTES)
#endif

#ifndef _MACHINE
#define	_MACHINE	sparc64
#endif
#ifndef _MACHINE_ARCH
#define	_MACHINE_ARCH	sparc64
#endif

#ifndef _NO_NAMESPACE_POLLUTION

#ifndef _MACHINE_PARAM_H_
#define	_MACHINE_PARAM_H_

#ifndef MACHINE
#define MACHINE		"sparc64"
#endif
#ifndef MACHINE_ARCH
#define	MACHINE_ARCH	"sparc64"
#endif
#define MID_MACHINE	MID_SPARC64

/*
 * OBJFORMAT_NAMES is a comma-separated list of the object formats
 * that are supported on the architecture.
 */
#define OBJFORMAT_NAMES		"elf"
#define OBJFORMAT_DEFAULT	"elf"

#ifdef SMP
#define MAXCPU		16
#else
#define MAXCPU		1
#endif /* SMP */

#define	INT_SHIFT	2
#define	PTR_SHIFT	3

#define ALIGNBYTES	_ALIGNBYTES
#define ALIGN(p)	_ALIGN(p)

#define	PAGE_SHIFT_8K	13
#define	PAGE_SIZE_8K	(1UL<<PAGE_SHIFT_8K)
#define	PAGE_MASK_8K	(PAGE_SIZE_8K-1)

#define	PAGE_SHIFT_64K	16
#define	PAGE_SIZE_64K	(1UL<<PAGE_SHIFT_64K)
#define	PAGE_MASK_64K	(PAGE_SIZE_64K-1)

#define	PAGE_SHIFT_512K	19
#define	PAGE_SIZE_512K	(1UL<<PAGE_SHIFT_512K)
#define	PAGE_MASK_512K	(PAGE_SIZE_512K-1)

#define	PAGE_SHIFT_4M	22
#define	PAGE_SIZE_4M	(1UL<<PAGE_SHIFT_4M)
#define	PAGE_MASK_4M	(PAGE_SIZE_4M-1)

#define PAGE_SHIFT_MIN	PAGE_SHIFT_8K
#define PAGE_SIZE_MIN	PAGE_SIZE_8K
#define PAGE_MASK_MIN	PAGE_MASK_8K
#define PAGE_SHIFT	PAGE_SHIFT_8K	/* LOG2(PAGE_SIZE) */
#define PAGE_SIZE	PAGE_SIZE_8K	/* bytes/page */
#define PAGE_MASK	PAGE_MASK_8K
#define PAGE_SHIFT_MAX	PAGE_SHIFT_4M
#define PAGE_SIZE_MAX	PAGE_SIZE_4M
#define PAGE_MASK_MAX	PAGE_MASK_4M

#define DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define DEV_BSIZE	(1<<DEV_BSHIFT)

#ifndef BLKDEV_IOSIZE
#define BLKDEV_IOSIZE	PAGE_SIZE	/* default block device I/O size */
#endif
#define DFLTPHYS	(64 * 1024)	/* default max raw I/O transfer size */
#define MAXPHYS		(128 * 1024)	/* max raw I/O transfer size */
#define MAXDUMPPGS	(DFLTPHYS/PAGE_SIZE)

#define KSTACK_PAGES		4	/* pages of kernel stack (with pcb) */
#define UAREA_PAGES		1	/* pages of user area */

#define KSTACK_GUARD 		/* compile in kstack guard page */
#define KSTACK_GUARD_PAGES	1

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than PAGE_SIZE.
 */
#ifndef	MSIZE
#define MSIZE		256		/* size of an mbuf */
#endif	/* MSIZE */

#ifndef	MCLSHIFT
#define MCLSHIFT	11		/* convert bytes to mbuf clusters */
#endif	/* MCLSHIFT */
#define MCLBYTES	(1 << MCLSHIFT)	/* size of an mbuf cluster */

/*
 * Some macros for units conversion
 */

/* clicks to bytes */
#define ctob(x)	((unsigned long)(x)<<PAGE_SHIFT)

/* bytes to clicks */
#define btoc(x)	(((unsigned long)(x)+PAGE_MASK)>>PAGE_SHIFT)

/* bytes to disk blocks */
#define btodb(bytes)	 		/* calculates (bytes / DEV_BSIZE) */ \
	(daddr_t)((unsigned long)(bytes) >> DEV_BSHIFT)

/* disk blocks to bytes */
#define dbtob(db)			/* calculates (db * DEV_BSIZE) */ \
	(off_t)((unsigned long)(db) << DEV_BSHIFT)

/*
 * Mach derived conversion macros
 */
#define round_page(x)		(((unsigned long)(x) + PAGE_MASK) & ~PAGE_MASK)
#define trunc_page(x)		((unsigned long)(x) & ~PAGE_MASK)

#define atop(x)			((unsigned long)(x) >> PAGE_SHIFT)
#define ptoa(x)			((unsigned long)(x) << PAGE_SHIFT)

#define sparc64_btop(x)		((unsigned long)(x) >> PAGE_SHIFT)
#define sparc64_ptob(x)		((unsigned long)(x) << PAGE_SHIFT)

#define	pgtok(x)		((unsigned long)(x) * (PAGE_SIZE / 1024))

#define	CTASSERT(x)		_CTASSERT(x, __LINE__)
#define	_CTASSERT(x, y)		__CTASSERT(x, y)
#define	__CTASSERT(x, y)	typedef char __assert ## y[(x) ? 1 : -1]

#endif /* !_MACHINE_PARAM_H_ */
#endif /* !_NO_NAMESPACE_POLLUTION */
