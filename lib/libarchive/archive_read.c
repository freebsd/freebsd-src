/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file contains the "essential" portions of the read API, that
 * is, stuff that will probably always be used by any client that
 * actually needs to read an archive.  Optional pieces have been, as
 * far as possible, separated out into separate files to avoid
 * needlessly bloating statically-linked clients.
 */

#include "archive_platform.h"
__FBSDID("$FreeBSD$");

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_private.h"

#define minimum(a, b) (a < b ? a : b)

static int	build_stream(struct archive_read *);
static int	choose_format(struct archive_read *);

/*
 * Allocate, initialize and return a struct archive object.
 */
struct archive *
archive_read_new(void)
{
	struct archive_read *a;

	a = (struct archive_read *)malloc(sizeof(*a));
	if (a == NULL)
		return (NULL);
	memset(a, 0, sizeof(*a));
	a->archive.magic = ARCHIVE_READ_MAGIC;

	a->archive.state = ARCHIVE_STATE_NEW;
	a->entry = archive_entry_new();

	/* Initialize reblocking logic. */
	a->buffer_size = 64 * 1024; /* 64k */
	a->buffer = (char *)malloc(a->buffer_size);
	a->next = a->buffer;
	if (a->buffer == NULL) {
		archive_entry_free(a->entry);
		free(a);
		return (NULL);
	}

	return (&a->archive);
}

/*
 * Record the do-not-extract-to file. This belongs in archive_read_extract.c.
 */
void
archive_read_extract_set_skip_file(struct archive *_a, dev_t d, ino_t i)
{
	struct archive_read *a = (struct archive_read *)_a;
	__archive_check_magic(_a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_ANY,
	    "archive_read_extract_set_skip_file");
	a->skip_file_dev = d;
	a->skip_file_ino = i;
}


/*
 * Open the archive
 */
int
archive_read_open(struct archive *a, void *client_data,
    archive_open_callback *client_opener, archive_read_callback *client_reader,
    archive_close_callback *client_closer)
{
	/* Old archive_read_open() is just a thin shell around
	 * archive_read_open2. */
	return archive_read_open2(a, client_data, client_opener,
	    client_reader, NULL, client_closer);
}

static ssize_t
client_read_proxy(struct archive_read_source *self, const void **buff)
{
	return (self->archive->client.reader)((struct archive *)self->archive,
	    self->data, buff);
}

static int64_t
client_skip_proxy(struct archive_read_source *self, int64_t request)
{
	return (self->archive->client.skipper)((struct archive *)self->archive,
	    self->data, request);
}

static int
client_close_proxy(struct archive_read_source *self)
{
	int r = ARCHIVE_OK;

	if (self->archive->client.closer != NULL)
		r = (self->archive->client.closer)((struct archive *)self->archive,
		    self->data);
	free(self);
	return (r);
}


int
archive_read_open2(struct archive *_a, void *client_data,
    archive_open_callback *client_opener,
    archive_read_callback *client_reader,
    archive_skip_callback *client_skipper,
    archive_close_callback *client_closer)
{
	struct archive_read *a = (struct archive_read *)_a;
	int e;

	__archive_check_magic(_a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW,
	    "archive_read_open");

	if (client_reader == NULL)
		__archive_errx(1,
		    "No reader function provided to archive_read_open");

	/* Open data source. */
	if (client_opener != NULL) {
		e =(client_opener)(&a->archive, client_data);
		if (e != 0) {
			/* If the open failed, call the closer to clean up. */
			if (client_closer)
				(client_closer)(&a->archive, client_data);
			return (e);
		}
	}

	/* Save the client functions and mock up the initial source. */
	a->client.opener = client_opener; /* Do we need to remember this? */
	a->client.reader = client_reader;
	a->client.skipper = client_skipper;
	a->client.closer = client_closer;
	a->client.data = client_data;

	{
		struct archive_read_source *source;

		source = calloc(1, sizeof(*source));
		if (source == NULL)
			return (ARCHIVE_FATAL);
		source->reader = NULL;
		source->upstream = NULL;
		source->archive = a;
		source->data = client_data;
		source->read = client_read_proxy;
		source->skip = client_skip_proxy;
		source->close = client_close_proxy;
		a->source = source;
	}

	/* In case there's no filter. */
	a->archive.compression_code = ARCHIVE_COMPRESSION_NONE;
	a->archive.compression_name = "none";

	/* Build out the input pipeline. */
	e = build_stream(a);
	if (e == ARCHIVE_OK)
		a->archive.state = ARCHIVE_STATE_HEADER;

	return (e);
}

