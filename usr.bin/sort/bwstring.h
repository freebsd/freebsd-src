/*	$FreeBSD$	*/

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2009 Gabor Kovesdan <gabor@FreeBSD.org>
 * Copyright (C) 2012 Oleg Moskalenko <mom040267@gmail.com>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if !defined(__BWSTRING_H__)
#define	__BWSTRING_H__

#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <sysexits.h>
#include <wchar.h>

#include "sort.h"
#include "mem.h"

extern bool byte_sort;

/* wchar_t is of 4 bytes: */
#define	SIZEOF_WCHAR_STRING(LEN) ((LEN)*sizeof(wchar_t))

struct wstr {
	size_t len;
	wchar_t str[];
};

struct cstr {
	size_t len;
	char str[];
};

/*
 * Binary "wide" string
 */
struct bwstring
{
	union {
		struct wstr wdata;
		struct cstr cdata;
	};
};

typedef void *bwstring_iterator;

#define	BWSLEN(s) ((mb_cur_max == 1) ? (s)->cdata.len : (s)->wdata.len)
struct bwstring *bwsalloc(size_t sz);

size_t bwsrawlen(const struct bwstring *bws);
const void* bwsrawdata(const struct bwstring *bws);
void bws_setlen(struct bwstring *bws, size_t newlen);
size_t bws_memsize(const struct bwstring *bws);
double bwstod(struct bwstring *s0, bool *empty);
int bws_month_score(const struct bwstring *s0);

struct bwstring *ignore_leading_blanks(struct bwstring *str);
struct bwstring *ignore_nonprinting(struct bwstring *str);
struct bwstring *dictionary_order(struct bwstring *str);
struct bwstring *ignore_case(struct bwstring *str);

void bwsprintf(FILE*, struct bwstring*, const char *prefix, const char *suffix);
void bws_disorder_warnx(struct bwstring *s, const char *fn, size_t pos);

struct bwstring *bwsdup(const struct bwstring *s);
struct bwstring *bwssbdup(const wchar_t *str, size_t size);
struct bwstring *bwscsbdup(const unsigned char *str, size_t size);
void bwsfree(const struct bwstring *s);
struct bwstring *bwsnocpy(struct bwstring *dst, const struct bwstring *src, size_t offset, size_t size);
int bwscmp(const struct bwstring *bws1, const struct bwstring *bws2, size_t offset);
int bwsncmp(const struct bwstring *bws1, const struct bwstring *bws2, size_t offset, size_t len);
int bwscoll(const struct bwstring *bws1, const struct bwstring *bws2, size_t offset);
size_t bwsfwrite(struct bwstring *bws, FILE *f, bool zero_ended);

static inline bwstring_iterator
bws_begin(struct bwstring *bws)
{

	return ((bwstring_iterator)bws->wdata.str);
}

static inline bwstring_iterator
bws_end(struct bwstring *bws)
{

	return ((mb_cur_max == 1) ?
	    (bwstring_iterator) (bws->cdata.str + bws->cdata.len) :
	    (bwstring_iterator) (bws->wdata.str + bws->wdata.len));
}

static inline bwstring_iterator
bws_iterator_inc(bwstring_iterator iter, size_t pos)
{

	if (mb_cur_max == 1)
		return ((unsigned char *) iter) + pos;
	else
		return ((wchar_t*) iter) + pos;
}

static inline wchar_t
bws_get_iter_value(bwstring_iterator iter)
{

	if (mb_cur_max == 1)
		return *((unsigned char *) iter);
	else
		return *((wchar_t*) iter);
}

int
bws_iterator_cmp(bwstring_iterator iter1, bwstring_iterator iter2, size_t len);

#define	BWS_GET(bws, pos) ((mb_cur_max == 1) ? (bws->cdata.str[(pos)]) : bws->wdata.str[(pos)])

void initialise_months(void);

#endif /* __BWSTRING_H__ */
