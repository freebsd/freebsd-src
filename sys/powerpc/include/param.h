/*-
 * Copyright (c) 2001 David E. O'Brien
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
 * $FreeBSD$
 */

/*
 * Machine dependent constants for PowerPC (32-bit only currently)
 */

#include <machine/pte.h>

/*
 * Round p (pointer or byte index) up to a correctly-aligned value
 * for all data types (int, long, ...).   The result is unsigned int
 * and must be cast to any desired pointer type.
 */
#ifndef _ALIGNBYTES
#define	_ALIGNBYTES	(sizeof(int) - 1)
#endif
#ifndef _ALIGN
#define	_ALIGN(p)	(((unsigned)(p) + _ALIGNBYTES) & ~_ALIGNBYTES)
#endif

#ifndef	_MACHINE
#define	_MACHIN		"powerpc"
#endif
#ifndef _MACHINE_ARCH
#define	_MACHINE_ARCH	"powerpc"
#endif

#ifndef _NO_NAMESPACE_POLLUTION

#ifndef _MACHINE_PARAM_H_
#define	_MACHINE_PARAM_H_

#ifndef MACHINE
#define	MACHINE		"powerpc"
#endif
#ifndef MACHINE_ARCH
#define	MACHINE_ARCH	"powerpc"
#endif
#define	MID_MACHINE	MID_POWERPC

#if !defined(LOCORE)
#include <machine/cpu.h>
#endif

/*
 * OBJFORMAT_NAMES is a comma-separated list of the object formats
 * that are supported on the architecture.
 */
#define	OBJFORMAT_NAMES		"elf"
#define	OBJFORMAT_DEFAULT	"elf"

#ifdef SMP
#define	MAXCPU		2
#else
#define	MAXCPU		1
#endif /* SMP */

#define	ALIGNBYTES	_ALIGNBYTES
#define	ALIGN(p)	_ALIGN(p)

#define	PAGE_SHIFT	12
#define	PAGE_SIZE	(1 << PAGE_SHIFT)	/* Page size */
#define	PAGE_MASK	(PAGE_SIZE - 1)
#define	NPTEPG		(PAGE_SIZE/(sizeof (pt_entry_t)))

#define	KERNBASE	0x100000	/* start of kernel virtual */
#define	BTOPKERNBASE	((u_long)KERNBASE >> PGSHIFT)

#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define	DEV_BSIZE	(1 << DEV_BSHIFT)

#ifndef BLKDEV_IOSIZE
#define	BLKDEV_IOSIZE	PAGE_SIZE	/* default block device I/O size */
#endif
#define	DFLTPHYS	(64 * 1024)	/* default max raw I/O transfer size */
#define	MAXPHYS		(128 * 1024)	/* max raw I/O transfer size */
#define	MAXDUMPPGS	(DFLTPHYS/PAGE_SIZE)

#ifndef KSTACK_UPAGES
#define	KSTACK_PAGES		2		/* includes pcb */
#define	KSTACK_GUARD_PAGES	1
#endif
#define	USPACE		(KSTACK_PAGES * PAGE_SIZE)	/* total size of pcb */
#define	UAREA_PAGES	1		/* holds struct user WITHOUT PCB */

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than PAGE_SIZE.
 */
#ifndef	MSIZE
#define	MSIZE		256		/* size of an mbuf */
#endif	/* MSIZE */

#ifndef	MCLSHIFT
#define	MCLSHIFT	11		/* convert bytes to mbuf clusters */
#endif	/* MCLSHIFT */
#define	MCLBYTES	(1 << MCLSHIFT)	/* size of an mbuf cluster */

/*
 * Some macros for units conversion
 */

/* pages ("clicks") to bytes */
#define	ctob(x)		((x) << PAGE_SHIFT)

/* bytes to clicks */
#define	btoc(x)		(((unsigned)(x) + PAGE_MASK) >> PAGE_SHIFT)

/* pages ("clicks") to disk blocks */
#define	ctod(x)		((x) << (PAGE_SHIFT - DEV_BSHIFT))
#define	dtoc(x)		((x) >> (PAGE_SHIFT - DEV_BSHIFT))

/* bytes to disk blocks */
/*
 * btodb() is messy and perhaps slow because `bytes' may be an off_t.  We
 * want to shift an unsigned type to avoid sign extension and we don't
 * want to widen `bytes' unnecessarily.  Assume that the result fits in
 * a daddr_t.
 */
#define	btodb(bytes)	 		/* calculates (bytes / DEV_BSIZE) */ \
	(sizeof (bytes) > sizeof(long) \
	 ? (daddr_t)((unsigned long long)(bytes) >> DEV_BSHIFT) \
	 : (daddr_t)((unsigned long)(bytes) >> DEV_BSHIFT))

#define	dbtob(db)			/* calculates (db * DEV_BSIZE) */ \
	((off_t)(db) << DEV_BSHIFT)

/*
 * Mach derived conversion macros
 */
#define	trunc_page(x)		((unsigned long)(x) & ~(PAGE_MASK))
#define	round_page(x)		(((x) + PAGE_MASK) & ~PAGE_MASK)
#define	trunc_4mpage(x)		((unsigned)(x) & ~PDRMASK)
#define	round_4mpage(x)		((((unsigned)(x)) + PDRMASK) & ~PDRMASK)

#define	atop(x)			((unsigned long)(x) >> PAGE_SHIFT)
#define	ptoa(x)			((unsigned long)(x) << PAGE_SHIFT)

#define	powerpc_btop(x)		((unsigned)(x) >> PAGE_SHIFT)
#define	powerpc_ptob(x)		((unsigned)(x) << PAGE_SHIFT)

#define	pgtok(x)		((x) * (PAGE_SIZE / 1024))

/* XXX: NetBSD defines that we're using for the moment */
#define	USER_SR		13
#define	KERNEL_SR	14
#define	KERNEL_VSIDBITS	0xfffff
#define	KERNEL_SEGMENT	(0xfffff0 + KERNEL_SR)
#define	EMPTY_SEGMENT	0xfffff0
#define	USER_ADDR	((void *)(USER_SR << ADDR_SR_SHFT))
#define	SEGMENT_LENGTH	0x10000000
#define	SEGMENT_MASK	0xf0000000

#if !defined(NPMAPS)
#define	NPMAPS		32768
#endif /* !defined(NPMAPS) */

#if !defined(MSGBUFSIZE)
#define	MSGBUFSIZE	PAGE_SIZE
#endif /* !defined(MSGBUFSIZE) */

/*
 * XXX: Stop NetBSD msgbuf_paddr code from happening.
 */
#define	MSGBUFADDR

#endif /* !_MACHINE_PARAM_H_ */
#endif /* !_NO_NAMESPACE_POLLUTION */