/*
 * Allow each registered stream transform to bid on whether
 * it wants to handle this stream.  Repeat until we've finished
 * building the pipeline.
 */
static int
build_stream(struct archive_read *a)
{
	int number_readers, i, bid, best_bid;
	struct archive_reader *reader, *best_reader;
	struct archive_read_source *source;
	const void *block;
	ssize_t bytes_read;

	/* Read first block now for compress format detection. */
	bytes_read = (a->source->read)(a->source, &block);
	if (bytes_read < 0) {
		/* If the first read fails, close before returning error. */
		if (a->source->close != NULL) {
			(a->source->close)(a->source);
			a->source = NULL;
		}
		/* source->read should have already set error information. */
		return (ARCHIVE_FATAL);
	}

	number_readers = sizeof(a->readers) / sizeof(a->readers[0]);

	best_bid = 0;
	best_reader = NULL;

	reader = a->readers;
	for (i = 0, reader = a->readers; i < number_readers; i++, reader++) {
		if (reader->bid != NULL) {
			bid = (reader->bid)(reader, block, bytes_read);
			if (bid > best_bid) {
				best_bid = bid;
				best_reader = reader;
			}
		}
	}

	/*
	 * If we have a winner, it becomes the next stage in the pipeline.
	 */
	if (best_reader != NULL) {
		source = (best_reader->init)(a, best_reader, a->source,
		    block, bytes_read);
		if (source == NULL)
			return (ARCHIVE_FATAL);
		/* Record the best decompressor for this stream. */
		a->source = source;
		/* Recurse to get next pipeline stage. */
		return (build_stream(a));
	}

	/* Save first block of data. */
	a->client_buff = block;
	a->client_total = bytes_read;
	a->client_next = a->client_buff;
	a->client_avail = a->client_total;
	return (ARCHIVE_OK);
}

/*
 * Read header of next entry.
 */
int
archive_read_next_header(struct archive *_a, struct archive_entry **entryp)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct archive_entry *entry;
	int slot, ret;

	__archive_check_magic(_a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_HEADER | ARCHIVE_STATE_DATA,
	    "archive_read_next_header");

	*entryp = NULL;
	entry = a->entry;
	archive_entry_clear(entry);
	archive_clear_error(&a->archive);

	/*
	 * If no format has yet been chosen, choose one.
	 */
	if (a->format == NULL) {
		slot = choose_format(a);
		if (slot < 0) {
			a->archive.state = ARCHIVE_STATE_FATAL;
			return (ARCHIVE_FATAL);
		}
		a->format = &(a->formats[slot]);
	}

	/*
	 * If client didn't consume entire data, skip any remainder
	 * (This is especially important for GNU incremental directories.)
	 */
	if (a->archive.state == ARCHIVE_STATE_DATA) {
		ret = archive_read_data_skip(&a->archive);
		if (ret == ARCHIVE_EOF) {
			archive_set_error(&a->archive, EIO, "Premature end-of-file.");
			a->archive.state = ARCHIVE_STATE_FATAL;
			return (ARCHIVE_FATAL);
		}
		if (ret != ARCHIVE_OK)
			return (ret);
	}

	/* Record start-of-header. */
	a->header_position = a->archive.file_position;

	ret = (a->format->read_header)(a, entry);

	/*
	 * EOF and FATAL are persistent at this layer.  By
	 * modifying the state, we guarantee that future calls to
	 * read a header or read data will fail.
	 */
	switch (ret) {
	case ARCHIVE_EOF:
		a->archive.state = ARCHIVE_STATE_EOF;
		break;
	case ARCHIVE_OK:
		a->archive.state = ARCHIVE_STATE_DATA;
		break;
	case ARCHIVE_WARN:
		a->archive.state = ARCHIVE_STATE_DATA;
		break;
	case ARCHIVE_RETRY:
		break;
	case ARCHIVE_FATAL:
		a->archive.state = ARCHIVE_STATE_FATAL;
		break;
	}

	*entryp = entry;
	a->read_data_output_offset = 0;
	a->read_data_remaining = 0;
	return (ret);
}

