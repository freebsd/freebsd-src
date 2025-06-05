/*
 * Copyright (C) 2002, 2016 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Olson.
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
static char sccsid[] = "@(#)bt_seq.c	8.9 (Berkeley) 6/20/95";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db-int.h"
#include "btree.h"

static int __bt_first __P((BTREE *, const DBT *, EPG *, int *));
static int __bt_seqadv __P((BTREE *, EPG *, int));
static int __bt_seqset __P((BTREE *, EPG *, DBT *, int));

static int bt_rseq_next(BTREE *, EPG *);
static int bt_rseq_prev(BTREE *, EPG *);

/*
 * Sequential scan support.
 *
 * The tree can be scanned sequentially, starting from either end of the
 * tree or from any specific key.  A scan request before any scanning is
 * done is initialized as starting from the least node.
 */

/*
 * __bt_seq --
 *	Btree sequential scan interface.
 *
 * Parameters:
 *	dbp:	pointer to access method
 *	key:	key for positioning and return value
 *	data:	data return value
 *	flags:	R_CURSOR, R_FIRST, R_LAST, R_NEXT, R_PREV.
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS or RET_SPECIAL if there's no next key.
 */
int
__bt_seq(dbp, key, data, flags)
	const DB *dbp;
	DBT *key, *data;
	u_int flags;
{
	BTREE *t;
	EPG e;
	int status;

	t = dbp->internal;

	/* Toss any page pinned across calls. */
	if (t->bt_pinned != NULL) {
		mpool_put(t->bt_mp, t->bt_pinned, 0);
		t->bt_pinned = NULL;
	}

	/*
	 * If scan unitialized as yet, or starting at a specific record, set
	 * the scan to a specific key.  Both __bt_seqset and __bt_seqadv pin
	 * the page the cursor references if they're successful.
	 */
	switch (flags) {
	case R_NEXT:
	case R_PREV:
	case R_RNEXT:
	case R_RPREV:
		if (F_ISSET(&t->bt_cursor, CURS_INIT)) {
			status = __bt_seqadv(t, &e, flags);
			break;
		}
		/* FALLTHROUGH */
	case R_FIRST:
	case R_LAST:
	case R_CURSOR:
		status = __bt_seqset(t, &e, key, flags);
		break;
	default:
		errno = EINVAL;
		return (RET_ERROR);
	}

	if (status == RET_SUCCESS) {
		__bt_setcur(t, e.page->pgno, e.index);

		status =
		    __bt_ret(t, &e, key, &t->bt_rkey, data, &t->bt_rdata, 0);

		/*
		 * If the user is doing concurrent access, we copied the
		 * key/data, toss the page.
		 */
		if (F_ISSET(t, B_DB_LOCK))
			mpool_put(t->bt_mp, e.page, 0);
		else
			t->bt_pinned = e.page;
	}
	return (status);
}

/*
 * __bt_seqset --
 *	Set the sequential scan to a specific key.
 *
 * Parameters:
 *	t:	tree
 *	ep:	storage for returned key
 *	key:	key for initial scan position
 *	flags:	R_CURSOR, R_FIRST, R_LAST, R_NEXT, R_PREV
 *
 * Side effects:
 *	Pins the page the cursor references.
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS or RET_SPECIAL if there's no next key.
 */
