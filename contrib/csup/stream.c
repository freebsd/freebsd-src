/*-
 * Copyright (c) 2003-2006, Maxime Henrion <mux@FreeBSD.org>
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <zlib.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "misc.h"
#include "stream.h"

/*
 * Simple stream API to make my life easier.  If the fgetln() and
 * funopen() functions were standard and if funopen() wasn't using
 * wrong types for the function pointers, I could have just used
 * stdio, but life sucks.
 *
 * For now, streams are always block-buffered.
 */

/*
 * Try to quiet warnings as much as possible with GCC while staying
 * compatible with other compilers.
 */
#ifndef __unused
#if defined(__GNUC__) && (__GNUC__ > 2 || __GNUC__ == 2 && __GNUC_MINOR__ >= 7)
#define	__unused	__attribute__((__unused__))
#else
#define	__unused
#endif
#endif

/*
 * Flags passed to the flush methods.
 *
 * STREAM_FLUSH_CLOSING is passed during the last flush call before
 * closing a stream.  This allows the zlib filter to emit the EOF
 * marker as appropriate.  In all other cases, STREAM_FLUSH_NORMAL
 * should be passed.
 *
 * These flags are completely unused in the default flush method,
 * but they are very important for the flush method of the zlib
 * filter.
 */
typedef enum {
	STREAM_FLUSH_NORMAL,
	STREAM_FLUSH_CLOSING
} stream_flush_t;

/*
 * This is because buf_new() will always allocate size + 1 bytes,
 * so our buffer sizes will still be power of 2 values.
 */
#define	STREAM_BUFSIZ	1023

struct buf {
	char *buf;
	size_t size;
	size_t in;
	size_t off;
};

struct stream {
	void *cookie;
	int fd;
	struct buf *rdbuf;
	struct buf *wrbuf;
	stream_readfn_t *readfn;
	stream_writefn_t *writefn;
	stream_closefn_t *closefn;
	int eof;
	struct stream_filter *filter;
	void *fdata;
};

typedef int	stream_filter_initfn_t(struct stream *, void *);
typedef void	stream_filter_finifn_t(struct stream *);
typedef int	stream_filter_flushfn_t(struct stream *, struct buf *,
		    stream_flush_t);
typedef ssize_t	stream_filter_fillfn_t(struct stream *, struct buf *);

struct stream_filter {
	stream_filter_t id;
	stream_filter_initfn_t *initfn;
	stream_filter_finifn_t *finifn;
	stream_filter_fillfn_t *fillfn;
	stream_filter_flushfn_t *flushfn;
};

/* Low-level buffer API. */
#define	buf_avail(buf)		((buf)->size - (buf)->off - (buf)->in)
#define	buf_count(buf)		((buf)->in)
#define	buf_size(buf)		((buf)->size)

static struct buf	*buf_new(size_t);
static void		 buf_more(struct buf *, size_t);
static void		 buf_less(struct buf *, size_t);
static void		 buf_free(struct buf *);
static void		 buf_grow(struct buf *, size_t);

/* Internal stream functions. */
static ssize_t		 stream_fill(struct stream *);
static ssize_t		 stream_fill_default(struct stream *, struct buf *);
static int		 stream_flush_int(struct stream *, stream_flush_t);
static int		 stream_flush_default(struct stream *, struct buf *,
			     stream_flush_t);

/* Filters specific functions. */
static struct stream_filter *stream_filter_lookup(stream_filter_t);
static int		 stream_filter_init(struct stream *, void *);
static void		 stream_filter_fini(struct stream *);

/* The zlib stream filter declarations. */
#define	ZFILTER_EOF	1				/* Got Z_STREAM_END. */

struct zfilter {
	int flags;
	struct buf *rdbuf;
	struct buf *wrbuf;
	z_stream *rdstate;
	z_stream *wrstate;
};

static int		 zfilter_init(struct stream *, void *);
static void		 zfilter_fini(struct stream *);
static ssize_t		 zfilter_fill(struct stream *, struct buf *);
static int		 zfilter_flush(struct stream *, struct buf *,
			     stream_flush_t);

/* The MD5 stream filter. */
struct md5filter {
	MD5_CTX ctx;
	char *md5;
};

