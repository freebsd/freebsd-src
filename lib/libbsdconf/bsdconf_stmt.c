/*
 * Copyright (c) 2015-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Statement scanning, value rendering, and related helpers used by
 * bsdconf_put(). Kept separate so bsdconf_put.c stays focused on the
 * rewrite driver.
 */

#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "bsdconf.h"
#include "bsdconf_internal.h"

/*
 * Map an assignment operator to its config file token. BSDCONF_OP_DEFAULT
 * maps to the plain `=' token.
 */
const char *
bsdconf_op_token(enum bsdconf_op op)
{
	switch (op) {
	case BSDCONF_OP_APPEND:
		return ("+=");
	case BSDCONF_OP_COND:
		return ("?=");
	case BSDCONF_OP_EXPAND:
		return (":=");
	case BSDCONF_OP_SHELL:
		return ("!=");
	case BSDCONF_OP_DEFAULT:
	case BSDCONF_OP_ASSIGN:
	default:
		return ("=");
	}
}

/*
 * Write exactly `len' bytes from `data' to `fd', restarting short and
 * interrupted writes. Returns zero on success; -1 (with errno set) on error.
 * Shared with bsdconf_spool() (see bsdconf_internal.h).
 */
int
bsdconf_writeall(int fd, const void *data, size_t len)
{
	const char *p = data;
	ssize_t w;

	while (len > 0) {
		w = write(fd, p, len);
		if (w < 0) {
			if (errno == EINTR)
				continue;
			return (-1);
		}
		if (w == 0) {
			errno = EIO;
			return (-1);
		}
		p += w;
		len -= (size_t)w;
	}
	return (0);
}

/*
 * Read the entire contents of the open descriptor `fd' (whose size is `size')
 * into a freshly allocated, NUL-terminated buffer. The actual number of bytes
 * read is stored through `lenp'. Returns the buffer on success (which the
 * caller must free) or NULL (with errno set) on error.
 */
char *
bsdconf_readfile(int fd, size_t size, size_t *lenp)
{
	char *buf;
	size_t off = 0;
	ssize_t r;

	if ((buf = malloc(size + 1)) == NULL)
		return (NULL);

	while (off < size) {
		r = read(fd, buf + off, size - off);
		if (r < 0) {
			if (errno == EINTR)
				continue;
			free(buf);
			return (NULL);
		}
		if (r == 0) /* premature EOF (file shrank); stop */
			break;
		off += (size_t)r;
	}

	buf[off] = '\0';
	*lenp = off;
	return (buf);
}

/*
 * Write `len' bytes to `fd' and, when any bytes are written, remember the
 * last one through `last' (as an unsigned char, or left untouched for a
 * zero-length write). This lets the caller track whether the output so far
 * ends in a newline without having to re-read the descriptor. Returns zero
 * on success; -1 (with errno set) on error.
 */
int
bsdconf_emit(int fd, const void *data, size_t len, int *last)
{

	if (len == 0)
		return (0);
	if (bsdconf_writeall(fd, data, len) != 0)
		return (-1);
	*last = (unsigned char)((const char *)data)[len - 1];
	return (0);
}

/*
 * Lazily create the replacement temporary in the target's directory. Called
 * on the first edit so that CHECK-only and equal-value SET_VALUE paths never
 * open a writable descriptor (and never bump mtime or sever hard links).
 */
int
bsdconf_ensure_tmp(int *tmpfdp, char *tpath, size_t tpathsz,
    const char *rpath, const struct stat *sb)
{
	char *slash;

	if (*tmpfdp >= 0)
		return (0);

	if ((slash = strrchr(rpath, '/')) != NULL) {
		if (snprintf(tpath, tpathsz, "%.*s/.bsdconf.XXXXXXXXXX",
		    (int)(slash - rpath), rpath) >= (int)tpathsz) {
			errno = ENAMETOOLONG;
			return (-1);
		}
	} else if (snprintf(tpath, tpathsz, ".bsdconf.XXXXXXXXXX") >=
	    (int)tpathsz) {
		errno = ENAMETOOLONG;
		return (-1);
	}
	if ((*tmpfdp = mkstemp(tpath)) == -1) {
		tpath[0] = '\0';
		return (-1);
	}
	if (fchmod(*tmpfdp, sb->st_mode & 0666) != 0)
		return (-1);
	(void)fchown(*tmpfdp, sb->st_uid, sb->st_gid); /* best effort */
	return (0);
}