/*
 * Allow each registered format to bid on whether it wants to handle
 * the next entry.  Return index of winning bidder.
 */
static int
choose_format(struct archive_read *a)
{
	int slots;
	int i;
	int bid, best_bid;
	int best_bid_slot;

	slots = sizeof(a->formats) / sizeof(a->formats[0]);
	best_bid = -1;
	best_bid_slot = -1;

	/* Set up a->format and a->pformat_data for convenience of bidders. */
	a->format = &(a->formats[0]);
	for (i = 0; i < slots; i++, a->format++) {
		if (a->format->bid) {
			bid = (a->format->bid)(a);
			if (bid == ARCHIVE_FATAL)
				return (ARCHIVE_FATAL);
			if ((bid > best_bid) || (best_bid_slot < 0)) {
				best_bid = bid;
				best_bid_slot = i;
			}
		}
	}

	/*
	 * There were no bidders; this is a serious programmer error
	 * and demands a quick and definitive abort.
	 */
	if (best_bid_slot < 0)
		__archive_errx(1, "No formats were registered; you must "
		    "invoke at least one archive_read_support_format_XXX "
		    "function in order to successfully read an archive.");

	/*
	 * There were bidders, but no non-zero bids; this means we
	 * can't support this stream.
	 */
	if (best_bid < 1) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Unrecognized archive format");
		return (ARCHIVE_FATAL);
	}

	return (best_bid_slot);
}

/*
 * Return the file offset (within the uncompressed data stream) where
 * the last header started.
 */
int64_t
archive_read_header_position(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	__archive_check_magic(_a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_read_header_position");
	return (a->header_position);
}

/*
 * Read data from an archive entry, using a read(2)-style interface.
 * This is a convenience routine that just calls
 * archive_read_data_block and copies the results into the client
 * buffer, filling any gaps with zero bytes.  Clients using this
 * API can be completely ignorant of sparse-file issues; sparse files
 * will simply be padded with nulls.
 *
 * DO NOT intermingle calls to this function and archive_read_data_block
 * to read a single entry body.
 */
ssize_t
archive_read_data(struct archive *_a, void *buff, size_t s)
{
	struct archive_read *a = (struct archive_read *)_a;
	char	*dest;
	const void *read_buf;
	size_t	 bytes_read;
	size_t	 len;
	int	 r;

	bytes_read = 0;
	dest = (char *)buff;

	while (s > 0) {
		if (a->read_data_remaining == 0) {
			read_buf = a->read_data_block;
			r = archive_read_data_block(&a->archive, &read_buf,
			    &a->read_data_remaining, &a->read_data_offset);
			a->read_data_block = read_buf;
			if (r == ARCHIVE_EOF)
				return (bytes_read);
			/*
			 * Error codes are all negative, so the status
			 * return here cannot be confused with a valid
			 * byte count.  (ARCHIVE_OK is zero.)
			 */
			if (r < ARCHIVE_OK)
				return (r);
		}

		if (a->read_data_offset < a->read_data_output_offset) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_FILE_FORMAT,
			    "Encountered out-of-order sparse blocks");
			return (ARCHIVE_RETRY);
		}

		/* Compute the amount of zero padding needed. */
		if (a->read_data_output_offset + (off_t)s <
		    a->read_data_offset) {
			len = s;
		} else if (a->read_data_output_offset <
		    a->read_data_offset) {
			len = a->read_data_offset -
			    a->read_data_output_offset;
		} else
			len = 0;

		/* Add zeroes. */
		memset(dest, 0, len);
		s -= len;
		a->read_data_output_offset += len;
		dest += len;
		bytes_read += len;

		/* Copy data if there is any space left. */
		if (s > 0) {
			len = a->read_data_remaining;
			if (len > s)
				len = s;
			memcpy(dest, a->read_data_block, len);
			s -= len;
			a->read_data_block += len;
			a->read_data_remaining -= len;
			a->read_data_output_offset += len;
			a->read_data_offset += len;
			dest += len;
			bytes_read += len;
		}
	}
	return (bytes_read);
}

