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

#include <sys/cdefs.h>
#include <ctype.h>
#include <errno.h>
#include <err.h>
#include <langinfo.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "bwstring.h"
#include "sort.h"

bool byte_sort;

struct wmonth {
	wchar_t *mon;
	wchar_t *ab;
	wchar_t *alt;
};

struct cmonth {
	char *mon;
	char *ab;
	char *alt;
};

static struct wmonth *wmonths;
static struct cmonth *cmonths;

static int
populate_cmonth(char **field, const nl_item item, int idx)
{
	char *tmp, *m;
	size_t i, len;

	tmp = nl_langinfo(item);
	if (debug_sort)
		printf("month[%d]=%s\n", idx, tmp);
	if (*tmp == '\0')
		return (0);
	m = sort_strdup(tmp);
	len = strlen(tmp);
	for (i = 0; i < len; i++)
		m[i] = toupper(m[i]);
	*field = m;

	return (1);
}

static int
populate_wmonth(wchar_t **field, const nl_item item, int idx)
{
	wchar_t *m;
	char *tmp;
	size_t i, len;

	tmp = nl_langinfo(item);
	if (debug_sort)
		printf("month[%d]=%s\n", idx, tmp);
	if (*tmp == '\0')
		return (0);
	len = strlen(tmp);
	m = sort_malloc(SIZEOF_WCHAR_STRING(len + 1));
	if (mbstowcs(m, tmp, len) == ((size_t) - 1)) {
		sort_free(m);
		return (0);
	}
	m[len] = L'\0';
	for (i = 0; i < len; i++)
		m[i] = towupper(m[i]);
	*field = m;

	return (1);
}

void
initialise_months(void)
{
	const nl_item mon_item[12] = { MON_1, MON_2, MON_3, MON_4,
	    MON_5, MON_6, MON_7, MON_8, MON_9, MON_10,
	    MON_11, MON_12 };
	const nl_item ab_item[12] = { ABMON_1, ABMON_2, ABMON_3, ABMON_4,
	    ABMON_5, ABMON_6, ABMON_7, ABMON_8, ABMON_9, ABMON_10,
	    ABMON_11, ABMON_12 };
	const nl_item alt_item[12] = { ALTMON_1, ALTMON_2, ALTMON_3, ALTMON_4,
	    ALTMON_5, ALTMON_6, ALTMON_7, ALTMON_8, ALTMON_9, ALTMON_10,
	    ALTMON_11, ALTMON_12 };
	int i;

	/*
	 * Handle all possible month formats: abbrevation, full name,
	 * standalone name (without case ending).
	 */
	if (mb_cur_max == 1) {
		if (cmonths == NULL) {
			cmonths = sort_malloc(sizeof(struct cmonth) * 12);
			for (i = 0; i < 12; i++) {
				if (!populate_cmonth(&cmonths[i].mon,
				    mon_item[i], i))
					continue;
				if (!populate_cmonth(&cmonths[i].ab,
				    ab_item[i], i))
					continue;
				if (!populate_cmonth(&cmonths[i].alt,
				    alt_item[i], i))
					continue;
			}
		}

	} else {
		if (wmonths == NULL) {
			wmonths = sort_malloc(sizeof(struct wmonth) * 12);
			for (i = 0; i < 12; i++) {
				if (!populate_wmonth(&wmonths[i].mon,
				    mon_item[i], i))
					continue;
				if (!populate_wmonth(&wmonths[i].ab,
				    ab_item[i], i))
					continue;
				if (!populate_wmonth(&wmonths[i].alt,
				    alt_item[i], i))
					continue;
			}
		}
	}
}

/*
 * Compare two wide-character strings
 */
