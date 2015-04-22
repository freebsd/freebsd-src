/*-
 * Copyright (c) 2014 Mark Johnston <markj@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <sys/gzio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/zutil.h>

#define	KERN_GZ_HDRLEN		10	/* gzip header length */
#define	KERN_GZ_TRAILERLEN	8	/* gzip trailer length */
#define	KERN_GZ_MAGIC1		0x1f	/* first magic byte */
#define	KERN_GZ_MAGIC2		0x8b	/* second magic byte */

MALLOC_DEFINE(M_GZIO, "gzio", "zlib state");

struct gzio_stream {
	uint8_t *	gz_buffer;	/* output buffer */
	size_t		gz_bufsz;	/* total buffer size */
	off_t		gz_off;		/* offset into the output stream */
	enum gzio_mode	gz_mode;	/* stream mode */
	uint32_t	gz_crc;		/* stream CRC32 */
	gzio_cb		gz_cb;		/* output callback */
	void *		gz_arg;		/* private callback arg */
	z_stream	gz_stream;	/* zlib state */
};

static void *	gz_alloc(void *, u_int, u_int);
static void	gz_free(void *, void *);
static int	gz_write(struct gzio_stream *, void *, u_int, int);

struct gzio_stream *
gzio_init(gzio_cb cb, enum gzio_mode mode, size_t bufsz, int level, void *arg)
{
	struct gzio_stream *s;
	uint8_t *hdr;
	int error;

	if (bufsz < KERN_GZ_HDRLEN)
		return (NULL);
	if (mode != GZIO_DEFLATE)
		return (NULL);

	s = gz_alloc(NULL, 1, sizeof(*s));
	s->gz_bufsz = bufsz;
	s->gz_buffer = gz_alloc(NULL, 1, s->gz_bufsz);
	s->gz_mode = mode;
	s->gz_crc = ~0U;
	s->gz_cb = cb;
	s->gz_arg = arg;

	s->gz_stream.zalloc = gz_alloc;
	s->gz_stream.zfree = gz_free;
	s->gz_stream.opaque = NULL;
	s->gz_stream.next_in = Z_NULL;
	s->gz_stream.avail_in = 0;

	error = deflateInit2(&s->gz_stream, level, Z_DEFLATED, -MAX_WBITS,
	    DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
	if (error != 0)
		goto fail;

	s->gz_stream.avail_out = s->gz_bufsz;
	s->gz_stream.next_out = s->gz_buffer;

	/* Write the gzip header to the output buffer. */
	hdr = s->gz_buffer;
	memset(hdr, 0, KERN_GZ_HDRLEN);
	hdr[0] = KERN_GZ_MAGIC1;
	hdr[1] = KERN_GZ_MAGIC2;
	hdr[2] = Z_DEFLATED;
	hdr[9] = OS_CODE;
	s->gz_stream.next_out += KERN_GZ_HDRLEN;
	s->gz_stream.avail_out -= KERN_GZ_HDRLEN;

	return (s);

fail:
	gz_free(NULL, s->gz_buffer);
	gz_free(NULL, s);
	return (NULL);
}

int
gzio_write(struct gzio_stream *s, void *data, u_int len)
{

	return (gz_write(s, data, len, Z_NO_FLUSH));
}

int
gzio_flush(struct gzio_stream *s)
{

	return (gz_write(s, NULL, 0, Z_FINISH));
}

void
gzio_fini(struct gzio_stream *s)
{

	(void)deflateEnd(&s->gz_stream);
	gz_free(NULL, s->gz_buffer);
	gz_free(NULL, s);
}

static void *
gz_alloc(void *arg __unused, u_int n, u_int sz)
{

	/*
	 * Memory for zlib state is allocated using M_NODUMP since it may be
	 * used to compress a kernel dump, and we don't want zlib to attempt to
	 * compress its own state.
	 */
	return (malloc(n * sz, M_GZIO, M_WAITOK | M_ZERO | M_NODUMP));
}

static void
gz_free(void *arg __unused, void *ptr)
{

	free(ptr, M_GZIO);
}

static int
gz_write(struct gzio_stream *s, void *buf, u_int len, int zflag)
{
	uint8_t trailer[KERN_GZ_TRAILERLEN];
	size_t room;
	int error, zerror;

	KASSERT(zflag == Z_FINISH || zflag == Z_NO_FLUSH,
	    ("unexpected flag %d", zflag));
	KASSERT(s->gz_mode == GZIO_DEFLATE,
	    ("invalid stream mode %d", s->gz_mode));

	if (len > 0) {
		s->gz_stream.avail_in = len;
		s->gz_stream.next_in = buf;
		s->gz_crc = crc32_raw(buf, len, s->gz_crc);
	} else
		s->gz_crc ^= ~0U;

	error = 0;
	do {
		zerror = deflate(&s->gz_stream, zflag);
		if (zerror != Z_OK && zerror != Z_STREAM_END) {
			error = EIO;
			break;
		}

		if (s->gz_stream.avail_out == 0 || zerror == Z_STREAM_END) {
			/*
			 * Our output buffer is full or there's nothing left
			 * to produce, so we're flushing the buffer.
			 */
			len = s->gz_bufsz - s->gz_stream.avail_out;
			if (zerror == Z_STREAM_END) {
				/*
				 * Try to pack as much of the trailer into the
				 * output buffer as we can.
				 */
				((uint32_t *)trailer)[0] = s->gz_crc;
				((uint32_t *)trailer)[1] =
				    s->gz_stream.total_in;
				room = MIN(KERN_GZ_TRAILERLEN,
				    s->gz_bufsz - len);
				memcpy(s->gz_buffer + len, trailer, room);
				len += room;
			}

			error = s->gz_cb(s->gz_buffer, len, s->gz_off,
			    s->gz_arg);
			if (error != 0)
				break;

			s->gz_off += len;
			s->gz_stream.next_out = s->gz_buffer;
			s->gz_stream.avail_out = s->gz_bufsz;

			/*
			 * If we couldn't pack the trailer into the output
			 * buffer, write it out now.
			 */
			if (zerror == Z_STREAM_END && room < KERN_GZ_TRAILERLEN)
				error = s->gz_cb(trailer + room,
				    KERN_GZ_TRAILERLEN - room, s->gz_off,
				    s->gz_arg);
		}
	} while (zerror != Z_STREAM_END &&
	    (zflag == Z_FINISH || s->gz_stream.avail_in > 0));

	return (error);
}
