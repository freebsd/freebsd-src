/*	$OpenBSD: mem.c,v 1.5 2009/10/27 23:59:37 deraadt Exp $	*/

/*
 * Copyright (c) 2003, Otto Moerbeek <otto@drijf.net>
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

#include <openssl/err.h>

#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

struct number *
new_number(void)
{
	struct number	*n;

	n = bmalloc(sizeof(*n));
	n->scale = 0;
	n->number = BN_new();
	if (n->number == NULL)
		err(1, NULL);
	return (n);
}

void
free_number(struct number *n)
{

	BN_free(n->number);
	free(n);
}

struct number *
dup_number(const struct number *a)
{
	struct number	*n;

	n = bmalloc(sizeof(*n));
	n->scale = a->scale;
	n->number = BN_dup(a->number);
	bn_checkp(n->number);
	return (n);
}

void *
bmalloc(size_t sz)
{
	void	*p;

	p = malloc(sz);
	if (p == NULL)
		err(1, NULL);
	return (p);
}

void *
brealloc(void *p, size_t sz)
{
	void	*q;

	q = realloc(p, sz);
	if (q == NULL)
		err(1, NULL);
	return (q);
}

char *
bstrdup(const char *p)
{
	char	*q;

	q = strdup(p);
	if (q == NULL)
		err(1, NULL);
	return (q);
}

void
bn_check(int x)						\
{

	if (x == 0)
		err(1, "big number failure %lx", ERR_get_error());
}

void
bn_checkp(const void *p)						\
{

	if (p == NULL)
		err(1, "allocation failure %lx", ERR_get_error());
}
