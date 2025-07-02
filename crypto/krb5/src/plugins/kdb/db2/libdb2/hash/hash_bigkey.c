/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
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
static char sccsid[] = "@(#)hash_bigkey.c	8.5 (Berkeley) 11/2/95";
#endif /* LIBC_SCCS and not lint */

/*
 * PACKAGE: hash
 * DESCRIPTION:
 *	Big key/data handling for the hashing package.
 *
 * ROUTINES:
 * External
 *	__big_keydata
 *	__big_split
 *	__big_insert
 *	__big_return
 *	__big_delete
 *	__find_last_page
 * Internal
 *	collect_key
 *	collect_data
 */
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#ifdef DEBUG
#include <assert.h>
#endif

#include "db-int.h"
#include "hash.h"
#include "page.h"
#include "extern.h"

static int32_t collect_key __P((HTAB *, PAGE16 *, int32_t, db_pgno_t *));
static int32_t collect_data __P((HTAB *, PAGE16 *, int32_t));

/*
 * Big_insert
 *
 * You need to do an insert and the key/data pair is greater than
 * MINFILL * the bucket size
 *
 * Returns:
 *	 0 ==> OK
 *	-1 ==> ERROR
 */
int32_t
__big_insert(hashp, pagep, key, val)
	HTAB *hashp;
	PAGE16 *pagep;
	const DBT *key, *val;
{
	size_t  key_size, val_size;
	indx_t  key_move_bytes, val_move_bytes;
	int8_t *key_data, *val_data, base_page;

	key_data = (int8_t *)key->data;
	key_size = key->size;
	val_data = (int8_t *)val->data;
	val_size = val->size;

	NUM_ENT(pagep) = NUM_ENT(pagep) + 1;

	for (base_page = 1; key_size + val_size;) {
		/* Add a page! */
		pagep =
		    __add_bigpage(hashp, pagep, NUM_ENT(pagep) - 1, base_page);
		if (!pagep)
			return (-1);

		/* There's just going to be one entry on this page. */
		NUM_ENT(pagep) = 1;

		/* Move the key's data. */
		key_move_bytes = MIN(FREESPACE(pagep), key_size);
		/* Mark the page as to how much key & data is on this page. */
		BIGKEYLEN(pagep) = key_move_bytes;
		val_move_bytes =
		    MIN(FREESPACE(pagep) - key_move_bytes, val_size);
		BIGDATALEN(pagep) = val_move_bytes;

		/* Note big pages build beginning --> end, not vice versa. */
		if (key_move_bytes)
			memmove(BIGKEY(pagep), key_data, key_move_bytes);
		if (val_move_bytes)
			memmove(BIGDATA(pagep), val_data, val_move_bytes);

		key_size -= key_move_bytes;
		key_data += key_move_bytes;
		val_size -= val_move_bytes;
		val_data += val_move_bytes;

		base_page = 0;
	}
	__put_page(hashp, pagep, A_RAW, 1);
	return (0);
}

/*
 * Called when we need to delete a big pair.
 *
 * Returns:
 *	 0 => OK
 *	-1 => ERROR
 */
int32_t
#ifdef __STDC__
__big_delete(HTAB *hashp, PAGE16 *pagep, indx_t ndx)
#else
__big_delete(hashp, pagep, ndx)
	HTAB *hashp;
	PAGE16 *pagep;
	u_int32_t ndx;		/* Index of big pair on base page. */
#endif
{
	PAGE16 *last_pagep;

	/* Get first page with big key/data. */
	pagep = __get_page(hashp, OADDR_TO_PAGE(DATA_OFF(pagep, ndx)), A_RAW);
	if (!pagep)
		return (-1);

	/*
	 * Traverse through the pages, freeing the previous one (except
	 * the first) at each new page.
	 */
	while (NEXT_PGNO(pagep) != INVALID_PGNO) {
		last_pagep = pagep;
		pagep = __get_page(hashp, NEXT_PGNO(pagep), A_RAW);
		if (!pagep)
			return (-1);
		__delete_page(hashp, last_pagep, A_OVFL);
	}

	/* Free the last page in the chain. */
	__delete_page(hashp, pagep, A_OVFL);
	return (0);
}

/*
 * Given a key, indicates whether the big key at cursorp matches the
 * given key.
 *
 * Returns:
 *	 1 = Found!
 *	 0 = Key not found
 *	-1 error
 */
