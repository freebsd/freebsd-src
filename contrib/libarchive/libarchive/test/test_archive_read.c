/*-
 * Copyright (c) 2024 Tobias Stoeckmann
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

#include "test.h"

#define __LIBARCHIVE_TEST
#include "archive_read_private.h"

static char buf[1024];

DEFINE_TEST(test_archive_read_ahead_eof)
{
	struct archive *a;
	struct archive_read *ar;
	ssize_t avail;

	/* prepare a reader of raw in-memory data */
	assert((a = archive_read_new()) != NULL);
	ar = (struct archive_read *)a;

	assertA(0 == archive_read_support_format_raw(a));
	assertA(0 == archive_read_open_memory(a, buf, sizeof(buf)));

	/* perform a read which can be fulfilled */
	assert(NULL != __archive_read_ahead(ar, sizeof(buf) - 1, &avail));
	assertEqualInt(sizeof(buf), avail);

	/* perform a read which cannot be fulfilled due to EOF */
	assert(NULL == __archive_read_ahead(ar, sizeof(buf) + 1, &avail));
	assertEqualInt(sizeof(buf), avail);

	/* perform the same read again */
	assert(NULL == __archive_read_ahead(ar, sizeof(buf) + 1, &avail));
	assertEqualInt(sizeof(buf), avail);

	/* perform another read which can be fulfilled */
	assert(NULL != __archive_read_ahead(ar, sizeof(buf), &avail));
	assertEqualInt(sizeof(buf), avail);

	assert(0 == archive_read_free(a));
}

DEFINE_TEST(test_archive_read_ahead_zero)
{
	struct archive *a;
	struct archive_read *ar;
	ssize_t avail;

	/* prepare a reader of raw in-memory data */
	assert((a = archive_read_new()) != NULL);
	ar = (struct archive_read *)a;

	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_raw(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, buf, sizeof(buf)));

	/* perform a zero read which can be fulfilled */
	assert(NULL != __archive_read_ahead(ar, 0, &avail));
	assertEqualIntA(ar, sizeof(buf), avail);

	/* perform a read which can be fulfilled */
	assert(NULL != __archive_read_ahead(ar, 1, &avail));
	assertEqualIntA(ar, sizeof(buf), avail);

	/* perform the zero read again */
	assert(NULL != __archive_read_ahead(ar, 0, &avail));
	assertEqualIntA(ar, sizeof(buf), avail);

	/* consume all available bytes */
	assertEqualIntA(ar, sizeof(buf),
	    __archive_read_consume(ar, sizeof(buf)));

	/* perform a read which cannot be fulfilled */
	assert(NULL == __archive_read_ahead(ar, 1, &avail));
	assertEqualIntA(ar, 0, avail);

	/* perform another zero read which can be fulfilled */
	assert(NULL != __archive_read_ahead(ar, 0, &avail));
	assertEqualIntA(ar, 0, avail);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));

	/* prepare a reader of raw in-memory data */
	assert((a = archive_read_new()) != NULL);
	ar = (struct archive_read *)a;

	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_empty(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_open_memory(a, buf, 0));

	/* perform a zero read which can be fulfilled */
	assert(NULL != __archive_read_ahead(ar, 0, &avail));
	assertEqualIntA(ar, 0, avail);

	/* perform a read which cannot be fulfilled */
	assert(NULL == __archive_read_ahead(ar, 1, &avail));
	assertEqualIntA(ar, 0, avail);

	/* perform another zero read which can be fulfilled */
	assert(NULL != __archive_read_ahead(ar, 0, &avail));
	assertEqualIntA(ar, 0, avail);

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

static ssize_t
eof_read_callback(struct archive *a, void *data, const void **buffer)
{
	(void)a;
	(void)data;
	(void)buffer;

	return (0);
}

static int
evil_open_callback(struct archive *a, void *data)
{
	(void)data;

	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_set_read_callback(a, NULL));
	return (ARCHIVE_OK);
}

