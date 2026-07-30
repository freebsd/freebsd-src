/*-
 * Copyright (c) 2014 Sebastian Freundt
 * Author: Sebastian Freundt  <devel@fresse.org>
 *
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

#include "archive_platform.h"

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
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "archive.h"
#include "archive_entry.h"
#include "archive_entry_locale.h"
#include "archive_private.h"
#include "archive_random_private.h"
#include "archive_write_private.h"
#include "archive_write_set_format_private.h"

/*
 * Overview of the WARC writer:
 *
 * This writer emits WARC/1.0 resource records for regular files.  It
 * writes a warcinfo record once by default, unless the omit-warcinfo
 * option is used.  Each resource record gets a WARC-Target-URI from
 * the entry pathname, a generated WARC-Record-ID, and a mandatory
 * Content-Length header.
 */

struct warc {
	unsigned int omit_warcinfo:1;

	time_t now;
	mode_t filetype;
	/* Remaining bytes to write for the current entry */
	uint64_t entry_bytes_remaining;
};

#define WARC_HEADER_MAX_SIZE 512

static const char warcinfo_payload[] =
    "software: libarchive/" ARCHIVE_VERSION_ONLY_STRING "\r\n"
    "format: WARC file version 1.0\r\n";

enum warc_type {
	WARC_TYPE_NONE,
	/* WARC info */
	WARC_TYPE_INFO,
	/* Metadata */
	WARC_TYPE_METADATA,
	/* Resource */
	WARC_TYPE_RESOURCE,
	/* Request, unsupported */
	WARC_TYPE_REQUEST,
	/* Response, unsupported by this writer */
	WARC_TYPE_RESPONSE,
	/* Revisit, unsupported */
	WARC_TYPE_REVISIT,
	/* Conversion, unsupported */
	WARC_TYPE_CONVERSION,
	/* Continuation, currently unsupported */
	WARC_TYPE_CONTINUATION,
	WARC_TYPE_LAST
};

struct warc_header {
	enum warc_type type;
	const char *target_uri;
	const char *record_id;
	time_t record_time;
	time_t modification_time;
	const char *content_type;
	uint64_t content_length;
};

struct warc_uuid {
	unsigned int value[4U];
};

static int	archive_write_warc_options(struct archive_write *,
		    const char *, const char *);
static int	archive_write_warc_header(struct archive_write *,
		    struct archive_entry *);
static ssize_t	archive_write_warc_data(struct archive_write *, const void *,
		    size_t);
static int	archive_write_warc_finish_entry(struct archive_write *);
static int	archive_write_warc_close(struct archive_write *);
static int	archive_write_warc_free(struct archive_write *);

static void	warc_format_time(struct archive_string *, const char *, time_t);
static ssize_t	warc_populate_header(struct archive_string *, size_t,
		    struct warc_header);
static void	warc_generate_uuid(struct warc_uuid *);

/*
 * Set output format to ISO 28500 (aka WARC) format.
 */
