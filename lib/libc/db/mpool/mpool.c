/*-
 * Copyright (c) 1990, 1993
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)mpool.c	8.2 (Berkeley) 2/21/94";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <db.h>
#define	__MPOOLINTERFACE_PRIVATE
#include "mpool.h"

static BKT *mpool_bkt __P((MPOOL *));
static BKT *mpool_look __P((MPOOL *, pgno_t));
static int  mpool_write __P((MPOOL *, BKT *));
#ifdef DEBUG
static void __mpoolerr __P((const char *fmt, ...));
#endif

/*
 * MPOOL_OPEN -- initialize a memory pool.
 *
 * Parameters:
 *	key:		Shared buffer key.
 *	fd:		File descriptor.
 *	pagesize:	File page size.
 *	maxcache:	Max number of cached pages.
 *
 * Returns:
 *	MPOOL pointer, NULL on error.
 */
MPOOL *
mpool_open(key, fd, pagesize, maxcache)
	DBT *key;
	int fd;
	pgno_t pagesize, maxcache;
{
	struct stat sb;
	MPOOL *mp;
	int entry;

	if (fstat(fd, &sb))
		return (NULL);
	/* XXX
	 * We should only set st_size to 0 for pipes -- 4.4BSD has the fix so
	 * that stat(2) returns true for ISSOCK on pipes.  Until then, this is
	 * fairly close.
	 */
	if (!S_ISREG(sb.st_mode)) {
		errno = ESPIPE;
		return (NULL);
	}

	if ((mp = (MPOOL *)malloc(sizeof(MPOOL))) == NULL)
		return (NULL);
	mp->free.cnext = mp->free.cprev = (BKT *)&mp->free;
	mp->lru.cnext = mp->lru.cprev = (BKT *)&mp->lru;
	for (entry = 0; entry < HASHSIZE; ++entry)
		mp->hashtable[entry].hnext = mp->hashtable[entry].hprev = 
		    mp->hashtable[entry].cnext = mp->hashtable[entry].cprev =
		    (BKT *)&mp->hashtable[entry];
	mp->curcache = 0;
	mp->maxcache = maxcache;
	mp->pagesize = pagesize;
	mp->npages = sb.st_size / pagesize;
	mp->fd = fd;
	mp->pgcookie = NULL;
	mp->pgin = mp->pgout = NULL;

#ifdef STATISTICS
	mp->cachehit = mp->cachemiss = mp->pagealloc = mp->pageflush = 
	    mp->pageget = mp->pagenew = mp->pageput = mp->pageread = 
	    mp->pagewrite = 0;
#endif
	return (mp);
}

/*
 * MPOOL_FILTER -- initialize input/output filters.
 *
 * Parameters:
 *	pgin:		Page in conversion routine.
 *	pgout:		Page out conversion routine.
 *	pgcookie:	Cookie for page in/out routines.
 */
void
mpool_filter(mp, pgin, pgout, pgcookie)
	MPOOL *mp;
	void (*pgin) __P((void *, pgno_t, void *));
	void (*pgout) __P((void *, pgno_t, void *));
	void *pgcookie;
{
	mp->pgin = pgin;
	mp->pgout = pgout;
	mp->pgcookie = pgcookie;
}
	
/*
 * MPOOL_NEW -- get a new page
 *
 * Parameters:
 *	mp:		mpool cookie
 *	pgnoadddr:	place to store new page number
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
void *
mpool_new(mp, pgnoaddr)
	MPOOL *mp;
	pgno_t *pgnoaddr;
{
	BKT *b;
	BKTHDR *hp;

#ifdef STATISTICS
	++mp->pagenew;
#endif
	/*
	 * Get a BKT from the cache.  Assign a new page number, attach it to
	 * the hash and lru chains and return.
	 */
	if ((b = mpool_bkt(mp)) == NULL)
		return (NULL);
	*pgnoaddr = b->pgno = mp->npages++;
	b->flags = MPOOL_PINNED;
	inshash(b, b->pgno);
	inschain(b, &mp->lru);
	return (b->page);
}

