/*-
 * Copyright (c) 1990, 1993
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
static char sccsid[] = "@(#)bt_search.c	8.6 (Berkeley) 3/15/94";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>

#include <stdio.h>

#include <db.h>
#include "btree.h"

static int bt_snext __P((BTREE *, PAGE *, const DBT *, int *));
static int bt_sprev __P((BTREE *, PAGE *, const DBT *, int *));

/*
 * __BT_SEARCH -- Search a btree for a key.
 *
 * Parameters:
 *	t:	tree to search
 *	key:	key to find
 *	exactp:	pointer to exact match flag
 *
 * Returns:
 *	The EPG for matching record, if any, or the EPG for the location
 *	of the key, if it were inserted into the tree, is entered into
 *	the bt_cur field of the tree.  A pointer to the field is returned.
 */
EPG *
__bt_search(t, key, exactp)
	BTREE *t;
	const DBT *key;
	int *exactp;
{
	PAGE *h;
	indx_t base, index, lim;
	pgno_t pg;
	int cmp;

	BT_CLR(t);
	for (pg = P_ROOT;;) {
		if ((h = mpool_get(t->bt_mp, pg, 0)) == NULL)
			return (NULL);

		/* Do a binary search on the current page. */
		t->bt_cur.page = h;
		for (base = 0, lim = NEXTINDEX(h); lim; lim >>= 1) {
			t->bt_cur.index = index = base + (lim >> 1);
			if ((cmp = __bt_cmp(t, key, &t->bt_cur)) == 0) {
				if (h->flags & P_BLEAF) {
					*exactp = 1;
					return (&t->bt_cur);
				}
				goto next;
			}
			if (cmp > 0) {
				base = index + 1;
				--lim;
			}
		}

		/*
		 * If it's a leaf page, and duplicates aren't allowed, we're
		 * done.  If duplicates are allowed, it's possible that there
		 * were duplicate keys on duplicate pages, and they were later
		 * deleted, so we could be on a page with no matches while
		 * there are matches on other pages.  If we're at the start or
		 * end of a page, check on both sides.
		 */
		if (h->flags & P_BLEAF) {
			t->bt_cur.index = base;
			*exactp = 0;
			if (!ISSET(t, B_NODUPS)) {
				if (base == 0 &&
				    bt_sprev(t, h, key, exactp))
					return (&t->bt_cur);
				if (base == NEXTINDEX(h) &&
				    bt_snext(t, h, key, exactp))
					return (&t->bt_cur);
			}
			return (&t->bt_cur);
		}

		/*
		 * No match found.  Base is the smallest index greater than
		 * key and may be zero or a last + 1 index.  If it's non-zero,
		 * decrement by one, and record the internal page which should
		 * be a parent page for the key.  If a split later occurs, the
		 * inserted page will be to the right of the saved page.
		 */
		index = base ? base - 1 : base;

next:		if (__bt_push(t, h->pgno, index) == RET_ERROR)
			return (NULL);
		pg = GETBINTERNAL(h, index)->pgno;
		mpool_put(t->bt_mp, h, 0);
	}
}

/*
 * BT_SNEXT -- Check for an exact match after the key.
 *
 * Parameters:
 *	t:	tree to search
 *	h:	current page.
 *	key:	key to find
 *	exactp:	pointer to exact match flag
 *
 * Returns:
 *	If an exact match found.
 */
static int
bt_snext(t, h, key, exactp)
	BTREE *t;
	PAGE *h;
	const DBT *key;
	int *exactp;
{
	EPG e;
	PAGE *tp;
	pgno_t pg;

	/* Skip until reach the end of the tree or a key. */
	for (pg = h->nextpg; pg != P_INVALID;) {
		if ((tp = mpool_get(t->bt_mp, pg, 0)) == NULL) {
			mpool_put(t->bt_mp, h, 0);
			return (NULL);
		}
		if (NEXTINDEX(tp) != 0)
			break;
		pg = tp->prevpg;
		mpool_put(t->bt_mp, tp, 0);
	}
	/*
	 * The key is either an exact match, or not as good as
	 * the one we already have.
	 */
	if (pg != P_INVALID) {
		e.page = tp;
		e.index = NEXTINDEX(tp) - 1;
		if (__bt_cmp(t, key, &e) == 0) {
			mpool_put(t->bt_mp, h, 0);
			t->bt_cur = e;
			*exactp = 1;
			return (1);
		}
	}
	return (0);
}

/*
 * BT_SPREV -- Check for an exact match before the key.
 *
 * Parameters:
 *	t:	tree to search
 *	h:	current page.
 *	key:	key to find
 *	exactp:	pointer to exact match flag
 *
 * Returns:
 *	If an exact match found.
 */
static int
bt_sprev(t, h, key, exactp)
	BTREE *t;
	PAGE *h;
	const DBT *key;
	int *exactp;
{
	EPG e;
	PAGE *tp;
	pgno_t pg;

	/* Skip until reach the beginning of the tree or a key. */
	for (pg = h->prevpg; pg != P_INVALID;) {
		if ((tp = mpool_get(t->bt_mp, pg, 0)) == NULL) {
			mpool_put(t->bt_mp, h, 0);
			return (NULL);
		}
		if (NEXTINDEX(tp) != 0)
			break;
		pg = tp->prevpg;
		mpool_put(t->bt_mp, tp, 0);
	}
	/*
	 * The key is either an exact match, or not as good as
	 * the one we already have.
	 */
	if (pg != P_INVALID) {
		e.page = tp;
		e.index = NEXTINDEX(tp) - 1;
		if (__bt_cmp(t, key, &e) == 0) {
			mpool_put(t->bt_mp, h, 0);
			t->bt_cur = e;
			*exactp = 1;
			return (1);
		}
	}
	return (0);
}
