/*-
 * Copyright (c) 2004 Tim Kientzle
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

#include "archive_platform.h"
__FBSDID("$FreeBSD$");

#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"

struct zip {
	off_t			entry_bytes_remaining;
	off_t			entry_offset;

	unsigned		version;
	unsigned		system;
	unsigned		flags;
	unsigned		compression;
	const char *		compression_name;
	time_t			mtime;
	char			end_of_entry;

	long			crc32;
	ssize_t			filename_length;
	ssize_t			extra_length;
	off_t			uncompressed_size;
	off_t			compressed_size;

	unsigned char 		*uncompressed_buffer;
	size_t 			uncompressed_buffer_size;
#ifdef HAVE_ZLIB_H
	z_stream		stream;
#endif

	struct archive_string	pathname;
	struct archive_string	extra;
	char	format_name[64];
};

#define ZIP_LENGTH_AT_END	8

struct zip_file_header {
	char	signature[4];
	char	version[1];
	char	reserved[1];
	char	flags[2];
	char	compression[2];
	char	timedate[4];
	char	crc32[4];
	char	compressed_size[4];
	char	uncompressed_size[4];
	char	filename_length[2];
	char	extra_length[2];
};

const char *compression_names[] = {
	"uncompressed",
	"shrinking",
	"reduced-1",
	"reduced-2",
	"reduced-3",
	"reduced-4",
	"imploded",
	"reserved",
	"deflation"
};

static int	archive_read_format_zip_bid(struct archive *);
static int	archive_read_format_zip_cleanup(struct archive *);
static int	archive_read_format_zip_read_data(struct archive *,
		    const void **, size_t *, off_t *);
static int	archive_read_format_zip_read_header(struct archive *,
		    struct archive_entry *);
static int	i2(const char *);
static int	i4(const char *);
static int	zip_read_data_deflate(struct archive *a, const void **buff,
		    size_t *size, off_t *offset);
static int	zip_read_data_none(struct archive *a, const void **buff,
		    size_t *size, off_t *offset);
static int	zip_read_data_skip(struct archive *a, const void **buff,
		    size_t *size, off_t *offset);
static time_t	zip_time(const char *);

int
archive_read_support_format_zip(struct archive *a)
{
	struct zip *zip;
	int r;

	zip = malloc(sizeof(*zip));
	memset(zip, 0, sizeof(*zip));

	r = __archive_read_register_format(a,
	    zip,
	    archive_read_format_zip_bid,
	    archive_read_format_zip_read_header,
	    archive_read_format_zip_read_data,
	    archive_read_format_zip_cleanup);

	if (r != ARCHIVE_OK)
		free(zip);
	return (ARCHIVE_OK);
}


static int
archive_read_format_zip_bid(struct archive *a)
{
	int bytes_read;
	int bid = 0;
	const void *h;
	const char *p;

	if (a->archive_format == ARCHIVE_FORMAT_ZIP)
		bid += 1;

	bytes_read = (a->compression_read_ahead)(a, &h, 4);
	if (bytes_read < 4)
	    return (-1);
	p = h;

	if (p[0] == 'P' && p[1] == 'K') {
		bid += 16;
		if (p[2] == '\001' && p[3] == '\002')
			bid += 16;
		else if (p[2] == '\003' && p[3] == '\004')
			bid += 16;
		else if (p[2] == '\005' && p[3] == '\006')
			bid += 16;
		else if (p[2] == '\007' && p[3] == '\010')
			bid += 16;
	}
	return (bid);
}

static int
archive_read_format_zip_read_header(struct archive *a,
    struct archive_entry *entry)
{
	int bytes_read;
	const void *h;
	const struct zip_file_header *p;
	struct zip *zip;

	a->archive_format = ARCHIVE_FORMAT_ZIP;
	if (a->archive_format_name == NULL)
		a->archive_format_name = "ZIP";

	zip = *(a->pformat_data);
	zip->end_of_entry = 0;
	bytes_read =
	    (a->compression_read_ahead)(a, &h, sizeof(struct zip_file_header));
	if (bytes_read < 4)
		return (ARCHIVE_FATAL);

	p = h;
	if (p->signature[0] != 'P' || p->signature[1] != 'K') {
		archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Bad ZIP file");
		return (ARCHIVE_FATAL);
	}

	if (p->signature[2] == '\001' && p->signature[3] == '\002') {
		/* Beginning of central directory. */
		return (ARCHIVE_EOF);
	} else if (p->signature[2] == '\003' && p->signature[3] == '\004') {
		/* Regular file entry; fall through. */
	} else if (p->signature[2] == '\005' && p->signature[3] == '\006') {
		/* End-of-archive record. */
		return (ARCHIVE_EOF);
	} else if (p->signature[2] == '\007' && p->signature[3] == '\010') {
		/* ??? Need to research this. ??? */
	} else {
		archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Damaged ZIP file or unsupported format variant (%d,%d)", p->signature[2], p->signature[3]);
		return (ARCHIVE_FATAL);
	}

	if (bytes_read < (int)sizeof(struct zip_file_header)) {
		archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated ZIP file header");
		return (ARCHIVE_FATAL);
	}

	zip->version = p->version[0];
	zip->system = p->version[1];
	zip->flags = i2(p->flags);
	zip->compression = i2(p->compression);
	if (zip->compression <
	    sizeof(compression_names)/sizeof(compression_names[0]))
		zip->compression_name = compression_names[zip->compression];
	else
		zip->compression_name = "??";
	zip->mtime = zip_time(p->timedate);
	zip->crc32 = i4(p->crc32);
	zip->filename_length = i2(p->filename_length);
	zip->extra_length = i2(p->extra_length);
	zip->uncompressed_size = i4(p->uncompressed_size);
	zip->compressed_size = i4(p->compressed_size);

	(a->compression_read_consume)(a, sizeof(struct zip_file_header));


	/* Read the filename. */
	bytes_read = (a->compression_read_ahead)(a, &h, zip->filename_length);
	if (bytes_read < zip->filename_length) {
		archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated ZIP file header");
		return (ARCHIVE_FATAL);
	}
	archive_string_ensure(&zip->pathname, zip->filename_length);
	archive_strncpy(&zip->pathname, h, zip->filename_length);
	(a->compression_read_consume)(a, zip->filename_length);
	archive_entry_set_pathname(entry, zip->pathname.s);

	/* Read the extra data. */
	bytes_read = (a->compression_read_ahead)(a, &h, zip->extra_length);
	if (bytes_read < zip->extra_length) {
		archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated ZIP file header");
		return (ARCHIVE_FATAL);
	}
	/* TODO: Store the extra data somewhere? */
	(a->compression_read_consume)(a, zip->extra_length);

	/* Populate some additional entry fields: */
	archive_entry_set_mtime(entry, zip->mtime, 0);
	if (zip->pathname.s[archive_strlen(&zip->pathname) - 1] == '/')
		archive_entry_set_mode(entry, S_IFDIR | 0777);
	else
		archive_entry_set_mode(entry, S_IFREG | 0777);
	archive_entry_set_size(entry, zip->uncompressed_size);
	zip->entry_bytes_remaining = zip->compressed_size;
	zip->entry_offset = 0;

	/* Set up a more descriptive format name. */
	sprintf(zip->format_name, "ZIP %d.%d (%s)",
	    zip->version / 10, zip->version % 10,
	    zip->compression_name);
	a->archive_format_name = zip->format_name;

	return (ARCHIVE_OK);
}

