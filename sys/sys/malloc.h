/*
 * Copyright (c) 1987, 1993
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
 *	@(#)malloc.h	8.5 (Berkeley) 5/3/95
 * $Id: malloc.h,v 1.33 1997/12/05 05:36:49 dyson Exp $
 */

#ifndef _SYS_MALLOC_H_
#define	_SYS_MALLOC_H_

#define KMEMSTATS

/*
 * flags to malloc
 */
#define	M_WAITOK	0x0000
#define	M_NOWAIT	0x0001
#define M_KERNEL	0x0002

#define M_MAGIC		877983977	/* time when first defined :-) */

struct malloc_type {
	struct malloc_type *ks_next; /* Next pointer */
	long 	ks_memuse;	/* total memory held in bytes */
	long	ks_limit;	/* most that are allowed to exist */
	long	ks_size;	/* sizes of this thing that are allocated */
	long	ks_inuse;	/* # of packets of this type currently in use */
	long	ks_calls;	/* total packets of this type ever allocated */
	long	ks_maxused;	/* maximum number ever used */
	u_long	ks_magic;	/* If if's not magic, don't touch it */
	const char * const ks_shortdesc;	/* Short description */
	u_short	ks_limblocks;	/* number of times blocked for hitting limit */
	u_short	ks_mapblocks;	/* number of times blocked for kernel map */
};

#define	MALLOC_DEFINE(type, shortdesc, longdesc) \
	struct malloc_type type[1] = { { NULL, 0, 0, 0, 0, 0, 0, \
					 M_MAGIC, shortdesc, 0, 0 } }; \
	struct __hack

#define	MALLOC_DECLARE(type) \
	extern struct malloc_type type[1]; \
	struct __hack

#ifdef MALLOC_INSTANTIATE
#define MALLOC_MAKE_TYPE(type, shortdesc, longdesc) \
	MALLOC_DEFINE(type, shortdesc, longdesc);
#else
#define MALLOC_MAKE_TYPE(type, shortdesc, longdesc) \
	MALLOC_DECLARE(type);
#endif

MALLOC_MAKE_TYPE(M_CACHE, "namecache", "Dynamically allocated cache entries");
MALLOC_MAKE_TYPE(M_DEVBUF, "devbuf", "device driver memory");
MALLOC_MAKE_TYPE(M_TEMP, "temp", "misc temporary data buffers");

/*
 * Array of descriptors that describe the contents of each page
 */
struct kmemusage {
	short ku_indx;		/* bucket index */
	union {
		u_short freecnt;/* for small allocations, free pieces in page */
		u_short pagecnt;/* for large allocations, pages alloced */
	} ku_un;
};
#define ku_freecnt ku_un.freecnt
#define ku_pagecnt ku_un.pagecnt

/*
 * Set of buckets for each size of memory block that is retained
 */
struct kmembuckets {
	caddr_t kb_next;	/* list of free blocks */
	caddr_t kb_last;	/* last free block */
	long	kb_total;	/* total number of blocks allocated */
	long	kb_elmpercl;	/* # of elements in this sized allocation */
	long	kb_totalfree;	/* # of free elements in this bucket */
	long	kb_calls;	/* total calls to allocate this size */
	long	kb_highwat;	/* high water mark */
	long	kb_couldfree;	/* over high water mark and could free */
};

#ifdef KERNEL

#include <vm/vm_zone.h>