static int		 md5filter_init(struct stream *, void *);
static void		 md5filter_fini(struct stream *);
static ssize_t		 md5filter_fill(struct stream *, struct buf *);
static int		 md5filter_flush(struct stream *, struct buf *,
			     stream_flush_t);

/* The available stream filters. */
struct stream_filter stream_filters[] = {
	{
		STREAM_FILTER_NULL,
		NULL,
		NULL,
		stream_fill_default,
		stream_flush_default
	},
	{
	       	STREAM_FILTER_ZLIB,
		zfilter_init,
		zfilter_fini,
		zfilter_fill,
		zfilter_flush
	},
	{
		STREAM_FILTER_MD5,
		md5filter_init,
		md5filter_fini,
		md5filter_fill,
		md5filter_flush
	}
};


/* Create a new buffer. */
static struct buf *
buf_new(size_t size)
{
	struct buf *buf;

	buf = xmalloc(sizeof(struct buf));
	/*
	 * We keep one spare byte so that stream_getln() can put a '\0'
	 * there in case the stream doesn't have an ending newline.
	 */
	buf->buf = xmalloc(size + 1);
	buf->size = size;
	buf->in = 0;
	buf->off = 0;
	return (buf);
}

/*
 * Grow the size of the buffer.  If "need" is 0, bump its size to the
 * next power of 2 value.  Otherwise, bump it to the next power of 2
 * value bigger than "need".
 */
static void
buf_grow(struct buf *buf, size_t need)
{

	if (need == 0)
		buf->size = buf->size * 2 + 1; /* Account for the spare byte. */
	else {
		assert(need > buf->size);
		while (buf->size < need)
			buf->size = buf->size * 2 + 1;
	}
	buf->buf = xrealloc(buf->buf, buf->size + 1);
}

/* Make more room in the buffer if needed. */
static void
buf_prewrite(struct buf *buf)
{

	if (buf_count(buf) == buf_size(buf))
		buf_grow(buf, 0);
	if (buf_count(buf) > 0 && buf_avail(buf) == 0) {
		memmove(buf->buf, buf->buf + buf->off, buf_count(buf));
		buf->off = 0;
	}
}

/* Account for "n" bytes being added in the buffer. */
static void
buf_more(struct buf *buf, size_t n)
{

	assert(n <= buf_avail(buf));
	buf->in += n;
}

/* Account for "n" bytes having been read in the buffer. */
static void
buf_less(struct buf *buf, size_t n)
{

	assert(n <= buf_count(buf));
	buf->in -= n;
	if (buf->in == 0)
		buf->off = 0;
	else
		buf->off += n;
}

/* Free a buffer. */
static void
buf_free(struct buf *buf)
{

	free(buf->buf);
	free(buf);
}

static struct stream *
stream_new(stream_readfn_t *readfn, stream_writefn_t *writefn,
    stream_closefn_t *closefn)
{
	struct stream *stream;

	stream = xmalloc(sizeof(struct stream));
	if (readfn == NULL && writefn == NULL) {
		errno = EINVAL;
		return (NULL);
	}
	if (readfn != NULL)
		stream->rdbuf = buf_new(STREAM_BUFSIZ);
	else
		stream->rdbuf = NULL;
	if (writefn != NULL)
		stream->wrbuf = buf_new(STREAM_BUFSIZ);
	else
		stream->wrbuf = NULL;
	stream->cookie = NULL;
	stream->fd = -1;
	stream->readfn = readfn;
	stream->writefn = writefn;
	stream->closefn = closefn;
	stream->filter = stream_filter_lookup(STREAM_FILTER_NULL);
	stream->fdata = NULL;
	stream->eof = 0;
	return (stream);
}

/* Create a new stream associated with a void *. */
struct stream *
stream_open(void *cookie, stream_readfn_t *readfn, stream_writefn_t *writefn,
    stream_closefn_t *closefn)
{
	struct stream *stream;

	stream = stream_new(readfn, writefn, closefn);
	stream->cookie = cookie;
	return (stream);
}

