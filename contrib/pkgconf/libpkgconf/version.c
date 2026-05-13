/*
 * version.c
 * version comparison
 *
 * Copyright (c) 2011-2026 pkgconf authors (see AUTHORS).
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <libpkgconf/config.h>
#include <libpkgconf/stdinc.h>
#include <libpkgconf/libpkgconf.h>

typedef enum {
	PKGCONF_VERSION_TOKEN_END = 0,
	PKGCONF_VERSION_TOKEN_TILDE,
	PKGCONF_VERSION_TOKEN_NUMERIC,
	PKGCONF_VERSION_TOKEN_ALPHA,
	PKGCONF_VERSION_TOKEN_OTHER
} pkgconf_version_token_kind_t;

typedef struct {
	pkgconf_version_token_kind_t kind;
	const char *start;
	const char *end;
} pkgconf_version_token_t;

typedef struct {
	const char *cur;
} pkgconf_version_iter_t;

static inline bool
pkgconf_version_is_separator(unsigned char ch)
{
	return !isalnum(ch) && ch != '~';
}

static const char *
pkgconf_version_skip_separators(const char *s)
{
	while (*s && pkgconf_version_is_separator((unsigned char)*s))
		s++;

	return s;
}

static pkgconf_version_token_t
pkgconf_version_next_token(pkgconf_version_iter_t *it)
{
	pkgconf_version_token_t tok;
	const char *s = pkgconf_version_skip_separators(it->cur);

	tok.start = s;
	tok.end = s;
	tok.kind = PKGCONF_VERSION_TOKEN_END;

	if (*s == '\0')
	{
		it->cur = s;
		return tok;
	}

	if (*s == '~')
	{
		tok.kind = PKGCONF_VERSION_TOKEN_TILDE;
		tok.end = s + 1;
		it->cur = tok.end;
		return tok;
	}

	if (isdigit((unsigned char)*s))
	{
		tok.kind = PKGCONF_VERSION_TOKEN_NUMERIC;
		while (*tok.end && isdigit((unsigned char)*tok.end))
			tok.end++;
		it->cur = tok.end;
		return tok;
	}

	if (isalpha((unsigned char)*s))
	{
		tok.kind = PKGCONF_VERSION_TOKEN_ALPHA;
		while (*tok.end && isalpha((unsigned char)*tok.end))
			tok.end++;
		it->cur = tok.end;
		return tok;
	}

	tok.kind = PKGCONF_VERSION_TOKEN_OTHER;
	tok.end = s + 1;
	it->cur = tok.end;

	return tok;
}

static int
pkgconf_version_compare_numeric(const pkgconf_version_token_t *a, const pkgconf_version_token_t *b)
{
	const char *ap = a->start;
	const char *bp = b->start;
	size_t alen, blen;
	int ret;

	while (ap < a->end && *ap == '0')
		ap++;

	while (bp < b->end && *bp == '0')
		bp++;

	alen = (size_t)(a->end - ap);
	blen = (size_t)(b->end - bp);

	if (alen > blen)
		return 1;
	if (alen < blen)
		return -1;

	if (alen == 0)
		return 0;

	ret = strncmp(ap, bp, alen);
	if (ret < 0)
		return -1;
	if (ret > 0)
		return 1;

	return 0;
}

static int
pkgconf_version_compare_alpha(const pkgconf_version_token_t *a, const pkgconf_version_token_t *b)
{
	size_t alen = (size_t)(a->end - a->start);
	size_t blen = (size_t)(b->end - b->start);
	size_t len = alen < blen ? alen : blen;
	int ret;

	ret = strncmp(a->start, b->start, len);
	if (ret < 0)
		return -1;
	if (ret > 0)
		return 1;

	if (alen < blen)
		return -1;
	if (alen > blen)
		return 1;

	return 0;
}

static int
pkgconf_version_compare_token(const pkgconf_version_token_t *a, const pkgconf_version_token_t *b)
{
	if (a->kind == PKGCONF_VERSION_TOKEN_TILDE || b->kind == PKGCONF_VERSION_TOKEN_TILDE)
        {
		if (a->kind != PKGCONF_VERSION_TOKEN_TILDE)
			return 1;
		if (b->kind != PKGCONF_VERSION_TOKEN_TILDE)
			return -1;

		return 0;
	}

	if (a->kind == PKGCONF_VERSION_TOKEN_END || b->kind == PKGCONF_VERSION_TOKEN_END)
	{
		if (a->kind == PKGCONF_VERSION_TOKEN_END && b->kind == PKGCONF_VERSION_TOKEN_END)
			return 0;
		if (a->kind == PKGCONF_VERSION_TOKEN_END)
			return -1;

		return 1;
	}

	/* left-side is numeric, beats any right-side non-numeric */
	if (a->kind == PKGCONF_VERSION_TOKEN_NUMERIC)
	{
		if (b->kind != PKGCONF_VERSION_TOKEN_NUMERIC)
			return 1;

		return pkgconf_version_compare_numeric(a, b);
	}

	/* left-side is alpha, any right-side non-alpha wins */
	if (a->kind == PKGCONF_VERSION_TOKEN_ALPHA)
	{
		if (b->kind != PKGCONF_VERSION_TOKEN_ALPHA)
			return -1;

		return pkgconf_version_compare_alpha(a, b);
	}

	if (a->kind < b->kind)
		return -1;
	if (a->kind > b->kind)
		return 1;

	return 0;
}

/*
 * !doc
 *
 * .. c:function:: int pkgconf_compare_version(const char *a, const char *b)
 *
 *    Compare versions using RPM version comparison rules as described in the LSB.
 *
 *    :param char* a: The first version to compare in the pair.
 *    :param char* b: The second version to compare in the pair.
 *    :return: -1 if the first version is less than, 0 if both versions are equal, 1 if the second version is less than.
 *    :rtype: int
 */
int
pkgconf_compare_version(const char *a, const char *b)
{
        pkgconf_version_iter_t ia, ib;

        if (a == NULL)
                return -1;
        if (b == NULL)
                return 1;

        if (!strcasecmp(a, b))
                return 0;

        ia.cur = a;
        ib.cur = b;

        for (;;)
        {
                pkgconf_version_token_t ta = pkgconf_version_next_token(&ia);
                pkgconf_version_token_t tb = pkgconf_version_next_token(&ib);
                int ret = pkgconf_version_compare_token(&ta, &tb);

                if (ret != 0)
                        return ret;

                if (ta.kind == PKGCONF_VERSION_TOKEN_END &&
                    tb.kind == PKGCONF_VERSION_TOKEN_END)
                        return 0;
        }
}