static int
__bt_seqset(t, ep, key, flags)
	BTREE *t;
	EPG *ep;
	DBT *key;
	int flags;
{
	PAGE *h;
	db_pgno_t pg;
	int exact;

	/*
	 * Find the first, last or specific key in the tree and point the
	 * cursor at it.  The cursor may not be moved until a new key has
	 * been found.
	 */
	switch (flags) {
	case R_CURSOR:				/* Keyed scan. */
		/*
		 * Find the first instance of the key or the smallest key
		 * which is greater than or equal to the specified key.
		 */
		if (key->data == NULL || key->size == 0) {
			errno = EINVAL;
			return (RET_ERROR);
		}
		return (__bt_first(t, key, ep, &exact));
	case R_FIRST:				/* First record. */
	case R_NEXT:
	case R_RNEXT:
		BT_CLR(t);
		/* Walk down the left-hand side of the tree. */
		for (pg = P_ROOT;;) {
			if ((h = mpool_get(t->bt_mp, pg, 0)) == NULL)
				return (RET_ERROR);

			/* Check for an empty tree. */
			if (NEXTINDEX(h) == 0) {
				mpool_put(t->bt_mp, h, 0);
				return (RET_SPECIAL);
			}

			if (h->flags & (P_BLEAF | P_RLEAF))
				break;
			pg = GETBINTERNAL(h, 0)->pgno;
			BT_PUSH(t, h->pgno, 0);
			mpool_put(t->bt_mp, h, 0);
		}
		ep->page = h;
		ep->index = 0;
		break;
	case R_LAST:				/* Last record. */
	case R_PREV:
	case R_RPREV:
		BT_CLR(t);
		/* Walk down the right-hand side of the tree. */
		for (pg = P_ROOT;;) {
			if ((h = mpool_get(t->bt_mp, pg, 0)) == NULL)
				return (RET_ERROR);

			/* Check for an empty tree. */
			if (NEXTINDEX(h) == 0) {
				mpool_put(t->bt_mp, h, 0);
				return (RET_SPECIAL);
			}

			if (h->flags & (P_BLEAF | P_RLEAF))
				break;
			pg = GETBINTERNAL(h, NEXTINDEX(h) - 1)->pgno;
			BT_PUSH(t, h->pgno, NEXTINDEX(h) - 1);
			mpool_put(t->bt_mp, h, 0);
		}

		ep->page = h;
		ep->index = NEXTINDEX(h) - 1;
		break;
	}
	return (RET_SUCCESS);
}

/*
 * __bt_seqadvance --
 *	Advance the sequential scan.
 *
 * Parameters:
 *	t:	tree
 *	flags:	R_NEXT, R_PREV
 *
 * Side effects:
 *	Pins the page the new key/data record is on.
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS or RET_SPECIAL if there's no next key.
 */
static int
__bt_seqadv(t, ep, flags)
	BTREE *t;
	EPG *ep;
	int flags;
{
	CURSOR *c;
	PAGE *h;
	indx_t idx = 0;
	db_pgno_t pg;
	int exact, rval;

	/*
	 * There are a couple of states that we can be in.  The cursor has
	 * been initialized by the time we get here, but that's all we know.
	 */
	c = &t->bt_cursor;

	/*
	 * The cursor was deleted and there weren't any duplicate records,
	 * so the cursor's key was saved.  Find out where that key would
	 * be in the current tree.  If the returned key is an exact match,
	 * it means that a key/data pair was inserted into the tree after
	 * the delete.  We could reasonably return the key, but the problem
	 * is that this is the access pattern we'll see if the user is
	 * doing seq(..., R_NEXT)/put(..., 0) pairs, i.e. the put deletes
	 * the cursor record and then replaces it, so the cursor was saved,
	 * and we'll simply return the same "new" record until the user
	 * notices and doesn't do a put() of it.  Since the key is an exact
	 * match, we could as easily put the new record before the cursor,
	 * and we've made no guarantee to return it.  So, move forward or
	 * back a record if it's an exact match.
	 *
	 * XXX
	 * In the current implementation, put's to the cursor are done with
	 * delete/add pairs.  This has two consequences.  First, it means
	 * that seq(..., R_NEXT)/put(..., R_CURSOR) pairs are going to exhibit
	 * the same behavior as above.  Second, you can return the same key
	 * twice if you have duplicate records.  The scenario is that the
	 * cursor record is deleted, moving the cursor forward or backward
	 * to a duplicate.  The add then inserts the new record at a location
	 * ahead of the cursor because duplicates aren't sorted in any way,
	 * and the new record is later returned.  This has to be fixed at some
	 * point.
	 */
	if (F_ISSET(c, CURS_ACQUIRE)) {
		if ((rval = __bt_first(t, &c->key, ep, &exact)) == RET_ERROR)
			return (RET_ERROR);
		if (!exact)
			return (rval);
		/*
		 * XXX
		 * Kluge -- get, release, get the page.
		 */
		c->pg.pgno = ep->page->pgno;
		c->pg.index = ep->index;
		mpool_put(t->bt_mp, ep->page, 0);
	}

	/* Get the page referenced by the cursor. */
	if ((h = mpool_get(t->bt_mp, c->pg.pgno, 0)) == NULL)
		return (RET_ERROR);

	/*
 	 * Find the next/previous record in the tree and point the cursor at
	 * it.  The cursor may not be moved until a new key has been found.
	 */
	switch (flags) {
	case R_NEXT:			/* Next record. */
	case R_RNEXT:
		/*
		 * The cursor was deleted in duplicate records, and moved
		 * forward to a record that has yet to be returned.  Clear
		 * that flag, and return the record.
		 */
		if (F_ISSET(c, CURS_AFTER))
			goto usecurrent;
		idx = c->pg.index;
		if (++idx == NEXTINDEX(h)) {
			if (flags == R_RNEXT) {
				ep->page = h;
				ep->index = idx;
				return (bt_rseq_next(t, ep));
			}
			pg = h->nextpg;
			mpool_put(t->bt_mp, h, 0);
			if (pg == P_INVALID)
				return (RET_SPECIAL);
			if ((h = mpool_get(t->bt_mp, pg, 0)) == NULL)
				return (RET_ERROR);
			idx = 0;
		}
		break;
	case R_PREV:			/* Previous record. */
	case R_RPREV:
		/*
		 * The cursor was deleted in duplicate records, and moved
		 * backward to a record that has yet to be returned.  Clear
		 * that flag, and return the record.
		 */
		if (F_ISSET(c, CURS_BEFORE)) {
usecurrent:		F_CLR(c, CURS_AFTER | CURS_BEFORE);
			ep->page = h;
			ep->index = c->pg.index;
			return (RET_SUCCESS);
		}
		idx = c->pg.index;
		if (idx == 0) {
			if (flags == R_RPREV) {
				ep->page = h;
				ep->index = idx;
				return (bt_rseq_prev(t, ep));
			}
			pg = h->prevpg;
			mpool_put(t->bt_mp, h, 0);
			if (pg == P_INVALID)
				return (RET_SPECIAL);
			if ((h = mpool_get(t->bt_mp, pg, 0)) == NULL)
				return (RET_ERROR);
			idx = NEXTINDEX(h) - 1;
		} else
			--idx;
		break;
	}

	ep->page = h;
	ep->index = idx;
	return (RET_SUCCESS);
}

