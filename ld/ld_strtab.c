/*-
 * Copyright (c) 2012,2013 Kai Wang
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "ld.h"
#include "ld_strtab.h"

ELFTC_VCSID("$Id: ld_strtab.c 3279 2015-12-11 21:39:16Z kaiwang27 $");

#define	_DEFAULT_STRTAB_SIZE	512

struct ld_str {
	char *s;
	size_t off, len;
	UT_hash_handle hh;
};

struct ld_strtab {
	struct ld_str *st_pool;
	char *st_buf;
	size_t st_cap;
	size_t st_size;
	unsigned char st_suffix;
};


static void _resize_strtab(struct ld *ld, struct ld_strtab *st,
    size_t newsize);

struct ld_strtab *
ld_strtab_alloc(struct ld *ld, unsigned char suffix)
{
	struct ld_strtab *st;

	if ((st = calloc(1, sizeof(*st))) == NULL)
		ld_fatal_std(ld, "calloc");
	
	st->st_size = 0;
	if (suffix) {
		st->st_suffix = 1;
		st->st_cap = _DEFAULT_STRTAB_SIZE;
		if ((st->st_buf = calloc(1, st->st_cap)) == NULL)
			ld_fatal_std(ld, "calloc");
		ld_strtab_insert(ld, st, "");
	} else
		st->st_size = 1;

	return (st);
}

void
ld_strtab_free(struct ld_strtab *st)
{
	struct ld_str *str, *tmp;

	if (st == NULL)
		return;

	if (st->st_pool != NULL) {
		HASH_ITER(hh, st->st_pool, str, tmp) {
			HASH_DELETE(hh, st->st_pool, str);
			free(str->s);
			free(str);
		}
	}

	free(st->st_buf);
	free(st);
}

char *
ld_strtab_getbuf(struct ld *ld, struct ld_strtab *st)
{
	struct ld_str *str, *tmp;
	char *p, *end;

	assert(st != NULL);

	if (st->st_suffix)
		return (st->st_buf);

	if (st->st_buf == NULL) {
		if ((st->st_buf = malloc(st->st_size)) == NULL)
			ld_fatal_std(ld, "malloc");
		/* Flatten the string hash table. */
		p = st->st_buf;
		end = p + st->st_size;
		*p++ = '\0';
		HASH_ITER(hh, st->st_pool, str, tmp) {
			memcpy(p, str->s, str->len);
			p[str->len] = '\0';
			p += str->len + 1;
		}
		assert(p == end);
	}

	return (st->st_buf);
}

size_t
ld_strtab_getsize(struct ld_strtab *st)
{

	return (st->st_size);
}

static void
_resize_strtab(struct ld *ld, struct ld_strtab *st, size_t newsize)
{

	assert(st != NULL && st->st_suffix);
	if ((st->st_buf = realloc(st->st_buf, newsize)) == NULL)
		ld_fatal_std(ld, "realloc");
	st->st_cap = newsize;
}

size_t
ld_strtab_insert_no_suffix(struct ld *ld, struct ld_strtab *st, char *s)
{
	struct ld_str *str;

	assert(st != NULL && st->st_suffix == 0);

	if (s == NULL)
		return (0);

	if (*s == '\0')
		return (0);

	HASH_FIND_STR(st->st_pool, s, str);
	if (str != NULL)
		return (str->off);

	if ((str = calloc(1, sizeof(*str))) == NULL)
		ld_fatal_std(ld, "calloc");

	if ((str->s = strdup(s)) == NULL)
		ld_fatal_std(ld, "strdup");

	str->len = strlen(s);
	HASH_ADD_KEYPTR(hh, st->st_pool, str->s, str->len, str);

	str->off = st->st_size;
	st->st_size += str->len + 1;

	return (str->off);
}

void
ld_strtab_insert(struct ld *ld, struct ld_strtab *st, const char *s)
{
	const char *r;
	char *b, *c;
	size_t len, slen;
	int append;

	assert(st != NULL && st->st_buf != NULL && st->st_suffix);

	if (s == NULL)
		return;

	slen = strlen(s);
	append = 0;
	b = st->st_buf;
	for (c = b; c < b + st->st_size;) {
		len = strlen(c);
		if (!append && len >= slen) {
			r = c + (len - slen);
			if (strcmp(r, s) == 0)
				return;
		} else if (len < slen && len != 0) {
			r = s + (slen - len);
			if (strcmp(c, r) == 0) {
				st->st_size -= len + 1;
				memmove(c, c + len + 1, st->st_size - (c - b));
				append = 1;
				continue;
			}
		}
		c += len + 1;
	}

	while (st->st_size + slen + 1 >= st->st_cap)
		_resize_strtab(ld, st, st->st_cap * 2);

	b = st->st_buf;
	strncpy(&b[st->st_size], s, slen);
	b[st->st_size + slen] = '\0';
	st->st_size += slen + 1;
}

size_t
ld_strtab_lookup(struct ld_strtab *st, const char *s)
{
	const char *b, *c, *r;
	size_t len, slen;

	assert(st != NULL && st->st_buf != NULL && st->st_suffix);

	if (s == NULL)
		return (0);

	slen = strlen(s);
	b = st->st_buf;
	for (c = b; c < b + st->st_size;) {
		len = strlen(c);
		if (len >= slen) {
			r = c + (len - slen);
			if (strcmp(r, s) == 0)
				return (r - b);
		}
		c += len + 1;
	}

	return (-1);
}
