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
static char sccsid[] = "@(#)rec_utils.c	8.2 (Berkeley) 9/7/93";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <db.h>
#include "recno.h"

/*
 * __REC_RET -- Build return data as a result of search or scan.
 *
 * Parameters:
 *	t:	tree
 *	d:	LEAF to be returned to the user.
 *	data:	user's data structure
 *
 * Returns:
 *	RET_SUCCESS, RET_ERROR.
 */
int
__rec_ret(t, e, nrec, key, data)
	BTREE *t;
	EPG *e;
	recno_t nrec;
	DBT *key, *data;
{
	register RLEAF *rl;
	register void *p;

	if (data == NULL)
		goto retkey;

	rl = GETRLEAF(e->page, e->index);

	/*
	 * We always copy big data to make it contigous.  Otherwise, we
	 * leave the page pinned and don't copy unless the user specified
	 * concurrent access.
	 */
	if (rl->flags & P_BIGDATA) {
		if (__ovfl_get(t, rl->bytes,
		    &data->size, &t->bt_dbuf, &t->bt_dbufsz))
			return (RET_ERROR);
		data->data = t->bt_dbuf;
	} else if (ISSET(t, B_DB_LOCK)) {
		/* Use +1 in case the first record retrieved is 0 length. */
		if (rl->dsize + 1 > t->bt_dbufsz) {
			if ((p = realloc(t->bt_dbuf, rl->dsize + 1)) == NULL)
				return (RET_ERROR);
			t->bt_dbuf = p;
			t->bt_dbufsz = rl->dsize + 1;
		}
		memmove(t->bt_dbuf, rl->bytes, rl->dsize);
		data->size = rl->dsize;
		data->data = t->bt_dbuf;
	} else {
		data->size = rl->dsize;
		data->data = rl->bytes;
	}

retkey:	if (key == NULL)
		return (RET_SUCCESS);

	/* We have to copy the key, it's not on the page. */
	if (sizeof(recno_t) > t->bt_kbufsz) {
		if ((p = realloc(t->bt_kbuf, sizeof(recno_t))) == NULL)
			return (RET_ERROR);
		t->bt_kbuf = p;
		t->bt_kbufsz = sizeof(recno_t);
	}
	memmove(t->bt_kbuf, &nrec, sizeof(recno_t));
	key->size = sizeof(recno_t);
	key->data = t->bt_kbuf;
	return (RET_SUCCESS);
}