int
archive_write_set_format_warc(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct warc *warc;

	archive_check_magic(_a, ARCHIVE_WRITE_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_write_set_format_warc");

	/* If another format was already registered, unregister it. */
	(void)__archive_write_unregister_format(a);

	warc = malloc(sizeof(*warc));
	if (warc == NULL) {
		archive_set_error(&a->archive, ENOMEM,
		    "Can't allocate warc data");
		return (ARCHIVE_FATAL);
	}
	/* Emit a warcinfo record by default. */
	warc->omit_warcinfo = 0U;
	/* Use the current time for WARC-Date values. */
	warc->now = time(NULL);
	/* Reset file type information. */
	warc->filetype = 0;

	a->format_data = warc;
	a->format_name = "WARC/1.0";
	a->format_options = archive_write_warc_options;
	a->format_write_header = archive_write_warc_header;
	a->format_write_data = archive_write_warc_data;
	a->format_close = archive_write_warc_close;
	a->format_free = archive_write_warc_free;
	a->format_finish_entry = archive_write_warc_finish_entry;
	a->archive.archive_format = ARCHIVE_FORMAT_WARC;
	a->archive.archive_format_name = "WARC/1.0";
	return (ARCHIVE_OK);
}

static int
archive_write_warc_options(struct archive_write *a, const char *key,
    const char *val)
{
	struct warc *warc = a->format_data;

	if (strcmp(key, "omit-warcinfo") == 0) {
		if (val == NULL || strcmp(val, "true") == 0) {
			/* Option accepted. */
			warc->omit_warcinfo = 1U;
			return (ARCHIVE_OK);
		}
	}

	/* ARCHIVE_WARN tells the options supervisor that this option was not
	 * handled here.  It will report an error if no module uses it. */
	return (ARCHIVE_WARN);
}

static int
archive_write_warc_header(struct archive_write *a, struct archive_entry *entry)
{
	struct warc *warc = a->format_data;
	struct archive_string header;

	/* Emit the warcinfo record if needed. */
	if (!warc->omit_warcinfo) {
		ssize_t header_size;
		int ret;
		struct warc_header warcinfo_header = {
			WARC_TYPE_INFO,
			/* URI */NULL,
			/* Record ID */NULL,
			/* Record time */0,
			/* Modified time */0,
			/* Content type */"application/warc-fields",
			/* Content length */sizeof(warcinfo_payload) - 1U,
		};
		warcinfo_header.record_time = warc->now;
		warcinfo_header.modification_time = warc->now;

		archive_string_init(&header);
		header_size = warc_populate_header(&header,
		    WARC_HEADER_MAX_SIZE, warcinfo_header);
		if (header_size < 0) {
			archive_string_free(&header);
			archive_set_error(&a->archive,
			    ARCHIVE_ERRNO_FILE_FORMAT,
			    "Cannot archive warcinfo record");
			return (ARCHIVE_FAILED);
		}

		/* Reuse the header buffer for the warcinfo payload. */
		archive_strncat(&header, warcinfo_payload,
		    sizeof(warcinfo_payload) - 1);

		/* Append the end-of-record indicator. */
		archive_strncat(&header, "\r\n\r\n", 4);

		/* Write the warcinfo record to the output stream. */
		ret = __archive_write_output(a, header.s,
		    archive_strlen(&header));
		if (ret != ARCHIVE_OK) {
			archive_string_free(&header);
			return (ret);
		}

		/* Mark the file header as written. */
		warc->omit_warcinfo = 1U;
		archive_string_free(&header);
	}

	if (archive_entry_pathname(entry) == NULL) {
		archive_set_error(&a->archive, EINVAL,
		    "Invalid filename");
		return (ARCHIVE_WARN);
	}

	warc->filetype = archive_entry_filetype(entry);
	warc->entry_bytes_remaining = 0U;
	if (warc->filetype == AE_IFREG) {
		struct warc_header resource_header = {
			WARC_TYPE_RESOURCE,
			/* URI */NULL,
			/* Record ID */NULL,
			/* Record time */0,
			/* Modified time */0,
			/* Content type */NULL,
			/* Content length */0,
		};
		ssize_t header_size;
		int ret;
		int64_t size;

		resource_header.target_uri = archive_entry_pathname(entry);
		resource_header.record_time = warc->now;
		resource_header.modification_time = archive_entry_mtime(entry);
		if (!archive_entry_size_is_set(entry)) {
			archive_set_error(&a->archive, -1,
			    "Size required");
			return (ARCHIVE_FAILED);
		}
		size = archive_entry_size(entry);
		if (size < 0) {
			archive_set_error(&a->archive, -1,
			    "Size required");
			return (ARCHIVE_FAILED);
		}
		resource_header.content_length = (uint64_t)size;

		archive_string_init(&header);
		header_size = warc_populate_header(&header,
		    WARC_HEADER_MAX_SIZE, resource_header);
		if (header_size < 0) {
			/* Header generation failed. */
			archive_string_free(&header);
			archive_set_error(&a->archive,
			    ARCHIVE_ERRNO_FILE_FORMAT,
			    "WARC resource header is too large");
			return (ARCHIVE_FATAL);
		}
		/* Append the header to the output stream. */
		ret = __archive_write_output(a, header.s, header_size);
		if (ret != ARCHIVE_OK) {
			archive_string_free(&header);
			return (ret);
		}
		/* Save the remaining size for subsequent data callbacks. */
		warc->entry_bytes_remaining = resource_header.content_length;
		archive_string_free(&header);
		return (ARCHIVE_OK);
	}
	/* Report unsupported file types through the common helper. */
	__archive_write_entry_filetype_unsupported(
	    &a->archive, entry, "WARC");
	return (ARCHIVE_FAILED);
}

static ssize_t
archive_write_warc_data(struct archive_write *a, const void *buf, size_t len)
{
	struct warc *warc = a->format_data;

	if (warc->filetype == AE_IFREG) {
		int ret;

		/* Never write more bytes than announced. */
		if ((uint64_t)len > warc->entry_bytes_remaining) {
			len = (size_t)warc->entry_bytes_remaining;
		}

		/* Write the entry data. */
		ret = __archive_write_output(a, buf, len);
		if (ret != ARCHIVE_OK) {
			return (ret);
		}
		warc->entry_bytes_remaining -= len;
	}
	return (len);
}

static int
archive_write_warc_finish_entry(struct archive_write *a)
{
	static const char end_of_record[] = "\r\n\r\n";
	struct warc *warc = a->format_data;

	if (warc->filetype == AE_IFREG) {
		int ret;

		if (warc->entry_bytes_remaining != 0U) {
			archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC,
			    "WARC entry is shorter than Content-Length");
			return (ARCHIVE_FATAL);
		}

		ret = __archive_write_output(a, end_of_record,
		    sizeof(end_of_record) - 1U);
		if (ret != ARCHIVE_OK) {
			return (ret);
		}
	}
	/* Reset file type information. */
	warc->filetype = 0;
	return (ARCHIVE_OK);
}

