/*-
 * Copyright (c) 1996  Peter Wemm <peter@freebsd.org>
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

#if defined(LIBC_RCS) && !defined(lint)
static char rcsid[] = "$Id$";
#endif /* LIBC_RCS and not lint */

#include <stdio.h>
#include <stdlib.h>

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#define CHUNK_SPARE 128 /* how much spare to allocate to avoid realloc calls */

struct bufcookie {
	char	*base;	/* start of buffer */
	int	size;
	int	left;
};

static int
writehook(cookie, buf, len)
	void *cookie;
	char *buf;
	int   len;
{
	struct bufcookie *h = (struct bufcookie *)cookie;

	if (len == 0)
		return 0;

	if (len > h->left) {
		/* grow malloc region */
		h->left = h->left + len + CHUNK_SPARE;
		h->size = h->size + len + CHUNK_SPARE;
		h->base = realloc(h->base, h->size);
		if (h->base == NULL)
			return (-1);
	}
	/* "write" it */
	(void)memcpy(h->base + h->size - h->left, buf, len);
	h->left -= len;
	return (0);
}


int
vasprintf(str, fmt, ap)
	char **str;
	const char *fmt;
	va_list ap;
{
	int ret;
	FILE *f;
	struct bufcookie h;

	h.base = malloc(CHUNK_SPARE);
	if (h.base == NULL)
		return (-1);
	h.size = CHUNK_SPARE;
	h.left = CHUNK_SPARE;

	f = funopen(&h, NULL, writehook, NULL, NULL);
	if (f == NULL) {
		free(h.base);
		return (-1);
	}
	ret = vfprintf(f, fmt, ap);
	fclose(f);
	if (ret < 0) {
		free(h.base);
		return (-1);
	}
	if (h.base == NULL)	/* failed to realloc in writehook */
		return (-1);

	h.base[h.size - h.left] = '\0';
	*str = realloc(h.base, h.size - h.left + 1);
	if (*str == NULL)	/* failed to realloc it to actual size */
		return -1;
	return (ret);
}
