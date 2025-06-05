/*-
 * Copyright (c) 1990, 1993, 1994
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
static char sccsid[] = "@(#)mpool.c	8.7 (Berkeley) 11/2/95";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "db-int.h"
#include "mpool.h"

static BKT *mpool_bkt __P((MPOOL *));
static BKT *mpool_look __P((MPOOL *, db_pgno_t));
static int  mpool_write __P((MPOOL *, BKT *));

/*
 * mpool_open --
 *	Initialize a memory pool.
 */
MPOOL *
mpool_open(key, fd, pagesize, maxcache)
	void *key;
	int fd;
	db_pgno_t pagesize, maxcache;
{
	struct stat sb;
	MPOOL *mp;
	int entry;

	/*
	 * Get information about the file.
	 *
	 * XXX
	 * We don't currently handle pipes, although we should.
	 */
	if (fstat(fd, &sb))
		return (NULL);
	if (!S_ISREG(sb.st_mode)) {
		errno = ESPIPE;
		return (NULL);
	}

	/* Allocate and initialize the MPOOL cookie. */
	if ((mp = (MPOOL *)calloc(1, sizeof(MPOOL))) == NULL)
		return (NULL);
	TAILQ_INIT(&mp->lqh);
	for (entry = 0; entry < HASHSIZE; ++entry)
		TAILQ_INIT(&mp->hqh[entry]);
	mp->maxcache = maxcache;
	mp->npages = sb.st_size / pagesize;
	mp->pagesize = pagesize;
	mp->fd = fd;
	return (mp);
}

/*
 * mpool_filter --
 *	Initialize input/output filters.
 */
void
mpool_filter(mp, pgin, pgout, pgcookie)
	MPOOL *mp;
	void (*pgin) __P((void *, db_pgno_t, void *));
	void (*pgout) __P((void *, db_pgno_t, void *));
	void *pgcookie;
{
	mp->pgin = pgin;
	mp->pgout = pgout;
	mp->pgcookie = pgcookie;
}

/*
 * mpool_new --
 *	Get a new page of memory.
 */
void *
mpool_new(mp, pgnoaddr, flags)
	MPOOL *mp;
	db_pgno_t *pgnoaddr;
	u_int flags;
{
	struct _hqh *head;
	BKT *bp;

	if (mp->npages == MAX_PAGE_NUMBER) {
		(void)fprintf(stderr, "mpool_new: page allocation overflow.\n");
		abort();
	}
#ifdef STATISTICS
	++mp->pagenew;
#endif
	/*
	 * Get a BKT from the cache.  Assign a new page number, attach
	 * it to the head of the hash chain, the tail of the lru chain,
	 * and return.
	 */
	if ((bp = mpool_bkt(mp)) == NULL)
		return (NULL);
	if (flags == MPOOL_PAGE_REQUEST) {
		mp->npages++;
		bp->pgno = *pgnoaddr;
	} else
		bp->pgno = *pgnoaddr = mp->npages++;

	bp->flags = MPOOL_PINNED | MPOOL_INUSE;

	head = &mp->hqh[HASHKEY(bp->pgno)];
	TAILQ_INSERT_HEAD(head, bp, hq);
	TAILQ_INSERT_TAIL(&mp->lqh, bp, q);
	return (bp->page);
}

int
mpool_delete(mp, page)
	MPOOL *mp;
	void *page;
{
	struct _hqh *head;
	BKT *bp;

	bp = (void *)((char *)page - sizeof(BKT));

#ifdef DEBUG
	if (!(bp->flags & MPOOL_PINNED)) {
		(void)fprintf(stderr,
		    "mpool_delete: page %d not pinned\n", bp->pgno);
		abort();
	}
#endif

	/* Remove from the hash and lru queues. */
	head = &mp->hqh[HASHKEY(bp->pgno)];
	TAILQ_REMOVE(head, bp, hq);
	TAILQ_REMOVE(&mp->lqh, bp, q);

	free(bp);
	return (RET_SUCCESS);
}

/*
 * mpool_get
 *	Get a page.
 */