/*
 * MPOOL_GET -- get a page from the pool
 *
 * Parameters:
 *	mp:	mpool cookie
 *	pgno:	page number
 *	flags:	not used
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
void *
mpool_get(mp, pgno, flags)
	MPOOL *mp;
	pgno_t pgno;
	u_int flags;		/* XXX not used? */
{
	BKT *b;
	BKTHDR *hp;
	off_t off;
	int nr;

	/*
	 * If asking for a specific page that is already in the cache, find
	 * it and return it.
	 */
	if (b = mpool_look(mp, pgno)) {
#ifdef STATISTICS
		++mp->pageget;
#endif
#ifdef DEBUG
		if (b->flags & MPOOL_PINNED)
			__mpoolerr("mpool_get: page %d already pinned",
			    b->pgno);
#endif
		rmchain(b);
		inschain(b, &mp->lru);
		b->flags |= MPOOL_PINNED;
		return (b->page);
	}

	/* Not allowed to retrieve a non-existent page. */
	if (pgno >= mp->npages) {
		errno = EINVAL;
		return (NULL);
	}

	/* Get a page from the cache. */
	if ((b = mpool_bkt(mp)) == NULL)
		return (NULL);
	b->pgno = pgno;
	b->flags = MPOOL_PINNED;

#ifdef STATISTICS
	++mp->pageread;
#endif
	/* Read in the contents. */
	off = mp->pagesize * pgno;
	if (lseek(mp->fd, off, SEEK_SET) != off)
		return (NULL);
	if ((nr = read(mp->fd, b->page, mp->pagesize)) != mp->pagesize) {
		if (nr >= 0)
			errno = EFTYPE;
		return (NULL);
	}
	if (mp->pgin)
		(mp->pgin)(mp->pgcookie, b->pgno, b->page);

	inshash(b, b->pgno);
	inschain(b, &mp->lru);
#ifdef STATISTICS
	++mp->pageget;
#endif
	return (b->page);
}

/*
 * MPOOL_PUT -- return a page to the pool
 *
 * Parameters:
 *	mp:	mpool cookie
 *	page:	page pointer
 *	pgno:	page number
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
int
mpool_put(mp, page, flags)
	MPOOL *mp;
	void *page;
	u_int flags;
{
	BKT *baddr;
#ifdef DEBUG
	BKT *b;
#endif

#ifdef STATISTICS
	++mp->pageput;
#endif
	baddr = (BKT *)((char *)page - sizeof(BKT));
#ifdef DEBUG
	if (!(baddr->flags & MPOOL_PINNED))
		__mpoolerr("mpool_put: page %d not pinned", b->pgno);
	for (b = mp->lru.cnext; b != (BKT *)&mp->lru; b = b->cnext) {
		if (b == (BKT *)&mp->lru)
			__mpoolerr("mpool_put: %0x: bad address", baddr);
		if (b == baddr)
			break;
	}
#endif
	baddr->flags &= ~MPOOL_PINNED;
	baddr->flags |= flags & MPOOL_DIRTY;
	return (RET_SUCCESS);
}

/*
 * MPOOL_CLOSE -- close the buffer pool
 *
 * Parameters:
 *	mp:	mpool cookie
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
int
mpool_close(mp)
	MPOOL *mp;
{
	BKT *b, *next;

	/* Free up any space allocated to the lru pages. */
	for (b = mp->lru.cprev; b != (BKT *)&mp->lru; b = next) {
		next = b->cprev;
		free(b);
	}
	free(mp);
	return (RET_SUCCESS);
}

/*
 * MPOOL_SYNC -- sync the file to disk.
 *
 * Parameters:
 *	mp:	mpool cookie
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
int
mpool_sync(mp)
	MPOOL *mp;
{
	BKT *b;

	for (b = mp->lru.cprev; b != (BKT *)&mp->lru; b = b->cprev)
		if (b->flags & MPOOL_DIRTY && mpool_write(mp, b) == RET_ERROR)
			return (RET_ERROR);
	return (fsync(mp->fd) ? RET_ERROR : RET_SUCCESS);
}

/*
 * MPOOL_BKT -- get/create a BKT from the cache
 *
 * Parameters:
 *	mp:	mpool cookie
 *
 * Returns:
 *	NULL on failure and a pointer to the BKT on success	
 */