/* Associate a file descriptor with a stream. */
struct stream *
stream_open_fd(int fd, stream_readfn_t *readfn, stream_writefn_t *writefn,
    stream_closefn_t *closefn)
{
	struct stream *stream;

	stream = stream_new(readfn, writefn, closefn);
	stream->cookie = &stream->fd;
	stream->fd = fd;
	return (stream);
}

/* Like open() but returns a stream. */
struct stream *
stream_open_file(const char *path, int flags, ...)
{
	struct stream *stream;
	stream_readfn_t *readfn;
	stream_writefn_t *writefn;
	va_list ap;
	mode_t mode;
	int fd;

	va_start(ap, flags);
	if (flags & O_CREAT) {
		/*
		 * GCC says I should not be using mode_t here since it's
		 * promoted to an int when passed through `...'.
		 */
		mode = va_arg(ap, int);
		fd = open(path, flags, mode);
	} else
		fd = open(path, flags);
	va_end(ap);
	if (fd == -1)
		return (NULL);

	flags &= O_ACCMODE;
	if (flags == O_RDONLY) {
		readfn = stream_read_fd;
		writefn = NULL;
	} else if (flags == O_WRONLY) {
		readfn = NULL;
		writefn = stream_write_fd;
	} else if (flags == O_RDWR) {
		assert(flags == O_RDWR);
		readfn = stream_read_fd;
		writefn = stream_write_fd;
	} else {
		errno = EINVAL;
		close(fd);
		return (NULL);
	}

	stream = stream_open_fd(fd, readfn, writefn, stream_close_fd);
	if (stream == NULL)
		close(fd);
	return (stream);
}

/* Return the file descriptor associated with this stream, or -1. */
int
stream_fileno(struct stream *stream)
{

	return (stream->fd);
}

/* Convenience read function for file descriptors. */
ssize_t
stream_read_fd(void *cookie, void *buf, size_t size)
{
	ssize_t nbytes;
	int fd;

	fd = *(int *)cookie;
	nbytes = read(fd, buf, size);
	return (nbytes);
}

/* Convenience write function for file descriptors. */
ssize_t
stream_write_fd(void *cookie, const void *buf, size_t size)
{
	ssize_t nbytes;
	int fd;

	fd = *(int *)cookie;
	nbytes = write(fd, buf, size);
	return (nbytes);
}

/* Convenience close function for file descriptors. */
int
stream_close_fd(void *cookie)
{
	int fd, ret;

	fd = *(int *)cookie;
	ret = close(fd);
	return (ret);
}

/* Read some bytes from the stream. */
ssize_t
stream_read(struct stream *stream, void *buf, size_t size)
{
	struct buf *rdbuf;
	ssize_t ret;
	size_t n;

	rdbuf = stream->rdbuf;
	if (buf_count(rdbuf) == 0) {
		ret = stream_fill(stream);
		if (ret <= 0)
			return (-1);
	}
	n = min(size, buf_count(rdbuf));
	memcpy(buf, rdbuf->buf + rdbuf->off, n);
	buf_less(rdbuf, n);
	return (n);
}

/*
 * Read a line from the stream and return a pointer to it.
 *
 * If "len" is non-NULL, the length of the string will be put into it.
 * The pointer is only valid until the next stream API call.  The line
 * can be modified by the caller, provided he doesn't write before or
 * after it.
 *
 * This is somewhat similar to the BSD fgetln() function, except that
 * "len" can be NULL here.  In that case the string is terminated by
 * overwriting the '\n' character with a NUL character.  If it's the
 * last line in the stream and it has no ending newline, we can still
 * add '\0' after it, because we keep one spare byte in the buffers.
 *
 * However, be warned that one can't handle binary lines properly
 * without knowing the size of the string since those can contain
 * NUL characters.
 */
