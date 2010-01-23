/*-
 * Copyright (c) 2008 Anselm Strauss
 * Copyright (c) 2009 Joerg Sonnenberger
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
 * Development supported by Google Summer of Code 2008.
 */

/*
 * The current implementation is very limited:
 *
 *   - No encryption support.
 *   - No ZIP64 support.
 *   - No support for splitting and spanning.
 *   - Only supports regular file and folder entries.
 *
 * Note that generally data in ZIP files is little-endian encoded,
 * with some exceptions.
 *
 * TODO: Since Libarchive is generally 64bit oriented, but this implementation
 * does not yet support sizes exceeding 32bit, it is highly fragile for
 * big archives. This should change when ZIP64 is finally implemented, otherwise
 * some serious checking has to be done.
 *
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
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#include "archive.h"
#include "archive_endian.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_write_private.h"

#ifndef HAVE_ZLIB_H
#include "archive_crc32.h"
#endif

#define ZIP_SIGNATURE_LOCAL_FILE_HEADER 0x04034b50
#define ZIP_SIGNATURE_DATA_DESCRIPTOR 0x08074b50
#define ZIP_SIGNATURE_FILE_HEADER 0x02014b50
#define ZIP_SIGNATURE_CENTRAL_DIRECTORY_END 0x06054b50
#define ZIP_SIGNATURE_EXTRA_TIMESTAMP 0x5455
#define ZIP_SIGNATURE_EXTRA_UNIX 0x7855
#define ZIP_VERSION_EXTRACT 0x0014 /* ZIP version 2.0 is needed. */
#define ZIP_VERSION_BY 0x0314 /* Made by UNIX, using ZIP version 2.0. */
#define ZIP_FLAGS 0x08 /* Flagging bit 3 (count from 0) for using data descriptor. */

enum compression {
	COMPRESSION_STORE = 0
#ifdef HAVE_ZLIB_H
	,
	COMPRESSION_DEFLATE = 8
#endif
};

static ssize_t archive_write_zip_data(struct archive_write *, const void *buff, size_t s);
static int archive_write_zip_finish(struct archive_write *);
static int archive_write_zip_destroy(struct archive_write *);
static int archive_write_zip_finish_entry(struct archive_write *);
static int archive_write_zip_header(struct archive_write *, struct archive_entry *);
static unsigned int dos_time(const time_t);
static size_t path_length(struct archive_entry *);
static int write_path(struct archive_entry *, struct archive_write *);

struct zip_local_file_header {
	char signature[4];
	char version[2];
	char flags[2];
	char compression[2];
	char timedate[4];
	char crc32[4];
	char compressed_size[4];
	char uncompressed_size[4];
	char filename_length[2];
	char extra_length[2];
};

struct zip_file_header {
	char signature[4];
	char version_by[2];
	char version_extract[2];
	char flags[2];
	char compression[2];
	char timedate[4];
	char crc32[4];
	char compressed_size[4];
	char uncompressed_size[4];
	char filename_length[2];
	char extra_length[2];
	char comment_length[2];
	char disk_number[2];
	char attributes_internal[2];
	char attributes_external[4];
	char offset[4];
};

struct zip_data_descriptor {
	char signature[4]; /* Not mandatory, but recommended by specification. */
	char crc32[4];
	char compressed_size[4];
	char uncompressed_size[4];
};

struct zip_extra_data_local {
	char time_id[2];
	char time_size[2];
	char time_flag[1];
	char mtime[4];
	char atime[4];
	char ctime[4];
	char unix_id[2];
	char unix_size[2];
	char unix_uid[2];
	char unix_gid[2];
};

struct zip_extra_data_central {
	char time_id[2];
	char time_size[2];
	char time_flag[1];
	char mtime[4];
	char unix_id[2];
	char unix_size[2];
};

struct zip_file_header_link {
	struct zip_file_header_link *next;
	struct archive_entry *entry;
	off_t offset;
	unsigned long crc32;
	off_t compressed_size;
	enum compression compression;
};