/*
 * Determine whether the string value `s' must be double-quoted on output so
 * that it round-trips through the parser unchanged. Empty strings are quoted
 * so that they remain distinguishable from a value-less directive.
 */
static int
bsdconf_needs_quote(const char *s, bool bsemicolon)
{
	const char *p;

	if (*s == '\0')
		return (1);
	for (p = s; *p != '\0'; p++) {
		if (isspace((unsigned char)*p) || *p == '#' || *p == '"' ||
		    *p == '\\')
			return (1);
		if (bsemicolon && *p == ';')
			return (1);
	}
	return (0);
}

/*
 * Determine whether the string value `s' round-trips through the parser
 * when written verbatim (no quoting available; make.conf(5) semantics).
 * Embedded whitespace is fine; what is not is an embedded newline, a `#'
 * (or, with `bsemicolon', a `;') at an unquoted position -- either would
 * terminate the value early on re-parse -- or a trailing unescaped
 * backslash, which would escape the statement's own newline.
 *
 * After counting a run of backslashes, `p' already points at the following
 * byte. An odd-length run escapes that byte (skip it); an even-length run
 * leaves it unescaped, so it must be re-examined by the loop body below
 * (do not `continue', which would advance past it a second time).
 */
static int
bsdconf_verbatim_ok(const char *s, bool bsemicolon)
{
	const char *p;
	int quote = 0;
	size_t nbs;

	for (p = s; *p != '\0'; p++) {
		if (*p == '\\') {
			for (nbs = 0; *p == '\\'; p++)
				nbs++;
			if (*p == '\0')
				return ((nbs & 1) == 0);
			if ((nbs & 1) != 0) {
				/* Odd run: the following byte is escaped */
				if (*p == '\n')
					return (0);
				continue;
			}
			/* Even run: *p is unescaped; fall through */
		}
		if (*p == '\n')
			return (0);
		if (*p == '"')
			quote = !quote;
		else if (!quote &&
		    (*p == '#' || (bsemicolon && *p == ';')))
			return (0);
	}
	return (1);
}

/*
 * Produce the textual value to be written for `option', following the value
 * model documented in bsdconf.h: the value is always taken from value.str and
 * the type governs only whether quoting/escaping is applied.
 *
 * With `quote_always' (loader.conf(5) semantics) every value is enclosed in
 * double-quotes with embedded quotes and backslashes escaped. With `unquoted'
 * (make.conf(5) semantics / BSDCONF_PUT_UNQUOTED) `value.str' is emitted as
 * literal file text -- no quotes are added and no characters are
 * backslash-escaped -- so the caller supplies the bytes that should appear
 * on disk. Embedded whitespace is fine (the value runs to the end of the
 * line); a value that could not round-trip under the parser (embedded
 * newline, unescaped comment marker, or trailing unescaped backslash) is
 * rejected (errno = EINVAL) rather than silently corrupting the file. Note
 * the asymmetry with the reader, which still runs bsdconf_strunexpand() on
 * input: escape sequences present in the file become the logical value on
 * read, but this path does not re-encode them. Otherwise (sysctl.conf(5)
 * and generic semantics), values are quoted only when required.
 *
 * Returns a newly allocated NUL-terminated string (which the caller must
 * free) or NULL (with errno set) on failure.
 */