#if ARCHIVE_API_VERSION < 3
/*
 * Obsolete function provided for compatibility only.  Note that the API
 * of this function doesn't allow the caller to detect if the remaining
 * data from the archive entry is shorter than the buffer provided, or
 * even if an error occurred while reading data.
 */
int
archive_read_data_into_buffer(struct archive *a, void *d, ssize_t len)
{

	archive_read_data(a, d, len);
	return (ARCHIVE_OK);
}
#endif

/*
 * Skip over all remaining data in this entry.
 */
int
archive_read_data_skip(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	int r;
	const void *buff;
	size_t size;
	off_t offset;

	__archive_check_magic(_a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_DATA,
	    "archive_read_data_skip");

	if (a->format->read_data_skip != NULL)
		r = (a->format->read_data_skip)(a);
	else {
		while ((r = archive_read_data_block(&a->archive,
			    &buff, &size, &offset))
		    == ARCHIVE_OK)
			;
	}

	if (r == ARCHIVE_EOF)
		r = ARCHIVE_OK;

	a->archive.state = ARCHIVE_STATE_HEADER;
	return (r);
}

/*
 * Read the next block of entry data from the archive.
 * This is a zero-copy interface; the client receives a pointer,
 * size, and file offset of the next available block of data.
 *
 * Returns ARCHIVE_OK if the operation is successful, ARCHIVE_EOF if
 * the end of entry is encountered.
 */
int
archive_read_data_block(struct archive *_a,
    const void **buff, size_t *size, off_t *offset)
{
	struct archive_read *a = (struct archive_read *)_a;
	__archive_check_magic(_a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_DATA,
	    "archive_read_data_block");

	if (a->format->read_data == NULL) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_PROGRAMMER,
		    "Internal error: "
		    "No format_read_data_block function registered");
		return (ARCHIVE_FATAL);
	}

	return (a->format->read_data)(a, buff, size, offset);
}

/*
 * Close the file and release most resources.
 *
 * Be careful: client might just call read_new and then read_finish.
 * Don't assume we actually read anything or performed any non-trivial
 * initialization.
 */
int
archive_read_close(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	int r = ARCHIVE_OK, r1 = ARCHIVE_OK;
	size_t i, n;

	__archive_check_magic(&a->archive, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_ANY, "archive_read_close");
	a->archive.state = ARCHIVE_STATE_CLOSED;

	/* Call cleanup functions registered by optional components. */
	if (a->cleanup_archive_extract != NULL)
		r = (a->cleanup_archive_extract)(a);

	/* TODO: Clean up the formatters. */

	/* Clean up the stream pipeline. */
	if (a->source != NULL) {
		r1 = (a->source->close)(a->source);
		if (r1 < r)
			r = r1;
		a->source = NULL;
	}

	/* Release the reader objects. */
	n = sizeof(a->readers)/sizeof(a->readers[0]);
	for (i = 0; i < n; i++) {
		if (a->readers[i].free != NULL) {
			r1 = (a->readers[i].free)(&a->readers[i]);
			if (r1 < r)
				r = r1;
		}
	}

	return (r);
}

/*
 * Release memory and other resources.
 */
#if ARCHIVE_API_VERSION > 1
int
#else
/* Temporarily allow library to compile with either 1.x or 2.0 API. */
void
#endif
archive_read_finish(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	int i;
	int slots;
	int r = ARCHIVE_OK;

	__archive_check_magic(_a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_ANY,
	    "archive_read_finish");
	if (a->archive.state != ARCHIVE_STATE_CLOSED)
		r = archive_read_close(&a->archive);

	/* Cleanup format-specific data. */
	slots = sizeof(a->formats) / sizeof(a->formats[0]);
	for (i = 0; i < slots; i++) {
		a->format = &(a->formats[i]);
		if (a->formats[i].cleanup)
			(a->formats[i].cleanup)(a);
	}

	archive_string_free(&a->archive.error_string);
	if (a->entry)
		archive_entry_free(a->entry);
	a->archive.magic = 0;
	free(a->buffer);
	free(a);
#if ARCHIVE_API_VERSION > 1
	return (r);
#endif
}

/*
 * Used internally by read format handlers to register their bid and
 * initialization functions.
 */