struct zip {
	struct zip_data_descriptor data_descriptor;
	struct zip_file_header_link *central_directory;
	struct zip_file_header_link *central_directory_end;
	int64_t offset;
	int64_t written_bytes;
	int64_t remaining_data_bytes;
	enum compression compression;

#ifdef HAVE_ZLIB_H
	z_stream stream;
	size_t len_buf;
	unsigned char *buf;
#endif
};

struct zip_central_directory_end {
	char signature[4];
	char disk[2];
	char start_disk[2];
	char entries_disk[2];
	char entries[2];
	char size[4];
	char offset[4];
	char comment_length[2];
};

static int
archive_write_zip_options(struct archive_write *a, const char *key,
    const char *value)
{
	struct zip *zip = a->format_data;

	if (strcmp(key, "compression") == 0) {
		if (strcmp(value, "deflate") == 0) {
#ifdef HAVE_ZLIB_H
			zip->compression = COMPRESSION_DEFLATE;
#else
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "deflate compression not supported");
			return ARCHIVE_WARN;
#endif
		} else if (strcmp(value, "store") == 0)
			zip->compression = COMPRESSION_STORE;
		else
			return (ARCHIVE_WARN);
		return (ARCHIVE_OK);
	}
	return (ARCHIVE_WARN);
}

int
archive_write_set_format_zip(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct zip *zip;

	/* If another format was already registered, unregister it. */
	if (a->format_destroy != NULL)
		(a->format_destroy)(a);

	zip = (struct zip *) calloc(1, sizeof(*zip));
	if (zip == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Can't allocate zip data");
		return (ARCHIVE_FATAL);
	}
	zip->central_directory = NULL;
	zip->central_directory_end = NULL;
	zip->offset = 0;
	zip->written_bytes = 0;
	zip->remaining_data_bytes = 0;

#ifdef HAVE_ZLIB_H
	zip->compression = COMPRESSION_DEFLATE;
	zip->len_buf = 65536;
	zip->buf = malloc(zip->len_buf);
	if (zip->buf == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Can't allocate compression buffer");
		return (ARCHIVE_FATAL);
	}
#else
	zip->compression = COMPRESSION_STORE;
#endif

	a->format_data = zip;

	a->pad_uncompressed = 0; /* Actually not needed for now, since no compression support yet. */
	a->format_name = "zip";
	a->format_options = archive_write_zip_options;
	a->format_write_header = archive_write_zip_header;
	a->format_write_data = archive_write_zip_data;
	a->format_finish_entry = archive_write_zip_finish_entry;
	a->format_finish = archive_write_zip_finish;
	a->format_destroy = archive_write_zip_destroy;
	a->archive.archive_format = ARCHIVE_FORMAT_ZIP;
	a->archive.archive_format_name = "ZIP";

	archive_le32enc(&zip->data_descriptor.signature,
	    ZIP_SIGNATURE_DATA_DESCRIPTOR);

	return (ARCHIVE_OK);
}