/* Convert an MSDOS-style date/time into Unix-style time. */
static time_t
zip_time(const char *p)
{
	int msTime, msDate;
	struct tm ts;

	msTime = (0xff & (unsigned)p[0]) + 256 * (0xff & (unsigned)p[1]);
	msDate = (0xff & (unsigned)p[2]) + 256 * (0xff & (unsigned)p[3]);

	memset(&ts, 0, sizeof(ts));
	ts.tm_year = ((msDate >> 9) & 0x7f) + 80; /* Years since 1900. */
	ts.tm_mon = ((msDate >> 5) & 0x0f) - 1; /* Month number. */
	ts.tm_mday = msDate & 0x1f; /* Day of month. */
	ts.tm_hour = (msTime >> 11) & 0x1f;
	ts.tm_min = (msTime >> 5) & 0x3f;
	ts.tm_sec = (msTime << 1) & 0x3e;
	ts.tm_isdst = -1;
	return mktime(&ts);
}

static int
archive_read_format_zip_read_data(struct archive *a,
    const void **buff, size_t *size, off_t *offset)
{
	int r;
	struct zip *zip;

	zip = *(a->pformat_data);

	if (!zip->end_of_entry) {
		switch(zip->compression) {
		case 0:  /* No compression. */
			r =  zip_read_data_none(a, buff, size, offset);
			break;
		case 8: /* Deflate compression. */
			r =  zip_read_data_deflate(a, buff, size, offset);
			break;
		default: /* Unsupported compression. */
			r =  zip_read_data_skip(a, buff, size, offset);
			/* Return a warning. */
			archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
			    "Unsupported ZIP compression method (%s)",
			    zip->compression_name);
			r = ARCHIVE_WARN;
			break;
		}
	} else {
		r = ARCHIVE_EOF;
		if (zip->flags & ZIP_LENGTH_AT_END) {
			/* TODO: Read the "PK\007\008" trailer that follows. */
		}
	}
	if (r == ARCHIVE_EOF)
		zip->end_of_entry = 1;
	return (r);
}

