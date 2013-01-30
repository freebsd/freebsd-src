/*-
Copyright (C) 2013 Pietro Cerutti <gahr@FreeBSD.org>

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct __fmemopen_cookie
{
	char *buf;	/* pointer to the memory region */
	char  own;	/* did we allocate the buffer ourselves? */
	long  len;	/* buffer length in bytes */
	long  off;	/* current offset into the buffer */
};

static int	fmemopen_read  (void *cookie, char *buf, int nbytes);
static int	fmemopen_write (void *cookie, const char *buf, int nbytes);
static fpos_t	fmemopen_seek  (void *cookie, fpos_t offset, int whence);
static int	fmemopen_close (void *cookie);

FILE *
fmemopen (void * __restrict buf, size_t size, const char * __restrict mode)
{
	/* allocate cookie */
	struct __fmemopen_cookie *ck = malloc (sizeof (struct __fmemopen_cookie));
	if (ck == NULL) {
		errno = ENOMEM;
		return (NULL);
	}

	ck->off = 0;
	ck->len = size;

	/* do we have to allocate the buffer ourselves? */
	ck->own = ((ck->buf = buf) == NULL);
	if (ck->own) {
		ck->buf = malloc (size);
		if (ck->buf == NULL) {
			free (ck);
			errno = ENOMEM;
			return (NULL);
		}
		ck->buf[0] = '\0';
	}

	if (mode[0] == 'a')
		ck->off = strnlen(ck->buf, ck->len);

	/* actuall wrapper */
	FILE *f = funopen ((void *)ck, fmemopen_read, fmemopen_write,
	    fmemopen_seek, fmemopen_close);

	if (f == NULL) {
		if (ck->own)
			free (ck->buf);
		free (ck);
		return (NULL);
	}

	/* turn off buffering, so a write past the end of the buffer
	 * correctly returns a short object count */
	setvbuf (f, (char *) NULL, _IONBF, 0);

	return (f);
}

static int
fmemopen_read (void *cookie, char *buf, int nbytes)
{
	struct __fmemopen_cookie *ck = cookie;

	if (nbytes > ck->len - ck->off)
		nbytes = ck->len - ck->off;

	if (nbytes == 0)
		return (0);

	memcpy (buf, ck->buf + ck->off, nbytes);

	ck->off += nbytes;

	return (nbytes);
}

static int
fmemopen_write (void *cookie, const char *buf, int nbytes)
{
	struct __fmemopen_cookie *ck = cookie;

	if (nbytes > ck->len - ck->off)
		nbytes = ck->len - ck->off;

	if (nbytes == 0)
		return (0);

	memcpy (ck->buf + ck->off, buf, nbytes);

	ck->off += nbytes;

	if (ck->off < ck->len && ck->buf[ck->off - 1] != '\0')
		ck->buf[ck->off] = '\0';

	return (nbytes);
}

static fpos_t
fmemopen_seek (void *cookie, fpos_t offset, int whence)
{
	struct __fmemopen_cookie *ck = cookie;


	switch (whence) {
	case SEEK_SET:
		if (offset > ck->len) {
			errno = EINVAL;
			return (-1);
		}
		ck->off = offset;
		break;

	case SEEK_CUR:
		if (ck->off + offset > ck->len) {
			errno = EINVAL;
			return (-1);
		}
		ck->off += offset;
		break;

	case SEEK_END:
		if (offset > 0 || -offset > ck->len) {
			errno = EINVAL;
			return (-1);
		}
		ck->off = ck->len + offset;
		break;

	default:
		errno = EINVAL;
		return (-1);
	}

	return (ck->off);
}

static int
fmemopen_close (void *cookie)
{
	struct __fmemopen_cookie *ck = cookie;

	if (ck->own)
		free (ck->buf);

	free (ck);

	return (0);
}
