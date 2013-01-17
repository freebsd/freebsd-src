/* $OpenBSD: ohash_init.c,v 1.2 2004/06/22 20:00:16 espie Exp $ */
/* ex:ts=8 sw=4: 
 */

/* Copyright (c) 1999, 2004 Marc Espie <espie@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "ohash_int.h"

void 
ohash_init(struct ohash *h, unsigned int size, struct ohash_info *info)
{
	h->size = 1UL << size;
	if (h->size < MINSIZE)
		h->size = MINSIZE;
#ifdef STATS_HASH
	STAT_HASH_CREATION++;
	STAT_HASH_SIZE += h->size;
#endif
	/* Copy info so that caller may free it.  */
	h->info.key_offset = info->key_offset;
	h->info.halloc = info->halloc;
	h->info.hfree = info->hfree;
	h->info.alloc = info->alloc;
	h->info.data = info->data;
	h->t = (h->info.halloc)(sizeof(struct _ohash_record) * h->size,
	    h->info.data);
	h->total = h->deleted = 0;
}