char *
bsdconf_format_value(const struct bsdconf_option *option, bool unquoted,
    bool quote_always, bool bsemicolon)
{
	const char *s;
	char *out;
	char *q;
	size_t extra;
	size_t i;
	size_t slen;

	if (option->type == BSDCONF_TYPE_NONE)
		return (strdup(""));

	s = option->value.str != NULL ? option->value.str : "";

	if (unquoted) {
		if (!bsdconf_verbatim_ok(s, bsemicolon)) {
			errno = EINVAL;
			return (NULL);
		}
		return (strdup(s));
	}

	if (!quote_always) {
		if (option->type == BSDCONF_TYPE_INT ||
		    option->type == BSDCONF_TYPE_UINT ||
		    option->type == BSDCONF_TYPE_INT64 ||
		    option->type == BSDCONF_TYPE_UINT64 ||
		    option->type == BSDCONF_TYPE_BOOL)
			return (strdup(s));
		if (!bsdconf_needs_quote(s, bsemicolon))
			return (strdup(s));
	}

	/* Count the characters that require a backslash escape */
	slen = strlen(s);
	extra = 0;
	for (i = 0; i < slen; i++)
		if (s[i] == '"' || s[i] == '\\')
			extra++;

	/* Enclosing quotes plus escapes plus terminator */
	if ((out = malloc(slen + extra + 3)) == NULL)
		return (NULL);
	q = out;
	*q++ = '"';
	for (i = 0; i < slen; i++) {
		if (s[i] == '"' || s[i] == '\\')
			*q++ = '\\';
		*q++ = s[i];
	}
	*q++ = '"';
	*q = '\0';
	return (out);
}

/*
 * Test whether a SET_VALUE/CHECK value is considered "empty" for the
 * purposes of BSDCONF_PUT_ALLOW_EMPTY.
 */
int
bsdconf_value_empty(const struct bsdconf_option *option)
{

	return (option->type != BSDCONF_TYPE_NONE &&
	    (option->value.str == NULL || option->value.str[0] == '\0'));
}

/*
 * Match the directive spanning buf[start, end) against the directive of a
 * bsdconf_option. Comparison is exact (whole-token); unlike bsdconf_parse(),
 * bsdconf_put() does not glob-match, since a pattern is not a writable
 * target.
 */
int
bsdconf_dir_matches(const char *buf, size_t start, size_t end,
    const char *directive, bool case_sensitive)
{
	size_t len = end - start;

	if (strlen(directive) != len)
		return (0);
	if (case_sensitive)
		return (memcmp(buf + start, directive, len) == 0);
	return (strncasecmp(buf + start, directive, len) == 0);
}

/*
 * Tokenize the statement beginning at buf[*ip], advancing *ip past it and
 * filling in `st'. The scan mirrors bsdconf_fparse() exactly (comment,
 * quote, operator, and backslash-escape handling included) so that a file
 * written by bsdconf_put() re-parses identically. Returns 1 if a statement
 * was found, or 0 at end of input.
 */
int
bsdconf_scan(const char *buf, size_t len, size_t *ip, uint32_t *linep,
    bool bequals, bool bsemicolon, bool strict_equals, bool operator_equals,
    struct bsdconf_stmt *st)
{
	size_t i = *ip;
	size_t j;
	uint32_t line = *linep;
	bool comment = false;
	bool novalue = false;
	bool quote = false;

	/* Skip whitespace and comments to the beginning of a directive */
	st->line_start = i;
	while (i < len && (isspace((unsigned char)buf[i]) || buf[i] == '#' ||
	    comment || (bsemicolon && buf[i] == ';'))) {
		if (buf[i] == '#')
			comment = true;
		else if (buf[i] == '\n') {
			comment = false;
			line++;
			st->line_start = i + 1;
		}
		i++;
	}
	if (i >= len) {
		*ip = i;
		*linep = line;
		return (0);
	}

	st->line = line;
	st->dir_start = i;
	st->have_equals = false;
	st->op = BSDCONF_OP_DEFAULT;
	st->eq = 0;

	/* Find the end of the directive */
	while (i < len) {
		if (isspace((unsigned char)buf[i]))
			break;
		if (bequals && buf[i] == '=') {
			st->have_equals = true;
			break;
		}
		if (bsemicolon && buf[i] == ';')
			break;
		i++;
	}
	st->dir_end = i;
	st->op_start = i;