static int
archive_write_warc_close(struct archive_write *a)
{
	(void)a; /* UNUSED */
	return (ARCHIVE_OK);
}

static int
archive_write_warc_free(struct archive_write *a)
{
	struct warc *warc = a->format_data;

	free(warc);
	a->format_data = NULL;
	return (ARCHIVE_OK);
}

/* Like strftime(3), but for time_t objects. */
static void
warc_format_time(struct archive_string *str, const char *format, time_t t)
{
	struct tm *tm;
#if defined(HAVE_GMTIME_R) || defined(HAVE_GMTIME_S)
	struct tm tm_storage;
#endif
	char time_string[100];
	size_t len;

#if defined(HAVE_GMTIME_S)
	tm = gmtime_s(&tm_storage, &t) ? NULL : &tm_storage;
#elif defined(HAVE_GMTIME_R)
	tm = gmtime_r(&t, &tm_storage);
#else
	tm = gmtime(&t);
#endif
	if (tm == NULL)
		return;
	/* Let strftime() handle the actual formatting. */
	len = strftime(time_string, sizeof(time_string) - 1, format, tm);
	archive_strncat(str, time_string, len);
}

static ssize_t
warc_populate_header(struct archive_string *header_string, size_t max_size,
    struct warc_header header)
{
	static const char version[] = "WARC/1.0\r\n";
	static const char * const record_types[WARC_TYPE_LAST] = {
		NULL, "warcinfo", "metadata", "resource", NULL
	};
	char generated_record_id[48U];

	if (header.type == WARC_TYPE_NONE || header.type > WARC_TYPE_RESOURCE) {
		/* Invalid record type for this writer. */
		return (-1);
	}

	archive_strcpy(header_string, version);

	archive_string_sprintf(header_string, "WARC-Type: %s\r\n",
	    record_types[header.type]);

	if (header.target_uri != NULL) {
		const char *uri_prefix;
		const char *scheme = strchr(header.target_uri, ':');

		/* Check whether the value already contains ://. */
		if (scheme != NULL && scheme[1U] == '/' && scheme[2U] == '/') {
			/* Already has a scheme-style :// prefix. */
			uri_prefix = "";
		} else {
			/* Prepend file:// for local paths. */
			uri_prefix = "file://";
		}
		archive_string_sprintf(header_string,
		    "WARC-Target-URI: %s%s\r\n", uri_prefix,
		    header.target_uri);
	}

	/* Write WARC-Date from header.record_time. */
	warc_format_time(header_string,
	    "WARC-Date: %Y-%m-%dT%H:%M:%SZ\r\n", header.record_time);

	/* Also write Last-Modified from header.modification_time. */
	warc_format_time(header_string,
	    "Last-Modified: %Y-%m-%dT%H:%M:%SZ\r\n",
	    header.modification_time);

	if (header.record_id == NULL) {
		/* Generate a record ID when one was not provided. */
		struct warc_uuid uuid;

		warc_generate_uuid(&uuid);
		/* archive_string_sprintf() does not support minimum field widths, so
		 * use snprintf() for UUID formatting. */
#if defined(_WIN32) && !defined(__CYGWIN__) && \
    !(defined(_MSC_VER) && _MSC_VER >= 1900)
#define snprintf _snprintf
#endif
		snprintf(generated_record_id, sizeof(generated_record_id),
		    "<urn:uuid:%08x-%04x-%04x-%04x-%04x%08x>",
		    uuid.value[0U],
		    uuid.value[1U] >> 16U, uuid.value[1U] & 0xffffU,
		    uuid.value[2U] >> 16U, uuid.value[2U] & 0xffffU,
		    uuid.value[3U]);
		header.record_id = generated_record_id;
	}

	/* WARC-Record-ID is mandatory. */
	archive_string_sprintf(header_string, "WARC-Record-ID: %s\r\n",
	    header.record_id);

	if (header.content_type != NULL) {
		archive_string_sprintf(header_string, "Content-Type: %s\r\n",
		    header.content_type);
	}

	/* Content-Length is mandatory. */
	archive_string_sprintf(header_string, "Content-Length: %ju\r\n",
	    (uintmax_t)header.content_length);
	/* End of header. */
	archive_strncat(header_string, "\r\n", 2);

	return (archive_strlen(header_string) >= max_size) ?
	    -1 : (ssize_t)archive_strlen(header_string);
}

static void
warc_generate_uuid(struct warc_uuid *uuid)
{
	archive_random(uuid->value, sizeof(uuid->value));
	/* Apply UUID version 4 rules. */
	uuid->value[1U] &= 0xffff0fffU;
	uuid->value[1U] |= 0x4000U;
	uuid->value[2U] &= 0x3fffffffU;
	uuid->value[2U] |= 0x80000000U;
}