char *
stream_getln(struct stream *stream, size_t *len)
{
	struct buf *buf;
	char *cp, *line;
	ssize_t n;
	size_t done, size;

	buf = stream->rdbuf;
	if (buf_count(buf) == 0) {
		n = stream_fill(stream);
		if (n <= 0)
			return (NULL);
	}
	cp = memchr(buf->buf + buf->off, '\n', buf_count(buf));
	for (done = buf_count(buf); cp == NULL; done += n) {
		n = stream_fill(stream);
		if (n < 0)
			return (NULL);
		if (n == 0)
			/* Last line of the stream. */
			cp = buf->buf + buf->off + buf->in - 1;
		else
			cp = memchr(buf->buf + buf->off + done, '\n',
			    buf_count(buf) - done);
	}
	line = buf->buf + buf->off;
	assert(cp >= line);
	size = cp - line + 1;
	buf_less(buf, size);
	if (len != NULL) {
		*len = size;
	} else {
		/* Terminate the string when len == NULL. */
		if (line[size - 1] == '\n')
			line[size - 1] = '\0';
		else
			line[size] = '\0';
	}
	return (line);
}

/* Write some bytes to a stream. */
ssize_t
stream_write(struct stream *stream, const void *src, size_t nbytes)
{
	struct buf *buf;
	int error;

	buf = stream->wrbuf;
	if (nbytes > buf_size(buf))
		buf_grow(buf, nbytes);
	if (nbytes > buf_avail(buf)) {
		error = stream_flush_int(stream, STREAM_FLUSH_NORMAL);
		if (error)
			return (-1);
	}
	memcpy(buf->buf + buf->off + buf->in, src, nbytes);
	buf_more(buf, nbytes);
	return (nbytes);
}

/* Formatted output to a stream. */
int
stream_printf(struct stream *stream, const char *fmt, ...)
{
	struct buf *buf;
	va_list ap;
	int error, ret;

	buf = stream->wrbuf;
again:
	va_start(ap, fmt);
	ret = vsnprintf(buf->buf + buf->off + buf->in, buf_avail(buf), fmt, ap);
	va_end(ap);
	if (ret < 0)
		return (ret);
	if ((unsigned)ret >= buf_avail(buf)) {
		if ((unsigned)ret >= buf_size(buf))
			buf_grow(buf, ret + 1);
		if ((unsigned)ret >= buf_avail(buf)) {
			error = stream_flush_int(stream, STREAM_FLUSH_NORMAL);
			if (error)
				return (-1);
		}
		goto again;
	}
	buf_more(buf, ret);
	return (ret);
}

/* Flush the entire write buffer of the stream. */
int
stream_flush(struct stream *stream)
{
	int error;

	error = stream_flush_int(stream, STREAM_FLUSH_NORMAL);
	return (error);
}

/* Internal flush API. */
static int
stream_flush_int(struct stream *stream, stream_flush_t how)
{
	struct buf *buf;
	int error;

	buf = stream->wrbuf;
	error = (*stream->filter->flushfn)(stream, buf, how);
	assert(buf_count(buf) == 0);
	return (error);
}

/* The default flush method. */
static int
stream_flush_default(struct stream *stream, struct buf *buf,
    stream_flush_t __unused how)
{
	ssize_t n;

	while (buf_count(buf) > 0) {
		do {
			n = (*stream->writefn)(stream->cookie,
			    buf->buf + buf->off, buf_count(buf));
		} while (n == -1 && errno == EINTR);
		if (n <= 0)
			return (-1);
		buf_less(buf, n);
	}
	return (0);
}

/* Flush the write buffer and call fsync() on the file descriptor. */
int
stream_sync(struct stream *stream)
{
	int error;

	if (stream->fd == -1) {
		errno = EINVAL;
		return (-1);
	}
	error = stream_flush_int(stream, STREAM_FLUSH_NORMAL);
	if (error)
		return (-1);
	error = fsync(stream->fd);
	return (error);
}

/* Like truncate() but on a stream. */
int
stream_truncate(struct stream *stream, off_t size)
{
	int error;

	if (stream->fd == -1) {
		errno = EINVAL;
		return (-1);
	}
	error = stream_flush_int(stream, STREAM_FLUSH_NORMAL);
	if (error)
		return (-1);
	error = ftruncate(stream->fd, size);
	return (error);
}