int
__archive_read_register_format(struct archive_read *a,
    void *format_data,
    int (*bid)(struct archive_read *),
    int (*read_header)(struct archive_read *, struct archive_entry *),
    int (*read_data)(struct archive_read *, const void **, size_t *, off_t *),
    int (*read_data_skip)(struct archive_read *),
    int (*cleanup)(struct archive_read *))
{
	int i, number_slots;

	__archive_check_magic(&a->archive,
	    ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW,
	    "__archive_read_register_format");

	number_slots = sizeof(a->formats) / sizeof(a->formats[0]);

	for (i = 0; i < number_slots; i++) {
		if (a->formats[i].bid == bid)
			return (ARCHIVE_WARN); /* We've already installed */
		if (a->formats[i].bid == NULL) {
			a->formats[i].bid = bid;
			a->formats[i].read_header = read_header;
			a->formats[i].read_data = read_data;
			a->formats[i].read_data_skip = read_data_skip;
			a->formats[i].cleanup = cleanup;
			a->formats[i].data = format_data;
			return (ARCHIVE_OK);
		}
	}

	__archive_errx(1, "Not enough slots for format registration");
	return (ARCHIVE_FATAL); /* Never actually called. */
}

/*
 * Used internally by decompression routines to register their bid and
 * initialization functions.
 */
struct archive_reader *
__archive_read_get_reader(struct archive_read *a)
{
	int i, number_slots;

	__archive_check_magic(&a->archive,
	    ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW,
	    "__archive_read_get_reader");

	number_slots = sizeof(a->readers) / sizeof(a->readers[0]);

	for (i = 0; i < number_slots; i++) {
		if (a->readers[i].bid == NULL)
			return (a->readers + i);
	}

	__archive_errx(1, "Not enough slots for compression registration");
	return (NULL); /* Never actually executed. */
}

/*
 * The next three functions comprise the peek/consume internal I/O
 * system used by archive format readers.  This system allows fairly
 * flexible read-ahead and allows the I/O code to operate in a
 * zero-copy manner most of the time.
 *
 * In the ideal case, block providers give the I/O code blocks of data
 * and __archive_read_ahead() just returns pointers directly into
 * those blocks.  Then __archive_read_consume() just bumps those
 * pointers.  Only if your request would span blocks does the I/O
 * layer use a copy buffer to provide you with a contiguous block of
 * data.  The __archive_read_skip() is an optimization; it scans ahead
 * very quickly (it usually translates into a seek() operation if
 * you're reading uncompressed disk files).
 *
 * A couple of useful idioms:
 *  * "I just want some data."  Ask for 1 byte and pay attention to
 *    the "number of bytes available" from __archive_read_ahead().
 *    You can consume more than you asked for; you just can't consume
 *    more than is available right now.  If you consume everything that's
 *    immediately available, the next read_ahead() call will pull
 *    the next block.
 *  * "I want to output a large block of data."  As above, ask for 1 byte,
 *    emit all that's available (up to whatever limit you have), then
 *    repeat until you're done.
 *  * "I want to peek ahead by a large amount."  Ask for 4k or so, then
 *    double and repeat until you get an error or have enough.  Note
 *    that the I/O layer will likely end up expanding its copy buffer
 *    to fit your request, so use this technique cautiously.  This
 *    technique is used, for example, by some of the format tasting
 *    code that has uncertain look-ahead needs.
 *
 * TODO: Someday, provide a more generic __archive_read_seek() for
 * those cases where it's useful.  This is tricky because there are lots
 * of cases where seek() is not available (reading gzip data from a
 * network socket, for instance), so there needs to be a good way to
 * communicate whether seek() is available and users of that interface
 * need to use non-seeking strategies whenever seek() is not available.
 */

/*
 * Looks ahead in the input stream:
 *  * If 'avail' pointer is provided, that returns number of bytes available
 *    in the current buffer, which may be much larger than requested.
 *  * If end-of-file, *avail gets set to zero.
 *  * If error, *avail gets error code.
 *  * If request can be met, returns pointer to data, returns NULL
 *    if request is not met.
 *
 * Note: If you just want "some data", ask for 1 byte and pay attention
 * to *avail, which will have the actual amount available.  If you
 * know exactly how many bytes you need, just ask for that and treat
 * a NULL return as an error.
 *
 * Important:  This does NOT move the file pointer.  See
 * __archive_read_consume() below.
 */