/*
 * Get the first item on the next page, but by going up and down the tree.
 */
static int
bt_rseq_next(BTREE *t, EPG *ep)
{
	PAGE *h;
	indx_t idx;
	EPGNO *up;
	db_pgno_t pg;

	h = ep->page;
	idx = ep->index;
	do {
		/* Move up the tree. */
		up = BT_POP(t);
		mpool_put(t->bt_mp, h, 0);
		/* Did we hit the right edge of the root? */
		if (up == NULL)
			return (RET_SPECIAL);
		if ((h = mpool_get(t->bt_mp, up->pgno, 0)) == NULL)
			return (RET_ERROR);
		idx = up->index;
	} while (++idx == NEXTINDEX(h));

	while (!(h->flags & (P_BLEAF | P_RLEAF))) {
		/* Move back down the tree. */
		BT_PUSH(t, h->pgno, idx);
		pg = GETBINTERNAL(h, idx)->pgno;
		mpool_put(t->bt_mp, h, 0);
		if ((h = mpool_get(t->bt_mp, pg, 0)) == NULL)
			return (RET_ERROR);
		idx = 0;
	}
	ep->page = h;
	ep->index = idx;
	return (RET_SUCCESS);
}

/*
 * Get the last item on the previous page, but by going up and down the tree.
 */
static int
bt_rseq_prev(BTREE *t, EPG *ep)
{
	PAGE *h;
	indx_t idx;
	EPGNO *up;
	db_pgno_t pg;

	h = ep->page;
	idx = ep->index;
	do {
		/* Move up the tree. */
		up = BT_POP(t);
		mpool_put(t->bt_mp, h, 0);
		/* Did we hit the left edge of the root? */
		if (up == NULL)
			return (RET_SPECIAL);
		if ((h = mpool_get(t->bt_mp, up->pgno, 0)) == NULL)
			return (RET_ERROR);
		idx = up->index;
	} while (idx == 0);
	--idx;
	while (!(h->flags & (P_BLEAF | P_RLEAF))) {
		/* Move back down the tree. */
		BT_PUSH(t, h->pgno, idx);
		pg = GETBINTERNAL(h, idx)->pgno;
		mpool_put(t->bt_mp, h, 0);
		if ((h = mpool_get(t->bt_mp, pg, 0)) == NULL)
			return (RET_ERROR);
		idx = NEXTINDEX(h) - 1;
	}
	ep->page = h;
	ep->index = idx;
	return (RET_SUCCESS);
}

