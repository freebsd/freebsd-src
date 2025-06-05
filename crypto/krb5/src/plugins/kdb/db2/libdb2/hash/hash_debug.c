/*-
 * Copyright (c) 1995
 *	The President and Fellows of Harvard University
 *
 * This code is derived from software contributed to Harvard by
 * Jeremy Rassen.
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
static char sccsid[] = "@(#)hash_debug.c	8.4 (Berkeley) 11/7/95";
#endif /* LIBC_SCCS and not lint */

#ifdef DEBUG
/*
 * PACKAGE:  hashing
 *
 * DESCRIPTION:
 *	Debug routines.
 *
 * ROUTINES:
 *
 * External
 *	__dump_bucket
 */
#include <stdio.h>
#include <string.h>

#include "db-int.h"
#include "hash.h"
#include "page.h"
#include "extern.h"

void
__dump_bucket(hashp, bucket)
	HTAB *hashp;
	u_int32_t bucket;
{
	CURSOR cursor;
	DBT key, val;
	ITEM_INFO item_info;
	int var;
	char *cp;

	cursor.pagep = NULL;
	item_info.seek_size = 0;
	item_info.seek_found_page = 0;

	__get_item_reset(hashp, &cursor);

	cursor.bucket = bucket;
	for (;;) {
		__get_item_next(hashp, &cursor, &key, &val, &item_info);
		if (item_info.status == ITEM_ERROR) {
			(void)printf("get_item_next returned error\n");
			break;
		} else if (item_info.status == ITEM_NO_MORE)
			break;

		if (item_info.key_off == BIGPAIR) {
			if (__big_keydata(hashp, cursor.pagep, &key, &val,
			    item_info.pgndx)) {
				(void)printf("__big_keydata returned error\n");
				break;
			}
		}

		if (key.size == sizeof(int)) {
			memcpy(&var, key.data, sizeof(int));
			(void)printf("%d\n", var);
		} else {
			for (cp = (char *)key.data; key.size--; cp++)
				(void)printf("%c", *cp);
			(void)printf("\n");
		}
	}
	__get_item_done(hashp, &cursor);
}
#endif /* DEBUG */