int32_t
__find_bigpair(hashp, cursorp, key, size)
	HTAB *hashp;
	CURSOR *cursorp;
	int8_t *key;
	int32_t size;
{
	PAGE16 *pagep, *hold_pagep;
	db_pgno_t  next_pgno;
	int32_t ksize;
	int8_t *kkey;

	ksize = size;
	kkey = key;

	hold_pagep = NULL;
	/* Chances are, hashp->cpage is the base page. */
	if (cursorp->pagep)
		pagep = hold_pagep = cursorp->pagep;
	else {
		pagep = __get_page(hashp, cursorp->pgno, A_RAW);
		if (!pagep)
			return (-1);
	}

	/*
	 * Now, get the first page with the big stuff on it.
	 *
	 * XXX
	 * KLUDGE: we know that cursor is looking at the _next_ item, so
	 * we have to look at pgndx - 1.
	 */
	next_pgno = OADDR_TO_PAGE(DATA_OFF(pagep, (cursorp->pgndx - 1)));
	if (!hold_pagep)
		__put_page(hashp, pagep, A_RAW, 0);
	pagep = __get_page(hashp, next_pgno, A_RAW);
	if (!pagep)
		return (-1);

	/* While there are both keys to compare. */
	while ((ksize > 0) && (BIGKEYLEN(pagep))) {
		if (ksize < KEY_OFF(pagep, 0) ||
		    memcmp(BIGKEY(pagep), kkey, BIGKEYLEN(pagep))) {
			__put_page(hashp, pagep, A_RAW, 0);
			return (0);
		}
		kkey += BIGKEYLEN(pagep);
		ksize -= BIGKEYLEN(pagep);
		if (NEXT_PGNO(pagep) != INVALID_PGNO) {
			next_pgno = NEXT_PGNO(pagep);
			__put_page(hashp, pagep, A_RAW, 0);
			pagep = __get_page(hashp, next_pgno, A_RAW);
			if (!pagep)
				return (-1);
		}
	}
	__put_page(hashp, pagep, A_RAW, 0);
#ifdef DEBUG
	assert(ksize >= 0);
#endif
	if (ksize != 0) {
#ifdef HASH_STATISTICS
		++hash_collisions;
#endif
		return (0);
	} else
		return (1);
}

/*
 * Fill in the key and data for this big pair.
 */
int32_t
__big_keydata(hashp, pagep, key, val, ndx)
	HTAB *hashp;
	PAGE16 *pagep;
	DBT *key, *val;
	int32_t ndx;
{
	ITEM_INFO ii;
	PAGE16 *key_pagep;
	db_pgno_t last_page;

	key_pagep =
	    __get_page(hashp, OADDR_TO_PAGE(DATA_OFF(pagep, ndx)), A_RAW);
	if (!key_pagep)
		return (-1);
	key->size = collect_key(hashp, key_pagep, 0, &last_page);
	key->data = hashp->bigkey_buf;
	__put_page(hashp, key_pagep, A_RAW, 0);

	if (key->size == (size_t)-1)
		return (-1);

	/* Create an item_info to direct __big_return to the beginning pgno. */
	ii.pgno = last_page;
	return (__big_return(hashp, &ii, val, 1));
}

/*
 * Return the big key on page, ndx.
 */
int32_t
#ifdef __STDC__
__get_bigkey(HTAB *hashp, PAGE16 *pagep, indx_t ndx, DBT *key)
#else
__get_bigkey(hashp, pagep, ndx, key)
	HTAB *hashp;
	PAGE16 *pagep;
	u_int32_t ndx;
	DBT *key;
#endif
{
	PAGE16 *key_pagep;

	key_pagep =
	    __get_page(hashp, OADDR_TO_PAGE(DATA_OFF(pagep, ndx)), A_RAW);
	if (!key_pagep)
		return (-1);
	key->size = collect_key(hashp, key_pagep, 0, NULL);
	key->data = hashp->bigkey_buf;

	__put_page(hashp, key_pagep, A_RAW, 0);

	return (0);
}

/*
 * Return the big key and data indicated in item_info.
 */
int32_t
__big_return(hashp, item_info, val, on_bigkey_page)
	HTAB *hashp;
	ITEM_INFO *item_info;
	DBT *val;
	int32_t on_bigkey_page;
{
	PAGE16 *pagep;
	db_pgno_t next_pgno;

	if (!on_bigkey_page) {
		/* Get first page with big pair on it. */
		pagep = __get_page(hashp,
		    OADDR_TO_PAGE(item_info->data_off), A_RAW);
		if (!pagep)
			return (-1);
	} else {
		pagep = __get_page(hashp, item_info->pgno, A_RAW);
		if (!pagep)
			return (-1);
	}

	/* Traverse through the bigkey pages until a page with data is found. */
	while (!BIGDATALEN(pagep)) {
		next_pgno = NEXT_PGNO(pagep);
		__put_page(hashp, pagep, A_RAW, 0);
		pagep = __get_page(hashp, next_pgno, A_RAW);
		if (!pagep)
			return (-1);
	}

	val->size = collect_data(hashp, pagep, 0);
	if (val->size < 1)
		return (-1);
	val->data = (void *)hashp->bigdata_buf;

	__put_page(hashp, pagep, A_RAW, 0);
	return (0);
}