static int
wide_str_coll(const wchar_t *s1, const wchar_t *s2)
{
	int ret;

	errno = 0;
	ret = wcscoll(s1, s2);
	if (errno == EILSEQ) {
		errno = 0;
		ret = wcscmp(s1, s2);
		if (errno != 0) {
			for (size_t i = 0; ; ++i) {
				wchar_t c1 = s1[i];
				wchar_t c2 = s2[i];
				if (c1 == L'\0')
					return ((c2 == L'\0') ? 0 : -1);
				if (c2 == L'\0')
					return (+1);
				if (c1 == c2)
					continue;
				return ((int)(c1 - c2));
			}
		}
	}
	return (ret);
}

/* counterparts of wcs functions */

void
bwsprintf(FILE *f, struct bwstring *bws, const char *prefix, const char *suffix)
{

	if (mb_cur_max == 1)
		fprintf(f, "%s%s%s", prefix, bws->cdata.str, suffix);
	else
		fprintf(f, "%s%S%s", prefix, bws->wdata.str, suffix);
}

const void* bwsrawdata(const struct bwstring *bws)
{

	return (bws->wdata.str);
}

size_t bwsrawlen(const struct bwstring *bws)
{

	return ((mb_cur_max == 1) ? bws->cdata.len :
	    SIZEOF_WCHAR_STRING(bws->wdata.len));
}

size_t
bws_memsize(const struct bwstring *bws)
{

	return ((mb_cur_max == 1) ?
	    (bws->cdata.len + 2 + sizeof(struct bwstring)) :
	    (SIZEOF_WCHAR_STRING(bws->wdata.len + 1) + sizeof(struct bwstring)));
}

void
bws_setlen(struct bwstring *bws, size_t newlen)
{

	if (mb_cur_max == 1 && bws && newlen != bws->cdata.len &&
	    newlen <= bws->cdata.len) {
		bws->cdata.len = newlen;
		bws->cdata.str[newlen] = '\0';
	} else if (bws && newlen != bws->wdata.len && newlen <= bws->wdata.len) {
		bws->wdata.len = newlen;
		bws->wdata.str[newlen] = L'\0';
	}
}

/*
 * Allocate a new binary string of specified size
 */
struct bwstring *
bwsalloc(size_t sz)
{
	struct bwstring *ret;

	if (mb_cur_max == 1) {
		ret = sort_malloc(sizeof(struct bwstring) + 1 + sz);
		ret->cdata.len = sz;
		ret->cdata.str[sz] = '\0';
	} else {
		ret = sort_malloc(
		    sizeof(struct bwstring) + SIZEOF_WCHAR_STRING(sz + 1));
		ret->wdata.len = sz;
		ret->wdata.str[sz] = L'\0';
	}

	return (ret);
}

/*
 * Create a copy of binary string.
 * New string size equals the length of the old string.
 */
struct bwstring *
bwsdup(const struct bwstring *s)
{

	if (s == NULL)
		return (NULL);
	else {
		struct bwstring *ret = bwsalloc(BWSLEN(s));

		if (mb_cur_max == 1)
			memcpy(ret->cdata.str, s->cdata.str, (s->cdata.len));
		else
			memcpy(ret->wdata.str, s->wdata.str,
			    SIZEOF_WCHAR_STRING(s->wdata.len));

		return (ret);
	}
}

/*
 * Create a new binary string from a wide character buffer.
 */
struct bwstring *
bwssbdup(const wchar_t *str, size_t len)
{

	if (str == NULL)
		return ((len == 0) ? bwsalloc(0) : NULL);
	else {
		struct bwstring *ret;

		ret = bwsalloc(len);

		if (mb_cur_max == 1)
			for (size_t i = 0; i < len; ++i)
				ret->cdata.str[i] = (char)str[i];
		else
			memcpy(ret->wdata.str, str, SIZEOF_WCHAR_STRING(len));

		return (ret);
	}
}

/*
 * Create a new binary string from a raw binary buffer.
 */