static int
zip_read_data_none(struct archive *a, const void **buff,
    size_t *size, off_t *offset)
{
	struct zip *zip;
	ssize_t bytes_read;

	zip = *(a->pformat_data);

	if (zip->entry_bytes_remaining == 0) {
		*buff = NULL;
		*size = 0;
		*offset = zip->entry_offset;
		return (ARCHIVE_EOF);
	}

	bytes_read = (a->compression_read_ahead)(a, buff,
	    zip->entry_bytes_remaining);
	if (bytes_read <= 0) {
		archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated ZIP file data");
		return (ARCHIVE_FATAL);
	}
	if (bytes_read > zip->entry_bytes_remaining)
		bytes_read = zip->entry_bytes_remaining;
	(a->compression_read_consume)(a, bytes_read);
	*size = bytes_read;
	*offset = zip->entry_offset;
	zip->entry_offset += *size;
	zip->entry_bytes_remaining -= *size;
	return (ARCHIVE_OK);
}

#ifdef HAVE_ZLIB_H
static int
zip_read_data_deflate(struct archive *a, const void **buff,
    size_t *size, off_t *offset)
{
	struct zip *zip;
	size_t bytes_read;
	const void *compressed_buff;
	int r;

	zip = *(a->pformat_data);

	/* If the buffer hasn't been allocated, allocate it now. */
	if (zip->uncompressed_buffer == NULL) {
		zip->uncompressed_buffer_size = 32 * 1024;
		zip->uncompressed_buffer
		    = malloc(zip->uncompressed_buffer_size);
		if (zip->uncompressed_buffer == NULL) {
			archive_set_error(a, ENOMEM,
			    "No memory for ZIP decompression");
			return (ARCHIVE_FATAL);
		}
	}

	/* If we haven't yet read any data, initialize the decompressor. */
	if (zip->entry_bytes_remaining == zip->compressed_size) {
		r = inflateInit2(&zip->stream,
		    -15 /* Don't check for zlib header */);
		if (r != Z_OK) {
			archive_set_error(a, ARCHIVE_ERRNO_MISC,
			    "Can't initialize ZIP decompression.");
			return (ARCHIVE_FATAL);
		}
	}

	/* Read the next block of compressed data. */
	bytes_read = (a->compression_read_ahead)(a, &compressed_buff,
	    zip->entry_bytes_remaining);
	if (bytes_read <= 0) {
		archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
		    "Truncated ZIP file body");
		return (ARCHIVE_FATAL);
	}
	if (bytes_read > zip->entry_bytes_remaining)
		bytes_read = zip->entry_bytes_remaining;

	/*
	 * A bug in zlib.h: stream.next_in should be marked 'const'
	 * but isn't (the library never alters data through the
	 * next_in pointer, only reads it).  The result: this ugly
	 * cast to remove 'const'.
	 */
	zip->stream.next_in = (void *)(uintptr_t)(const void *)compressed_buff;
	zip->stream.avail_in = bytes_read;
	zip->stream.total_in = 0;
	zip->stream.next_out = zip->uncompressed_buffer;
	zip->stream.avail_out = zip->uncompressed_buffer_size;
	zip->stream.total_out = 0;

	r = inflate(&zip->stream, 0);
	switch (r) {
	case Z_OK:
		break;
	case Z_STREAM_END:
		zip->end_of_entry = 1;
		break;
	case Z_MEM_ERROR:
		archive_set_error(a, ENOMEM,
		    "Out of memory for ZIP decompression");
		return (ARCHIVE_FATAL);
	default:
		archive_set_error(a, ARCHIVE_ERRNO_MISC,
		    "ZIP decompression failed (%d)", r);
		return (ARCHIVE_FATAL);
	}

	/* Consume as much as the compressor actually used. */
	bytes_read = zip->stream.total_in;
	(a->compression_read_consume)(a, bytes_read);
	zip->entry_bytes_remaining -= bytes_read;


	*offset = zip->entry_offset;
	*size = zip->stream.total_out;
	*buff = zip->uncompressed_buffer;
	zip->entry_offset += *size;
	return (ARCHIVE_OK);
}
#else
static int
zip_read_data_deflate(struct archive *a, const void **buff,
    size_t *size, off_t *offset)
{
	int r;

	r = zip_read_data_skip(a, buff, size, offset);
	archive_set_error(a, ARCHIVE_ERRNO_MISC,
	    "libarchive compiled without deflate support (no libz)");
	return (ARCHIVE_WARN);
}
#endif