/* Like stream_truncate() except the off_t parameter is an offset. */
int
stream_truncate_rel(struct stream *stream, off_t off)
{
	struct stat sb;
	int error;

	if (stream->fd == -1) {
		errno = EINVAL;
		return (-1);
	}
	error = stream_flush_int(stream, STREAM_FLUSH_NORMAL);
	if (error)
		return (-1);
	error = fstat(stream->fd, &sb);
	if (error)
		return (-1);
	error = stream_truncate(stream, sb.st_size + off);
	return (error);
}

/* Rewind the stream. */
int
stream_rewind(struct stream *stream)
{
	int error;

	if (stream->fd == -1) {
		errno = EINVAL;
		return (-1);
	}
	if (stream->rdbuf != NULL)
		buf_less(stream->rdbuf, buf_count(stream->rdbuf));
	if (stream->wrbuf != NULL) {
		error = stream_flush_int(stream, STREAM_FLUSH_NORMAL);
		if (error)
			return (error);
	}
	error = lseek(stream->fd, 0, SEEK_SET);
	return (error);
}

/* Return EOF status. */
int
stream_eof(struct stream *stream)
{

	return (stream->eof);
}

/* Close a stream and free any resources held by it. */
int
stream_close(struct stream *stream)
{
	int error;

	if (stream == NULL)
		return (0);

	error = 0;
	if (stream->wrbuf != NULL)
		error = stream_flush_int(stream, STREAM_FLUSH_CLOSING);
	stream_filter_fini(stream);
	if (stream->closefn != NULL)
		/*
		 * We might overwrite a previous error from stream_flush(),
		 * but we have no choice, because wether it had worked or
		 * not, we need to close the file descriptor.
		 */
		error = (*stream->closefn)(stream->cookie);
	if (stream->rdbuf != NULL)
		buf_free(stream->rdbuf);
	if (stream->wrbuf != NULL)
		buf_free(stream->wrbuf);
	free(stream);
	return (error);
}

/* The default fill method. */
static ssize_t
stream_fill_default(struct stream *stream, struct buf *buf)
{
	ssize_t n;

	if (stream->eof)
		return (0);
	assert(buf_avail(buf) > 0);
	n = (*stream->readfn)(stream->cookie, buf->buf + buf->off + buf->in,
	    buf_avail(buf));
	if (n < 0)
		return (-1);
	if (n == 0) {
		stream->eof = 1;
		return (0);
	}
	buf_more(buf, n);
	return (n);
}

/*
 * Refill the read buffer.  This function is not permitted to return
 * without having made more bytes available, unless there was an error.
 * Moreover, stream_fill() returns the number of bytes added.
 */
static ssize_t
stream_fill(struct stream *stream)
{
	struct stream_filter *filter;
	struct buf *buf;
#ifndef NDEBUG
	size_t oldcount;
#endif
	ssize_t n;

	filter = stream->filter;
	buf = stream->rdbuf;
	buf_prewrite(buf);
#ifndef NDEBUG
	oldcount = buf_count(buf);
#endif
	n = (*filter->fillfn)(stream, buf);
	assert((n > 0 && n == (signed)(buf_count(buf) - oldcount)) ||
	    (n <= 0 && buf_count(buf) == oldcount));
	return (n);
}

/*
 * Lookup a stream filter.
 *
 * We are not supposed to get passed an invalid filter id, since
 * filter ids are an enum type and we don't have invalid filter
 * ids in the enum :-).  Thus, we are not checking for out of
 * bounds access here.  If it happens, it's the caller's fault
 * anyway.
 */
static struct stream_filter *
stream_filter_lookup(stream_filter_t id)
{
	struct stream_filter *filter;

	filter = stream_filters;
	while (filter->id != id)
		filter++;
	return (filter);
}

static int
stream_filter_init(struct stream *stream, void *data)
{
	struct stream_filter *filter;
	int error;

	filter = stream->filter;
	if (filter->initfn == NULL)
		return (0);
	error = (*filter->initfn)(stream, data);
	return (error);
}

static void
stream_filter_fini(struct stream *stream)
{
	struct stream_filter *filter;

	filter = stream->filter;
	if (filter->finifn != NULL)
		(*filter->finifn)(stream);
}

/*
 * Start a filter on a stream.
 */