struct bwstring *
bwscsbdup(const unsigned char *str, size_t len)
{
	struct bwstring *ret;

	ret = bwsalloc(len);

	if (str) {
		if (mb_cur_max == 1)
			memcpy(ret->cdata.str, str, len);
		else {
			mbstate_t mbs;
			const char *s;
			size_t charlen, chars, cptr;

			chars = 0;
			cptr = 0;
			s = (const char *) str;

			memset(&mbs, 0, sizeof(mbs));

			while (cptr < len) {
				size_t n = mb_cur_max;

				if (n > len - cptr)
					n = len - cptr;
				charlen = mbrlen(s + cptr, n, &mbs);
				switch (charlen) {
				case 0:
					/* FALLTHROUGH */
				case (size_t) -1:
					/* FALLTHROUGH */
				case (size_t) -2:
					ret->wdata.str[chars++] =
					    (unsigned char) s[cptr];
					++cptr;
					break;
				default:
					n = mbrtowc(ret->wdata.str + (chars++),
					    s + cptr, charlen, &mbs);
					if ((n == (size_t)-1) || (n == (size_t)-2))
						/* NOTREACHED */
						err(2, "mbrtowc error");
					cptr += charlen;
				}
			}

			ret->wdata.len = chars;
			ret->wdata.str[ret->wdata.len] = L'\0';
		}
	}
	return (ret);
}

/*
 * De-allocate object memory
 */
void
bwsfree(const struct bwstring *s)
{

	if (s)
		sort_free(s);
}

/*
 * Copy content of src binary string to dst,
 * with specified number of symbols to be copied.
 * An offset value can be specified, from the start of src string.
 * If the capacity of the dst string is not sufficient,
 * then the data is truncated.
 */
struct bwstring *
bwsnocpy(struct bwstring *dst, const struct bwstring *src, size_t offset,
    size_t size)
{

	if (offset >= BWSLEN(src)) {
		bws_setlen(dst, 0);
	} else {
		size_t nums = BWSLEN(src) - offset;

		if (nums > BWSLEN(dst))
			nums = BWSLEN(dst);
		if (nums > size)
			nums = size;
		if (mb_cur_max == 1) {
			memcpy(dst->cdata.str, src->cdata.str + offset, nums);
			dst->cdata.len = nums;
			dst->cdata.str[nums] = '\0';
		} else {
			memcpy(dst->wdata.str, src->wdata.str + offset,
			    SIZEOF_WCHAR_STRING(nums));
			dst->wdata.len = nums;
			dst->wdata.str[nums] = L'\0';
		}
	}
	return (dst);
}

/*
 * Write binary string to the file.
 * The output is ended either with '\n' (nl == true)
 * or '\0' (nl == false).
 */
size_t
bwsfwrite(struct bwstring *bws, FILE *f, bool zero_ended)
{

	if (mb_cur_max == 1) {
		size_t len = bws->cdata.len;

		if (!zero_ended) {
			bws->cdata.str[len] = '\n';

			if (fwrite(bws->cdata.str, len + 1, 1, f) < 1)
				err(2, NULL);

			bws->cdata.str[len] = '\0';
		} else if (fwrite(bws->cdata.str, len + 1, 1, f) < 1)
			err(2, NULL);

		return (len + 1);

	} else {
		wchar_t eols;
		size_t printed = 0;

		eols = zero_ended ? btowc('\0') : btowc('\n');

		while (printed < BWSLEN(bws)) {
			const wchar_t *s = bws->wdata.str + printed;

			if (*s == L'\0') {
				int nums;

				nums = fwprintf(f, L"%lc", *s);

				if (nums != 1)
					err(2, NULL);
				++printed;
			} else {
				int nums;

				nums = fwprintf(f, L"%ls", s);

				if (nums < 1)
					err(2, NULL);
				printed += nums;
			}
		}
		fwprintf(f, L"%lc", eols);
		return (printed + 1);
	}
}

