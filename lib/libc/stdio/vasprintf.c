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
static char rcsid[] = "$Id: vasprintf.c,v 1.3 1996/07/28 16:16:11 peter Exp $";
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
	size_t	size;
	size_t	left;
};

static int 	writehook __P((void *cookie, const char *, int));

static int
writehook(cookie, buf, len)
	void *cookie;
	const char *buf;
	int   len;
{
	struct bufcookie *h = (struct bufcookie *)cookie;
	char *newbuf;

	if (len == 0)
		return 0;

	if (len > h->left) {
		/* grow malloc region */
 		/*
		 * XXX this is linearly expanded, which is slow for obscenely
		 * large strings.
		 */
		h->left = h->left + len + CHUNK_SPARE;
		h->size = h->size + len + CHUNK_SPARE;
		newbuf = realloc(h->base, h->size);
		if (newbuf == NULL) {
			free(h->base);
			h->base = NULL;
			return (-1);
		} else
			h->base = newbuf;
	}
	/* "write" it */
	(void)memcpy(h->base + h->size - h->left, buf, (size_t)len);
	h->left -= len;
	return (len);
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

	/*
	 * clean up the wreckage. Did writehook fail or did something else
	 * in stdio explode perhaps?
	 */
	if (h.base == NULL)	/* realloc failed in writehook */
		return (-1);
	if (ret < 0) {		/* something else? */
		free(h.base);
		return (-1);
	}

	/*
	 * At this point, we have a non-null terminated string in a
	 * buffer.  There may not be enough room to null-terminate it
	 * (h.left == 0) - if realloc failes to expand it, it's fatal.
	 * If we were merely trying to shrink the buffer, a realloc failure
	 * is not [yet] fatal. Note that when realloc returns NULL,
	 * the original buffer is left allocated and valid.
	 */
	if (h.left == 1)	/* exact fit, do not realloc */
		*str = h.base;
	else {
		*str = realloc(h.base, (size_t)(h.size - h.left + 1));
		if (*str == NULL) {
			/* failed to expand? - fatal */
			if (h.left == 0) {
				free(h.base);
				return (-1);
			}
			*str = h.base;	/* use oversize original buffer */
		}
	}
	(*str)[h.size - h.left] = '\0';
	return (ret);
}