/*
 * Given a page with a big key on it, traverse through the pages counting data
 * length, and collect all of the data on the way up.  Store the key in
 * hashp->bigkey_buf.  last_page indicates to the calling function what the
 * last page with key on it is; this will help if you later want to retrieve
 * the data portion.
 *
 * Does the work for __get_bigkey.
 *
 * Return total length of data; -1 if error.
 */
static int32_t
collect_key(hashp, pagep, len, last_page)
	HTAB *hashp;
	PAGE16 *pagep;
	int32_t len;
	db_pgno_t *last_page;
{
	PAGE16 *next_pagep;
	int32_t totlen, retval;
	db_pgno_t next_pgno;
#ifdef DEBUG
	db_pgno_t save_addr;
#endif

	/* If this is the last page with key. */
	if (BIGDATALEN(pagep)) {
		totlen = len + BIGKEYLEN(pagep);
		if (hashp->bigkey_buf)
			free(hashp->bigkey_buf);
		hashp->bigkey_buf = (u_int8_t *)malloc(totlen);
		if (!hashp->bigkey_buf)
			return (-1);
		memcpy(hashp->bigkey_buf + len,
		    BIGKEY(pagep), BIGKEYLEN(pagep));
		if (last_page)
			*last_page = ADDR(pagep);
		return (totlen);
	}

	/* Key filled up all of last key page, so we've gone 1 too far. */
	if (BIGKEYLEN(pagep) == 0) {
		if (hashp->bigkey_buf)
			free(hashp->bigkey_buf);
		hashp->bigkey_buf = (u_int8_t *)malloc(len);
		return (hashp->bigkey_buf ? len : -1);
	}
	totlen = len + BIGKEYLEN(pagep);

	/* Set pagep to the next page in the chain. */
	if (last_page)
		*last_page = ADDR(pagep);
	next_pgno = NEXT_PGNO(pagep);
	next_pagep = __get_page(hashp, next_pgno, A_RAW);
	if (!next_pagep)
		return (-1);
#ifdef DEBUG
	save_addr = ADDR(pagep);
#endif
	retval = collect_key(hashp, next_pagep, totlen, last_page);

#ifdef DEBUG
	assert(save_addr == ADDR(pagep));
#endif
	memcpy(hashp->bigkey_buf + len, BIGKEY(pagep), BIGKEYLEN(pagep));
	__put_page(hashp, next_pagep, A_RAW, 0);

	return (retval);
}

/*
 * Given a page with big data on it, recur through the pages counting data
 * length, and collect all of the data on the way up.  Store the data in
 * hashp->bigdata_buf.
 *
 * Does the work for __big_return.
 *
 * Return total length of data; -1 if error.
 */
static int32_t
collect_data(hashp, pagep, len)
	HTAB *hashp;
	PAGE16 *pagep;
	int32_t len;
{
	PAGE16 *next_pagep;
	int32_t totlen, retval;
	db_pgno_t next_pgno;
#ifdef DEBUG
	db_pgno_t save_addr;
#endif

	/* If there is no next page. */
	if (NEXT_PGNO(pagep) == INVALID_PGNO) {
		if (hashp->bigdata_buf)
			free(hashp->bigdata_buf);
		totlen = len + BIGDATALEN(pagep);
		hashp->bigdata_buf = (u_int8_t *)malloc(totlen);
		if (!hashp->bigdata_buf)
			return (-1);
		memcpy(hashp->bigdata_buf + totlen - BIGDATALEN(pagep),
		    BIGDATA(pagep), BIGDATALEN(pagep));
		return (totlen);
	}
	totlen = len + BIGDATALEN(pagep);

	/* Set pagep to the next page in the chain. */
	next_pgno = NEXT_PGNO(pagep);
	next_pagep = __get_page(hashp, next_pgno, A_RAW);
	if (!next_pagep)
		return (-1);

#ifdef DEBUG
	save_addr = ADDR(pagep);
#endif
	retval = collect_data(hashp, next_pagep, totlen);
#ifdef DEBUG
	assert(save_addr == ADDR(pagep));
#endif
	memcpy(hashp->bigdata_buf + totlen - BIGDATALEN(pagep),
	    BIGDATA(pagep), BIGDATALEN(pagep));
	__put_page(hashp, next_pagep, A_RAW, 0);

	return (retval);
}