int
stream_filter_start(struct stream *stream, stream_filter_t id, void *data)
{
	struct stream_filter *filter;
	int error;

	filter = stream->filter;
	if (id == filter->id)
		return (0);
	stream_filter_fini(stream);
	stream->filter = stream_filter_lookup(id);
	stream->fdata = NULL;
	error = stream_filter_init(stream, data);
	return (error);
}


/* Stop a filter, this is equivalent to setting the null filter. */
void
stream_filter_stop(struct stream *stream)
{

	stream_filter_start(stream, STREAM_FILTER_NULL, NULL);
}

/* The zlib stream filter implementation. */

/* Take no chances with zlib... */
static void *
zfilter_alloc(void __unused *opaque, unsigned int items, unsigned int size)
{

	return (xmalloc(items * size));
}

static void
zfilter_free(void __unused *opaque, void *ptr)
{

	free(ptr);
}

static int
zfilter_init(struct stream *stream, void __unused *data)
{
	struct zfilter *zf;
	struct buf *buf;
	z_stream *state;
	int rv;

	zf = xmalloc(sizeof(struct zfilter));
	memset(zf, 0, sizeof(struct zfilter));
	if (stream->rdbuf != NULL) {
		state = xmalloc(sizeof(z_stream));
		state->zalloc = zfilter_alloc;
		state->zfree = zfilter_free;
		state->opaque = Z_NULL;
		rv = inflateInit(state);
		if (rv != Z_OK)
			errx(1, "inflateInit: %s", state->msg);
		buf = buf_new(buf_size(stream->rdbuf));
		zf->rdbuf = stream->rdbuf;
		stream->rdbuf = buf;
		zf->rdstate = state;
	}
	if (stream->wrbuf != NULL) {
		state = xmalloc(sizeof(z_stream));
		state->zalloc = zfilter_alloc;
		state->zfree = zfilter_free;
		state->opaque = Z_NULL;
		rv = deflateInit(state, Z_DEFAULT_COMPRESSION);
		if (rv != Z_OK)
			errx(1, "deflateInit: %s", state->msg);
		buf = buf_new(buf_size(stream->wrbuf));
		zf->wrbuf = stream->wrbuf;
		stream->wrbuf = buf;
		zf->wrstate = state;
	}
	stream->fdata = zf;
	return (0);
}

static void
zfilter_fini(struct stream *stream)
{
	struct zfilter *zf;
	struct buf *zbuf;
	z_stream *state;
	ssize_t n;

	zf = stream->fdata;
	if (zf->rdbuf != NULL) {
		state = zf->rdstate;
		zbuf = zf->rdbuf;
		/*
		 * Even if it has produced all the bytes, zlib sometimes
		 * hasn't seen the EOF marker, so we need to call inflate()
		 * again to make sure we have eaten all the zlib'ed bytes.
		 */
		if ((zf->flags & ZFILTER_EOF) == 0) {
			n = zfilter_fill(stream, stream->rdbuf);
			assert(n == 0 && zf->flags & ZFILTER_EOF);
		}
		inflateEnd(state);
		free(state);
		buf_free(stream->rdbuf);
		stream->rdbuf = zbuf;
	}
	if (zf->wrbuf != NULL) {
		state = zf->wrstate;
		zbuf = zf->wrbuf;
		/*
		 * Compress the remaining bytes in the buffer, if any,
		 * and emit an EOF marker as appropriate.  We ignore
		 * the error because we can't do anything about it at
		 * this point, and it can happen if we're getting
		 * disconnected.
		 */
		(void)zfilter_flush(stream, stream->wrbuf,
		    STREAM_FLUSH_CLOSING);
		deflateEnd(state);
		free(state);
		buf_free(stream->wrbuf);
		stream->wrbuf = zbuf;
	}
	free(zf);
}

