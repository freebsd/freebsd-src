/*
 * Copyright (c) 2001-2026 Devin Teske <dteske@FreeBSD.org>
 * Copyright (c) 2021-2026 Faraz Vahedi <kfv@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "bsdconf_internal.h"

/*
 * Counts the number of occurrences of one string that appear in the source
 * string. Return value is the total count.
 *
 * An example use would be if you need to know how large a buffer needs to be
 * for a bsdconf_replaceall() series.
 */
unsigned int
bsdconf_strcount(const char *source, const char *find)
{
	const char *p;
	size_t flen;
	unsigned int n = 0;

	if (source == NULL || find == NULL || *source == '\0' || *find == '\0')
		return (0);

	flen = strlen(find);
	for (p = source; (p = strstr(p, find)) != NULL; p += flen)
		n++;

	return (n);
}

/*
 * Replaces all occurrences of `find' in `buf' with `replace'.
 *
 * `buf' must point to a mutable buffer of at least `buflen' bytes (including
 * space for the terminating NUL). A string constant will not compile as the
 * first argument without a cast; do not cast one in. A local or global
 * non-const array is fine.
 *
 * The result is always built in a temporary buffer and copied back, so the
 * same path is taken whether `replace' is longer or shorter than `find'.
 * Pass a `buflen' large enough for the expanded result (bsdconf_strcount()
 * can size it); if the result would not fit, -1 is returned with errno set
 * to ENOSPC and `buf' is left unmodified. On success the return value is
 * the length (in bytes) of the result, not counting the terminating NUL.
 *
 * When an error occurs, -1 is returned and the global variable errno is set
 * accordingly.
 */
int
bsdconf_replaceall(char *buf, size_t buflen, const char *find,
    const char *replace)
{
	char *dst;
	char *out;
	const char *hit;
	const char *src;
	size_t flen, need, rlen, slen;
	unsigned int n;

	if (buf == NULL)
		return (0);
	if (find == NULL)
		return ((int)strlen(buf));

	slen = strlen(buf);
	flen = strlen(find);
	rlen = replace != NULL ? strlen(replace) : 0;

	if (slen == 0 || flen == 0 || slen < flen)
		return ((int)slen);

	n = bsdconf_strcount(buf, find);
	if (n == 0)
		return ((int)slen);

	if (rlen >= flen)
		need = slen + (size_t)n * (rlen - flen) + 1;
	else
		need = slen - (size_t)n * (flen - rlen) + 1;
	if (need > buflen) {
		errno = ENOSPC;
		return (-1);
	}

	if ((out = malloc(need)) == NULL)
		return (-1);

	dst = out;
	src = buf;
	while ((hit = strstr(src, find)) != NULL) {
		memcpy(dst, src, (size_t)(hit - src));
		dst += hit - src;
		if (rlen > 0) {
			memcpy(dst, replace, rlen);
			dst += rlen;
		}
		src = hit + flen;
	}
	memcpy(dst, src, strlen(src) + 1);
	memcpy(buf, out, need);
	free(out);

	return ((int)(need - 1));
}

/*
 * Unexpands (collapses) C-style escape sequences in `src' into `dst'.
 *
 * The result is never longer than the input, so `dst' may be the same buffer
 * as `src' (cf. strunvis(3)). Do not pass a string constant as `dst'; a local
 * or global non-const array is fine.
 *
 * Interpreted sequences are:
 *
 * 	\NNN	character with octal value NNN (1 to 3 digits)
 * 	\a	alert (BEL)
 * 	\b	backspace
 * 	\f	form feed
 * 	\n	new line
 * 	\r	carriage return
 * 	\t	horizontal tab
 * 	\v	vertical tab
 * 	\xNN	byte with hexadecimal value NN (1 to 2 digits)
 *
 * All other sequences are unescaped (ie. '\"' and '\#'). A trailing backslash
 * or a `\x' with no following hex digits is emitted verbatim (the backslash
 * is dropped for `\x', leaving `x').
 */
void
bsdconf_strunexpand(char *dst, const char *src)
{
	char *d;
	const char *s;
	unsigned int n, v;
	unsigned char c;

	d = dst;
	s = src;

	/*
	 * Loop until we hit the end of the input. The stop condition must
	 * track the input cursor (s): collapsing an escape advances s ahead
	 * of the output cursor (d), so once the two diverge *d no longer
	 * reflects where the input terminates.
	 */
	while (*s != '\0') {
		if (*s != '\\') {
			*d++ = *s++;
			continue;
		}

		/*
		 * A backslash at the very end of the string escapes nothing
		 * (there is no next character); emit it verbatim and stop
		 * rather than read past the terminator.
		 */
		if (*(s + 1) == '\0') {
			*d++ = *s++;
			break;
		}

		/* Replace the backslash with the correct character */
		s++;
		switch (*s) {
		case 'a': *d = '\a'; break; /* bell/alert (BEL) */
		case 'b': *d = '\b'; break; /* backspace */
		case 'f': *d = '\f'; break; /* form feed */
		case 'n': *d = '\n'; break; /* new line */
		case 'r': *d = '\r'; break; /* carriage return */
		case 't': *d = '\t'; break; /* horizontal tab */
		case 'v': *d = '\v'; break; /* vertical tab */
		case 'x': /* hex value (1 to 2 digits)(\xNN) */
			v = 0;
			n = 0;
			while (n < 2) {
				c = (unsigned char)*(s + 1);
				if (c >= '0' && c <= '9')
					v = (v << 4) + (c - '0');
				else if (c >= 'A' && c <= 'F')
					v = (v << 4) + (c - 'A' + 10);
				else if (c >= 'a' && c <= 'f')
					v = (v << 4) + (c - 'a' + 10);
				else
					break;
				s++;
				n++;
			}
			/* \x with no digits: emit the 'x' (unknown escape) */
			*d = (n == 0) ? 'x' : (char)v;
			break;
		default: /* octal (\NNN, 1 to 3 digits) or unknown sequence */
			if (*s >= '0' && *s <= '7') {
				v = (unsigned int)(*s - '0');
				n = 1;
				while (n < 3 && *(s + 1) >= '0' &&
				    *(s + 1) <= '7') {
					s++;
					v = (v << 3) +
					    (unsigned int)(*s - '0');
					n++;
				}
				*d = (char)v;
			} else
				*d = *s;
			break;
		}

		/* Increment to next offset, possible next escape sequence */
		d++;
		s++;
	}

	/*
	 * Terminate at the (possibly earlier) output cursor. When any
	 * escape was collapsed the string shrank, so the trailing bytes
	 * between d and s are now stale and must be cut off here.
	 */
	*d = '\0';
}

/*
 * Convert a string to lower case. Pass a mutable buffer (a local or global
 * non-const array is fine); do not pass a string constant.
 */
void
bsdconf_strtolower(char *buf)
{
	char *p = buf;

	if (buf == NULL)
		return;

	while (*p != '\0') {
		*p = (char)tolower((unsigned char)*p);
		p++;
	}
}