/*
 * This is tricky.  We need to provide our clients with pointers to
 * contiguous blocks of memory but we want to avoid copying whenever
 * possible.
 *
 * Mostly, this code returns pointers directly into the block of data
 * provided by the client_read routine.  It can do this unless the
 * request would split across blocks.  In that case, we have to copy
 * into an internal buffer to combine reads.
 */
const void *
__archive_read_ahead(struct archive_read *a, size_t min, ssize_t *avail)
{
	ssize_t bytes_read;
	size_t tocopy;

	if (a->fatal) {
		if (avail)
			*avail = ARCHIVE_FATAL;
		return (NULL);
	}

	/*
	 * Keep pulling more data until we can satisfy the request.
	 */
	for (;;) {

		/*
		 * If we can satisfy from the copy buffer, we're done.
		 */
		if (a->avail >= min) {
			if (avail != NULL)
				*avail = a->avail;
			return (a->next);
		}

		/*
		 * We can satisfy directly from client buffer if everything
		 * currently in the copy buffer is still in the client buffer.
		 */
		if (a->client_total >= a->client_avail + a->avail
		    && a->client_avail + a->avail >= min) {
			/* "Roll back" to client buffer. */
			a->client_avail += a->avail;
			a->client_next -= a->avail;
			/* Copy buffer is now empty. */
			a->avail = 0;
			a->next = a->buffer;
			/* Return data from client buffer. */
			if (avail != NULL)
				*avail = a->client_avail;
			return (a->client_next);
		}

		/* Move data forward in copy buffer if necessary. */
		if (a->next > a->buffer &&
		    a->next + min > a->buffer + a->buffer_size) {
			if (a->avail > 0)
				memmove(a->buffer, a->next, a->avail);
			a->next = a->buffer;
		}

		/* If we've used up the client data, get more. */
		if (a->client_avail <= 0) {
			if (a->end_of_file) {
				if (avail != NULL)
					*avail = 0;
				return (NULL);
			}
			bytes_read = (a->source->read)(a->source,
			    &a->client_buff);
			if (bytes_read < 0) {		/* Read error. */
				a->client_total = a->client_avail = 0;
				a->client_next = a->client_buff = NULL;
				a->fatal = 1;
				if (avail != NULL)
					*avail = ARCHIVE_FATAL;
				return (NULL);
			}
			if (bytes_read == 0) {	/* Premature end-of-file. */
				a->client_total = a->client_avail = 0;
				a->client_next = a->client_buff = NULL;
				a->end_of_file = 1;
				/* Return whatever we do have. */
				if (avail != NULL)
					*avail = a->avail;
				return (NULL);
			}
			a->archive.raw_position += bytes_read;
			a->client_total = bytes_read;
			a->client_avail = a->client_total;
			a->client_next = a->client_buff;
		}
		else
		{
			/*
			 * We can't satisfy the request from the copy
			 * buffer or the existing client data, so we
			 * need to copy more client data over to the
			 * copy buffer.
			 */

			/* Ensure the buffer is big enough. */
			if (min > a->buffer_size) {
				size_t s, t;
				char *p;

				/* Double the buffer; watch for overflow. */
				s = t = a->buffer_size;
				while (s < min) {
					t *= 2;
					if (t <= s) { /* Integer overflow! */
						archive_set_error(&a->archive,
						    ENOMEM,
						    "Unable to allocate copy buffer");
						a->fatal = 1;
						if (avail != NULL)
							*avail = ARCHIVE_FATAL;
						return (NULL);
					}
					s = t;
				}
				/* Now s >= min, so allocate a new buffer. */
				p = (char *)malloc(s);
				if (p == NULL) {
					archive_set_error(&a->archive, ENOMEM,
					    "Unable to allocate copy buffer");
					a->fatal = 1;
					if (avail != NULL)
						*avail = ARCHIVE_FATAL;
					return (NULL);
				}
				/* Move data into newly-enlarged buffer. */
				if (a->avail > 0)
					memmove(p, a->next, a->avail);
				free(a->buffer);
				a->next = a->buffer = p;
				a->buffer_size = s;
			}

			/* We can add client data to copy buffer. */
			/* First estimate: copy to fill rest of buffer. */
			tocopy = (a->buffer + a->buffer_size)
			    - (a->next + a->avail);
			/* Don't waste time buffering more than we need to. */
			if (tocopy + a->avail > min)
				tocopy = min - a->avail;
			/* Don't copy more than is available. */
			if (tocopy > a->client_avail)
				tocopy = a->client_avail;

			memcpy(a->next + a->avail, a->client_next,
			    tocopy);
			/* Remove this data from client buffer. */
			a->client_next += tocopy;
			a->client_avail -= tocopy;
			/* add it to copy buffer. */
			a->avail += tocopy;
		}
	}
}

