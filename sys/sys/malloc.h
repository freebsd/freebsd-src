/*
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)malloc.h	7.25 (Berkeley) 5/15/91
 *	$Id: malloc.h,v 1.4 1993/11/07 17:52:43 wollman Exp $
 */

#ifndef _MALLOC_H_
#define	_MALLOC_H_

#define KMEMSTATS

/*
 * flags to malloc
 */
#define	M_WAITOK	0x0000
#define	M_NOWAIT	0x0001

/*
 * Types of memory to be allocated
 */
#define	M_FREE		0	/* should be on free list */
#define	M_MBUF		1	/* mbuf */
#define	M_DEVBUF	2	/* device driver memory */
#define	M_SOCKET	3	/* socket structure */
#define	M_PCB		4	/* protocol control block */
#define	M_RTABLE	5	/* routing tables */
#define	M_HTABLE	6	/* IMP host tables */
#define	M_FTABLE	7	/* fragment reassembly header */
#define	M_ZOMBIE	8	/* zombie proc status */
#define	M_IFADDR	9	/* interface address */
#define	M_SOOPTS	10	/* socket options */
#define	M_SONAME	11	/* socket name */
#define	M_NAMEI		12	/* namei path name buffer */
#define	M_GPROF		13	/* kernel profiling buffer */
#define	M_IOCTLOPS	14	/* ioctl data buffer */
#define	M_SUPERBLK	15	/* super block data */
#define	M_CRED		16	/* credentials */
#define	M_PGRP		17	/* process group header */
#define	M_SESSION	18	/* session header */
#define	M_IOV		19	/* large iov's */
#define	M_MOUNT		20	/* vfs mount struct */
#define	M_FHANDLE	21	/* network file handle */
#define	M_NFSREQ	22	/* NFS request header */
#define	M_NFSMNT	23	/* NFS mount structure */
#define	M_VNODE		24	/* Dynamically allocated vnodes */
#define	M_CACHE		25	/* Dynamically allocated cache entries */
#define	M_DQUOT		26	/* UFS quota entries */
#define	M_UFSMNT	27	/* UFS mount structure */
#define	M_MAPMEM	28	/* mapped memory descriptors */
#define	M_SHM		29	/* SVID compatible shared memory segments */
#define	M_VMMAP		30	/* VM map structures */
#define	M_VMMAPENT	31	/* VM map entry structures */
#define	M_VMOBJ		32	/* VM object structure */
#define	M_VMOBJHASH	33	/* VM object hash structure */
#define	M_VMPMAP	34	/* VM pmap */
#define	M_VMPVENT	35	/* VM phys-virt mapping entry */
#define	M_VMPAGER	36	/* XXX: VM pager struct */
#define	M_VMPGDATA	37	/* XXX: VM pager private data */
#define	M_FILE		38	/* Open file structure */
#define	M_FILEDESC	39	/* Open file descriptor table */
#define	M_LOCKF		40	/* Byte-range locking structures */
#define	M_PROC		41	/* Proc structures */
#define	M_SUBPROC	42	/* Proc sub-structures */
#define M_ISOFSMNT      48	/* isofs mount structures */
#define	M_TEMP		49	/* misc temporary data buffers */
#define	M_PCFSMNT	50	/* PCFS mount structure */
#define	M_PCFSFAT	51	/* PCFS fat table */
#define M_IOBUF		52	/* buffer cache buffers */
#define	M_LAST		53