#define	MINALLOCSIZE	(1 << MINBUCKET)
#define BUCKETINDX(size) \
	((size) <= (MINALLOCSIZE * 128) \
		? (size) <= (MINALLOCSIZE * 8) \
			? (size) <= (MINALLOCSIZE * 2) \
				? (size) <= (MINALLOCSIZE * 1) \
					? (MINBUCKET + 0) \
					: (MINBUCKET + 1) \
				: (size) <= (MINALLOCSIZE * 4) \
					? (MINBUCKET + 2) \
					: (MINBUCKET + 3) \
			: (size) <= (MINALLOCSIZE* 32) \
				? (size) <= (MINALLOCSIZE * 16) \
					? (MINBUCKET + 4) \
					: (MINBUCKET + 5) \
				: (size) <= (MINALLOCSIZE * 64) \
					? (MINBUCKET + 6) \
					: (MINBUCKET + 7) \
		: (size) <= (MINALLOCSIZE * 2048) \
			? (size) <= (MINALLOCSIZE * 512) \
				? (size) <= (MINALLOCSIZE * 256) \
					? (MINBUCKET + 8) \
					: (MINBUCKET + 9) \
				: (size) <= (MINALLOCSIZE * 1024) \
					? (MINBUCKET + 10) \
					: (MINBUCKET + 11) \
			: (size) <= (MINALLOCSIZE * 8192) \
				? (size) <= (MINALLOCSIZE * 4096) \
					? (MINBUCKET + 12) \
					: (MINBUCKET + 13) \
				: (size) <= (MINALLOCSIZE * 16384) \
					? (MINBUCKET + 14) \
					: (MINBUCKET + 15))

/*
 * Turn virtual addresses into kmem map indices
 */
#define kmemxtob(alloc)	(kmembase + (alloc) * PAGE_SIZE)
#define btokmemx(addr)	(((caddr_t)(addr) - kmembase) / PAGE_SIZE)
#define btokup(addr)	(&kmemusage[(caddr_t)(addr) - kmembase >> PAGE_SHIFT])

/*
 * Macro versions for the usual cases of malloc/free
 */
#if defined(KMEMSTATS) || defined(DIAGNOSTIC)
#define	MALLOC(space, cast, size, type, flags) \
	(space) = (cast)malloc((u_long)(size), type, flags)
#define FREE(addr, type) free((addr), type)

#else /* do not collect statistics */
#define	MALLOC(space, cast, size, type, flags) do { \
	register struct kmembuckets *kbp = &bucket[BUCKETINDX(size)]; \
	long s = splimp(); \
	if (kbp->kb_next == NULL) { \
		(space) = (cast)malloc((u_long)(size), type, flags); \
	} else { \
		(space) = (cast)kbp->kb_next; \
		kbp->kb_next = *(caddr_t *)(space); \
	} \
	splx(s); \
} while (0)

#define	FREE(addr, type) do { \
	register struct kmembuckets *kbp; \
	register struct kmemusage *kup = btokup(addr); \
	long s = splimp(); \
	if (1 << kup->ku_indx > MAXALLOCSAVE) { \
		free((addr), type); \
	} else { \
		kbp = &bucket[kup->ku_indx]; \
		if (kbp->kb_next == NULL) \
			kbp->kb_next = (caddr_t)(addr); \
		else \
			*(caddr_t *)(kbp->kb_last) = (caddr_t)(addr); \
		*(caddr_t *)(addr) = NULL; \
		kbp->kb_last = (caddr_t)(addr); \
	} \
	splx(s); \
} while (0)

extern struct kmemusage *kmemusage;
extern char *kmembase;
extern struct kmembuckets bucket[];
#endif /* do not collect statistics */

/*
 * XXX this should be declared in <sys/uio.h>, but that tends to fail
 * because <sys/uio.h> is included in a header before the source file
 * has a chance to include <sys/malloc.h> to get MALLOC_DECLARE() defined.
 */
MALLOC_DECLARE(M_IOV);

void	*contigmalloc __P((unsigned long size, struct malloc_type *type,
			   int flags,
			   unsigned long low, unsigned long high,
			   unsigned long alignment, unsigned long boundary));
void	free __P((void *addr, struct malloc_type *type));
void	*malloc __P((unsigned long size, struct malloc_type *type, int flags));
#endif /* KERNEL */

#endif /* !_SYS_MALLOC_H_ */