int
bwsncmp(const struct bwstring *bws1, const struct bwstring *bws2,
    size_t offset, size_t len)
{
	size_t cmp_len, len1, len2;
	int res;

	len1 = BWSLEN(bws1);
	len2 = BWSLEN(bws2);

	if (len1 <= offset) {
		return ((len2 <= offset) ? 0 : -1);
	} else {
		if (len2 <= offset)
			return (+1);
		else {
			len1 -= offset;
			len2 -= offset;

			cmp_len = len1;

			if (len2 < cmp_len)
				cmp_len = len2;

			if (len < cmp_len)
				cmp_len = len;

			if (mb_cur_max == 1) {
				const char *s1, *s2;

				s1 = bws1->cdata.str + offset;
				s2 = bws2->cdata.str + offset;

				res = memcmp(s1, s2, cmp_len);

			} else {
				const wchar_t *s1, *s2;

				s1 = bws1->wdata.str + offset;
				s2 = bws2->wdata.str + offset;

				res = memcmp(s1, s2, SIZEOF_WCHAR_STRING(cmp_len));
			}
		}
	}

	if (res == 0) {
		if (len1 < cmp_len && len1 < len2)
			res = -1;
		else if (len2 < cmp_len && len2 < len1)
			res = +1;
	}

	return (res);
}

int
bwscmp(const struct bwstring *bws1, const struct bwstring *bws2, size_t offset)
{
	size_t len1, len2, cmp_len;
	int res;

	len1 = BWSLEN(bws1);
	len2 = BWSLEN(bws2);

	len1 -= offset;
	len2 -= offset;

	cmp_len = len1;

	if (len2 < cmp_len)
		cmp_len = len2;

	res = bwsncmp(bws1, bws2, offset, cmp_len);

	if (res == 0) {
		if( len1 < len2)
			res = -1;
		else if (len2 < len1)
			res = +1;
	}

	return (res);
}

int
bws_iterator_cmp(bwstring_iterator iter1, bwstring_iterator iter2, size_t len)
{
	wchar_t c1, c2;
	size_t i;

	for (i = 0; i < len; ++i) {
		c1 = bws_get_iter_value(iter1);
		c2 = bws_get_iter_value(iter2);
		if (c1 != c2)
			return (c1 - c2);
		iter1 = bws_iterator_inc(iter1, 1);
		iter2 = bws_iterator_inc(iter2, 1);
	}

	return (0);
}