#define INITKMEMNAMES { \
	"free",		/* 0 M_FREE */ \
	"mbuf",		/* 1 M_MBUF */ \
	"devbuf",	/* 2 M_DEVBUF */ \
	"socket",	/* 3 M_SOCKET */ \
	"pcb",		/* 4 M_PCB */ \
	"routetbl",	/* 5 M_RTABLE */ \
	"hosttbl",	/* 6 M_HTABLE */ \
	"fragtbl",	/* 7 M_FTABLE */ \
	"zombie",	/* 8 M_ZOMBIE */ \
	"ifaddr",	/* 9 M_IFADDR */ \
	"soopts",	/* 10 M_SOOPTS */ \
	"soname",	/* 11 M_SONAME */ \
	"namei",	/* 12 M_NAMEI */ \
	"gprof",	/* 13 M_GPROF */ \
	"ioctlops",	/* 14 M_IOCTLOPS */ \
	"superblk",	/* 15 M_SUPERBLK */ \
	"cred",		/* 16 M_CRED */ \
	"pgrp",		/* 17 M_PGRP */ \
	"session",	/* 18 M_SESSION */ \
	"iov",		/* 19 M_IOV */ \
	"mount",	/* 20 M_MOUNT */ \
	"fhandle",	/* 21 M_FHANDLE */ \
	"NFS req",	/* 22 M_NFSREQ */ \
	"NFS mount",	/* 23 M_NFSMNT */ \
	"vnodes",	/* 24 M_VNODE */ \
	"namecache",	/* 25 M_CACHE */ \
	"UFS quota",	/* 26 M_DQUOT */ \
	"UFS mount",	/* 27 M_UFSMNT */ \
	"mapmem",	/* 28 M_MAPMEM */ \
	"shm",		/* 29 M_SHM */ \
	"VM map",	/* 30 M_VMMAP */ \
	"VM mapent",	/* 31 M_VMMAPENT */ \
	"VM object",	/* 32 M_VMOBJ */ \
	"VM objhash",	/* 33 M_VMOBJHASH */ \
	"VM pmap",	/* 34 M_VMPMAP */ \
	"VM pvmap",	/* 35 M_VMPVENT */ \
	"VM pager",	/* 36 M_VMPAGER */ \
	"VM pgdata",	/* 37 M_VMPGDATA */ \
	"file",		/* 38 M_FILE */ \
	"file desc",	/* 39 M_FILEDESC */ \
	"lockf",	/* 40 M_LOCKF */ \
	"proc",		/* 41 M_PROC */ \
	"subproc",	/* 42 M_PROC */ \
	0, 0, 0, 0, 0, \
	"isofs mount",  /* 48 M_ISOFSMNT */ \
	"temp",		/* 49 M_TEMP */ \
	"PCFS mount",	/* 50 M_PCFSMNT */ \
	"PCFS fat",	/* 51 M_PCFSFAT */ \
	"buffer cache",	/* 52 M_IOBUF */ \
}

struct kmemstats {
	long	ks_inuse;	/* # of packets of this type currently in use */
	long	ks_calls;	/* total packets of this type ever allocated */
	long 	ks_memuse;	/* total memory held in bytes */
	u_short	ks_limblocks;	/* number of times blocked for hitting limit */
	u_short	ks_mapblocks;	/* number of times blocked for kernel map */
	long	ks_maxused;	/* maximum number ever used */
	long	ks_limit;	/* most that are allowed to exist */
};

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
	long	kb_calls;	/* total calls to allocate this size */
	long	kb_total;	/* total number of blocks allocated */
	long	kb_totalfree;	/* # of free elements in this bucket */
	long	kb_elmpercl;	/* # of elements in this sized allocation */
	long	kb_highwat;	/* high water mark */
	long	kb_couldfree;	/* over high water mark and could free */
};

#ifdef KERNEL
#define	MINALLOCSIZE	(1 << MINBUCKET)
#define BUCKETINDX(size) \
	(size) <= (MINALLOCSIZE * 128) \
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
					: (MINBUCKET + 15)

/*
 * Turn virtual addresses into kmem map indicies
 */
#define kmemxtob(alloc)	(kmembase + (alloc) * NBPG)
#define btokmemx(addr)	(((caddr_t)(addr) - kmembase) / NBPG)
#define btokup(addr)	(&kmemusage[((caddr_t)(addr) - kmembase) >> CLSHIFT])

/*
 * Macro versions for the usual cases of malloc/free
 */
#ifdef KMEMSTATS
#define	MALLOC(space, cast, size, type, flags) \
	(space) = (cast)malloc((u_long)(size), type, flags)
#define FREE(addr, type) free((caddr_t)(addr), type)

#else /* do not collect statistics */
#define	MALLOC(space, cast, size, type, flags) { \
	register struct kmembuckets *kbp = &bucket[BUCKETINDX(size)]; \
	long s = splimp(); \
	if (kbp->kb_next == NULL) { \
		(space) = (cast)malloc((u_long)(size), type, flags); \
	} else { \
		(space) = (cast)kbp->kb_next; \
		kbp->kb_next = *(caddr_t *)(space); \
	} \
	splx(s); \
}

#define FREE(addr, type) { \
	register struct kmembuckets *kbp; \
	register struct kmemusage *kup = btokup(addr); \
	long s = splimp(); \
	if (1 << kup->ku_indx > MAXALLOCSAVE) { \
		free((caddr_t)(addr), type); \
	} else { \
		kbp = &bucket[kup->ku_indx]; \
		*(caddr_t *)(addr) = kbp->kb_next; \
		kbp->kb_next = (caddr_t)(addr); \
	} \
	splx(s); \
}
#endif /* do not collect statistics */

extern struct kmemstats kmemstats[];
extern struct kmemusage *kmemusage;
extern char *kmembase;
extern struct kmembuckets bucket[];
extern void *malloc __P((unsigned long size, int type, int flags));
extern void free __P((void *addr, int type));
#endif /* KERNEL */
#endif /* !_MALLOC_H_ */