static int
archive_write_zip_header(struct archive_write *a, struct archive_entry *entry)
{
	struct zip *zip;
	struct zip_local_file_header h;
	struct zip_extra_data_local e;
	struct zip_data_descriptor *d;
	struct zip_file_header_link *l;
	int ret;
	int64_t size;
	mode_t type;

	/* Entries other than a regular file or a folder are skipped. */
	type = archive_entry_filetype(entry);
	if ((type != AE_IFREG) & (type != AE_IFDIR)) {
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC, "Filetype not supported");
		return ARCHIVE_FAILED;
	};

	/* Directory entries should have a size of 0. */
	if (type == AE_IFDIR)
		archive_entry_set_size(entry, 0);

	zip = a->format_data;
	d = &zip->data_descriptor;
	size = archive_entry_size(entry);
	zip->remaining_data_bytes = size;

	/* Append archive entry to the central directory data. */
	l = (struct zip_file_header_link *) malloc(sizeof(*l));
	if (l == NULL) {
		archive_set_error(&a->archive, ENOMEM, "Can't allocate zip header data");
		return (ARCHIVE_FATAL);
	}
	l->entry = archive_entry_clone(entry);
	/* Initialize the CRC variable and potentially the local crc32(). */
	l->crc32 = crc32(0, NULL, 0);
	l->compression = zip->compression;
	l->compressed_size = 0;
	l->next = NULL;
	if (zip->central_directory == NULL) {
		zip->central_directory = l;
	} else {
		zip->central_directory_end->next = l;
	}
	zip->central_directory_end = l;

	/* Store the offset of this header for later use in central directory. */
	l->offset = zip->written_bytes;

	memset(&h, 0, sizeof(h));
	archive_le32enc(&h.signature, ZIP_SIGNATURE_LOCAL_FILE_HEADER);
	archive_le16enc(&h.version, ZIP_VERSION_EXTRACT);
	archive_le16enc(&h.flags, ZIP_FLAGS);
	archive_le16enc(&h.compression, zip->compression);
	archive_le32enc(&h.timedate, dos_time(archive_entry_mtime(entry)));
	archive_le16enc(&h.filename_length, (uint16_t)path_length(entry));

	switch (zip->compression) {
	case COMPRESSION_STORE:
		/* Setting compressed and uncompressed sizes even when specification says
		 * to set to zero when using data descriptors. Otherwise the end of the
		 * data for an entry is rather difficult to find. */
		archive_le32enc(&h.compressed_size, size);
		archive_le32enc(&h.uncompressed_size, size);
		break;
#ifdef HAVE_ZLIB_H
	case COMPRESSION_DEFLATE:
		archive_le32enc(&h.uncompressed_size, size);

		zip->stream.zalloc = Z_NULL;
		zip->stream.zfree = Z_NULL;
		zip->stream.opaque = Z_NULL;
		zip->stream.next_out = zip->buf;
		zip->stream.avail_out = zip->len_buf;
		if (deflateInit2(&zip->stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
		    -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
			archive_set_error(&a->archive, ENOMEM, "Can't init deflate compressor");
			return (ARCHIVE_FATAL);
		}
		break;
#endif
	}

	/* Formatting extra data. */
	archive_le16enc(&h.extra_length, sizeof(e));
	archive_le16enc(&e.time_id, ZIP_SIGNATURE_EXTRA_TIMESTAMP);
	archive_le16enc(&e.time_size, sizeof(e.time_flag) +
	    sizeof(e.mtime) + sizeof(e.atime) + sizeof(e.ctime));
	e.time_flag[0] = 0x07;
	archive_le32enc(&e.mtime, archive_entry_mtime(entry));
	archive_le32enc(&e.atime, archive_entry_atime(entry));
	archive_le32enc(&e.ctime, archive_entry_ctime(entry));
	    
	archive_le16enc(&e.unix_id, ZIP_SIGNATURE_EXTRA_UNIX);
	archive_le16enc(&e.unix_size, sizeof(e.unix_uid) + sizeof(e.unix_gid));
	archive_le16enc(&e.unix_uid, archive_entry_uid(entry));
	archive_le16enc(&e.unix_gid, archive_entry_gid(entry));

	archive_le32enc(&d->uncompressed_size, size);

	ret = (a->compressor.write)(a, &h, sizeof(h));
	if (ret != ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	zip->written_bytes += sizeof(h);

	ret = write_path(entry, a);
	if (ret <= ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	zip->written_bytes += ret;

	ret = (a->compressor.write)(a, &e, sizeof(e));
	if (ret != ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	zip->written_bytes += sizeof(e);

	return (ARCHIVE_OK);
}

static ssize_t
archive_write_zip_data(struct archive_write *a, const void *buff, size_t s)
{
	int ret;
	struct zip *zip = a->format_data;
	struct zip_file_header_link *l = zip->central_directory_end;

	if ((int64_t)s > zip->remaining_data_bytes)
		s = (size_t)zip->remaining_data_bytes;

	if (s == 0) return 0;

	switch (zip->compression) {
	case COMPRESSION_STORE:
		ret = (a->compressor.write)(a, buff, s);
		if (ret != ARCHIVE_OK) return (ret);
		zip->written_bytes += s;
		zip->remaining_data_bytes -= s;
		l->compressed_size += s;
		l->crc32 = crc32(l->crc32, buff, s);
		return (s);
#if HAVE_ZLIB_H
	case COMPRESSION_DEFLATE:
		zip->stream.next_in = (unsigned char*)(uintptr_t)buff;
		zip->stream.avail_in = s;
		do {
			ret = deflate(&zip->stream, Z_NO_FLUSH);
			if (ret == Z_STREAM_ERROR)
				return (ARCHIVE_FATAL);
			if (zip->stream.avail_out == 0) {
				ret = (a->compressor.write)(a, zip->buf, zip->len_buf);
				if (ret != ARCHIVE_OK)
					return (ret);
				l->compressed_size += zip->len_buf;
				zip->written_bytes += zip->len_buf;
				zip->stream.next_out = zip->buf;
				zip->stream.avail_out = zip->len_buf;
			}
		} while (zip->stream.avail_in != 0);
		zip->remaining_data_bytes -= s;
		/* If we have it, use zlib's fast crc32() */
		l->crc32 = crc32(l->crc32, buff, s);
		return (s);
#endif

	default:
		archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
		    "Invalid ZIP compression type");
		return ARCHIVE_FATAL;
	}
}

static int
archive_write_zip_finish_entry(struct archive_write *a)
{
	/* Write the data descripter after file data has been written. */
	int ret;
	struct zip *zip = a->format_data;
	struct zip_data_descriptor *d = &zip->data_descriptor;
	struct zip_file_header_link *l = zip->central_directory_end;
#if HAVE_ZLIB_H
	size_t reminder;
#endif

	switch(zip->compression) {
	case COMPRESSION_STORE:
		break;
#if HAVE_ZLIB_H
	case COMPRESSION_DEFLATE:
		for (;;) {
			ret = deflate(&zip->stream, Z_FINISH);
			if (ret == Z_STREAM_ERROR)
				return (ARCHIVE_FATAL);
			reminder = zip->len_buf - zip->stream.avail_out;
			ret = (a->compressor.write)(a, zip->buf, reminder);
			if (ret != ARCHIVE_OK)
				return (ret);
			l->compressed_size += reminder;
			zip->written_bytes += reminder;
			zip->stream.next_out = zip->buf;
			if (zip->stream.avail_out != 0)
				break;
			zip->stream.avail_out = zip->len_buf;
		}
		deflateEnd(&zip->stream);
		break;
#endif
	}

	archive_le32enc(&d->crc32, l->crc32);
	archive_le32enc(&d->compressed_size, l->compressed_size);
	ret = (a->compressor.write)(a, d, sizeof(*d));
	if (ret != ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	zip->written_bytes += sizeof(*d);
	return (ARCHIVE_OK);
}

static int
archive_write_zip_finish(struct archive_write *a)
{
	struct zip *zip;
	struct zip_file_header_link *l;
	struct zip_file_header h;
	struct zip_central_directory_end end;
	struct zip_extra_data_central e;
	off_t offset_start, offset_end;
	int entries;
	int ret;

	zip = a->format_data;
	l = zip->central_directory;

	/*
	 * Formatting central directory file header fields that are fixed for all entries.
	 * Fields not used (and therefor 0) are:
	 *
	 *   - comment_length
	 *   - disk_number
	 *   - attributes_internal
	 */
	memset(&h, 0, sizeof(h));
	archive_le32enc(&h.signature, ZIP_SIGNATURE_FILE_HEADER);
	archive_le16enc(&h.version_by, ZIP_VERSION_BY);
	archive_le16enc(&h.version_extract, ZIP_VERSION_EXTRACT);
	archive_le16enc(&h.flags, ZIP_FLAGS);

	entries = 0;
	offset_start = zip->written_bytes;

	/* Formatting individual header fields per entry and
	 * writing each entry. */
	while (l != NULL) {
		archive_le16enc(&h.compression, l->compression);
		archive_le32enc(&h.timedate, dos_time(archive_entry_mtime(l->entry)));
		archive_le32enc(&h.crc32, l->crc32);
		archive_le32enc(&h.compressed_size, l->compressed_size);
		archive_le32enc(&h.uncompressed_size, archive_entry_size(l->entry));
		archive_le16enc(&h.filename_length, (uint16_t)path_length(l->entry));
		archive_le16enc(&h.extra_length, sizeof(e));
		archive_le16enc(&h.attributes_external[2], archive_entry_mode(l->entry));
		archive_le32enc(&h.offset, l->offset);

		/* Formatting extra data. */
		archive_le16enc(&e.time_id, ZIP_SIGNATURE_EXTRA_TIMESTAMP);
		archive_le16enc(&e.time_size, sizeof(e.mtime) + sizeof(e.time_flag));
		e.time_flag[0] = 0x07;
		archive_le32enc(&e.mtime, archive_entry_mtime(l->entry));
		archive_le16enc(&e.unix_id, ZIP_SIGNATURE_EXTRA_UNIX);
		archive_le16enc(&e.unix_size, 0x0000);

		ret = (a->compressor.write)(a, &h, sizeof(h));
		if (ret != ARCHIVE_OK)
			return (ARCHIVE_FATAL);
		zip->written_bytes += sizeof(h);

		ret = write_path(l->entry, a);
		if (ret <= ARCHIVE_OK)
			return (ARCHIVE_FATAL);
		zip->written_bytes += ret;

		ret = (a->compressor.write)(a, &e, sizeof(e));
		if (ret != ARCHIVE_OK)
			return (ARCHIVE_FATAL);
		zip->written_bytes += sizeof(e);

		l = l->next;
		entries++;
	}
	offset_end = zip->written_bytes;

	/* Formatting end of central directory. */
	memset(&end, 0, sizeof(end));
	archive_le32enc(&end.signature, ZIP_SIGNATURE_CENTRAL_DIRECTORY_END);
	archive_le16enc(&end.entries_disk, entries);
	archive_le16enc(&end.entries, entries);
	archive_le32enc(&end.size, offset_end - offset_start);
	archive_le32enc(&end.offset, offset_start);

	/* Writing end of central directory. */
	ret = (a->compressor.write)(a, &end, sizeof(end));
	if (ret != ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	zip->written_bytes += sizeof(end);
	return (ARCHIVE_OK);
}

static int
archive_write_zip_destroy(struct archive_write *a)
{
	struct zip *zip;
	struct zip_file_header_link *l;

	zip = a->format_data;
	while (zip->central_directory != NULL) {
	   l = zip->central_directory;
	   zip->central_directory = l->next;
	   archive_entry_free(l->entry);
	   free(l);
	}
#ifdef HAVE_ZLIB_H
	free(zip->buf);
#endif
	free(zip);
	a->format_data = NULL;
	return (ARCHIVE_OK);
}

/* Convert into MSDOS-style date/time. */
static unsigned int
dos_time(const time_t unix_time)
{
	struct tm *t;
	unsigned int dt;

	/* This will not preserve time when creating/extracting the archive
	 * on two systems with different time zones. */
	t = localtime(&unix_time);

	dt = 0;
	dt += ((t->tm_year - 80) & 0x7f) << 9;
	dt += ((t->tm_mon + 1) & 0x0f) << 5;
	dt += (t->tm_mday & 0x1f);
	dt <<= 16;
	dt += (t->tm_hour & 0x1f) << 11;
	dt += (t->tm_min & 0x3f) << 5;
	dt += (t->tm_sec & 0x3e) >> 1; /* Only counting every 2 seconds. */
	return dt;
}

static size_t
path_length(struct archive_entry *entry)
{
	mode_t type;
	const char *path;

	type = archive_entry_filetype(entry);
	path = archive_entry_pathname(entry);

	if ((type == AE_IFDIR) & (path[strlen(path) - 1] != '/')) {
		return strlen(path) + 1;
	} else {
		return strlen(path);
	}
}

static int
write_path(struct archive_entry *entry, struct archive_write *archive)
{
	int ret;
	const char *path;
	mode_t type;
	size_t written_bytes;

	path = archive_entry_pathname(entry);
	type = archive_entry_filetype(entry);
	written_bytes = 0;

	ret = (archive->compressor.write)(archive, path, strlen(path));
	if (ret != ARCHIVE_OK)
		return (ARCHIVE_FATAL);
	written_bytes += strlen(path);

	/* Folders are recognized by a traling slash. */
	if ((type == AE_IFDIR) & (path[strlen(path) - 1] != '/')) {
		ret = (archive->compressor.write)(archive, "/", 1);
		if (ret != ARCHIVE_OK)
			return (ARCHIVE_FATAL);
		written_bytes += 1;
	}

	return ((int)written_bytes);
}
