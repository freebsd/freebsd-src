/*
 * Copyright (c) 2013-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Lexer for UCL text. It finds spans; it does not decide what a file
 * means. libucl does that, after the edit.
 */


#include <ctype.h>
#include <stdckdint.h>

#include <ucl.h>

#include "sysconf_priv.h"
#include "sysconf_ucl.h"

/* Block comments nest, as libucl reads them */
static size_t
skip_comment(const char *s, size_t n, size_t i)
{
	int depth;

	if (s[i] == '#') {
		while (i < n && s[i] != '\n')
			i++;
		return (i);
	}
	if (s[i] == '/' && i + 1 < n && s[i + 1] == '*') {
		depth = 1;
		i += 2;
		while (i + 1 < n) {
			if (s[i] == '*' && s[i + 1] == '/') {
				i += 2;
				if (--depth == 0)
					return (i);
			} else if (s[i] == '/' && s[i + 1] == '*') {
				depth++;
				i += 2;
			} else
				i++;
		}
		return (n);
	}
	return (i);
}

bool
comment_at(const char *s, size_t n, size_t i)
{

	return (s[i] == '#' ||
	    (s[i] == '/' && i + 1 < n && s[i + 1] == '*'));
}

/*
 * Skip blanks, optionally newlines and statement separators, and
 * comments.
 */
size_t
skip_ws(const char *s, size_t n, size_t i, bool newlines, bool seps)
{
	size_t j;

	for (;;) {
		while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' ||
		    (newlines && s[i] == '\n') ||
		    (seps && (s[i] == ',' || s[i] == ';'))))
			i++;
		if (i >= n || !comment_at(s, n, i))
			return (i);
		j = skip_comment(s, n, i);
		if (j == i)
			return (i);
		i = j;
	}
}

size_t
skip_quoted(const char *s, size_t n, size_t i)
{
	char q = s[i++];

	while (i < n && s[i] != q) {
		if (s[i] == '\\' && i + 1 < n)
			i++;
		i++;
	}
	return (i < n ? i + 1 : n);
}

/* Skip a balanced `{ ... }' or `[ ... ]', strings and comments aware */
size_t
skip_balanced(const char *s, size_t n, size_t i)
{
	int depth = 0;

	for (; i < n; i++) {
		if (s[i] == '"' || s[i] == '\'') {
			i = skip_quoted(s, n, i) - 1;
			continue;
		}
		if (comment_at(s, n, i)) {
			i = skip_comment(s, n, i) - 1;
			continue;
		}
		if (s[i] == '{' || s[i] == '[')
			depth++;
		else if (s[i] == '}' || s[i] == ']') {
			if (--depth == 0)
				return (i + 1);
		}
	}
	return (n);
}

bool
key_char(char c)
{

	return (!(c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
	    c == ':' || c == '=' || c == '{' || c == ';' ||
	    c == ',' || c == '}' || c == '#' || c == '"'));
}

size_t
line_begin(const char *s, size_t i)
{

	while (i > 0 && s[i - 1] != '\n')
		i--;
	return (i);
}

bool
blank_span(const char *s, size_t from, size_t to)
{

	for (; from < to; from++)
		if (s[from] != ' ' && s[from] != '\t' && s[from] != '\r')
			return (false);
	return (true);
}

/*
 * True when `s[from,to)' is blank or a comment that rides out the line:
 * a statement or element followed only by that owns its line, and a
 * removal takes the comment with it, as sysrc(8) does.
 */
bool
tail_span(const char *s, size_t n, size_t from, size_t to)
{

	while (from < to && (s[from] == ' ' || s[from] == '\t' ||
	    s[from] == '\r'))
		from++;
	if (from >= to)
		return (true);
	if (!comment_at(s, n, from))
		return (false);
	/* A block comment running past the line is not ours to take */
	from = skip_comment(s, n, from);
	return (from <= to && blank_span(s, from, to));
}

/*
 * Parse the statement starting at `i' (its key). Returns zero and
 * fills `st', or -1 when the text there is not a statement.
 */