void *
mpool_get(mp, pgno, flags)
	MPOOL *mp;
	db_pgno_t pgno;
	u_int flags;				/* XXX not used? */
{
	struct _hqh *head;
	BKT *bp;
	off_t off;
	int nr;

#ifdef STATISTICS
	++mp->pageget;
#endif

	/* Check for a page that is cached. */
	if ((bp = mpool_look(mp, pgno)) != NULL) {
#ifdef DEBUG
		if (!(flags & MPOOL_IGNOREPIN) && bp->flags & MPOOL_PINNED) {
			(void)fprintf(stderr,
			    "mpool_get: page %d already pinned\n", bp->pgno);
			abort();
		}
#endif
		/*
		 * Move the page to the head of the hash chain and the tail
		 * of the lru chain.
		 */
		head = &mp->hqh[HASHKEY(bp->pgno)];
		TAILQ_REMOVE(head, bp, hq);
		TAILQ_INSERT_HEAD(head, bp, hq);
		TAILQ_REMOVE(&mp->lqh, bp, q);
		TAILQ_INSERT_TAIL(&mp->lqh, bp, q);

		/* Return a pinned page. */
		if (!(flags & MPOOL_IGNOREPIN))
			bp->flags |= MPOOL_PINNED;
		return (bp->page);
	}

	/* Get a page from the cache. */
	if ((bp = mpool_bkt(mp)) == NULL)
		return (NULL);

	/* Read in the contents. */
#ifdef STATISTICS
	++mp->pageread;
#endif
	off = mp->pagesize * pgno;
	if (off / mp->pagesize != pgno) {
	    /* Run past the end of the file, or at least the part we
	       can address without large-file support?  */
	    errno = E2BIG;
	    return NULL;
	}
	if (lseek(mp->fd, off, SEEK_SET) != off)
		return (NULL);

	if ((nr = read(mp->fd, bp->page, mp->pagesize)) !=
	    (ssize_t)mp->pagesize) {
		if (nr > 0) {
			/* A partial read is definitely bad. */
			errno = EINVAL;
			return (NULL);
		} else {
			/*
			 * A zero-length reads, means you need to create a
			 * new page.
			 */
			memset(bp->page, 0, mp->pagesize);
		}
	}

	/* Set the page number, pin the page. */
	bp->pgno = pgno;
	if (!(flags & MPOOL_IGNOREPIN))
		bp->flags = MPOOL_PINNED;
	bp->flags |= MPOOL_INUSE;

	/*
	 * Add the page to the head of the hash chain and the tail
	 * of the lru chain.
	 */
	head = &mp->hqh[HASHKEY(bp->pgno)];
	TAILQ_INSERT_HEAD(head, bp, hq);
	TAILQ_INSERT_TAIL(&mp->lqh, bp, q);

	/* Run through the user's filter. */
	if (mp->pgin != NULL)
		(mp->pgin)(mp->pgcookie, bp->pgno, bp->page);

	return (bp->page);
}

/*
 * mpool_put
 *	Return a page.
 */
int
mpool_put(mp, page, flags)
	MPOOL *mp;
	void *page;
	u_int flags;
{
	BKT *bp;

#ifdef STATISTICS
	++mp->pageput;
#endif
	bp = (void *)((char *)page - sizeof(BKT));
#ifdef DEBUG
	if (!(bp->flags & MPOOL_PINNED)) {
		(void)fprintf(stderr,
		    "mpool_put: page %d not pinned\n", bp->pgno);
		abort();
	}
#endif
	bp->flags &= ~MPOOL_PINNED;
	if (flags & MPOOL_DIRTY)
		bp->flags |= flags & MPOOL_DIRTY;
	return (RET_SUCCESS);
}

/*
 * mpool_close
 *	Close the buffer pool.
 */
int
mpool_close(mp)
	MPOOL *mp;
{
	BKT *bp;

	/* Free up any space allocated to the lru pages. */
	while ((bp = mp->lqh.tqh_first) != NULL) {
		TAILQ_REMOVE(&mp->lqh, mp->lqh.tqh_first, q);
		free(bp);
	}

	/* Free the MPOOL cookie. */
	free(mp);
	return (RET_SUCCESS);
}

/*
 * mpool_sync
 *	Sync the pool to disk.
 */
int
mpool_sync(mp)
	MPOOL *mp;
{
	BKT *bp;

	/* Walk the lru chain, flushing any dirty pages to disk. */
	for (bp = mp->lqh.tqh_first; bp != NULL; bp = bp->q.tqe_next)
		if (bp->flags & MPOOL_DIRTY &&
		    mpool_write(mp, bp) == RET_ERROR)
			return (RET_ERROR);

	/* Sync the file descriptor. */
	return (fsync(mp->fd) ? RET_ERROR : RET_SUCCESS);
}

/*
 * mpool_bkt
 *	Get a page from the cache (or create one).
 */