DEFINE_TEST(test_archive_read_state_open)
{
	struct archive *a;

	assert((a = archive_read_new()) != NULL);

	/* Prepare callbacks which modify callbacks when used. */
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_open_callback(a, evil_open_callback));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_read_callback(a, eof_read_callback));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_set_callback_data(a, NULL));

	/* Try to open archive. */
	assertEqualIntA(a, ARCHIVE_FATAL, archive_read_open1(a));

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

struct read_memory_data {
	const unsigned char	*start;
	const unsigned char	*p;
	const unsigned char	*end;
	ssize_t	 read_size;
};

/*
 * Set read position to beginning of memory.
 */
static int
memory_read_open(struct archive *a, void *client_data)
{
	struct read_memory_data *mine = (struct read_memory_data *)client_data;

	(void)a; /* UNUSED */
	mine->p = mine->start;
	return (ARCHIVE_OK);
}

/*
 * This is scary simple:  Just advance a pointer.  Limiting
 * to read_size is not technically necessary, but it exercises
 * more of the internal logic when used with a small block size
 * in a test harness.  Production use should not specify a block
 * size; then this is much faster.
 */
static ssize_t
memory_read(struct archive *a, void *client_data, const void **buff)
{
	struct read_memory_data *mine = (struct read_memory_data *)client_data;
	ssize_t size;

	(void)a; /* UNUSED */
	*buff = mine->p;
	size = mine->end - mine->p;
	if (size > mine->read_size)
		size = mine->read_size;
        mine->p += size;
	return (size);
}

/*
 * Advancing is just as simple.  Again, this is doing more than
 * necessary in order to better exercise internal code when used
 * as a test harness.
 */
static int64_t
memory_read_skip(struct archive *a, void *client_data, int64_t skip)
{
	struct read_memory_data *mine = (struct read_memory_data *)client_data;

	(void)a; /* UNUSED */
	if ((int64_t)skip > (int64_t)(mine->end - mine->p))
		skip = mine->end - mine->p;
	/* Round down to block size. */
	skip /= mine->read_size;
	skip *= mine->read_size;
	mine->p += skip;
	return (skip);
}

/*
 * Seeking.
 */
static int64_t
memory_read_seek(struct archive *a, void *client_data,
    int64_t offset, int whence)
{
	struct read_memory_data *mine = (struct read_memory_data *)client_data;
	const unsigned char *p;

	(void)a; /* UNUSED */
	switch (whence) {
	case SEEK_SET:
		p = mine->start + offset;
		break;
	case SEEK_CUR:
		p = mine->p + offset;
		break;
	case SEEK_END:
		p = mine->end + offset;
		break;
	default:
		return ARCHIVE_FAILED;
	}
	if (p < mine->start)
		return ARCHIVE_FAILED;
	if (p > mine->end)
		return ARCHIVE_FAILED;
	mine->p = p;
	return (mine->p - mine->start);
}

/*
 * Seeking always fails.
 */
static int64_t
memory_read_failing_seek(struct archive *a, void *client_data,
    int64_t offset, int whence)
{
	(void)a; /* UNUSED */
	(void)client_data;
	(void)offset;
	(void)whence;

	errno = ESPIPE;
	return (ARCHIVE_FAILED);
}

/*
 * Close is just cleaning up our one small bit of data.
 */
static int
memory_read_close(struct archive *a, void *client_data)
{
	struct read_memory_data *mine = (struct read_memory_data *)client_data;
	(void)a; /* UNUSED */
	free(mine);
	return (ARCHIVE_OK);
}

/*
 * Switching keeps memory allocated and just resets read pointer.
 */
static int
memory_read_switch(struct archive *a, void *client_data1, void *client_data2)
{
	(void)client_data1; /* UNUSED */
	return memory_read_open(a, client_data2);
}

