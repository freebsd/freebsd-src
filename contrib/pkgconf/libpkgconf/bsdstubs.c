/*	$OpenBSD: strlcpy.c,v 1.10 2005/08/08 08:05:37 espie Exp $	*/
/*	$OpenBSD: strlcat.c,v 1.12 2005/03/30 20:13:52 otto Exp $	*/

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
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

#include <libpkgconf/stdinc.h>
#include <libpkgconf/bsdstubs.h>
#include <libpkgconf/config.h>

#if !HAVE_DECL_STRLCPY
/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
static inline size_t
strlcpy(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0) {
		while (--n != 0) {
			if ((*d++ = *s++) == '\0')
				break;
		}
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}
#endif

#if !HAVE_DECL_STRLCAT
/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
static inline size_t
strlcat(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != '\0')
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return(dlen + (s - src));	/* count does not include NUL */
}
#endif

/*
 * Copyright (c) 2012 William Pitcock <nenolod@dereferenced.org>.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#if !HAVE_DECL_STRNDUP
/*
 * Creates a memory buffer and copies at most 'len' characters to it.
 * If 'len' is less than the length of the source string, truncation occured.
 */
static inline char *
strndup(const char *src, size_t len)
{
	char *out = malloc(len + 1);
	pkgconf_strlcpy(out, src, len + 1);
	return out;
}
#endif

#if !HAVE_DECL_PLEDGE
static inline int
pledge(const char *promises, const char *execpromises)
{
	(void) promises;
	(void) execpromises;

	return 0;
}
#endif

#if !HAVE_DECL_UNVEIL
static inline int
unveil(const char *path, const char *permissions)
{
	(void) path;
	(void) permissions;

	return 0;
}
#endif

size_t
pkgconf_strlcpy(char *dst, const char *src, size_t siz)
{
	return strlcpy(dst, src, siz);
}

size_t
pkgconf_strlcat(char *dst, const char *src, size_t siz)
{
	return strlcat(dst, src, siz);
}

char *
pkgconf_strndup(const char *src, size_t len)
{
	return strndup(src, len);
}

#if !HAVE_DECL_REALLOCARRAY
void *
reallocarray(void *ptr, size_t m, size_t n)
{
	if (n && m > -1 / n)
	{
		errno = ENOMEM;
		return 0;
	}

	return realloc(ptr, m * n);
}
#endif

void *
pkgconf_reallocarray(void *ptr, size_t m, size_t n)
{
	return reallocarray(ptr, m, n);
}

int
pkgconf_pledge(const char *promises, const char *execpromises)
{
	return pledge(promises, execpromises);
}

int
pkgconf_unveil(const char *path, const char *permissions)
{
	return unveil(path, permissions);
}
