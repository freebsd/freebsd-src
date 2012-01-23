/* $OpenBSD: ohash_delete.c,v 1.2 2004/06/22 20:00:16 espie Exp $ */
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
/* hash_delete only frees the hash structure. Use hash_first/hash_next
 * to free entries as well.  */
void 
ohash_delete(struct ohash *h)
{
	(h->info.hfree)(h->t, sizeof(struct _ohash_record) * h->size,
		h->info.data);
#ifndef NDEBUG
	h->t = NULL;
#endif
}
