/*-
 * Copyright (c) 2003-2004 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"

static int	choose_decompressor(struct archive *, const void*, size_t);
static int	choose_format(struct archive *);

/*
 * Allocate, initialize and return a struct archive object.
 */
struct archive *
archive_read_new(void)
{
	struct archive	*a;
	unsigned char	*nulls;

	a = malloc(sizeof(*a));
	if (a == NULL) {
		archive_set_error(a, ENOMEM, "Can't allocate archive object");
		return (NULL);
	}
	memset(a, 0, sizeof(*a));

	a->user_uid = geteuid();
	a->magic = ARCHIVE_READ_MAGIC;
	a->bytes_per_block = ARCHIVE_DEFAULT_BYTES_PER_BLOCK;

	a->null_length = 1024;
	nulls = malloc(a->null_length);
	if (nulls == NULL) {
		archive_set_error(a, ENOMEM, "Can't allocate archive object 'nulls' element");
		free(a);
		return (NULL);
	}
	memset(nulls, 0, a->null_length);
	a->nulls = nulls;

	a->state = ARCHIVE_STATE_NEW;
	a->entry = archive_entry_new();

	/* We always support uncompressed archives. */
	archive_read_support_compression_none((struct archive*)a);

	return (a);
}

/*
 * Set the block size.
 */
/*
int
archive_read_set_bytes_per_block(struct archive *a, int bytes_per_block)
{
	__archive_check_magic(a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW, "archive_read_set_bytes_per_block");
	if (bytes_per_block < 1)
		bytes_per_block = 1;
	a->bytes_per_block = bytes_per_block;
	return (0);
}
*/

/*
 * Open the archive
 */
int
archive_read_open(struct archive *a, void *client_data,
    archive_open_callback *client_opener, archive_read_callback *client_reader,
    archive_close_callback *client_closer)
{
	const void *buffer;
	ssize_t bytes_read;
	int high_bidder;
	int e;

	__archive_check_magic(a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW, "archive_read_open");

	if (client_reader == NULL)
		__archive_errx(1,
		    "No reader function provided to archive_read_open");

	/*
	 * Set these NULL initially.  If the open or initial read fails,
	 * we'll leave them NULL to indicate that the file is invalid.
	 * (In particular, this helps ensure that the closer doesn't
	 * get called more than once.)
	 */
	a->client_opener = NULL;
	a->client_reader = NULL;
	a->client_closer = NULL;
	a->client_data = NULL;

	/* Open data source. */
	if (client_opener != NULL) {
		e =(client_opener)(a, client_data);
		if (e != 0) {
			/* If the open failed, call the closer to clean up. */
			if (client_closer)
				(client_closer)(a, client_data);
			return (e);
		}
	}

	/* Read first block now for format detection. */
	bytes_read = (client_reader)(a, client_data, &buffer);

	if (bytes_read < 0) {
		/* If the first read fails, close before returning error. */
		if (client_closer)
			(client_closer)(a, client_data);
		/* client_reader should have already set error information. */
		return (ARCHIVE_FATAL);
	}

	/* An empty archive is a serious error. */
	if (bytes_read == 0) {
		archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Empty input file");
		/* Close the empty file. */
		if (client_closer)
			(client_closer)(a, client_data);
		return (ARCHIVE_FATAL);
	}

	/* Now that the client callbacks have worked, remember them. */
	a->client_opener = client_opener; /* Do we need to remember this? */
	a->client_reader = client_reader;
	a->client_closer = client_closer;
	a->client_data = client_data;

	/* Select a decompression routine. */
	high_bidder = choose_decompressor(a, buffer, bytes_read);
	if (high_bidder < 0)
		return (ARCHIVE_FATAL);

	/* Initialize decompression routine with the first block of data. */
	e = (a->decompressors[high_bidder].init)(a, buffer, bytes_read);

	if (e == ARCHIVE_OK)
		a->state = ARCHIVE_STATE_HEADER;

	return (e);
}

/*
 * Allow each registered decompression routine to bid on whether it
 * wants to handle this stream.  Return index of winning bidder.
 */
