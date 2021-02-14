/*	$OpenBSD: sshbuf-misc.c,v 1.11 2019/07/30 05:04:49 djm Exp $	*/
/*
 * Copyright (c) 2011 Damien Miller
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

#include "includes.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <resolv.h>
#include <ctype.h>

#include "ssherr.h"
#define SSHBUF_INTERNAL
#include "sshbuf.h"

void
sshbuf_dump_data(const void *s, size_t len, FILE *f)
{
	size_t i, j;
	const u_char *p = (const u_char *)s;

	for (i = 0; i < len; i += 16) {
		fprintf(f, "%.4zu: ", i);
		for (j = i; j < i + 16; j++) {
			if (j < len)
				fprintf(f, "%02x ", p[j]);
			else
				fprintf(f, "   ");
		}
		fprintf(f, " ");
		for (j = i; j < i + 16; j++) {
			if (j < len) {
				if  (isascii(p[j]) && isprint(p[j]))
					fprintf(f, "%c", p[j]);
				else
					fprintf(f, ".");
			}
		}
		fprintf(f, "\n");
	}
}

void
sshbuf_dump(struct sshbuf *buf, FILE *f)
{
	fprintf(f, "buffer %p len = %zu\n", buf, sshbuf_len(buf));
	sshbuf_dump_data(sshbuf_ptr(buf), sshbuf_len(buf), f);
}

char *
sshbuf_dtob16(struct sshbuf *buf)
{
	size_t i, j, len = sshbuf_len(buf);
	const u_char *p = sshbuf_ptr(buf);
	char *ret;
	const char hex[] = "0123456789abcdef";

	if (len == 0)
		return strdup("");
	if (SIZE_MAX / 2 <= len || (ret = malloc(len * 2 + 1)) == NULL)
		return NULL;
	for (i = j = 0; i < len; i++) {
		ret[j++] = hex[(p[i] >> 4) & 0xf];
		ret[j++] = hex[p[i] & 0xf];
	}
	ret[j] = '\0';
	return ret;
}

int
sshbuf_dtob64(const struct sshbuf *d, struct sshbuf *b64, int wrap)
{
	size_t i, slen = 0;
	char *s = NULL;
	int r;

	if (d == NULL || b64 == NULL || sshbuf_len(d) >= SIZE_MAX / 2)
		return SSH_ERR_INVALID_ARGUMENT;
	if (sshbuf_len(d) == 0)
		return 0;
	slen = ((sshbuf_len(d) + 2) / 3) * 4 + 1;
	if ((s = malloc(slen)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if (b64_ntop(sshbuf_ptr(d), sshbuf_len(d), s, slen) == -1) {
		r = SSH_ERR_INTERNAL_ERROR;
		goto fail;
	}
	if (wrap) {
		for (i = 0; s[i] != '\0'; i++) {
			if ((r = sshbuf_put_u8(b64, s[i])) != 0)
				goto fail;
			if (i % 70 == 69 && (r = sshbuf_put_u8(b64, '\n')) != 0)
				goto fail;
		}
		if ((i - 1) % 70 != 69 && (r = sshbuf_put_u8(b64, '\n')) != 0)
			goto fail;
	} else {
		if ((r = sshbuf_put(b64, s, strlen(s))) != 0)
			goto fail;
	}
	/* Success */
	r = 0;
 fail:
	freezero(s, slen);
	return r;
}

char *
sshbuf_dtob64_string(const struct sshbuf *buf, int wrap)
{
	struct sshbuf *tmp;
	char *ret;

	if ((tmp = sshbuf_new()) == NULL)
		return NULL;
	if (sshbuf_dtob64(buf, tmp, wrap) != 0) {
		sshbuf_free(tmp);
		return NULL;
	}
	ret = sshbuf_dup_string(tmp);
	sshbuf_free(tmp);
	return ret;
}

int
sshbuf_b64tod(struct sshbuf *buf, const char *b64)
{
	size_t plen = strlen(b64);
	int nlen, r;
	u_char *p;

	if (plen == 0)
		return 0;
	if ((p = malloc(plen)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((nlen = b64_pton(b64, p, plen)) < 0) {
		explicit_bzero(p, plen);
		free(p);
		return SSH_ERR_INVALID_FORMAT;
	}
	if ((r = sshbuf_put(buf, p, nlen)) < 0) {
		explicit_bzero(p, plen);
		free(p);
		return r;
	}
	explicit_bzero(p, plen);
	free(p);
	return 0;
}

char *
sshbuf_dup_string(struct sshbuf *buf)
{
	const u_char *p = NULL, *s = sshbuf_ptr(buf);
	size_t l = sshbuf_len(buf);
	char *r;

	if (s == NULL || l > SIZE_MAX)
		return NULL;
	/* accept a nul only as the last character in the buffer */
	if (l > 0 && (p = memchr(s, '\0', l)) != NULL) {
		if (p != s + l - 1)
			return NULL;
		l--; /* the nul is put back below */
	}
	if ((r = malloc(l + 1)) == NULL)
		return NULL;
	if (l > 0)
		memcpy(r, s, l);
	r[l] = '\0';
	return r;
}

int
sshbuf_cmp(const struct sshbuf *b, size_t offset,
    const void *s, size_t len)
{
	if (sshbuf_ptr(b) == NULL)
		return SSH_ERR_INTERNAL_ERROR;
	if (offset > SSHBUF_SIZE_MAX || len > SSHBUF_SIZE_MAX || len == 0)
		return SSH_ERR_INVALID_ARGUMENT;
	if (offset + len > sshbuf_len(b))
		return SSH_ERR_MESSAGE_INCOMPLETE;
	if (timingsafe_bcmp(sshbuf_ptr(b) + offset, s, len) != 0)
		return SSH_ERR_INVALID_FORMAT;
	return 0;
}

int
sshbuf_find(const struct sshbuf *b, size_t start_offset,
    const void *s, size_t len, size_t *offsetp)
{
	void *p;

	if (offsetp != NULL)
		*offsetp = 0;
	if (sshbuf_ptr(b) == NULL)
		return SSH_ERR_INTERNAL_ERROR;
	if (start_offset > SSHBUF_SIZE_MAX || len > SSHBUF_SIZE_MAX || len == 0)
		return SSH_ERR_INVALID_ARGUMENT;
	if (start_offset > sshbuf_len(b) || start_offset + len > sshbuf_len(b))
		return SSH_ERR_MESSAGE_INCOMPLETE;
	if ((p = memmem(sshbuf_ptr(b) + start_offset,
	    sshbuf_len(b) - start_offset, s, len)) == NULL)
		return SSH_ERR_INVALID_FORMAT;
	if (offsetp != NULL)
		*offsetp = (const u_char *)p - sshbuf_ptr(b);
	return 0;
}
