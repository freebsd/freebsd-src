/*	$Id: mdoc_hash.c,v 1.27 2016/07/15 18:03:45 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2015 Ingo Schwarze <schwarze@openbsd.org>
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
#include "config.h"

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mandoc.h"
#include "roff.h"
#include "mdoc.h"
#include "libmandoc.h"
#include "libmdoc.h"

static	unsigned char	 table[27 * 12];


void
mdoc_hash_init(void)
{
	int		 i, j, major;
	const char	*p;

	if (*table != '\0')
		return;

	memset(table, UCHAR_MAX, sizeof(table));

	for (i = 0; i < (int)MDOC_MAX; i++) {
		p = mdoc_macronames[i];

		if (isalpha((unsigned char)p[1]))
			major = 12 * (tolower((unsigned char)p[1]) - 97);
		else
			major = 12 * 26;

		for (j = 0; j < 12; j++)
			if (UCHAR_MAX == table[major + j]) {
				table[major + j] = (unsigned char)i;
				break;
			}

		assert(j < 12);
	}
}

int
mdoc_hash_find(const char *p)
{
	int		  major, i, j;

	if (0 == p[0])
		return TOKEN_NONE;
	if ( ! isalpha((unsigned char)p[0]) && '%' != p[0])
		return TOKEN_NONE;

	if (isalpha((unsigned char)p[1]))
		major = 12 * (tolower((unsigned char)p[1]) - 97);
	else if ('1' == p[1])
		major = 12 * 26;
	else
		return TOKEN_NONE;

	if (p[2] && p[3])
		return TOKEN_NONE;

	for (j = 0; j < 12; j++) {
		if (UCHAR_MAX == (i = table[major + j]))
			break;
		if (0 == strcmp(p, mdoc_macronames[i]))
			return i;
	}

	return TOKEN_NONE;
}
