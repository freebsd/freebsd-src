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
static char sccsid[] = "@(#)bt_delete.c	8.2 (Berkeley) 9/7/93";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <db.h>
#include "btree.h"

static int bt_bdelete __P((BTREE *, const DBT *));

/*
 * __BT_DELETE -- Delete the item(s) referenced by a key.
 *
 * Parameters:
 *	dbp:	pointer to access method
 *	key:	key to delete
 *	flags:	R_CURSOR if deleting what the cursor references
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS and RET_SPECIAL if the key not found.
 */
int
__bt_delete(dbp, key, flags)
	const DB *dbp;
	const DBT *key;
	u_int flags;
{
	BTREE *t;
	int status;

	t = dbp->internal;

	/* Toss any page pinned across calls. */
	if (t->bt_pinned != NULL) {
		mpool_put(t->bt_mp, t->bt_pinned, 0);
		t->bt_pinned = NULL;
	}

	if (ISSET(t, B_RDONLY)) {
		errno = EPERM;
		return (RET_ERROR);
	}

	switch(flags) {
	case 0:
		status = bt_bdelete(t, key);
		break;
	case R_CURSOR:
		/*
		 * If flags is R_CURSOR, delete the cursor; must already have
		 * started a scan and not have already deleted the record.  For
		 * the delete cursor bit to have been set requires that the
		 * scan be initialized, so no reason to check.
		 */
		if (!ISSET(t, B_SEQINIT))
                        goto einval;
		status = ISSET(t, B_DELCRSR) ?
		    RET_SPECIAL : __bt_crsrdel(t, &t->bt_bcursor);
		break;
	default:
einval:		errno = EINVAL;
		return (RET_ERROR);
	}
	if (status == RET_SUCCESS)
		SET(t, B_MODIFIED);
	return (status);
}

/*
 * BT_BDELETE -- Delete all key/data pairs matching the specified key.
 *
 * Parameters:
 *	tree:	tree
 *	key:	key to delete
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS and RET_SPECIAL if the key not found.
 */
static int
bt_bdelete(t, key)
	BTREE *t;
	const DBT *key;
{
	EPG *e, save;
	PAGE *h;
	pgno_t cpgno, pg;
	indx_t cindex;
	int deleted, dirty1, dirty2, exact;

	/* Find any matching record; __bt_search pins the page. */
	if ((e = __bt_search(t, key, &exact)) == NULL)
		return (RET_ERROR);
	if (!exact) {
		mpool_put(t->bt_mp, e->page, 0);
		return (RET_SPECIAL);
	}

	/*
	 * Delete forward, then delete backward, from the found key.  The
	 * ordering is so that the deletions don't mess up the page refs.
	 * The first loop deletes the key from the original page, the second
	 * unpins the original page.  In the first loop, dirty1 is set if
	 * the original page is modified, and dirty2 is set if any subsequent
	 * pages are modified.  In the second loop, dirty1 starts off set if
	 * the original page has been modified, and is set if any subsequent
	 * pages are modified.
	 *
	 * If find the key referenced by the cursor, don't delete it, just
	 * flag it for future deletion.  The cursor page number is P_INVALID
	 * unless the sequential scan is initialized, so no reason to check.
	 * A special case is when the already deleted cursor record was the
	 * only record found.  If so, then the delete opertion fails as no
	 * records were deleted.
	 *
	 * Cycle in place in the current page until the current record doesn't
	 * match the key or the page is empty.  If the latter, walk forward,
	 * skipping empty pages and repeating until a record doesn't match
	 * the key or the end of the tree is reached.
	 */
	cpgno = t->bt_bcursor.pgno;
	cindex = t->bt_bcursor.index;
	save = *e;
	dirty1 = 0;
	for (h = e->page, deleted = 0;;) {
		dirty2 = 0;
		do {
			if (h->pgno == cpgno && e->index == cindex) {
				if (!ISSET(t, B_DELCRSR)) {
					SET(t, B_DELCRSR);
					deleted = 1;
				}
				++e->index;
			} else {
				if (__bt_dleaf(t, h, e->index)) {
					if (h->pgno != save.page->pgno)
						mpool_put(t->bt_mp, h, dirty2);
					mpool_put(t->bt_mp, save.page, dirty1);
					return (RET_ERROR);
				}
				if (h->pgno == save.page->pgno)
					dirty1 = MPOOL_DIRTY;
				else
					dirty2 = MPOOL_DIRTY;
				deleted = 1;
			}
		} while (e->index < NEXTINDEX(h) && __bt_cmp(t, key, e) == 0);

		/*
		 * Quit if didn't find a match, no next page, or first key on
		 * the next page doesn't match.  Don't unpin the original page
		 * unless an error occurs.
		 */
		if (e->index < NEXTINDEX(h))
			break;
		for (;;) {
			if ((pg = h->nextpg) == P_INVALID)
				goto done1;
			if (h->pgno != save.page->pgno)
				mpool_put(t->bt_mp, h, dirty2);
			if ((h = mpool_get(t->bt_mp, pg, 0)) == NULL) {
				mpool_put(t->bt_mp, save.page, dirty1);
				return (RET_ERROR);
			}
			if (NEXTINDEX(h) != 0) {
				e->page = h;
				e->index = 0;
				break;
			}
		}

		if (__bt_cmp(t, key, e) != 0)
			break;
	}

	/*
	 * Reach here with the original page and the last page referenced
	 * pinned (they may be the same).  Release it if not the original.
	 */
done1:	if (h->pgno != save.page->pgno)
		mpool_put(t->bt_mp, h, dirty2);

	/*
	 * Walk backwards from the record previous to the record returned by
	 * __bt_search, skipping empty pages, until a record doesn't match
	 * the key or reach the beginning of the tree.
	 */
	*e = save;
	for (;;) {
		if (e->index)
			--e->index;
		for (h = e->page; e->index; --e->index) {
			if (__bt_cmp(t, key, e) != 0)
				goto done2;
			if (h->pgno == cpgno && e->index == cindex) {
				if (!ISSET(t, B_DELCRSR)) {
					SET(t, B_DELCRSR);
					deleted = 1;
				}
			} else {
				if (__bt_dleaf(t, h, e->index) == RET_ERROR) {
					mpool_put(t->bt_mp, h, dirty1);
					return (RET_ERROR);
				}
				if (h->pgno == save.page->pgno)
					dirty1 = MPOOL_DIRTY;
				deleted = 1;
			}
		}

		if ((pg = h->prevpg) == P_INVALID)
			goto done2;
		mpool_put(t->bt_mp, h, dirty1);
		dirty1 = 0;
		if ((e->page = mpool_get(t->bt_mp, pg, 0)) == NULL)
			return (RET_ERROR);
		e->index = NEXTINDEX(e->page);
	}

	/*
	 * Reach here with the last page that was looked at pinned.  Release
	 * it.
	 */
done2:	mpool_put(t->bt_mp, h, dirty1);
	return (deleted ? RET_SUCCESS : RET_SPECIAL);
}