int
parse_stmt(const char *s, size_t n, size_t i, struct ustmt *st)
{
	size_t j;
	size_t k;

	memset(st, 0, sizeof(*st));
	st->start = i;
	if (s[i] == '"')
		j = skip_quoted(s, n, i);
	else {
		j = i;
		while (j < n && key_char(s[j]))
			j++;
	}
	if (j == i)
		return (-1);
	j = skip_ws(s, n, j, false, false);
	if (j < n && (s[j] == ':' || s[j] == '=')) {
		st->sep = s[j];
		j = skip_ws(s, n, j + 1, true, false);
	}
	st->val_start = j;
	if (j >= n) {
		st->val_end = j;
		st->end = j;
		goto lines;
	}
	switch (s[j]) {
	case '{':
		st->is_obj = true;
		st->obj_open = j;
		k = skip_balanced(s, n, j);
		break;
	case '[':
		st->is_arr = true;
		k = skip_balanced(s, n, j);
		break;
	case '"':
	case '\'':
		st->quoted = true;
		k = skip_quoted(s, n, j);
		break;
	case '<':
		if (j + 2 < n && s[j + 1] == '<' && isupper((unsigned char)
		    s[j + 2])) {
			size_t w = j + 2;
			size_t wl;
			const char *nl;

			while (w < n && isupper((unsigned char)s[w]))
				w++;
			wl = w - (j + 2);
			k = n;
			for (nl = s + w; (nl = memchr(nl, '\n',
			    (size_t)(s + n - nl))) != NULL; nl++) {
				if ((size_t)(s + n - (nl + 1)) >= wl &&
				    memcmp(nl + 1, s + j + 2, wl) == 0 &&
				    (nl + 1 + wl == s + n ||
				    nl[1 + wl] == '\n' || nl[1 + wl] == '\r')) {
					k = (size_t)(nl + 1 + wl - s);
					break;
				}
			}
			break;
		}
		/* FALLTHROUGH */
	default:
		k = j;
		while (k < n && s[k] != '\n' && s[k] != ';' && s[k] != ',' &&
		    s[k] != '}' && !comment_at(s, n, k))
			k++;
		while (k > j && (s[k - 1] == ' ' || s[k - 1] == '\t' ||
		    s[k - 1] == '\r'))
			k--;
		break;
	}
	st->val_end = k;
	k = skip_ws(s, n, k, false, false);
	if (k < n && (s[k] == ',' || s[k] == ';')) {
		st->term = s[k];
		st->end = k + 1;
	} else
		st->end = st->val_end;
lines:
	st->line_start = line_begin(s, st->start);
	for (k = st->end; k < n && s[k] != '\n'; k++)
		continue;
	st->line_end = (k < n) ? k + 1 : k;
	st->alone = blank_span(s, st->line_start, st->start) &&
	    tail_span(s, n, st->end, (k < n) ? k : n);
	return (0);
}

/*
 * Enumerate the statements of the container whose interior starts at
 * `i', recording its shape in `obj' and the first statement whose key is
 * `want' in `match'. Returns 1 when `want' was found, 0 if not, -1 on a
 * lexing failure.
 */
int
enum_object(const char *s, size_t n, size_t i, const char *want,
    struct ustmt *match, struct uobj *obj)
{
	struct ustmt st;
	size_t j;
	size_t klen;
	size_t kstart;
	int found = 0;
	int nl;

	obj->nstmts = 0;
	obj->has_scalar = false;
	obj->blank_between = false;
	for (;;) {
		j = skip_ws(s, n, i, true, true);
		if (j >= n || s[j] == '}') {
			obj->close = j;
			return (found);
		}
		if (parse_stmt(s, n, j, &st) != 0)
			return (-1);
		if (obj->nstmts == 0)
			obj->first = st;
		else if (obj->nstmts == 1) {
			nl = 0;
			for (i = obj->first.end; i < st.start; i++)
				if (s[i] == '\n')
					nl++;
			obj->blank_between = (nl >= 2);
		}
		if (obj->nstmts > 0)
			obj->prev = obj->last;
		obj->last = st;
		obj->nstmts++;
		/* `key [' and `key {' are idioms; scalars set the style */
		if (!st.is_obj && !st.is_arr && !obj->has_scalar) {
			obj->scalar = st;
			obj->has_scalar = true;
		}
		if (s[st.start] == '"') {
			kstart = st.start + 1;
			/* A lone `"' at the end has no closing quote */
			if (ckd_sub(&klen, skip_quoted(s, n, st.start) -
			    st.start, 2))
				return (-1);
		} else {
			kstart = st.start;
			klen = 0;
			while (kstart + klen < n && key_char(s[kstart + klen]))
				klen++;
		}
		/*
		 * A key repeated in one file is an implicit array to
		 * libucl, which presents its first element; edit that one.
		 */
		if (!found && strlen(want) == klen &&
		    strncmp(s + kstart, want, klen) == 0) {
			*match = st;
			found = 1;
		}
		i = st.end;
	}
}