static BKT *
mpool_bkt(mp)
	MPOOL *mp;
{
	BKT *b;

	if (mp->curcache < mp->maxcache)
		goto new;

	/*
	 * If the cache is maxxed out, search the lru list for a buffer we
	 * can flush.  If we find one, write it if necessary and take it off
	 * any lists.  If we don't find anything we grow the cache anyway.
	 * The cache never shrinks.
	 */
	for (b = mp->lru.cprev; b != (BKT *)&mp->lru; b = b->cprev)
		if (!(b->flags & MPOOL_PINNED)) {
			if (b->flags & MPOOL_DIRTY &&
			    mpool_write(mp, b) == RET_ERROR)
				return (NULL);
			rmhash(b);
			rmchain(b);
#ifdef STATISTICS
			++mp->pageflush;
#endif
#ifdef DEBUG
			{
				void *spage;
				spage = b->page;
				memset(b, 0xff, sizeof(BKT) + mp->pagesize);
				b->page = spage;
			}
#endif
			return (b);
		}

new:	if ((b = (BKT *)malloc(sizeof(BKT) + mp->pagesize)) == NULL)
		return (NULL);
#ifdef STATISTICS
	++mp->pagealloc;
#endif
#ifdef DEBUG
	memset(b, 0xff, sizeof(BKT) + mp->pagesize);
#endif
	b->page = (char *)b + sizeof(BKT);
	++mp->curcache;
	return (b);
}

/*
 * MPOOL_WRITE -- sync a page to disk
 *
 * Parameters:
 *	mp:	mpool cookie
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
static int
mpool_write(mp, b)
	MPOOL *mp;
	BKT *b;
{
	off_t off;

	if (mp->pgout)
		(mp->pgout)(mp->pgcookie, b->pgno, b->page);

#ifdef STATISTICS
	++mp->pagewrite;
#endif
	off = mp->pagesize * b->pgno;
	if (lseek(mp->fd, off, SEEK_SET) != off)
		return (RET_ERROR);
	if (write(mp->fd, b->page, mp->pagesize) != mp->pagesize)
		return (RET_ERROR);
	b->flags &= ~MPOOL_DIRTY;
	return (RET_SUCCESS);
}

/*
 * MPOOL_LOOK -- lookup a page
 *
 * Parameters:
 *	mp:	mpool cookie
 *	pgno:	page number
 *
 * Returns:
 *	NULL on failure and a pointer to the BKT on success
 */
static BKT *
mpool_look(mp, pgno)
	MPOOL *mp;
	pgno_t pgno;
{
	register BKT *b;
	register BKTHDR *tb;

	/* XXX
	 * If find the buffer, put it first on the hash chain so can
	 * find it again quickly.
	 */
	tb = &mp->hashtable[HASHKEY(pgno)];
	for (b = tb->hnext; b != (BKT *)tb; b = b->hnext)
		if (b->pgno == pgno) {
#ifdef STATISTICS
			++mp->cachehit;
#endif
			return (b);
		}
#ifdef STATISTICS
	++mp->cachemiss;
#endif
	return (NULL);
}

#ifdef STATISTICS
/*
 * MPOOL_STAT -- cache statistics
 *
 * Parameters:
 *	mp:	mpool cookie
 */
void
mpool_stat(mp)
	MPOOL *mp;
{
	BKT *b;
	int cnt;
	char *sep;

	(void)fprintf(stderr, "%lu pages in the file\n", mp->npages);
	(void)fprintf(stderr,
	    "page size %lu, cacheing %lu pages of %lu page max cache\n",
	    mp->pagesize, mp->curcache, mp->maxcache);
	(void)fprintf(stderr, "%lu page puts, %lu page gets, %lu page new\n",
	    mp->pageput, mp->pageget, mp->pagenew);
	(void)fprintf(stderr, "%lu page allocs, %lu page flushes\n",
	    mp->pagealloc, mp->pageflush);
	if (mp->cachehit + mp->cachemiss)
		(void)fprintf(stderr,
		    "%.0f%% cache hit rate (%lu hits, %lu misses)\n", 
		    ((double)mp->cachehit / (mp->cachehit + mp->cachemiss))
		    * 100, mp->cachehit, mp->cachemiss);
	(void)fprintf(stderr, "%lu page reads, %lu page writes\n",
	    mp->pageread, mp->pagewrite);

	sep = "";
	cnt = 0;
	for (b = mp->lru.cnext; b != (BKT *)&mp->lru; b = b->cnext) {
		(void)fprintf(stderr, "%s%d", sep, b->pgno);
		if (b->flags & MPOOL_DIRTY)
			(void)fprintf(stderr, "d");
		if (b->flags & MPOOL_PINNED)
			(void)fprintf(stderr, "P");
		if (++cnt == 10) {
			sep = "\n";
			cnt = 0;
		} else
			sep = ", ";
			
	}
	(void)fprintf(stderr, "\n");
}
#endif

#ifdef DEBUG
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

static void
#if __STDC__
__mpoolerr(const char *fmt, ...)
#else
__mpoolerr(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	abort();
	/* NOTREACHED */
}
#endif