static int
zfilter_flush(struct stream *stream, struct buf *buf, stream_flush_t how)
{
	struct zfilter *zf;
	struct buf *zbuf;
	z_stream *state;
	size_t lastin, lastout, ate, prod;
	int done, error, flags, rv;

	zf = stream->fdata;
	state = zf->wrstate;
	zbuf = zf->wrbuf;

	if (how == STREAM_FLUSH_NORMAL)
		flags = Z_SYNC_FLUSH;
	else
		flags = Z_FINISH;

	done = 0;
	rv = Z_OK;

again:
	/*
	 * According to zlib.h, we should have at least 6 bytes
	 * available when using deflate() with Z_SYNC_FLUSH.
	 */
	if ((buf_avail(zbuf) < 6 && flags == Z_SYNC_FLUSH) ||
	    rv == Z_BUF_ERROR || buf_avail(buf) == 0) {
		error = stream_flush_default(stream, zbuf, how);
		if (error)
			return (error);
	}

	state->next_in = (Bytef *)(buf->buf + buf->off);
	state->avail_in = buf_count(buf);
	state->next_out = (Bytef *)(zbuf->buf + zbuf->off + zbuf->in);
	state->avail_out = buf_avail(zbuf);
	lastin = state->avail_in;
	lastout = state->avail_out;
	rv = deflate(state, flags);
	if (rv != Z_BUF_ERROR && rv != Z_OK && rv != Z_STREAM_END)
		errx(1, "deflate: %s", state->msg);
	ate = lastin - state->avail_in;
	prod = lastout - state->avail_out;
	buf_less(buf, ate);
	buf_more(zbuf, prod);
	if ((flags == Z_SYNC_FLUSH && buf_count(buf) > 0) ||
	    (flags == Z_FINISH && rv != Z_STREAM_END) ||
	    (rv == Z_BUF_ERROR))
		goto again;

	assert(rv == Z_OK || (rv == Z_STREAM_END && flags == Z_FINISH));
	error = stream_flush_default(stream, zbuf, how);
	return (error);
}

static ssize_t
zfilter_fill(struct stream *stream, struct buf *buf)
{
	struct zfilter *zf;
	struct buf *zbuf;
	z_stream *state;
	size_t lastin, lastout, new;
	ssize_t n;
	int rv;

	zf = stream->fdata;
	state = zf->rdstate;
	zbuf = zf->rdbuf;

	assert(buf_avail(buf) > 0);
	if (buf_count(zbuf) == 0) {
		n = stream_fill_default(stream, zbuf);
		if (n <= 0)
			return (n);
	}
again:
	assert(buf_count(zbuf) > 0);
	state->next_in = (Bytef *)(zbuf->buf + zbuf->off);
	state->avail_in = buf_count(zbuf);
	state->next_out = (Bytef *)(buf->buf + buf->off + buf->in);
	state->avail_out = buf_avail(buf);
	lastin = state->avail_in;
	lastout = state->avail_out;
	rv = inflate(state, Z_SYNC_FLUSH);
	buf_less(zbuf, lastin - state->avail_in);
	new = lastout - state->avail_out;
	if (new == 0 && rv != Z_STREAM_END) {
		n = stream_fill_default(stream, zbuf);
		if (n == -1)
			return (-1);
		if (n == 0)
			return (0);
		goto again;
	}
	if (rv != Z_STREAM_END && rv != Z_OK)
		errx(1, "inflate: %s", state->msg);
	if (rv == Z_STREAM_END)
		zf->flags |= ZFILTER_EOF;
	buf_more(buf, new);
	return (new);
}

/* The MD5 stream filter implementation. */
static int
md5filter_init(struct stream *stream, void *data)
{
	struct md5filter *mf;

	mf = xmalloc(sizeof(struct md5filter));
	MD5_Init(&mf->ctx);
	mf->md5 = data;
	stream->fdata = mf;
	return (0);
}

static void
md5filter_fini(struct stream *stream)
{
	struct md5filter *mf;

	mf = stream->fdata;
	MD5_End(mf->md5, &mf->ctx);
	free(stream->fdata);
}

static ssize_t
md5filter_fill(struct stream *stream, struct buf *buf)
{
	ssize_t n;

	assert(buf_avail(buf) > 0);
	n = stream_fill_default(stream, buf);
	return (n);
}

static int
md5filter_flush(struct stream *stream, struct buf *buf, stream_flush_t how)
{
	struct md5filter *mf;
	int error;

	mf = stream->fdata;
	MD5_Update(&mf->ctx, buf->buf + buf->off, buf->in);
	error = stream_flush_default(stream, buf, how);
	return (error);
}