	/*
	 * Split a make(1)-style operator (`+=' `?=' `:=' `!=') off the tail
	 * of the directive if requested; the operator character rode along
	 * with the directive because only the `=' terminates the scan.
	 */
	if (st->have_equals) {
		st->eq = i;
		st->op = BSDCONF_OP_ASSIGN;
		if (operator_equals && st->dir_end - st->dir_start > 1) {
			switch (buf[st->dir_end - 1]) {
			case '+': st->op = BSDCONF_OP_APPEND; break;
			case '?': st->op = BSDCONF_OP_COND; break;
			case ':': st->op = BSDCONF_OP_EXPAND; break;
			case '!': st->op = BSDCONF_OP_SHELL; break;
			}
			if (st->op != BSDCONF_OP_ASSIGN) {
				st->dir_end--;
				st->op_start = st->dir_end;
			}
		}
	}

	/* Step over an `=' acting as the directive terminator */
	if (bequals && i < len && buf[i] == '=') {
		i++;
		if (strict_equals && i < len &&
		    isspace((unsigned char)buf[i]) && buf[i] != '\n')
			novalue = true; /* strict `=' then space: no value */
	}

	/* Advance across separating whitespace to the value */
	if (!novalue && !(bsemicolon && i < len && buf[i] == ';') &&
	    !(strict_equals && i < len && buf[i] == '=')) {
		while (i < len && isspace((unsigned char)buf[i]) &&
		    buf[i] != '\n')
			i++;
	}

	/* Consume an `=' surrounded by whitespace (non-strict) */
	if (!novalue && i < len && bequals && buf[i] == '=' &&
	    !strict_equals) {
		st->have_equals = true;
		st->eq = i;
		if (st->op == BSDCONF_OP_DEFAULT)
			st->op = BSDCONF_OP_ASSIGN;
		i++;
		while (i < len && isspace((unsigned char)buf[i]) &&
		    buf[i] != '\n')
			i++;
	}

	/* A directive with no value */
	if (novalue || i >= len || buf[i] == '\n' || buf[i] == '#' ||
	    (bsemicolon && buf[i] == ';')) {
		st->have_value = false;
		st->val_start = st->val_end = i;
	} else {
		st->have_value = true;
		st->val_start = i;
		quote = false;
		while (i < len) {
			char c = buf[i];

			if (c != '"' && c != '#' && c != '\n' &&
			    (!bsemicolon || c != ';')) {
				i++;
				continue;
			}

			/* Count the backslashes immediately preceding `c' */
			j = i;
			while (j > st->val_start && buf[j - 1] == '\\')
				j--;

			if (((i - j) & 1) == 0) { /* not escaped */
				if (c == '"') {
					quote = !quote;
					i++;
					continue;
				}
				if (c == '#') {
					if (!quote)
						break;
					i++;
					continue;
				}
				if (c == '\n')
					break;
				if (c == ';') {
					if (!quote && bsemicolon)
						break;
					i++;
					continue;
				}
			} else { /* escaped: part of the value */
				if (c == '\n')
					line++;
				i++;
				continue;
			}
		}
		/* Trim trailing whitespace from the value */
		st->val_end = i;
		while (st->val_end > st->val_start &&
		    isspace((unsigned char)buf[st->val_end - 1]))
			st->val_end--;
	}

	/* Record the terminator position and the end of the physical line */
	st->term = i;
	st->line_end = i;
	while (st->line_end < len && buf[st->line_end] != '\n')
		st->line_end++;
	if (st->line_end < len)
		st->line_end++;

	*ip = i;
	*linep = line;
	return (1);
}

/*
 * Search for a config option (struct bsdconf_option) in the array of config
 * options and stage the value of the struct whose directive matches the
 * given parameter. On success, returns 1, otherwise 0.
 */
int
bsdconf_set_option(struct bsdconf_option options[], const char *directive,
    union bsdconf_value *value)
{
	uint32_t n;

	/* Check arguments */
	if (options == NULL || directive == NULL || value == NULL)
		return (0);

	/* Loop through the array, staging the first match */
	for (n = 0; options[n].directive != NULL; n++) {
		if (strcmp(options[n].directive, directive) == 0) {
			options[n].value = *value;
			return (1);
		}
	}

	return (0);
}