static int
choose_decompressor(struct archive *a, const void *buffer, size_t bytes_read)
{
	int decompression_slots, i, bid, best_bid, best_bid_slot;

	decompression_slots = sizeof(a->decompressors) /
	    sizeof(a->decompressors[0]);

	best_bid = -1;
	best_bid_slot = -1;

	for (i = 0; i < decompression_slots; i++) {
		if (a->decompressors[i].bid) {
			bid = (a->decompressors[i].bid)(buffer, bytes_read);
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
		__archive_errx(1, "No decompressors were registered; you "
		    "must call at least one "
		    "archive_read_support_compression_XXX function in order "
		    "to successfully read an archive.");

	/*
	 * There were bidders, but no non-zero bids; this means we  can't
	 * support this stream.
	 */
	if (best_bid < 1) {
		archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Unrecognized archive format");
		return (ARCHIVE_FATAL);
	}

	return (best_bid_slot);
}

/*
 * Read header of next entry.
 */
int
archive_read_next_header(struct archive *a, struct archive_entry **entryp)
{
	struct archive_entry *entry;
	int slot, ret;

	__archive_check_magic(a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_HEADER | ARCHIVE_STATE_DATA, "archive_read_next_header");

	*entryp = NULL;
	entry = a->entry;
	archive_entry_clear(entry);
	archive_string_empty(&a->error_string);

	/*
	 * If client didn't consume entire data, skip any remainder
	 * (This is especially important for GNU incremental directories.)
	 */
	if (a->state == ARCHIVE_STATE_DATA) {
		ret = archive_read_data_skip(a);
		if (ret == ARCHIVE_EOF) {
			archive_set_error(a, EIO, "Premature end-of-file.");
			a->state = ARCHIVE_STATE_FATAL;
			return (ARCHIVE_FATAL);
		}
		if (ret != ARCHIVE_OK)
			return (ret);
	}

	/* Record start-of-header. */
	a->header_position = a->file_position;

	slot = choose_format(a);
	if (slot < 0) {
		a->state = ARCHIVE_STATE_FATAL;
		return (ARCHIVE_FATAL);
	}
	a->format = &(a->formats[slot]);
	a->pformat_data = &(a->format->format_data);
	ret = (a->format->read_header)(a, entry);

	/*
	 * EOF and FATAL are persistent at this layer.  By
	 * modifying the state, we gaurantee that future calls to
	 * read a header or read data will fail.
	 */
	switch (ret) {
	case ARCHIVE_EOF:
		a->state = ARCHIVE_STATE_EOF;
		break;
	case ARCHIVE_OK:
		a->state = ARCHIVE_STATE_DATA;
		break;
	case ARCHIVE_WARN:
		a->state = ARCHIVE_STATE_DATA;
		break;
	case ARCHIVE_RETRY:
		break;
	case ARCHIVE_FATAL:
		a->state = ARCHIVE_STATE_FATAL;
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
choose_format(struct archive *a)
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
			a->pformat_data = &(a->format->format_data);
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
		archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
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
archive_read_header_position(struct archive *a)
{
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
archive_read_data(struct archive *a, void *buff, size_t s)
{
	char	*dest;
	size_t	 bytes_read;
	size_t	 len;
	int	 r;

	bytes_read = 0;
	dest = buff;

	while (s > 0) {
		if (a->read_data_remaining <= 0) {
			r = archive_read_data_block(a,
			    (const void **)&a->read_data_block,
			    &a->read_data_remaining,
			    &a->read_data_offset);
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
			archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
			    "Encountered out-of-order sparse blocks");
			return (ARCHIVE_RETRY);
		} else {
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

/*
 * Skip over all remaining data in this entry.
 */
int
archive_read_data_skip(struct archive *a)
{
	int r;
	const void *buff;
	size_t size;
	off_t offset;

	__archive_check_magic(a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_DATA, "archive_read_data_skip");

	if (a->format->read_data_skip != NULL)
		r = (a->format->read_data_skip)(a);
	else {
		while ((r = archive_read_data_block(a, &buff, &size, &offset))
		    == ARCHIVE_OK)
			;
	}

	if (r == ARCHIVE_EOF)
		r = ARCHIVE_OK;

	a->state = ARCHIVE_STATE_HEADER;
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
archive_read_data_block(struct archive *a,
    const void **buff, size_t *size, off_t *offset)
{
	__archive_check_magic(a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_DATA, "archive_read_data_block");

	if (a->format->read_data == NULL) {
		archive_set_error(a, ARCHIVE_ERRNO_PROGRAMMER,
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
archive_read_close(struct archive *a)
{
	__archive_check_magic(a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_ANY, "archive_read_close");
	a->state = ARCHIVE_STATE_CLOSED;

	/* Call cleanup functions registered by optional components. */
	if (a->cleanup_archive_extract != NULL)
		(a->cleanup_archive_extract)(a);

	/* TODO: Finish the format processing. */

	/* Close the input machinery. */
	if (a->compression_finish != NULL)
		(a->compression_finish)(a);
	return (ARCHIVE_OK);
}

/*
 * Release memory and other resources.
 */
void
archive_read_finish(struct archive *a)
{
	int i;
	int slots;

	__archive_check_magic(a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_ANY, "archive_read_finish");
	if (a->state != ARCHIVE_STATE_CLOSED)
		archive_read_close(a);

	/* Cleanup format-specific data. */
	slots = sizeof(a->formats) / sizeof(a->formats[0]);
	for (i = 0; i < slots; i++) {
		a->pformat_data = &(a->formats[i].format_data);
		if (a->formats[i].cleanup)
			(a->formats[i].cleanup)(a);
	}

	/* Casting a pointer to int allows us to remove 'const.' */
	free((void *)(uintptr_t)(const void *)a->nulls);
	archive_string_free(&a->error_string);
	if (a->entry)
		archive_entry_free(a->entry);
	a->magic = 0;
	free(a);
}

/*
 * Used internally by read format handlers to register their bid and
 * initialization functions.
 */
int
__archive_read_register_format(struct archive *a,
    void *format_data,
    int (*bid)(struct archive *),
    int (*read_header)(struct archive *, struct archive_entry *),
    int (*read_data)(struct archive *, const void **, size_t *, off_t *),
    int (*read_data_skip)(struct archive *),
    int (*cleanup)(struct archive *))
{
	int i, number_slots;

	__archive_check_magic(a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW, "__archive_read_register_format");

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
			a->formats[i].format_data = format_data;
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
int
__archive_read_register_compression(struct archive *a,
    int (*bid)(const void *, size_t),
    int (*init)(struct archive *, const void *, size_t))
{
	int i, number_slots;

	__archive_check_magic(a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW, "__archive_read_register_compression");

	number_slots = sizeof(a->decompressors) / sizeof(a->decompressors[0]);

	for (i = 0; i < number_slots; i++) {
		if (a->decompressors[i].bid == bid)
			return (ARCHIVE_OK); /* We've already installed */
		if (a->decompressors[i].bid == NULL) {
			a->decompressors[i].bid = bid;
			a->decompressors[i].init = init;
			return (ARCHIVE_OK);
		}
	}

	__archive_errx(1, "Not enough slots for compression registration");
	return (ARCHIVE_FATAL); /* Never actually executed. */
}