int
bwscoll(const struct bwstring *bws1, const struct bwstring *bws2, size_t offset)
{
	size_t len1, len2;

	len1 = BWSLEN(bws1);
	len2 = BWSLEN(bws2);

	if (len1 <= offset)
		return ((len2 <= offset) ? 0 : -1);
	else {
		if (len2 <= offset)
			return (+1);
		else {
			len1 -= offset;
			len2 -= offset;

			if (mb_cur_max == 1) {
				const char *s1, *s2;

				s1 = bws1->cdata.str + offset;
				s2 = bws2->cdata.str + offset;

				if (byte_sort) {
					int res;

					if (len1 > len2) {
						res = memcmp(s1, s2, len2);
						if (!res)
							res = +1;
					} else if (len1 < len2) {
						res = memcmp(s1, s2, len1);
						if (!res)
							res = -1;
					} else
						res = memcmp(s1, s2, len1);

					return (res);

				} else {
					int res;
					size_t i, maxlen;

					i = 0;
					maxlen = len1;

					if (maxlen > len2)
						maxlen = len2;

					while (i < maxlen) {
						/* goto next non-zero part: */
						while ((i < maxlen) &&
						    !s1[i] && !s2[i])
							++i;

						if (i >= maxlen)
							break;

						if (s1[i] == 0) {
							if (s2[i] == 0)
								/* NOTREACHED */
								err(2, "bwscoll error 01");
							else
								return (-1);
						} else if (s2[i] == 0)
							return (+1);

						res = strcoll((const char*)(s1 + i), (const char*)(s2 + i));
						if (res)
							return (res);

						while ((i < maxlen) &&
						    s1[i] && s2[i])
							++i;

						if (i >= maxlen)
							break;

						if (s1[i] == 0) {
							if (s2[i] == 0) {
								++i;
								continue;
							} else
								return (-1);
						} else if (s2[i] == 0)
							return (+1);
						else
							/* NOTREACHED */
							err(2, "bwscoll error 02");
					}

					if (len1 < len2)
						return (-1);
					else if (len1 > len2)
						return (+1);

					return (0);
				}
			} else {
				const wchar_t *s1, *s2;
				size_t i, maxlen;
				int res;

				s1 = bws1->wdata.str + offset;
				s2 = bws2->wdata.str + offset;

				i = 0;
				maxlen = len1;

				if (maxlen > len2)
					maxlen = len2;

				while (i < maxlen) {

					/* goto next non-zero part: */
					while ((i < maxlen) &&
					    !s1[i] && !s2[i])
						++i;

					if (i >= maxlen)
						break;

					if (s1[i] == 0) {
						if (s2[i] == 0)
							/* NOTREACHED */
							err(2, "bwscoll error 1");
						else
							return (-1);
					} else if (s2[i] == 0)
						return (+1);

					res = wide_str_coll(s1 + i, s2 + i);
					if (res)
						return (res);

					while ((i < maxlen) && s1[i] && s2[i])
						++i;

					if (i >= maxlen)
						break;

					if (s1[i] == 0) {
						if (s2[i] == 0) {
							++i;
							continue;
						} else
							return (-1);
					} else if (s2[i] == 0)
						return (+1);
					else
						/* NOTREACHED */
						err(2, "bwscoll error 2");
				}

				if (len1 < len2)
					return (-1);
				else if (len1 > len2)
					return (+1);

				return (0);
			}
		}
	}
}

/*
 * Correction of the system API
 */
double
bwstod(struct bwstring *s0, bool *empty)
{
	double ret;

	if (mb_cur_max == 1) {
		char *end, *s;
		char *ep;

		s = s0->cdata.str;
		end = s + s0->cdata.len;
		ep = NULL;

		while (isblank(*s) && s < end)
			++s;

		if (!isprint(*s)) {
			*empty = true;
			return (0);
		}

		ret = strtod((char*)s, &ep);
		if (ep == s) {
			*empty = true;
			return (0);
		}
	} else {
		wchar_t *end, *ep, *s;

		s = s0->wdata.str;
		end = s + s0->wdata.len;
		ep = NULL;

		while (iswblank(*s) && s < end)
			++s;

		if (!iswprint(*s)) {
			*empty = true;
			return (0);
		}

		ret = wcstod(s, &ep);
		if (ep == s) {
			*empty = true;
			return (0);
		}
	}

	*empty = false;
	return (ret);
}

/*
 * A helper function for monthcoll.  If a line matches
 * a month name, it returns (number of the month - 1),
 * while if there is no match, it just return -1.
 */

int
bws_month_score(const struct bwstring *s0)
{

	if (mb_cur_max == 1) {
		const char *end, *s;

		s = s0->cdata.str;
		end = s + s0->cdata.len;

		while (isblank(*s) && s < end)
			++s;

		for (int i = 11; i >= 0; --i) {
			if (cmonths[i].mon && (s == strstr(s, cmonths[i].mon)))
				return (i);
			if (cmonths[i].ab && (s == strstr(s, cmonths[i].ab)))
				return (i);
			if (cmonths[i].alt && (s == strstr(s, cmonths[i].alt)))
				return (i);
		}

	} else {
		const wchar_t *end, *s;

		s = s0->wdata.str;
		end = s + s0->wdata.len;

		while (iswblank(*s) && s < end)
			++s;

		for (int i = 11; i >= 0; --i) {
			if (wmonths[i].ab && (s == wcsstr(s, wmonths[i].ab)))
				return (i);
			if (wmonths[i].mon && (s == wcsstr(s, wmonths[i].mon)))
				return (i);
			if (wmonths[i].alt && (s == wcsstr(s, wmonths[i].alt)))
				return (i);
		}
	}

	return (-1);
}

