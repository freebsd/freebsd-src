/*-
 * Copyright (c) 1991, 1993, 1994
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
 *	@(#)mpool.h	8.4 (Berkeley) 11/2/95
 */

#include "db-queue.h"

/*
 * The memory pool scheme is a simple one.  Each in-memory page is referenced
 * by a bucket which is threaded in up to two of three ways.  All active pages
 * are threaded on a hash chain (hashed by page number) and an lru chain.
 * Inactive pages are threaded on a free chain.  Each reference to a memory
 * pool is handed an opaque MPOOL cookie which stores all of this information.
 */
#define	HASHSIZE	128
#define	HASHKEY(pgno)	((pgno - 1) % HASHSIZE)

/* The BKT structures are the elements of the queues. */
typedef struct _bkt {
	TAILQ_ENTRY(_bkt) hq;		/* hash queue */
	TAILQ_ENTRY(_bkt) q;		/* lru queue */
	void    *page;			/* page */
	db_pgno_t   pgno;			/* page number */

#define	MPOOL_DIRTY	0x01		/* page needs to be written */
#define	MPOOL_PINNED	0x02		/* page is pinned into memory */
#define	MPOOL_INUSE	0x04		/* page address is valid */
	u_int8_t flags;			/* flags */
} BKT;

typedef struct MPOOL {
	TAILQ_HEAD(_lqh, _bkt) lqh;	/* lru queue head */
					/* hash queue array */
	TAILQ_HEAD(_hqh, _bkt) hqh[HASHSIZE];
	db_pgno_t	curcache;		/* current number of cached pages */
	db_pgno_t	maxcache;		/* max number of cached pages */
	db_pgno_t	npages;			/* number of pages in the file */
	u_long	pagesize;		/* file page size */
	int	fd;			/* file descriptor */
					/* page in conversion routine */
	void    (*pgin) __P((void *, db_pgno_t, void *));
					/* page out conversion routine */
	void    (*pgout) __P((void *, db_pgno_t, void *));
	void	*pgcookie;		/* cookie for page in/out routines */
#ifdef STATISTICS
	u_long	cachehit;
	u_long	cachemiss;
	u_long	pagealloc;
	u_long	pageflush;
	u_long	pageget;
	u_long	pagenew;
	u_long	pageput;
	u_long	pageread;
	u_long	pagewrite;
#endif
} MPOOL;

#define	MPOOL_IGNOREPIN	0x01		/* Ignore if the page is pinned. */
#define	MPOOL_PAGE_REQUEST	0x01	/* Allocate a new page with a
					   specific page number. */
#define	MPOOL_PAGE_NEXT		0x02	/* Allocate a new page with the next
					  page number. */

#define mpool_open	kdb2_mpool_open
#define mpool_filter	kdb2_mpool_filter
#define mpool_new	kdb2_mpool_new
#define mpool_get	kdb2_mpool_get
#define mpool_delete	kdb2_mpool_delete
#define mpool_put	kdb2_mpool_put
#define mpool_sync	kdb2_mpool_sync
#define mpool_close	kdb2_mpool_close
#define mpool_stat	kdb2_mpool_stat

__BEGIN_DECLS
MPOOL	*mpool_open __P((void *, int, db_pgno_t, db_pgno_t));
void	 mpool_filter __P((MPOOL *, void (*)(void *, db_pgno_t, void *),
	    void (*)(void *, db_pgno_t, void *), void *));
void	*mpool_new __P((MPOOL *, db_pgno_t *, u_int));
void	*mpool_get __P((MPOOL *, db_pgno_t, u_int));
int	 mpool_delete __P((MPOOL *, void *));
int	 mpool_put __P((MPOOL *, void *, u_int));
int	 mpool_sync __P((MPOOL *));
int	 mpool_close __P((MPOOL *));

void	 mpool_stat __P((MPOOL *));

__END_DECLS