/*
 * __bt_first --
 *	Find the first entry.
 *
 * Parameters:
 *	t:	the tree
 *    key:	the key
 *  erval:	return EPG
 * exactp:	pointer to exact match flag
 *
 * Returns:
 *	The first entry in the tree greater than or equal to key,
 *	or RET_SPECIAL if no such key exists.
 */
static int
__bt_first(t, key, erval, exactp)
	BTREE *t;
	const DBT *key;
	EPG *erval;
	int *exactp;
{
	PAGE *h, *hprev;
	EPG *ep, save;
	db_pgno_t pg;

	/*
	 * Find any matching record; __bt_search pins the page.
	 *
	 * If it's an exact match and duplicates are possible, walk backwards
	 * in the tree until we find the first one.  Otherwise, make sure it's
	 * a valid key (__bt_search may return an index just past the end of a
	 * page) and return it.
	 */
	if ((ep = __bt_search(t, key, exactp)) == NULL)
		return (RET_SPECIAL);
	if (*exactp) {
		if (F_ISSET(t, B_NODUPS)) {
			*erval = *ep;
			return (RET_SUCCESS);
		}

		/*
		 * Walk backwards, as long as the entry matches and there are
		 * keys left in the tree.  Save a copy of each match in case
		 * we go too far.
		 */
		save = *ep;
		h = ep->page;
		do {
			if (save.page->pgno != ep->page->pgno) {
				mpool_put(t->bt_mp, save.page, 0);
				save = *ep;
			} else
				save.index = ep->index;

			/*
			 * Don't unpin the page the last (or original) match
			 * was on, but make sure it's unpinned if an error
			 * occurs.
			 */
			if (ep->index == 0) {
				if (h->prevpg == P_INVALID)
					break;
				if (h->pgno != save.page->pgno)
					mpool_put(t->bt_mp, h, 0);
				if ((hprev = mpool_get(t->bt_mp,
				    h->prevpg, 0)) == NULL) {
					if (h->pgno == save.page->pgno)
						mpool_put(t->bt_mp,
						    save.page, 0);
					return (RET_ERROR);
				}
				ep->page = h = hprev;
				ep->index = NEXTINDEX(h);
			}
			--ep->index;
		} while (__bt_cmp(t, key, ep) == 0);

		/*
		 * Reach here with the last page that was looked at pinned,
		 * which may or may not be the same as the last (or original)
		 * match page.  If it's not useful, release it.
		 */
		if (h->pgno != save.page->pgno)
			mpool_put(t->bt_mp, h, 0);

		*erval = save;
		return (RET_SUCCESS);
	}

	/* If at the end of a page, find the next entry. */
	if (ep->index == NEXTINDEX(ep->page)) {
		h = ep->page;
		pg = h->nextpg;
		mpool_put(t->bt_mp, h, 0);
		if (pg == P_INVALID)
			return (RET_SPECIAL);
		if ((h = mpool_get(t->bt_mp, pg, 0)) == NULL)
			return (RET_ERROR);
		ep->index = 0;
		ep->page = h;
	}
	*erval = *ep;
	return (RET_SUCCESS);
}

/*
 * __bt_setcur --
 *	Set the cursor to an entry in the tree.
 *
 * Parameters:
 *	t:	the tree
 *   pgno:	page number
 *  index:	page index
 */
void
__bt_setcur(t, pgno, idx)
	BTREE *t;
	db_pgno_t pgno;
	u_int idx;
{
	/* Lose any already deleted key. */
	if (t->bt_cursor.key.data != NULL) {
		free(t->bt_cursor.key.data);
		t->bt_cursor.key.size = 0;
		t->bt_cursor.key.data = NULL;
	}
	F_CLR(&t->bt_cursor, CURS_ACQUIRE | CURS_AFTER | CURS_BEFORE);

	/* Update the cursor. */
	t->bt_cursor.pg.pgno = pgno;
	t->bt_cursor.pg.index = idx;
	F_SET(&t->bt_cursor, CURS_INIT);
}
