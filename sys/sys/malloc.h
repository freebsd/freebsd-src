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
 * $FreeBSD$
 */

#ifndef _SYS_MALLOC_H_
#define	_SYS_MALLOC_H_

#define splmem splhigh

/*
 * flags to malloc.
 */
#define	M_WAITOK	0x0000
#define	M_NOWAIT	0x0001		/* do not block */
#define	M_USE_RESERVE	0x0002		/* can alloc out of reserve memory */
#define	M_ASLEEP	0x0004		/* async sleep on failure */
#define	M_ZERO		0x0008		/* bzero the allocation */

#define	M_MAGIC		877983977	/* time when first defined :-) */

struct malloc_type {
	struct malloc_type *ks_next;	/* next in list */
	long 	ks_memuse;	/* total memory held in bytes */
	long	ks_limit;	/* most that are allowed to exist */
	long	ks_size;	/* sizes of this thing that are allocated */
	long	ks_inuse;	/* # of packets of this type currently in use */
	int64_t	ks_calls;	/* total packets of this type ever allocated */
	long	ks_maxused;	/* maximum number ever used */
	u_long	ks_magic;	/* if it's not magic, don't touch it */
	const char *ks_shortdesc;	/* short description */
	u_short	ks_limblocks;	/* number of times blocked for hitting limit */
	u_short	ks_mapblocks;	/* number of times blocked for kernel map */
};

#ifdef _KERNEL
#define	MALLOC_DEFINE(type, shortdesc, longdesc) \
	struct malloc_type type[1] = { \
		{ NULL, 0, 0, 0, 0, 0, 0, M_MAGIC, shortdesc, 0, 0 } \
	}; \
	SYSINIT(type##_init, SI_SUB_KMEM, SI_ORDER_ANY, malloc_init, type); \
	SYSUNINIT(type##_uninit, SI_SUB_KMEM, SI_ORDER_ANY, malloc_uninit, type)

#define	MALLOC_DECLARE(type) \
	extern struct malloc_type type[1]

MALLOC_DECLARE(M_CACHE);
MALLOC_DECLARE(M_DEVBUF);
MALLOC_DECLARE(M_TEMP);

MALLOC_DECLARE(M_IP6OPT); /* for INET6 */
MALLOC_DECLARE(M_IP6NDP); /* for INET6 */
#endif /* _KERNEL */

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
	int64_t	kb_calls;	/* total calls to allocate this size */
	long	kb_total;	/* total number of blocks allocated */
	long	kb_elmpercl;	/* # of elements in this sized allocation */
	long	kb_totalfree;	/* # of free elements in this bucket */
	long	kb_highwat;	/* high water mark */
	long	kb_couldfree;	/* over high water mark and could free */
};

#ifdef _KERNEL
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
#define btokup(addr)	(&kmemusage[((caddr_t)(addr) - kmembase) >> PAGE_SHIFT])

/*
 * Deprecated macro versions of not-quite-malloc() and free().
 */
#define	MALLOC(space, cast, size, type, flags) \
	(space) = (cast)malloc((u_long)(size), (type), (flags))
#define	FREE(addr, type) free((addr), (type))

/*
 * XXX this should be declared in <sys/uio.h>, but that tends to fail
 * because <sys/uio.h> is included in a header before the source file
 * has a chance to include <sys/malloc.h> to get MALLOC_DECLARE() defined.
 */
MALLOC_DECLARE(M_IOV);

/* XXX struct malloc_type is unused for contig*(). */
void	contigfree __P((void *addr, unsigned long size,
			struct malloc_type *type));
void	*contigmalloc __P((unsigned long size, struct malloc_type *type,
			   int flags, vm_paddr_t low, vm_paddr_t high,
			   unsigned long alignment, unsigned long boundary));
void	free __P((void *addr, struct malloc_type *type));
void	*malloc __P((unsigned long size, struct malloc_type *type, int flags));
void	malloc_init __P((void *));
void	malloc_uninit __P((void *));
void	*realloc __P((void *addr, unsigned long size,
		      struct malloc_type *type, int flags));
void	*reallocf __P((void *addr, unsigned long size,
		      struct malloc_type *type, int flags));
#endif /* _KERNEL */

#endif /* !_SYS_MALLOC_H_ */
