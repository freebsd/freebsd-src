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
 *	$Id: param.h,v 1.41 1997/08/30 08:07:50 fsmp Exp $
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
#define DFLTPHYS	(64 * 1024)		/* default max raw I/O transfer size */
#define MAXPHYS		(128 * 1024)	/* max raw I/O transfer size */

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

#endif /* !_MACHINE_PARAM_H_ */