DEFINE_TEST(test_archive_read_consume_beyond_eof)
{
	struct archive *a;
	struct archive_entry *ae;
	struct read_memory_data *mine;
	char *buff1, *buff2;
	size_t buffsize = 128 * 1024;
	size_t contentsize = 65 * 1024;
	size_t used;

	buff1 = calloc(1, buffsize);
	assert(buff1 != NULL);
	buff2 = calloc(1, buffsize);
	assert(buff2 != NULL);

	/* Create a new archive in memory. */
	assert((a = archive_write_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_set_format_ustar(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_add_filter_none(a));
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_write_open_memory(a, buff1, buffsize, &used));

	/* Add a large (>64 KB) file. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_pathname(ae, "file1");
	archive_entry_set_mode(ae, S_IFREG | 0664);
	archive_entry_set_size(ae, 67 * 1024);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualIntA(a, contentsize, archive_write_data(a, buff2, contentsize));

	/* Add a small file at the end. */
	assert((ae = archive_entry_new()) != NULL);
	archive_entry_set_mtime(ae, 1, 10);
	archive_entry_set_pathname(ae, "file2");
	archive_entry_set_mode(ae, S_IFREG | 0664);
	archive_entry_set_size(ae, 6);
	assertEqualIntA(a, ARCHIVE_OK, archive_write_header(a, ae));
	archive_entry_free(ae);
	assertEqualIntA(a, 6, archive_write_data(a, "canary", 6));

	/* Close out the archive. */
	assertEqualInt(ARCHIVE_OK, archive_write_free(a));

	/* Split archive into two memory areas. */
	memcpy(buff2, buff1 + 64 * 1024, 64 * 1024);
	memset(buff1 + 64 * 1024, 0, 64 * 1024);

	/* Prepare a tar reader. */
	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_tar(a));

	/* Add first volume. */
	mine = calloc(1, sizeof(*mine));
	assert(mine != NULL);
	mine->start = (const unsigned char *)buff1;
	mine->end = mine->start + 64 * 1024;
	mine->read_size = 1024;
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_append_callback_data(a, mine));

	/* Add second volume. */
	mine = calloc(1, sizeof(*mine));
	assert(mine != NULL);
	mine->start = (const unsigned char *)buff2;
	mine->end = mine->start + 64 * 1024;
	mine->read_size = 1024;
	assertEqualIntA(a, ARCHIVE_OK,
	    archive_read_append_callback_data(a, mine));

	/* Register callbacks: seek but no skip */
	archive_read_set_open_callback(a, memory_read_open);
	archive_read_set_read_callback(a, memory_read);
	archive_read_set_seek_callback(a, memory_read_seek);
	archive_read_set_close_callback(a, memory_read_close);
	archive_read_set_switch_callback(a, memory_read_switch);

	/* Open archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open1(a));

	/* Read both files. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_next_header(a, &ae));

	/* Verify end of archive is reached. */
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
}

DEFINE_TEST(test_archive_read_seek_fails)
{
	struct archive *a;
	struct archive_entry *ae;
	struct read_memory_data *mine;
	char *buff;
	size_t size;

	size = 1024;
	buff = calloc(1, size);
	assert(buff != NULL);

	assert((a = archive_read_new()) != NULL);

	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));

	/* Create memory reader which fails seeking. */
	mine = calloc(1, sizeof(*mine));
	assert(mine != NULL);
	mine->start = mine->p = (const unsigned char *)buff;
	mine->end = mine->start + size;
	mine->read_size = 512;
	archive_read_set_open_callback(a, memory_read_open);
	archive_read_set_read_callback(a, memory_read);
	archive_read_set_seek_callback(a, memory_read_failing_seek);
	archive_read_set_skip_callback(a, memory_read_skip);
	archive_read_set_close_callback(a, memory_read_close);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_set_callback_data(a, mine));

	/* Read empty tar archive. */
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open1(a));
	assertEqualIntA(a, ARCHIVE_EOF, archive_read_next_header(a, &ae));
	assertEqualIntA(a, ARCHIVE_FORMAT_TAR, archive_format(a));

	assertEqualInt(ARCHIVE_OK, archive_read_free(a));
	free(buff);
}