static int
zip_read_data_skip(struct archive *a, const void **buff,
    size_t *size, off_t *offset)
{
	struct zip *zip;
	ssize_t bytes_read;

	zip = *(a->pformat_data);

	/* Return nothing gracefully. */
	*buff = NULL;
	*size = 0;
	*offset = 0;
	zip->end_of_entry = 1;

	/* Skip body of entry. */
	while (zip->entry_bytes_remaining > 0) {
		bytes_read = (a->compression_read_ahead)(a, buff,
		    zip->entry_bytes_remaining);
		if (bytes_read <= 0) {
			archive_set_error(a, ARCHIVE_ERRNO_FILE_FORMAT,
			    "Truncated ZIP file body");
			return (ARCHIVE_FATAL);
		}
		if (bytes_read > zip->entry_bytes_remaining)
			bytes_read = zip->entry_bytes_remaining;
		(a->compression_read_consume)(a, bytes_read);
		zip->entry_bytes_remaining -= bytes_read;
	}
	return (ARCHIVE_OK);
}

static int
archive_read_format_zip_cleanup(struct archive *a)
{
	struct zip *zip;

	zip = *(a->pformat_data);
	if (zip->uncompressed_buffer != NULL)
		free(zip->uncompressed_buffer);
	archive_string_free(&(zip->pathname));
	archive_string_free(&(zip->extra));
	free(zip);
	*(a->pformat_data) = NULL;
	return (ARCHIVE_OK);
}

static int
i2(const char *p)
{
	return ((0xff & (int)p[0]) + 256 * (0xff & (int)p[1]));
}


static int
i4(const char *p)
{
	return ((0xffff & i2(p)) + 0x10000 * (0xffff & i2(p+2)));
}
