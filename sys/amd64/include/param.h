/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 2002 David E. O'Brien.  All rights reserved.
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
 */

#ifndef _AMD64_INCLUDE_PARAM_H_
#define	_AMD64_INCLUDE_PARAM_H_

#include <sys/_align.h>

/*
 * Machine dependent constants for AMD64.
 */

#ifndef MACHINE
#define	MACHINE		"amd64"
#endif
#ifndef MACHINE_ARCH
#define	MACHINE_ARCH	"amd64"
#endif
#ifndef MACHINE_ARCH32
#define	MACHINE_ARCH32	"i386"
#endif

#ifdef SMP
#ifndef MAXCPU
#define MAXCPU		1024
#endif
#else
#define MAXCPU		1
#endif

#ifndef MAXMEMDOM
#define	MAXMEMDOM	8
#endif

#define	ALIGNBYTES		_ALIGNBYTES
#define	ALIGN(p)		_ALIGN(p)
/*
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits).
 */
#define	ALIGNED_POINTER(p, t)	1

/*
 * CACHE_LINE_SIZE is the compile-time maximum cache line size for an
 * architecture.  It should be used with appropriate caution.
 */
#define	CACHE_LINE_SHIFT	6
#define	CACHE_LINE_SIZE		(1 << CACHE_LINE_SHIFT)

/* Size of the level 1 page table units */
#define NPTEPG		(PAGE_SIZE_PT/(sizeof (pt_entry_t)))
#define NDPTEPG		(NPTEPG / PAGE_SIZE_PTES)
#define	NPTEPGSHIFT	9		/* LOG2(NPTEPG) */
#ifdef OS_PAGE_SHIFT
#define PAGE_SHIFT	OS_PAGE_SHIFT	/* LOG2(PAGE_SIZE) */
#else
#define PAGE_SHIFT	12		/* LOG2(PAGE_SIZE) */
#endif
#define PAGE_SIZE	(1<<PAGE_SHIFT)	/* bytes/page */
#define PAGE_MASK	(PAGE_SIZE-1)
#define PAGE_SHIFT_4K	12		/* LOG2(PAGE_SIZE_4K) */
#define PAGE_SIZE_4K	(1<<PAGE_SHIFT_4K)	/* bytes/page */
#define PAGE_MASK_4K	(PAGE_SIZE_4K-1)
#define PAGE_SHIFT_PT	PAGE_SHIFT_4K
#define PAGE_SIZE_PT	PAGE_SIZE_4K
#define PAGE_MASK_PT	PAGE_MASK_4K
#define PAGE_SHIFT_PV	PAGE_SHIFT_4K
#define PAGE_SIZE_PV	PAGE_SIZE_4K
#define PAGE_MASK_PV	PAGE_MASK_4K
#define PAGE_SIZE_PTES	(PAGE_SIZE / PAGE_SIZE_PT)
#define	MINIDUMP_PAGE_SIZE	PAGE_SIZE_4K
#define	MINIDUMP_PAGE_MASK	PAGE_MASK_4K
#define	MINIDUMP_PAGE_SHIFT	PAGE_SHIFT_4K
/* Size of the level 2 page directory units */
#define	NPDEPG		(PAGE_SIZE_PT/(sizeof (pd_entry_t)))
#define	NPDEPGSHIFT	9		/* LOG2(NPDEPG) */
#define	PDRSHIFT	21              /* LOG2(NBPDR) */
#define	NBPDR		(1<<PDRSHIFT)   /* bytes/page dir */
#define	PDRMASK		(NBPDR-1)
/* Size of the level 3 page directory pointer table units */
#define	NPDPEPG		(PAGE_SIZE_PT/(sizeof (pdp_entry_t)))
#define	NPDPEPGSHIFT	9		/* LOG2(NPDPEPG) */
#define	PDPSHIFT	30		/* LOG2(NBPDP) */
#define	NBPDP		(1<<PDPSHIFT)	/* bytes/page dir ptr table */
#define	PDPMASK		(NBPDP-1)
/* Size of the level 4 page-map level-4 table units */
#define	NPML4EPG	(PAGE_SIZE_PT/(sizeof (pml4_entry_t)))
#define	NPML4EPGSHIFT	9		/* LOG2(NPML4EPG) */
#define	PML4SHIFT	39		/* LOG2(NBPML4) */
#define	NBPML4		(1UL<<PML4SHIFT)/* bytes/page map lev4 table */
#define	PML4MASK	(NBPML4-1)
/* Size of the level 5 page-map level-5 table units */
#define	NPML5EPG	(PAGE_SIZE_PT/(sizeof (pml5_entry_t)))
#define	NPML5EPGSHIFT	9		/* LOG2(NPML5EPG) */
#define	PML5SHIFT	48		/* LOG2(NBPML5) */
#define	NBPML5		(1UL<<PML5SHIFT)/* bytes/page map lev5 table */
#define	PML5MASK	(NBPML5-1)

#define	MAXPAGESIZES	3	/* maximum number of supported page sizes */

/*
 * I/O permission bitmap has a bit for each I/O port plus an additional
 * byte at the end with all bits set. See section "I/O Permission Bit Map"
 * in the Intel SDM for more details.
 */
#define	IOPERM_BITMAP_BITS	(64 * 1024)
#define	IOPERM_BITMAP_BYTES	(IOPERM_BITMAP_BITS / NBBY)
#define	IOPERM_BITMAP_SIZE	(IOPERM_BITMAP_BYTES + 1)

#ifndef	KSTACK_PAGES
#if defined(KASAN) || defined(KMSAN)
#define	KSTACK_BYTES	(6 * 4096)
#else
#define	KSTACK_BYTES	(4 * 4096)	/* pages of kstack (with pcb) */
#endif
#define	KSTACK_PAGES	howmany(KSTACK_BYTES, PAGE_SIZE)
#endif
#define	KSTACK_GUARD_PAGES 1	/* pages of kstack guard; 0 disables */

/*
 * Mach derived conversion macros
 */
#define trunc_2mpage(x)	((unsigned long)(x) & ~PDRMASK)
#define round_2mpage(x)	((((unsigned long)(x)) + PDRMASK) & ~PDRMASK)
#define trunc_1gpage(x)	((unsigned long)(x) & ~PDPMASK)

#define	ptoa_pt(x)	((unsigned long )(x) << PAGE_SHIFT_PT)
#define	atop_pt(x)	((unsigned long )(x) >> PAGE_SHIFT_PT)

#define	amd64_btop(x)	((unsigned long)(x) >> PAGE_SHIFT)
#define	amd64_ptob(x)	((unsigned long)(x) << PAGE_SHIFT)

#define	INKERNEL(va)	\
    (((va) >= kva_layout.dmap_low && (va) < kva_layout.dmap_high) || \
    ((va) >= kva_layout.km_low && (va) < kva_layout.km_high))

/*
 * Must be power of 2.
 *
 * Perhaps should be autosized on boot based on found ncpus.
 */
#if MAXCPU > 256
#define SC_TABLESIZE    2048
#else
#define SC_TABLESIZE    1024
#endif

#endif /* !_AMD64_INCLUDE_PARAM_H_ */
