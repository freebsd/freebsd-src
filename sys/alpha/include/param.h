/* $Id: param.h,v 1.4 1998/06/14 13:45:13 dfr Exp $ */
/* From: NetBSD: param.h,v 1.20 1997/09/19 13:52:53 leo Exp */

/*
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
 * from: Utah $Hdr: machparam.h 1.11 89/08/14$
 *
 *	@(#)param.h	8.1 (Berkeley) 6/10/93
 */

/*
 * Machine dependent constants for the Alpha.
 */
#define	_MACHINE	alpha
#define	MACHINE		"alpha"
#define	_MACHINE_ARCH	alpha
#define	MACHINE_ARCH	"alpha"
#define	MID_MACHINE	MID_ALPHA

#include <machine/alpha_cpu.h>
#include <machine/cpu.h>

/*
 * Round p (pointer or byte index) up to a correctly-aligned value for all
 * data types (int, long, ...).   The result is u_long and must be cast to
 * any desired pointer type.
 *
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits). 
 *
 */
#define	ALIGNBYTES		7
#define	ALIGN(p)		(((u_long)(p) + ALIGNBYTES) &~ ALIGNBYTES)
#define ALIGNED_POINTER(p,t)	((((u_long)(p)) & (sizeof(t)-1)) == 0)

#define	PAGE_SIZE	(1 << ALPHA_PGSHIFT)		/* bytes/page */
#define PAGE_SHIFT	ALPHA_PGSHIFT
#define PAGE_MASK	(PAGE_SIZE-1)
#define NPTEPG		(PAGE_SIZE/(sizeof (pt_entry_t)))

#define	KERNBASE	0xfffffc0000230000LL	/* start of kernel virtual */
#define	BTOPKERNBASE	((u_long)KERNBASE >> PGSHIFT)

#define	DEV_BSHIFT	9		/* log2(DEV_BSIZE) */
#define	DEV_BSIZE	(1<<DEV_BSHIFT)

#define BLKDEV_IOSIZE	2048
#define DFLTPHYS	(64 * 1024)	/* default max raw I/O transfer size */
#define MAXPHYS		(128 * 1024)	/* max raw I/O transfer size */

#define	CLSIZE		1
#define	CLSIZELOG2	0

/* NOTE: SSIZE, SINCR and UPAGES must be multiples of CLSIZE */
#define	SSIZE		1		/* initial stack size/NBPG */
#define	SINCR		1		/* increment of stack/NBPG */

#define	UPAGES		2		/* pages of u-area */
#define	USPACE		(UPAGES * PAGE_SIZE) /* total size of u-area */

#ifndef MSGBUFSIZE
#define MSGBUFSIZE	PAGE_SIZE	/* default message buffer size */
#endif

/*
 * Constants related to network buffer management.
 * MCLBYTES must be no larger than CLBYTES (the software page size), and,
 * on machines that exchange pages of input or output buffers with mbuf
 * clusters (MAPPED_MBUFS), MCLBYTES must also be an integral multiple
 * of the hardware page size.
 */
#define	MSIZE		256		/* size of an mbuf */
#ifndef MCLSHIFT
# define	MCLSHIFT	11	/* convert bytes to m_buf clusters */
					/* 2K cluster can hold Ether frame */
#endif	/* MCLSHIFT */
#define	MCLBYTES	(1 << MCLSHIFT)	/* size of a m_buf cluster */
#define	MCLOFSET	(MCLBYTES - 1)
#ifndef NMBCLUSTERS
#ifdef GATEWAY
#define	NMBCLUSTERS	512		/* map size, max cluster allocation */
#else
#define	NMBCLUSTERS	256		/* map size, max cluster allocation */
#endif
#endif

/*
 * Size of kernel malloc arena in CLBYTES-sized logical pages
 */ 
#ifndef NKMEMCLUSTERS
#define	NKMEMCLUSTERS	(4096*1024/CLBYTES)	/* XXX? */
#endif

/* pages ("clicks") to disk blocks */
#define	ctod(x)		((x) << (PAGE_SHIFT - DEV_BSHIFT))
#define	dtoc(x)		((x) >> (PAGE_SHIFT - DEV_BSHIFT))

/* pages to bytes */
#define	ctob(x)		((x) << PAGE_SHIFT)
#define	btoc(x)		(((x) + PAGE_MASK) >> PAGE_SHIFT)

/* bytes to disk blocks */
#define	btodb(x)	((x) >> DEV_BSHIFT)
#define	dbtob(x)	((x) << DEV_BSHIFT)

/*
 * Map a ``block device block'' to a file system block.
 * This should be device dependent, and should use the bsize
 * field from the disk label.
 * For now though just use DEV_BSIZE.
 */
#define	bdbtofsb(bn)	((bn) / (BLKDEV_IOSIZE/DEV_BSIZE))

/*
 * Mach derived conversion macros
 */
#define	round_page(x)	((((unsigned long)(x)) + PAGE_MASK) & ~(PAGE_MASK))
#define	trunc_page(x)	((unsigned long)(x) & ~(PAGE_MASK))

#define atop(x)			((unsigned long)(x) >> PAGE_SHIFT)
#define ptoa(x)			((unsigned long)(x) << PAGE_SHIFT)

#define	alpha_btop(x)		((unsigned long)(x) >> PAGE_SHIFT)
#define	alpha_ptob(x)		((unsigned long)(x) << PAGE_SHIFT)

#ifdef _KERNEL
#ifndef _LOCORE

void	delay __P((unsigned long));
#define	DELAY(n)	delay(n)

/* XXX THE FOLLOWING PROTOTYPE BELONGS IN INTR.H */
int spl0 __P((void));					/* drop ipl to zero */
/* XXX END INTR.H */

/* XXX THE FOLLOWING PROTOTYPE SHOULD BE A BUS.H INTERFACE */
vm_offset_t alpha_XXX_dmamap __P((vm_offset_t));
/* XXX END BUS.H */

#endif
#endif /* !_KERNEL */