static BKT *
mpool_bkt(mp)
	MPOOL *mp;
{
	struct _hqh *head;
	BKT *bp;

	/* If under the max cached, always create a new page. */
	if (mp->curcache < mp->maxcache)
		goto new;

	/*
	 * If the cache is max'd out, walk the lru list for a buffer we
	 * can flush.  If we find one, write it (if necessary) and take it
	 * off any lists.  If we don't find anything we grow the cache anyway.
	 * The cache never shrinks.
	 */
	for (bp = mp->lqh.tqh_first; bp != NULL; bp = bp->q.tqe_next)
		if (!(bp->flags & MPOOL_PINNED)) {
			/* Flush if dirty. */
			if (bp->flags & MPOOL_DIRTY &&
			    mpool_write(mp, bp) == RET_ERROR)
				return (NULL);
#ifdef STATISTICS
			++mp->pageflush;
#endif
			/* Remove from the hash and lru queues. */
			head = &mp->hqh[HASHKEY(bp->pgno)];
			TAILQ_REMOVE(head, bp, hq);
			TAILQ_REMOVE(&mp->lqh, bp, q);
#if defined(DEBUG) && !defined(DEBUG_IDX0SPLIT)
			{ void *spage;
				spage = bp->page;
				memset(bp, 0xff, sizeof(BKT) + mp->pagesize);
				bp->page = spage;
			}
#endif
			bp->flags = 0;
			return (bp);
		}

new:	if ((bp = (BKT *)malloc(sizeof(BKT) + mp->pagesize)) == NULL)
		return (NULL);
#ifdef STATISTICS
	++mp->pagealloc;
#endif
#if defined(DEBUG) || defined(PURIFY) || 1
	memset(bp, 0xff, sizeof(BKT) + mp->pagesize);
#endif
	bp->page = (char *)bp + sizeof(BKT);
	bp->flags = 0;
	++mp->curcache;
	return (bp);
}

/*
 * mpool_write
 *	Write a page to disk.
 */
static int
mpool_write(mp, bp)
	MPOOL *mp;
	BKT *bp;
{
	off_t off;

#ifdef STATISTICS
	++mp->pagewrite;
#endif

	/* Run through the user's filter. */
	if (mp->pgout)
		(mp->pgout)(mp->pgcookie, bp->pgno, bp->page);

	off = mp->pagesize * bp->pgno;
	if (off / mp->pagesize != bp->pgno) {
	    /* Run past the end of the file, or at least the part we
	       can address without large-file support?  */
	    errno = E2BIG;
	    return RET_ERROR;
	}
	if (lseek(mp->fd, off, SEEK_SET) != off)
		return (RET_ERROR);
	if (write(mp->fd, bp->page, mp->pagesize) !=
	    (ssize_t)mp->pagesize)
		return (RET_ERROR);

	/*
	 * Re-run through the input filter since this page may soon be
	 * accessed via the cache, and whatever the user's output filter
	 * did may screw things up if we don't let the input filter
	 * restore the in-core copy.
	 */
	if (mp->pgin)
		(mp->pgin)(mp->pgcookie, bp->pgno, bp->page);
	bp->flags &= ~MPOOL_DIRTY;
	return (RET_SUCCESS);
}

/*
 * mpool_look
 *	Lookup a page in the cache.
 */
static BKT *
mpool_look(mp, pgno)
	MPOOL *mp;
	db_pgno_t pgno;
{
	struct _hqh *head;
	BKT *bp;

	head = &mp->hqh[HASHKEY(pgno)];
	for (bp = head->tqh_first; bp != NULL; bp = bp->hq.tqe_next)
		if ((bp->pgno == pgno) && (bp->flags & MPOOL_INUSE)) {
#ifdef STATISTICS
			++mp->cachehit;
#endif
			return (bp);
		}
#ifdef STATISTICS
	++mp->cachemiss;
#endif
	return (NULL);
}

#ifdef STATISTICS
/*
 * mpool_stat
 *	Print out cache statistics.
 */
void
mpool_stat(mp)
	MPOOL *mp;
{
	BKT *bp;
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
	for (bp = mp->lqh.tqh_first; bp != NULL; bp = bp->q.tqe_next) {
		(void)fprintf(stderr, "%s%d", sep, bp->pgno);
		if (bp->flags & MPOOL_DIRTY)
			(void)fprintf(stderr, "d");
		if (bp->flags & MPOOL_PINNED)
			(void)fprintf(stderr, "P");
		if (++cnt == 10) {
			sep = "\n";
			cnt = 0;
		} else
			sep = ", ";

	}
	(void)fprintf(stderr, "\n");
}
#else
void
mpool_stat(mp)
	MPOOL *mp;
{
}
#endif