/*
 * __BT_DLEAF -- Delete a single record from a leaf page.
 *
 * Parameters:
 *	t:	tree
 *	index:	index on current page to delete
 *
 * Returns:
 *	RET_SUCCESS, RET_ERROR.
 */
int
__bt_dleaf(t, h, index)
	BTREE *t;
	PAGE *h;
	int index;
{
	register BLEAF *bl;
	register indx_t *ip, offset;
	register size_t nbytes;
	register int cnt;
	char *from;
	void *to;

	/*
	 * Delete a record from a btree leaf page.  Internal records are never
	 * deleted from internal pages, regardless of the records that caused
	 * them to be added being deleted.  Pages made empty by deletion are
	 * not reclaimed.  They are, however, made available for reuse.
	 *
	 * Pack the remaining entries at the end of the page, shift the indices
	 * down, overwriting the deleted record and its index.  If the record
	 * uses overflow pages, make them available for reuse.
	 */
	to = bl = GETBLEAF(h, index);
	if (bl->flags & P_BIGKEY && __ovfl_delete(t, bl->bytes) == RET_ERROR)
		return (RET_ERROR);
	if (bl->flags & P_BIGDATA &&
	    __ovfl_delete(t, bl->bytes + bl->ksize) == RET_ERROR)
		return (RET_ERROR);
	nbytes = NBLEAF(bl);

	/*
	 * Compress the key/data pairs.  Compress and adjust the [BR]LEAF
	 * offsets.  Reset the headers.
	 */
	from = (char *)h + h->upper;
	memmove(from + nbytes, from, (char *)to - from);
	h->upper += nbytes;

	offset = h->linp[index];
	for (cnt = index, ip = &h->linp[0]; cnt--; ++ip)
		if (ip[0] < offset)
			ip[0] += nbytes;
	for (cnt = NEXTINDEX(h) - index; --cnt; ++ip)
		ip[0] = ip[1] < offset ? ip[1] + nbytes : ip[1];
	h->lower -= sizeof(indx_t);
	return (RET_SUCCESS);
}