/*
 * Move the file pointer forward.  This should be called after
 * __archive_read_ahead() returns data to you.  Don't try to move
 * ahead by more than the amount of data available according to
 * __archive_read_ahead().
 */
/*
 * Mark the appropriate data as used.  Note that the request here will
 * often be much smaller than the size of the previous read_ahead
 * request.
 */
ssize_t
__archive_read_consume(struct archive_read *a, size_t request)
{
	if (a->avail > 0) {
		/* Read came from copy buffer. */
		a->next += request;
		a->avail -= request;
	} else {
		/* Read came from client buffer. */
		a->client_next += request;
		a->client_avail -= request;
	}
	a->archive.file_position += request;
	return (request);
}

/*
 * Move the file pointer ahead by an arbitrary amount.  If you're
 * reading uncompressed data from a disk file, this will actually
 * translate into a seek() operation.  Even in cases where seek()
 * isn't feasible, this at least pushes the read-and-discard loop
 * down closer to the data source.
 */
int64_t
__archive_read_skip(struct archive_read *a, int64_t request)
{
	off_t bytes_skipped, total_bytes_skipped = 0;
	size_t min;

	if (a->fatal)
		return (-1);
	/*
	 * If there is data in the buffers already, use that first.
	 */
	if (a->avail > 0) {
		min = minimum(request, (off_t)a->avail);
		bytes_skipped = __archive_read_consume(a, min);
		request -= bytes_skipped;
		total_bytes_skipped += bytes_skipped;
	}
	if (a->client_avail > 0) {
		min = minimum(request, (off_t)a->client_avail);
		bytes_skipped = __archive_read_consume(a, min);
		request -= bytes_skipped;
		total_bytes_skipped += bytes_skipped;
	}
	if (request == 0)
		return (total_bytes_skipped);
	/*
	 * If a client_skipper was provided, try that first.
	 */
#if ARCHIVE_API_VERSION < 2
	if ((a->source->skip != NULL) && (request < SSIZE_MAX)) {
#else
	if (a->source->skip != NULL) {
#endif
		bytes_skipped = (a->source->skip)(a->source, request);
		if (bytes_skipped < 0) {	/* error */
			a->client_total = a->client_avail = 0;
			a->client_next = a->client_buff = NULL;
			a->fatal = 1;
			return (bytes_skipped);
		}
		total_bytes_skipped += bytes_skipped;
		a->archive.file_position += bytes_skipped;
		request -= bytes_skipped;
		a->client_next = a->client_buff;
		a->archive.raw_position += bytes_skipped;
		a->client_avail = a->client_total = 0;
	}
	/*
	 * Note that client_skipper will usually not satisfy the
	 * full request (due to low-level blocking concerns),
	 * so even if client_skipper is provided, we may still
	 * have to use ordinary reads to finish out the request.
	 */
	while (request > 0) {
		const void* dummy_buffer;
		ssize_t bytes_read;
		dummy_buffer = __archive_read_ahead(a, 1, &bytes_read);
		if (bytes_read < 0)
			return (bytes_read);
		if (bytes_read == 0) {
			/* We hit EOF before we satisfied the skip request. */
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "Truncated input file (need to skip %jd bytes)",
			    (intmax_t)request);
			return (ARCHIVE_FATAL);
		}
		min = (size_t)(minimum(bytes_read, request));
		bytes_read = __archive_read_consume(a, min);
		total_bytes_skipped += bytes_read;
		request -= bytes_read;
	}
	return (total_bytes_skipped);
}