/*
 * Rips out leading blanks (-b).
 */
struct bwstring *
ignore_leading_blanks(struct bwstring *str)
{

	if (mb_cur_max == 1) {
		char *dst, *end, *src;

		src = str->cdata.str;
		dst = src;
		end = src + str->cdata.len;

		while (src < end && isblank(*src))
			++src;

		if (src != dst) {
			size_t newlen;

			newlen = BWSLEN(str) - (src - dst);

			while (src < end) {
				*dst = *src;
				++dst;
				++src;
			}
			bws_setlen(str, newlen);
		}
	} else {
		wchar_t *dst, *end, *src;

		src = str->wdata.str;
		dst = src;
		end = src + str->wdata.len;

		while (src < end && iswblank(*src))
			++src;

		if (src != dst) {

			size_t newlen = BWSLEN(str) - (src - dst);

			while (src < end) {
				*dst = *src;
				++dst;
				++src;
			}
			bws_setlen(str, newlen);

		}
	}
	return (str);
}

/*
 * Rips out nonprinting characters (-i).
 */
struct bwstring *
ignore_nonprinting(struct bwstring *str)
{
	size_t newlen = BWSLEN(str);

	if (mb_cur_max == 1) {
		char *dst, *end, *src;
		char c;

		src = str->cdata.str;
		dst = src;
		end = src + str->cdata.len;

		while (src < end) {
			c = *src;
			if (isprint(c)) {
				*dst = c;
				++dst;
				++src;
			} else {
				++src;
				--newlen;
			}
		}
	} else {
		wchar_t *dst, *end, *src;
		wchar_t c;

		src = str->wdata.str;
		dst = src;
		end = src + str->wdata.len;

		while (src < end) {
			c = *src;
			if (iswprint(c)) {
				*dst = c;
				++dst;
				++src;
			} else {
				++src;
				--newlen;
			}
		}
	}
	bws_setlen(str, newlen);

	return (str);
}

/*
 * Rips out any characters that are not alphanumeric characters
 * nor blanks (-d).
 */
struct bwstring *
dictionary_order(struct bwstring *str)
{
	size_t newlen = BWSLEN(str);

	if (mb_cur_max == 1) {
		char *dst, *end, *src;
		char c;

		src = str->cdata.str;
		dst = src;
		end = src + str->cdata.len;

		while (src < end) {
			c = *src;
			if (isalnum(c) || isblank(c)) {
				*dst = c;
				++dst;
				++src;
			} else {
				++src;
				--newlen;
			}
		}
	} else {
		wchar_t *dst, *end, *src;
		wchar_t c;

		src = str->wdata.str;
		dst = src;
		end = src + str->wdata.len;

		while (src < end) {
			c = *src;
			if (iswalnum(c) || iswblank(c)) {
				*dst = c;
				++dst;
				++src;
			} else {
				++src;
				--newlen;
			}
		}
	}
	bws_setlen(str, newlen);

	return (str);
}

/*
 * Converts string to lower case(-f).
 */
struct bwstring *
ignore_case(struct bwstring *str)
{

	if (mb_cur_max == 1) {
		char *end, *s;

		s = str->cdata.str;
		end = s + str->cdata.len;

		while (s < end) {
			*s = toupper(*s);
			++s;
		}
	} else {
		wchar_t *end, *s;

		s = str->wdata.str;
		end = s + str->wdata.len;

		while (s < end) {
			*s = towupper(*s);
			++s;
		}
	}
	return (str);
}

void
bws_disorder_warnx(struct bwstring *s, const char *fn, size_t pos)
{

	if (mb_cur_max == 1)
		warnx("%s:%zu: disorder: %s", fn, pos + 1, s->cdata.str);
	else
		warnx("%s:%zu: disorder: %ls", fn, pos + 1, s->wdata.str);
}
